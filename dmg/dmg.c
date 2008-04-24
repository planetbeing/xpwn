#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "dmg.h"

char endianness;

void TestByteOrder()
{
	short int word = 0x0001;
	char *byte = (char *) &word;
	endianness = byte[0] ? IS_LITTLE_ENDIAN : IS_BIG_ENDIAN;
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

int buildDmg(const char* source, const char* dest) {
	FILE* file;
	FILE* outFile;
	
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
	
	io = openFlatFileRO(source);
	volume = openVolume(io); 
	volumeHeader = volume->volumeHeader;
	
	file = fopen(source, "rb");
	outFile = fopen(dest, "wb");
	
	if(volumeHeader->signature != 0x4858)
		return -1;
    
	resources = NULL;
	nsiz = NULL;
    
	memset(&dataForkToken, 0, sizeof(ChecksumToken));
	
	printf("Creating and writing DDM and partition map...\n"); fflush(stdout);
	
	DDM = createDriverDescriptorMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE);
	
	partitions = createApplePartitionMap((volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, HFSX_VOLUME_TYPE);
	
	writeDriverDescriptorMap(outFile, DDM, &CRCProxy, (void*) (&dataForkToken), &resources);
	free(DDM);
	writeApplePartitionMap(outFile, partitions, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz);
	free(partitions);
	writeATAPI(outFile, &CRCProxy, (void*) (&dataForkToken), &resources, &nsiz);
	
	memset(&uncompressedToken, 0, sizeof(uncompressedToken));
	SHA1Init(&(uncompressedToken.sha1));
	
	printf("Writing main data blkx...\n"); fflush(stdout);
	
	fseeko(file, 0, SEEK_SET);
	blkx = insertBLKX(outFile, (void*) file, USER_OFFSET, (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, 2, CHECKSUM_CRC32, &freadWrapper, &fseekWrapper, &ftellWrapper,
					  &BlockSHA1CRC, &uncompressedToken, &CRCProxy, &dataForkToken, volume);
	
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
	
	writeFreePartition(outFile, (volumeHeader->totalBlocks * volumeHeader->blockSize)/SECTOR_SIZE, &resources);
	
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
	
	plistOffset = ftello(outFile);
	writeResources(outFile, resources);
	plistSize = ftello(outFile) - plistOffset;
	
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
	
	writeUDIFResourceFile(outFile, &koly);
	
	printf("Cleaning up...\n"); fflush(stdout);
	
	releaseResources(resources);
	
	fclose(outFile);
	fclose(file);
	closeVolume(volume);
	CLOSE(io);
	
	printf("Done.\n"); fflush(stdout);
	
	return TRUE;
}

int convertToDMG(const char* source, const char* dest) {
	FILE* file;
	FILE* outFile;
	Partition* partitions;
	DriverDescriptorRecord* DDM;
	int i;
	
	BLKXTable* blkx;
	ResourceKey* resources;
	ResourceKey* curResource;
	
	ChecksumToken dataForkToken;
	ChecksumToken uncompressedToken;
	
	NSizResource* nsiz;
	NSizResource* myNSiz;
	CSumResource csum;
	
	off_t plistOffset;
	uint32_t plistSize;
	uint32_t dataForkChecksum;
	uint64_t numSectors;
	
	UDIFResourceFile koly;
	
	char partitionName[512];
	
	off_t fileLength;
	
	
	numSectors = 0;
	
	resources = NULL;
	nsiz = NULL;
	myNSiz = NULL;
	memset(&dataForkToken, 0, sizeof(ChecksumToken));
	
	partitions = (Partition*) malloc(SECTOR_SIZE);
	
	ASSERT(file = fopen(source, "rb"), "fopen");
	ASSERT(outFile = fopen(dest, "wb"), "fopen");
	
	printf("Processing DDM...\n"); fflush(stdout);
	DDM = (DriverDescriptorRecord*) malloc(SECTOR_SIZE);
	fseeko(file, 0, SEEK_SET);
	ASSERT(fread(DDM, SECTOR_SIZE, 1, file) == 1, "fread");
	flipDriverDescriptorRecord(DDM, FALSE);
	
	if(DDM->sbSig == 0x4552) {
		writeDriverDescriptorMap(outFile, DDM, &CRCProxy, (void*) (&dataForkToken), &resources);
		free(DDM);
		
		printf("Processing partition map...\n"); fflush(stdout);
		
		fseeko(file, SECTOR_SIZE, SEEK_SET);
		ASSERT(fread(partitions, SECTOR_SIZE, 1, file) == 1, "fread");
		flipPartition(partitions, FALSE);
		
		partitions = (Partition*) realloc(partitions, SECTOR_SIZE * partitions->pmMapBlkCnt);
		
		fseeko(file, SECTOR_SIZE, SEEK_SET);
		ASSERT(fread(partitions, SECTOR_SIZE * partitions->pmMapBlkCnt, 1, file) == 1, "fread");
		flipPartition(partitions, FALSE);
		
		printf("Writing blkx...\n"); fflush(stdout);
		
		for(i = 0; i < partitions->pmPartBlkCnt; i++) {
			if(partitions[i].pmSig != APPLE_PARTITION_MAP_SIGNATURE) {
				break;
			}
			
			printf("Processing blkx %d...\n", i); fflush(stdout);
			
			sprintf(partitionName, "%s (%s : %d)", partitions[i].pmPartName, partitions[i].pmParType, i + 1);
			
			memset(&uncompressedToken, 0, sizeof(uncompressedToken));
			
			fseeko(file, partitions[i].pmPyPartStart * SECTOR_SIZE, SEEK_SET);
			blkx = insertBLKX(outFile, (void*) file, partitions[i].pmPyPartStart, partitions[i].pmPartBlkCnt, i, CHECKSUM_CRC32, &freadWrapper, &fseekWrapper, &ftellWrapper,
							  &BlockCRC, &uncompressedToken, &CRCProxy, &dataForkToken, NULL);
			
			blkx->checksum.data[0] = uncompressedToken.crc;	
			resources = insertData(resources, "blkx", i, partitionName, (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
			free(blkx);
			
			memset(&csum, 0, sizeof(CSumResource));
			csum.version = 1;
			csum.type = CHECKSUM_MKBLOCK;
			csum.checksum = uncompressedToken.block;
			resources = insertData(resources, "cSum", i, "", (const char*) (&csum), sizeof(csum), 0);
			
			if(nsiz == NULL) {
				nsiz = myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
			} else {
				myNSiz->next = (NSizResource*) malloc(sizeof(NSizResource));
				myNSiz = myNSiz->next;
			}
			
			memset(myNSiz, 0, sizeof(NSizResource));
			myNSiz->isVolume = FALSE;
			myNSiz->blockChecksum2 = uncompressedToken.block;
			myNSiz->partitionNumber = i;
			myNSiz->version = 6;
			myNSiz->next = NULL;
			
			if((partitions[i].pmPyPartStart + partitions[i].pmPartBlkCnt) > numSectors) {
				numSectors = partitions[i].pmPyPartStart + partitions[i].pmPartBlkCnt;
			}
		}
		
		koly.fUDIFImageVariant = kUDIFDeviceImageType;
	} else {
		printf("No DDM! Just doing one huge blkx then...\n"); fflush(stdout);
		
		fseeko(file, 0, SEEK_END);
		fileLength = ftello(file);
		
		memset(&uncompressedToken, 0, sizeof(uncompressedToken));
		
		fseeko(file, 0, SEEK_SET);
		blkx = insertBLKX(outFile, (void*) file, 0, fileLength/SECTOR_SIZE, ENTIRE_DEVICE_DESCRIPTOR, CHECKSUM_CRC32, &freadWrapper, &fseekWrapper, &ftellWrapper,
							  &BlockCRC, &uncompressedToken, &CRCProxy, &dataForkToken, NULL);
		resources = insertData(resources, "blkx", 0, "whole disk (unknown partition : 0)", (const char*) blkx, sizeof(BLKXTable) + (blkx->blocksRunCount * sizeof(BLKXRun)), ATTRIBUTE_HDIUTIL);
		free(blkx);
		
		memset(&csum, 0, sizeof(CSumResource));
		csum.version = 1;
		csum.type = CHECKSUM_MKBLOCK;
		csum.checksum = uncompressedToken.block;
		resources = insertData(resources, "cSum", 0, "", (const char*) (&csum), sizeof(csum), 0);
		
		if(nsiz == NULL) {
			nsiz = myNSiz = (NSizResource*) malloc(sizeof(NSizResource));
		} else {
			myNSiz->next = (NSizResource*) malloc(sizeof(NSizResource));
			myNSiz = myNSiz->next;
		}
		
		memset(myNSiz, 0, sizeof(NSizResource));
		myNSiz->isVolume = FALSE;
		myNSiz->blockChecksum2 = uncompressedToken.block;
		myNSiz->partitionNumber = 0;
		myNSiz->version = 6;
		myNSiz->next = NULL;
		
		koly.fUDIFImageVariant = kUDIFPartitionImageType;
	}
	
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
	
	plistOffset = ftello(outFile);
	writeResources(outFile, resources);
	plistSize = ftello(outFile) - plistOffset;
	
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
	
	koly.fUDIFSectorCount = numSectors;
	koly.reserved2 = 0;
	koly.reserved3 = 0;
	koly.reserved4 = 0;
	
	printf("Writing out UDIF resource file...\n"); fflush(stdout); 
	
	writeUDIFResourceFile(outFile, &koly);
	
	printf("Cleaning up...\n"); fflush(stdout);
	
	releaseResources(resources);
	
	fclose(file);
	free(partitions);
	
	printf("Done\n"); fflush(stdout);
	
	return TRUE;
}

int convertToISO(const char* source, const char* dest) {
	FILE* file;
	FILE* outFile;
	off_t fileLength;
	UDIFResourceFile resourceFile;
	ResourceKey* resources;
	ResourceData* blkx;
	BLKXTable* blkxTable;
	
	file = fopen(source, "rb");
	
	if(!file) {
		fprintf(stderr, "Cannot open source file\n");
		return FALSE;
	}
	
	fseeko(file, 0, SEEK_END);
	fileLength = ftello(file);
	fseeko(file, fileLength - sizeof(UDIFResourceFile), SEEK_SET);
	readUDIFResourceFile(file, &resourceFile);
	resources = readResources(file, &resourceFile);
	
	outFile = fopen(dest, "wb");
	if(!outFile) {
		fprintf(stderr, "Cannot open target file\n");
		releaseResources(resources);
		
		fclose(file);
		return FALSE;
	}
	
	blkx = (getResourceByKey(resources, "blkx"))->data;
	
	printf("Writing out data..\n"); fflush(stdout);
	
	while(blkx != NULL) {
		blkxTable = (BLKXTable*)(blkx->data);
		fseeko(outFile, blkxTable->firstSectorNumber * 512, SEEK_SET);
		extractBLKX(file, (void*) outFile, blkxTable, &fwriteWrapper, &fseekWrapper, &ftellWrapper);
		blkx = blkx->next;
	}
	
	fclose(outFile);
	
	releaseResources(resources);
	fclose(file);
	
	return TRUE;
	
}

int extractDmg(const char* source, const char* dest, int partNum) {
	FILE* file;
	FILE* outFile;
	off_t fileLength;
	UDIFResourceFile resourceFile;
	ResourceKey* resources;
	
	file = fopen(source, "rb");
	
	if(!file) {
		fprintf(stderr, "Cannot open source file\n");
		return FALSE;
	}
	
	fseeko(file, 0, SEEK_END);
	fileLength = ftello(file);
	fseeko(file, fileLength - sizeof(UDIFResourceFile), SEEK_SET);
	readUDIFResourceFile(file, &resourceFile);
	resources = readResources(file, &resourceFile);
	
	outFile = fopen(dest, "wb");
	if(!outFile) {
		fprintf(stderr, "Cannot open target file\n");
		releaseResources(resources);
		
		fclose(file);
		return FALSE;
	}
	
	printf("Writing out data..\n"); fflush(stdout);
	
	/* reasonable assumption that 2 is the main partition, given that that's usually the case in SPUD layouts */
	extractBLKX(file, (void*) outFile, (BLKXTable*)(getDataByID(getResourceByKey(resources, "blkx"), partNum)->data), &fwriteWrapper, &fseekWrapper, &ftellWrapper);
	fclose(outFile);
	
	releaseResources(resources);
	fclose(file);
	
	return TRUE;
}

int main(int argc, char* argv[]) {
	int partNum;
	
	TestByteOrder();
	
	if(argc < 4) {
		printf("usage: %s [extract <dmg> <img> (partition)|build <img> <dmg>]\n", argv[0]);
		return 0;
	}
	
	if(strcmp(argv[1], "extract") == 0) {
		partNum = 2;
		
		if(argc > 4) {
			sscanf(argv[4], "%d", &partNum);
		}
		extractDmg(argv[2], argv[3], partNum);
	} else if(strcmp(argv[1], "build") == 0) {
		buildDmg(argv[2], argv[3]);
	} else if(strcmp(argv[1], "iso") == 0) {
		convertToISO(argv[2], argv[3]);
	} else if(strcmp(argv[1], "dmg") == 0) {
		convertToDMG(argv[2], argv[3]);
	}
	
	return 0;
}
