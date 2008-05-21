#include "../hfs/common.h"
#include "8900.h"
#include "img2.h"
#include "lzssfile.h"
#include "ibootim.h"

AbstractFile* openAbstractFile(AbstractFile* file) {
	uint32_t signatureBE;
	uint32_t signatureLE;
	
	file->seek(file, 0);
	file->read(file, &signatureBE, sizeof(signatureBE));
	signatureLE = signatureBE;
	FLIPENDIAN(signatureBE);
	FLIPENDIANLE(signatureLE);
	file->seek(file, 0);
	
	if(signatureBE == SIGNATURE_8900) {
		return openAbstractFile(createAbstractFileFrom8900(file));
	} else if(signatureLE == IMG2_SIGNATURE) {
		return openAbstractFile(createAbstractFileFromImg2(file));
	} else if(signatureBE == COMP_SIGNATURE) {
		return openAbstractFile(createAbstractFileFromComp(file));
	} else if(signatureBE == IBOOTIM_SIG_UINT) {
		return openAbstractFile(createAbstractFileFromIBootIM(file));
	} else {
		return file;
	}
}

AbstractFile* duplicateAbstractFile(AbstractFile* file, AbstractFile* backing) {
	uint32_t signatureBE;
	uint32_t signatureLE;
	AbstractFile* orig;
	
	file->seek(file, 0);
	file->read(file, &signatureBE, sizeof(signatureBE));
	signatureLE = signatureBE;
	FLIPENDIAN(signatureBE);
	FLIPENDIANLE(signatureLE);
	file->seek(file, 0);
	
	if(signatureBE == SIGNATURE_8900) {
		orig = createAbstractFileFrom8900(file);
		return duplicateAbstractFile(orig, duplicate8900File(orig, backing));
	} else if(signatureLE == IMG2_SIGNATURE) {
		orig = createAbstractFileFromImg2(file);
		return duplicateAbstractFile(orig, duplicateImg2File(orig, backing));
	} else if(signatureBE == COMP_SIGNATURE) {
		orig = createAbstractFileFromComp(file);
		return duplicateAbstractFile(orig, duplicateCompFile(orig, backing));
	} else if(signatureBE == IBOOTIM_SIG_UINT) {
		orig = createAbstractFileFromIBootIM(file);
		return duplicateAbstractFile(orig, duplicateIBootIMFile(orig, backing));
	} else {
		file->close(file);
		return backing;
	}
}
