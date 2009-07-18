#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "common.h"
#include <xpwn/libxpwn.h>
#include <xpwn/nor_files.h>
#include <dmg/dmg.h>
#include <dmg/filevault.h>
#include <xpwn/ibootim.h>
#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <hfs/hfslib.h>
#include <dmg/dmglib.h>
#include <xpwn/pwnutil.h>
#include <stdio.h>

#ifdef WIN32
#include <windows.h>
#endif

char endianness;

static char* tmpFile = NULL;

static AbstractFile* openRoot(void** buffer, size_t* rootSize) {
	static char tmpFileBuffer[512];

	if((*buffer) != NULL) {
		return createAbstractFileFromMemoryFile(buffer, rootSize);
	} else {
		if(tmpFile == NULL) {
#ifdef WIN32
			char tmpFilePath[512];
			GetTempPath(512, tmpFilePath);
			GetTempFileName(tmpFilePath, "root", 0, tmpFileBuffer);
			CloseHandle(CreateFile(tmpFilePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_DELETE, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_TEMPORARY, NULL));
#else
			strcpy(tmpFileBuffer, "/tmp/rootXXXXXX");
			close(mkstemp(tmpFileBuffer));
			FILE* tFile = fopen(tmpFileBuffer, "wb");
			fclose(tFile);
#endif
			tmpFile = tmpFileBuffer;
		}
		return createAbstractFileFromFile(fopen(tmpFile, "r+b"));
	}
}

void closeRoot(void* buffer) {
	if(buffer != NULL) {
		free(buffer);
	}

	if(tmpFile != NULL) {
		unlink(tmpFile);
	}
}

int main(int argc, char* argv[]) {
	init_libxpwn();
	
	Dictionary* info;
	Dictionary* firmwarePatches;
	Dictionary* patchDict;
	ArrayValue* patchArray;
	
	void* buffer;
	
	StringValue* actionValue;
	StringValue* pathValue;
	
	StringValue* fileValue;
	
	StringValue* patchValue;
	char* patchPath;

	char* rootFSPathInIPSW;
	io_func* rootFS;
	Volume* rootVolume;
	size_t rootSize;
	size_t preferredRootSize = 0;
	size_t minimumRootSize = 0;
	
	char* ramdiskFSPathInIPSW;
	unsigned int ramdiskKey[16];
	unsigned int ramdiskIV[16];
	unsigned int* pRamdiskKey = NULL;
	unsigned int* pRamdiskIV = NULL;
	io_func* ramdiskFS;
	Volume* ramdiskVolume;

	char* updateRamdiskFSPathInIPSW = NULL; 

	int i;

	OutputState* outputState;

	char* bundlePath;
	char* bundleRoot = "FirmwareBundles/";

	int mergePaths;
	char* outputIPSW;

	void* imageBuffer;	
	size_t imageSize;

	AbstractFile* bootloader39 = NULL;
	AbstractFile* bootloader46 = NULL;
	AbstractFile* applelogo = NULL;
	AbstractFile* recoverymode = NULL;

	char noWipe = FALSE;
	
	char unlockBaseband = FALSE;
	char selfDestruct = FALSE;
	char use39 = FALSE;
	char use46 = FALSE;
	char doBootNeuter = FALSE;
	char updateBB = FALSE;
	char useMemory = FALSE;

	unsigned int key[16];
	unsigned int iv[16];

	unsigned int* pKey = NULL;
	unsigned int* pIV = NULL;

	if(argc < 3) {
		XLOG(0, "usage %s <input.ipsw> <target.ipsw> [-b <bootimage.png>] [-r <recoveryimage.png>] [-s <system partition size>] [-memory] [-bbupdate] [-nowipe] [-e \"<action to exclude>\"] [[-unlock] [-use39] [-use46] [-cleanup] -3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] <package1.tar> <package2.tar>...\n", argv[0]);
		return 0;
	}

	outputIPSW = argv[2];

	int* toRemove = NULL;
	int numToRemove = 0;

	for(i = 3; i < argc; i++) {
		if(argv[i][0] != '-') {
			break;
		}

		if(strcmp(argv[i], "-memory") == 0) {
			useMemory = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-s") == 0) {
			int size;
			sscanf(argv[i + 1], "%d", &size);
			preferredRootSize = size;
			i++;
			continue;
		}

		if(strcmp(argv[i], "-nowipe") == 0) {
			noWipe = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-bbupdate") == 0) {
			updateBB = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-e") == 0) {
			numToRemove++;
			toRemove = realloc(toRemove, numToRemove * sizeof(int));
			toRemove[numToRemove - 1] = i + 1;
			i++;
			continue;
		}
		
		if(strcmp(argv[i], "-unlock") == 0) {
			unlockBaseband = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-cleanup") == 0) {
			selfDestruct = TRUE;
			continue;
		}
		
		if(strcmp(argv[i], "-use39") == 0) {
			if(use46) {
				XLOG(0, "error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use39 = TRUE;
			continue;
		}
		
		if(strcmp(argv[i], "-use46") == 0) {
			if(use39) {
				XLOG(0, "error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use46 = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-b") == 0) {
			applelogo = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!applelogo) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-r") == 0) {
			recoverymode = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!recoverymode) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-3") == 0) {
			bootloader39 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader39) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-4") == 0) {
			bootloader46 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader46) {
				XLOG(0, "cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
	}

	mergePaths = i;

	if(use39 || use46 || unlockBaseband || selfDestruct || bootloader39 || bootloader46) {
		if(!(bootloader39) || !(bootloader46)) {
			XLOG(0, "error: you must specify both bootloader files.\n");
			exit(1);
		} else {
			doBootNeuter = TRUE;
		}
	}

	info = parseIPSW2(argv[1], bundleRoot, &bundlePath, &outputState, useMemory);
	if(info == NULL) {
		XLOG(0, "error: Could not load IPSW\n");
		exit(1);
	}

	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");

	int j;
	for(j = 0; j < numToRemove; j++) {
		removeKey(firmwarePatches, argv[toRemove[j]]);
	}
	free(toRemove);

	firmwarePatches = (Dictionary*)getValueByKey(info, "FirmwarePatches");
	patchDict = (Dictionary*) firmwarePatches->values;
	while(patchDict != NULL) {
		fileValue = (StringValue*) getValueByKey(patchDict, "File");

		StringValue* keyValue = (StringValue*) getValueByKey(patchDict, "Key");
		StringValue* ivValue = (StringValue*) getValueByKey(patchDict, "IV");
		pKey = NULL;
		pIV = NULL;

		if(keyValue) {
			sscanf(keyValue->value, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
				&key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15]);

			pKey = key;
		}

		if(ivValue) {
			sscanf(ivValue->value, "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
				&iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);
			pIV = iv;
		}

		if(strcmp(patchDict->dValue.key, "Restore Ramdisk") == 0) {
			ramdiskFSPathInIPSW = fileValue->value;
			if(pKey) {
				memcpy(ramdiskKey, key, sizeof(key));
				memcpy(ramdiskIV, iv, sizeof(iv));
				pRamdiskKey = ramdiskKey;
				pRamdiskIV = ramdiskIV;
			} else {
				pRamdiskKey = NULL;
				pRamdiskIV = NULL;
			}
		}

		if(strcmp(patchDict->dValue.key, "Update Ramdisk") == 0) {
			updateRamdiskFSPathInIPSW = fileValue->value;
		}

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch2");
		if(patchValue) {
			if(noWipe) {
				XLOG(0, "%s: ", patchDict->dValue.key); fflush(stdout);
				doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV, useMemory);
				patchDict = (Dictionary*) patchDict->dValue.next;
				continue; /* skip over the normal Patch */
			}
		}

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
		if(patchValue) {
			XLOG(0, "%s: ", patchDict->dValue.key); fflush(stdout);
			doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV, useMemory);
		}
		
		if(strcmp(patchDict->dValue.key, "AppleLogo") == 0 && applelogo) {
			XLOG(0, "replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, applelogo, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}

		if(strcmp(patchDict->dValue.key, "RecoveryMode") == 0 && recoverymode) {
			XLOG(0, "replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, recoverymode, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}
		
		patchDict = (Dictionary*) patchDict->dValue.next;
	}

	fileValue = (StringValue*) getValueByKey(info, "RootFilesystem");
	rootFSPathInIPSW = fileValue->value;

	size_t defaultRootSize = ((IntegerValue*) getValueByKey(info, "RootFilesystemSize"))->value;
	minimumRootSize = defaultRootSize * 1000 * 1000;
	minimumRootSize -= minimumRootSize % 512;

	if(preferredRootSize == 0) {	
		preferredRootSize = defaultRootSize;
	}

	rootSize =  preferredRootSize * 1000 * 1000;
	rootSize -= rootSize % 512;

	if(useMemory) {
		buffer = malloc(rootSize);
	} else {
		buffer = NULL;
	}

	if(buffer == NULL) {
		XLOG(2, "using filesystem backed temporary storage\n");
	}

	extractDmg(
		createAbstractFileFromFileVault(getFileFromOutputState(&outputState, rootFSPathInIPSW), ((StringValue*)getValueByKey(info, "RootFilesystemKey"))->value),
		openRoot((void**)&buffer, &rootSize), -1);

	
	rootFS = IOFuncFromAbstractFile(openRoot((void**)&buffer, &rootSize));
	rootVolume = openVolume(rootFS);
	XLOG(0, "Growing root to minimum: %ld\n", (long) defaultRootSize); fflush(stdout);
	grow_hfs(rootVolume, minimumRootSize);
	if(rootSize > minimumRootSize) {
		XLOG(0, "Growing root: %ld\n", (long) preferredRootSize); fflush(stdout);
		grow_hfs(rootVolume, rootSize);
	}
	
	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");
	patchArray = (ArrayValue*) firmwarePatches->values;
	while(patchArray != NULL) {
		for(i = 0; i < patchArray->size; i++) {
			patchDict = (Dictionary*) patchArray->values[i];
			fileValue = (StringValue*) getValueByKey(patchDict, "File");
					
			actionValue = (StringValue*) getValueByKey(patchDict, "Action"); 
			if(strcmp(actionValue->value, "ReplaceKernel") == 0) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");
				XLOG(0, "replacing kernel... %s -> %s\n", fileValue->value, pathValue->value); fflush(stdout);
				add_hfs(rootVolume, getFileFromOutputState(&outputState, fileValue->value), pathValue->value);
			} if(strcmp(actionValue->value, "Patch") == 0) {
				patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
				patchPath = (char*) malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
				strcpy(patchPath, bundlePath);
				strcat(patchPath, "/");
				strcat(patchPath, patchValue->value);
				
				XLOG(0, "patching %s (%s)... ", fileValue->value, patchPath);
				doPatchInPlace(rootVolume, fileValue->value, patchPath);
				free(patchPath);
			}
		}
		
		patchArray = (ArrayValue*) patchArray->dValue.next;
	}
	
	for(; mergePaths < argc; mergePaths++) {
		XLOG(0, "merging %s\n", argv[mergePaths]);
		AbstractFile* tarFile = createAbstractFileFromFile(fopen(argv[mergePaths], "rb"));
		if(tarFile == NULL) {
			XLOG(1, "cannot find %s, make sure your slashes are in the right direction\n", argv[mergePaths]);
			releaseOutput(&outputState);
			closeRoot(buffer);
			exit(0);
		}
		hfs_untar(rootVolume, tarFile);
		tarFile->close(tarFile);
	}
	
	if(pRamdiskKey) {
		ramdiskFS = IOFuncFromAbstractFile(openAbstractFile2(getFileFromOutputStateForOverwrite(&outputState, ramdiskFSPathInIPSW), pRamdiskKey, pRamdiskIV));
	} else {
		XLOG(0, "unencrypted ramdisk\n");
		ramdiskFS = IOFuncFromAbstractFile(openAbstractFile(getFileFromOutputStateForOverwrite(&outputState, ramdiskFSPathInIPSW)));
	}
	ramdiskVolume = openVolume(ramdiskFS);
	XLOG(0, "growing ramdisk: %d -> %d\n", ramdiskVolume->volumeHeader->totalBlocks * ramdiskVolume->volumeHeader->blockSize, (ramdiskVolume->volumeHeader->totalBlocks + 4) * ramdiskVolume->volumeHeader->blockSize);
	grow_hfs(ramdiskVolume, (ramdiskVolume->volumeHeader->totalBlocks + 4) * ramdiskVolume->volumeHeader->blockSize);

	if(doBootNeuter) {
		firmwarePatches = (Dictionary*)getValueByKey(info, "BasebandPatches");
		if(firmwarePatches != NULL) {
			patchDict = (Dictionary*) firmwarePatches->values;
			while(patchDict != NULL) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");

				fileValue = (StringValue*) getValueByKey(patchDict, "File");		
				if(fileValue) {
					XLOG(0, "copying %s -> %s... ", fileValue->value, pathValue->value); fflush(stdout);
					if(copyAcrossVolumes(ramdiskVolume, rootVolume, fileValue->value, pathValue->value)) {
						patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
						if(patchValue) {
							patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
							strcpy(patchPath, bundlePath);
							strcat(patchPath, "/");
							strcat(patchPath, patchValue->value);
							XLOG(0, "patching %s (%s)... ", pathValue->value, patchPath); fflush(stdout);
							doPatchInPlace(rootVolume, pathValue->value, patchPath);
							free(patchPath);
						}
					}
				}

				if(strcmp(patchDict->dValue.key, "Bootloader 3.9") == 0 && bootloader39 != NULL) {
					add_hfs(rootVolume, bootloader39, pathValue->value);
				}

				if(strcmp(patchDict->dValue.key, "Bootloader 4.6") == 0 && bootloader46 != NULL) {
					add_hfs(rootVolume, bootloader46, pathValue->value);
				}
				
				patchDict = (Dictionary*) patchDict->dValue.next;
			}
		}
	
		fixupBootNeuterArgs(rootVolume, unlockBaseband, selfDestruct, use39, use46);
	}

	createRestoreOptions(ramdiskVolume, preferredRootSize, updateBB);
	closeVolume(ramdiskVolume);
	CLOSE(ramdiskFS);

	if(updateRamdiskFSPathInIPSW)
		removeFileFromOutputState(&outputState, updateRamdiskFSPathInIPSW);

	closeVolume(rootVolume);
	CLOSE(rootFS);

	buildDmg(openRoot((void**)&buffer, &rootSize), getFileFromOutputStateForReplace(&outputState, rootFSPathInIPSW), 2048);

	closeRoot(buffer);

	writeOutput(&outputState, outputIPSW);
	
	releaseDictionary(info);

	free(bundlePath);
	
	return 0;
}
