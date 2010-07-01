// WinAppleUSB.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <windows.h>
#include <setupapi.h>
#include "AppleMobileDevice.h"

#ifdef __MINGW32__
int main(int argc, _TCHAR* argv[])
#else
int _tmain(int argc, _TCHAR* argv[])
#endif
{
	system("TASKKILL /F /IM iTunes.exe > NUL");
	system("TASKKILL /F /IM iTunesHelper.exe > NUL");
	AppleMobileDevice* device;

	AppleMobileDevice::WaitFor(kDFUMode);
	device = AppleMobileDevice::Enumerate(NULL);
	device->Open();
	printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("C:\\Users\\David\\Documents\\WTF.s5l8900xall.RELEASE.dfu")));
	device->Close();
	Sleep(1000);

	AppleMobileDevice::WaitFor(kDFUMode);
	device = AppleMobileDevice::Enumerate(NULL);
	device->Open();
	printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("C:\\Users\\David\\Documents\\WTF.n82ap.RELEASE.dfu")));
	device->Close();
	Sleep(1000);

	AppleMobileDevice::WaitFor(kDFUMode);
	device = AppleMobileDevice::Enumerate(NULL);
	device->Open();
	printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("C:\\Users\\David\\Documents\\iBSS.n82ap.RELEASE.dfu")));
	device->Close();
	Sleep(1000);

	AppleMobileDevice::WaitFor(kRecoveryMode);
	device = AppleMobileDevice::Enumerate(NULL);
	device->Open();
	printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("C:\\Users\\David\\Documents\\iBEC.n82ap.RELEASE.dfu")));
	device->RecoverySendCommand("go");
	device->Close();
	Sleep(1000);
/*
	if(device)
	{
		device->Open();

		//char* bootPath = device->RecoveryGetEnv("boot-path");
		//printf("boot-path: %s\n", bootPath);
		//free(bootPath);

		//printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("C:\\WINDOWS\\regedit.exe")));
		//printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("WTF.s5l8900xall.RELEASE.dfu")));
		//printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("WTF.n82ap.RELEASE.dfu")));
		printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("WTF.s5l8900xall.RELEASE.dfu")));
		printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("WTF.n82ap.RELEASE.dfu")));
		printf("Sending file, result: %d\n", device->DFUSendFile(TEXT("iBSS.n82ap.RELEASE.dfu")));

//		device->RecoverySendCommand("setenv auto-boot true");
//		device->RecoverySendCommand("saveenv");
//		device->RecoverySendCommand("reboot");

		delete device;
	}
	else
	{
		printf("No recovery mode device found\n");
	}
*/
	printf("Press any key to exit...\n");
	getchar();
	return 0;
}

