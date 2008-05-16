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

