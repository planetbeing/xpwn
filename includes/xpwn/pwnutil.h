#ifndef PWNUTIL_H
#define PWNUTIL_H

#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <hfs/hfsplus.h>

#ifdef __cplusplus
extern "C" {
#endif
	int patch(AbstractFile* in, AbstractFile* out, AbstractFile* patch);
	Dictionary* parseIPSW(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state);
	Dictionary* parseIPSW2(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state, int useMemory);
	int doPatch(StringValue* patchValue, StringValue* fileValue, const char* bundlePath, OutputState** state, unsigned int* key, unsigned int* iv);
	void doPatchInPlace(Volume* volume, const char* filePath, const char* patchPath);
	void fixupBootNeuterArgs(Volume* volume, char unlockBaseband, char selfDestruct, char use39, char use46);
	void createRestoreOptions(Volume* volume, int SystemPartitionSize, int UpdateBaseband);
#ifdef __cplusplus
}
#endif

#endif
