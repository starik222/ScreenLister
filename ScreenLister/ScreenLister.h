#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#define EXPORT __declspec(dllexport)
#define IO_BUFFER_SIZE (32 * 1024)

typedef struct ImageBuf {
	float        ImageTime;
	unsigned long BufSize;
	void* ImgBuf;
	int          Width;
	int          Height;
} ImageBuf;

typedef struct MemData {
	const uint8_t* buffer;
	size_t         length;
	size_t         pos;
} MemData;

typedef struct ScreenList
{
	int Width;
	int Height;
	double Period;
	int ImgCount;
	ImageBuf* bufs;

} ScreenList;

typedef struct VideoInfo
{
	int Width;
	int Height;
} VideoInfo;




EXPORT void message(void);
EXPORT ScreenList GetImages(const char* filename, int LineCount, int AllCount, int ResizePercent);
EXPORT ScreenList GetImagesV2(const char* filename, int imagesCount, int ResizePercent);

EXPORT void FreeImageList(ScreenList list);

EXPORT VideoInfo GetVideoInfo(const char* filename);

EXPORT ImageBuf DecodeFrame(int codecId, char* buffer, int bufSize, char* extraData, int extraDataSize);
EXPORT ImageBuf DecodeH264Frame(char* buffer, int bufSize, char* extraData, int extraDataSize);

EXPORT ImageBuf GetImageFromVideoBuffer(char* buffer, int bufSize, int64_t imageTime);
EXPORT ImageBuf GetImageFromVideoFile(const char* filename, int64_t imageTime);

EXPORT void FreeImageBuffer(ImageBuf buf);