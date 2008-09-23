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

typedef mach_error_t (*AMRestorePerformRecoveryModeRestoreFunctionType)(struct am_recovery_device *rdev, CFDictionaryRef opts, void *callback, void *user_info);

typedef int (__cdecl * cmdsend)  (am_recovery_device *, am_recovery_device *, CFStringRef, int block) __attribute__ ((cdecl));

typedef unsigned int* (*AMRecoveryModeDeviceGetProgressFunctionType)(struct am_recovery_device *rdev, unsigned int* progress, unsigned int* total);

typedef mach_error_t (*AMRecoveryModeDeviceSetAutoBootFunctionType)(struct am_recovery_device *rdev, char autoboot);

typedef mach_error_t (*AMDeviceEnterRecoveryFunctionType)(struct am_device *rdev);

typedef mach_error_t (*AMDeviceConnectFunctionType)(struct am_device *device);
typedef mach_error_t (*AMDeviceIsPairedFunctionType)(struct am_device *device);
typedef mach_error_t (*AMDevicePairFunctionType)(struct am_device *device);
typedef mach_error_t (*AMDeviceValidatePairingFunctionType)(struct am_device *device);
typedef mach_error_t (*AMDeviceStartSessionFunctionType)(struct am_device *device);

typedef CFStringRef (*AMDeviceCopyValueFunctionType)(struct am_device *device, unsigned int, const CFStringRef cfstring);

static AMDeviceNotificationSubscribeFunctionType AMDeviceNotificationSubscribeFunction = NULL;
static AMRestoreRegisterForDeviceNotificationsFunctionType AMRestoreRegisterForDeviceNotificationsFunction = NULL;
static AMRestoreCreateDefaultOptionsFunctionType AMRestoreCreateDefaultOptionsFunction = NULL;
static AMRestoreEnableFileLoggingFunctionType AMRestoreEnableFileLoggingFunction = NULL;
static AMRestorePerformDFURestoreFunctionType AMRestorePerformDFURestoreFunction = NULL;
static AMRestorePerformRecoveryModeRestoreFunctionType AMRestorePerformRecoveryModeRestoreFunction = NULL;
static AMRecoveryModeDeviceGetProgressFunctionType AMRecoveryModeDeviceGetProgressFunction = NULL;
static AMRecoveryModeDeviceSetAutoBootFunctionType AMRecoveryModeDeviceSetAutoBootFunction = NULL;
static AMDeviceEnterRecoveryFunctionType AMDeviceEnterRecoveryFunction = NULL;
static AMDeviceConnectFunctionType AMDeviceConnectFunction = NULL;
static AMDeviceIsPairedFunctionType AMDeviceIsPairedFunction = NULL;
static AMDevicePairFunctionType AMDevicePairFunction = NULL;
static AMDeviceValidatePairingFunctionType AMDeviceValidatePairingFunction = NULL;
static AMDeviceStartSessionFunctionType AMDeviceStartSessionFunction = NULL;
static AMDeviceCopyValueFunctionType AMDeviceCopyValueFunction = NULL;
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
	{0x0B90002,	0x00000004,	0x0000ED90,	0x0000F2C0,	0x0000F6F0},
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
	
	AMRestorePerformRecoveryModeRestoreFunction =
		(AMRestorePerformRecoveryModeRestoreFunctionType) GetProcAddress(deviceDLL, "AMRestorePerformRecoveryModeRestore");
		
	if(!AMRestorePerformRecoveryModeRestoreFunction	)
	{
		XLOG(2, "failed to load AMRestorePerformRecoveryModeRestore");
		return -1;
	}

	AMRestoreEnableFileLoggingFunction =
		(AMRestoreEnableFileLoggingFunctionType) GetProcAddress(deviceDLL, "AMRestoreEnableFileLogging");
		
	if(!AMRestoreEnableFileLoggingFunction	)
	{
		XLOG(2, "failed to load AMRestoreEnableFileLogging");
		return -1;
	}
	
	AMRecoveryModeDeviceGetProgressFunction =
		(AMRecoveryModeDeviceGetProgressFunctionType) GetProcAddress(deviceDLL, "AMRecoveryModeDeviceGetProgress");
		
	if(!AMRecoveryModeDeviceGetProgressFunction	)
	{
		XLOG(2, "failed to load AMRecoveryModeDeviceGetProgress");
		return -1;
	}
	
	AMRecoveryModeDeviceSetAutoBootFunction =
		(AMRecoveryModeDeviceSetAutoBootFunctionType) GetProcAddress(deviceDLL, "AMRecoveryModeDeviceSetAutoBoot");
		
	if(!AMRecoveryModeDeviceSetAutoBootFunction	)
	{
		XLOG(2, "failed to load AMRecoveryModeDeviceSetAutoBoot");
		return -1;
	}

	AMDeviceEnterRecoveryFunction =
		(AMDeviceEnterRecoveryFunctionType) GetProcAddress(deviceDLL, "AMDeviceEnterRecovery");
		
	if(!AMDeviceEnterRecoveryFunction	)
	{
		XLOG(2, "failed to load AMDeviceEnterRecovery");
		return -1;
	}

	AMDeviceConnectFunction =
		(AMDeviceConnectFunctionType) GetProcAddress(deviceDLL, "AMDeviceConnect");
		
	if(!AMDeviceConnectFunction)
	{
		XLOG(2, "failed to load AMDeviceConnect");
		return -1;
	}

	AMDeviceIsPairedFunction =
		(AMDeviceIsPairedFunctionType) GetProcAddress(deviceDLL, "AMDeviceIsPaired");
		
	if(!AMDeviceIsPairedFunction)
	{
		XLOG(2, "failed to load AMDeviceIsPaired");
		return -1;
	}

	AMDevicePairFunction =
		(AMDevicePairFunctionType) GetProcAddress(deviceDLL, "AMDevicePair");
		
	if(!AMDevicePairFunction)
	{
		XLOG(2, "failed to load AMDevicePair");
		return -1;
	}

	AMDeviceValidatePairingFunction =
		(AMDeviceValidatePairingFunctionType) GetProcAddress(deviceDLL, "AMDeviceValidatePairing");
		
	if(!AMDeviceValidatePairingFunction)
	{
		XLOG(2, "failed to load AMDeviceValidatePairing");
		return -1;
	}

	AMDeviceStartSessionFunction =
		(AMDeviceStartSessionFunctionType) GetProcAddress(deviceDLL, "AMDeviceStartSession");
		
	if(!AMDeviceStartSessionFunction)
	{
		XLOG(2, "failed to load AMDeviceStartSession");
		return -1;
	}

	AMDeviceCopyValueFunction =
		(AMDeviceCopyValueFunctionType) GetProcAddress(deviceDLL, "AMDeviceCopyValue");
		
	if(!AMDeviceCopyValueFunction)
	{
		XLOG(2, "failed to load AMDeviceCopyValue");
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

mach_error_t AMRestorePerformRecoveryModeRestore(struct am_recovery_device *rdev, CFDictionaryRef opts, void *callback, void *user_info)
{
	XLOG(3, "calling Windows AMRestorePerformRecoveryModeRestore");
	return AMRestorePerformRecoveryModeRestoreFunction(rdev, opts, callback, user_info);
}

mach_error_t AMRecoveryModeDeviceSetAutoBoot(struct am_recovery_device *rdev, char autoboot)
{
	XLOG(3, "calling Windows AMRecoveryModeDeviceSetAutoBoot");
	return AMRecoveryModeDeviceSetAutoBootFunction(rdev, autoboot);
}

mach_error_t AMDeviceEnterRecovery(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDeviceEnterRecovery");
	return AMDeviceEnterRecoveryFunction(rdev);
}

mach_error_t AMDeviceConnect(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDeviceConnect");
	return AMDeviceConnectFunction(rdev);
}

mach_error_t AMDeviceIsPaired(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDeviceIsPaired");
	return AMDeviceIsPairedFunction(rdev);
}

mach_error_t AMDevicePair(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDevicePair");
	return AMDevicePairFunction(rdev);
}

mach_error_t AMDeviceValidatePairing(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDeviceValidatePairing");
	return AMDeviceValidatePairingFunction(rdev);
}

mach_error_t AMDeviceStartSession(struct am_device *rdev)
{
	XLOG(3, "calling Windows AMDeviceStartSession");
	return AMDeviceStartSessionFunction(rdev);
}

CFStringRef AMDeviceCopyValue(struct am_device *device, unsigned int should_be_zero, const CFStringRef cfstring)
{
	XLOG(3, "calling Windows AMDeviceCopyValue");
	return AMDeviceCopyValueFunction(device, should_be_zero, cfstring);
}

unsigned int* AMRecoveryModeDeviceGetProgress(struct am_recovery_device *rdev, unsigned int* progress, unsigned int* total)
{
	return AMRecoveryModeDeviceGetProgressFunction(rdev, progress, total);
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
