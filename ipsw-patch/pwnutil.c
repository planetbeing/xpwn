#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <openssl/sha.h>
#include <xpwn/libxpwn.h>
#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <xpwn/pwnutil.h>
#include <xpwn/nor_files.h>
#include <hfs/hfslib.h>

#define BUFFERSIZE (1024*1024)

Dictionary* parseIPSW(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state) {
	return parseIPSW2(inputIPSW, bundleRoot, bundlePath, state, FALSE);
}

Dictionary* parseIPSW2(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state, int useMemory) {
	Dictionary* info;
	char* infoPath;

	AbstractFile* plistFile;
	char* plist;
	FILE* inputIPSWFile;

	SHA_CTX sha1_ctx;
	char* buffer;
	int read;
	unsigned char hash[20];

	DIR* dir;
	struct dirent* ent;
	StringValue* plistSHA1String;
	unsigned int plistHash[20];
	int i;

	*bundlePath = NULL;

	inputIPSWFile = fopen(inputIPSW, "rb");
	if(!inputIPSWFile) {
		return NULL;
	}

	XLOG(0, "Hashing IPSW...\n");

	buffer = malloc(BUFFERSIZE);
	SHA1_Init(&sha1_ctx);
	while(!feof(inputIPSWFile)) {
		read = fread(buffer, 1, BUFFERSIZE, inputIPSWFile);
		SHA1_Update(&sha1_ctx, buffer, read);
	}
	SHA1_Final(hash, &sha1_ctx);
	free(buffer);

	fclose(inputIPSWFile);

	XLOG(0, "Matching IPSW in %s... (%02x%02x%02x%02x...)\n", bundleRoot, (int) hash[0], (int) hash[1], (int) hash[2], (int) hash[3]);

	dir = opendir(bundleRoot);
	if(dir == NULL) {
		XLOG(1, "Bundles directory not found\n");
		return NULL;
	}

	while((ent = readdir(dir)) != NULL) {
		if(ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
			continue;
		}

		infoPath = (char*) malloc(sizeof(char) * (strlen(bundleRoot) + sizeof(PATH_SEPARATOR) + strlen(ent->d_name) + sizeof(PATH_SEPARATOR "Info.plist")));
		sprintf(infoPath, "%s" PATH_SEPARATOR "%s" PATH_SEPARATOR "Info.plist", bundleRoot, ent->d_name);
		XLOG(0, "checking: %s\n", infoPath);

		if((plistFile = createAbstractFileFromFile(fopen(infoPath, "rb"))) != NULL) {
			plist = (char*) malloc(plistFile->getLength(plistFile));
			plistFile->read(plistFile, plist, plistFile->getLength(plistFile));
			plistFile->close(plistFile);
			info = createRoot(plist);
			free(plist);

			plistSHA1String = (StringValue*)getValueByKey(info, "SHA1");
			if(plistSHA1String) {
				sscanf(plistSHA1String->value, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
					&plistHash[0], &plistHash[1], &plistHash[2], &plistHash[3], &plistHash[4],
					&plistHash[5], &plistHash[6], &plistHash[7], &plistHash[8], &plistHash[9],
					&plistHash[10], &plistHash[11], &plistHash[12], &plistHash[13], &plistHash[14],
					&plistHash[15], &plistHash[16], &plistHash[17], &plistHash[18], &plistHash[19]);

				for(i = 0; i < 20; i++) {
					if(plistHash[i] != hash[i]) {
						break;
					}
				}

				if(i == 20) {
					*bundlePath = (char*) malloc(sizeof(char) * (strlen(bundleRoot) + sizeof(PATH_SEPARATOR) + strlen(ent->d_name)));
					sprintf(*bundlePath, "%s" PATH_SEPARATOR "%s", bundleRoot, ent->d_name);

					free(infoPath);
					break;
				}
			}

			releaseDictionary(info);
		}

		free(infoPath);
	}

	closedir(dir);

	if(*bundlePath == NULL) {
		return NULL;
	}

	*state = loadZip2(inputIPSW, useMemory);

	return info;
}

int doPatch(StringValue* patchValue, StringValue* fileValue, const char* bundlePath, OutputState** state, unsigned int* key, unsigned int* iv, int useMemory) {
	char* patchPath;
	size_t bufferSize;
	void* buffer;
	
	AbstractFile* patchFile;
	AbstractFile* file;
	AbstractFile* out;
	AbstractFile* outRaw;

	char* tmpFileName;

	if(useMemory) {
		bufferSize = 0;
		buffer = malloc(1);
		outRaw = createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize);
	} else {
		tmpFileName = createTempFile();
		outRaw = createAbstractFileFromFile(fopen(tmpFileName, "wb"));
	}
			
	patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
	strcpy(patchPath, bundlePath);
	strcat(patchPath, "/");
	strcat(patchPath, patchValue->value);
	
	XLOG(0, "%s (%s)... ", fileValue->value, patchPath); fflush(stdout);
	
	patchFile = createAbstractFileFromFile(fopen(patchPath, "rb"));

	if(key != NULL) {
		XLOG(0, "encrypted input... ");
		out = duplicateAbstractFile2(getFileFromOutputState(state, fileValue->value), outRaw, key, iv, NULL);
	} else {
		out = duplicateAbstractFile(getFileFromOutputState(state, fileValue->value), outRaw);
	}

	if(key != NULL) {
		XLOG(0, "encrypted output... ");
		file = openAbstractFile2(getFileFromOutputState(state, fileValue->value), key, iv);
	} else {
		file = openAbstractFile(getFileFromOutputState(state, fileValue->value));
	}
	
	if(!patchFile || !file || !out) {
		XLOG(0, "file error\n");
		exit(0);
	}

	if(patch(file, out, patchFile) != 0) {
		XLOG(0, "patch failed\n");
		exit(0);
	}

	if(strstr(fileValue->value, "WTF.s5l8900xall.RELEASE")) {
		XLOG(0, "Exploiting 8900 vulnerability... ;)\n");
		AbstractFile* exploited;
		if(useMemory) {
			exploited = createAbstractFileFrom8900(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
		} else {
			exploited = createAbstractFileFrom8900(createAbstractFileFromFile(fopen(tmpFileName, "r+b")));
		}
		exploit8900(exploited);
		exploited->close(exploited);
	}
	
	XLOG(0, "writing... "); fflush(stdout);
	
	if(useMemory) {
		addToOutput(state, fileValue->value, buffer, bufferSize);
	} else {
		outRaw = createAbstractFileFromFile(fopen(tmpFileName, "rb"));
		size_t length = outRaw->getLength(outRaw);
		outRaw->close(outRaw);
		addToOutput2(state, fileValue->value, NULL, length, tmpFileName);
	}

	XLOG(0, "success\n"); fflush(stdout);

	free(patchPath);

	return 0;
}

void doPatchInPlace(Volume* volume, const char* filePath, const char* patchPath) {
	void* buffer;
	void* buffer2;
	size_t bufferSize;
	size_t bufferSize2;
	AbstractFile* bufferFile;
	AbstractFile* patchFile;
	AbstractFile* out;

	
	buffer = malloc(1);
	bufferSize = 0;
	bufferFile = createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize);

	XLOG(0, "retrieving..."); fflush(stdout);
	get_hfs(volume, filePath, bufferFile);
	bufferFile->close(bufferFile);
	
	XLOG(0, "patching..."); fflush(stdout);
				
	patchFile = createAbstractFileFromFile(fopen(patchPath, "rb"));

	buffer2 = malloc(1);
	bufferSize2 = 0;
	out = duplicateAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize), createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2));

	// reopen the inner package
	bufferFile = openAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
	
	if(!patchFile || !bufferFile || !out) {
		XLOG(0, "file error\n");
		exit(0);
	}

	if(patch(bufferFile, out, patchFile) != 0) {
		XLOG(0, "patch failed\n");
		exit(0);
	}
	
	XLOG(0, "writing... "); fflush(stdout);
	add_hfs(volume, createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2), filePath);
	free(buffer2);
	free(buffer);

	XLOG(0, "success\n"); fflush(stdout);
}

void doPatchInPlaceMemoryPatch(Volume* volume, const char* filePath, void** patchData, size_t* patchSize) {
	void* buffer;
	void* buffer2;
	size_t bufferSize;
	size_t bufferSize2;
	AbstractFile* bufferFile;
	AbstractFile* patchFile;
	AbstractFile* out;
	
	buffer = malloc(1);
	bufferSize = 0;
	bufferFile = createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize);

	XLOG(0, "retrieving..."); fflush(stdout);
	get_hfs(volume, filePath, bufferFile);
	bufferFile->close(bufferFile);
	
	XLOG(0, "patching..."); fflush(stdout);
				
	patchFile = createAbstractFileFromMemoryFile(patchData, patchSize);

	buffer2 = malloc(1);
	bufferSize2 = 0;
	out = duplicateAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize), createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2));

	// reopen the inner package
	bufferFile = openAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
	
	if(!patchFile || !bufferFile || !out) {
		XLOG(0, "file error\n");
		exit(0);
	}

	if(patch(bufferFile, out, patchFile) != 0) {
		XLOG(0, "patch failed\n");
		exit(0);
	}
	
	XLOG(0, "writing... "); fflush(stdout);
	add_hfs(volume, createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2), filePath);
	free(buffer2);
	free(buffer);

	XLOG(0, "success\n"); fflush(stdout);
}

void createRestoreOptions(Volume* volume, int SystemPartitionSize, int UpdateBaseband) {
	const char optionsPlist[] = "/usr/local/share/restore/options.plist";
	AbstractFile* plistFile;
	Dictionary* info;
	char* plist;

	XLOG(0, "start create restore options\n");

	info = createRoot("<dict></dict>");
	addBoolToDictionary(info, "CreateFilesystemPartitions", TRUE);
	addIntegerToDictionary(info, "SystemPartitionSize", SystemPartitionSize);
	addBoolToDictionary(info, "UpdateBaseband", UpdateBaseband);

	plist = getXmlFromRoot(info);
	releaseDictionary(info);
	
	XLOG(0, "%s", plist);

	plistFile = createAbstractFileFromMemory((void**)&plist, sizeof(char) * strlen(plist));

	add_hfs(volume, plistFile, optionsPlist);
	free(plist);
}

void fixupBootNeuterArgs(Volume* volume, char unlockBaseband, char selfDestruct, char use39, char use46) {
	const char bootNeuterPlist[] = "/System/Library/LaunchDaemons/com.devteam.bootneuter.auto.plist";
	AbstractFile* plistFile;
	char* plist;
	Dictionary* info;
	size_t bufferSize;
	ArrayValue* arguments;
	
	XLOG(0, "fixing up BootNeuter arguments...\n");
	
	plist = malloc(1);
	bufferSize = 0;
	plistFile = createAbstractFileFromMemoryFile((void**)&plist, &bufferSize);
	get_hfs(volume, bootNeuterPlist, plistFile);	
	plistFile->close(plistFile);
	info = createRoot(plist);
	free(plist);

	arguments = (ArrayValue*) getValueByKey(info, "ProgramArguments");
	addStringToArray(arguments, "-autoMode");
	addStringToArray(arguments, "YES");
	addStringToArray(arguments, "-RegisterForSystemEvents");
	addStringToArray(arguments, "YES");
	
	if(unlockBaseband) {
		addStringToArray(arguments, "-unlockBaseband");
		addStringToArray(arguments, "YES");
	}
	
	if(selfDestruct) {
		addStringToArray(arguments, "-selfDestruct");
		addStringToArray(arguments, "YES");
	}
	
	if(use39) {
		addStringToArray(arguments, "-bootLoader");
		addStringToArray(arguments, "3.9");
	} else if(use46) {
		addStringToArray(arguments, "-bootLoader");
		addStringToArray(arguments, "4.6");
	}
	
	plist = getXmlFromRoot(info);
	releaseDictionary(info);
	
	plistFile = createAbstractFileFromMemory((void**)&plist, sizeof(char) * strlen(plist));
	add_hfs(volume, plistFile, bootNeuterPlist);
	free(plist);
}

int patchSigCheck(AbstractFile* file) {
	const uint8_t patch[] = {0x01, 0xE0, 0x01, 0x20, 0x40, 0x42, 0x88, 0x23};

	// for 3gs, signature check
	const uint8_t patch2[] = {0x08, 0xB1, 0x4F, 0xF0, 0xFF, 0x30, 0xA7, 0xF1, 0x10, 0x0D};
	
	// for 3gs, prod check
	const uint8_t patch3[] = {0x03, 0x94, 0xFF, 0xF7, 0x11, 0xFF, 0x04, 0x46};

	// for 3gs, ecid check
	const uint8_t patch4[] = {0x50, 0x46, 0xFF, 0xF7, 0xB1, 0xFE, 0x04, 0x46};

	size_t length = file->getLength(file);
	uint8_t* buffer = (uint8_t*)malloc(length + sizeof(patch2));
	file->seek(file, 0);
	file->read(file, buffer, length);
	memset(buffer + length, 0, sizeof(patch2));
	
	int retval = FALSE;
	int i;
	for(i = 0; i < length; i++) {
		uint8_t* candidate = &buffer[i];
		if(memcmp(candidate, patch, sizeof(patch)) == 0) {
			candidate[4] = 0;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch));
			retval = TRUE;
			XLOG(3, "iBoot armv6 signature check patch success\n"); fflush(stdout);
			continue;
		}
		if(memcmp(candidate, patch2, sizeof(patch2)) == 0) {
			candidate[2] = 0x0;
			candidate[3] = 0x20;
			candidate[4] = 0x0;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch2));
			retval = TRUE;
			XLOG(3, "iBoot armv7 signature check patch success\n"); fflush(stdout);
			continue;
		}
		if(memcmp(candidate, patch3, sizeof(patch3)) == 0) {
			candidate[2] = 0x0;
			candidate[3] = 0x20;
			candidate[4] = 0x0;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch3));
			retval = TRUE;
			XLOG(3, "iBoot armv7 PROD check patch success\n"); fflush(stdout);
			continue;
		}
		if(memcmp(candidate, patch4, sizeof(patch4)) == 0) {
			candidate[2] = 0x0;
			candidate[3] = 0x20;
			candidate[4] = 0x0;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch4));
			retval = TRUE;
			XLOG(3, "iBoot armv7 ECID check patch success\n"); fflush(stdout);
			continue;
		}
	}
	
	free(buffer);
	return retval;
}

int Do30Patches = TRUE;

int patchKernel(AbstractFile* file) {
	// codesign
	const char patch[] = {0x00, 0x00, 0x00, 0x0A, 0x00, 0x40, 0xA0, 0xE3, 0x04, 0x00, 0xA0, 0xE1, 0x90, 0x80, 0xBD, 0xE8};

	const char patch_old[] = {0xFF, 0x50, 0xA0, 0xE3, 0x04, 0x00, 0xA0, 0xE1, 0x0A, 0x10, 0xA0, 0xE1};

	// 2.0 vm_map max_prot
	const char patch3[] = {0x99, 0x91, 0x43, 0x2B, 0x91, 0xCD, 0xE7, 0x04, 0x24, 0x1D, 0xB0};
	
	// 3.0 vm_map max_prot
	const char patch2[] = {0x2E, 0xD1, 0x35, 0x98, 0x80, 0x07, 0x33, 0xD4, 0x6B, 0x08, 0x1E, 0x1C};

	// 3.0 illb img3 patch 1
	const char patch4[] = {0x98, 0x47, 0x00, 0x28, 0x00, 0xD0, 0xAE, 0xE0, 0x06, 0x98};

	// 3.0 illb img3 patch 2
	const char patch5[] = {0x05, 0x1E, 0x00, 0xD0, 0xA8, 0xE0, 0x03, 0x9B};

	// 3.0 CS enforcement patch
	const char patch6[] = {0x9C, 0x22, 0x03, 0x59, 0x99, 0x58};

	// 3.0 armv7 vm_map_enter max_prot
	const char n88patch1[] = {0x93, 0xBB, 0x16, 0xF0, 0x02, 0x0F, 0x40, 0xF0, 0x36, 0x80, 0x63, 0x08};

	// 3.0 armv7 cs patch
	const char n88patch2[] = {0x05, 0x4B, 0x98, 0x47, 0x00, 0xB1, 0x00, 0x24, 0x20, 0x46, 0x90, 0xBD};

	// 3.0 armv7 cs_enforcement_disable patch
	const char n88patch3[] = {0xD3, 0xF8, 0x9C, 0x20, 0xDF, 0xF8};

	// 3.0 arm7 img3 prod patch
	const char n88patch4[] = {0x03, 0x94, 0xFF, 0xF7, 0x29, 0xFF, 0xF8, 0xB1};

	// 3.0 arm7 img3 ecid patch
	const char n88patch5[] = {0x0C, 0xCA, 0xFF, 0xF7, 0x10, 0xFF, 0x00, 0x38};

	// 3.0 arm7 img3 signature patch
	const char n88patch6[] = {0x30, 0xE0, 0x4F, 0xF0, 0xFF, 0x30, 0x2D, 0xE0};

	// 3.0 arm7 img3 signature patch
	const char n88patch_test1[] = {0x67, 0x4B, 0x98, 0x47, 0x00, 0x28};

	// 3.0 arm7 img3 signature patch
	const char n88patch_test2[] = {0x04, 0x98, 0xFF, 0xF7, 0xD9, 0xFD, 0x04, 0x46};

	// 3.0 arm7 img3 signature patch
	const char n88patch_test3[] = {0x01, 0x99, 0xFF, 0xF7, 0xBE, 0xFC, 0x00, 0xB3};

	size_t length = file->getLength(file);
	uint8_t* buffer = (uint8_t*)malloc(length + sizeof(patch));
	file->seek(file, 0);
	file->read(file, buffer, length);
	memset(buffer + length, 0, sizeof(patch));
	
	int retval = 0;
	int i;
	for(i = 0; i < length; i++) {
		uint8_t* candidate = &buffer[i];
		if(memcmp(candidate, patch, sizeof(patch)) == 0) {
			XLOG(3, "kernel patch1 success\n"); fflush(stdout);
			candidate[4] = 0x01;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch));
			retval = TRUE;
			continue;
		}
		if(memcmp(candidate, patch_old, sizeof(patch_old)) == 0) {
			XLOG(3, "kernel patch_old success\n"); fflush(stdout);
			candidate[0] = 0x0;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch_old));
			retval = TRUE;
			continue;
		}
		if(memcmp(candidate, patch3, sizeof(patch3)) == 0) {
			XLOG(3, "kernel patch3 success\n"); fflush(stdout);
			candidate[0] = 0x2B;
			candidate[1] = 0x99;
			candidate[2] = 0x00;
			candidate[3] = 0x00;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch3));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, patch2, sizeof(patch2)) == 0) {
			XLOG(3, "kernel patch2 success\n"); fflush(stdout);
			// NOP out the BMI
			candidate[6] = 0x00;
			candidate[7] = 0x28;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch2));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, patch4, sizeof(patch4)) == 0) {
			XLOG(3, "kernel patch4 success\n"); fflush(stdout);
			candidate[3] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch4));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, patch5, sizeof(patch5)) == 0) {
			XLOG(3, "kernel patch5 success\n"); fflush(stdout);
			candidate[0] = 0x00;
			candidate[1] = 0x25;
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch5));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch1, sizeof(n88patch1)) == 0) {
			XLOG(3, "kernel armv7 vm_map_enter patch success\n"); fflush(stdout);
			candidate[6] = 0x8B;
			candidate[7] = 0x46;
			candidate[8] = 0x8B;
			candidate[9] = 0x46;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch1));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch2, sizeof(n88patch2)) == 0) {
			XLOG(3, "kernel armv7 cs patch success\n"); fflush(stdout);
			candidate[6] = 0x1;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch2));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch4, sizeof(n88patch4)) == 0) {
			XLOG(3, "kernel armv7 img3 prod patch success\n"); fflush(stdout);
			candidate[2] = 0x00;
			candidate[3] = 0x20;
			candidate[4] = 0x00;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch4));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch5, sizeof(n88patch5)) == 0) {
			XLOG(3, "kernel armv7 img3 ecid patch success\n"); fflush(stdout);
			candidate[2] = 0x00;
			candidate[3] = 0x20;
			candidate[4] = 0x00;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch5));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch6, sizeof(n88patch6)) == 0) {
			XLOG(3, "kernel armv7 img3 signature patch success\n"); fflush(stdout);
			candidate[2] = 0x00;
			candidate[3] = 0x20;
			candidate[4] = 0x00;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch6));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch_test1, sizeof(n88patch_test1)) == 0) {
			XLOG(3, "kernel armv7 img3 ParseFirmwareFooter patch success\n"); fflush(stdout);
			candidate[2] = 0x00;
			candidate[3] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch_test1));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch_test2, sizeof(n88patch_test2)) == 0) {
			XLOG(3, "kernel armv7 img3 CheckMetaTags patch success\n"); fflush(stdout);
			candidate[2] = 0x00;
			candidate[3] = 0x20;
			candidate[4] = 0x00;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch_test2));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch_test3, sizeof(n88patch_test3)) == 0) {
			XLOG(3, "kernel armv7 img3 encrypt SHSH with 89A patch success\n"); fflush(stdout);
			candidate[2] = 0x01;
			candidate[3] = 0x20;
			candidate[4] = 0x01;
			candidate[5] = 0x20;
			file->seek(file, i);
			file->write(file, candidate, sizeof(n88patch_test3));
			retval = TRUE;
			continue;
		}
		if(Do30Patches && memcmp(candidate, patch6, sizeof(patch6)) == 0) {
			if(candidate[7] != 0x4B)
				continue;

			uint32_t cs_enforcement_disable = *((uint32_t*)(((intptr_t)(&candidate[6] + 0x4) & ~0x3) + (0x4 * candidate[6])));
			FLIPENDIANLE(cs_enforcement_disable);

			uint32_t offset = cs_enforcement_disable - 0xC0008000;

			XLOG(3, "kernel cs_enforcement_disable at: 0x%X, 0x%X\n", cs_enforcement_disable, offset); fflush(stdout);

			uint32_t value = 1;

			FLIPENDIANLE(value);

			*((uint32_t*)(buffer + offset)) = value;

			file->seek(file, offset);
			file->write(file, &value, sizeof(value));
			continue;
		}
		if(Do30Patches && memcmp(candidate, n88patch3, sizeof(n88patch3)) == 0) {
			uint32_t cs_enforcement_disable = *((uint32_t*)(((intptr_t)(&candidate[4] + 0x4) & ~0x3) + (((candidate[7] & 0xF) << 8) + candidate[6])));
			FLIPENDIANLE(cs_enforcement_disable);

			uint32_t offset = cs_enforcement_disable - 0xC0008000;

			XLOG(3, "kernel cs_enforcement_disable at: 0x%X, 0x%X\n", cs_enforcement_disable, offset); fflush(stdout);

			uint32_t value = 1;

			FLIPENDIANLE(value);

			*((uint32_t*)(buffer + offset)) = value;

			file->seek(file, offset);
			file->write(file, &value, sizeof(value));
			continue;
		}
	}
	
	free(buffer);
	return retval;
}

int patchDeviceTree(AbstractFile* file) {
	const char patch[] = "secure-root-prefix";
	const char patch2[] = "function-disable_keys";
	
	size_t length = file->getLength(file);
	uint8_t* buffer = (uint8_t*)malloc(length + sizeof(patch2));
	file->seek(file, 0);
	file->read(file, buffer, length);
	memset(buffer + length, 0, sizeof(patch2));
	
	int retval = 0;
	int i;
	for(i = 0; i < length; i++) {
		uint8_t* candidate = &buffer[i];
		if(memcmp(candidate, patch, sizeof(patch) - 1) == 0) {
			candidate[0] = 'x';
			candidate[1] = 'x';
			candidate[2] = 'x';
			candidate[3] = 'x';
			candidate[4] = 'x';
			candidate[5] = 'x';
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch) - 1);
			retval++;
			continue;
		}
		if(memcmp(candidate, patch2, sizeof(patch2) - 1) == 0) {
			candidate[0] = 'x';
			candidate[1] = 'x';
			candidate[2] = 'x';
			candidate[3] = 'x';
			candidate[4] = 'x';
			candidate[5] = 'x';
			candidate[6] = 'x';
			candidate[7] = 'x';
			file->seek(file, i);
			file->write(file, candidate, sizeof(patch) - 1);
			retval++;
			continue;
		}
	}
	
	free(buffer);
	if(retval == 2)
		return TRUE;
	else
		return FALSE;
}

