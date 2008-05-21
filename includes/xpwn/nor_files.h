#ifndef NOR_FILES_H
#define NOR_FILES_H

#include <abstractfile.h>
#include <xpwn/8900.h>
#include <xpwn/img2.h>
#include <xpwn/lzssfile.h>

#ifdef __cplusplus
extern "C" {
#endif
	AbstractFile* openAbstractFile(AbstractFile* file);
	AbstractFile* duplicateAbstractFile(AbstractFile* file, AbstractFile* backing);
#ifdef __cplusplus
}
#endif

#endif

