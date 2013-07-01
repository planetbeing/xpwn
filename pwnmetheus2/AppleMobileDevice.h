#pragma once

#ifndef APPLEMOBILEDEVICE_H
#define APPLEMOBILEDEVICE_H

#ifdef WIN32
#include <windows.h>
#else
#include "common.h"
#include <stdint.h>
typedef uint32_t DWORD;
typedef char TCHAR;
typedef int BOOL;
#ifdef __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#include <IOKit/IOCFPlugIn.h>
#include <IOKit/usb/IOUSBLib.h>
#else
#include <usb.h>
#endif
#endif

typedef void (*AppleMobileDeviceCallback)(void* opaque, int progress, int total);

enum {
	kDFUMode = 1,
	kRecoveryMode = 2
};

class AppleMobileDevice
{
private:

#ifdef WIN32
	TCHAR* iBootPath;
	TCHAR* DfuPath;
	TCHAR* DfuPipePath;

	HANDLE hIB;
	HANDLE hDFU;
	HANDLE hDFUPipe;
#else
#ifdef __APPLE__
	IOUSBDeviceInterface** dev;
	IOUSBInterfaceInterface190** interface;
	int opened;
#else
	struct usb_device* dev;
	struct usb_dev_handle* handle;
#endif
#endif

	AppleMobileDevice* next;

#ifdef __APPLE__
	static void AddDevicesForPID(SInt32 usbPID);
#endif

	static void initializeList();

public:

	static AppleMobileDevice* Enumerate(AppleMobileDevice* last);
	static void WaitFor(int type);

#ifdef WIN32
	AppleMobileDevice(const TCHAR* path);
#else
#ifdef __APPLE__
	AppleMobileDevice(IOUSBDeviceInterface** dev);
#else
	AppleMobileDevice(struct usb_device* dev);
#endif
#endif

	virtual BOOL Open(void);
	virtual BOOL Close(void);

	virtual BOOL RecoverySend(const char* str, int len);
	virtual DWORD RecoveryReceive(char* buffer, int len);

	virtual BOOL RecoverySendCommand(const char* str);

	virtual char* RecoveryGetEnv(const char* var);

	virtual BOOL DFUGetStatus(int* status, int* state);

	virtual BOOL DFUSend(const unsigned char* data, int len, AppleMobileDeviceCallback cb = NULL, void* opaque = NULL);

	virtual BOOL DFUSendFile(const TCHAR* fileName, AppleMobileDeviceCallback cb = NULL, void* opaque = NULL);

	virtual int Mode();

	virtual ~AppleMobileDevice(void);
};

#endif
