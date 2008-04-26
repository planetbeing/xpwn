HFSPLUSOBJS=btree.o catalog.o extents.o flatfile.o rawfile.o volume.o utility.o hfs.o
CFLAGS=-D_FILE_OFFSET_BITS=64

all: hfsplus

hfsplus: $(HFSPLUSOBJS)
	$(CC) $(CFLAGS) $(HFSPLUSOBJS) -o hfsplus

%.o:	%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-rm *.o
	-rm hfsplus

