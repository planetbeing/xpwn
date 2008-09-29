// Shamelesssly ripped off from wizdaz

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <IOKit/IOKitLib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <libgen.h>

typedef struct 
{
	void*		inbuf;
	void*		outbuf;
	uint32_t	size;
	uint8_t		iv[16];
	uint32_t	mode;
	uint32_t	bits;
	uint8_t		keybuf[32];
	uint32_t	mask;
} IOAESStruct;

#define kIOAESAcceleratorInfo 0
#define kIOAESAcceleratorTask 1
#define kIOAESAcceleratorTest 2

#define kIOAESAcceleratorEncrypt 0
#define kIOAESAcceleratorDecrypt 1

#define kIOAESAcceleratorGIDMask 0x3E8
#define kIOAESAcceleratorUIDMask 0x7D0
#define kIOAESAcceleratorCustomMask 0

typedef enum {
	UID,
	GID,
	Custom
} IOAESKeyType;

IOReturn doAES(io_connect_t conn, void* inbuf, void *outbuf, uint32_t size, IOAESKeyType keyType, void* key, void* iv, int mode) {
	IOAESStruct in;

	in.mode = mode;
	in.bits = 128;
	in.inbuf = inbuf;
	in.outbuf = outbuf;
	in.size = size;

	switch(keyType) {
		case UID:
			in.mask = kIOAESAcceleratorUIDMask;
			break;
		case GID:
			in.mask = kIOAESAcceleratorGIDMask;
			break;
		case Custom:
			in.mask = kIOAESAcceleratorCustomMask;
			break;
	}
	memset(in.keybuf, 0, sizeof(in.keybuf));

	if(key)
		memcpy(in.keybuf, key, in.bits / 8);

	if(iv)
		memcpy(in.iv, iv, 16);
	else
		memset(in.iv, 0, 16);

	IOByteCount inSize = sizeof(in);

	return IOConnectCallStructMethod(conn, kIOAESAcceleratorTask, &in, inSize, &in, &inSize);
}

void hexToBytes(const char* hex, uint8_t** buffer, size_t* bytes) {
	*bytes = strlen(hex) / 2;
	*buffer = (uint8_t*) malloc(*bytes);
	size_t i;
	for(i = 0; i < *bytes; i++) {
		uint32_t byte;
		sscanf(hex, "%02x", &byte);
		(*buffer)[i] = byte;
		hex += 2;
	}
}

void bytesToHex(const uint8_t* buffer, size_t bytes) {
	size_t i;
	while(bytes > 0) {
		printf("%02x", *buffer);
		buffer++;
		bytes--;
	}
}

int main(int argc, char* argv[]) {
	IOReturn ret;

	size_t keyLength;
	size_t ivLength;
	size_t dataLength;

	uint8_t* key = NULL;
	uint8_t* iv = NULL;
	uint8_t* data = NULL;

	int direction;
	IOAESKeyType keyType;

	int stdinFile = 0;
	int stdoutFile = 0;

	struct stat std_stat;
	fstat(fileno(stdin), &std_stat);
	if((std_stat.st_mode & S_IFREG) != 0)
		stdinFile = 1;

	fstat(fileno(stdout), &std_stat);
	if((std_stat.st_mode & S_IFREG) != 0)
		stdoutFile = 1;

	if(strcmp(basename(argv[0]), "aescmd") == 0) {
		stdinFile = 0;
		stdoutFile = 0;
	}

	if(argc < 3) {
		fprintf(stderr, "usage: %s <enc/dec> <GID/UID/key> [data] [iv]\n", argv[0]);
		return 0;
	}

	if(strncasecmp(argv[1], "enc", 3) == 0) {
		direction = kIOAESAcceleratorEncrypt;
	} else if(strncasecmp(argv[1], "dec", 3) == 0) {
		direction = kIOAESAcceleratorDecrypt;
	} else {
		fprintf(stderr, "error: method must be 'enc' or 'dec'\n");
		return 1;
	}

	if(strcasecmp(argv[2], "GID") == 0) {
		keyType = GID;
	} else if(strcasecmp(argv[2], "UID") == 0) {
		keyType = UID;
	} else {
		keyType = Custom;
		hexToBytes(argv[2], &key, &keyLength);
	}

	if(stdinFile) {
		if(argc >= 4)
			hexToBytes(argv[3], &iv, &ivLength);
	} else {
		hexToBytes(argv[3], &data, &dataLength);

		if(argc >= 5)
			hexToBytes(argv[4], &iv, &ivLength);
	}


	CFMutableDictionaryRef dict = IOServiceMatching("IOAESAccelerator");
	io_service_t dev = IOServiceGetMatchingService(kIOMasterPortDefault, dict);
	io_connect_t conn = 0;

	if(!dev) {
		fprintf(stderr, "error: IOAESAccelerator device not found!\n");
		goto quit;
	}

	ret = IOServiceOpen(dev, mach_task_self(), 0, &conn);

	if(ret != kIOReturnSuccess) {
		fprintf(stderr, "error: Cannot open service\n");
		goto quit;
	}

	if(stdinFile) {
		uint8_t aIV[16];
		if(!iv) {
			memset(aIV, 0, 16);
			iv = aIV;
		}
		data = (uint8_t*) malloc(16);
		while(!feof(stdin)) {
			dataLength = 16;
			dataLength = fread(data, 1, dataLength, stdin);
			
			if(dataLength == 0)
				break;

			if((ret = doAES(conn, data, data, dataLength, keyType, key, iv, direction)) != kIOReturnSuccess) {
				fprintf(stderr, "IOAESAccelerator returned: %x\n", ret);
				goto quit;
			}
			if(stdoutFile) {
				fwrite(data, 1, dataLength, stdout);
			} else {
				bytesToHex(data, dataLength);
				printf("\n");
			}
		}
	} else {
		if((ret = doAES(conn, data, data, dataLength, keyType, key, iv, direction)) != kIOReturnSuccess) {
			fprintf(stderr, "IOAESAccelerator returned: %x\n", ret);
			goto quit;
		}

		if(stdoutFile) {
			fwrite(data, 1, dataLength, stdout);
		} else {
			bytesToHex(data, dataLength);
			printf("\n");
		}
	}

quit:
	if(data)
		free(data);

	if(conn)
		IOServiceClose(conn);

	if(dev)
		IOObjectRelease(dev);

	return 0;
}

