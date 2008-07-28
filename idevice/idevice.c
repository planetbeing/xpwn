#include <stdio.h>
#include <unistd.h>
#include "idevice.h"
#include "mobiledevice.h"
#include <xpwn/libxpwn.h>
#include <xpwn/outputstate.h>
#include <sys/types.h>
#include <sys/stat.h>

static char* extractedIPSWPath;
static char* bootImagePath;

static char tmpFilePath[MAX_PATH];
static char tmpFirmwarePath[MAX_PATH];
static char tmpDFUPath[MAX_PATH];
static char tmpXALLPath[MAX_PATH];
static char tmpWTFPath[MAX_PATH];
static char tmpIBSSPath[MAX_PATH];
static char tmpRestorePath[MAX_PATH];

static OutputState* data = NULL;

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
	XLOG(3, "sendCommandToDevice(setenv auto-boot) returned: %d", sendCommandToDevice(rdev, CFSTR("setenv auto-boot true"), 0));
	XLOG(3, "sendCommandToDevice(saveenv) returned: %d", sendCommandToDevice(rdev, CFSTR("saveenv"), 0));
	XLOG(3, "sendCommandToDevice(setenv idle-off) returned: %d", sendCommandToDevice(rdev, CFSTR("setenv idle-off false"), 0));
	XLOG(3, "sendFileToDevice(bootimage) returned: %d", sendFileToDevice(rdev, CFStringCreateWithCString(NULL, bootImagePath, kCFStringEncodingASCII)));
	XLOG(3, "sendCommandToDevice(setpicture) returned: %d", sendCommandToDevice(rdev, CFSTR("setpicture 0"), 0));
	XLOG(3, "sendCommandToDevice(bgcolor) returned: %d", sendCommandToDevice(rdev, CFSTR("bgcolor 0 0 0"), 0));

	unlink(tmpXALLPath);
	unlink(tmpWTFPath);
	unlink(tmpIBSSPath);
	unlink(tmpRestorePath);
	rmdir(tmpDFUPath);
	rmdir(tmpFirmwarePath);
	rmdir(tmpFilePath);
	releaseOutput(&data);

	exit(0);
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
	init_libxpwn();

	printf("---------------------------PLEASE READ THIS---------------------------\n");
	printf("Please make certain that all iTunes related processes are not running\n");
	printf("at this time (use Task Manager, etc. to end them). Your iPhone/iPod\n");
	printf("must be placed into DFU mode AFTER iTunes had been turned off. This\n");
	printf(" will allow me to talk to it without iTunes getting in beforehand.\n");
	printf("USB Product ID of iPhone ought to be 0x1222\n");
	printf("---------------------------PLEASE READ THIS---------------------------\n");

	if(argc < 4) {
		printf("usage: %s <custom.ipsw> <n82ap|m68ap|n45ap> <image-to-display.img3>\n", argv[0]);
		printf("n82ap = 3G iPhone, m68ap = First-generation iPhone, n45ap = iPod touch\n");
		return 0;
	}


	char ibssName[100];
	char wtfName[100];
	sprintf(ibssName, "Firmware/dfu/iBSS.%s.RELEASE.dfu", argv[2]);
	sprintf(wtfName, "Firmware/dfu/WTF.%s.RELEASE.dfu", argv[2]);

	data = NULL;
	loadZipFile(argv[1], &data, "Firmware/dfu/WTF.s5l8900xall.RELEASE.dfu");
	loadZipFile(argv[1], &data, ibssName);
	loadZipFile(argv[1], &data, wtfName);
	loadZipFile(argv[1], &data, "Restore.plist");

	AbstractFile* xallFile = getFileFromOutputState(&data, "Firmware/dfu/WTF.s5l8900xall.RELEASE.dfu");
	AbstractFile* wtfFile = getFileFromOutputState(&data, wtfName);
	AbstractFile* ibssFile = getFileFromOutputState(&data, ibssName);
	AbstractFile* restoreFile = getFileFromOutputState(&data, "Restore.plist");

	struct stat st;
	GetTempPath(MAX_PATH, tmpFilePath);

	strcat(tmpFilePath, "/restore");
	if(stat(tmpFilePath, &st) < 0) {
		mkdir(tmpFilePath, 0755);
	}

	strcpy(tmpFirmwarePath, tmpFilePath);
	strcat(tmpFirmwarePath, "/Firmware");
	if(stat(tmpFirmwarePath, &st) < 0) {
		mkdir(tmpFirmwarePath, 0755);
	}

	strcpy(tmpDFUPath, tmpFirmwarePath);
	strcat(tmpDFUPath, "/dfu");
	if(stat(tmpDFUPath, &st) < 0) {
		mkdir(tmpDFUPath, 0755);
	}

	strcpy(tmpXALLPath, tmpFilePath);
	strcat(tmpXALLPath, "/");
	strcat(tmpXALLPath, "Firmware/dfu/WTF.s5l8900xall.RELEASE.dfu");

	strcpy(tmpWTFPath, tmpFilePath);
	strcat(tmpWTFPath, "/");
	strcat(tmpWTFPath, wtfName);

	strcpy(tmpIBSSPath, tmpFilePath);
	strcat(tmpIBSSPath, "/");
	strcat(tmpIBSSPath, ibssName);

	strcpy(tmpRestorePath, tmpFilePath);
	strcat(tmpRestorePath, "/");
	strcat(tmpRestorePath, "Restore.plist");

	FILE* file;
	void* buffer;
	size_t length;

	length = xallFile->getLength(xallFile);
	buffer = malloc(length);
	xallFile->read(xallFile, buffer, length);
       	file = fopen(tmpXALLPath, "wb");
	fwrite(buffer, 1, length, file);
	fclose(file);
	free(buffer);
	xallFile->close(xallFile);

	length = wtfFile->getLength(wtfFile);
	buffer = malloc(length);
	wtfFile->read(wtfFile, buffer, length);
       	file = fopen(tmpWTFPath, "wb");
	fwrite(buffer, 1, length, file);
	fclose(file);
	free(buffer);
	wtfFile->close(wtfFile);

	length = ibssFile->getLength(ibssFile);
	buffer = malloc(length);
	ibssFile->read(ibssFile, buffer, length);
       	file = fopen(tmpIBSSPath, "wb");
	fwrite(buffer, 1, length, file);
	fclose(file);
	free(buffer);
	ibssFile->close(ibssFile);

	extractedIPSWPath = argv[1];
	length = restoreFile->getLength(restoreFile);
	buffer = malloc(length);
	restoreFile->read(restoreFile, buffer, length);
       	file = fopen(tmpRestorePath, "wb");
	fwrite(buffer, 1, length, file);
	fclose(file);
	free(buffer);
	restoreFile->close(restoreFile);

	extractedIPSWPath = tmpFilePath;
	bootImagePath = argv[3];
	
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
