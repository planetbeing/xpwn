#ifndef PWNUTIL_H
#define PWNUTIL_H

#include <xpwn/plist.h>
#include <xpwn/outputstate.h>
#include <hfs/hfsplus.h>

#ifdef __cplusplus
extern "C" {
#endif
	Dictionary* parseIPSW(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state);
	int doPatch(StringValue* patchValue, StringValue* fileValue, const char* bundlePath, OutputState** state);
	void doPatchInPlace(Volume* volume, const char* filePath, const char* patchPath);
#ifdef __cplusplus
}
#endif

#endif
