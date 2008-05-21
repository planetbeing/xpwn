#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include "dmg.h"
#include "outputstate.h"
#include <contrib/minizip/zip.h>
#include <contrib/minizip/unzip.h>

void addToOutputQueue(OutputState** state, char* fileName, void* buffer, size_t bufferSize) {
	OutputState* leftNeighbor;
	OutputState* rightNeighbor;
	OutputState* next;
	OutputState* newFile;
	int ret;
	char* myName;

	myName = malloc(sizeof(char) * (strlen(fileName) + 1));
	strcpy(myName, fileName);

	leftNeighbor = NULL;
	rightNeighbor = NULL;

	next = *state;
	while(next != NULL) {
		ret = strcmp(next->fileName, myName);
		if(ret > 0) {
			leftNeighbor = next->prev;
			rightNeighbor = next;
			break;
		}

		leftNeighbor = next;
		next = next->next;
	}

	if(leftNeighbor != NULL && strcasecmp(leftNeighbor->fileName, myName) == 0) {
		newFile = leftNeighbor;
		free(newFile->fileName);
		free(newFile->buffer);
	} else {
		newFile = (OutputState*) malloc(sizeof(OutputState));
		newFile->next = rightNeighbor;
		newFile->prev = leftNeighbor;
		if(leftNeighbor == NULL) {
			*state = newFile;
		} else {
			leftNeighbor->next = newFile;
		}
		if(rightNeighbor != NULL) {
			rightNeighbor->prev = newFile;
		}
	}

	newFile->fileName = myName;
	newFile->buffer = buffer;
	newFile->bufferSize = bufferSize;
}

void addToOutput(OutputState** state, char* fileName, void* buffer, size_t bufferSize) {
	char* fileNamePath;
	char* path;
	char* fileNameNoPath;
	char pathExists;
	OutputState* leftNeighbor;
	OutputState* rightNeighbor;

	fileNamePath = (char*) malloc(sizeof(char) * (strlen(fileName) + 1));
	strcpy(fileNamePath, fileName);
	fileNameNoPath = fileNamePath + strlen(fileNamePath);

	if(*fileNamePath != '/') {
		while(fileNameNoPath > fileNamePath) {
			if(*fileNameNoPath == '/') {
				*(fileNameNoPath + 1) = '\0';
				addToOutputQueue(state, fileNamePath, malloc(1), 0);
			}

			fileNameNoPath--;
		}
	}

	free(fileNamePath);

	addToOutputQueue(state, fileName, buffer, bufferSize);
}

AbstractFile* getFileFromOutputState(OutputState** state, const char* fileName) {
	OutputState* curFile;

	curFile = *state;
	while(curFile != NULL) {
		if(strcmp(curFile->fileName, fileName) == 0) {
			return createAbstractFileFromMemory(&(curFile->buffer), curFile->bufferSize);
		}
		curFile = curFile->next;
	}

	return NULL;
}

AbstractFile* getFileFromOutputStateForOverwrite(OutputState** state, const char* fileName) {
	OutputState* curFile;
	size_t bufSize;

	curFile = *state;
	while(curFile != NULL) {
		if(strcmp(curFile->fileName, fileName) == 0) {
			bufSize = curFile->bufferSize;
			curFile->bufferSize = 0;
			return createAbstractFileFromMemoryFileBuffer(&(curFile->buffer), &curFile->bufferSize, bufSize);
		}
		curFile = curFile->next;
	}

	return NULL;
}

void writeOutput(OutputState** state, char* ipsw) {
	OutputState* curFile;
	OutputState* next;
	zip_fileinfo info;
	struct tm* filedate;
	time_t tm_t;

	zipFile zip;

	tm_t = time(NULL);
	filedate = localtime(&tm_t);
	info.tmz_date.tm_sec  = filedate->tm_sec;
	info.tmz_date.tm_min  = filedate->tm_min;
	info.tmz_date.tm_hour = filedate->tm_hour;
	info.tmz_date.tm_mday = filedate->tm_mday;
	info.tmz_date.tm_mon  = filedate->tm_mon ;
	info.tmz_date.tm_year = filedate->tm_year;
	info.dosDate = 0;
	info.internal_fa = 0;
	info.external_fa = 0;
	
	ASSERT(zip = zipOpen(ipsw,  APPEND_STATUS_CREATE), "Cannot open output zip file");

	next = *state;

	while(next != NULL) {
		curFile = next;
		next = next->next;
		printf("packing: %s (%d)\n", curFile->fileName, curFile->bufferSize); fflush(stdout);
		
		if(curFile->bufferSize > 0) {
			ASSERT(zipOpenNewFileInZip(zip, curFile->fileName, &info, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION) == 0, "error adding to zip");
			ASSERT(zipWriteInFileInZip(zip, curFile->buffer, curFile->bufferSize) == 0, "error writing to zip");
		} else {
			ASSERT(zipOpenNewFileInZip(zip, curFile->fileName, &info, NULL, 0, NULL, 0, NULL, 0, 0) == 0, "error adding to zip");
		}
		ASSERT(zipCloseFileInZip(zip) == 0, "error closing file in zip");
		free(curFile->fileName);
		free(curFile->buffer);
		free(curFile);
	}

	zipClose(zip, NULL);
}

void releaseOutput(OutputState** state) {
	OutputState* curFile;
	OutputState* next;

	next = *state;
	while(next != NULL) {
		curFile = next;
		next = next->next;

		free(curFile->fileName);
		free(curFile->buffer);
		free(curFile);
	}
	*state = NULL;
}

OutputState* loadZip(const char* ipsw) {
	OutputState* toReturn;
	char* fileName;
	void* buffer;
	unzFile zip;
	unz_file_info pfile_info;

	toReturn = NULL;

	ASSERT(zip = unzOpen(ipsw), "cannot open input ipsw");
	ASSERT(unzGoToFirstFile(zip) == UNZ_OK, "cannot seek to first file in input ipsw");

	do {
		ASSERT(unzGetCurrentFileInfo(zip, &pfile_info, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file info from ipsw");
		fileName = (char*) malloc(pfile_info.size_filename + 1);
		ASSERT(unzGetCurrentFileInfo(zip, NULL, fileName, pfile_info.size_filename + 1, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file name from ipsw");
		if(fileName[strlen(fileName) - 1] != '/') {
			buffer = malloc((pfile_info.uncompressed_size > 0) ? pfile_info.uncompressed_size : 1);
			printf("loading: %s (%d)\n", fileName, pfile_info.uncompressed_size); fflush(stdout);
			ASSERT(unzOpenCurrentFile(zip) == UNZ_OK, "cannot open compressed file in IPSW");
			ASSERT(unzReadCurrentFile(zip, buffer, pfile_info.uncompressed_size) == pfile_info.uncompressed_size, "cannot read file from ipsw");
			ASSERT(unzCloseCurrentFile(zip) == UNZ_OK, "cannot close compressed file in IPSW");
			addToOutput(&toReturn, fileName, buffer, pfile_info.uncompressed_size);
		}
		free(fileName);
	} while(unzGoToNextFile(zip) == UNZ_OK);

	ASSERT(unzClose(zip) == UNZ_OK, "cannot close input ipsw file");

	return toReturn;
}

