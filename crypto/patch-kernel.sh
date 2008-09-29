#!/bin/bash
XPWNTOOL=./xpwntool
PATCHKERNEL=./patch-kernel-crypto
KERNEL=/System/Library/Caches/com.apple.kernelcaches/kernelcache.s5l8900x

if [ $# -ne 2 ]
then
	${XPWNTOOL} ${KERNEL} /tmp/a
	${PATCHKERNEL} /tmp/a
	${XPWNTOOL} /tmp/a /tmp/b -t ${KERNEL}
else
	${XPWNTOOL} ${KERNEL} /tmp/a -iv $1 -k $2
	${PATCHKERNEL} /tmp/a
	${XPWNTOOL} /tmp/a /tmp/b -t ${KERNEL} -iv $1 -k $2
fi

rm /tmp/a
cp ${KERNEL} /kernel.backup
cp /tmp/b ${KERNEL}
rm /tmp/b

