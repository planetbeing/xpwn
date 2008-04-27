all:	hfs/hfsplus dmg/dmg hdutil/hdutil

dmg/dmg:
	cd dmg; make

hfs/hfsplus:
	cd hfs; make

hdutil/hdutil:
	cd hdutil; make

clean:
	cd dmg; make clean
	cd hfs; make clean
	cd hdutil; make clean

dist-clean:	clean
	-cd dmg/zlib-1.2.3; make clean
	-rm dmg/zlib-1.2.3/Makefile
	-rm -rf ide/xcode/build
