UTILOBJS=hdutil.o
DMGOBJS=../dmg/base64.o ../dmg/resources.o ../dmg/checksum.o ../dmg/udif.o ../dmg/partition.o ../dmg/io.o ../dmg/abstractfile.o ../dmg/filevault.o ../dmg/dmgfile.o ../dmg/zlib-1.2.3/libz.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o ../hfs/fastunicodecompare.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -DHAVE_CRYPT
LIBRARIES=-lcrypto

all: util

util: $(DMGOBJS) $(HFSOBJS) $(UTILOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) $(UTILOBJS) $(LIBRARIES) -o hdutil

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

../dmg/zlib-1.2.3/Makefile:
	cd ../dmg/zlib-1.2.3; ./configure

../dmg/zlib-1.2.3/libz.a:	../dmg/zlib-1.2.3/Makefile
	cd ../dmg/zlib-1.2.3; make

clean:
	-rm *.o
	-rm hdutil
