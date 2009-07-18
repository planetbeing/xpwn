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

static volatile int Stage;

typedef enum {
	Disconnected,
	DFUConnected,
	NormalConnected,
	RecoveryConnected
} ConnectStatus;

static volatile ConnectStatus Status;


void cleanup_and_exit() {
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

	if(ret != 0) {
		fprintf(stdout, "I'm sorry, but something seems to have gone wrong during the download. Please try connecting your iPhone/iPod to another USB port (NOT through a hub) and trying again.\n");
		fflush(stdout);
		cleanup_and_exit();
	}

	XLOG(3, "AMRestorePerformDFURestore: %d", ret);

	CFRelease(value);
}

void notification(struct am_device_notification_callback_info *info)
{
	XLOG(3, "device notification");
	
	unsigned int msg = info->msg;
	
	if(msg == ADNCI_MSG_CONNECTED)
	{
		Status = NormalConnected;
		XLOG(3, "device connected in normal mode");

		if(Stage == 2) {
			fprintf(stdout, "Sorry, but you did not follow the instructions correctly. Please try again.\n");
			cleanup_and_exit();
		}
	} else if ( msg == ADNCI_MSG_DISCONNECTED ) {
		Status = Disconnected;
		XLOG(3, "device disconnected in normal mode");
	}
}

void recovery_connect_callback(am_recovery_device *rdev)
{
	Status = RecoveryConnected;
	XLOG(3, "device connected in recovery mode");

	if(Stage == 2) {
		fprintf(stdout, "Sorry, but you did not follow the instructions correctly. Please try again.\n");
		cleanup_and_exit();
	} else if(Stage != 3) {
		return;
	}

	XLOG(3, "sendCommandToDevice(setenv auto-boot) returned: %d", sendCommandToDevice(rdev, CFSTR("setenv auto-boot true"), 0));
	XLOG(3, "sendCommandToDevice(saveenv) returned: %d", sendCommandToDevice(rdev, CFSTR("saveenv"), 0));
	XLOG(3, "sendCommandToDevice(setenv idle-off) returned: %d", sendCommandToDevice(rdev, CFSTR("setenv idle-off false"), 0));
	XLOG(3, "sendFileToDevice(bootimage) returned: %d", sendFileToDevice(rdev, CFStringCreateWithCString(NULL, bootImagePath, kCFStringEncodingASCII)));
	XLOG(3, "sendCommandToDevice(setpicture) returned: %d", sendCommandToDevice(rdev, CFSTR("setpicture 0"), 0));
	XLOG(3, "sendCommandToDevice(bgcolor) returned: %d", sendCommandToDevice(rdev, CFSTR("bgcolor 0 0 0"), 0));

	fprintf(stdout, "Please use iTunes to restore your iPhone/iPod with a custom IPSW now. You may now let go of the home button.\n");
	fflush(stdout);

	cleanup_and_exit();
}

void recovery_disconnect_callback(am_recovery_device *rdev)
{
	Status = Disconnected;
	XLOG(3, "device disconnected in recovery mode");
}

void dfu_connect_callback(am_recovery_device *rdev)
{
	Status = DFUConnected;
	XLOG(3, "device connected in dfu mode");
	if(Stage != 3) {
		Stage = 3;
		fprintf(stdout, "Congratulations! You have successfully entered DFU mode. Please wait while your iPhone/iPod is being prepared to accept custom IPSWs...\n");
		fflush(stdout);
	}
	DoDFU(rdev, extractedIPSWPath);
}

void dfu_disconnect_callback(am_recovery_device *rdev)
{
	Status = Disconnected;
	XLOG(3, "device disconnected in dfu mode");
}

void logCB(const char* Message) {
	printf("%s\n", Message);
}

int main(int argc, char* argv[])
{
	struct stat st;

	init_libxpwn();
	libxpwn_log(logCB);
	libxpwn_loglevel(2);

	printf("---------------------------PLEASE READ THIS---------------------------\n");
	printf("Please make certain that all iTunes related processes are not running\n");
	printf("at this time (use Task Manager, etc. to end them).\n");
	printf("---------------------------PLEASE READ THIS---------------------------\n\n\n");

	if(argc < 3) {
		printf("usage: %s <custom.ipsw> <n82ap|m68ap|n45ap> [loglevel]\n", argv[0]);
		printf("n82ap = 3G iPhone, m68ap = First-generation iPhone, n45ap = iPod touch\n");
		return 0;
	}

	if(argc >= 4) {
		int logLevel;
		sscanf(argv[3], "%d", &logLevel);
		libxpwn_loglevel(logLevel);
	}

	if(stat("restore.img3", &st) < 0) {
		fprintf(stderr, "missing restore.img3\n");
		return 1;
	}

	Stage = 0;
	Status = Disconnected;

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
	bootImagePath = "restore.img3";

	fprintf(stdout, "\nGetting iPhone/iPod status...\n");
	fflush(stdout);

	if(LoadWindowsDLL() < 0) {
		printf("Failed to load iTunes Mobile Device driver!\n");
		cleanup_and_exit();
	}
	
	mach_error_t ret;
	struct am_device_notification *notif; 
	
	ret = AMDeviceNotificationSubscribe(notification, 0, 0, 0, &notif);
	if(ret < 0) {
		printf("Failed to subscribe for device notifications!\n");
		cleanup_and_exit();
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
		cleanup_and_exit();
	}


	sleep(2);

	char responseBuffer[10];
	int countdown;

	if(Status == Disconnected) {
connectDevice:
		fprintf(stdout, "Is your iPhone/iPod connected to your computer via USB?\n");
		fprintf(stdout, "Please answer (y/n): ");
		fflush(stdout);
		fgets(responseBuffer, 10, stdin);
		if(responseBuffer[0] == 'y' || responseBuffer[0] == 'Y') {
			goto isPoweringOn;
		} else if(responseBuffer[0] == 'n' || responseBuffer[0] == 'N') {
			fprintf(stdout, "Please connect your iPhone/iPod to your computer\n");
			fprintf(stdout, "Press enter when you have connected your iPhone/iPod... ");
			fflush(stdout);
			fgets(responseBuffer, 10, stdin);
			sleep(2);
			if(Status != Disconnected) {
				goto turnOffDevice;
			} else {
isPoweringOn:
				fprintf(stdout, "Is your iPhone currently powering on?\n");
				fprintf(stdout, "Please answer (y/n): ");
				fflush(stdout);
				fgets(responseBuffer, 10, stdin);
				if(responseBuffer[0] == 'y' || responseBuffer[0] == 'Y') {
					fprintf(stdout, "Waiting for iPhone/iPod to power on...\n");
					fflush(stdout);
					while(Status == Disconnected) {
						sleep(1);
					}
					goto turnOffDevice;
				} else if(responseBuffer[0] == 'n' || responseBuffer[0] == 'N') {
					goto beginDFU;
				} else {
					goto isPoweringOn;
				}
			}
		} else {
			goto connectDevice;
		}
	} else {
turnOffDevice:
		fprintf(stdout, "Please turn off your iPhone/iPod without disconnecting the cable connecting it to the computer\n");
		fprintf(stdout, "Press enter when you have turned off your iPhone/iPod... ");
		fflush(stdout);
		fgets(responseBuffer, 10, stdin);

		fprintf(stdout, "Waiting for iPhone/iPod to power off...\n");
		fflush(stdout);
		while(Status != Disconnected) {
			sleep(1);
		}
	}

beginDFU:
	fprintf(stdout, "\n!!! Your device should now be off. If it is not, please make sure it is before proceeding !!!\n\n");

	fprintf(stdout, "Timing is crucial for the following tasks. I will ask you to do the following (DON'T START YET):\n");
	fprintf(stdout, "\t1. Press and hold down the power button for five seconds\n");
	fprintf(stdout, "\t2. Without letting go of the power button, press and hold down the power AND home buttons for ten seconds\n");
	fprintf(stdout, "\t3. Without letting go of the home button, release the power button\n");
	fprintf(stdout, "\t4. Wait 30 seconds while holding down the home button\n");
	fprintf(stdout, "\nTry to get the timing as correct as possible, but don't fret if you miss it by a few seconds. It might still work, and if it doesn't, you can always try again. If you fail, you can always just turn the phone completely off by holding power and home for ten seconds, then pushing power to turn it back on.\n");
	fprintf(stdout, "\nAre you ready to begin?\n");
	fprintf(stdout, "Please answer (y/n): ");
	fflush(stdout);
	fgets(responseBuffer, 10, stdin);
	if(responseBuffer[0] != 'y' && responseBuffer[0] != 'Y')
		goto beginDFU;

	for(countdown = 5; countdown > 0; countdown--) {
		fprintf(stdout, "Beginning process in %d seconds...\n", countdown);
		fflush(stdout);
		sleep(1);
	}

	fprintf(stdout, "\nPress and hold down the POWER button (you should now be just holding the power button)... ");
	fflush(stdout);

	for(countdown = 5; countdown > 0; countdown--) {
		fprintf(stdout, "%d... ", countdown);
		fflush(stdout);
		sleep(1);
	}


	fprintf(stdout, "\n\nPress and hold down the HOME button, DO NOT LET GO OF THE POWER BUTTON (you should now be just holding both the power and home buttons)... ");
	fflush(stdout);

	for(countdown = 10; countdown > 0; countdown--) {
		fprintf(stdout, "%d... ", countdown);
		fflush(stdout);
		sleep(1);
	}

	fprintf(stdout, "\n\nRelease the POWER button, DO NOT LET GO OF THE HOME BUTTON (you should now be just holding the home button)... ");
	fflush(stdout);

	Stage = 2;

	for(countdown = 30; countdown > 0; countdown--) {
		if(Status != Disconnected)
			goto waitForFinish;

		fprintf(stdout, "%d... ", countdown);
		fflush(stdout);
		sleep(1);
	}

	fprintf(stdout, "\n\nEither you did not follow instructions correctly or your USB hardware is malfunctioning. Please use another USB port to connect your iPhone/iPod (NOT through a USB hub) and consider restarting your computer before trying again.\n");
	fflush(stdout);
	cleanup_and_exit();

waitForFinish:
	while(1) {
		msleep(1);
	}
}
