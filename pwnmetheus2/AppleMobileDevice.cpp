#include <stdio.h>

#ifdef WIN32
#include <tchar.h>
#include <windows.h>
#include <setupapi.h>
#endif

#include "AppleMobileDevice.h"
#include "RecoveryMode.h"
#include "DFUMode.h"

#ifdef __MINGW32__
#define sprintf_s sprintf
#include <stdint.h>
#else
#ifdef WIN32
#define uint32_t unsigned __int32
#define uint16_t unsigned __int16
#define uint8_t unsigned __int8
#else
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#define Sleep(x) usleep(1000 * (useconds_t)(x))
#endif
#endif

#ifdef WIN32
static const GUID GUID_DEVINTERFACE_IBOOT = {0xED82A167L, 0xD61A, 0x4AF6, {0x9A, 0xB6, 0x11, 0xE5, 0x22, 0x36, 0xC5, 0x76}};
static const GUID GUID_DEVINTERFACE_DFU = {0xB8085869L, 0xFEB9, 0x404B, {0x8C, 0xB1, 0x1E, 0x5C, 0x14, 0xFA, 0x8C, 0x54}};
#endif

static AppleMobileDevice* DeviceList = NULL;

/*
 *  * CRC32 code ripped off (and adapted) from the zlib-1.1.3 distribution by Jean-loup Gailly and Mark Adler.
 *   *
 *    * Copyright (C) 1995-1998 Mark Adler
 *     * For conditions of distribution and use, see copyright notice in zlib.h
 *      *
 *       */

/* ========================================================================
 *  * Table of CRC-32's of all single-byte values (made by make_crc_table)
 *   */
static uint32_t crc_table[256] = {
	0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
	0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
	0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
	0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
	0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
	0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
	0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
	0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
	0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
	0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
	0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
	0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
	0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
	0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
	0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
	0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
	0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
	0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
	0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
	0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
	0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
	0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
	0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
	0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
	0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
	0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
	0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
	0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
	0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
	0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
	0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
	0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
	0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
	0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
	0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
	0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
	0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
	0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
	0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
	0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
	0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
	0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
	0x68ddb3f8l, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
	0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
	0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
	0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
	0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
	0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
	0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
	0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
	0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
	0x2d02ef8dL
};

/* ========================================================================= */
#define DO1(buf) crc = crc_table[((int)crc ^ (*buf++)) & 0xff] ^ (crc >> 8);
#define DO2(buf)  DO1(buf); DO1(buf);
#define DO4(buf)  DO2(buf); DO2(buf);
#define DO8(buf)  DO4(buf); DO4(buf);

/* ========================================================================= */
uint32_t AppleCRC32Checksum(uint32_t* ckSum, const unsigned char *buf, size_t len)
{
	uint32_t crc;

	crc = *ckSum;

	if (buf == NULL) return crc;

	while (len >= 8)
	{
		DO8(buf);
		len -= 8;
	}
	if (len)
	{
		do {
			DO1(buf);
		} while (--len);
	}

	*ckSum = crc;
	return crc;
}

#ifdef WIN32
static TCHAR* GetPropertyFromPath(const TCHAR* path, const TCHAR* prop)
{
	const TCHAR* propStart = _tcsstr(path, prop);

	if(propStart == NULL)
		return NULL;

	propStart += _tcslen(prop) + 1;

	const TCHAR* propEnd = _tcschr(propStart, '_');

	if(propEnd == NULL)
	{
		propEnd = _tcschr(propStart, '#');
		if(propEnd == NULL)
			return NULL;
	}

	int length = propEnd - propStart;
	TCHAR* result = (TCHAR*) malloc((length + 1) * sizeof(TCHAR));
	memcpy(result, propStart, length * sizeof(TCHAR));
	result[length] = '\0';

	return result;
}
#endif

#ifdef __APPLE__
int MasterPortInit = FALSE;
mach_port_t MasterPort;

void AppleMobileDevice::AddDevicesForPID(SInt32 usbPID)
{
	IOReturn err;
	CFMutableDictionaryRef dict;
	io_iterator_t anIterator;
	SInt32 usbVID = 0x5AC;

	dict = IOServiceMatching("IOUSBDevice");
	if(!dict)
		return;

	CFDictionarySetValue(dict, CFSTR(kUSBVendorID), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbVID));
	CFDictionarySetValue(dict, CFSTR(kUSBProductID), CFNumberCreate(kCFAllocatorDefault, kCFNumberSInt32Type, &usbPID));

	err = IOServiceGetMatchingServices(MasterPort, dict, &anIterator);
	if(err != kIOReturnSuccess)
		return;

	io_object_t usbDevice;
	while((usbDevice = IOIteratorNext(anIterator)))
	{
		SInt32 score;
		IOUSBDeviceInterface **dev = NULL;
		IOCFPlugInInterface **plugInInterface = NULL;

		err = IOCreatePlugInInterfaceForService(usbDevice,
				kIOUSBDeviceUserClientTypeID,
				kIOCFPlugInInterfaceID,
				&plugInInterface,
				&score);

		if ((kIOReturnSuccess == err) && (plugInInterface != NULL) )
		{
			HRESULT res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBDeviceInterfaceID), (LPVOID*)&dev);
			(*plugInInterface)->Release(plugInInterface);
			if(!res && dev) {
				AppleMobileDevice* newDevice = new AppleMobileDevice(dev);

				if(DeviceList)
					newDevice->next = DeviceList;

				DeviceList = newDevice;
			}
		}

		IOObjectRelease(usbDevice);
	}

	IOObjectRelease(anIterator);
}
#endif

void AppleMobileDevice::initializeList()
{
#ifdef WIN32
	HDEVINFO usbDevices = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DFU, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if(!usbDevices)
	{
		fprintf(stderr, "AppleMobileDevice: SetupDiGetClassDevs error %d\n", GetLastError());
		return;
	}

	SP_DEVICE_INTERFACE_DATA currentInterface;
	currentInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	for(DWORD i = 0; SetupDiEnumDeviceInterfaces(usbDevices, NULL, &GUID_DEVINTERFACE_DFU, i, &currentInterface); i++)
	{
		DWORD requiredSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA details;

		SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, NULL, 0, &requiredSize, NULL);
		details = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredSize);

		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, details, requiredSize, NULL, NULL))
		{
			fprintf(stderr, "AppleMobileDevice: SetupDiGetDeviceInterfaceDetail error %d\n", GetLastError());
			free(details);
		}
		else
		{
			TCHAR* result = (TCHAR*) malloc(requiredSize - sizeof(DWORD));
			memcpy((void*) result, details->DevicePath, requiredSize - sizeof(DWORD));
			free(details);


			TCHAR* pathEnd = _tcsstr(result, TEXT("#{"));
			*pathEnd = '\0';

			AppleMobileDevice* newDevice = new AppleMobileDevice(result);

			if(DeviceList)
				newDevice->next = DeviceList;

			DeviceList = newDevice;

			free(result);
		}
	}

	SetupDiDestroyDeviceInfoList(usbDevices);
#else
#ifdef __APPLE__
	IOReturn err;

	if(!MasterPortInit)
	{
		err = IOMasterPort(MACH_PORT_NULL, &MasterPort);
		if(err != kIOReturnSuccess)
			return;

		MasterPortInit = TRUE;
	}

	AppleMobileDevice::AddDevicesForPID(0x1281);
	AppleMobileDevice::AddDevicesForPID(0x1222);
	AppleMobileDevice::AddDevicesForPID(0x1227);


#else
	struct usb_device *dev;
	struct usb_bus *bus;
	static int hasInit = FALSE;

	if(!hasInit) {
		usb_init();
		hasInit = TRUE;
	}

	usb_find_busses();
	usb_find_devices();

	for(bus = usb_get_busses(); bus; bus = bus->next) {
		for(dev = bus->devices; dev; dev = dev->next) {
			if(dev->descriptor.idVendor == 0x5AC && (dev->descriptor.idProduct == 0x1281 || dev->descriptor.idProduct == 0x1222 || dev->descriptor.idProduct == 0x1227)) {
				AppleMobileDevice* newDevice = new AppleMobileDevice(dev);

				if(DeviceList)
					newDevice->next = DeviceList;

				DeviceList = newDevice;
			}
		}
	}
#endif
#endif
}

AppleMobileDevice* AppleMobileDevice::Enumerate(AppleMobileDevice* last)
{
	if(last == NULL)
	{
		if(DeviceList != NULL)
			delete DeviceList;

		DeviceList = NULL;

		AppleMobileDevice::initializeList();
		return DeviceList;
	}

	return last->next;
}

#ifdef WIN32
AppleMobileDevice::AppleMobileDevice(const TCHAR* path)
{
	next = NULL;
	this->iBootPath = NULL;

	SP_DEVICE_INTERFACE_DATA currentInterface;
	HDEVINFO usbDevices;

	// Get iBoot path
	usbDevices = SetupDiGetClassDevs(&GUID_DEVINTERFACE_IBOOT, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if(!usbDevices)
	{
		fprintf(stderr, "AppleMobileDevice: SetupDiGetClassDevs error %d\n", GetLastError());
		return;
	}

	currentInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	for(DWORD i = 0; SetupDiEnumDeviceInterfaces(usbDevices, NULL, &GUID_DEVINTERFACE_IBOOT, i, &currentInterface); i++)
	{
		DWORD requiredSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA details;

		SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, NULL, 0, &requiredSize, NULL);
		details = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredSize);

		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, details, requiredSize, NULL, NULL))
		{
			fprintf(stderr, "AppleMobileDevice: SetupDiGetDeviceInterfaceDetail error %d\n", GetLastError());
			free(details);
		}
		else
		{
			TCHAR* result = (TCHAR*) malloc(requiredSize - sizeof(DWORD));
			memcpy((void*) result, details->DevicePath, requiredSize - sizeof(DWORD));
			free(details);

			if(_tcsstr(result, path) == NULL)
				continue;

			this->iBootPath = result;

			break;
		}
	}

	SetupDiDestroyDeviceInfoList(usbDevices);

	// Get DFU paths
	usbDevices = SetupDiGetClassDevs(&GUID_DEVINTERFACE_DFU, NULL, NULL, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);

	if(!usbDevices)
	{
		fprintf(stderr, "AppleMobileDevice: SetupDiGetClassDevs error %d\n", GetLastError());
		return;
	}

	currentInterface.cbSize = sizeof(SP_DEVICE_INTERFACE_DATA);
	for(DWORD i = 0; SetupDiEnumDeviceInterfaces(usbDevices, NULL, &GUID_DEVINTERFACE_DFU, i, &currentInterface); i++)
	{
		DWORD requiredSize = 0;
		PSP_DEVICE_INTERFACE_DETAIL_DATA details;

		SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, NULL, 0, &requiredSize, NULL);
		details = (PSP_DEVICE_INTERFACE_DETAIL_DATA) malloc(requiredSize);

		details->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA);
		if(!SetupDiGetDeviceInterfaceDetail(usbDevices, &currentInterface, details, requiredSize, NULL, NULL))
		{
			fprintf(stderr, "AppleMobileDevice: SetupDiGetDeviceInterfaceDetail error %d\n", GetLastError());
			free(details);
		}
		else
		{
			TCHAR* result = (TCHAR*) malloc(requiredSize - sizeof(DWORD));
			memcpy((void*) result, details->DevicePath, requiredSize - sizeof(DWORD));
			free(details);

			if(_tcsstr(result, path) == NULL)
				continue;

			this->DfuPath = result;

			break;
		}
	}

	SetupDiDestroyDeviceInfoList(usbDevices);


	this->DfuPipePath = (TCHAR*) malloc((_tcslen(this->DfuPath) + 10) * sizeof(TCHAR));
	wsprintf(this->DfuPipePath, TEXT("%s\\PIPE%d"), this->DfuPath, 0);

	this->hIB = NULL;
	this->hDFU = NULL;
	this->hDFUPipe = NULL;
}
#else
#ifdef __APPLE__
AppleMobileDevice::AppleMobileDevice(IOUSBDeviceInterface** dev)
{
	this->dev = dev;
	this->opened = FALSE;
	this->next = NULL;
	this->interface = NULL;
}
#else
AppleMobileDevice::AppleMobileDevice(struct usb_device* dev)
{
	this->dev = dev;
	this->handle = NULL;
	this->next = NULL;
}
#endif
#endif

BOOL AppleMobileDevice::Open(void)
{
#ifdef WIN32
	BOOL ret = TRUE;

	this->Close();

	if(this->iBootPath && !(this->hIB = CreateFile(this->iBootPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)))
	{
		fprintf(stderr, "AppleMobileDevice: CreateFile error %d\n", GetLastError());
		ret = FALSE;
	}

	if(this->DfuPath && !(this->hDFU = CreateFile(this->DfuPath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)))
	{
		fprintf(stderr, "AppleMobileDevice: CreateFile error %d\n", GetLastError());
		ret = FALSE;
	}

	if(this->DfuPipePath && !(this->hDFUPipe = CreateFile(this->DfuPipePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL)))
	{
		fprintf(stderr, "AppleMobileDevice: CreateFile error %d\n", GetLastError());
		ret = FALSE;
	}

	return ret;
#else
#ifdef __APPLE__
	IOReturn err;
	err = (*dev)->USBDeviceOpen(dev);
	if(err == kIOReturnSuccess)
	{
		int ret;

		if(this->Mode() == kRecoveryMode)
		{
			/*IOUSBFindInterfaceRequest interfaceRequest;
			io_iterator_t iterator;
			io_service_t iface;
			IOCFPlugInInterface **iodev;

			interfaceRequest.bInterfaceClass = 255;
			interfaceRequest.bInterfaceSubClass = 255;
			interfaceRequest.bInterfaceProtocol = 81;
			interfaceRequest.bAlternateSetting = kIOUSBFindInterfaceDontCare;

			ret = (*dev)->CreateInterfaceIterator(dev, &interfaceRequest, &iterator);
			if(ret)
				return 0;

			while(iface = IOIteratorNext(iterator))
			{
				SInt32 score;
				IOCFPlugInInterface **plugInInterface = NULL;

				ret = IOCreatePlugInInterfaceForService(iface,
						kIOUSBInterfaceUserClientTypeID,
						kIOCFPlugInInterfaceID,
						&plugInInterface,
						&score);

				if ((kIOReturnSuccess == ret) && (plugInInterface != NULL) )
				{
					HRESULT res = (*plugInInterface)->QueryInterface(plugInInterface, CFUUIDGetUUIDBytes(kIOUSBInterfaceInterfaceID), (LPVOID*)&interface);
					(*plugInInterface)->Release(plugInInterface);
					if(!res && interface) {
						uint8_t num;
						if((*interface)->USBInterfaceOpen(interface) == kIOReturnSuccess)
						{
							(*interface)->SetAlternateInterface(interface, 1);
							this->opened = TRUE;
						}
					}
				}

				IOObjectRelease(iface);
			}

			IOObjectRelease(iterator);*/
			this->opened = TRUE;
		}
		else
		{
			this->opened = TRUE;
		}

		if(!this->opened)
		{
			(*dev)->USBDeviceClose(dev);
		}

		return this->opened;
	} else {
		return FALSE;
	}
#else
	this->handle = usb_open(this->dev);
	if(this->handle)
		return TRUE;
	else
		return FALSE;
#endif
#endif
}

BOOL AppleMobileDevice::Close(void)
{
#ifndef WIN32
#ifdef __APPLE__
	if(this->opened) {
		this->opened = FALSE;
		//(*interface)->USBInterfaceClose(interface);
		return ((*dev)->USBDeviceClose(dev) == kIOReturnSuccess) ? TRUE : FALSE;
	}

	return TRUE;
#else
	if(this->handle) {
		if(usb_close(this->handle) == 0)
		{
			this->handle = NULL;
			return TRUE;
		}
		else
		{
			this->handle = NULL;
			return FALSE;
		}
	}

	return TRUE;
#endif
#else
	BOOL ret = TRUE;

	if(this->hIB)
	{
		if(!CloseHandle(this->hIB))
		{
			fprintf(stderr, "AppleMobileDevice: CloseHandle error %d\n", GetLastError());
			ret = FALSE;
		}

		this->hIB = NULL;
	}

	if(this->hDFU)
	{
		if(!CloseHandle(this->hDFU))
		{
			fprintf(stderr, "AppleMobileDevice: CloseHandle error %d\n", GetLastError());
			ret = FALSE;
		}

		this->hDFU = NULL;
	}


	if(this->hDFUPipe)
	{
		if(!CloseHandle(this->hDFUPipe))
		{
			fprintf(stderr, "AppleMobileDevice: CloseHandle error %d\n", GetLastError());
			ret = FALSE;
		}

		this->hDFUPipe = NULL;
	}

	return ret;
#endif
}

BOOL AppleMobileDevice::RecoverySend(const char* str, int len)
{
#ifdef WIN32
	BOOL ret;
	DWORD written = 0;

	if(!hIB)
		return FALSE;

	int toSendLen = sizeof(RecoveryModeData) + len;
	RecoveryModeData* toSend = (RecoveryModeData*) malloc(toSendLen);
	memset(toSend, 0, sizeof(RecoveryModeData));
	toSend->requestType = 0x40;
	toSend->dataLen = len;
	memcpy(toSend->data, str, len);

	ret = DeviceIoControl(this->hIB, 0x2200A0, toSend, toSendLen, toSend, toSendLen, &written, NULL);

	free(toSend);

	return ret;
#else
#ifdef __APPLE__
	char* toSend = (char*) malloc(len);
	memcpy(toSend, str, len);

	IOUSBDevRequest req;
	req.bmRequestType = 0x40;
	req.bRequest = 0x0;
	req.wValue = 0x0;
	req.wIndex = 0x0;
	req.wLength = len;
	req.pData = toSend;

	IOReturn err = (*dev)->DeviceRequest(dev, &req);
	free(toSend);

	return (err == kIOReturnSuccess) ? TRUE : FALSE;
#else
	char* toSend = (char*) malloc(len);
	memcpy(toSend, str, len);
	if(usb_control_msg(this->handle, 0x40, 0, 0, 0, toSend, len, 1000) == len) {
		free(toSend);
		return TRUE;
	} else {
		free(toSend);
		return FALSE;
	}
#endif
#endif

}

DWORD AppleMobileDevice::RecoveryReceive(char* buffer, int len)
{
#ifdef WIN32
	DWORD written = 0;

	if(!hIB)
		return FALSE;

	int toSendLen = sizeof(RecoveryModeData) + len;
	RecoveryModeData* toSend = (RecoveryModeData*) malloc(toSendLen);
	memset(toSend, 0, sizeof(RecoveryModeData));
	toSend->requestType = 0x40 | (1 << 7);
	toSend->dataLen = len;

	if(!DeviceIoControl(this->hIB, 0x2200A0, toSend, toSendLen, toSend, toSendLen, &written, NULL))
	{
		free(toSend);
		return 0;
	}

	memcpy(buffer, toSend->data, written);

	free(toSend);

	return written;
#else
	int ret;

#ifdef __APPLE__
	UInt32 readBytes = len;
	memset(buffer, 0, readBytes);

	ret =(*interface)->ReadPipeTO(interface, 1, buffer, &readBytes, 1000, 1000);

	if(ret == kIOUSBTransactionTimeout && (*interface)->GetPipeStatus(interface, 1) == kIOUSBPipeStalled)
		(*interface)->ClearPipeStallBothEnds(interface, 1);

	ret = readBytes;
#else
	ret = usb_bulk_read(this->handle, 0x01, buffer, len, 1000);
#endif

	return ret;
#endif
}

char* AppleMobileDevice::RecoveryGetEnv(const char* var)
{
#ifdef WIN32
	char buffer[1024];
	int read;

	sprintf_s(buffer, "getenv %s", var);
	this->RecoverySendCommand(buffer);

	if((read = this->RecoveryReceive(buffer, sizeof(buffer))) == 0)
	{
		fprintf(stderr, "AppleMobileDevice: RecoveryReceive error %d\n", GetLastError());
	}

	char* ret = (char*) malloc(read);
	memcpy(ret, buffer, read);

	return ret;
#else
	return NULL;
#endif
}


BOOL AppleMobileDevice::RecoverySendCommand(const char* str)
{
	return this->RecoverySend(str, strlen(str) + 1);
}

#ifndef WIN32
static int LastUSBError;
int GetLastError() {
	return LastUSBError;
}
#endif

BOOL AppleMobileDevice::DFUGetStatus(int* status, int* state)
{
	BOOL ret;
	DWORD read;
	DFUGetStatusData buffer;

	buffer.bmRequestType = 0xA1;
	buffer.bRequest = 0x03;
	buffer.wValue = 0x0;
	buffer.wIndex = 0x0;
	buffer.wLength = 0x6;

#ifdef WIN32
	ret = ReadFile(this->hDFUPipe, &buffer, sizeof(DFUGetStatusData), &read, NULL);
#else
#ifdef __APPLE__
	IOUSBDevRequest req;
	req.bmRequestType = buffer.bmRequestType;
	req.bRequest = buffer.bRequest;
	req.wValue = buffer.wValue;
	req.wIndex = buffer.wIndex;
	req.wLength = buffer.wLength;
	req.pData = &buffer.bStatus;

	ret = ((*dev)->DeviceRequest(dev, &req) == kIOReturnSuccess) ? TRUE : FALSE;
#else
	ret = ((LastUSBError = usb_control_msg(this->handle, buffer.bmRequestType, buffer.bRequest, buffer.wValue, buffer.wIndex, (char*) &buffer.bStatus, buffer.wLength, 1000)) == buffer.wLength) ? TRUE : FALSE;
#endif
#endif

	if(ret)
	{
		*status = buffer.bStatus;
		*state = buffer.bState;
	}
	else
	{
		*status = -1;
		*state = -1;
	}

	return ret;
}

BOOL AppleMobileDevice::DFUSend(const unsigned char* data, int len, AppleMobileDeviceCallback cb, void* opaque)
{
	int packetSize = 0x800;
	unsigned char hash[] = {0xff, 0xff, 0xff, 0xff, 0xac, 0x05, 0x00, 0x01, 0x55, 0x46, 0x44, 0x10, 0x0, 0x0, 0x0, 0x0};
	uint32_t crc = 0xFFFFFFFF;
	unsigned char* newData = NULL;

	DFUDownloadData* packet = (DFUDownloadData*) malloc(sizeof(DFUDownloadData) + packetSize);

	if(this->Mode() == kDFUMode) {
		unsigned char* newData = (unsigned char*) malloc(len + 16);
		AppleCRC32Checksum(&crc, data, len);
		AppleCRC32Checksum(&crc, hash, 12);

		hash[12] = crc & 0xff;
		hash[13] = (crc >> 8) & 0xff;
		hash[14] = (crc >> 16) & 0xff;
		hash[15] = (crc >> 24) & 0xff;

		memcpy(newData, data, len);
		memcpy(newData + len, hash, 16);
		data = newData;
		len = len + 16;
	}

	int totalLen = len + 1;
	int progress = 0;

	int status;
	int state;
	int packetNum = 0;
	DWORD written;

	while(len > 0)
	{
		int toSend;

		if(len > packetSize)
			toSend = packetSize;
		else
			toSend = len;

		packet->bmRequestType = 0x21;
		packet->bRequest = 0x1;
		packet->wValue = packetNum;
		packet->wIndex = 0x0;
		packet->wLength = toSend;

		memcpy(packet->data, data, toSend);

#ifdef WIN32
		written = 0;
		if(!WriteFile(this->hDFUPipe, packet, sizeof(DFUDownloadData) + toSend, &written, NULL))
		{
			fprintf(stderr, "AppleMobileDevice: WriteFile error %d\n", GetLastError());
			free(packet);
			return FALSE;
		}
#else
#ifdef __APPLE__
		IOUSBDevRequest req;
		req.bmRequestType = packet->bmRequestType;
		req.bRequest = packet->bRequest;
		req.wValue = packet->wValue;
		req.wIndex = packet->wIndex;
		req.wLength = packet->wLength;
		req.pData = packet->data;

		if((LastUSBError  = ((*dev)->DeviceRequest(dev, &req))) != kIOReturnSuccess) {
			fprintf(stderr, "AppleMobileDevice: usb_control_msg error %d\n", LastUSBError);
			free(packet);
			return FALSE;
		}
#else
		if((LastUSBError = usb_control_msg(this->handle, packet->bmRequestType, packet->bRequest, packet->wValue, packet->wIndex, packet->data, packet->wLength, 1000)) != packet->wLength) {
			fprintf(stderr, "AppleMobileDevice: usb_control_msg error %d\n", LastUSBError);
			free(packet);
			return FALSE;
		}

		written = sizeof(DFUDownloadData) + packet->wLength;
#endif
#endif
		do {
			if(!this->DFUGetStatus(&status, &state))
			{
				fprintf(stderr, "AppleMobileDevice: DFUGetStatus error %d\n", GetLastError());
				free(packet);
				return FALSE;
			}
		} while(state == dfuDNBUSY && status == 0);

		if(state != dfuDNLOAD_IDLE || status != 0)
		{
			fprintf(stderr, "AppleMobileDevice: Unexpected DFU state during send: %d, status: %d\n", state, status);
			free(packet);
			return FALSE;
		}

		progress += toSend;
		len -= toSend;
		data += toSend;

		if(cb)
			cb(opaque, progress, totalLen);

		packetNum++;
	}

	// Send ZLP

	packet->bmRequestType = 0x21;
	packet->bRequest = 0x1;
	packet->wValue = packetNum;
	packet->wIndex = 0x0;
	packet->wLength = 0x0;

#ifdef WIN32
	written = 0;
	if(!WriteFile(this->hDFUPipe, packet, sizeof(DFUDownloadData), &written, NULL))
	{
		fprintf(stderr, "AppleMobileDevice: WriteFile error %d\n", GetLastError());
		free(packet);
		return FALSE;
	}
#else
#ifdef __APPLE__
	IOUSBDevRequest req;
	req.bmRequestType = packet->bmRequestType;
	req.bRequest = packet->bRequest;
	req.wValue = packet->wValue;
	req.wIndex = packet->wIndex;
	req.wLength = packet->wLength;
	req.pData = packet->data;

	if((LastUSBError  = ((*dev)->DeviceRequest(dev, &req))) != kIOReturnSuccess) {
		fprintf(stderr, "AppleMobileDevice: usb_control_msg error %d\n", LastUSBError);
		free(packet);
		return FALSE;
	}
#else
	if((LastUSBError = usb_control_msg(this->handle, packet->bmRequestType, packet->bRequest, packet->wValue, packet->wIndex, packet->data, packet->wLength, 1000)) != packet->wLength) {
		fprintf(stderr, "AppleMobileDevice: usb_control_msg error %d\n", LastUSBError);
		free(packet);
		return FALSE;
	}

	written = sizeof(DFUDownloadData) + packet->wLength;
#endif
#endif

	free(packet);

	if(newData)
		free(newData);

	// Wait for proper state to appear

	do {
		if(!this->DFUGetStatus(&status, &state))
		{
			fprintf(stderr, "AppleMobileDevice: DFUGetStatus error %d while waiting for MANIFEST_SYNC\n", GetLastError());
			return FALSE;
		}
	} while((state == dfuDNBUSY || state == dfuMANIFEST_SYNC) && status == 0);

	if(state != dfuMANIFEST || status != 0)
	{
		fprintf(stderr, "AppleMobileDevice: Unexpected DFU state during manifest: %d, status: %d\n", state, status);
		return FALSE;
	}

	do {
		if(!this->DFUGetStatus(&status, &state))
		{
			if(this->Mode() == kDFUMode)
			{
				if(cb)
					cb(opaque, totalLen, totalLen);

				return TRUE;
			}

			fprintf(stderr, "AppleMobileDevice: DFUGetStatus error %d while waiting for not busy\n", GetLastError());
			return FALSE;
		}
	} while(state == dfuDNBUSY && status == 0);

	if(status != 0)
	{
		fprintf(stderr, "AppleMobileDevice: Unexpected DFU state during manifest: %d, status: %d\n", state, status);
		return FALSE;
	}

	if(state == dfuMANIFEST_WAIT_RESET)
	{
		// Perform device reset	
		int ret;
#ifdef WIN32
		ret = DeviceIoControl(this->hDFU, 0x22000C, NULL, 0, NULL, 0, &written, NULL);
#else
#ifdef __APPLE__
		ret = ((*dev)->ResetDevice(dev) == kIOReturnSuccess) ? TRUE : FALSE;
#else
		ret = ((usb_reset(this->handle) == 0) ? TRUE : FALSE);
#endif
#endif

		if(ret && cb)
			cb(opaque, totalLen, totalLen);

		return TRUE;
		//return ret;
	}
	else
	{
		if(cb)
			cb(opaque, totalLen, totalLen);

		return TRUE;
	}
}

BOOL AppleMobileDevice::DFUSendFile(const TCHAR* fileName, AppleMobileDeviceCallback cb, void* opaque)
{
	DWORD size = 0;
	BOOL ret;

#ifdef WIN32
	DWORD highSize = 0;

	HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING, 0, NULL);
	if(!hFile)
	{
		fprintf(stderr, "AppleMobileDevice: CreateFile error %d\n", GetLastError());
		return FALSE;
	}

	size = GetFileSize(hFile, &highSize);
	if(highSize || size > (32 * 1024 * 1024))
	{
		fprintf(stderr, "AppleMobileDevice: File too large\n");
		CloseHandle(hFile);
		return FALSE;
	}

	HANDLE hMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if(!hMapping)
	{
		fprintf(stderr, "AppleMobileDevice: CreateFileMapping error %d\n", GetLastError());
		CloseHandle(hFile);
		return FALSE;
	}

	unsigned char* data = (unsigned char*) MapViewOfFile(hMapping, FILE_MAP_READ, 0, 0, 0);
	if(!data)
	{
		fprintf(stderr, "AppleMobileDevice: MapViewOfFile error %d\n", GetLastError());
		CloseHandle(hMapping);
		CloseHandle(hFile);
		return FALSE;
	}
#else
	struct stat st;
	if(stat(fileName, &st) < 0)
		return FALSE;

	int filedes = open(fileName, O_RDONLY);
	if(filedes < 0)
		return FALSE;

	size = st.st_size;
	unsigned char* data = (unsigned char*) mmap(NULL, size, PROT_READ, MAP_PRIVATE, filedes, 0);
#endif

	ret = this->DFUSend(data, size, cb, opaque);

#ifdef WIN32
	if(!UnmapViewOfFile(data))
	{
		fprintf(stderr, "AppleMobileDevice: UnmapViewOfFile error %d\n", GetLastError());
		CloseHandle(hMapping);
		CloseHandle(hFile);
		return FALSE;
	}

	CloseHandle(hMapping);
	CloseHandle(hFile);

	return ret;
#else
	munmap(data, size);
	return ret;
#endif
}

int AppleMobileDevice::Mode()
{
#ifdef WIN32
	if(this->iBootPath == NULL)
		return kDFUMode;
	else
		return kRecoveryMode;
#else
#ifdef __APPLE__
	UInt16 idProduct = 0;
	(*dev)->GetDeviceProduct(dev, &idProduct);
	if(idProduct == 0x1222 || idProduct == 0x1227)
		return kDFUMode;
	else
		return kRecoveryMode;
#else
	if(dev->descriptor.idProduct == 0x1222 || dev->descriptor.idProduct == 0x1227)
		return kDFUMode;
	else
		return kRecoveryMode;
#endif
#endif
}

void AppleMobileDevice::WaitFor(int type)
{
	while(TRUE)
	{
		Sleep(500);
		AppleMobileDevice* device = AppleMobileDevice::Enumerate(NULL);
		while(device)
		{
			if(device->Mode() == type)
				return;

			device = AppleMobileDevice::Enumerate(device);
		}
	}

}

AppleMobileDevice::~AppleMobileDevice(void)
{
#ifdef WIN32
	free(this->iBootPath);
	free(this->DfuPath);
	free(this->DfuPipePath);
#endif

	this->Close();

#ifdef __APPLE__
	(*dev)->Release(dev);

	if(interface)
		(*interface)->Release(interface);
#endif

	if(this->next != NULL)
		delete next;
}
