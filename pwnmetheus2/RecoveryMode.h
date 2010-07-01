#pragma warning(disable : 4200)

#include "common.h"

typedef struct RecoveryModeData {
	uint8_t requestType;
	uint8_t unk1;
	uint16_t unk2;
	uint16_t unk3;
	uint16_t dataLen;

	char data[];
} RecoveryModeData;

