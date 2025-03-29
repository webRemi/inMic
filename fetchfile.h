#include <Windows.h>
#ifndef FETCHFILE_H
#define FETCHFILE_H

typedef struct {
	char* cContent;
	SIZE_T sContentSize;
} FileData;

FileData fetchFile(LPCSTR lpFileName);

#endif#pragma once
