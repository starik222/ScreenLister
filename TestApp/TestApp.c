#include <stdio.h>
#include "ScreenLister.h"
#include <Windows.h>





int main(int argc, char **argv)
{
	//if (argc != 2) {
	//	printf("usage: %s <input_file>\n"
	//		"example program to demonstrate the use of the libavformat metadata API.\n"
	//		"\n", argv[0]);
	//	return 1;
	//}
	//ReadFile("H:\\ScreenLister\\x64\\Debug\\frameData.dat");

	//-------------------
	//HANDLE hFile;

	//hFile = CreateFile("H:\\ScreenLister\\x64\\Debug\\frameData.dat",                // name of the write
	//	GENERIC_READ,          // open for writing
	//	0,                      // do not share
	//	NULL,                   // default security
	//	OPEN_EXISTING,             // create new file only
	//	FILE_ATTRIBUTE_NORMAL,  // normal file
	//	NULL);                  // no attr. template

	//DWORD fSize = GetFileSize(hFile, NULL);
	//DWORD readed = 0;
	//unsigned char * bufferData = (unsigned char*)malloc(fSize);
	//ReadFile(hFile, bufferData, fSize, &readed, NULL);
	//CloseHandle(hFile);

	//hFile = CreateFile("H:\\ScreenLister\\x64\\Debug\\extraData.dat",                // name of the write
	//	GENERIC_READ,          // open for writing
	//	0,                      // do not share
	//	NULL,                   // default security
	//	OPEN_EXISTING,             // create new file only
	//	FILE_ATTRIBUTE_NORMAL,  // normal file
	//	NULL);                  // no attr. template

	//DWORD fSize2 = GetFileSize(hFile, NULL);
	//unsigned char * bufferExtra = (unsigned char*)malloc(fSize2);
	//ReadFile(hFile, bufferExtra, fSize2, &readed, NULL);
	//CloseHandle(hFile);

	//ImageBuf b = DecodeH264Frame(bufferData, fSize, bufferExtra, fSize2);
	//free(bufferData);
	//free(bufferExtra);
	//FreeImageBuffer(b);
	//-------------------------
	//FILE * pFileData2 = fopen("H:\\ScreenLister\\x64\\Debug\\extraData.dat", "rb");
	//fseek(pFileData2, 0, SEEK_END);
	//long lSize2 = ftell(pFileData2);
	//rewind(pFileData2);

	//char * bufferExtra = (char*)malloc(lSize2);
	//fread(bufferExtra, 1, lSize2, pFileData2);
	//fclose(pFileData2);

	//ImageBuf b = DecodeH264Frame(buffer, lSize, bufferExtra, lSize2);
	ImageBuf img1 = GetImageFromVideoFile(argv[1]);
	getch();
	return 0;
	FILE* f;
	errno_t err;
	err = fopen_s(&f, argv[1], "r");
	ImageBuf img;
	if (!err)
	{
		fseek(f, 0, SEEK_END);
		int size = ftell(f);
		fseek(f, 0, SEEK_SET);
		char* buf = malloc(size);
		fread_s(buf, size, size, 1, f);
		fclose(f);
		img = GetImageFromVideoBuffer(buf, size);
		printf_s("get image size: %d", img.BufSize);
	}
	
	
	
	//ScreenList list = GetImages(argv[1], 30, 300, 30);
	//
	//FreeImageList(list);
	getch();
}

