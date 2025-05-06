#define EXPORT __declspec(dllexport)





typedef struct ImageBuf
{
	float ImageTime;
	unsigned long BufSize;
	void *ImgBuf;
	int Width;
	int Height;
} ImageBuf;

typedef struct ScreenList
{
	int Width;
	int Height;
	double Period;
	int ImgCount;
	ImageBuf *bufs;

} ScreenList;

typedef struct VideoInfo
{
	int Width;
	int Height;
} VideoInfo;

typedef struct MemData {
	char* buffer;
	size_t length;
	size_t pos;
} MemData;

typedef struct MyFile* FilePtr;
typedef struct MyStream* StreamPtr;

EXPORT void message(void);
EXPORT ScreenList GetImages(const char * filename, int LineCount, int AllCount, int ResizePercent);

EXPORT void FreeImageList(ScreenList list);

EXPORT VideoInfo GetVideoInfo(const char * filename);

EXPORT ImageBuf DecodeFrame(int codecId, char* buffer, int bufSize, char* extraData, int extraDataSize);
EXPORT ImageBuf DecodeH264Frame(char* buffer, int bufSize, char* extraData, int extraDataSize);

EXPORT ImageBuf GetImageFromVideoBuffer(char* buffer, int bufSize);

EXPORT void FreeImageBuffer(ImageBuf buf);

FilePtr openAVData(const char* name, char* buffer, size_t buffer_len);

int MemData_read(char* opaque, char* buf, int buf_size);
int MemData_write(char* opaque, char* buf, int buf_size);
long MemData_seek(char* opaque, long offset, int whence);