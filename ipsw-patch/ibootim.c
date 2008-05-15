#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "../hfs/common.h"
#include "../dmg/dmg.h"
#include "ibootim.h"
#include "lzss.h"
#include <png.h>
#include "nor_files.h"

void flipIBootIMHeader(IBootIMHeader* header) {
	FLIPENDIANLE(header->unknown);
	FLIPENDIANLE(header->compression_type);
	FLIPENDIANLE(header->format);
	FLIPENDIANLE(header->width);
	FLIPENDIANLE(header->height);
}

size_t readIBootIM(AbstractFile* file, void* data, size_t len) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data); 
	memcpy(data, (void*)((uint8_t*)info->buffer + (uint32_t)info->offset), len);
	info->offset += (size_t)len;
	return len;
}

size_t writeIBootIM(AbstractFile* file, const void* data, size_t len) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data);

	while((info->offset + (size_t)len) > info->length) {
		info->length = info->offset + (size_t)len;
		info->buffer = realloc(info->buffer, info->length);
	}
	
	memcpy((void*)((uint8_t*)info->buffer + (uint32_t)info->offset), data, len);
	info->offset += (size_t)len;
	
	info->dirty = TRUE;
	
	return len;
}

int seekIBootIM(AbstractFile* file, off_t offset) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data);
	info->offset = (size_t)offset;
	return 0;
}

off_t tellIBootIM(AbstractFile* file) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data);
	return (off_t)info->offset;
}

off_t getLengthIBootIM(AbstractFile* file) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data);
	return info->length;
}

void closeIBootIM(AbstractFile* file) {
	InfoIBootIM* info = (InfoIBootIM*) (file->data);
	uint32_t cksum;
	uint8_t *compressed;
	if(info->dirty) {
		compressed = malloc(info->length * 2);
		info->compLength = (uint32_t)(compress_lzss(compressed, info->length * 2, info->buffer, info->length) - compressed);
		
		flipIBootIMHeader(&(info->header));
		info->file->seek(info->file, 0);
		info->file->write(info->file, &(info->header), sizeof(info->header));
		
		info->file->seek(info->file, sizeof(info->header));
		info->file->write(info->file, compressed, info->compLength);
		free(compressed);
		
	}
	
	free(info->buffer);
	info->file->close(info->file);
	free(info);
	free(file);
}


AbstractFile* createAbstractFileFromIBootIM(AbstractFile* file) {
	InfoIBootIM* info;
	AbstractFile* toReturn;
	uint8_t *compressed;

	if(!file) {
		return NULL;
	}
	
	info = (InfoIBootIM*) malloc(sizeof(InfoIBootIM));
	info->file = file;
	file->seek(file, 0);
	file->read(file, &(info->header), sizeof(info->header));
	flipIBootIMHeader(&(info->header));
	if(strcmp(info->header.signature, IBOOTIM_SIGNATURE) != 0) {
		free(info);
		return NULL;
	}
	
	info->compLength = file->getLength(file) - sizeof(info->header);
	if(info->header.compression_type != IBOOTIM_LZSS_TYPE) {
		free(info);
		return NULL;
	}
	
	if(info->header.format == IBOOTIM_ARGB) {
		info->length = 4 * info->header.width * info->header.height;
	} else if(info->header.format == IBOOTIM_GREY) {
		info->length = 2 * info->header.width * info->header.height;
	} else {
		free(info);
		return NULL;
	}
	
	info->buffer = malloc(info->length);
	compressed = malloc(info->compLength);
	file->read(file, compressed, info->compLength);

	if(decompress_lzss(info->buffer, compressed, info->compLength) != info->length) {
		free(compressed);
		free(info);
		return NULL;
	}

	free(compressed);
	
	info->dirty = FALSE;
	
	info->offset = 0;
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readIBootIM;
	toReturn->write = writeIBootIM;
	toReturn->seek = seekIBootIM;
	toReturn->tell = tellIBootIM;
	toReturn->getLength = getLengthIBootIM;
	toReturn->close = closeIBootIM;
	
	return toReturn;
}

AbstractFile* duplicateIBootIMFile(AbstractFile* file, AbstractFile* backing) {
	InfoIBootIM* info;
	unsigned char* copyCertificate;
	AbstractFile* toReturn;

	if(!file) {
		return NULL;
	}

	info = (InfoIBootIM*) malloc(sizeof(InfoIBootIM));
	memcpy(info, file->data, sizeof(InfoIBootIM));
	
	info->file = backing;
	info->buffer = malloc(1);
	info->length = 0;
	info->dirty = TRUE;
	info->offset = 0;
	
	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile));
	toReturn->data = info;
	toReturn->read = readIBootIM;
	toReturn->write = writeIBootIM;
	toReturn->seek = seekIBootIM;
	toReturn->tell = tellIBootIM;
	toReturn->getLength = getLengthIBootIM;
	toReturn->close = closeIBootIM;
	
	return toReturn;
}

void pngRead(png_structp png_ptr, png_bytep data, png_size_t length) {
	AbstractFile* imageFile;
	imageFile = png_get_io_ptr(png_ptr);
	imageFile->read(imageFile, data, length);
}

void pngError(png_structp png_ptr, png_const_charp error_msg) {
	printf("error: %s\n", error_msg);
	exit(0);
}

void* replaceBootImage(AbstractFile* imageWrapper, AbstractFile* png, size_t *fileSize) {
	AbstractFile* imageFile;
	char header[8];
	InfoIBootIM* info;
	png_uint_32 i;
	png_bytepp row_pointers;
	
	uint8_t* imageBuffer;
	void* buffer;

	png->read(png, header, 8);
	if(png_sig_cmp(header, 0, 8) != 0) {
		printf("error: not a valid png file\n");
		return NULL;
	}
	png->seek(png, 0);

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, pngError, pngError);
	if (!png_ptr) {
		return NULL;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return NULL;
	}

	png_infop end_info = png_create_info_struct(png_ptr);
	if (!end_info)
	{
		png_destroy_read_struct(&png_ptr, &info_ptr, (png_infopp)NULL);
		return NULL;
	}

	if (setjmp(png_jmpbuf(png_ptr)))
	{
		printf("error reading png\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		free(buffer);
		return NULL;
	}

	png_set_read_fn(png_ptr, png, pngRead);

	png_read_info(png_ptr, info_ptr);
	
	if(info_ptr->bit_depth > 8) {
		printf("warning: bit depth per channel is greater than 8 (%d). Attempting to strip, but image quality will be degraded.\n", info_ptr->bit_depth);
	}
	
	if(info_ptr->color_type == PNG_COLOR_TYPE_GRAY || info_ptr->color_type == PNG_COLOR_TYPE_RGB) {
		printf("notice: attempting to add dummy transparency channel\n");
	}
	
	if(info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
		printf("notice: attempting to expand palette into full rgb\n");
	}
	
  png_set_expand(png_ptr);
	png_set_strip_16(png_ptr);
	png_set_bgr(png_ptr);
	png_set_add_alpha(png_ptr, 0xff, PNG_FILLER_AFTER);
	png_set_invert_alpha(png_ptr);
	
	png_read_update_info(png_ptr, info_ptr);
	

	if(info_ptr->width > 320 || info_ptr->height > 480) {
		printf("error: dimensions out of range, must be within 320x480, not %dx%d\n", info_ptr->width, info_ptr->height);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	if(info_ptr->bit_depth != 8) {
		printf("error: bit depth per channel must be 8 not %d!\n", info_ptr->bit_depth);
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	if(info_ptr->color_type != PNG_COLOR_TYPE_GRAY_ALPHA && info_ptr->color_type != PNG_COLOR_TYPE_RGB_ALPHA) {
		printf("error: incorrect color type, must be greyscale with alpha, or rgb with alpha\n");
		if(info_ptr->color_type == PNG_COLOR_TYPE_GRAY || info_ptr->color_type == PNG_COLOR_TYPE_RGB) {
			printf("It appears you're missing an alpha channel. Add transparency to your image\n");
		}
		if(info_ptr->color_type == PNG_COLOR_TYPE_PALETTE) {
			printf("This PNG is saved with the palette color type rather than ARGB.\n");
		}
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

		return NULL;
	}

	row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * info_ptr->height);
	imageBuffer = malloc(info_ptr->height * info_ptr->rowbytes);
	for(i = 0; i < info_ptr->height; i++) {
		row_pointers[i] = imageBuffer + (info_ptr->rowbytes * i);
	}

  png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, end_info);
	
	buffer = malloc(1);
	*fileSize = 0;
	
	imageFile = duplicateAbstractFile(imageWrapper, createAbstractFileFromMemoryFile((void**)&buffer, fileSize));
	info = (InfoIBootIM*) (imageFile->data);
	
	info->header.width = (uint16_t) info_ptr->width;
	info->header.height = (uint16_t) info_ptr->height;
	if(info_ptr->color_type == PNG_COLOR_TYPE_GRAY_ALPHA) {
		info->header.format = IBOOTIM_GREY;
	} else {
		info->header.format = IBOOTIM_ARGB;
	}
	
	imageFile->write(imageFile, imageBuffer, info_ptr->height * info_ptr->rowbytes);
	
	imageFile->close(imageFile);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	png->close(png);
	
	free(row_pointers);

	return buffer;
}

