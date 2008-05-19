#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <openssl/sha.h>
#include "plist.h"
#include "outputstate.h"
#include "pwnutil.h"

#define BUFFERSIZE (1024*1024)

Dictionary* parseIPSW(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state) {
	Dictionary* info;
	char* infoPath;

	char* ipswName;
	AbstractFile* plistFile;
	char* plist;
	FILE* inputIPSWFile;

	SHA_CTX sha1_ctx;
	char buffer[BUFFERSIZE];
	int read;
	char hash[20];

	DIR* dir;
	struct dirent* ent;
	StringValue* plistSHA1String;
	char plistHash[20];
	int i;

	*bundlePath = NULL;

	inputIPSWFile = fopen(inputIPSW, "rb");
	if(!inputIPSWFile) {
		return NULL;
	}

	printf("Hashing IPSW...\n");

	SHA1_Init(&sha1_ctx);
	while(!feof(inputIPSWFile)) {
		read = fread(buffer, 1, BUFFERSIZE, inputIPSWFile);
		SHA1_Update(&sha1_ctx, buffer, read);
	}
	SHA1_Final(hash, &sha1_ctx);

	fclose(inputIPSWFile);

	printf("Matching IPSW...\n");

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
					if(plistHash[0] != hash[0]) {
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
