#ifndef _SAM7DFU_H
#define _SAM7DFU_H
#include "abstractfile.h"

int sam7dfu_do_dnload(struct usb_dev_handle *usb_handle, int interface,
		      int xfer_size, AbstractFile* file);

#endif
