#include "../../hfs/common.h"
#include "../../ipsw-patch/pwnutil.h"
#include "../../ipsw-patch/plist.h"
#include "../../ipsw-patch/outputstate.h"
#include "../../hfs/hfslib.h"
#include "../../ipsw-patch/ibootim.h"
#include "libibooter.h"
#include <iostream>

using namespace ibooter;
using namespace std;

char endianness;

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

int main(int argc, char *argv[]) 
{
	const char* bundleRoot = "FirmwareBundles/";
	const char *pResponse = NULL;
	CIBootConn conn;
	ERR_CODE code;
	OutputState* ipswContents;
	char* bundlePath;
	Dictionary* info;
	StringValue* kernelValue;
	AbstractFile* ramdisk;

	AbstractFile* applelogo;
	AbstractFile* recoverymode;
	int i;

	TestByteOrder();

	applelogo = NULL;
	recoverymode = NULL;

	if(argc < 2) {
		cout << "usage: " << argv[0] << " <input ipsw> [-b <bootimage.png>] [-r <recoveryimage.png>]" << endl;
		return 1;
	}

	for(i = 2; i < argc; i++) {	
		if(strcmp(argv[i], "-b") == 0) {
			applelogo = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!applelogo) {
				cout << "cannot open " << argv[i + 1] << endl;
				return 1;
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-r") == 0) {
			recoverymode = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!recoverymode) {
				cout << "cannot open " << argv[i + 1] << endl;
				return 1;
			}
			i++;
			continue;
		}
	}

	cout << " ... Connecting" << endl;
	if ((code = conn.Connect()) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		return 1;
	}

	cout << " ... Loading IPSW" << endl;
	info = parseIPSW(argv[1], bundleRoot, &bundlePath, &ipswContents);

	if(!info) {
		printf("error: cannot load IPSW file\n");
		exit(1);
	}

	cout << " ... Opening ramdisk" << endl;
	AbstractFile* ramdiskSrc = createAbstractFileFromFile(fopen("ramdisk.dmg", "rb"));
	size_t bufferSize = ramdiskSrc->getLength(ramdiskSrc);
	void* buffer = (void*) malloc(bufferSize);
	cout << " ... Reading ramdisk" << endl;
	ramdiskSrc->read(ramdiskSrc, buffer, bufferSize);

	if(applelogo || recoverymode) {
		io_func* myRamdisk = IOFuncFromAbstractFile(createAbstractFileFromMemoryFile(&buffer, &bufferSize));
		Volume* ramdiskVolume = openVolume(myRamdisk);
		cout << " ... Adding boot logos" << endl;

		StringValue* fileValue;
		void* imageBuffer;
		size_t imageSize;

		if(applelogo) {
			fileValue = (StringValue*) getValueByKey((Dictionary*)getValueByKey((Dictionary*)getValueByKey(info, "FirmwarePatches"), "AppleLogo"), "File");
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&ipswContents, fileValue->value), applelogo, &imageSize)) != NULL, "failed to use new image");
			add_hfs(ramdiskVolume, createAbstractFileFromMemory(&imageBuffer, imageSize), "/ipwner/logo.img2");
		}

		if(recoverymode) {
			fileValue = (StringValue*) getValueByKey((Dictionary*)getValueByKey((Dictionary*)getValueByKey(info, "FirmwarePatches"), "RecoveryMode"), "File");
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&ipswContents, fileValue->value), recoverymode, &imageSize)) != NULL, "failed to use new image");
			add_hfs(ramdiskVolume, createAbstractFileFromMemory(&imageBuffer, imageSize), "/ipwner/recovery.img2");			
		}

		cout << " ... Finalizing ramdisk" << endl;
		closeVolume(ramdiskVolume);
		CLOSE(myRamdisk);
	}

	ramdisk = createAbstractFileFromMemoryFile(&buffer, &bufferSize);

	kernelValue = (StringValue*) getValueByKey((Dictionary*)getValueByKey((Dictionary*)getValueByKey(info, "FirmwarePatches"), "KernelCache"), "File");
	if(!kernelValue) {
		cout << "Unable to determine kernel cache file name from bundle plist!";
		return 1;
	}
	
	cout << " ... Will send kernel at: " << kernelValue->value << endl;

	cout << " ... Sending ramdisk" << endl;
	if ((code = conn.SendFile(ramdisk, 0x09400000)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;

	cout << " ... Sending kernelcache" << endl;
	if ((code = conn.SendFile(getFileFromOutputState(&ipswContents, kernelValue->value), 0x09000000)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;

	cout << " ... Clearing boot arguments" << endl;
	if ((code = conn.SendCommand("setenv boot-args \"\"\n")) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}
	cout << "Response: " << pResponse << endl;

	cout << " ... Setting auto-reboot" << endl;
	if ((code = conn.SendCommand("setenv auto-boot true\n")) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;

	cout << " ... Saving environment" << endl;
	if ((code = conn.SendCommand("saveenv\n")) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;

	cout << " ... Setting up ramdisk" << endl;
	char bootArgsBuf[1024];
	sprintf(bootArgsBuf, "setenv boot-args \"-v pmd0=0x09400000.0x%x pmd1=0x8000000.0x8000000 rd=md0\"\n", bufferSize);
	if ((code = conn.SendCommand(bootArgsBuf)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	cout << "Response: " << pResponse << endl;

	cout << " ... Booting" << endl;
	if ((code = conn.SendCommand("bootx\n")) != IB_SUCCESS)
	{
		cout << errcode_to_str(code) << endl;
		conn.Disconnect();
		return 1;
	}

	if ((code = conn.GetResponse(pResponse)) == IB_SUCCESS)
	{
		conn.Disconnect();
		cout << "Response: " << pResponse << endl;
		cout << "Booting did not appear to be successful." << endl;
	} else {

		conn.Disconnect();
		cout << "Disconnected. Please wait patiently until it has rebooted to the SpringBoard." << endl;
		cout << "If you get repeating 'bsd root' messages, it means the ramdisk somehow got corrupted in memory before it could be loaded. Just reboot into recovery mode and try again." << endl;
	}

	releaseOutput(&ipswContents);
	releaseDictionary(info);
	free(bundlePath);

	return 0;
}
