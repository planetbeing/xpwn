#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include "common.h"
#include <xpwn/nor_files.h>
#include <dmg/dmg.h>
#include <dmg/filevault.h>
#include <xpwn/ibootim.h>
#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <hfs/hfslib.h>
#include <dmg/dmglib.h>
#include <xpwn/pwnutil.h>

char endianness;

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

int main(int argc, char* argv[]) {
	TestByteOrder();
	
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
	
	char* ramdiskFSPathInIPSW;
	unsigned int ramdiskKey[16];
	unsigned int ramdiskIV[16];
	unsigned int* pRamdiskKey = NULL;
	unsigned int* pRamdiskIV = NULL;
	io_func* ramdiskFS;
	Volume* ramdiskVolume;
	
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

	unsigned int key[16];
	unsigned int iv[16];

	unsigned int* pKey = NULL;
	unsigned int* pIV = NULL;

	if(argc < 3) {
		printf("usage %s <input.ipsw> <target.ipsw> [-b <bootimage.png>] [-r <recoveryimage.png>] [-nowipe] [-e \"<action to exclude>\"] [[-unlock] [-use39] [-use46] [-cleanup] -3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] <package1.tar> <package2.tar>...\n", argv[0]);
		return 0;
	}

	outputIPSW = argv[2];

	info = parseIPSW(argv[1], bundleRoot, &bundlePath, &outputState);
	if(info == NULL) {
		printf("error: Could not load IPSW\n");
		exit(1);
	}

	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");
	for(i = 3; i < argc; i++) {
		if(argv[i][0] != '-') {
			break;
		}

		if(strcmp(argv[i], "-nowipe") == 0) {
			noWipe = TRUE;
		}

		if(strcmp(argv[i], "-e") == 0) {
			removeKey(firmwarePatches, argv[i + 1]);
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
				printf("error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use39 = TRUE;
			continue;
		}
		
		if(strcmp(argv[i], "-use46") == 0) {
			if(use39) {
				printf("error: select only one of -use39 and -use46\n");
				exit(1);
			}
			use46 = TRUE;
			continue;
		}

		if(strcmp(argv[i], "-b") == 0) {
			applelogo = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!applelogo) {
				printf("cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-r") == 0) {
			recoverymode = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!recoverymode) {
				printf("cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-3") == 0) {
			bootloader39 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader39) {
				printf("cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}

		if(strcmp(argv[i], "-4") == 0) {
			bootloader46 = createAbstractFileFromFile(fopen(argv[i + 1], "rb"));
			if(!bootloader46) {
				printf("cannot open %s\n", argv[i + 1]);
				exit(1);
			}
			i++;
			continue;
		}
	}
	
	if(use39 || use46 || unlockBaseband || selfDestruct || bootloader39 || bootloader46) {
		if(!(bootloader39) || !(bootloader46)) {
			printf("error: you must specify both bootloader files.\n");
			exit(1);
		} else {
			doBootNeuter = TRUE;
		}
	}

	mergePaths = i;

	
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

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch2");
		if(patchValue) {
			if(noWipe) {
				printf("%s: ", patchDict->dValue.key); fflush(stdout);
				doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV);
				patchDict = (Dictionary*) patchDict->dValue.next;
				continue; /* skip over the normal Patch */
			}
		}

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
		if(patchValue) {
			printf("%s: ", patchDict->dValue.key); fflush(stdout);
			doPatch(patchValue, fileValue, bundlePath, &outputState, pKey, pIV);
		}
		
		if(strcmp(patchDict->dValue.key, "AppleLogo") == 0 && applelogo) {
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, applelogo, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}

		if(strcmp(patchDict->dValue.key, "RecoveryMode") == 0 && recoverymode) {
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), pKey, pIV, recoverymode, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}
		
		patchDict = (Dictionary*) patchDict->dValue.next;
	}
	
	fileValue = (StringValue*) getValueByKey(info, "RootFilesystem");
	rootFSPathInIPSW = fileValue->value;
		
	rootSize = ((IntegerValue*) getValueByKey(info, "RootFilesystemSize"))->value;
	rootSize *= 1024 * 1024;
	rootSize -= 47438 * 512;
	buffer = malloc(rootSize);

	extractDmg(
		createAbstractFileFromFileVault(getFileFromOutputState(&outputState, rootFSPathInIPSW), ((StringValue*)getValueByKey(info, "RootFilesystemKey"))->value),
		createAbstractFileFromMemoryFile((void**)&buffer, &rootSize), -1);

	
	rootFS = IOFuncFromAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &rootSize));
	rootVolume = openVolume(rootFS);
	printf("Growing root: %ld\n", (long) rootSize); fflush(stdout);
	grow_hfs(rootVolume, rootSize);
	
	firmwarePatches = (Dictionary*)getValueByKey(info, "FilesystemPatches");
	patchArray = (ArrayValue*) firmwarePatches->values;
	while(patchArray != NULL) {
		for(i = 0; i < patchArray->size; i++) {
			patchDict = (Dictionary*) patchArray->values[i];
			fileValue = (StringValue*) getValueByKey(patchDict, "File");
					
			actionValue = (StringValue*) getValueByKey(patchDict, "Action"); 
			if(strcmp(actionValue->value, "ReplaceKernel") == 0) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");
				printf("replacing kernel... %s -> %s\n", fileValue->value, pathValue->value); fflush(stdout);
				add_hfs(rootVolume, getFileFromOutputState(&outputState, fileValue->value), pathValue->value);
			} if(strcmp(actionValue->value, "Patch") == 0) {
				patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
				patchPath = (char*) malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
				strcpy(patchPath, bundlePath);
				strcat(patchPath, "/");
				strcat(patchPath, patchValue->value);
				
				printf("patching %s (%s)... ", fileValue->value, patchPath);
				doPatchInPlace(rootVolume, fileValue->value, patchPath);
				free(patchPath);
			}
		}
		
		patchArray = (ArrayValue*) patchArray->dValue.next;
	}
	
	for(; mergePaths < argc; mergePaths++) {
		printf("merging %s\n", argv[mergePaths]);
		AbstractFile* tarFile = createAbstractFileFromFile(fopen(argv[mergePaths], "rb"));
		hfs_untar(rootVolume, tarFile);
		tarFile->close(tarFile);
	}
	
	if(doBootNeuter) {
		if(pRamdiskKey) {
			printf("%p: %02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx\n",
				pRamdiskKey, pRamdiskKey[0], pRamdiskKey[1], pRamdiskKey[2], pRamdiskKey[3], pRamdiskKey[4], pRamdiskKey[5], pRamdiskKey[6], pRamdiskKey[7],
				pRamdiskKey[8], pRamdiskKey[9], pRamdiskKey[10], pRamdiskKey[11], pRamdiskKey[12], pRamdiskKey[13], pRamdiskKey[14], pRamdiskKey[15]);

			ramdiskFS = IOFuncFromAbstractFile(openAbstractFile2(getFileFromOutputState(&outputState, ramdiskFSPathInIPSW), pRamdiskKey, pRamdiskIV));
		} else {
			printf("unencrypted ramdisk\n");
			ramdiskFS = IOFuncFromAbstractFile(openAbstractFile(getFileFromOutputState(&outputState, ramdiskFSPathInIPSW)));
		}
		ramdiskVolume = openVolume(ramdiskFS);
		firmwarePatches = (Dictionary*)getValueByKey(info, "BasebandPatches");
		if(firmwarePatches != NULL) {
			patchDict = (Dictionary*) firmwarePatches->values;
			while(patchDict != NULL) {
				pathValue = (StringValue*) getValueByKey(patchDict, "Path");

				fileValue = (StringValue*) getValueByKey(patchDict, "File");		
				if(fileValue) {
					printf("copying %s -> %s... ", fileValue->value, pathValue->value); fflush(stdout);
					if(copyAcrossVolumes(ramdiskVolume, rootVolume, fileValue->value, pathValue->value)) {
						patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
						if(patchValue) {
							patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
							strcpy(patchPath, bundlePath);
							strcat(patchPath, "/");
							strcat(patchPath, patchValue->value);
							printf("patching %s (%s)... ", pathValue->value, patchPath); fflush(stdout);
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
		closeVolume(ramdiskVolume);
		CLOSE(ramdiskFS);
	
		fixupBootNeuterArgs(rootVolume, unlockBaseband, selfDestruct, use39, use46);
	}

	closeVolume(rootVolume);
	CLOSE(rootFS);

	buildDmg(createAbstractFileFromMemoryFile((void**)&buffer, &rootSize), getFileFromOutputStateForOverwrite(&outputState, rootFSPathInIPSW));
	free(buffer);


	writeOutput(&outputState, outputIPSW);
	
	releaseDictionary(info);

	free(bundlePath);
	
	return 0;
}
