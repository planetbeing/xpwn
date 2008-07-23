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
	Dictionary* info;
	char* infoPath;

	AbstractFile* plistFile;
	char* plist;
	FILE* inputIPSWFile;

	SHA_CTX sha1_ctx;
	char buffer[BUFFERSIZE];
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

	SHA1_Init(&sha1_ctx);
	while(!feof(inputIPSWFile)) {
		read = fread(buffer, 1, BUFFERSIZE, inputIPSWFile);
		SHA1_Update(&sha1_ctx, buffer, read);
	}
	SHA1_Final(hash, &sha1_ctx);

	fclose(inputIPSWFile);

	XLOG(0, "Matching IPSW... (%02x%02x%02x%02x...)\n", (int) hash[0], (int) hash[1], (int) hash[2], (int) hash[3]);

	dir = opendir(bundleRoot);
	if(dir == NULL) {
		return NULL;
	}

	while((ent = readdir(dir)) != NULL) {
		if(ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
			continue;
		}

		infoPath = (char*) malloc(sizeof(char) * (strlen(bundleRoot) + strlen(ent->d_name) + sizeof("/Info.plist")));
		strcpy(infoPath, bundleRoot);
		strcat(infoPath, ent->d_name);
		strcat(infoPath, "/Info.plist");
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
					*bundlePath = (char*) malloc(sizeof(char) * (strlen(bundleRoot) + strlen(ent->d_name) + 1));
					strcpy(*bundlePath, bundleRoot);
					strcat(*bundlePath, ent->d_name);

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

	*state = loadZip(inputIPSW);

	return info;
}

int doPatch(StringValue* patchValue, StringValue* fileValue, const char* bundlePath, OutputState** state, unsigned int* key, unsigned int* iv) {
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
	
	XLOG(0, "%s (%s)... ", fileValue->value, patchPath); fflush(stdout);
	
	patchFile = createAbstractFileFromFile(fopen(patchPath, "rb"));
	
	bufferSize = 0;

	if(key != NULL) {
		XLOG(0, "encrypted input... ");
		out = duplicateAbstractFile2(getFileFromOutputState(state, fileValue->value), createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize), key, iv, NULL);
	} else {
		out = duplicateAbstractFile(getFileFromOutputState(state, fileValue->value), createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
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
		AbstractFile* exploited = createAbstractFileFrom8900(createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize));
		exploit8900(exploited);
		exploited->close(exploited);
	}
	
	XLOG(0, "writing... "); fflush(stdout);
	
	addToOutput(state, fileValue->value, buffer, bufferSize);

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
