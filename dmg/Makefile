DMGOBJS=dmg.o base64.o resources.o checksum.o udif.o partition.o io.o abstractfile.o filevault.o dmgfile.o zlib-1.2.3/libz.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o ../hfs/fastunicodecompare.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -DHAVE_CRYPT
LIBRARIES=-lcrypto

all: dmg

dmg: $(DMGOBJS) $(HFSOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) $(LIBRARIES) -o dmg

%.o:	%.c dmg.h filevault.h dmgfile.h
	$(CC) $(CFLAGS) -c $< -o $@

zlib-1.2.3/Makefile:
	cd zlib-1.2.3; ./configure

zlib-1.2.3/libz.a:	zlib-1.2.3/Makefile
	cd zlib-1.2.3; make

clean:
	-rm *.o
	-rm dmg
