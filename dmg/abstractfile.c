#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include "dmg.h"

size_t freadWrapper(AbstractFile* file, void* data, size_t len) {
  return fread(data, 1, len, (FILE*) (file->data));
}

size_t fwriteWrapper(AbstractFile* file, const void* data, size_t len) {
  return fwrite(data, 1, len, (FILE*) (file->data));
}

int fseekWrapper(AbstractFile* file, off_t offset) {
  return fseeko((FILE*) (file->data), offset, SEEK_SET);
}

off_t ftellWrapper(AbstractFile* file) {
  return ftello((FILE*) (file->data));
}

void fcloseWrapper(AbstractFile* file) {
  fclose((FILE*) (file->data));
  free(file);
}

off_t fileGetLength(AbstractFile* file) {
	off_t length;
	off_t pos;

	pos = ftello((FILE*) (file->data));

	fseeko((FILE*) (file->data), 0, SEEK_END);
	length = ftello((FILE*) (file->data));

	fseeko((FILE*) (file->data), pos, SEEK_SET);

	return length;
}

AbstractFile* createAbstractFileFromFile(FILE* file) {
	AbstractFile* toReturn;

	if(file == NULL) {
		return NULL;
	}

	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = file;
	toReturn->read = freadWrapper;
	toReturn->write = fwriteWrapper;
	toReturn->seek = fseekWrapper;
	toReturn->tell = ftellWrapper;
	toReturn->getLength = fileGetLength;
	toReturn->close = fcloseWrapper;
	return toReturn;
}

size_t dummyRead(AbstractFile* file, void* data, size_t len) {
  return 0;
}

size_t dummyWrite(AbstractFile* file, const void* data, size_t len) {
  *((off_t*) (file->data)) += len;
  return len;
}

int dummySeek(AbstractFile* file, off_t offset) {
  *((off_t*) (file->data)) = offset;
  return 0;
}

off_t dummyTell(AbstractFile* file) {
  return *((off_t*) (file->data));
}

void dummyClose(AbstractFile* file) {
  free(file);
}

AbstractFile* createAbstractFileFromDummy() {
	AbstractFile* toReturn;
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = NULL;
	toReturn->read = dummyRead;
	toReturn->write = dummyWrite;
	toReturn->seek = dummySeek;
	toReturn->tell = dummyTell;
	toReturn->getLength = NULL;
	toReturn->close = dummyClose;
	return toReturn;
}

size_t memRead(AbstractFile* file, void* data, size_t len) {
  MemWrapperInfo* info = (MemWrapperInfo*) (file->data); 
  memcpy(data, (void*)((uint64_t)info->buffer + (uint64_t)info->offset), len);
  info->offset += (size_t)len;
  return len;
}

size_t memWrite(AbstractFile* file, const void* data, size_t len) {
  MemWrapperInfo* info = (MemWrapperInfo*) (file->data);
  
  while((info->offset + (size_t)len) > info->bufferSize) {
    info->bufferSize <<= 1;
    info->buffer = realloc(info->buffer, info->bufferSize);
  }
  
  memcpy((void*)((uint64_t)info->buffer + (uint64_t)info->offset), data, len);
  info->offset += (size_t)len;
  return len;
}

int memSeek(AbstractFile* file, off_t offset) {
  MemWrapperInfo* info = (MemWrapperInfo*) (file->data);
  info->offset = (size_t)offset;
  return 0;
}

off_t memTell(AbstractFile* file) {
  MemWrapperInfo* info = (MemWrapperInfo*) (file->data);
  return (off_t)info->offset;
}

off_t memGetLength(AbstractFile* file) {
  MemWrapperInfo* info = (MemWrapperInfo*) (file->data);
  return info->bufferSize;
}

void memClose(AbstractFile* file) {
  free(file->data);
  free(file);
}

AbstractFile* createAbstractFileFromMemory(void* buffer, size_t size) {
	MemWrapperInfo* info;
	AbstractFile* toReturn;
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	
	info = (MemWrapperInfo*) malloc(sizeof(MemWrapperInfo));
	info->offset = 0;
	info->buffer = buffer;
	info->bufferSize = size;

	toReturn->data = info;
	toReturn->read = memRead;
	toReturn->write = memWrite;
	toReturn->seek = memSeek;
	toReturn->tell = memTell;
	toReturn->getLength = memGetLength;
	toReturn->close = memClose;
	return toReturn;
}

void abstractFilePrint(AbstractFile* file, const char* format, ...) {
	va_list args;
	char buffer[1024];
	size_t length;

	buffer[0] = '\0';
	va_start(args, format);
	length = vsprintf(buffer, format, args);
	va_end(args);
	ASSERT(file->write(file, buffer, length) == length, "fwrite");
}
