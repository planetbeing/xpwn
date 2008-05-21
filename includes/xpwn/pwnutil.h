#ifndef PWNUTIL_H
#define PWNUTIL_H

#include <xpwn/plist.h>
#include <xpwn/outputstate.h>

#ifdef __cplusplus
extern "C" {
#endif
	Dictionary* parseIPSW(const char* inputIPSW, const char* bundleRoot, char** bundlePath, OutputState** state);
#ifdef __cplusplus
}
#endif

#endif
