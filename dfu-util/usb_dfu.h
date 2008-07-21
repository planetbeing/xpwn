#ifndef _USB_DFU_H
#define _USB_DFU_H
/* USB Device Firmware Update Implementation for OpenPCD
 * (C) 2006 by Harald Welte <hwelte@hmw-consulting.de>
 *
 * Protocol definitions for USB DFU
 *
 * This ought to be compliant to the USB DFU Spec 1.0 as available from
 * http://www.usb.org/developers/devclass_docs/usbdfu10.pdf
 *
 */

#include <sys/types.h>

#include <stdint.h>

#define USB_DT_DFU			0x21

struct usb_dfu_func_descriptor {
	uint8_t		bLength;
	uint8_t		bDescriptorType;
	uint8_t		bmAttributes;
#define USB_DFU_CAN_DOWNLOAD	(1 << 0)
#define USB_DFU_CAN_UPLOAD	(1 << 1)
#define USB_DFU_MANIFEST_TOL	(1 << 2)
#define USB_DFU_WILL_DETACH	(1 << 3)
	uint16_t		wDetachTimeOut;
	uint16_t		wTransferSize;
	uint16_t		bcdDFUVersion;
} __attribute__ ((packed));

#define USB_DT_DFU_SIZE			9

#define USB_TYPE_DFU		(USB_TYPE_CLASS|USB_RECIP_INTERFACE)

/* DFU class-specific requests (Section 3, DFU Rev 1.1) */
#define USB_REQ_DFU_DETACH	0x00
#define USB_REQ_DFU_DNLOAD	0x01
#define USB_REQ_DFU_UPLOAD	0x02
#define USB_REQ_DFU_GETSTATUS	0x03
#define USB_REQ_DFU_CLRSTATUS	0x04
#define USB_REQ_DFU_GETSTATE	0x05
#define USB_REQ_DFU_ABORT	0x06

#define DFU_STATUS_OK			0x00
#define DFU_STATUS_errTARGET		0x01
#define DFU_STATUS_errFILE		0x02
#define DFU_STATUS_errWRITE		0x03
#define DFU_STATUS_errERASE		0x04
#define DFU_STATUS_errCHECK_ERASED	0x05
#define DFU_STATUS_errPROG		0x06
#define DFU_STATUS_errVERIFY		0x07
#define DFU_STATUS_errADDRESS		0x08
#define DFU_STATUS_errNOTDONE		0x09
#define DFU_STATUS_errFIRMWARE		0x0a
#define DFU_STATUS_errVENDOR		0x0b
#define DFU_STATUS_errUSBR		0x0c
#define DFU_STATUS_errPOR		0x0d
#define DFU_STATUS_errUNKNOWN		0x0e
#define DFU_STATUS_errSTALLEDPKT	0x0f

enum dfu_state {
	DFU_STATE_appIDLE		= 0,
	DFU_STATE_appDETACH		= 1,
	DFU_STATE_dfuIDLE		= 2,
	DFU_STATE_dfuDNLOAD_SYNC	= 3,
	DFU_STATE_dfuDNBUSY		= 4,
	DFU_STATE_dfuDNLOAD_IDLE	= 5,
	DFU_STATE_dfuMANIFEST_SYNC	= 6,
	DFU_STATE_dfuMANIFEST		= 7,
	DFU_STATE_dfuMANIFEST_WAIT_RST	= 8,
	DFU_STATE_dfuUPLOAD_IDLE	= 9,
	DFU_STATE_dfuERROR		= 10,
};

/* this is the 'swapped' definition, i.e. after/before mirroring */
struct dfu_file_suffix {
	uint32_t dwCRC;	/* CRC32 ANSI X3.66 */
	uint8_t bLength;	/* 16 bytes */
	char ucDfuSignature[3];	/* "DFU" */
	uint16_t bcdDFU;	/* Version */
	uint16_t idVendor;	/* VendorID */
	uint16_t idProduct;	/* ProductID */
	uint16_t bcdDevice;	/* Device Revision, or 0xffff */
} __attribute__ ((packed));

#endif /* _USB_DFU_H */
