#include <stdlib.h>
#include <stdio.h>
#include <xpwn/libxpwn.h>
#include "abstractfile.h"
#include <xpwn/nor_files.h>
#include <xpwn/ibootim.h>
#include <string.h>

void print_usage() {
	XLOG(0, "usage:\timagetool extract <source.img2/3> <destination.png> [iv] [key]");
	XLOG(0, "usage:\timagetool inject <source.png> <destination.img2/3> <template.img2/3> [iv] [key]");
}

int main(int argc, char* argv[]) {
	init_libxpwn();
	
	if(argc < 4) {
		print_usage();
		return 0;
	}

	AbstractFile* png;
	AbstractFile* img;
	AbstractFile* dst;
	void* imageBuffer;
	size_t imageSize;

	unsigned int key[16];
	unsigned int iv[16];
	unsigned int* pKey = NULL;
	unsigned int* pIV = NULL;

	if(strcmp(argv[1], "inject") == 0) {
		if(argc < 5) {
			print_usage();
			return 0;
		}

		png = createAbstractFileFromFile(fopen(argv[2], "rb"));
		img = createAbstractFileFromFile(fopen(argv[4], "rb"));
		dst = createAbstractFileFromFile(fopen(argv[3], "wb"));

		if(argc >= 7) {
			sscanf(argv[5], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
				&iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);

			sscanf(argv[6], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
				&key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15]);

			pKey = key;
			pIV = iv;
		}

		imageBuffer = replaceBootImage(img, pKey, pIV, png, &imageSize);
		dst->write(dst, imageBuffer, imageSize);
		dst->close(dst);
	} else if(strcmp(argv[1], "extract") == 0) {
		img = createAbstractFileFromFile(fopen(argv[2], "rb"));

		if(argc >= 6) {
			sscanf(argv[4], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
				&iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);

			sscanf(argv[5], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
				&key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15]);

			pKey = key;
			pIV = iv;
		}

		if(convertToPNG(img, pKey, pIV, argv[3]) < 0) {
			XLOG(1, "error converting img to png");
		}
	}

	return 0;
}

