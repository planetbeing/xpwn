#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <bzlib.h>
#include "abstractfile.h"

#define BUFFERSIZE (256 * 1024)

static off_t offtin(unsigned char *buf)
{
	off_t y;

	y = buf[7] & 0x7F;
	y <<= 8; y += buf[6];
	y <<= 8; y += buf[5];
	y <<= 8; y += buf[4];
	y <<= 8; y += buf[3];
	y <<= 8; y += buf[2];
	y <<= 8; y += buf[1];
	y <<= 8; y += buf[0];
	
	if(buf[7] & 0x80) y = -y;
	
	return y;
}

typedef struct {
	bz_stream bz2;
	AbstractFile* file;
	off_t offset;
	unsigned char* inBuffer;
	unsigned char* outBuffer;
	size_t bufferLen;
	char ended;
} BZStream;

size_t bzRead(int *bzerr, BZStream* stream, unsigned char* out, size_t len) {
	size_t toRead;
	size_t haveRead;
	size_t total;
	
	total = len;
	
	*bzerr = BZ_OK;
	
	while(total > 0) {
		if(!stream->ended) {
			memmove(stream->inBuffer, stream->bz2.next_in, stream->bz2.avail_in);
			stream->file->seek(stream->file, stream->offset);
			haveRead = stream->file->read(stream->file, stream->inBuffer + stream->bz2.avail_in, stream->bufferLen - stream->bz2.avail_in);
			stream->offset += haveRead;
			stream->bz2.avail_in += haveRead;
			stream->bz2.next_in = (char*) stream->inBuffer;
			
			*bzerr = BZ2_bzDecompress(&(stream->bz2));
			
			if(*bzerr == BZ_STREAM_END) {
				stream->ended = TRUE;
			} else {
				if(*bzerr != BZ_OK) {
					return 0;
				}
			}
		}
		
		if(total > (stream->bufferLen - stream->bz2.avail_out)) {
			toRead = stream->bufferLen - stream->bz2.avail_out;
		} else {
			toRead = total;
		}
		
		memcpy(out, stream->outBuffer, toRead);
		memmove(stream->outBuffer, stream->outBuffer + toRead, stream->bufferLen - toRead);
		stream->bz2.next_out -= toRead;
		stream->bz2.avail_out += toRead;
		out += toRead;
		total -= toRead;
		
		if(total > 0 && stream->ended) {
			return (len - total);
		}
	}
	
	return len;
}

void closeBZStream(BZStream* stream) {
	free(stream->inBuffer);
	free(stream->outBuffer);
	BZ2_bzDecompressEnd(&(stream->bz2));
	free(stream);
}

BZStream* openBZStream(AbstractFile* file, off_t offset, size_t bufferLen) {
	BZStream* stream;
	stream = (BZStream*) malloc(sizeof(BZStream));
	stream->file = file;
	stream->offset = offset;
	stream->bufferLen = bufferLen;
	stream->inBuffer = (unsigned char*) malloc(bufferLen);
	stream->outBuffer = (unsigned char*) malloc(bufferLen);
	memset(&(stream->bz2), 0, sizeof(bz_stream));
	BZ2_bzDecompressInit(&(stream->bz2), 0, FALSE);
	
	stream->bz2.next_in = (char*) stream->inBuffer;
	stream->bz2.avail_in = 0;
	stream->bz2.next_out = (char*) stream->outBuffer;
	stream->bz2.avail_out = bufferLen;

	stream->ended = FALSE;
	return stream;
}

int patch(AbstractFile* in, AbstractFile* out, AbstractFile* patch) {
	unsigned char header[32], buf[8];
	off_t oldsize, newsize;
	off_t bzctrllen, bzdatalen;
	off_t oldpos, newpos;
	int i;
	int cbz2err, dbz2err, ebz2err;
	off_t ctrl[3];
	size_t lenread;
	
	BZStream* cpfbz2;
	BZStream* dpfbz2;
	BZStream* epfbz2;
	
	/* Read header */
	if (patch->read(patch, header, 32) < 32) {
		return -1;
	}
	
	/* Check for appropriate magic */
	if (memcmp(header, "BSDIFF40", 8) != 0)
		return -2;
		
	/* Read lengths from header */
	bzctrllen = offtin(header + 8);
	bzdatalen = offtin(header + 16);
	newsize = offtin(header + 24);

	if((bzctrllen < 0) || (bzdatalen < 0) || (newsize < 0))
		return -3;
		
	cpfbz2 = openBZStream(patch, 32, 1024);
	dpfbz2 = openBZStream(patch, 32 + bzctrllen, 1024);
	epfbz2 = openBZStream(patch, 32 + bzctrllen + bzdatalen, 1024);
	
	oldsize = in->getLength(in);
	
	oldpos = 0;
	newpos = 0;
	unsigned char* writeBuffer = (unsigned char*) malloc(BUFFERSIZE);
	unsigned char* readBuffer = (unsigned char*) malloc(BUFFERSIZE);

	while(newpos < newsize) {
		/* Read control data */
		for(i=0;i<=2;i++) {
			lenread = bzRead(&cbz2err, cpfbz2, buf, 8);
			if ((lenread < 8) || ((cbz2err != BZ_OK) &&
			    (cbz2err != BZ_STREAM_END)))
				return -4;
			ctrl[i] = offtin(buf);
		};
			
		/* Sanity-check */
		if((newpos + ctrl[0]) > newsize)
			return -5;

		/* Read diff string */
		unsigned int toRead;
		unsigned int total = ctrl[0];
		while(total > 0) {
			if(total > BUFFERSIZE)
				toRead = BUFFERSIZE;
			else
				toRead = total;

			memset(writeBuffer, 0, toRead);
			lenread = bzRead(&dbz2err, dpfbz2, writeBuffer, toRead);
			if ((lenread < toRead) ||
			    ((dbz2err != BZ_OK) && (dbz2err != BZ_STREAM_END)))
				return -6;

			/* Add old data to diff string */
			in->seek(in, oldpos);
			unsigned int maxRead;
			if((oldpos + toRead) > oldsize)
				maxRead = oldsize - oldpos;
			else
				maxRead = toRead;

			in->read(in, readBuffer, maxRead);
			for(i = 0; i < maxRead; i++) {
				writeBuffer[i] += readBuffer[i];
			}

			out->seek(out, newpos);
			out->write(out, writeBuffer, toRead);

			/* Adjust pointers */
			newpos += toRead;
			oldpos += toRead;
			total -= toRead;
		}

		/* Sanity-check */
		if((newpos + ctrl[1]) > newsize)
			return -7;

		total = ctrl[1];

		while(total > 0){ 
			if(total > BUFFERSIZE)
				toRead = BUFFERSIZE;
			else
				toRead = total;

			/* Read extra string */
			lenread = bzRead(&ebz2err, epfbz2, writeBuffer, toRead);
			if ((lenread < toRead) ||
			    ((ebz2err != BZ_OK) && (ebz2err != BZ_STREAM_END)))
				return -8;

			out->seek(out, newpos);
			out->write(out, writeBuffer, toRead);
			
			/* Adjust pointers */
			newpos += toRead;
			total -= toRead;
		}

		oldpos += ctrl[2];
	};
	
	free(writeBuffer);
	free(readBuffer);

	closeBZStream(cpfbz2);
	closeBZStream(dpfbz2);
	closeBZStream(epfbz2);

	out->close(out);
	in->close(in);

	patch->close(patch);
	return 0;
}
