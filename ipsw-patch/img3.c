#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <xpwn/img3.h>

void writeImg3Element(AbstractFile* file, Img3Element* element);

void writeImg3Root(AbstractFile* file, Img3Element* element);

void flipAppleImg3Header(AppleImg3Header* header) {
	FLIPENDIANLE(header->magic);
	FLIPENDIANLE(header->size);
	FLIPENDIANLE(header->dataSize);
}

void flipAppleImg3RootExtra(AppleImg3RootExtra* extra) {
	FLIPENDIANLE(extra->shshOffset);
	FLIPENDIANLE(extra->name);
}

size_t readImg3(AbstractFile* file, void* data, size_t len) {
	Img3Info* info = (Img3Info*) file->data;
	memcpy(data, (void*)((uint8_t*)info->data->data + (uint32_t)info->offset), len);
	info->offset += (size_t)len;
	return len;
}

size_t writeImg3(AbstractFile* file, const void* data, size_t len) {
	Img3Info* info = (Img3Info*) file->data;

	while((info->offset + (size_t)len) > info->data->header->dataSize) {
		info->data->header->dataSize = info->offset + (size_t)len;
		info->data->header->size = info->data->header->dataSize + sizeof(AppleImg3Header);
		if(info->data->header->size % 4 != 0) {
			info->data->header->size += 4 - (info->data->header->size % 4);
		}
		info->data->data = realloc(info->data->data, info->data->header->dataSize);
	}
	
	memcpy((void*)((uint8_t*)info->data->data + (uint32_t)info->offset), data, len);
	info->offset += (size_t)len;
	
	info->dirty = TRUE;
	
	return len;
}

int seekImg3(AbstractFile* file, off_t offset) {
	Img3Info* info = (Img3Info*) file->data;
	info->offset = (size_t)offset;
	return 0;
}

off_t tellImg3(AbstractFile* file) {
	Img3Info* info = (Img3Info*) file->data;
	return (off_t)info->offset;
}

off_t getLengthImg3(AbstractFile* file) {
	Img3Info* info = (Img3Info*) file->data;
	return info->data->header->dataSize;
}

void closeImg3(AbstractFile* file) {
	int i;
	Img3Info* info = (Img3Info*) file->data;

	if(info->dirty) {
		info->file->seek(info->file, 0);
		info->root->header->dataSize = 0;	/* hack to make certain writeImg3Element doesn't preallocate */
		info->root->header->size = 0;
		writeImg3Element(info->file, info->root);
	}

	info->root->free(info->root);
	info->file->close(info->file);
	free(info);
	free(file);
}

Img3Element* readImg3Element(AbstractFile* file);

void freeImg3Default(Img3Element* element) {
	free(element->header);
	free(element->data);
	free(element);
}

void freeImg3Root(Img3Element* element) {
	Img3Element* current;
	Img3Element* toFree;

	free(element->header);

	current = (Img3Element*)(element->data);

	while(current != NULL) {
		toFree = current;
		current = current->next;
		toFree->free(toFree);
	}

	free(element);
}

void readImg3Root(AbstractFile* file, Img3Element* element) {
	Img3Element* children;
	Img3Element* current;
	uint32_t remaining;
	AppleImg3RootHeader* header;

	children = NULL;

	header = (AppleImg3RootHeader*) realloc(element->header, sizeof(AppleImg3RootHeader));
	element->header = (AppleImg3Header*) header;

	file->read(file, &(header->extra), sizeof(AppleImg3RootExtra));
	flipAppleImg3RootExtra(&(header->extra));

	remaining = header->base.dataSize;

	while(remaining > 0) {
		if(children != NULL) {
			current->next = readImg3Element(file);
			current = current->next;
		} else {
			current = readImg3Element(file);
			children = current;
		}
		remaining -= current->header->size;
	}

	element->data = (void*) children;
	element->write = writeImg3Root;
	element->free = freeImg3Root;
}

void writeImg3Root(AbstractFile* file, Img3Element* element) {
	AppleImg3RootHeader* header;
	Img3Element* current;
	off_t curPos;

	curPos = file->tell(file);
	curPos -= sizeof(AppleImg3Header);

	file->seek(file, curPos + sizeof(AppleImg3RootHeader));

	header = (AppleImg3RootHeader*) element->header;

	current = (Img3Element*) element->data;
	while(current != NULL) {
		if(current->header->magic == IMG3_SHSH_MAGIC) {
			header->extra.shshOffset = (uint32_t)(file->tell(file) - sizeof(AppleImg3RootHeader));
		}

		writeImg3Element(file, current);
		current = current->next;
	}

	header->base.dataSize = file->tell(file) - (curPos + sizeof(AppleImg3RootHeader));
	header->base.size = sizeof(AppleImg3RootHeader) + header->base.dataSize;

	file->seek(file, curPos);

	flipAppleImg3Header(&(header->base));
	flipAppleImg3RootExtra(&(header->extra));
	file->write(file, header, sizeof(AppleImg3RootHeader));
	flipAppleImg3RootExtra(&(header->extra));
	flipAppleImg3Header(&(header->base));

	file->seek(file, header->base.size);
}

void writeImg3Default(AbstractFile* file, Img3Element* element) {
	file->write(file, element->data, element->header->dataSize);
}

void writeImg3Element(AbstractFile* file, Img3Element* element) {
	off_t curPos;
	char zero;

	zero = '\0';

	curPos = file->tell(file);

	file->seek(file, curPos + element->header->size - 1);
	file->write(file, &zero, 1);

	file->seek(file, curPos);

	flipAppleImg3Header(element->header);
	file->write(file, element->header, sizeof(AppleImg3Header));
	flipAppleImg3Header(element->header);

	element->write(file, element);

	file->seek(file, curPos + element->header->size);
}

Img3Element* readImg3Element(AbstractFile* file) {
	Img3Element* toReturn;
	AppleImg3Header* header;
	off_t curPos;

	curPos = file->tell(file);

	header = (AppleImg3Header*) malloc(sizeof(AppleImg3Header));
	file->read(file, header, sizeof(AppleImg3Header));
	flipAppleImg3Header(header);

	toReturn = (Img3Element*) malloc(sizeof(Img3Element));
	toReturn->header = header;
	toReturn->next = NULL;

	switch(header->magic) {
		case IMG3_MAGIC:
			readImg3Root(file, toReturn);
			break;

		default:
			toReturn->data = (unsigned char*) malloc(header->dataSize);
			toReturn->write = writeImg3Default;
			toReturn->free = freeImg3Default;
			file->read(file, toReturn->data, header->dataSize);
	}

	file->seek(file, curPos + toReturn->header->size);

	return toReturn;
}

AbstractFile* createAbstractFileFromImg3(AbstractFile* file) {
	AbstractFile* toReturn;
	Img3Info* info;
	Img3Element* current;

	if(!file) {
		return NULL;
	}

	file->seek(file, 0);

	info = (Img3Info*) malloc(sizeof(Img3Info));
	info->file = file;
	info->root = readImg3Element(file);

	info->data = NULL;

	current = (Img3Element*) info->root->data;
	while(current != NULL) {
		if(current->header->magic == IMG3_DATA_MAGIC) {
			info->data = current;
			break;
		}
		current = current->next;
	}

	info->offset = 0;
	info->dirty = FALSE;

	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readImg3;
	toReturn->write = writeImg3;
	toReturn->seek = seekImg3;
	toReturn->tell = tellImg3;
	toReturn->getLength = getLengthImg3;
	toReturn->close = closeImg3;
	return toReturn;
}
AbstractFile* duplicateImg3File(AbstractFile* file, AbstractFile* backing) {
	Img3Info* info;
	AbstractFile* toReturn;

	if(!file) {
		return NULL;
	}

	toReturn = createAbstractFileFromImg3(((Img3Info*)file->data)->file);
	info = (Img3Info*)toReturn->data;

	info->file = backing;
	info->offset = 0;
	info->dirty = TRUE;
	info->data->header->dataSize = 0;
	info->data->header->size = info->data->header->dataSize + sizeof(AppleImg3Header);

	return toReturn;
}

