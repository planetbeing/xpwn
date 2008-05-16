#include "../hfs/common.h"
#include "../hfs/hfsplus.h"
#include "../dmg/dmg.h"

#ifdef __cplusplus
extern "C" {
#endif
	int extractDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, int partNum);
	uint32_t calculateMasterChecksum(ResourceKey* resources);
	int buildDmg(AbstractFile* abstractIn, AbstractFile* abstractOut);
#ifdef __cplusplus
}
#endif

