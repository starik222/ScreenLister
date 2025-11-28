#include <stdio.h>
#include <string.h>
#include <Windows.h>
#include "ScreenLister.h"




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
	//ImageBuf img1 = GetImageFromVideoFile(argv[1], 0);
	//getch();
	//return 0;
	//FILE* f;
	//errno_t err;
	//err = fopen_s(&f, argv[1], "r");
	//ImageBuf img;
	//if (!err)
	//{
	//	fseek(f, 0, SEEK_END);
	//	int size = ftell(f);
	//	fseek(f, 0, SEEK_SET);
	//	char* buf = malloc(size);
	//	fread_s(buf, size, size, 1, f);
	//	fclose(f);
	//	img = GetImageFromVideoBuffer(buf, size);
	//	printf_s("get image size: %d", img.BufSize);
	//}

	if (argc != 2) {
		printf("Usage: %s <directory>\n", argv[0]);
		return 1;
	}

	char searchPath[MAX_PATH];
	snprintf(searchPath, MAX_PATH, "%s\\*.mp4", argv[1]);

	WIN32_FIND_DATA findFileData;
	HANDLE hFind = FindFirstFile(searchPath, &findFileData);

	if (hFind == INVALID_HANDLE_VALUE) {
		printf("No AVI files found or directory doesn't exist\n");
		return 1;
	}

	do {
		if (!(findFileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			char inputPath[MAX_PATH];
			snprintf(inputPath, MAX_PATH, "%s\\%s", argv[1], findFileData.cFileName);

			char outputPath[MAX_PATH];
			strcpy(outputPath, inputPath);
			char* dot = strrchr(outputPath, '.');
			if (dot) *dot = '\0';
			strcat(outputPath, ".bmp");

			printf("Processing: %s\n", inputPath);
			ScreenList srcList = GetImagesV2(inputPath, 10, 0);
			FreeImageList(srcList);
			ImageBuf image = GetImageFromVideoFile(inputPath, 0);
			if (image.ImgBuf == NULL || image.BufSize == 0) {
				printf("Error processing file: %s\n", inputPath);
				continue;
			}

			//FILE* bmpFile = fopen(outputPath, "wb");
			//if (!bmpFile) {
			//	printf("Error creating output file: %s\n", outputPath);
			//	continue;
			//}

			//fwrite(image.ImgBuf, 1, image.BufSize, bmpFile);
			//fclose(bmpFile);

			printf("Created: %s\n", outputPath);
			FreeImageBuffer(image);
		}
	} while (FindNextFile(hFind, &findFileData) != 0);

	FindClose(hFind);
	return 0;
}

