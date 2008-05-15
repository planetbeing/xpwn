#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include "../hfs/common.h"
#include "../hfs/hfsplus.h"
#include "../dmg/dmg.h"
#include "dmghfs.h"

int extractDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, int partNum) {
	off_t fileLength;
	UDIFResourceFile resourceFile;
	ResourceKey* resources;
	ResourceData* blkxData;
		
	fileLength = abstractIn->getLength(abstractIn);
	abstractIn->seek(abstractIn, fileLength - sizeof(UDIFResourceFile));
	readUDIFResourceFile(abstractIn, &resourceFile);
	resources = readResources(abstractIn, &resourceFile);
	
	printf("Writing out data..\n"); fflush(stdout);
	
	/* reasonable assumption that 2 is the main partition, given that that's usually the case in SPUD layouts */
	if(partNum < 0) {
		blkxData = getResourceByKey(resources, "blkx")->data;
		while(blkxData != NULL) {
			if(strstr(blkxData->name, "Apple_HFS") != NULL) {
				break;
			}
			blkxData = blkxData->next;
		}
	} else {
		blkxData = getDataByID(getResourceByKey(resources, "blkx"), partNum);
	}
	
	if(blkxData) {
		extractBLKX(abstractIn, abstractOut, (BLKXTable*)(blkxData->data));
	} else {
		printf("BLKX not found!\n"); fflush(stdout);
	}
	abstractOut->close(abstractOut);
	
	releaseResources(resources);
	abstractIn->close(abstractIn);
	
	return TRUE;
}

uint32_t calculateMasterChecksum(ResourceKey* resources) {
	ResourceKey* blkxKeys;
	ResourceData* data;
	BLKXTable* blkx;
	unsigned char* buffer;
	int blkxNum;
	uint32_t result;
	
	blkxKeys = getResourceByKey(resources, "blkx");
	
	data = blkxKeys->data;
	blkxNum = 0;
	while(data != NULL) {
		blkx = (BLKXTable*) data->data;
		if(blkx->checksum.type == CHECKSUM_CRC32) {
			blkxNum++;
		}
		data = data->next;
	}
	
	buffer = (unsigned char*) malloc(4 * blkxNum) ;
	data = blkxKeys->data;
	blkxNum = 0;
	while(data != NULL) {
		blkx = (BLKXTable*) data->data;
		if(blkx->checksum.type == CHECKSUM_CRC32) {
			buffer[(blkxNum * 4) + 0] = (blkx->checksum.data[0] >> 24) & 0xff;
			buffer[(blkxNum * 4) + 1] = (blkx->checksum.data[0] >> 16) & 0xff;
			buffer[(blkxNum * 4) + 2] = (blkx->checksum.data[0] >> 8) & 0xff;
			buffer[(blkxNum * 4) + 3] = (blkx->checksum.data[0] >> 0) & 0xff;
			blkxNum++;
		}
		data = data->next;
	}
	
	result = 0;
	CRC32Checksum(&result, (const unsigned char*) buffer, 4 * blkxNum);
	free(buffer);
	return result;  
}

int buildDmg(AbstractFile* abstractIn, AbstractFile* abstractOut) {	
	io_func* io;
	Volume* volume;  
	
	HFSPlusVolumeHeader* volumeHeader;
	DriverDescriptorRecord* DDM;
	Partition* partitions;
	
	ResourceKey* resources;
	ResourceKey* curResource;
	
	NSizResource* nsiz;
	NSizResource* myNSiz;
	CSumResource csum;
	
	BLKXTable* blkx;
	ChecksumToken uncompressedToken;
	
	ChecksumToken dataForkToken;
	
	UDIFResourceFile koly;
	
	off_t plistOffset;
	uint32_t plistSize;
	uint32_t dataForkChecksum;
	
	io = IOFuncFromAbstractFile(abstractIn);
	volume = openVolume(io); 
	volumeHeader = volume->volumeHeader;
	

	if(volumeHeader->signature != HFSX_SIGNATURE) {
		printf("Warning: ASR data only reverse engineered for case-sensitive HFS+ volumes\n");fflush(stdout);
	}
    
	resources = NULL;
	nsiz = NULL;
    
	memset(&dataForkToken, 0, sizeof(ChecksumToken));
	
	printf("Creating and writing DDM and partition map...\n"); fflush(stdout);
	
	DDM = createDriverDescriptorMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE);
	
	partitions = createApplePartitionMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, HFSX_VOLUME_TYPE);
	
	writeDriverDescriptorMap(abstractOut, DDM, &CRCProxy, (void*) (&dataForkToken), &resources);
	free(DDM);
	writeApplePartitionMap(abstractOut, partitions, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz);
	free(partitions);
	writeATAPI(abstractOut, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz);
	
	memset(&uncompressedToken, 0, sizeof(uncompressedToken));
	SHA1Init(&(uncompressedToken.sha1));
	
	printf("Writing main data blkx...\n"); fflush(stdout);
	
	abstractIn->seek(abstractIn, 0);
	blkx = insertBLKX(abstractOut, abstractIn, USER_OFFSET, (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE,
				2, CHECKSUM_CRC32, &BlockSHA1CRC, &uncompressedToken, &CRCProxy, &dataForkToken, volume);
	
	blkx->checksum.data[0] = uncompressedToken.crc;
	printf("Inserting main blkx...\n"); fflush(stdout);
	
	resources = insertData(resources, "blkx", 2, "Mac_OS_X (Apple_HFSX : 3)", (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
	free(blkx);
	
	
	printf("Inserting cSum data...\n"); fflush(stdout);
	
	csum.version = 1;
	csum.type = CHECKSUM_MKBLOCK;
	csum.checksum = uncompressedToken.block;
	
	resources = insertData(resources, "cSum", 2, "", (const char*) (&csum), sizeof(csum), 0);
	
	printf("Inserting nsiz data\n"); fflush(stdout);
	
	myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
	memset(myNSiz, 0, sizeof(NSizResource));
	myNSiz->isVolume = TRUE;
	myNSiz->blockChecksum2 = uncompressedToken.block;
	myNSiz->partitionNumber = 2;
	myNSiz->version = 6;
	myNSiz->bytes = (volumeHeader->totalBlocks - volumeHeader->freeBlocks) * volumeHeader->blockSize;
	myNSiz->modifyDate = volumeHeader->modifyDate;
	myNSiz->volumeSignature = volumeHeader->signature;
	myNSiz->sha1Digest = (unsigned char *)malloc(20);
	SHA1Final(myNSiz->sha1Digest, &(uncompressedToken.sha1));
	myNSiz->next = NULL;
	if(nsiz == NULL) {
		nsiz = myNSiz;
	} else {
		myNSiz->next = nsiz->next;
		nsiz->next = myNSiz;
	}
	
	printf("Writing free partition...\n"); fflush(stdout);
	
	writeFreePartition(abstractOut, (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, &resources);
	
	dataForkChecksum = dataForkToken.crc;
	
	printf("Writing XML data...\n"); fflush(stdout);
	curResource = resources;
	while(curResource->next != NULL)
		curResource = curResource->next;
    
	curResource->next = writeNSiz(nsiz);
	curResource = curResource->next;
	releaseNSiz(nsiz);
	
	curResource->next = makePlst();
	curResource = curResource->next;
	
	curResource->next = makeSize(volumeHeader);
	curResource = curResource->next;
	
	plistOffset = abstractOut->tell(abstractOut);
	writeResources(abstractOut, resources);
	plistSize = abstractOut->tell(abstractOut) - plistOffset;
	
	printf("Generating UDIF metadata...\n"); fflush(stdout);
	
	koly.fUDIFSignature = KOLY_SIGNATURE;
	koly.fUDIFVersion = 4;
	koly.fUDIFHeaderSize = sizeof(koly);
	koly.fUDIFFlags = kUDIFFlagsFlattened;
	koly.fUDIFRunningDataForkOffset = 0;
	koly.fUDIFDataForkOffset = 0;
	koly.fUDIFDataForkLength = plistOffset;
	koly.fUDIFRsrcForkOffset = 0;
	koly.fUDIFRsrcForkLength = 0;
	
	koly.fUDIFSegmentNumber = 1;
	koly.fUDIFSegmentCount = 1;
	koly.fUDIFSegmentID.data1 = rand();
	koly.fUDIFSegmentID.data2 = rand();
	koly.fUDIFSegmentID.data3 = rand();
	koly.fUDIFSegmentID.data4 = rand();
	koly.fUDIFDataForkChecksum.type = CHECKSUM_CRC32;
	koly.fUDIFDataForkChecksum.size = 0x20;
	koly.fUDIFDataForkChecksum.data[0] = dataForkChecksum;
	koly.fUDIFXMLOffset = plistOffset;
	koly.fUDIFXMLLength = plistSize;
	memset(&(koly.reserved1), 0, 0x78);
	
	koly.fUDIFMasterChecksum.type = CHECKSUM_CRC32;
	koly.fUDIFMasterChecksum.size = 0x20;
	koly.fUDIFMasterChecksum.data[0] = calculateMasterChecksum(resources);
	printf("Master checksum: %x\n", koly.fUDIFMasterChecksum.data[0]); fflush(stdout); 
	
	koly.fUDIFImageVariant = kUDIFDeviceImageType;
	koly.fUDIFSectorCount = EXTRA_SIZE + (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE;
	koly.reserved2 = 0;
	koly.reserved3 = 0;
	koly.reserved4 = 0;
	
	printf("Writing out UDIF resource file...\n"); fflush(stdout); 
	
	writeUDIFResourceFile(abstractOut, &koly);
	
	printf("Cleaning up...\n"); fflush(stdout);
	
	releaseResources(resources);
	
	abstractOut->close(abstractOut);
	closeVolume(volume);
	CLOSE(io);
	
	printf("Done.\n"); fflush(stdout);
	
	return TRUE;
}

#define BUFSIZE 1024*1024

void writeToFile(HFSPlusCatalogFile* file, AbstractFile* output, Volume* volume) {
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
			if(output->write(output, buffer, BUFSIZE) != BUFSIZE) {
				panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(!READ(io, curPosition, bytesLeft, buffer)) {
				panic("error reading");
			}
			if(output->write(output, buffer, bytesLeft) != bytesLeft) {
				panic("error writing");
			}
			curPosition += bytesLeft;
			bytesLeft -= bytesLeft;
		}
	}
	CLOSE(io);
}

void writeToHFSFile(HFSPlusCatalogFile* file, AbstractFile* input, Volume* volume) {
	unsigned char buffer[BUFSIZE];
	io_func* io;
	off_t curPosition;
	off_t bytesLeft;
	
	bytesLeft = input->getLength(input);

	io = openRawFile(file->fileID, &file->dataFork, (HFSPlusCatalogRecord*)file, volume);
	if(io == NULL) {
		panic("error opening file");
		return;
	}
	
	curPosition = 0;
	
	allocate((RawFile*)io->data, bytesLeft);
	
	while(bytesLeft > 0) {
		if(bytesLeft > BUFSIZE) {
			if(input->read(input, buffer, BUFSIZE) != BUFSIZE) {
				panic("error reading");
			}
			if(!WRITE(io, curPosition, BUFSIZE, buffer)) {
				panic("error writing");
			}
			curPosition += BUFSIZE;
			bytesLeft -= BUFSIZE;
		} else {
			if(input->read(input, buffer, (size_t)bytesLeft) != (size_t)bytesLeft) {
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

void get_hfs(Volume* volume, const char* inFileName, AbstractFile* output) {
	HFSPlusCatalogRecord* record;
	
	record = getRecordFromPath(inFileName, volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord)
			writeToFile((HFSPlusCatalogFile*)record,  output, volume);
		else {
			printf("Not a file\n");
			exit(0);
		}
	} else {
		printf("No such file or directory\n");
		exit(0);
	}
	
	free(record);
}

int add_hfs(Volume* volume, AbstractFile* inFile, const char* outFileName) {
	HFSPlusCatalogRecord* record;
	int ret;
	
	record = getRecordFromPath(outFileName, volume, NULL, NULL);
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFileRecord) {
			writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
			ret = TRUE;
		} else {
			printf("Not a file\n");
			exit(0);
		}
	} else {
		if(newFile(outFileName, volume)) {
			record = getRecordFromPath(outFileName, volume, NULL, NULL);
			writeToHFSFile((HFSPlusCatalogFile*)record, inFile, volume);
			ret = TRUE;
		} else {
			inFile->close(inFile);
			ret = FALSE;
		}
	}
	
	inFile->close(inFile);
	if(record != NULL) {
		free(record);
	}
	
	return ret;
}

void grow_hfs(Volume* volume, uint64_t newSize) {
	uint32_t newBlocks;
	uint32_t blocksToGrow;
	uint64_t newMapSize;
	uint64_t i;
	unsigned char zero;
	
	zero = 0;	
	
	newBlocks = newSize / volume->volumeHeader->blockSize;
	

	blocksToGrow = newBlocks - volume->volumeHeader->totalBlocks;
	newMapSize = newBlocks / 8;
	
	if(volume->volumeHeader->allocationFile.logicalSize < newMapSize) {
		if(volume->volumeHeader->freeBlocks
		   < ((newMapSize - volume->volumeHeader->allocationFile.logicalSize) / volume->volumeHeader->blockSize)) {
			printf("Not enough room to allocate new allocation map blocks\n");
			exit(0);
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
}

void addAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName) {
	CatalogRecordList* list;
	CatalogRecordList* theList;
	char cwd[1024];
	char fullName[1024];
	char testBuffer[1024];
	char* pathComponent;
	int pathLen;
	
	char* name;
	
	DIR* dir;
	DIR* tmp;
	
	HFSCatalogNodeID cnid;
	
	struct dirent* ent;
	
	AbstractFile* file;
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
			printf("folder: %s\n", fullName); fflush(stdout);
			
			if(cnid == 0) {
				cnid = newFolder(fullName, volume);
			}
			
			fullName[pathLen] = '/';
			fullName[pathLen + 1] = '\0';
			ASSERT(chdir(ent->d_name) == 0, "chdir");
			addAllInFolder(cnid, volume, fullName);
			ASSERT(chdir(cwd) == 0, "chdir");
		} else {
			printf("file: %s\n", fullName);	fflush(stdout);
			if(cnid == 0) {
				cnid = newFile(fullName, volume);
			}
			file = createAbstractFileFromFile(fopen(ent->d_name, "rb"));
			ASSERT(file != NULL, "fopen");
			outFile = (HFSPlusCatalogFile*)getRecordByCNID(cnid, volume);
			writeToHFSFile(outFile, file, volume);
			file->close(file);
			free(outFile);
			
			if(strncmp(fullName, "/Applications/", sizeof("/Applications/") - 1) == 0) {
				testBuffer[0] = '\0';
				strcpy(testBuffer, "/Applications/");
				strcat(testBuffer, ent->d_name);
				strcat(testBuffer, ".app/");
				strcat(testBuffer, ent->d_name);
				if(strcmp(testBuffer, fullName) == 0) {
					if(strcmp(ent->d_name, "Installer") == 0
					|| strcmp(ent->d_name, "BootNeuter") == 0
					) {
						printf("Giving setuid permissions to %s...\n", fullName); fflush(stdout);
						chmodFile(fullName, 04755, volume);
					} else {
						printf("Giving permissions to %s\n", fullName); fflush(stdout);
						chmodFile(fullName, 0755, volume);
					}
				}
			} else if(strncmp(fullName, "/bin/", sizeof("/bin/") - 1) == 0
				|| strncmp(fullName, "/Applications/BootNeuter.app/bin/", sizeof("/Applications/BootNeuter.app/bin/") - 1) == 0
				|| strncmp(fullName, "/sbin/", sizeof("/sbin/") - 1) == 0
				|| strncmp(fullName, "/usr/sbin/", sizeof("/usr/sbin/") - 1) == 0
				|| strncmp(fullName, "/usr/bin/", sizeof("/usr/bin/") - 1) == 0
				|| strncmp(fullName, "/usr/libexec/", sizeof("/usr/libexec/") - 1) == 0
				|| strncmp(fullName, "/usr/local/bin/", sizeof("/usr/local/bin/") - 1) == 0
				|| strncmp(fullName, "/usr/local/sbin/", sizeof("/usr/local/sbin/") - 1) == 0
				|| strncmp(fullName, "/usr/local/libexec/", sizeof("/usr/local/libexec/") - 1) == 0
				) {
				chmodFile(fullName, 0755, volume);
				printf("Giving permissions to %s\n", fullName); fflush(stdout);
			}
		}
	}
	
	closedir(dir);
	
	releaseCatalogRecordList(theList);
}

void addall_hfs(Volume* volume, char* dirToMerge, char* dest) {
	HFSPlusCatalogRecord* record;
	char* name;
	char cwd[1024];
	char initPath[1024];
	int lastCharOfPath;
	
	ASSERT(getcwd(cwd, 1024) != NULL, "cannot get current working directory");
	
	if(chdir(dirToMerge) != 0) {
		printf("Cannot open that directory: %s\n", dirToMerge);
		exit(0);
	}
	
	record = getRecordFromPath(dest, volume, &name, NULL);
	strcpy(initPath, dest);
	lastCharOfPath = strlen(dest) - 1;
	if(dest[lastCharOfPath] != '/') {
		initPath[lastCharOfPath + 1] = '/';
		initPath[lastCharOfPath + 2] = '\0';
	}
	
	if(record != NULL) {
		if(record->recordType == kHFSPlusFolderRecord)
			addAllInFolder(((HFSPlusCatalogFolder*)record)->folderID, volume, initPath);  
		else {
			printf("Not a folder\n");
			exit(0);
		}
	} else {
		printf("No such file or directory\n");
		exit(0);
	}
	
	ASSERT(chdir(cwd) == 0, "chdir");
	free(record);
	
}
int copyAcrossVolumes(Volume* volume1, Volume* volume2, char* path1, char* path2) {
	void* buffer;
	size_t bufferSize;
	AbstractFile* tmpFile;
	int ret;
	
	buffer = malloc(1);
	bufferSize = 0;
	tmpFile = createAbstractFileFromMemoryFile((void**)&buffer, &bufferSize);
	
	printf("retrieving... "); fflush(stdout);
	get_hfs(volume1, path1, tmpFile);
	tmpFile->seek(tmpFile, 0);
	printf("writing (%lld)... ", tmpFile->getLength(tmpFile)); fflush(stdout);
	ret = add_hfs(volume2, tmpFile, path2);
	printf("done\n");
	
	free(buffer);
	
	return ret;
}

