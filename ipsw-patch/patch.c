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

int doPatch(StringValue* patchValue, StringValue* fileValue, const char* bundlePath, OutputState** state) {
	char* patchPath;
	size_t bufferSize;
	void* buffer;
	
	AbstractFile* patchFile;
	AbstractFile* file;
	AbstractFile* out;

	buffer = malloc(1);
			
	patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
	strcpy(patchPath, bundlePath);
	strcat(patchPath, "/");
	strcat(patchPath, patchValue->value);
	
	printf("%s (%s)... ", fileValue->value, patchPath); fflush(stdout);
	
	patchFile = createAbstractFileFromFile(fopen(patchPath, "rb"));
	
	bufferSize = 0;

	out = duplicateAbstractFile(getFileFromOutputState(state, fileValue->value), createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
	
	file = openAbstractFile(getFileFromOutputState(state, fileValue->value));
	
	if(!patchFile || !file || !out) {
		printf("file error\n");
		exit(0);
	}

	if(patch(file, out, patchFile) != 0) {
		printf("patch failed\n");
		exit(0);
	}
	
	printf("writing... "); fflush(stdout);
	
/*	out = createAbstractFileFromFile(fopen(filePath, "wb"));
	out->write(out, buffer, bufferSize);
	out->close(out);*/

	addToOutput(state, fileValue->value, buffer, bufferSize);

	printf("success\n"); fflush(stdout);

	free(patchPath);
}

void doPatchInPlace(Volume* volume, char* filePath, char* patchPath) {
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

	printf("retrieving..."); fflush(stdout);
	get_hfs(volume, filePath, bufferFile);
	
	printf("patching..."); fflush(stdout);
				
	patchFile = createAbstractFileFromFile(fopen(patchPath, "rb"));

	buffer2 = malloc(1);
	bufferSize2 = 0;
	out = duplicateAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize), createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2));
	
	if(!patchFile || !bufferFile || !out) {
		printf("file error\n");
		exit(0);
	}

	if(patch(bufferFile, out, patchFile) != 0) {
		printf("patch failed\n");
		exit(0);
	}
	
	printf("writing... "); fflush(stdout);
	add_hfs(volume, createAbstractFileFromMemoryFile((void**)&buffer2, &bufferSize2), filePath);
	free(buffer2);
	free(buffer);

	printf("success\n"); fflush(stdout);
}



void fixupBootNeuterArgs(Volume* volume, char unlockBaseband, char selfDestruct, char use39, char use46) {
	char bootNeuterPlist[] = "/System/Library/LaunchDaemons/com.devteam.bootneuter.auto.plist";
	AbstractFile* plistFile;
	char* plist;
	Dictionary* info;
	size_t bufferSize;
	ArrayValue* arguments;
	
	printf("fixing up BootNeuter arguments...\n");
	
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

	FILE* temp;

	AbstractFile* bootloader39;
	AbstractFile* bootloader46;
	AbstractFile* applelogo;
	AbstractFile* recoverymode;
	
	char unlockBaseband;
	char selfDestruct;
	char use39;
	char use46;
	char doBootNeuter;
	char noBB;

	applelogo = NULL;
	recoverymode = NULL;
	bootloader39 = NULL;
	bootloader46 = NULL;

	unlockBaseband = FALSE;
	selfDestruct = FALSE;
	use39 = FALSE;
	use46 = FALSE;
	doBootNeuter = FALSE;
	noBB = FALSE;

	if(argc < 3) {
		printf("usage %s <input.ipsw> <target.ipsw> [-b <bootimage.png>] [-r <recoveryimage.png>] [-e \"<action to exclude>\"] [-nobbupdate] [[-unlock] [-use39] [-use46] [-cleanup] -3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] <path/to/merge1> <path/to/merge2>...\n", argv[0]);
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

		if(strcmp(argv[i], "-nobbupdate") == 0) {
			noBB = TRUE;
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
		if(noBB) {
			printf("error: bbupdate must be enabled for bootneuter\n");
			exit(1);
		} else {
			if(!(bootloader39) || !(bootloader46)) {
				printf("error: you must specify both bootloader files.\n");
				exit(1);
			} else {
				doBootNeuter = TRUE;
			}
		}
	}

	mergePaths = i;

	
	firmwarePatches = (Dictionary*)getValueByKey(info, "FirmwarePatches");
	patchDict = (Dictionary*) firmwarePatches->values;
	while(patchDict != NULL) {
		fileValue = (StringValue*) getValueByKey(patchDict, "File");

		if(strcmp(patchDict->dValue.key, "Restore Ramdisk") == 0) {
			ramdiskFSPathInIPSW = fileValue->value;
		}

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch2");
		if(patchValue) {
			if(!noBB) {
				printf("%s: ", patchDict->dValue.key); fflush(stdout);
				doPatch(patchValue, fileValue, bundlePath, &outputState);
				patchDict = (Dictionary*) patchDict->dValue.next;
				continue; /* skip over the normal Patch */
			}
		}

		patchValue = (StringValue*) getValueByKey(patchDict, "Patch");
		if(patchValue) {
			printf("%s: ", patchDict->dValue.key); fflush(stdout);
			doPatch(patchValue, fileValue, bundlePath, &outputState);
		}
		
		if(strcmp(patchDict->dValue.key, "AppleLogo") == 0 && applelogo) {
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), applelogo, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}

		if(strcmp(patchDict->dValue.key, "RecoveryMode") == 0 && recoverymode) {
			printf("replacing %s\n", fileValue->value); fflush(stdout);
			ASSERT((imageBuffer = replaceBootImage(getFileFromOutputState(&outputState, fileValue->value), recoverymode, &imageSize)) != NULL, "failed to use new image");
			addToOutput(&outputState, fileValue->value, imageBuffer, imageSize);
		}
		
		patchDict = (Dictionary*) patchDict->dValue.next;
	}
	
	fileValue = (StringValue*) getValueByKey(info, "RootFilesystem");
	rootFSPathInIPSW = fileValue->value;
		
	sscanf(((StringValue*) getValueByKey(info, "RootFilesystemResize"))->value, "%d", &rootSize);
	rootSize *= 1024 * 1024;
	buffer = malloc(rootSize);

	extractDmg(
		createAbstractFileFromFileVault(getFileFromOutputState(&outputState, rootFSPathInIPSW), ((StringValue*)getValueByKey(info, "RootFilesystemKey"))->value),
		createAbstractFileFromMemoryFile((void**)&buffer, &rootSize), -1);

	
	rootFS = IOFuncFromAbstractFile(createAbstractFileFromMemoryFile((void**)&buffer, &rootSize));
	rootVolume = openVolume(rootFS);
	printf("Growing root: %d\n", rootSize); fflush(stdout);
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
				patchPath = malloc(sizeof(char) * (strlen(bundlePath) + strlen(patchValue->value) + 2));
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
	
	for(mergePaths; mergePaths < argc; mergePaths++) {
		addall_hfs(rootVolume, argv[mergePaths], "/");
	}
	
	if(doBootNeuter) {
		ramdiskFS = IOFuncFromAbstractFile(openAbstractFile(getFileFromOutputState(&outputState, ramdiskFSPathInIPSW)));
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
