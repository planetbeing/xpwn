#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "abstractfile.h"
#include <xpwn/lzssfile.h>
#include <xpwn/lzss.h>
#include <xpwn/libxpwn.h>

void flipCompHeader(CompHeader* header) {
	FLIPENDIAN(header->signature);
	FLIPENDIAN(header->compression_type);
	FLIPENDIAN(header->checksum);
	FLIPENDIAN(header->length_uncompressed);
	FLIPENDIAN(header->length_compressed);
}

size_t readComp(AbstractFile* file, void* data, size_t len) {
	InfoComp* info = (InfoComp*) (file->data); 
	memcpy(data, (void*)((uint8_t*)info->buffer + (uint32_t)info->offset), len);
	info->offset += (size_t)len;
	return len;
}

size_t writeComp(AbstractFile* file, const void* data, size_t len) {
	InfoComp* info = (InfoComp*) (file->data);

	while((info->offset + (size_t)len) > info->header.length_uncompressed) {
		info->header.length_uncompressed = info->offset + (size_t)len;
		info->buffer = realloc(info->buffer, info->header.length_uncompressed);
	}
	
	memcpy((void*)((uint8_t*)info->buffer + (uint32_t)info->offset), data, len);
	info->offset += (size_t)len;
	
	info->dirty = TRUE;
	
	return len;
}

int seekComp(AbstractFile* file, off_t offset) {
	InfoComp* info = (InfoComp*) (file->data);
	info->offset = (size_t)offset;
	return 0;
}

off_t tellComp(AbstractFile* file) {
	InfoComp* info = (InfoComp*) (file->data);
	return (off_t)info->offset;
}

off_t getLengthComp(AbstractFile* file) {
	InfoComp* info = (InfoComp*) (file->data);
	return info->header.length_uncompressed;
}

void closeComp(AbstractFile* file) {
	InfoComp* info = (InfoComp*) (file->data);
	uint8_t *compressed;
	if(info->dirty) {
		info->header.checksum = lzadler32((uint8_t*)info->buffer, info->header.length_uncompressed);
		
		compressed = malloc(info->header.length_uncompressed * 2);
		info->header.length_compressed = (uint32_t)(compress_lzss(compressed, info->header.length_uncompressed * 2, info->buffer, info->header.length_uncompressed) - compressed);
		
		info->file->seek(info->file, sizeof(info->header));
		info->file->write(info->file, compressed, info->header.length_compressed);

		free(compressed);

		flipCompHeader(&(info->header));
		info->file->seek(info->file, 0);
		info->file->write(info->file, &(info->header), sizeof(info->header));
	}
	
	free(info->buffer);
	info->file->close(info->file);
	free(info);
	free(file);
}


AbstractFile* createAbstractFileFromComp(AbstractFile* file) {
	InfoComp* info;
	AbstractFile* toReturn;
	uint8_t *compressed;

	if(!file) {
		return NULL;
	}

	info = (InfoComp*) malloc(sizeof(InfoComp));
	info->file = file;
	file->seek(file, 0);
	file->read(file, &(info->header), sizeof(info->header));
	flipCompHeader(&(info->header));
	if(info->header.signature != COMP_SIGNATURE) {
		free(info);
		return NULL;
	}
	
	if(info->header.compression_type != LZSS_SIGNATURE) {
		free(info);
		return NULL;
	}
	
	info->buffer = malloc(info->header.length_uncompressed);
	compressed = malloc(info->header.length_compressed);
	file->read(file, compressed, info->header.length_compressed);

	uint32_t real_uncompressed = decompress_lzss(info->buffer, compressed, info->header.length_compressed);
	if(real_uncompressed != info->header.length_uncompressed) {
		XLOG(5, "mismatch: %d %d %d %x %x\n", info->header.length_compressed, real_uncompressed, info->header.length_uncompressed, compressed[info->header.length_compressed - 2], compressed[info->header.length_compressed - 1]);
		free(compressed);
		free(info);
		return NULL;
	}

	XLOG(5, "match: %d %d %d %x %x\n", info->header.length_compressed, real_uncompressed, info->header.length_uncompressed, compressed[info->header.length_compressed - 2], compressed[info->header.length_compressed - 1]);
	
	free(compressed);

	info->dirty = FALSE;
	
	info->offset = 0;
	
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readComp;
	toReturn->write = writeComp;
	toReturn->seek = seekComp;
	toReturn->tell = tellComp;
	toReturn->getLength = getLengthComp;
	toReturn->close = closeComp;
	toReturn->type = AbstractFileTypeLZSS;

	return toReturn;
}

AbstractFile* duplicateCompFile(AbstractFile* file, AbstractFile* backing) {
	InfoComp* info;
	AbstractFile* toReturn;

	if(!file) {
		return NULL;
	}

	info = (InfoComp*) malloc(sizeof(InfoComp));
	memcpy(info, file->data, sizeof(InfoComp));
	
	info->file = backing;
	info->buffer = malloc(1);
	info->header.length_uncompressed = 0;
	info->dirty = TRUE;
	info->offset = 0;
	
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readComp;
	toReturn->write = writeComp;
	toReturn->seek = seekComp;
	toReturn->tell = tellComp;
	toReturn->getLength = getLengthComp;
	toReturn->close = closeComp;
	toReturn->type = AbstractFileTypeLZSS;	

	return toReturn;
}

