#include <stdio.h>
#include <string.h>
#include <xpwn/nor_files.h>

#define BUFFERSIZE (1024*1024)

int main(int argc, char* argv[]) {
	char* inData;
	size_t inDataSize;
	init_libxpwn();

	if(argc < 3) {
		printf("usage: %s <infile> <outfile> [-t <template> [-c <certificate>]] [-k <key>] [-iv <key>]\n", argv[0]);
		return 0;
	}

	AbstractFile* template = NULL;
	AbstractFile* certificate = NULL;
	int key[16];
	int iv[16];
	int hasKey = FALSE;
	int hasIV = FALSE;

	int argNo = 3;
	while(argNo < argc) {
		if(strcmp(argv[argNo], "-t") == 0 && (argNo + 1) < argc) {
			template = createAbstractFileFromFile(fopen(argv[argNo + 1], "rb"));
			if(!template) {
				fprintf(stderr, "error: cannot open template\n");
				return 1;
			}
		}

		if(strcmp(argv[argNo], "-c") == 0 && (argNo + 1) < argc) {
			certificate = createAbstractFileFromFile(fopen(argv[argNo + 1], "rb"));
			if(!certificate) {
				fprintf(stderr, "error: cannot open template\n");
				return 1;
			}
		}

		if(strcmp(argv[argNo], "-k") == 0 && (argNo + 1) < argc) {
			sscanf(argv[argNo + 1], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&key[0], &key[1], &key[2], &key[3], &key[4], &key[5], &key[6], &key[7], &key[8],
				&key[9], &key[10], &key[11], &key[12], &key[13], &key[14], &key[15]);
			hasKey = TRUE;
		}

		if(strcmp(argv[argNo], "-iv") == 0 && (argNo + 1) < argc) {
			sscanf(argv[argNo + 1], "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
				&iv[0], &iv[1], &iv[2], &iv[3], &iv[4], &iv[5], &iv[6], &iv[7], &iv[8],
				&iv[9], &iv[10], &iv[11], &iv[12], &iv[13], &iv[14], &iv[15]);
			hasIV = TRUE;
		}

		argNo++;
	}

	AbstractFile* inFile;
	if(hasKey) {
		inFile = openAbstractFile2(createAbstractFileFromFile(fopen(argv[1], "rb")), key, iv);
	} else {
		inFile = openAbstractFile(createAbstractFileFromFile(fopen(argv[1], "rb")));
	}
	if(!inFile) {
		fprintf(stderr, "error: cannot open infile\n");
		return 2;
	}

	AbstractFile* outFile = createAbstractFileFromFile(fopen(argv[2], "wb"));
	if(!outFile) {
		fprintf(stderr, "error: cannot open outfile\n");
		return 3;
	}


	AbstractFile* newFile;

	if(template) {
		if(hasKey) {
			newFile = duplicateAbstractFile2(template, outFile, key, iv, certificate);
		} else {
			newFile = duplicateAbstractFile2(template, outFile, NULL, NULL, certificate);
		}
		if(!newFile) {
			fprintf(stderr, "error: cannot duplicate file from provided template\n");
			return 4;
		}
	} else {
		newFile = outFile;
	}

	if(hasKey) {
		if(newFile->type == AbstractFileTypeImg3) {
			AbstractFile2* abstractFile2 = (AbstractFile2*) newFile;
			abstractFile2->setKey(abstractFile2, key, iv);
		}
	}

	inDataSize = (size_t) inFile->getLength(inFile);
	inData = (char*) malloc(inDataSize);
	inFile->read(inFile, inData, inDataSize);
	inFile->close(inFile);

	newFile->write(newFile, inData, inDataSize);
	newFile->close(newFile);

	free(inData);

	return 0;
}

