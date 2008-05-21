HFSPLUSOBJS=btree.o catalog.o extents.o flatfile.o rawfile.o volume.o utility.o hfs.o fastunicodecompare.o abstractfile.o hfslib.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -O3

all: hfsplus

hfsplus: $(HFSPLUSOBJS)
	$(CC) $(CFLAGS) $(HFSPLUSOBJS) -o hfsplus

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm *.o
	-rm hfsplus
	-rm hfsplus.exe

