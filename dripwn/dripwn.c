#include <assert.h>
#include <stdio.h>
#include <unzip.h>
#include <common.h>
#include <string.h>
#include <dmg/dmgfile.h>
#include <dmg/filevault.h>
#include <xpwn/plist.h>

#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)
char endianness;

char *createTempFile() {
	char tmpFileBuffer[512];
#ifdef WIN32
	char tmpFilePath[512];
	GetTempPath(512, tmpFilePath);
	GetTempFileName(tmpFilePath, "pwn", 0, tmpFileBuffer);
	CloseHandle(CreateFile(tmpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
#else
	strcpy(tmpFileBuffer, "/tmp/pwnXXXXXX");
	close(mkstemp(tmpFileBuffer));
	FILE* tFile = fopen(tmpFileBuffer, "wb");
	fclose(tFile);
#endif
	return strdup(tmpFileBuffer);
}

// This gets any file over 100 MB from the IPSW and writes it to a temporary file and returns the name of the temporary file.
// It is meant to get the rootfs image and is a bit hacky but is unlikely to break.
char *loadZipLarge(const char* ipsw) {
	char *fileName;
	void* buffer;
	unzFile zip;
	unz_file_info pfile_info;
	
	char *tmpFileName = createTempFile();
	ASSERT(zip = unzOpen(ipsw), "error opening ipsw");
	ASSERT(unzGoToFirstFile(zip) == UNZ_OK, "cannot seek to first file in ipsw");
	
	do {
		ASSERT(unzGetCurrentFileInfo(zip, &pfile_info, NULL, 0, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file info from ipsw");
		fileName = (char*) malloc(pfile_info.size_filename + 1);
		ASSERT(unzGetCurrentFileInfo(zip, NULL, fileName, pfile_info.size_filename + 1, NULL, 0, NULL, 0) == UNZ_OK, "cannot get current file name from ipsw");
		if(pfile_info.uncompressed_size > (100UL * 1024UL * 1024UL)) {
			ASSERT(unzOpenCurrentFile(zip) == UNZ_OK, "cannot open compressed file in IPSW");
			uLong fileSize = pfile_info.uncompressed_size;
			
			FILE *tmpFile = fopen(tmpFileName, "wb");
			buffer = malloc(DEFAULT_BUFFER_SIZE);
			size_t left = pfile_info.uncompressed_size;
			while(left > 0) {
				size_t toRead;
				if(left > DEFAULT_BUFFER_SIZE)
					toRead = DEFAULT_BUFFER_SIZE;
				else
					toRead = left;
				ASSERT(unzReadCurrentFile(zip, buffer, toRead) == toRead, "cannot read file from ipsw");
				fwrite(buffer, toRead, 1, tmpFile);
				left -= toRead;
			}
			fclose(tmpFile);
			free(buffer);
			break;
		}
	} while(unzGoToNextFile(zip) == UNZ_OK);
	return tmpFileName;
}

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

char *extractPlist(Volume* volume) {
	HFSPlusCatalogRecord* record;
	AbstractFile *outFile;
	char* outFileName = createTempFile();
	
	outFile = createAbstractFileFromFile(fopen(outFileName, "wb"));
	
	if(outFile == NULL) {
		printf("cannot create file");
	}
	
	record = getRecordFromPath("/usr/share/firmware/multitouch/iPhone.mtprops", volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, outFile, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	outFile->close(outFile);
	free(record);
	
	return outFileName;
}

int main(int argc, const char *argv[]) {
	io_func* io;
	Volume* volume;
	AbstractFile* image;
	FILE * iphone_mtprops;
    size_t file_len;
    char * buffer;
    char * p;
    char * a_speed_firmware;
    size_t a_speed_fw_len;
    size_t final_a_speed_fw_len;
    char * firmware;
    size_t fw_len;
    size_t final_fw_len;
    FILE * output;
	
	if(argc < 3) {
		printf("usage: %s <ipsw> <key>\n", argv[0]);
		return 0;
	}
	char *filename = loadZipLarge(argv[1]);
	
	TestByteOrder();
	image = createAbstractFileFromFile(fopen(filename, "rb"));
	image = createAbstractFileFromFileVault(image, argv[2]);
	io = openDmgFilePartition(image, -1);
	
	if(io == NULL) {
		fprintf(stderr, "error: cannot open dmg image\n");
		return 1;
	}
	
	volume = openVolume(io);
	if(volume == NULL) {
		fprintf(stderr, "error: cannot open volume\n");
		CLOSE(io);
		return 1;
	}
	char *plistName = extractPlist(volume);
	CLOSE(io);
	ASSERT(remove(filename) == 0, "Error deleting root filesystem.");

    iphone_mtprops = fopen(plistName, "r");
    assert(iphone_mtprops != NULL);
    fseek(iphone_mtprops, 0, SEEK_END);
    file_len = ftell(iphone_mtprops);

    buffer = (char *)malloc(sizeof(char) * (file_len + 1));
    fseek(iphone_mtprops, 0, SEEK_SET);
    fread(buffer, file_len, 1, iphone_mtprops);

    fclose(iphone_mtprops);

	buffer[file_len] = '\0';

	Dictionary* info = createRoot(buffer);
	assert(info != NULL);
	free(buffer);

	Dictionary* zephyr1FW = (Dictionary*) getValueByKey(info, "Z1F50,1");
	if(zephyr1FW)
	{
		DataValue* aspeedData = (DataValue*) getValueByKey(zephyr1FW, "A-Speed Firmware");
		assert(aspeedData != NULL);

		DataValue* fwData = (DataValue*) getValueByKey(zephyr1FW, "Firmware");
		assert(fwData != NULL);

		output = fopen("zephyr_aspeed.bin", "wb");
		assert(output != NULL);
		fwrite(aspeedData->value, aspeedData->len, 1, output);
		fclose(output);

		output = fopen("zephyr_main.bin", "wb");
		assert(output != NULL);
		fwrite(fwData->value, fwData->len, 1, output);
		fclose(output);
	}

	Dictionary* zephyr2FW = (Dictionary*) getValueByKey(info, "Z2F52,1");
	if(zephyr2FW)
	{
		DataValue* fwData = (DataValue*) getValueByKey(zephyr2FW, "Constructed Firmware");
		assert(fwData != NULL);

		output = fopen("zephyr2.bin", "wb");
		assert(output != NULL);
		fwrite(fwData->value, fwData->len, 1, output);
		fclose(output);
	}

	ASSERT(remove(plistName) == 0, "Error deleting iPhone.mtprops");
	printf("Zephyr files extracted succesfully.\n");
	return 0;
}
