PCHOBJS=patch.o plist.o outputstate.o dmghfs.o nor_files.o 8900.o img2.o bspatch.o ibootim.o lzss.o lzssfile.o ../dmg/openssl-0.9.8g/libcrypto.a bzip2-1.0.5/libbz2.a ../dmg/zlib-1.2.3/contrib/minizip/ioapi.o ../dmg/zlib-1.2.3/contrib/minizip/unzip.o ../dmg/zlib-1.2.3/contrib/minizip/zip.o libpng-1.2.28/libpng.a
DMGOBJS=../dmg/base64.o ../dmg/resources.o ../dmg/checksum.o ../dmg/udif.o ../dmg/partition.o ../dmg/io.o ../dmg/abstractfile.o ../dmg/filevault.o ../dmg/dmgfile.o ../dmg/zlib-1.2.3/libz.a ../dmg/openssl-0.9.8g/libcrypto.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o ../hfs/fastunicodecompare.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -DHAVE_CRYPT -O3
LIBRARIES=`if $(CC) win32test.c -o /dev/null 2>/dev/null ; then echo ""; else echo "-lgdi32"; fi` -lm

all: pch

pch: $(DMGOBJS) $(HFSOBJS) $(PCHOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) $(PCHOBJS) $(LIBRARIES) -o pch

%.o:	%.c
	$(CC) $(CFLAGS) -Ilibpng-1.2.28 -I../dmg/zlib-1.2.3 -I../dmg/openssl-0.9.8g/include -Ibzip2-1.0.5 -c $< -o $@

../dmg/zlib-1.2.3/Makefile:
	cd ../dmg/zlib-1.2.3; ./configure

../dmg/zlib-1.2.3/libz.a:	../dmg/zlib-1.2.3/Makefile
	cd ../dmg/zlib-1.2.3; make

../dmg/openssl-0.9.8g/libcrypto.a:
	touch ../dmg/openssl-0.9.8g/Makefile
	cd ../dmg/openssl-0.9.8g/crypto; make

bzip2-1.0.5/libbz2.a:
	cd bzip2-1.0.5; make

libpng-1.2.28/libpng.a:
	cd libpng-1.2.28; make

clean:
	-rm *.o
	-rm pch
	-rm pch.exe
