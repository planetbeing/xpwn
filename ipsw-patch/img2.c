#include <zlib.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "abstractfile.h"
#include <xpwn/img2.h>

void flipImg2Header(Img2Header* header) {
	FLIPENDIANLE(header->signature);
	FLIPENDIANLE(header->imageType);
	FLIPENDIANLE(header->unknown1);
	FLIPENDIANLE(header->security_epoch);
	FLIPENDIANLE(header->flags1);
	FLIPENDIANLE(header->dataLen);
	FLIPENDIANLE(header->dataLenPadded);
	FLIPENDIANLE(header->unknown3);
	FLIPENDIANLE(header->flags2);
	FLIPENDIANLE(header->unknown4);
	FLIPENDIANLE(header->header_checksum);
	FLIPENDIANLE(header->checksum2);
}

size_t readImg2(AbstractFile* file, void* data, size_t len) {
	InfoImg2* info = (InfoImg2*) (file->data); 
	memcpy(data, (void*)((uint8_t*)info->buffer + (uint32_t)info->offset), len);
	info->offset += (size_t)len;
	return len;
}

size_t writeImg2(AbstractFile* file, const void* data, size_t len) {
	InfoImg2* info = (InfoImg2*) (file->data);

	while((info->offset + (size_t)len) > info->header.dataLen) {
		info->header.dataLen = info->offset + (size_t)len;
		info->buffer = realloc(info->buffer, info->header.dataLen);
	}
	
	memcpy((void*)((uint8_t*)info->buffer + (uint32_t)info->offset), data, len);
	info->offset += (size_t)len;
	
	info->dirty = TRUE;
	
	return len;
}

int seekImg2(AbstractFile* file, off_t offset) {
	InfoImg2* info = (InfoImg2*) (file->data);
	info->offset = (size_t)offset;
	return 0;
}

off_t tellImg2(AbstractFile* file) {
	InfoImg2* info = (InfoImg2*) (file->data);
	return (off_t)info->offset;
}

off_t getLengthImg2(AbstractFile* file) {
	InfoImg2* info = (InfoImg2*) (file->data);
	return info->header.dataLen;
}

void closeImg2(AbstractFile* file) {
	InfoImg2* info = (InfoImg2*) (file->data);
	uint32_t cksum;
	
	if(info->dirty) {
		info->file->seek(info->file, sizeof(info->header));
		info->file->write(info->file, info->buffer, info->header.dataLen);
		info->header.dataLenPadded = info->header.dataLen;
	  
		flipImg2Header(&(info->header));
		
		cksum = crc32(0, (unsigned char *)&(info->header), 0x64);
		FLIPENDIANLE(cksum);
		info->header.header_checksum = cksum;
			
		info->file->seek(info->file, 0);
		info->file->write(info->file, &(info->header), sizeof(info->header));
	}
	
	free(info->buffer);
	info->file->close(info->file);
	free(info);
	free(file);
}


AbstractFile* createAbstractFileFromImg2(AbstractFile* file) {
	InfoImg2* info;
	AbstractFile* toReturn;

	if(!file) {
		return NULL;
	}

	info = (InfoImg2*) malloc(sizeof(InfoImg2));
	info->file = file;
	file->seek(file, 0);
	file->read(file, &(info->header), sizeof(info->header));
	flipImg2Header(&(info->header));
	if(info->header.signature != IMG2_SIGNATURE) {
		free(info);
		return NULL;
	}
	
	info->buffer = malloc(info->header.dataLen);
	file->read(file, info->buffer, info->header.dataLen);

	info->dirty = FALSE;
	
	info->offset = 0;
	
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readImg2;
	toReturn->write = writeImg2;
	toReturn->seek = seekImg2;
	toReturn->tell = tellImg2;
	toReturn->getLength = getLengthImg2;
	toReturn->close = closeImg2;
	toReturn->type = AbstractFileTypeImg2;
	return toReturn;
}

AbstractFile* duplicateImg2File(AbstractFile* file, AbstractFile* backing) {
	InfoImg2* info;
	AbstractFile* toReturn;

	if(!file) {
		return NULL;
	}

	info = (InfoImg2*) malloc(sizeof(InfoImg2));
	memcpy(info, file->data, sizeof(InfoImg2));
	
	info->file = backing;
	info->buffer = malloc(1);
	info->header.dataLen = 0;
	info->dirty = TRUE;
	info->offset = 0;
	
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readImg2;
	toReturn->write = writeImg2;
	toReturn->seek = seekImg2;
	toReturn->tell = tellImg2;
	toReturn->getLength = getLengthImg2;
	toReturn->close = closeImg2;
	toReturn->type = AbstractFileTypeImg2;
	return toReturn;
}

