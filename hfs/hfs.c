#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <time.h>
#include <string.h>
#include <unistd.h>
#include "hfsplus.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

char endianness;

void displayFolder(HFSCatalogNodeID folderID, Volume* volume) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	HFSPlusCatalogFolder* folder;
	HFSPlusCatalogFile* file;
	time_t fileTime;
	struct tm *date;
	
	theList = list = getFolderContents(folderID, volume);
	
	while(list != NULL) {
		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			printf("%06o ", folder->permissions.fileMode);
			printf("%3d ", folder->permissions.ownerID);
			printf("%3d ", folder->permissions.groupID);
			printf("%12d ", folder->valence);
			fileTime = APPLE_TO_UNIX_TIME(folder->contentModDate);
		} else if(list->record->recordType == kHFSPlusFileRecord) {
			file = (HFSPlusCatalogFile*)list->record;
			printf("%06o ", file->permissions.fileMode);
			printf("%3d ", file->permissions.ownerID);
			printf("%3d ", file->permissions.groupID);
			printf("%12lld ", file->dataFork.logicalSize);
			fileTime = APPLE_TO_UNIX_TIME(file->contentModDate);
		}
			
		date = localtime(&fileTime);
		if(date != NULL) {
      printf("%2d/%2d/%4d %02d:%02d ", date->tm_mon, date->tm_mday, date->tm_year + 1900, date->tm_hour, date->tm_min);
		} else {
      printf("                 ");
		}

		printUnicode(&list->name);
		printf("\n");
		
		list = list->next;
	}
	
	releaseCatalogRecordList(theList);
}

void displayFileLSLine(HFSPlusCatalogFile* file, const char* name) {
	time_t fileTime;
	struct tm *date;
	
	printf("%06o ", file->permissions.fileMode);
	printf("%3d ", file->permissions.ownerID);
	printf("%3d ", file->permissions.groupID);
	printf("%12d ", file->dataFork.logicalSize);
	fileTime = APPLE_TO_UNIX_TIME(file->contentModDate);
	date = localtime(&fileTime);
  if(date != NULL) {
    printf("%2d/%2d/%4d %2d:%02d ", date->tm_mon, date->tm_mday, date->tm_year + 1900, date->tm_hour, date->tm_min);
  } else {
    printf("                 ");
  }
	printf("%s\n", name);
}

void cmd_ls(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char* name;
	
	if(argc > 1)
		record = getRecordFromPath(argv[1], volume, &name, NULL);
	else
		record = getRecordFromPath("/", volume, &name, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			displayFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);  
		else
			displayFileLSLine((HFSPlusCatalogFile*)record, name);
	} else {
		printf("No such file or directory\n");
	}
	
	free(record);
}


#define BUFSIZE 1024*1024

void writeToFile(HFSPlusCatalogFile* file, FILE* output, Volume* volume) {
	unsigned char buffer[BUFSIZE];
	io_func* io;
	off_t curPosition;
	size_t bytesLeft;
	
	io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
	if(io == NULL) {
		panic("error opening file");
		return;
	}
	
	curPosition = 0;
	bytesLeft = file->dataFork.logicalSize;
	
	while(bytesLeft > 0) {
		if(bytesLeft > BUFSIZE) {
			if(!READ(io, curPosition, BUFSIZE, buffer)) {
				panic("error reading");
			}
			if(fwrite(buffer, BUFSIZE, 1, output) != 1) {
				panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(!READ(io, curPosition, bytesLeft, buffer)) {
				panic("error reading");
			}
			if(fwrite(buffer, bytesLeft, 1, output) != 1) {
				panic("error writing");
			}
			curPosition += bytesLeft;
			bytesLeft -= bytesLeft;
		}
	}
	CLOSE(io);
}

void writeToHFSFile(HFSPlusCatalogFile* file, FILE* input, Volume* volume) {
	unsigned char buffer[BUFSIZE];
	io_func* io;
	off_t curPosition;
	off_t bytesLeft;
	
	fseeko(input, 0, SEEK_END);
	bytesLeft = ftello(input);
	fseeko(input, 0, SEEK_SET);  
	
	io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
	if(io == NULL) {
		panic("error opening file");
		return;
	}
	
	curPosition = 0;
	
	allocate((RawFile*)io->data, bytesLeft);
	
	while(bytesLeft > 0) {
		if(bytesLeft > BUFSIZE) {
			if(fread(buffer, BUFSIZE, 1, input) != 1) {
				panic("error reading");
			}
			if(!WRITE(io, curPosition, BUFSIZE, buffer)) {
				panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(fread(buffer, (size_t)bytesLeft, 1, input) != 1) {
				panic("error reading");
			}
			if(!WRITE(io, curPosition, (size_t)bytesLeft, buffer)) {
				panic("error reading");
			}
			curPosition += bytesLeft;
			bytesLeft -= bytesLeft;
		}
	}

	CLOSE(io);
}

void cmd_cat(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	
	record = getRecordFromPath(argv[1], volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, stdout, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	free(record);
}

void cmd_extract(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	FILE *outFile;
	
	if(argc < 3) {
		printf("Not enough arguments");
		return;
	}
	
	outFile = fopen(argv[2], "wb");
	
	if(outFile == NULL) {
		printf("cannot create file");
	}
	
	record = getRecordFromPath(argv[1], volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record, outFile, volume);
		else
			printf("Not a file\n");
	} else {
		printf("No such file or directory\n");
	}
	
	fclose(outFile);
	free(record);
}

void cmd_mv(Volume* volume, int argc, const char *argv[]) {
	if(argc > 2) {
		move(argv[1], argv[2], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_mkdir(Volume* volume, int argc, const char *argv[]) {
	if(argc > 1) {
		newFolder(argv[1], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_add(Volume* volume, int argc, const char *argv[]) {
	FILE *inFile;
	HFSPlusCatalogRecord* record;
	
	if(argc < 3) {
		printf("Not enough arguments");
		return;
	}
	
	inFile = fopen(argv[1], "rb");
	
	if(inFile == NULL) {
		printf("file to add not found");
	}
	
	record = getRecordFromPath(argv[2], volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
		else
			printf("Not a file\n");
	} else {
		newFile(argv[2], volume);
		record = getRecordFromPath(argv[2], volume, NULL, NULL);
		writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
	}
	
	fclose(inFile);
	free(record);
}

void cmd_rm(Volume* volume, int argc, const char *argv[]) {
	if(argc > 1) {
		removeFile(argv[1], volume);
	} else {
		printf("Not enough arguments");
	}
}

void cmd_chmod(Volume* volume, int argc, const char *argv[]) {
	int mode;
	
	if(argc > 2) {
		sscanf(argv[1], "%o", &mode);
		chmodFile(argv[2], mode, volume);
	} else {
		printf("Not enough arguments");
	}
}

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
}

void extractAllInFolder(HFSCatalogNodeID folderID, Volume* volume) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char cwd[1024];
	char* name;
	HFSPlusCatalogFolder* folder;
	HFSPlusCatalogFile* file;
	FILE* outFile;
	struct stat status;
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
	
	theList = list = getFolderContents(folderID, volume);
	
	while(list != NULL) {
		name = unicodeToAscii(&list->name);
		if(strncmp(name, ".HFS+ Private Directory Data", sizeof(".HFS+ Private Directory Data") - 1) == 0 || name[0] == '\0') {
			free(name);
			list = list->next;
			continue;
		}
		
		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			printf("folder: %s\n", name);
			if(stat(name, &status) != 0) {
				ASSERT(mkdir(name, 0755) == 0, "mkdir");
			}
			ASSERT(chdir(name) == 0, "chdir");
			extractAllInFolder(folder->folderID, volume);
			ASSERT(chdir(cwd) == 0, "chdir");
		} else if(list->record->recordType == kHFSPlusFileRecord) {
      printf("file: %s\n", name);
			file = (HFSPlusCatalogFile*)list->record;
			outFile = fopen(name, "wb");
			if(outFile != NULL) {
        writeToFile(file, outFile, volume);
        fclose(outFile);
      } else {
        printf("WARNING: cannot fopen %s\n", name);
      }
		}
		
		free(name);
		list = list->next;
	}
	releaseCatalogRecordList(theList);
}

void cmd_extractall(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char cwd[1024];
	char* name;
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
	
	if(argc > 1)
		record = getRecordFromPath(argv[1], volume, &name, NULL);
	else
		record = getRecordFromPath("/", volume, &name, NULL);
	
	if(argc > 2) {
		ASSERT(chdir(argv[2]) == 0, "chdir");
  }

	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			extractAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume);  
		else
			printf("Not a folder\n");
	} else {
		printf("No such file or directory\n");
	}
	free(record);
	
	ASSERT(chdir(cwd) == 0, "chdir");
}

void removeAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char fullName[1024];
	char* name;
	char* pathComponent;
	int pathLen;
	char isRoot;
	
	HFSPlusCatalogFolder* folder;
	theList = list = getFolderContents(folderID, volume);
	
	strcpy(fullName, parentName);
	pathComponent = fullName + strlen(fullName);
	
	isRoot = FALSE;
	if(strcmp(fullName, "/") == 0) {
		isRoot = TRUE;
	}
	
	while(list != NULL) {
		name = unicodeToAscii(&list->name);
		if(isRoot && (name[0] == '\0' || strncmp(name, ".HFS+ Private Directory Data", sizeof(".HFS+ Private Directory Data") - 1) == 0)) {
			free(name);
			list = list->next;
			continue;
		}
		
		strcpy(pathComponent, name);
		pathLen = strlen(fullName);
		
		if(list->record->recordType == kHFSPlusFolderRecord) {
			folder = (HFSPlusCatalogFolder*)list->record;
			fullName[pathLen] = '/';
			fullName[pathLen + 1] = '\0';
			removeAllInFolder(folder->folderID, volume, fullName);
		} else {
			printf("%s\n", fullName);
			removeFile(fullName, volume);
		}
		
		free(name);
		list = list->next;
	}
	
	releaseCatalogRecordList(theList);
	
	if(!isRoot) {
		*(pathComponent - 1) = '\0';
		printf("%s\n", fullName);
		removeFile(fullName, volume);
	}
}

void cmd_rmall(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char* name;
	char initPath[1024];
	int lastCharOfPath;
	
	if(argc > 1) {
		record = getRecordFromPath(argv[1], volume, &name, NULL);
		strcpy(initPath, argv[1]);
		lastCharOfPath = strlen(argv[1]) - 1;
		if(argv[1][lastCharOfPath] != '/') {
			initPath[lastCharOfPath + 1] = '/';
			initPath[lastCharOfPath + 2] = '\0';
		}
	} else {
		record = getRecordFromPath("/", volume, &name, NULL);
		initPath[0] = '/';
		initPath[1] = '\0';	
	}
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord) {
			removeAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume, initPath);
		} else {
			printf("Not a folder\n");
		}
	} else {
		printf("No such file or directory\n");
	}
	free(record);
}

void addAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char cwd[1024];
	char fullName[1024];
	char* pathComponent;
	int pathLen;
	
	char* name;
	
	DIR* dir;
	DIR* tmp;
	
	HFSCatalogNodeID cnid;
	
	struct dirent* ent;
	
	FILE* file;
	HFSPlusCatalogFile* outFile;
	
	strcpy(fullName, parentName);
	pathComponent = fullName + strlen(fullName);
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
	
	theList = list = getFolderContents(folderID, volume);
	
	ASSERT((dir = opendir(cwd)) != NULL, "opendir");
	
	while((ent = readdir(dir)) != NULL) {
		if(ent->d_name[0] == '.' && (ent->d_name[1] == '\0' || (ent->d_name[1] == '.' && ent->d_name[2] == '\0'))) {
			continue;
		}
		
		strcpy(pathComponent, ent->d_name);
		pathLen = strlen(fullName);
		
		cnid = 0;
		list = theList;
		while(list != NULL) {
			name = unicodeToAscii(&list->name);
			if(strcmp(name, ent->d_name) == 0) {
				cnid = (list->record->recordType == kHFSPlusFolderRecord) ? (((HFSPlusCatalogFolder*)list->record)->folderID)
				: (((HFSPlusCatalogFile*)list->record)->fileID);
				free(name);
				break;
			}
			free(name);
			list = list->next;
		}
		
		if((tmp = opendir(ent->d_name)) != NULL) {
			closedir(tmp);
			printf("folder: %s\n", fullName);
			
			if(cnid == 0) {
				cnid = newFolder(fullName, volume);
			}
			
			fullName[pathLen] = '/';
			fullName[pathLen + 1] = '\0';
			ASSERT(chdir(ent->d_name) == 0, "chdir");
			addAllInFolder(cnid, volume, fullName);
			ASSERT(chdir(cwd) == 0, "chdir");
		} else {
			printf("file: %s\n", fullName);	
			if(cnid == 0) {
				cnid = newFile(fullName, volume);
			}
			file = fopen(ent->d_name, "rb");
			ASSERT(file != NULL, "fopen");
			outFile = (HFSPlusCatalogFile*)getRecordByCNID(cnid, volume);
			writeToHFSFile(outFile, file, volume);
			fclose(file);
			free(outFile);
		}
	}
	
	closedir(dir);
	
	releaseCatalogRecordList(theList);
}

void cmd_addall(Volume* volume, int argc, const char *argv[]) {
	HFSPlusCatalogRecord* record;
	char* name;
	char cwd[1024];
	char initPath[1024];
	int lastCharOfPath;
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
    
	if(argc < 2) {
		printf("Not enough arguments");
		return;
	}
	
	if(chdir(argv[1]) != 0) {
		printf("Cannot open that directory: %s\n", argv[1]);
		return;
	}
	
	if(argc > 2) {
		record = getRecordFromPath(argv[2], volume, &name, NULL);
		strcpy(initPath, argv[2]);
		lastCharOfPath = strlen(argv[2]) - 1;
		if(argv[2][lastCharOfPath] != '/') {
			initPath[lastCharOfPath + 1] = '/';
			initPath[lastCharOfPath + 2] = '\0';
		}
	} else {
		record = getRecordFromPath("/", volume, &name, NULL);
		record = getRecordFromPath("/", volume, &name, NULL);
		initPath[0] = '/';
		initPath[1] = '\0';	
	}
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			addAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume, initPath);  
		else
			printf("Not a folder\n");
	} else {
		printf("No such file or directory\n");
	}
	
	ASSERT(chdir(cwd) == 0, "chdir");
	free(record);
	
}

void cmd_grow(Volume* volume, int argc, const char *argv[]) {
	uint64_t newSize;
	uint32_t newBlocks;
	uint32_t blocksToGrow;
	uint64_t newMapSize;
	uint64_t i;
	unsigned char zero;
	
	zero = 0;
	
	if(argc < 2) {
		printf("Not enough arguments\n");
		return;
	}
	
	newSize = 0;
	sscanf(argv[1], "%lld", &newSize);
	newBlocks = newSize / volume->volumeHeader->blockSize;
	
	if(newBlocks <= volume->volumeHeader->totalBlocks) {
		printf("Cannot shrink volume\n");
		return;
	}
	
	blocksToGrow = newBlocks - volume->volumeHeader->totalBlocks;
	newMapSize = newBlocks / 8;
	
	if(volume->volumeHeader->allocationFile.logicalSize < newMapSize) {
		if(volume->volumeHeader->freeBlocks
		   < ((newMapSize - volume->volumeHeader->allocationFile.logicalSize) / volume->volumeHeader->blockSize)) {
			printf("Not enough room to allocate new allocation map blocks\n");
		}
		
		allocate((RawFile*) (volume->allocationFile->data), newMapSize);
	}
	
	/* unreserve last block */	
	setBlockUsed(volume, volume->volumeHeader->totalBlocks - 1, 0);
	/* don't need to increment freeBlocks because we will allocate another alternate volume header later on */
	
	/* "unallocate" the new blocks */
	for(i = ((volume->volumeHeader->totalBlocks / 8) + 1); i < newMapSize; i++) {
		ASSERT(WRITE(volume->allocationFile, i, 1, &zero), "WRITE");
	}
	
	/* grow backing store size */
	ASSERT(WRITE(volume->image, newSize - 1, 1, &zero), "WRITE");
	
	/* write new volume information */
	volume->volumeHeader->totalBlocks = newBlocks;
	volume->volumeHeader->freeBlocks += blocksToGrow;
	
	/* reserve last block */	
	setBlockUsed(volume, volume->volumeHeader->totalBlocks - 1, 1);
	
	updateVolume(volume);
	
	printf("grew volume: %lld -> %lld (%d)\n", newSize - (blocksToGrow * volume->volumeHeader->blockSize), newSize, blocksToGrow);
}

int main(int argc, const char *argv[]) {
	io_func* io;
	Volume* volume;
	
	TestByteOrder();
	
	if(argc < 3) {
		printf("usage: %s <image-file> <ls|cat|mv|mkdir|add|rm|chmod|extract|extractall|rmall|addall> <arguments>\n", argv[0]);
		return 0;
	}
	
	io = openFlatFile(argv[1]);
	if(io == NULL) {
		return 1;
	}
	
	volume = openVolume(io); 
	if(volume == NULL) {
		CLOSE(io);
		return 1;
	}
	
	if(argc > 1) {
		if(strcmp(argv[2], "ls") == 0) {
			cmd_ls(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "cat") == 0) {
			cmd_cat(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "mv") == 0) {
			cmd_mv(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "mkdir") == 0) {
			cmd_mkdir(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "add") == 0) {
			cmd_add(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "rm") == 0) {
			cmd_rm(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "chmod") == 0) {
			cmd_chmod(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "extract") == 0) {
			cmd_extract(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "extractall") == 0) {
			cmd_extractall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "rmall") == 0) {
			cmd_rmall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "addall") == 0) {
			cmd_addall(volume, argc - 2, argv + 2);
		} else if(strcmp(argv[2], "grow") == 0) {
			cmd_grow(volume, argc - 2, argv + 2);
		}
	}
	
	closeVolume(volume);
	CLOSE(io);
	
	return 0;
}
