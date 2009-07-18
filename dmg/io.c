#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include <dmg/dmg.h>
#include <inttypes.h>

#define SECTORS_AT_A_TIME 0x200

// Okay, this value sucks. You shouldn't touch it because it affects how many ignore sections get added to the blkx list
// If the blkx list gets too fragmented with ignore sections, then the copy list in certain versions of the iPhone's
// asr becomes too big. Due to Apple's BUGGY CODE, this causes asr to segfault! This is because the copy list becomes
// too large for the initial buffer allocated, and realloc is called by asr. Unfortunately, after the realloc, the initial
// pointer is still used by asr for a little while! Frakking noob mistake.

// The only reason why it works at all is their really idiotic algorithm to determine where to put ignore blocks. It's
// certainly nothing reasonable like "put in an ignore block if you encounter more than X blank sectors" (like mine)
// There's always a large-ish one at the end, and a tiny 2 sector one at the end too, to take care of the space after
// the backup volume header. No frakking clue how they go about determining how to do that.

BLKXTable* insertBLKX(AbstractFile* out, AbstractFile* in, uint32_t firstSectorNumber, uint32_t numSectors, uint32_t blocksDescriptor,
			uint32_t checksumType, ChecksumFunc uncompressedChk, void* uncompressedChkToken, ChecksumFunc compressedChk,
			void* compressedChkToken, Volume* volume, int addComment) {
	BLKXTable* blkx;
	
	uint32_t roomForRuns;
	uint32_t curRun;
	uint64_t curSector;
	
	unsigned char* inBuffer;
	unsigned char* outBuffer;
	size_t bufferSize;
	size_t have;
	int ret;
	
	int IGNORE_THRESHOLD = 100000;

	z_stream strm;	
	
	blkx = (BLKXTable*) malloc(sizeof(BLKXTable) + (2 * sizeof(BLKXRun)));
	roomForRuns = 2;
	memset(blkx, 0, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
	
	blkx->fUDIFBlocksSignature = UDIF_BLOCK_SIGNATURE;
	blkx->infoVersion = 1;
	blkx->firstSectorNumber = firstSectorNumber;
	blkx->sectorCount = numSectors;
	blkx->dataStart = 0;
	blkx->decompressBufferRequested = 0x208;
	blkx->blocksDescriptor = blocksDescriptor;
	blkx->reserved1 = 0;
	blkx->reserved2 = 0;
	blkx->reserved3 = 0;
	blkx->reserved4 = 0;
	blkx->reserved5 = 0;
	blkx->reserved6 = 0;
	memset(&(blkx->checksum), 0, sizeof(blkx->checksum));
	blkx->checksum.type = checksumType;
	blkx->checksum.size = 0x20;
	blkx->blocksRunCount = 0;
		
	bufferSize = SECTOR_SIZE * blkx->decompressBufferRequested;
	
	ASSERT(inBuffer = (unsigned char*) malloc(bufferSize), "malloc");
	ASSERT(outBuffer = (unsigned char*) malloc(bufferSize), "malloc");
	 
	curRun = 0;
	curSector = 0;
	
	uint64_t startOff = in->tell(in);

	if(addComment)
	{
		blkx->runs[curRun].type = BLOCK_COMMENT;
		blkx->runs[curRun].reserved = 0x2B626567;
		blkx->runs[curRun].sectorStart = curSector;
		blkx->runs[curRun].sectorCount = 0;
		blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
		blkx->runs[curRun].compLength = 0;
		curRun++;

		if(curRun >= roomForRuns) {
			roomForRuns <<= 1;
			blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
		}
	}

	while(numSectors > 0) {
		if(curRun >= roomForRuns) {
			roomForRuns <<= 1;
			blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
		}
		
		blkx->runs[curRun].type = BLOCK_ZLIB;
		blkx->runs[curRun].reserved = 0;
		blkx->runs[curRun].sectorStart = curSector;
		blkx->runs[curRun].sectorCount = (numSectors > SECTORS_AT_A_TIME) ? SECTORS_AT_A_TIME : numSectors;

		memset(&strm, 0, sizeof(strm));
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		
		int amountRead;
		{
			size_t sectorsToSkip = 0;
			size_t processed = 0;

			while(processed < numSectors)
			{
				blkx->runs[curRun].sectorCount = ((numSectors - processed) > SECTORS_AT_A_TIME) ? SECTORS_AT_A_TIME : (numSectors - processed);

				//printf("Currently at %" PRId64 "\n", curOff);
				in->seek(in, startOff + (blkx->sectorCount - numSectors + processed) * SECTOR_SIZE);
				ASSERT((amountRead = in->read(in, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE)) == (blkx->runs[curRun].sectorCount * SECTOR_SIZE), "mRead");

				if(!addComment)
					break;

				processed += amountRead / SECTOR_SIZE;

				size_t* checkBuffer = (size_t*) inBuffer;
				size_t counter;
				size_t counter_max = amountRead / sizeof(size_t);
				for(counter = 0; counter < counter_max; counter++)
				{
					if(checkBuffer[counter] != 0) {
						//printf("Not empty at %" PRId64 " / %" PRId64 "\n", (int64_t)(counter * sizeof(size_t)) + curOff, (int64_t)((counter * sizeof(size_t)) / SECTOR_SIZE + sectorsToSkip + blkx->runs[curRun].sectorStart));
						break;
					}
				}

				size_t skipInBuffer = (counter * sizeof(size_t)) / SECTOR_SIZE;
				sectorsToSkip += skipInBuffer;

				//printf("sectorsToSkip: %d\n", sectorsToSkip);

				if(counter < counter_max)
				{
					if(sectorsToSkip > IGNORE_THRESHOLD)
					{
						//printf("Seeking back to %" PRId64 "\n", curOff + (skipInBuffer * SECTOR_SIZE));
						//in->seek(in, curOff + (skipInBuffer * SECTOR_SIZE));
					} else {
						//printf("Breaking out: %d / %d\n", (size_t) counter, (size_t) counter_max);
					}
					break;
				}
			}

			if(sectorsToSkip > IGNORE_THRESHOLD)
			{
				int remainder = sectorsToSkip & 0xf;

				if(sectorsToSkip != remainder)
				{
					blkx->runs[curRun].type = BLOCK_IGNORE;
					blkx->runs[curRun].reserved = 0;
					blkx->runs[curRun].sectorStart = curSector;
					blkx->runs[curRun].sectorCount = sectorsToSkip - remainder;
					blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
					blkx->runs[curRun].compLength = 0;

					printf("run %d: skipping sectors=%" PRId64 ", left=%d\n", curRun, (int64_t) sectorsToSkip, numSectors);

					curSector += blkx->runs[curRun].sectorCount;
					numSectors -= blkx->runs[curRun].sectorCount;

					curRun++;
				}

				if(remainder > 0)
				{
					blkx->runs[curRun].type = BLOCK_IGNORE;
					blkx->runs[curRun].reserved = 0;
					blkx->runs[curRun].sectorStart = curSector;
					blkx->runs[curRun].sectorCount = remainder;
					blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
					blkx->runs[curRun].compLength = 0;

					printf("run %d: skipping sectors=%" PRId64 ", left=%d\n", curRun, (int64_t) sectorsToSkip, numSectors);

					curSector += blkx->runs[curRun].sectorCount;
					numSectors -= blkx->runs[curRun].sectorCount;

					curRun++;
				}

				IGNORE_THRESHOLD = 0;
				
				continue;
			}
		}

		printf("run %d: sectors=%" PRId64 ", left=%d\n", curRun, blkx->runs[curRun].sectorCount, numSectors);

		ASSERT(deflateInit(&strm, 1) == Z_OK, "deflateInit");
		
		strm.avail_in = amountRead;
		strm.next_in = inBuffer;
		
		if(uncompressedChk)
			(*uncompressedChk)(uncompressedChkToken, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE);
	 
		blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
		blkx->runs[curRun].compLength = 0;

		strm.avail_out = bufferSize;
		strm.next_out = outBuffer;
	
		ASSERT((ret = deflate(&strm, Z_FINISH)) != Z_STREAM_ERROR, "deflate/Z_STREAM_ERROR");
		if(ret != Z_STREAM_END) {
			ASSERT(FALSE, "deflate");
		}
		have = bufferSize - strm.avail_out;
		
		if((have / SECTOR_SIZE) >= (blkx->runs[curRun].sectorCount - 15)) {
			blkx->runs[curRun].type = BLOCK_RAW;
			ASSERT(out->write(out, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE) == (blkx->runs[curRun].sectorCount * SECTOR_SIZE), "fwrite");
			blkx->runs[curRun].compLength += blkx->runs[curRun].sectorCount * SECTOR_SIZE;

			if(compressedChk)
				(*compressedChk)(compressedChkToken, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE);

		} else {
			ASSERT(out->write(out, outBuffer, have) == have, "fwrite");

			if(compressedChk)
				(*compressedChk)(compressedChkToken, outBuffer, have);

			blkx->runs[curRun].compLength += have;
		}
							
		deflateEnd(&strm);

		curSector += blkx->runs[curRun].sectorCount;
		numSectors -= blkx->runs[curRun].sectorCount;
		curRun++;
	}
	
	if(curRun >= roomForRuns) {
		roomForRuns <<= 1;
		blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
	}

	if(addComment)
	{	
		blkx->runs[curRun].type = BLOCK_COMMENT;
		blkx->runs[curRun].reserved = 0x2B656E64;
		blkx->runs[curRun].sectorStart = curSector;
		blkx->runs[curRun].sectorCount = 0;
		blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
		blkx->runs[curRun].compLength = 0;
		curRun++;

		if(curRun >= roomForRuns) {
			roomForRuns <<= 1;
			blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
		}
	}

	blkx->runs[curRun].type = BLOCK_TERMINATOR;
	blkx->runs[curRun].reserved = 0;
	blkx->runs[curRun].sectorStart = curSector;
	blkx->runs[curRun].sectorCount = 0;
	blkx->runs[curRun].compOffset = out->tell(out) - blkx->dataStart;
	blkx->runs[curRun].compLength = 0;
	blkx->blocksRunCount = curRun + 1;
	
	free(inBuffer);
	free(outBuffer);
	
	return blkx;
}

#define DEFAULT_BUFFER_SIZE (1 * 1024 * 1024)

void extractBLKX(AbstractFile* in, AbstractFile* out, BLKXTable* blkx) {
	unsigned char* inBuffer;
	unsigned char* outBuffer;
	unsigned char zero;
	size_t bufferSize;
	size_t have;
	off_t initialOffset;
	int i;
	int ret;
	
	z_stream strm;
	
	bufferSize = SECTOR_SIZE * blkx->decompressBufferRequested;
	
	ASSERT(inBuffer = (unsigned char*) malloc(bufferSize), "malloc");
	ASSERT(outBuffer = (unsigned char*) malloc(bufferSize), "malloc");
		 
	initialOffset =	out->tell(out);
	ASSERT(initialOffset != -1, "ftello");
	
	zero = 0;
	
	for(i = 0; i < blkx->blocksRunCount; i++) {
		ASSERT(in->seek(in, blkx->dataStart + blkx->runs[i].compOffset) == 0, "fseeko");
		ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");
		
		if(blkx->runs[i].sectorCount > 0) {
			ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart + blkx->runs[i].sectorCount) * SECTOR_SIZE - 1) == 0, "mSeek");
			ASSERT(out->write(out, &zero, 1) == 1, "mWrite");
			ASSERT(out->seek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");
		}
		
		if(blkx->runs[i].type == BLOCK_TERMINATOR) {
			break;
		}
		
		if( blkx->runs[i].compLength == 0) {
			continue;
		}
		
		printf("run %d: start=%" PRId64 " sectors=%" PRId64 ", length=%" PRId64 ", fileOffset=0x%" PRIx64 "\n", i, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE), blkx->runs[i].sectorCount, blkx->runs[i].compLength, blkx->runs[i].compOffset);
		
		switch(blkx->runs[i].type) {
			case BLOCK_ZLIB:
				strm.zalloc = Z_NULL;
				strm.zfree = Z_NULL;
				strm.opaque = Z_NULL;
				strm.avail_in = 0;
				strm.next_in = Z_NULL;
				
				ASSERT(inflateInit(&strm) == Z_OK, "inflateInit");
				
				ASSERT((strm.avail_in = in->read(in, inBuffer, blkx->runs[i].compLength)) == blkx->runs[i].compLength, "fread");
				strm.next_in = inBuffer;
				
				do {
					strm.avail_out = bufferSize;
					strm.next_out = outBuffer;
					ASSERT((ret = inflate(&strm, Z_NO_FLUSH)) != Z_STREAM_ERROR, "inflate/Z_STREAM_ERROR");
					if(ret != Z_OK && ret != Z_BUF_ERROR && ret != Z_STREAM_END) {
						ASSERT(FALSE, "inflate");
					}
					have = bufferSize - strm.avail_out;
					ASSERT(out->write(out, outBuffer, have) == have, "mWrite");
				} while (strm.avail_out == 0);
				
				ASSERT(inflateEnd(&strm) == Z_OK, "inflateEnd");
				break;
			case BLOCK_RAW:
				if(blkx->runs[i].compLength > bufferSize) {
					uint64_t left = blkx->runs[i].compLength;
					void* pageBuffer = malloc(DEFAULT_BUFFER_SIZE);
					while(left > 0) {
						size_t thisRead;
						if(left > DEFAULT_BUFFER_SIZE) {
							thisRead = DEFAULT_BUFFER_SIZE;
						} else {
							thisRead = left;
						}
						ASSERT((have = in->read(in, pageBuffer, thisRead)) == thisRead, "fread");
						ASSERT(out->write(out, pageBuffer, have) == have, "mWrite");
						left -= have;
					}
					free(pageBuffer);
				} else {
					ASSERT((have = in->read(in, inBuffer, blkx->runs[i].compLength)) == blkx->runs[i].compLength, "fread");
					ASSERT(out->write(out, inBuffer, have) == have, "mWrite");
				}
				break;
			case BLOCK_IGNORE:
				break;
			case BLOCK_COMMENT:
				break;
			case BLOCK_TERMINATOR:
				break;
			default:
				break;
		}
	}
	
	free(inBuffer);
	free(outBuffer);
}
