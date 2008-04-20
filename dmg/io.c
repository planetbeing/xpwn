#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "zlib.h"

#include "dmg.h"

size_t freadWrapper(void* token, unsigned char* data, size_t len) {
  return fread(data, 1, len, (FILE*) token);
}

size_t fwriteWrapper(void* token, const unsigned char* data, size_t len) {
  return fwrite(data, 1, len, (FILE*) token);
}

int fseekWrapper(void* token, off_t offset) {
  return fseeko((FILE*) token, offset, SEEK_SET);
}

off_t ftellWrapper(void* token) {
  return ftello((FILE*) token);
}

size_t dummyWrite(void* token, const unsigned char* data, size_t len) {
  *((off_t*) token) += len;
  return len;
}

int dummySeek(void* token, off_t offset) {
  *((off_t*) token) = offset;
  return 0;
}

off_t dummyTell(void* token) {
  return *((off_t*) token);
}

size_t memRead(void* token, unsigned char* data, size_t len) {
  MemWrapperInfo* info = (MemWrapperInfo*) token; 
  memcpy(data, info->buffer + info->offset, len);
  info->offset += (size_t)len;
  return len;
}

size_t memWrite(void* token, const unsigned char* data, size_t len) {
  MemWrapperInfo* info = (MemWrapperInfo*) token;
  
  while((info->offset + (size_t)len) > info->bufferSize) {
    info->bufferSize <<= 1;
    info->buffer = (unsigned char *) realloc(info->buffer, info->bufferSize);
  }
  
  memcpy(info->buffer + info->offset, data, len);
  info->offset += (size_t)len;
  return len;
}

int memSeek(void* token, off_t offset) {
  MemWrapperInfo* info = (MemWrapperInfo*) token;
  info->offset = (size_t)offset;
  return 0;
}

off_t memTell(void* token) {
  MemWrapperInfo* info = (MemWrapperInfo*) token;
  return (off_t)info->offset;
}

#define SECTORS_AT_A_TIME 0x200

BLKXTable* insertBLKX(FILE* out, void* in, uint32_t firstSectorNumber, uint32_t numSectors, uint32_t blocksDescriptor, uint32_t checksumType,
                    ReadFunc mRead, SeekFunc mSeek, TellFunc mTell,
                    ChecksumFunc uncompressedChk, void* uncompressedChkToken, ChecksumFunc compressedChk,  void* compressedChkToken, Volume* volume) {
  BLKXTable* blkx;
  
  uint32_t roomForRuns;
  uint32_t curRun;
  uint64_t curSector;
  
  unsigned char* inBuffer;
  unsigned char* outBuffer;
  size_t bufferSize;
  size_t have;
  int ret;
  
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
  blkx->checksum.type = checksumType;
  blkx->checksum.size = 0x20;
  blkx->blocksRunCount = 0;
    
  bufferSize = SECTOR_SIZE * blkx->decompressBufferRequested;
  
  ASSERT(inBuffer = (unsigned char*)  malloc(bufferSize), "malloc");
  ASSERT(outBuffer = (unsigned char*)  malloc(bufferSize), "malloc");
   
  curRun = 0;
  curSector = 0;
  
  while(numSectors > 0) {
    if(curRun >= roomForRuns) {
      roomForRuns <<= 1;
      blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
    }
    
    blkx->runs[curRun].type = BLOCK_ZLIB;
    blkx->runs[curRun].reserved = 0;
    blkx->runs[curRun].sectorStart = curSector;
    blkx->runs[curRun].sectorCount = (numSectors > SECTORS_AT_A_TIME) ? SECTORS_AT_A_TIME : numSectors;
       
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    
    /*printf("run %d: sectors=%lld, left=%d\n", curRun, blkx->runs[curRun].sectorCount, numSectors);*/
    
    ASSERT(deflateInit(&strm, Z_DEFAULT_COMPRESSION) == Z_OK, "deflateInit");
    
    ASSERT((strm.avail_in = mRead(in, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE)) == (blkx->runs[curRun].sectorCount * SECTOR_SIZE), "mRead");
    strm.next_in = inBuffer;
    
    if(uncompressedChk)
      (*uncompressedChk)(uncompressedChkToken, inBuffer, blkx->runs[curRun].sectorCount * SECTOR_SIZE);
   
    blkx->runs[curRun].compOffset = ftello(out) - blkx->dataStart;
    blkx->runs[curRun].compLength = 0;

    strm.avail_out = bufferSize;
    strm.next_out = outBuffer;
  
    ASSERT((ret = deflate(&strm, Z_FINISH)) != Z_STREAM_ERROR, "deflate/Z_STREAM_ERROR");
    if(ret != Z_STREAM_END) {
      ASSERT(FALSE, "deflate");
    }
    have = bufferSize - strm.avail_out;
    
    if((have / SECTOR_SIZE) > blkx->runs[curRun].sectorCount) {
      blkx->runs[curRun].type = BLOCK_RAW;
      ASSERT(fwrite(outBuffer, 1, blkx->runs[curRun].sectorCount * SECTOR_SIZE, out) == (blkx->runs[curRun].sectorCount * SECTOR_SIZE), "fwrite");
      blkx->runs[curRun].compLength += blkx->runs[curRun].sectorCount * SECTOR_SIZE;
    } else {
      ASSERT(fwrite(outBuffer, 1, have, out) == have, "fwrite");
      blkx->runs[curRun].compLength += have;
    }
        
    if(compressedChk)
      (*compressedChk)(compressedChkToken, outBuffer, have);
              
    deflateEnd(&strm);

    curSector += blkx->runs[curRun].sectorCount;
    numSectors -= blkx->runs[curRun].sectorCount;
    curRun++;
  }
  
  if(curRun >= roomForRuns) {
    roomForRuns <<= 1;
    blkx = (BLKXTable*) realloc(blkx, sizeof(BLKXTable) + (roomForRuns * sizeof(BLKXRun)));
  }
  
  blkx->runs[curRun].type = BLOCK_TERMINATOR;
  blkx->runs[curRun].reserved = 0;
  blkx->runs[curRun].sectorStart = curSector;
  blkx->runs[curRun].sectorCount = 0;
  blkx->runs[curRun].compOffset = ftello(out) - blkx->dataStart;
  blkx->runs[curRun].compLength = 0;
  blkx->blocksRunCount = curRun + 1;
  
  free(inBuffer);
  free(outBuffer);
  
  return blkx;
}

void extractBLKX(FILE* in, void* out, BLKXTable* blkx, WriteFunc mWrite, SeekFunc mSeek, TellFunc mTell) {
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
  
  ASSERT(inBuffer = (unsigned char*)  malloc(bufferSize), "malloc");
  ASSERT(outBuffer = (unsigned char*)  malloc(bufferSize), "malloc");
     
  initialOffset =  mTell(out);
  ASSERT(initialOffset != -1, "ftello");
  
  zero = 0;
  
  for(i = 0; i < blkx->blocksRunCount; i++) {
    ASSERT(fseeko(in, blkx->dataStart + blkx->runs[i].compOffset, SEEK_SET) == 0, "fseeko");
    ASSERT(mSeek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");
    
    if(blkx->runs[i].sectorCount > 0) {
      ASSERT(mSeek(out, initialOffset + (blkx->runs[i].sectorStart + blkx->runs[i].sectorCount) * SECTOR_SIZE - 1) == 0, "mSeek");
      ASSERT(mWrite(out, &zero, 1) == 1, "mWrite");
      ASSERT(mSeek(out, initialOffset + (blkx->runs[i].sectorStart * SECTOR_SIZE)) == 0, "mSeek");
    }
    
    if(blkx->runs[i].type == BLOCK_TERMINATOR) {
      break;
    }
    
    if( blkx->runs[i].compLength == 0) {
      continue;
    }
    
    printf("run %d: sectors=%lld, length=%lld, fileOffset=0x%llx\n", i, blkx->runs[i].sectorCount, blkx->runs[i].compLength, blkx->runs[i].compOffset);
    
    switch(blkx->runs[i].type) {
      case BLOCK_ZLIB:
        strm.zalloc = Z_NULL;
        strm.zfree = Z_NULL;
        strm.opaque = Z_NULL;
        strm.avail_in = 0;
        strm.next_in = Z_NULL;
        
        ASSERT(inflateInit(&strm) == Z_OK, "inflateInit");
        
        ASSERT((strm.avail_in = fread(inBuffer, 1, blkx->runs[i].compLength, in)) == blkx->runs[i].compLength, "fread");
        strm.next_in = inBuffer;
        
        do {
          strm.avail_out = bufferSize;
          strm.next_out = outBuffer;
          ASSERT((ret = inflate(&strm, Z_NO_FLUSH)) != Z_STREAM_ERROR, "inflate/Z_STREAM_ERROR");
          if(ret != Z_OK && ret != Z_BUF_ERROR && ret != Z_STREAM_END) {
            ASSERT(FALSE, "inflate");
          }
          have = bufferSize - strm.avail_out;
          ASSERT(mWrite(out, outBuffer, have) == have, "mWrite");
        } while (strm.avail_out == 0);
        
        ASSERT(inflateEnd(&strm) == Z_OK, "inflateEnd");
        break;
      case BLOCK_RAW:
        ASSERT((have = fread(inBuffer, 1, blkx->runs[i].compLength, in)) == blkx->runs[i].compLength, "fread");
        ASSERT(mWrite(out, inBuffer, have) == have, "mWrite");
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
