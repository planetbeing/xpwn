UTILOBJS=hdutil.o
DMGOBJS=../dmg/base64.o ../dmg/resources.o ../dmg/checksum.o ../dmg/udif.o ../dmg/partition.o ../dmg/io.o ../dmg/filevault.o ../dmg/dmgfile.o ../dmg/zlib-1.2.3/libz.a ../dmg/openssl-0.9.8g/libcrypto.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o ../hfs/fastunicodecompare.o ../hfs/hfslib.o
COMMONOBJS=../common/abstractfile.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -DHAVE_CRYPT
LIBRARIES=`if $(CC) win32test.c -o /dev/null 2>/dev/null ; then echo ""; else echo "-lgdi32"; fi`
INCLUDES=-I ../includes -I../dmg/zlib-1.2.3 -I../dmg/openssl-0.9.8g/include

all: util

util: $(DMGOBJS) $(HFSOBJS) $(UTILOBJS) $(COMMONOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) $(UTILOBJS) $(COMMONOBJS) $(LIBRARIES) -o hdutil

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

../dmg/zlib-1.2.3/Makefile:
	cd ../dmg/zlib-1.2.3; ./configure

../dmg/zlib-1.2.3/libz.a:	../dmg/zlib-1.2.3/Makefile
	cd ../dmg/zlib-1.2.3; make

../dmg/openssl-0.9.8g/libcrypto.a:
	touch ../dmg/openssl-0.9.8g/Makefile
	cd ../dmg/openssl-0.9.8g/crypto; make


clean:
	-rm *.o
	-rm hdutil
	-rm hdutil.exe
