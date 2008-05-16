all:	hfs/hfsplus dmg/dmg hdutil/hdutil ipsw-patch/pch xpwn/xpwn

dmg/dmg:
	cd dmg; make

hfs/hfsplus:
	cd hfs; make

hdutil/hdutil:
	cd hdutil; make

ipsw-patch/pch:	hfs/hfsplus dmg/dmg
	cd ipsw-patch; make

xpwn/xpwn:	hfs/hfsplus dmg/dmg
	cd xpwn; make

install:	xpwn/xpwn ipsw-patch/pch
	-rm -rf xpwn-build
	mkdir xpwn-build
	cp ipsw-patch/pch xpwn-build/ipsw
	cp xpwn/build/xpwn xpwn-build/xpwn
	cp xpwn/ramdisk.dmg xpwn-build/ramdisk.dmg
	cp -R ipsw-patch/FirmwareBundles xpwn-build/FirmwareBundles
	cp -R ipsw-patch/bundles xpwn-build/bundles
	cp README.markdown xpwn-build/README
	tar jcvf xpwn-linux.tar.bz2 xpwn-build

install-win:	xpwn/xpwn ipsw-patch/pch
	-rm -rf xpwn-build
	mkdir xpwn-build
	cp ipsw-patch/pch.exe xpwn-build/ipsw.exe
	cp xpwn/build/xpwn.exe xpwn-build/xpwn.exe
	cp xpwn/ramdisk.dmg xpwn-build/ramdisk.dmg
	cp -R ipsw-patch/FirmwareBundles xpwn-build/FirmwareBundles
	cp -R ipsw-patch/bundles xpwn-build/bundles
	cp README.markdown xpwn-build/README.txt

clean:
	cd dmg; make clean
	cd hfs; make clean
	cd hdutil; make clean
	cd ipsw-patch; make clean
	cd xpwn; make clean

dist-clean:	clean
	-cd dmg/zlib-1.2.3; make clean
	-rm dmg/zlib-1.2.3/Makefile
	-cd dmg/openssl-0.9.8g; make clean
	-cd ipsw-patch/libpng-1.2.28; make clean
	-cd ipsw-patch/bzip2-1.0.5; make clean
	-cd xpwn/libusb-0.1.12; make clean
	-rm -rf xpwn/libusb-0.1.12/autom4te.cache
	-rm xpwn/libusb-0.1.12/config.h
	-rm xpwn/libusb-0.1.12/config.log
	-rm xpwn/libusb-0.1.12/config.status
	-rm xpwn/libusb-0.1.12/config.status.lineno
	-rm xpwn/libusb-0.1.12/libtool
	-rm xpwn/libusb-0.1.12/doc/Makefile
	-rm xpwn/libusb-0.1.12/tests/Makefile
	-rm -rf xpwn/libusb-0.1.12/.deps
	-rm -rf xpwn/libusb-0.1.12/tests/.deps
	-rm xpwn/libusb-0.1.12/Makefile
	-cd xpwn/libusb-win32; make clean
	-rm -rf ide/xcode/build
	-rm dmg/zlib-1.2.3/contrib/minizip/*.o

