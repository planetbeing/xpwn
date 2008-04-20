DMGOBJS=dmg.o base64.o resources.o checksum.o udif.o partition.o io.o zlib-1.2.3/libz.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o
CC=gcc
CFLAGS=-D_FILE_OFFSET_BITS=64

all: dmg

dmg: $(DMGOBJS) $(HFSOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) -o dmg

%.o:	%.c dmg.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm *.o
	rm dmg
