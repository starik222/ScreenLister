#include <stdio.h>
#include <stdbool.h>
#include "ScreenLister.h"
#include <libavformat\avformat.h>
#include <libavutil\dict.h>
#include <libavcodec\avcodec.h>
#include<libswscale\swscale.h>
#include <libavutil\imgutils.h>
#include <libavutil\parseutils.h>


struct MyFile {
	AVFormatContext* FmtCtx;

	StreamPtr* Streams;
	size_t StreamsSize;

	struct MemData membuf;
};

struct PacketList {
	AVPacket pkt;
	struct PacketList* next;
};

struct MyStream {
	AVCodecContext* CodecCtx;
	int StreamIdx;

	struct PacketList* Packets;

	AVFrame* Frame;

	const uint8_t* FrameData;
	size_t FrameDataSize;

	FilePtr parent;
};

static int done_init = 0;

FilePtr openAVData(const char* fname, char* buffer, size_t buffer_len)
{
	FilePtr file;

	if (buffer_len>0 && !fname)
		fname = "";

	file = (FilePtr)calloc(1, sizeof(*file));
	AVDictionary* codec_opts = NULL;
	av_dict_set(&codec_opts, "timeout", "5", 0);

	if (buffer_len > 0) //using for operations from buffer
	{
		if (file && (file->FmtCtx = avformat_alloc_context()) != NULL)
		{
			file->membuf.buffer = buffer;
			file->membuf.length = buffer_len;
			file->membuf.pos = 0;

			file->FmtCtx->pb = avio_alloc_context(NULL, 0, 0, &file->membuf,
				MemData_read, MemData_write,
				MemData_seek);
			int res = avformat_open_input(&file->FmtCtx, fname, NULL, &codec_opts);
			if (res == 0 && file->FmtCtx->pb)
			{
				if (avformat_find_stream_info(file->FmtCtx, NULL) >= 0)
					return file;
				avformat_close_input(&file->FmtCtx);
			}
			if (file->FmtCtx)
				avformat_free_context(file->FmtCtx);
			file->FmtCtx = NULL;
		}
	}
	else //using for operations from file
	{
		int res = avformat_open_input(&file->FmtCtx, fname, NULL, NULL);
		if (res == 0 && file->FmtCtx->pb)
		{
			if (avformat_find_stream_info(file->FmtCtx, NULL) >= 0)
				return file;
			avformat_close_input(&file->FmtCtx);
		}
		if (file->FmtCtx)
			avformat_free_context(file->FmtCtx);
		file->FmtCtx = NULL;
	}


	free(file);
	return NULL;
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

	//инициализация
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




int MemData_read(char* opaque, char* buf, int buf_size)
{
	struct MemData* membuf = (struct MemData*)opaque;
	int rem = membuf->length - membuf->pos;

	if (rem > buf_size)
		rem = buf_size;

	memcpy((uint8_t*)buf, &membuf->buffer[membuf->pos], rem);
	membuf->pos += rem;

	return rem;
}

int MemData_write(char* opaque, char* buf, int buf_size)
{
	struct MemData* membuf = (struct MemData*)opaque;
	int rem = membuf->length - membuf->pos;

	if (rem > buf_size)
		rem = buf_size;

	memcpy(&membuf->buffer[membuf->pos], (uint8_t*)buf, rem);
	membuf->pos += rem;

	return rem;
}

long MemData_seek(char* opaque, long offset, int whence)
{
	struct MemData* membuf = (struct MemData*)opaque;

	whence &= ~AVSEEK_FORCE;
	switch (whence)
	{
	case SEEK_SET:
		if (offset < 0 || (uint64_t)offset > membuf->length)
			return -1;
		membuf->pos = offset;
		break;

	case SEEK_CUR:
		if ((offset >= 0 && (uint64_t)offset > membuf->length - membuf->pos) ||
			(offset < 0 && (uint64_t)(-offset) > membuf->pos))
			return -1;
		membuf->pos += offset;
		break;

	case SEEK_END:
		if (offset > 0 || (uint64_t)(-offset) > membuf->length)
			return -1;
		membuf->pos = membuf->length + offset;
		break;

	case AVSEEK_SIZE:
		return membuf->length;

	default:
		return -1;
	}

	return membuf->pos;
}

ImageBuf GetImageFromVideoBuffer(char* buffer, int bufSize)
{
	return GetImage(buffer, bufSize, NULL);
}

ImageBuf GetImageFromVideoFile(const char* filename)
{
	return GetImage(NULL, 0, filename);
}

ImageBuf GetImage(char* buffer, int bufSize, const char* filename)
{
	enum AVPixelFormat IMAGE_FORMAT = AV_PIX_FMT_BGRA;
	//AVFormatContext* fmt_ctx = NULL;
	AVCodecContext* decoder_ctx = NULL;
	struct SwsContext* sws_ctx = NULL;
	struct SwsContext* old_sws_ctx = NULL;
	AVCodec* decoder = NULL;
	AVStream* video = NULL;

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
	AVFrame* srcFrame = NULL;
	//AVFrame *dstFrame = NULL;
	uint8_t* dst_data[4];
	int dst_linesize[4];
	int framefinished = 0;
	int isNeedRetConv = 0;

	ImageBuf result;
	result.BufSize = 0;
	result.ImageTime = 0;
	int video_stream, ret;
	FilePtr ptr;
	if (bufSize > 0) 
	{
		ptr = openAVData(NULL, buffer, bufSize);
	}
	else
	{
		ptr = openAVData(filename, NULL, 0);
	}
	if (!ptr)
		return result;

	ret = av_find_best_stream(ptr->FmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &decoder, 0);
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
	video = ptr->FmtCtx->streams[video_stream];
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
		if (ptr->FmtCtx->duration != AV_NOPTS_VALUE)
		{
			videoDuration = (ptr->FmtCtx->duration / AV_TIME_BASE) * 1000;
		}
		else
			videoDuration = 10 * 60 * 1000;
	}
	startTime = ((video->start_time * video->time_base.num) / video->time_base.den) * 1000;
	videoWidth = decoder_ctx->width;
	videoHeight = decoder_ctx->height;
	videoHeightNew = videoHeight;
	videoWidthNew = videoWidth;
	result.Height = videoHeight;
	result.Width = videoWidth;
	sws_ctx = sws_getContext(videoWidth, videoHeight, decoder_ctx->pix_fmt, videoWidthNew, videoHeightNew, IMAGE_FORMAT, SWS_BILINEAR, NULL, NULL, NULL);
	if (!sws_ctx)
	{
		//return retList;
		goto exit;
	}

	srcFrame = av_frame_alloc();
	//dstFrame = av_frame_alloc();
	int imgBufSize = av_image_alloc(dst_data, dst_linesize, videoWidthNew, videoHeightNew, IMAGE_FORMAT, 1);


	while (av_read_frame(ptr->FmtCtx, &packet) >= 0)
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
			if (ret)
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
				result.ImageTime = decodecTs;
				int heightOut = sws_scale(sws_ctx, srcFrame->data, srcFrame->linesize, 0, (isNeedRetConv ? srcFrame->height : decoder_ctx->height), dst_data, dst_linesize);
				if (!heightOut)
				{
					result.BufSize = 0;
					break;
				}
				//int bs = videoWidthNew * videoHeightNew * 32 / 8;
				result.ImgBuf = malloc(imgBufSize);
				result.BufSize = imgBufSize;
				memcpy(result.ImgBuf, dst_data[0], imgBufSize);

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


exit:
	sws_freeContext(sws_ctx);
	avcodec_free_context(&decoder_ctx);
	avformat_close_input(&ptr->FmtCtx);
	free(ptr);
	return result;
}

EXPORT ScreenList GetImages(const char * filename, int LineCount, int AllCount, int ResizePercent) {

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
		if (list.bufs[i].BufSize > 0)
			free(list.bufs[i].ImgBuf);
	}
}

EXPORT void FreeImageBuffer(ImageBuf buf)
{

		if (buf.BufSize > 0)
			free(buf.ImgBuf);
}



EXPORT void message() {
	printf("Hello World");
}



