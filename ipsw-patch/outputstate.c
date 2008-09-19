#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "common.h"
#include <dmg/dmg.h>
#include <xpwn/outputstate.h>
#include <zip.h>
#include <unzip.h>

#ifdef WIN32
#include <windows.h>
#endif

#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)

uint64_t MaxLoadZipSize = UINT64_MAX;

void addToOutputQueue(OutputState** state, const char* fileName, void* buffer, const size_t bufferSize, char* tmpFileName) {
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
		if(newFile->tmpFileName) {
			unlink(newFile->tmpFileName);
			free(newFile->tmpFileName);
		}
		free(newFile->fileName);

		if(newFile->buffer) {
			free(newFile->buffer);
		}
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
	newFile->tmpFileName = tmpFileName;
	newFile->buffer = buffer;
	newFile->bufferSize = bufferSize;
}

void addToOutput2(OutputState** state, const char* fileName, void* buffer, const size_t bufferSize, char* tmpFileName) {
	char* fileNamePath;
	char* fileNameNoPath;

	fileNamePath = (char*) malloc(sizeof(char) * (strlen(fileName) + 1));
	strcpy(fileNamePath, fileName);
	fileNameNoPath = fileNamePath + strlen(fileNamePath);

	if(*fileNamePath != '/') {
		while(fileNameNoPath > fileNamePath) {
			if(*fileNameNoPath == '/') {
				*(fileNameNoPath + 1) = '\0';
				addToOutputQueue(state, fileNamePath, malloc(1), 0, NULL);
			}

			fileNameNoPath--;
		}
	}

	free(fileNamePath);

	addToOutputQueue(state, fileName, buffer, bufferSize, tmpFileName);
}

void addToOutput(OutputState** state, const char* fileName, void* buffer, const size_t bufferSize) {
	addToOutput2(state, fileName, buffer, bufferSize, NULL);
}

void removeFileFromOutputState(OutputState** state, const char* fileName) {
	OutputState* curFile;

	curFile = *state;
	while(curFile != NULL) {
		if(strcmp(curFile->fileName, fileName) == 0) {
			if(curFile->prev == NULL) {
				*state = curFile->next;
				(*state)->next->prev = NULL;
			} else {
				curFile->prev->next = curFile->next;
				curFile->next->prev = curFile->prev;
			}
			curFile->prev = NULL;
			curFile->next = NULL;
			releaseOutput(&curFile);
			return;
		}
		curFile = curFile->next;
	}
}

AbstractFile* getFileFromOutputState(OutputState** state, const char* fileName) {
	OutputState* curFile;

	curFile = *state;
	while(curFile != NULL) {
		if(strcmp(curFile->fileName, fileName) == 0) {
			if(curFile->tmpFileName == NULL) 
				return createAbstractFileFromMemory(&(curFile->buffer), curFile->bufferSize);
			else
				return createAbstractFileFromFile(fopen(curFile->tmpFileName, "rb"));
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
			if(curFile->tmpFileName == NULL) {
				bufSize = curFile->bufferSize;
				curFile->bufferSize = 0;
				return createAbstractFileFromMemoryFileBuffer(&(curFile->buffer), &curFile->bufferSize, bufSize);
			} else {
				return createAbstractFileFromFile(fopen(curFile->tmpFileName, "r+b"));
			}
		}
		curFile = curFile->next;
	}

	return NULL;
}

AbstractFile* getFileFromOutputStateForReplace(OutputState** state, const char* fileName) {
	OutputState* curFile;
	size_t bufSize;

	curFile = *state;
	while(curFile != NULL) {
		if(strcmp(curFile->fileName, fileName) == 0) {
			if(curFile->tmpFileName == NULL) {
				bufSize = curFile->bufferSize;
				curFile->bufferSize = 0;
				return createAbstractFileFromMemoryFileBuffer(&(curFile->buffer), &curFile->bufferSize, bufSize);
			} else {
				return createAbstractFileFromFile(fopen(curFile->tmpFileName, "wb"));
			}
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
		printf("packing: %s (%ld)\n", curFile->fileName, (long) curFile->bufferSize); fflush(stdout);
		
		if(curFile->bufferSize > 0) {
			ASSERT(zipOpenNewFileInZip(zip, curFile->fileName, &info, NULL, 0, NULL, 0, NULL, Z_DEFLATED, Z_DEFAULT_COMPRESSION) == 0, "error adding to zip");
			if(curFile->tmpFileName == NULL) {
				ASSERT(zipWriteInFileInZip(zip, curFile->buffer, curFile->bufferSize) == 0, "error writing to zip");
			} else {
				AbstractFile* tmpFile = createAbstractFileFromFile(fopen(curFile->tmpFileName, "rb"));
				char* buffer = malloc(DEFAULT_BUFFER_SIZE);
				size_t left = tmpFile->getLength(tmpFile);
				while(left > 0) {
					size_t toRead;
					if(left > DEFAULT_BUFFER_SIZE)
						toRead = DEFAULT_BUFFER_SIZE;
					else
						toRead = left;
					ASSERT(tmpFile->read(tmpFile, buffer, toRead) == toRead, "error reading data");
					ASSERT(zipWriteInFileInZip(zip, buffer, toRead) == 0, "error writing to zip");
					left -= toRead;
				}
				tmpFile->close(tmpFile);
				free(buffer);
			}
		} else {
			ASSERT(zipOpenNewFileInZip(zip, curFile->fileName, &info, NULL, 0, NULL, 0, NULL, 0, 0) == 0, "error adding to zip");
		}
		ASSERT(zipCloseFileInZip(zip) == 0, "error closing file in zip");
		if(curFile->tmpFileName) {
			unlink(curFile->tmpFileName);
			free(curFile->tmpFileName);
		}
		free(curFile->fileName);

		if(curFile->buffer) {
			free(curFile->buffer);
		}

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

		if(curFile->tmpFileName) {
			unlink(curFile->tmpFileName);
			free(curFile->tmpFileName);
		}

		free(curFile->fileName);

		if(curFile->buffer) {
			free(curFile->buffer);
		}

		free(curFile);
	}
	*state = NULL;
}

char* createTempFile() {
	char tmpFileBuffer[512];
#ifdef WIN32
	char tmpFilePath[512];
	GetTempPath(512, tmpFilePath);
	GetTempFileName(tmpFilePath, "pwn", 0, tmpFileBuffer);
	CloseHandle(CreateFile(tmpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
#else
	strcpy(tmpFileBuffer, "/tmp/pwnXXXXXX");
	close(mkstemp(tmpFileBuffer));
	FILE* tFile = fopen(tmpFileBuffer, "wb");
	fclose(tFile);
#endif

	return strdup(tmpFileBuffer);
}

OutputState* loadZip(const char* ipsw) {
	return loadZip2(ipsw, FALSE);
}

OutputState* loadZip2(const char* ipsw, int useMemory) {
	OutputState* toReturn = NULL;

	loadZipFile2(ipsw, &toReturn, NULL, useMemory);

	return toReturn;
}

void loadZipFile(const char* ipsw, OutputState** output, const char* file) {
	loadZipFile2(ipsw, output, file, TRUE);
}

void loadZipFile2(const char* ipsw, OutputState** output, const char* file, int useMemory) {
	char* fileName;
	void* buffer;
	unzFile zip;
	unz_file_info pfile_info;

	ASSERT(zip = unzOpen(ipsw), "cannot open input ipsw");
	ASSERT(unzGoToFirstFile(zip) == UNZ_OK, "cannot seek to first file in input ipsw");

	do {
		ASSERT(unzGetCurrentFileInfo(zip, &pfile_info, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file info from ipsw");
		fileName = (char*) malloc(pfile_info.size_filename + 1);
		ASSERT(unzGetCurrentFileInfo(zip, NULL, fileName, pfile_info.size_filename + 1, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file name from ipsw");
		if(((file == NULL && fileName[strlen(fileName) - 1] != '/') || (file != NULL && strcmp(fileName, file)) == 0) && pfile_info.uncompressed_size <= MaxLoadZipSize) {
			printf("loading: %s (%ld)\n", fileName, pfile_info.uncompressed_size); fflush(stdout);
			ASSERT(unzOpenCurrentFile(zip) == UNZ_OK, "cannot open compressed file in IPSW");
			if(useMemory) {
				buffer = malloc((pfile_info.uncompressed_size > 0) ? pfile_info.uncompressed_size : 1);
				ASSERT(unzReadCurrentFile(zip, buffer, pfile_info.uncompressed_size) == pfile_info.uncompressed_size, "cannot read file from ipsw");
				addToOutput(output, fileName, buffer, pfile_info.uncompressed_size);
			} else {
				char* tmpFileName = createTempFile();
				FILE* tmpFile = fopen(tmpFileName, "wb");
				buffer = malloc(DEFAULT_BUFFER_SIZE);
				size_t left = pfile_info.uncompressed_size;
				while(left > 0) {
					size_t toRead;
					if(left > DEFAULT_BUFFER_SIZE)
						toRead = DEFAULT_BUFFER_SIZE;
					else
						toRead = left;
					ASSERT(unzReadCurrentFile(zip, buffer, toRead) == toRead, "cannot read file from ipsw");
					fwrite(buffer, toRead, 1, tmpFile);
					left -= toRead;
				}
				fclose(tmpFile);
				free(buffer);
				addToOutput2(output, fileName, NULL, pfile_info.uncompressed_size, tmpFileName);
			}
			ASSERT(unzCloseCurrentFile(zip) == UNZ_OK, "cannot close compressed file in IPSW");
		}
		free(fileName);
	} while(unzGoToNextFile(zip) == UNZ_OK);

	ASSERT(unzClose(zip) == UNZ_OK, "cannot close input ipsw file");
}


