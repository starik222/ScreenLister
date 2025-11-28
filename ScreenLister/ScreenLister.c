#include <libavcodec/avcodec.h>
#include <libavcodec/packet.h>
#include <libavformat/avformat.h>
#include <libavformat/avio.h>
#include <libavutil/avutil.h>
#include <libavutil/error.h>
#include <libavutil/imgutils.h>
#include <libavutil/mem.h>
#include <libswscale/swscale.h>
#include "ScreenLister.h"
#include <windows.h>

typedef struct PacketList {
	AVPacket pkt;
	struct PacketList* next;
}PacketList;

typedef struct MyFile {
	AVFormatContext* fmt_ctx;
	AVIOContext* avio_ctx;
	MemData membuf;
} MyFile;

typedef MyFile* FilePtr;


static int      MemData_read(void* opaque, uint8_t* buf, int buf_size);
static int64_t  MemData_seek(void* opaque, int64_t offset, int whence);
static FilePtr  openAVData(const char* fname, const uint8_t* buffer, size_t buffer_len);
static void     closeAVData(FilePtr* file_ptr);
static char* ConvertPathToUtf8(const char* srcFilename);
static ImageBuf GetImage(const uint8_t* buffer, size_t bufSize, const char* filename, int64_t imageTime);


static int MemData_read(void* opaque, uint8_t* buf, int buf_size) {
	MemData* membuf = (MemData*)opaque;
	if (!membuf || !buf || buf_size <= 0) {
		return AVERROR(EINVAL);
	}
	if (membuf->pos >= membuf->length) {
		return AVERROR_EOF;
	}

	size_t remaining = membuf->length - membuf->pos;
	size_t to_copy = remaining < (size_t)buf_size ? remaining : (size_t)buf_size;

	memcpy(buf, membuf->buffer + membuf->pos, to_copy);
	membuf->pos += to_copy;

	return (int)to_copy;
}

static int64_t MemData_seek(void* opaque, int64_t offset, int whence) {
	MemData* membuf = (MemData*)opaque;
	if (!membuf) {
		return AVERROR(EINVAL);
	}

	int64_t new_pos = 0;
	switch (whence) {
	case SEEK_SET:
		new_pos = offset;
		break;
	case SEEK_CUR:
		new_pos = (int64_t)membuf->pos + offset;
		break;
	case SEEK_END:
		new_pos = (int64_t)membuf->length + offset;
		break;
	case AVSEEK_SIZE:
		return (int64_t)membuf->length;
	default:
		return AVERROR(EINVAL);
	}

	if (new_pos < 0 || new_pos >(int64_t)membuf->length) {
		return AVERROR(EINVAL);
	}

	membuf->pos = (size_t)new_pos;
	return new_pos;
}

static FilePtr openAVData(const char* fname, const uint8_t* buffer, size_t buffer_len) {
	if ((!buffer || buffer_len == 0) && !fname) {
		return NULL;
	}

	FilePtr file = (FilePtr)calloc(1, sizeof(*file));
	if (!file) {
		return NULL;
	}

	int ret = 0;

	if (buffer && buffer_len > 0) {
		file->membuf.buffer = buffer;
		file->membuf.length = buffer_len;
		file->membuf.pos = 0;

		file->fmt_ctx = avformat_alloc_context();
		if (!file->fmt_ctx) {
			closeAVData(&file);
			return NULL;
		}

		const int io_buffer_size = 32 * 1024;
		uint8_t* io_buffer = av_malloc(io_buffer_size);
		if (!io_buffer) {
			closeAVData(&file);
			return NULL;
		}

		file->avio_ctx = avio_alloc_context(io_buffer,
			io_buffer_size,
			0,
			&file->membuf,
			MemData_read,
			NULL,
			MemData_seek);
		if (!file->avio_ctx) {
			av_free(io_buffer);
			closeAVData(&file);
			return NULL;
		}

		file->fmt_ctx->pb = file->avio_ctx;
		file->fmt_ctx->flags |= AVFMT_FLAG_CUSTOM_IO;

		ret = avformat_open_input(&file->fmt_ctx, NULL, NULL, NULL);
	}
	else {
		ret = avformat_open_input(&file->fmt_ctx, fname, NULL, NULL);
	}

	if (ret < 0 || avformat_find_stream_info(file->fmt_ctx, NULL) < 0) {
		closeAVData(&file);
		return NULL;
	}

	return file;
}

static void closeAVData(FilePtr* file_ptr) {
	if (!file_ptr || !*file_ptr) {
		return;
	}

	FilePtr file = *file_ptr;

	if (file->fmt_ctx) {
		if (file->avio_ctx) {
			if (file->avio_ctx->buffer) {
				av_freep(&file->avio_ctx->buffer);
			}
			avio_context_free(&file->avio_ctx);
		}
		avformat_close_input(&file->fmt_ctx);
	}
	else if (file->avio_ctx) {
		if (file->avio_ctx->buffer) av_freep(&file->avio_ctx->buffer);
		avio_context_free(&file->avio_ctx);
	}

	free(file);
	*file_ptr = NULL;
}

ImageBuf GetImageFromVideoBuffer(char* buffer, int bufSize, int64_t imageTime) {
	ImageBuf empty = { 0 };
	if (!buffer || bufSize <= 0) {
		return empty;
	}
	return GetImage((const uint8_t*)buffer, (size_t)bufSize, NULL, imageTime);
}

ImageBuf GetImageFromVideoFile(const char* filename, int64_t imageTime) {
	ImageBuf empty = { 0 };
	if (!filename) {
		return empty;
	}
	return GetImage(NULL, 0, filename, imageTime);
}

static char* ConvertPathToUtf8(const char* srcFilename) {
	if (!srcFilename) return NULL;

	// 1. Узнаем длину строки в широких символах (UTF-16)
	// CP_ACP - системная кодировка (Active Code Page)
	int wLen = MultiByteToWideChar(CP_ACP, 0, srcFilename, -1, NULL, 0);
	if (wLen == 0) return NULL;

	// Выделяем память под UTF-16
	wchar_t* wPath = (wchar_t*)malloc(wLen * sizeof(wchar_t));
	if (!wPath) return NULL;

	// Конвертируем char -> wchar_t
	MultiByteToWideChar(CP_ACP, 0, srcFilename, -1, wPath, wLen);

	// 2. Узнаем длину строки в UTF-8
	int u8Len = WideCharToMultiByte(CP_UTF8, 0, wPath, -1, NULL, 0, NULL, NULL);
	if (u8Len == 0) {
		free(wPath);
		return NULL;
	}

	// Выделяем память под UTF-8
	char* u8Path = (char*)malloc(u8Len);
	if (!u8Path) {
		free(wPath);
		return NULL;
	}

	// Конвертируем wchar_t -> char (UTF-8)
	WideCharToMultiByte(CP_UTF8, 0, wPath, -1, u8Path, u8Len, NULL, NULL);

	free(wPath);
	return u8Path;
}

static ImageBuf GetImage(const uint8_t* buffer, size_t bufSize, const char* filename, int64_t imageTime) {
	ImageBuf result = { 0, 0, NULL, 0, 0 };
	char* utf8Filename = NULL;
	utf8Filename = ConvertPathToUtf8(filename);
	if (!utf8Filename) {
		return result;
	}

	FilePtr file = openAVData(utf8Filename, buffer, bufSize);
	if (!file) {
		free(utf8Filename);
		return result;
	}

	const AVCodec* decoder = NULL;
	int stream_index = av_find_best_stream(file->fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (stream_index < 0) {
		free(utf8Filename);
		closeAVData(&file);
		return result;
	}


	AVStream* video_stream = file->fmt_ctx->streams[stream_index];
	AVCodecContext* decoder_ctx = avcodec_alloc_context3(decoder);
	if (!decoder_ctx) {
		free(utf8Filename);
		closeAVData(&file);
		return result;
	}

	if (avcodec_parameters_to_context(decoder_ctx, video_stream->codecpar) < 0 ||
		avcodec_open2(decoder_ctx, decoder, NULL) < 0) {
		free(utf8Filename);
		avcodec_free_context(&decoder_ctx);
		closeAVData(&file);
		return result;
	}

	struct SwsContext* sws_ctx = NULL;
	AVFrame* srcFrame = av_frame_alloc();
	AVPacket* packet = av_packet_alloc();
	if (!srcFrame || !packet) {
		free(utf8Filename);
		av_packet_free(&packet);
		av_frame_free(&srcFrame);
		avcodec_free_context(&decoder_ctx);
		closeAVData(&file);
		return result;
	}

	int64_t videoDuration = 0;

	if (video_stream->duration != AV_NOPTS_VALUE)
	{
		videoDuration = ((video_stream->duration * video_stream->time_base.num) / video_stream->time_base.den) * 1000;
	}
	else
	{
		if (file->fmt_ctx->duration != AV_NOPTS_VALUE)
		{
			videoDuration = (file->fmt_ctx->duration / AV_TIME_BASE) * 1000;
		}
		else
			videoDuration = 10 * 60 * 1000;
	}
	int64_t targetTs = (int64_t)(imageTime / av_q2d(video_stream->time_base));
	if (targetTs < 0)
		targetTs = 0;
	if (targetTs > videoDuration)
		targetTs = videoDuration;
	if (imageTime > 0) 
	{
		if (av_seek_frame(file->fmt_ctx, stream_index, targetTs, AVSEEK_FLAG_BACKWARD) < 0) 
		{
			goto end;
		}
		avcodec_flush_buffers(decoder_ctx);
	}
	int ret = 0;
	int frameFound = 0;

	// 7. Цикл декодирования
	while (av_read_frame(file->fmt_ctx, packet) >= 0) {
		if (packet->stream_index == stream_index) {
			ret = avcodec_send_packet(decoder_ctx, packet);
			if (ret < 0) {
				av_packet_unref(packet);
				continue;
			}

			while (ret >= 0) {
				ret = avcodec_receive_frame(decoder_ctx, srcFrame);
				if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
					break;
				}
				else if (ret < 0) {
					goto end;
				}

				frameFound = 1;
				break;
			}
			if (frameFound) break;
		}
		av_packet_unref(packet);
	}

	// 8. Конвертация изображения
	if (frameFound) {
		// Учитываем, что размер кадра в потоке может быть динамическим,
		// берем размеры из декодированного pFrame, а не из codec context.
		result.Width = srcFrame->width;
		result.Height = srcFrame->height;
		int64_t pts = srcFrame->best_effort_timestamp;
		if (pts == AV_NOPTS_VALUE) pts = srcFrame->pts;
		if (pts == AV_NOPTS_VALUE) pts = srcFrame->pkt_dts;
		if (pts == AV_NOPTS_VALUE) pts = 0;

		int64_t start = (video_stream->start_time == AV_NOPTS_VALUE) ? 0 : video_stream->start_time;
		double seconds = (double)(pts - start) * av_q2d(video_stream->time_base);
		if (seconds < 0.0) seconds = 0.0;
		result.ImageTime = seconds;
		//result.ImageTime = imageTime; // Возвращаем запрошенное время (или можно вернуть pFrame->pts пересчитанный в секунды)

		// Формат AV_PIX_FMT_BGRA (как запрошено)
		// Создаем контекст масштабирования
		sws_ctx = sws_getContext(
			srcFrame->width, srcFrame->height, (enum AVPixelFormat)srcFrame->format,
			srcFrame->width, srcFrame->height, AV_PIX_FMT_BGRA,
			SWS_BILINEAR, NULL, NULL, NULL
		);

		if (sws_ctx) {
			// Временные массивы для данных и шага строк с учетом выравнивания FFmpeg
			uint8_t* align_data[4] = { NULL };
			int align_linesize[4] = { 0 };

			// 1. Выделяем правильный буфер средствами FFmpeg (с выравниванием 32 байта)
			if (av_image_alloc(align_data, align_linesize,
				result.Width, result.Height, AV_PIX_FMT_BGRA, 32) >= 0) {

				// 2. Конвертируем в этот выравненный буфер
				sws_scale(
					sws_ctx,
					(const uint8_t* const*)srcFrame->data,
					srcFrame->linesize,
					0,
					srcFrame->height,
					align_data,
					align_linesize
				);

				// 3. Выделяем память под итоговый результат (плотный массив без отступов)
				// av_image_get_buffer_size рассчитывает размер плотного буфера (align=1)
				int size = av_image_get_buffer_size(AV_PIX_FMT_BGRA, result.Width, result.Height, 1);
				result.BufSize = (unsigned long)size;
				result.ImgBuf = malloc(result.BufSize);

				if (result.ImgBuf) {
					// 4. Копируем из выравненного буфера FFmpeg в наш плотный буфер
					av_image_copy_to_buffer(
						(uint8_t*)result.ImgBuf,
						size,
						(const uint8_t* const*)align_data,
						align_linesize,
						AV_PIX_FMT_BGRA,
						result.Width,
						result.Height,
						1 // Выравнивание 1 байт (плотная упаковка)
					);
				}

				// Освобождаем временный буфер FFmpeg
				av_freep(&align_data[0]);
			}
			sws_freeContext(sws_ctx);
		}
	}

end:
	// 9. Очистка ресурсов
	if (utf8Filename) free(utf8Filename);
	if (packet) av_packet_free(&packet);
	if (srcFrame) av_frame_free(&srcFrame);
	if (decoder_ctx) avcodec_free_context(&decoder_ctx);
	closeAVData(&file);

	return result;

}


EXPORT VideoInfo GetVideoInfo(const char * filename)
{
	VideoInfo info;
	info.Height = 0;
	info.Width = 0;
	AVFormatContext *fmt_ctx = NULL;
	AVDictionaryEntry *tag = NULL;
	int video_stream, ret;
	AVStream *video = NULL;
	AVCodecContext *decoder_ctx = NULL;
	AVCodec *decoder = NULL;

	if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL)))
	{
		return info;
	}

	if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		return info;
	}

	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		return info;
	}
	video_stream = ret;
	if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate context\n");
		return info;
	}
	video = fmt_ctx->streams[video_stream];
	if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
		return info;
	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to open codec for stream\n");
		return info;
	}

	info.Height = decoder_ctx->height;
	info.Width = decoder_ctx->width;

	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&fmt_ctx);

	return info;

}

ScreenList GetImagesV2(const char* filename, int imagesCount, int ResizePercent) {
	ScreenList retList = { 0 };
	retList.ImgCount = 0;

	char* utf8Filename = NULL;
	utf8Filename = ConvertPathToUtf8(filename);
	if (!utf8Filename) {
		return retList;
	}

	enum AVPixelFormat IMAGE_FORMAT = AV_PIX_FMT_BGRA;
	if (ResizePercent)
		ResizePercent = 100 - ResizePercent;
	AVFormatContext* fmt_ctx = NULL;
	AVCodecContext* decoder_ctx = NULL;
	AVFrame* srcFrame = NULL;
	AVPacket* packet = NULL;
	int ret = 0;

	int64_t videoDuration = -1;
	int64_t startTime = -1;

	int videoHeight = 0;
	int videoWidth = 0;

	int videoHeightNew = 0;
	int videoWidthNew = 0;

	int64_t step = 0;
	int64_t curPos = 0;

	//инициализация
	if ((ret = avformat_open_input(&fmt_ctx, utf8Filename, NULL, NULL)))
	{
		free(utf8Filename);
		return retList;
	}

	if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		goto exit;
	}
	const AVCodec* decoder = NULL;
	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		goto exit;
	}
	int stream_index = ret;
	AVStream* video_stream = fmt_ctx->streams[stream_index];
	decoder_ctx = avcodec_alloc_context3(decoder);
	if (!decoder_ctx)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate context\n");
		goto exit;
	}

	if (avcodec_parameters_to_context(decoder_ctx, video_stream->codecpar) < 0)
		goto exit;
	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to open codec for stream\n");
		goto exit;
	}
	if (decoder_ctx->width == 0 || decoder_ctx->height == 0)
		goto exit;

	videoDuration = video_stream->duration;
	if (videoDuration <= 0) videoDuration = fmt_ctx->duration / (AV_TIME_BASE * av_q2d(video_stream->time_base));
	if (videoDuration <= 0) videoDuration = video_stream->nb_frames;
	if (videoDuration <= 0) videoDuration = 10 * 60 * 1000;
	startTime = (video_stream->start_time == AV_NOPTS_VALUE) ? 0 : video_stream->start_time;

	videoWidth = decoder_ctx->width;
	videoHeight = decoder_ctx->height;
	if (ResizePercent)
	{
		videoHeightNew = videoHeight * ResizePercent / 100;
		videoWidthNew = videoWidth * ResizePercent / 100;
	}
	else
	{
		videoHeightNew = videoHeight;
		videoWidthNew = videoWidth;
	}
	retList.bufs = malloc(sizeof(ImageBuf) * imagesCount);
	retList.Height = videoHeightNew;
	retList.Width = videoWidthNew;
	retList.ImgCount = imagesCount;
	curPos = startTime;
	step = videoDuration / imagesCount;
	retList.Period = step;

	struct SwsContext* sws_ctx = NULL;
	srcFrame = av_frame_alloc();
	packet = av_packet_alloc();
	if (!srcFrame || !packet) {
		goto exit;
	}
	// Временные массивы для данных и шага строк с учетом выравнивания FFmpeg
	uint8_t* align_data[4] = { NULL };
	int align_linesize[4] = { 0 };
	for (int i = 0; i < imagesCount; i++)
	{
		retList.bufs[i].BufSize = 0;
		retList.bufs[i].ImageTime = 0;
		ret = av_seek_frame(fmt_ctx, stream_index, curPos, AVSEEK_FLAG_BACKWARD);
		if (ret < 0)
		{
			retList.bufs[i].BufSize = 0;
			continue;
		}
		avcodec_flush_buffers(decoder_ctx);

		int frameFound = 0;
		// 7. Цикл декодирования
		while (av_read_frame(fmt_ctx, packet) >= 0) {
			if (packet->stream_index == stream_index) {
				ret = avcodec_send_packet(decoder_ctx, packet);
				if (ret < 0) {
					av_packet_unref(packet);
					continue;
				}

				while (ret >= 0) {
					ret = avcodec_receive_frame(decoder_ctx, srcFrame);
					if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
						break;
					}
					else if (ret < 0) {
						goto exit;
					}

					frameFound = 1;
					break;
				}
				if (frameFound) break;
			}
			av_packet_unref(packet);
		}

		// 8. Конвертация изображения
		if (frameFound) {

			//float decodecTs = (((float)(packet->pts != AV_NOPTS_VALUE ? packet->pts : packet->dts) - (float)video_stream->start_time) / (float)video_stream->time_base.den) * (float)video_stream->time_base.num;
			int64_t pts = srcFrame->best_effort_timestamp;
			if (pts == AV_NOPTS_VALUE) pts = srcFrame->pts;
			if (pts == AV_NOPTS_VALUE) pts = srcFrame->pkt_dts;
			if (pts == AV_NOPTS_VALUE) pts = 0;

			int64_t start = (video_stream->start_time == AV_NOPTS_VALUE) ? 0 : video_stream->start_time;
			float seconds = (float)(pts - start) * av_q2d(video_stream->time_base);
			retList.bufs[i].ImageTime = seconds;
			retList.bufs[i].Width = srcFrame->width;
			retList.bufs[i].Height = srcFrame->height;



			// Формат AV_PIX_FMT_BGRA (как запрошено)
			// Создаем контекст масштабирования
			sws_ctx = sws_getContext(
				srcFrame->width, srcFrame->height, (enum AVPixelFormat)srcFrame->format,
				videoWidthNew, videoHeightNew, IMAGE_FORMAT,
				SWS_BILINEAR, NULL, NULL, NULL
			);

			if (sws_ctx) {
				// 1. Выделяем правильный буфер средствами FFmpeg (с выравниванием 32 байта)
				if (av_image_alloc(align_data, align_linesize,
					videoWidthNew, videoHeightNew, AV_PIX_FMT_BGRA, 32) >= 0) {

					// 2. Конвертируем в этот выравненный буфер
					ret = sws_scale(
						sws_ctx,
						(const uint8_t* const*)srcFrame->data,
						srcFrame->linesize,
						0,
						srcFrame->height,
						align_data,
						align_linesize
					);
					if (!ret) {
						retList.bufs[i].BufSize = 0;
						av_freep(&align_data[0]);
						sws_freeContext(sws_ctx);
						continue;
					}
					// 3. Выделяем память под итоговый результат (плотный массив без отступов)
					// av_image_get_buffer_size рассчитывает размер плотного буфера (align=1)
					int size = av_image_get_buffer_size(AV_PIX_FMT_BGRA, videoWidthNew, videoHeightNew, 1);
					retList.bufs[i].BufSize = (unsigned long)size;
					retList.bufs[i].ImgBuf = malloc(retList.bufs[i].BufSize);

					if (retList.bufs[i].ImgBuf) {
						// 4. Копируем из выравненного буфера FFmpeg в наш плотный буфер
						av_image_copy_to_buffer(
							(uint8_t*)retList.bufs[i].ImgBuf,
							size,
							(const uint8_t* const*)align_data,
							align_linesize,
							AV_PIX_FMT_BGRA,
							videoWidthNew,
							videoHeightNew,
							1 // Выравнивание 1 байт (плотная упаковка)
						);
					}
					// Освобождаем временный буфер FFmpeg
					av_freep(&align_data[0]);
				}
				sws_freeContext(sws_ctx);
			}
		}
		curPos += step;
	}
exit:
	if (utf8Filename) free(utf8Filename);
	if (packet) av_packet_free(&packet);
	if (srcFrame) av_frame_free(&srcFrame);
	if (decoder_ctx) avcodec_free_context(&decoder_ctx);
	if (fmt_ctx) avformat_close_input(&fmt_ctx);
	return retList;
}

ScreenList GetImages(const char * filename, int LineCount, int AllCount, int ResizePercent) {

	enum AVPixelFormat IMAGE_FORMAT = AV_PIX_FMT_BGRA;
	if (ResizePercent)
		ResizePercent = 100 - ResizePercent;
	//char * errorText;
	AVFormatContext *fmt_ctx = NULL;
	AVDictionaryEntry *tag = NULL;
	int video_stream, ret;
	AVStream *video = NULL;
	AVCodecContext *decoder_ctx = NULL;
	AVCodec *decoder = NULL;
	struct SwsContext *sws_ctx = NULL;
	struct SwsContext *old_sws_ctx = NULL;
	AVPacket packet;
	int64_t videoDuration = -1;
	int64_t startTime = -1;

	int videoHeight = 0;
	int videoWidth = 0;

	int videoHeightNew = 0;
	int videoWidthNew = 0;

	double step = 0;
	double curPos = 0;

	//AVPacket packet = { .data = NULL,.size = 0 };
	AVFrame *srcFrame = NULL;
	//AVFrame *dstFrame = NULL;
	uint8_t *dst_data[4];
	int dst_linesize[4];


	int framefinished = 0;
	int isNeedRetConv = 0;
	ScreenList retList;
	retList.ImgCount = 0;
	//инициализация
	if ((ret = avformat_open_input(&fmt_ctx, filename, NULL, NULL))) 
	{
		const char* errText = av_err2str(ret);
		return retList;
	}
		
	if ((ret = avformat_find_stream_info(fmt_ctx, NULL)) < 0) 
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find stream information\n");
		//return retList;
		goto exit;
	}

	ret = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
	if (ret < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot find a video stream in the input file\n");
		//return retList;
		goto exit;
	}
	video_stream = ret;
	if (!(decoder_ctx = avcodec_alloc_context3(decoder))) 
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate context\n");
		//return retList;
		goto exit;
	}

	video = fmt_ctx->streams[video_stream];
	if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
		//return retList;
		goto exit;
	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0) 
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to open codec for stream\n");
		//return retList;
		goto exit;
	}
	if (decoder_ctx->width == 0 || decoder_ctx->height == 0)
		//return retList;
		goto exit;
	if (video->duration != AV_NOPTS_VALUE) 
	{
		videoDuration = ((video->duration * video->time_base.num) / video->time_base.den) * 1000;
	}
	else
	{
		if (fmt_ctx->duration != AV_NOPTS_VALUE)
		{
			videoDuration = (fmt_ctx->duration / AV_TIME_BASE) * 1000;
		}
		else
			videoDuration = 10 * 60 * 1000;
	}
	startTime = ((video->start_time * video->time_base.num) / video->time_base.den) * 1000;
	videoWidth = decoder_ctx->width;
	videoHeight = decoder_ctx->height;
	if (ResizePercent)
	{
		videoHeightNew = videoHeight * ResizePercent / 100;
		videoWidthNew = videoWidth * ResizePercent / 100;
		sws_ctx = sws_getContext(videoWidth, videoHeight, decoder_ctx->pix_fmt, videoWidthNew, videoHeightNew, IMAGE_FORMAT, SWS_BILINEAR, NULL, NULL, NULL);
	}
	else
	{
		videoHeightNew = videoHeight;
		videoWidthNew = videoWidth;
		sws_ctx = sws_getContext(videoWidth, videoHeight, decoder_ctx->pix_fmt, videoWidthNew, videoHeightNew, IMAGE_FORMAT, SWS_BILINEAR, NULL, NULL, NULL);
	}
	if (!sws_ctx)
	{
		//return retList;
		goto exit;
	}
	//обработка видео
	retList.bufs = malloc(sizeof(ImageBuf) * AllCount);
	retList.Height = videoHeightNew;
	retList.Width = videoWidthNew;
	retList.ImgCount = AllCount;
	curPos = video->start_time;
	step = (((float)videoDuration / (float)AllCount) / (float)1000.0f) * (float)video->time_base.den / (float)video->time_base.num;
	retList.Period = (double)videoDuration / (double)AllCount;
	
	for (int i = 0; i < AllCount; i++)
	{
		retList.bufs[i].BufSize = 0;
		retList.bufs[i].ImageTime = 0;
		
		//int64_t realPos = av_rescale(curPos, video->time_base.den, video->time_base.num);
		//realPos /= video->time_base;
		ret = av_seek_frame(fmt_ctx, video_stream, curPos, AVSEEK_FLAG_BACKWARD);
		//ret = av_seek_frame(fmt_ctx, video_stream, curPos, AVSEEK_FLAG_FRAME);
		if (ret < 0)
		{
			retList.bufs[i].BufSize = 0;
			continue;
		}
		avcodec_flush_buffers(decoder_ctx);
		srcFrame = av_frame_alloc();
		//dstFrame = av_frame_alloc();
		int imgBufSize = av_image_alloc(dst_data, dst_linesize, videoWidthNew, videoHeightNew, IMAGE_FORMAT, 1);

		//ret = av_image_fill_arrays(dstFrame->data, dstFrame->linesize, dstFrame, IMAGE_FORMAT, videoWidthNew, videoHeightNew, 1);
		//avpicture_alloc(dstFrame, IMAGE_FORMAT, videoWidthNew, videoHeightNew);
		//dstFrame->width = videoWidthNew;
		//dstFrame->height = videoHeightNew;
		//dstFrame->format = IMAGE_FORMAT;
		//av_frame_get_buffer(dstFrame, 1);
		while (av_read_frame(fmt_ctx, &packet) >= 0)
		{
			if (packet.stream_index == video_stream)
			{
				ret = avcodec_send_packet(decoder_ctx, &packet);
				if (ret)
					goto badFrame;

				//ret = avcodec_receive_frame(decoder_ctx, srcFrame);
				//if (ret)
				//	goto badFrame;
				ret = avcodec_receive_frame(decoder_ctx, srcFrame);
				if(ret)
					goto badFrame;

				//while (ret = avcodec_receive_frame(decoder_ctx, srcFrame) >= 0)
				//{
				//	if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
				//		goto badFrame;
				//	else if (ret < 0)
				//	{
				//		goto badFrame;
				//	}
				//}
				framefinished = 1;
				if (framefinished)
				{
					if (srcFrame->width != videoWidth || srcFrame->height != videoHeight)
					{
						isNeedRetConv = 1;
						old_sws_ctx = sws_ctx;
						sws_ctx = sws_getContext(srcFrame->width, srcFrame->height, decoder_ctx->pix_fmt, videoWidthNew, videoHeightNew, IMAGE_FORMAT, SWS_BILINEAR, NULL, NULL, NULL);
					}

					float decodecTs = (((float)(packet.pts != AV_NOPTS_VALUE ? packet.pts : packet.dts) - (float)video->start_time) / (float)video->time_base.den) * (float)video->time_base.num;
					retList.bufs[i].ImageTime = decodecTs;
					int heightOut = sws_scale(sws_ctx, srcFrame->data, srcFrame->linesize, 0, (isNeedRetConv ? srcFrame->height : decoder_ctx->height), dst_data, dst_linesize);
					if (!heightOut) 
					{
						retList.bufs[i].BufSize = 0;
						break;
					}
					//int bs = videoWidthNew * videoHeightNew * 32 / 8;
					retList.bufs[i].ImgBuf = malloc(imgBufSize);
					retList.bufs[i].BufSize = imgBufSize;
					memcpy(retList.bufs[i].ImgBuf, dst_data[0], imgBufSize);

					if (isNeedRetConv)
					{
						isNeedRetConv = 0;
						sws_freeContext(sws_ctx);
						sws_ctx = old_sws_ctx;
					}
					av_packet_unref(&packet);
					break;
				}
			}
badFrame:
			av_packet_unref(&packet);
		}
		av_frame_free(&srcFrame);
		//av_freep(&dstFrame->data[0]);
		av_freep(&dst_data[0]);
		curPos += step;
	}
exit:
	sws_freeContext(sws_ctx);
	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&fmt_ctx);
	return retList;
}

EXPORT ImageBuf DecodeH264Frame(char* buffer, int bufSize, char* extraData, int extraDataSize) {
	return DecodeFrame(AV_CODEC_ID_H264, buffer, bufSize, extraData, extraDataSize);
}


EXPORT ImageBuf DecodeFrame(int codecId,  char* buffer, int bufSize, char* extraData, int extraDataSize) {

	enum AVPixelFormat IMAGE_FORMAT = AV_PIX_FMT_BGRA;//AV_PIX_FMT_BGR24;//AV_PIX_FMT_BGRA;
	//char * errorText;
	AVFormatContext *fmt_ctx = NULL;
	AVDictionaryEntry *tag = NULL;
	int video_stream, ret;
	AVStream *video = NULL;
	AVCodecContext *decoder_ctx = NULL;
	AVCodec *decoder = NULL;
	struct SwsContext *sws_ctx = NULL;
	struct SwsContext *old_sws_ctx = NULL;
	AVPacket *packet;
	int64_t videoDuration = -1;
	int64_t startTime = -1;

	int videoHeight = 0;
	int videoWidth = 0;

	int videoHeightNew = 0;
	int videoWidthNew = 0;

	double step = 0;
	int64_t curPos = 0;

	//AVPacket packet = { .data = NULL,.size = 0 };
	AVFrame *srcFrame = NULL;
	//AVFrame *dstFrame = NULL;
	uint8_t *dst_data[4];
	int dst_linesize[4];


	int framefinished = 0;
	int isNeedRetConv = 0;
	ImageBuf result;
	result.BufSize = 0;
	result.ImageTime = 0;
	
	//инициализация
	decoder = avcodec_find_decoder(codecId);
	if (!decoder)
		return result;
	if (!(decoder_ctx = avcodec_alloc_context3(decoder)))
	{
		av_log(NULL, AV_LOG_ERROR, "Cannot allocate context\n");
		return result;
	}
	if (extraDataSize > 0)
	{
		decoder_ctx->extradata_size = extraDataSize;
		decoder_ctx->extradata = extraData;
	}

	//video = fmt_ctx->streams[video_stream];
	//if (avcodec_parameters_to_context(decoder_ctx, video->codecpar) < 0)
	//	return retList;
	if ((ret = avcodec_open2(decoder_ctx, decoder, NULL)) < 0)
	{
		av_log(NULL, AV_LOG_ERROR, "Failed to open codec for stream\n");
		return result;
	}


	srcFrame = av_frame_alloc();
	

	//av_init_packet(&packet);
	packet = av_packet_alloc();
	packet->data = buffer;
	packet->size = bufSize;
	packet->flags |= AV_PKT_FLAG_KEY;

	ret = avcodec_send_packet(decoder_ctx, packet);
	if (ret)
		return result;

	//ret = avcodec_receive_frame(decoder_ctx, srcFrame);
	//if (ret)
	//	goto badFrame;
	ret = avcodec_receive_frame(decoder_ctx, srcFrame);
	if (ret)
		return result;
	result.Width = decoder_ctx->width;
	result.Height = decoder_ctx->height;
	int imgBufSize = av_image_alloc(dst_data, dst_linesize, decoder_ctx->width, decoder_ctx->height, IMAGE_FORMAT, 1);

	sws_ctx = sws_getContext(decoder_ctx->width, decoder_ctx->height, decoder_ctx->pix_fmt, decoder_ctx->width, decoder_ctx->height, IMAGE_FORMAT, SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws_ctx)
		return result;

	int heightOut = sws_scale(sws_ctx, srcFrame->data, srcFrame->linesize, 0, decoder_ctx->height, dst_data, dst_linesize);
	
	result.ImgBuf = malloc(imgBufSize);
	result.BufSize = imgBufSize;

	memcpy(result.ImgBuf, dst_data[0], imgBufSize);
	av_packet_unref(packet);
	av_frame_free(&srcFrame);
	//av_freep(&dstFrame->data[0]);
	av_freep(&dst_data[0]);
	sws_freeContext(sws_ctx);
	avcodec_free_context(decoder_ctx);
	//avcodec_close(decoder_ctx);
	//avcodec_free_context(&decoder_ctx);
	//avformat_close_input(&fmt_ctx);
	return result;
}


EXPORT void FreeImageList(ScreenList list)
{
	
	for (int i = 0; i < list.ImgCount; i++)
	{
		FreeImageBuffer(list.bufs[i]);
	}
}

EXPORT void FreeImageBuffer(ImageBuf buf)
{
	if (buf.BufSize)
		free(buf.ImgBuf);
}



EXPORT void message() {
	printf("Hello World");
}



