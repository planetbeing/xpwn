#include "../hfs/common.h"
#include "../hfs/hfsplus.h"
#include "../dmg/dmg.h"

#ifdef __cplusplus
extern "C" {
#endif
	int extractDmg(AbstractFile* abstractIn, AbstractFile* abstractOut, int partNum);
	uint32_t calculateMasterChecksum(ResourceKey* resources);
	int buildDmg(AbstractFile* abstractIn, AbstractFile* abstractOut);
	void writeToFile(HFSPlusCatalogFile* file, AbstractFile* output, Volume* volume);
	void writeToHFSFile(HFSPlusCatalogFile* file, AbstractFile* input, Volume* volume);
	void get_hfs(Volume* volume, const char* inFileName, AbstractFile* output);
	int add_hfs(Volume* volume, AbstractFile* inFile, const char* outFileName);
	void grow_hfs(Volume* volume, uint64_t newSize);
	void addAllInFolder(HFSCatalogNodeID folderID, Volume* volume, const char* parentName);
	void addall_hfs(Volume* volume, char* dirToMerge, char* dest);
	int copyAcrossVolumes(Volume* volume1, Volume* volume2, char* path1, char* path2);
#ifdef __cplusplus
}
#endif

