#pragma warning(disable : 4200)

#include "common.h"

typedef struct DFUGetStatusData {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;

	uint8_t bStatus;
	uint8_t bwPollTimeout0;
	uint8_t bwPollTimeout1;
	uint8_t bwPollTimeout2;
	uint8_t bState;
	uint8_t iString;
} DFUGetStatusData;

typedef struct DFUDownloadData {
	uint8_t bmRequestType;
	uint8_t bRequest;
	uint16_t wValue;
	uint16_t wIndex;
	uint16_t wLength;

	char data[];
} DFUDownloadData;

typedef enum DFUState {
	appIDLE = 0,
	appDETACH = 1,
	dfuIDLE = 2,
	dfuDNLOAD_SYNC = 3,
	dfuDNBUSY = 4,
	dfuDNLOAD_IDLE = 5,
	dfuMANIFEST_SYNC = 6,
	dfuMANIFEST = 7,
	dfuMANIFEST_WAIT_RESET = 8,
	dfuUPLOAD_IDLE = 9,
	dfuERROR = 10
} DFUState;

