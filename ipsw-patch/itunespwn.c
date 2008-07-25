#include <windows.h>
#include <shlobj.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <xpwn/outputstate.h>

#define SHGFP_TYPE_CURRENT 0

const char restorePlist[] = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n<plist version=\"1.0\">\n<dict>\n	<key>DeviceClass</key>\n	<string>iPhone</string>\n	<key>DeviceMap</key>\n	<array/>\n	<key>FirmwareDirectory</key>\n	<string>Firmware</string>\n	<key>ProductBuildVersion</key>\n	<string>5A348</string>\n	<key>ProductType</key>\n	<string>iPhone1,1</string>\n	<key>ProductVersion</key>\n	<string>2.0</string>\n	<key>SupportedProductTypeIDs</key>\n	<dict>\n		<key>DFU</key>\n		<array>\n			<integer>304218112</integer>\n		</array>\n		<key>Recovery</key>\n		<array/>\n	</dict>\n</dict>\n</plist>\n";


int main(int argc, char* argv[]) {
	if(argc < 2) {
		printf("usage: %s <custom.ipsw>\n", argv[0]);
		return 0;
	}

	char path[MAX_PATH];
	SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, SHGFP_TYPE_CURRENT, path);
	strcat(path, "\\Apple Computer\\iTunes\\Device Support");

	struct stat st;
	if(stat(path, &st) < 0) {
		mkdir(path, 0755);
	}

	strcat(path, "\\x12220000_4_Recovery.ipsw");

	void* buffer;
	buffer = malloc(sizeof(restorePlist) - 1);
	memcpy(buffer, restorePlist, sizeof(restorePlist) - 1);

	OutputState* data = NULL;
	loadZipFile(argv[1], &data, "Firmware/dfu/WTF.s5l8900xall.RELEASE.dfu");
	addToOutput(&data, "Restore.plist", buffer, sizeof(restorePlist) - 1);
	writeOutput(&data, path);
	
	return 0;
}

