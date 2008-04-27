all:	hfs dmg

dmg:	dmg/dmg

hfs:	hfs/hfsplus

dmg/dmg:
	cd dmg; make

hfs/hfsplus:
	cd hfs; make

clean:
	cd dmg; make clean
	cd hfs; make clean

dist-clean:	clean
	-cd dmg/zlib-1.2.3; make clean
	-rm dmg/zlib-1.2.3/Makefile
	-rm -rf ide/xcode/build
