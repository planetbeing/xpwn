#include <stdio.h>
#include "idevice.h"
#include "mobiledevice.h"

char* extractedIPSWPath;
char* bootImagePath;

void dfu_progress_callback()
{
	XLOG(3, "DFU progress callback...");
}

void DoDFU(am_recovery_device *rdev, const char* restoreBundle) {
	CFMutableDictionaryRef opts;
	opts = AMRestoreCreateDefaultOptions(NULL);
	CFStringRef value = CFStringCreateWithCString(NULL, restoreBundle, kCFStringEncodingASCII);
	CFDictionarySetValue(opts, CFSTR("RestoreBundlePath"), value);
	
	int ret = AMRestorePerformDFURestore( rdev, opts,
									  (void*)dfu_progress_callback, NULL );

	XLOG(3, "AMRestorePerformDFURestore: %d", ret);

	CFRelease(value);
}

void notification(struct am_device_notification_callback_info *info)
{
	XLOG(3, "device notification");
	
	unsigned int msg = info->msg;
	
	if(msg == ADNCI_MSG_CONNECTED)
	{
		XLOG(3, "device connected in normal mode");
	} else if ( msg == ADNCI_MSG_DISCONNECTED ) {
		XLOG(3, "device disconnected in normal mode");
	}
}

void recovery_connect_callback(am_recovery_device *rdev)
{
	XLOG(3, "device connected in recovery mode");
	XLOG(3, "sendFileToDevice returned: %d", sendFileToDevice(rdev, CFStringCreateWithCString(NULL, bootImagePath, kCFStringEncodingASCII)));
	XLOG(3, "sendCommandToDevice returned: %d", sendCommandToDevice(rdev, CFSTR("setpicture 0"), 0));
	XLOG(3, "sendCommandToDevice returned: %d", sendCommandToDevice(rdev, CFSTR("bgcolor 0 0 0"), 0));
}

void recovery_disconnect_callback(am_recovery_device *rdev)
{
	XLOG(3, "device disconnected in recovery mode");
}

void dfu_connect_callback(am_recovery_device *rdev)
{
	XLOG(3, "device connected in dfu mode");
	DoDFU(rdev, extractedIPSWPath);
}

void dfu_disconnect_callback(am_recovery_device *rdev)
{
	XLOG(3, "device disconnected in dfu mode");
}

int main(int argc, char* argv[])
{
	if(argc < 3) {
		printf("usage: %s <path to extracted custom IPSW> <image to display>\n", argv[0]);
		return 0;
	}
	
	extractedIPSWPath = argv[1];
	bootImagePath = argv[2];
	
	if(LoadWindowsDLL() < 0) {
		printf("Failed to load iTunes Mobile Device driver!\n");
		return 1;
	}
	
	mach_error_t ret;
	struct am_device_notification *notif; 
	
	ret = AMDeviceNotificationSubscribe(notification, 0, 0, 0, &notif);
	if(ret < 0) {
		printf("Failed to subscribe for device notifications!\n");
		return 1;
	}
	
	ret = AMRestoreRegisterForDeviceNotifications(
						dfu_connect_callback,
						recovery_connect_callback,
						dfu_disconnect_callback,
						recovery_disconnect_callback,
						0,
						NULL);
						
	if(ret < 0) {
		printf("Failed to subscribe for device restore notifications!\n");
		return 1;
	}
	
	while(1) {
		msleep(1);
	}
}
