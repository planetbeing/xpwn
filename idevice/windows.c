#define _WIN32_WINNT 0x0502

#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "idevice.h"
#include "mobiledevice.h"
#include <xpwn/libxpwn.h>

#define SHGFP_TYPE_CURRENT 0

typedef mach_error_t (*AMDeviceNotificationSubscribeFunctionType)(am_device_notification_callback
    callback, unsigned int unused0, unsigned int unused1, unsigned int
    dn_unknown3, struct am_device_notification **notification);
    
typedef mach_error_t (*AMRestoreRegisterForDeviceNotificationsFunctionType)(
    am_restore_device_notification_callback dfu_connect_callback,
    am_restore_device_notification_callback recovery_connect_callback,
    am_restore_device_notification_callback dfu_disconnect_callback,
    am_restore_device_notification_callback recovery_disconnect_callback,
    unsigned int unknown0,
    void *user_info);

typedef CFMutableDictionaryRef (*AMRestoreCreateDefaultOptionsFunctionType)(CFAllocatorRef allocator);

typedef mach_error_t (*AMRestoreEnableFileLoggingFunctionType)(char *path);

typedef mach_error_t (*AMRestorePerformDFURestoreFunctionType)(struct am_recovery_device *rdev, CFDictionaryRef opts, void *callback, void *user_info);

typedef int (__cdecl * cmdsend)  (am_recovery_device *, am_recovery_device *, CFStringRef, int block) __attribute__ ((cdecl));

static AMDeviceNotificationSubscribeFunctionType AMDeviceNotificationSubscribeFunction = NULL;
static AMRestoreRegisterForDeviceNotificationsFunctionType AMRestoreRegisterForDeviceNotificationsFunction = NULL;
static AMRestoreCreateDefaultOptionsFunctionType AMRestoreCreateDefaultOptionsFunction = NULL;
static AMRestoreEnableFileLoggingFunctionType AMRestoreEnableFileLoggingFunction = NULL;
static AMRestorePerformDFURestoreFunctionType AMRestorePerformDFURestoreFunction = NULL;
static cmdsend   priv_sendCommandToDevice = NULL;
static cmdsend   priv_sendFileToDevice = NULL;

typedef struct tagDLLOFFSET {
	DWORD VerMS;
	DWORD VerLS;
	DWORD GetProductType;
	DWORD SendCmdO;
	DWORD SendFileO;
} DLLOFFSET;

DLLOFFSET OffSetTable[]= {
//	VerMS		VerLS		GetProductType,	SendCmdO	SendFileO
	{0x0070008,	0x00760000,	0x0000FDA0,	0x00010290,	0x00010630},
	{0x0070008,	0x00B00000,	0x0000FDF0,	0x000102E0,	0x00010680},
	{0x0000000,	0x00000000,	0x00000000,	0x00000000,	0x00000000}
        };

void SetDeviceDLLPath(char* path) {
	if(SHGetFolderPath(NULL, CSIDL_PROGRAM_FILES_COMMON, NULL, SHGFP_TYPE_CURRENT, path) < 0)
	{
		XLOG(2, "failed to get Common path");
		return;
	}
	
	strcat(path, "\\Apple\\Mobile Device Support\\bin");
	
	struct stat st;
	if(stat(path, &st) < 0) {
		XLOG(2, "failed to find driver path");
		return;
	}
	
	SetDllDirectory(path);
	
	strcpy(path, "iTunesMobileDevice.dll");
}

int initWindowsPrivateFunctions(HANDLE deviceDLL) {
	char path[MAX_PATH];
	
	SetDeviceDLLPath(path);

	DWORD why;
	BYTE *pBuffer;
	char Major[1024];
	char Minor[1024];
	unsigned int sLen;
	VS_FIXEDFILEINFO *DLLFileInfo = NULL;
	DWORD dwLen = GetFileVersionInfoSize(path, &why);
	
	XLOG(3, "Enter dll version lookup loop");
	
	if (dwLen) {
		pBuffer = (BYTE*)malloc(dwLen);
		if (pBuffer) {
			if (GetFileVersionInfo(path, 0, dwLen,(void *) pBuffer)) {
				VerQueryValue(pBuffer,"\\",(LPVOID*) &DLLFileInfo,&sLen);
			}
		}
	}
	
	if (DLLFileInfo) {
		_itoa(DLLFileInfo->dwFileVersionMS,Major,16);
		_itoa(DLLFileInfo->dwFileVersionLS,Minor,16);
		XLOG(3, "iTunes Version Major %s Minor %s", Major, Minor);
		int i;
		for (i = 0; OffSetTable[i].VerMS; i++)
		{
			if (OffSetTable[i].VerMS == DLLFileInfo->dwFileVersionMS)
			{
				if ((OffSetTable[i].VerLS == 0x00000000) || (OffSetTable[i].VerLS == DLLFileInfo->dwFileVersionLS))
				{
					char* fromOffset =
						((char*)GetProcAddress(deviceDLL, "AMRecoveryModeDeviceGetProductType")) - OffSetTable[i].GetProductType;
					priv_sendCommandToDevice = (cmdsend)   (fromOffset + OffSetTable[i].SendCmdO);
					priv_sendFileToDevice = (cmdsend) (fromOffset + OffSetTable[i].SendFileO );
				}
			}
		}
	}

	if(pBuffer) free(pBuffer);
	
	if(!priv_sendCommandToDevice) {
		XLOG(1, "Unsupported iTunesMobileDevice.dll Version Major %s Minor %s", Major, Minor);
	}
	
	return 0;
}

HANDLE GetDeviceDLL()
{
	char path[MAX_PATH];
	
	SetDeviceDLLPath(path);
	
	XLOG(3, "looking for driver at %s", path);
	
	return LoadLibrary(path);
}

int LoadWindowsDLL() {
	HANDLE deviceDLL = GetDeviceDLL();
	
	if(!deviceDLL)
	{
		XLOG(2, "failed to load Windows MobileDevice DLL");
		return -1;
	}
		
	AMDeviceNotificationSubscribeFunction =
		(AMDeviceNotificationSubscribeFunctionType) GetProcAddress(deviceDLL, "AMDeviceNotificationSubscribe");
	
	if(!AMDeviceNotificationSubscribeFunction)
	{
		XLOG(2, "failed to load AMDeviceNotificationSubscribe");
		return -1;
	}
		
	AMRestoreRegisterForDeviceNotificationsFunction =
		(AMRestoreRegisterForDeviceNotificationsFunctionType) GetProcAddress(deviceDLL, "AMRestoreRegisterForDeviceNotifications");
		
	if(!AMRestoreRegisterForDeviceNotificationsFunction)
	{
		XLOG(2, "failed to load AMRestoreRegisterForDeviceNotifications");
		return -1;
	}
	
	AMRestoreCreateDefaultOptionsFunction =
		(AMRestoreCreateDefaultOptionsFunctionType) GetProcAddress(deviceDLL, "AMRestoreCreateDefaultOptions");
		
	if(!AMRestoreCreateDefaultOptionsFunction)
	{
		XLOG(2, "failed to load AMRestoreCreateDefaultOptions");
		return -1;
	}
	
	AMRestorePerformDFURestoreFunction =
		(AMRestorePerformDFURestoreFunctionType) GetProcAddress(deviceDLL, "AMRestorePerformDFURestore");
		
	if(!AMRestorePerformDFURestoreFunction	)
	{
		XLOG(2, "failed to load AMRestorePerformDFURestore");
		return -1;
	}
	
	AMRestoreEnableFileLoggingFunction =
		(AMRestoreEnableFileLoggingFunctionType) GetProcAddress(deviceDLL, "AMRestoreEnableFileLogging");
		
	if(!AMRestoreEnableFileLoggingFunction	)
	{
		XLOG(2, "failed to load AMRestoreEnableFileLogging");
		return -1;
	}
	
	
	initWindowsPrivateFunctions(deviceDLL);
			
	return 0;
}

mach_error_t AMDeviceNotificationSubscribe(am_device_notification_callback
    callback, unsigned int unused0, unsigned int unused1, unsigned int
    dn_unknown3, struct am_device_notification **notification)
{
	XLOG(3, "calling Windows AMDeviceNotificationSubscribe");
    return AMDeviceNotificationSubscribeFunction(callback, unused0, unused1, dn_unknown3, notification);
    
}

mach_error_t AMRestoreRegisterForDeviceNotifications(
    am_restore_device_notification_callback dfu_connect_callback,
    am_restore_device_notification_callback recovery_connect_callback,
    am_restore_device_notification_callback dfu_disconnect_callback,
    am_restore_device_notification_callback recovery_disconnect_callback,
    unsigned int unknown0,
    void *user_info)   
{
	XLOG(3, "calling Windows AMRestoreRegisterForDeviceNotifications");
    return AMRestoreRegisterForDeviceNotificationsFunction(dfu_connect_callback, recovery_connect_callback, dfu_disconnect_callback,
		recovery_disconnect_callback, unknown0, user_info);
}

CFMutableDictionaryRef AMRestoreCreateDefaultOptions(CFAllocatorRef allocator)
{
	XLOG(3, "calling Windows AMRestoreCreateDefaultOptions");
	return AMRestoreCreateDefaultOptionsFunction(allocator);
}

mach_error_t AMRestoreEnableFileLogging(char *path) {
	XLOG(3, "calling Windows AMRestoreEnableFileLogging");
	return AMRestoreEnableFileLoggingFunction(path);
}

mach_error_t AMRestorePerformDFURestore(struct am_recovery_device *rdev, CFDictionaryRef opts, void *callback, void *user_info)
{
	XLOG(3, "calling Windows AMRestorePerformDFURestore");
	return AMRestorePerformDFURestoreFunction(rdev, opts, callback, user_info);
}

int sendCommandToDevice(am_recovery_device *rdev, CFStringRef cfs, int block)
{
	XLOG(3, "calling Windows sendCommandToDevice %p %p %p %d", priv_sendCommandToDevice, rdev, cfs, block);
	int retval;
	asm(
		"movl %4, %%ecx\n"
		"movl %2, %%edi\n"
		"push %1\n"
		"call *%0\n"
		"movl %%eax, %3\n"
		"addl $4, %%esp\n"
		:
		: "m"(priv_sendCommandToDevice), "m"(block), "m"(cfs), "m"(retval), "m"(rdev)
		: "edi", "eax", "ecx", "edx"
		);
	return retval;
}

int sendFileToDevice(am_recovery_device *rdev, CFStringRef filename)
{
	XLOG(3, "calling Windows sendFileToDevice: %p", priv_sendFileToDevice);
	int retval;
	asm(
		"movl %3, %%ecx\n"
		"push %1\n"
		"call *%0\n"
		"movl %%eax, %2\n"
		"addl $4, %%esp\n"
		:
		:"m"(priv_sendFileToDevice), "m"(filename), "m"(retval), "m"(rdev)
	);
	return retval;	
}
