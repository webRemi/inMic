#include <Windows.h>
#include <stdio.h>
#include "info.h"
#include "fetchfile.h"

FileData fetchFile(LPCSTR lpFileName) {
	HANDLE hFile = NULL;
	FileData fileData = { NULL, 0 };

	hFile = CreateFileA(lpFileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		NOT("CreateFile failed with error: 0x%x", GetLastError());
		exit(1);
	}
	YES("Opened file successfully");

	fileData.sContentSize = GetFileSize(hFile, NULL);
	if (fileData.sContentSize == INVALID_FILE_SIZE) {
		NOT("GetFileSize failed with error: 0x%x", GetLastError());
		exit(1);
	}
	YES("FileSize is: %zu", fileData.sContentSize);

	fileData.cContent = (char*)malloc(fileData.sContentSize + 1);
	if (fileData.cContent == NULL) {
		NOT("Malloc failed with error: 0x%x", GetLastError());
		exit(1);
	}
	YES("Successfully allocated % zu bytes of memory", fileData.sContentSize + 1);

	DWORD dwBytesRead;
	if (!ReadFile(hFile, fileData.cContent, fileData.sContentSize, &dwBytesRead, NULL)) {
		NOT("ReadFile failed with error: 0x%x", GetLastError());
		exit(1);
	}

	if (!CloseHandle(hFile)) {
		NOT("CloseHandle failed with error: 0x%x", GetLastError());
		exit(1);
	}
	fileData.cContent[fileData.sContentSize] = '\0';
	return fileData;
}