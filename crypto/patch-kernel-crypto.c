#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

const char patch[] = {0x7d, 0x0e, 0x53, 0xe3, 0x02, 0x60, 0xa0, 0xe1};

const char patch2[] = {0xfa, 0x0f, 0x53, 0xe3, 0x05, 0x00, 0x00, 0x1a};

int main(int argc, char* argv[]) {
	int matchLoc = 0;
	FILE* file = fopen(argv[1], "rb+");
	fseek(file, 0, SEEK_END);
	int length = ftell(file);
	fseek(file, 0, SEEK_SET);
	uint8_t* buffer = malloc(length);
	fread(buffer, 1, length, file);

	int i;
	for(i = 0; i < length; i++) {
		uint8_t* candidate = &buffer[i];
		if(memcmp(candidate, patch, sizeof(patch)) == 0) {
			candidate[2] = 0x5f;
			fseek(file, i, SEEK_SET);
			fwrite(candidate, sizeof(patch), 1, file);
			continue;
		}
		if(memcmp(candidate, patch2, sizeof(patch2)) == 0) {
			candidate[2] = 0x5f;
			fseek(file, i, SEEK_SET);
			fwrite(candidate, sizeof(patch2), 1, file);
			continue;
		}
	}

	fclose(file);

	return 0;
}

