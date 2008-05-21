HFSPLUSOBJS=btree.o catalog.o extents.o flatfile.o rawfile.o volume.o utility.o hfs.o fastunicodecompare.o hfslib.o
COMMONOBJS=../common/abstractfile.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -O3
INCLUDES=-I../includes

all: hfsplus

hfsplus: $(HFSPLUSOBJS) $(COMMONOBJS)
	$(CC) $(CFLAGS) $(HFSPLUSOBJS) $(COMMONOBJS) -o hfsplus

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@

clean:
	-rm *.o
	-rm hfsplus
	-rm hfsplus.exe

