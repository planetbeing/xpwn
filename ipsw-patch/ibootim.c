#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "abstractfile.h"
#include <xpwn/libxpwn.h>
#include <xpwn/ibootim.h>
#include <xpwn/lzss.h>
#include <png.h>
#include <xpwn/nor_files.h>

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
		XLOG(1, "createAbstractFileFromIBootIM: signature does not match\n");
		return NULL;
	}
	
	info->compLength = file->getLength(file) - sizeof(info->header);
	if(info->header.compression_type != IBOOTIM_LZSS_TYPE) {
		//free(info);
		XLOG(1, "createAbstractFileFromIBootIM: (warning) unsupported compression type: %x\n", info->header.compression_type);
		//return NULL;
	}

	int depth = 0;	
	if(info->header.format == IBOOTIM_ARGB) {
		info->length = 4 * info->header.width * info->header.height;
		depth = 4;
	} else if(info->header.format == IBOOTIM_GREY) {
		info->length = 2 * info->header.width * info->header.height;
		depth = 2;
	} else {
		XLOG(1, "createAbstractFileFromIBootIM: unsupported color type: %x\n", info->header.format);
		free(info);
		return NULL;
	}

	info->buffer = malloc(info->length);
	compressed = malloc(info->compLength);
	file->read(file, compressed, info->compLength);

	int length = decompress_lzss(info->buffer, compressed, info->compLength);
	if(length > info->length) {
		XLOG(1, "createAbstractFileFromIBootIM: decompression error\n");
		free(compressed);
		free(info);
		return NULL;
	} else if(length < info->length) {
		XLOG(1, "createAbstractFileFromIBootIM: (warning) uncompressed data shorter than expected: %d\n", length);
		info->length = length;
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
	toReturn->type = AbstractFileTypeIBootIM;
	
	return toReturn;
}

AbstractFile* duplicateIBootIMFile(AbstractFile* file, AbstractFile* backing) {
	InfoIBootIM* info;
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
	toReturn->type = AbstractFileTypeIBootIM;
	
	return toReturn;
}

void pngRead(png_structp png_ptr, png_bytep data, png_size_t length) {
	AbstractFile* imageFile;
	imageFile = png_get_io_ptr(png_ptr);
	imageFile->read(imageFile, data, length);
}

void pngError(png_structp png_ptr, png_const_charp error_msg) {
	XLOG(0, "error: %s\n", error_msg);
	exit(0);
}

void pngWarn(png_structp png_ptr, png_const_charp error_msg) {
	XLOG(0, "warning: %s\n", error_msg);
}

int convertToPNG(AbstractFile* imageWrapper, const unsigned int* key, const unsigned int* iv, const char* png) {
	AbstractFile* imageFile;

	FILE *fp = fopen(png, "wb");
	if(!fp)
		return -1;

	if(key != NULL) {
		imageFile = openAbstractFile2(imageWrapper, key, iv);
	} else {	
		imageFile = openAbstractFile(imageWrapper);
	}
	InfoIBootIM* info = (InfoIBootIM*) (imageFile->data);

	png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, pngError, pngWarn);
	if (!png_ptr) {
		return -1;
	}

	png_infop info_ptr = png_create_info_struct(png_ptr);
	if (!info_ptr)
	{
		png_destroy_read_struct(&png_ptr, (png_infopp)NULL, (png_infopp)NULL);
		return -1;
	}

	png_init_io(png_ptr, fp);

	int color_type;
	int bytes_per_pixel;

	if(info->header.format == IBOOTIM_ARGB) {
		XLOG(3, "ARGB");
		color_type = PNG_COLOR_TYPE_RGB_ALPHA;
		bytes_per_pixel = 4;
	} else if(info->header.format == IBOOTIM_GREY) {
		XLOG(3, "Grayscale");
		color_type = PNG_COLOR_TYPE_GRAY_ALPHA;
		bytes_per_pixel = 2;
	} else {
		XLOG(3, "Unknown color type!");
	}

	png_set_IHDR(png_ptr, info_ptr, info->header.width, info->header.height,
		     8, color_type, PNG_INTERLACE_NONE,
		     PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

	png_set_bgr(png_ptr);
	png_set_invert_alpha(png_ptr);

	png_write_info(png_ptr, info_ptr);


	void* imageBuffer = malloc(imageFile->getLength(imageFile));
	imageFile->read(imageFile, imageBuffer, imageFile->getLength(imageFile));

	png_bytepp row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * info->header.height);
	int i;
	for(i = 0; i < png_get_image_height(png_ptr, info_ptr); i++) {
		row_pointers[i] = imageBuffer + (info->header.width * bytes_per_pixel * i);
	}

	png_write_image(png_ptr, row_pointers);

	png_write_end(png_ptr, NULL);

	free(imageBuffer);

	return 0;
}

void* replaceBootImage(AbstractFile* imageWrapper, const unsigned int* key, const unsigned int* iv, AbstractFile* png, size_t *fileSize) {
	AbstractFile* imageFile;
	unsigned char header[8];
	InfoIBootIM* info;
	png_uint_32 i;
	png_bytepp row_pointers;
	
	uint8_t* imageBuffer;
	void* buffer;

	png->read(png, header, 8);
	if(png_sig_cmp(header, 0, 8) != 0) {
		XLOG(0, "error: not a valid png file\n");
		return NULL;
	}
	png->seek(png, 0);

	png_structp png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, pngError, pngWarn);
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
		XLOG(0, "error reading png\n");
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		free(buffer);
		return NULL;
	}

	png_set_read_fn(png_ptr, png, pngRead);

	png_read_info(png_ptr, info_ptr);
	
	if(png_get_bit_depth(png_ptr, info_ptr) > 8) {
		XLOG(0, "warning: bit depth per channel is greater than 8 (%d). Attempting to strip, but image quality will be degraded.\n", png_get_bit_depth(png_ptr, info_ptr));
	}
	
	if(png_get_color_type(png_ptr,info_ptr) == PNG_COLOR_TYPE_GRAY || png_get_color_type(png_ptr,info_ptr) == PNG_COLOR_TYPE_RGB) {
		XLOG(0, "notice: attempting to add dummy transparency channel\n");
	}
	
	if(png_get_color_type(png_ptr,info_ptr) == PNG_COLOR_TYPE_PALETTE) {
		XLOG(0, "notice: attempting to expand palette into full rgb\n");
	}
	
	png_set_expand(png_ptr);
	png_set_strip_16(png_ptr);
	png_set_bgr(png_ptr);
	png_set_add_alpha(png_ptr, 0x0, PNG_FILLER_AFTER);
	png_set_invert_alpha(png_ptr);
	
	png_read_update_info(png_ptr, info_ptr);
	

	if(png_get_image_width(png_ptr, info_ptr) > 320 || png_get_image_height(png_ptr, info_ptr) > 480) {
		XLOG(0, "error: dimensions out of range, must be within 320x480, not %lux%lu\n", png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr));
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	if(png_get_bit_depth(png_ptr, info_ptr) != 8) {
		XLOG(0, "error: bit depth per channel must be 8 not %d!\n", png_get_bit_depth(png_ptr, info_ptr));
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);
		return NULL;
	}

	if(png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_GRAY_ALPHA && png_get_color_type(png_ptr, info_ptr) != PNG_COLOR_TYPE_RGB_ALPHA) {
		XLOG(0, "error: incorrect color type, must be greyscale with alpha, or rgb with alpha\n");
		if(png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY || png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_RGB) {
			XLOG(0, "It appears you're missing an alpha channel. Add transparency to your image\n");
		}
		if(png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_PALETTE) {
			XLOG(0, "This PNG is saved with the palette color type rather than ARGB.\n");
		}
		png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

		return NULL;
	}

	row_pointers = (png_bytepp) malloc(sizeof(png_bytep) * png_get_image_height(png_ptr, info_ptr));
	imageBuffer = malloc(png_get_image_height(png_ptr, info_ptr) * png_get_rowbytes(png_ptr, info_ptr));
	for(i = 0; i < png_get_image_height(png_ptr, info_ptr); i++) {
		row_pointers[i] = imageBuffer + (png_get_rowbytes(png_ptr, info_ptr) * i);
	}

	png_read_image(png_ptr, row_pointers);
	png_read_end(png_ptr, end_info);
	
	buffer = malloc(1);
	*fileSize = 0;

	if(key != NULL) {
		imageFile = duplicateAbstractFile2(imageWrapper, createAbstractFileFromMemoryFile((void**)&buffer, fileSize), key, iv, NULL);
	} else {	
		imageFile = duplicateAbstractFile(imageWrapper, createAbstractFileFromMemoryFile((void**)&buffer, fileSize));
	}
	info = (InfoIBootIM*) (imageFile->data);
	
	info->header.width = (uint16_t) png_get_image_width(png_ptr, info_ptr);
	info->header.height = (uint16_t) png_get_image_height(png_ptr, info_ptr);
	if(png_get_color_type(png_ptr, info_ptr) == PNG_COLOR_TYPE_GRAY_ALPHA) {
		info->header.format = IBOOTIM_GREY;
	} else {
		info->header.format = IBOOTIM_ARGB;
	}
	
	imageFile->write(imageFile, imageBuffer, png_get_image_height(png_ptr, info_ptr) * png_get_rowbytes(png_ptr, info_ptr));
	
	imageFile->close(imageFile);

	png_destroy_read_struct(&png_ptr, &info_ptr, &end_info);

	png->close(png);
	
	free(row_pointers);

	free(imageBuffer);

	return buffer;
}