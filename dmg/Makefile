DMGOBJS=dmg.o base64.o resources.o checksum.o udif.o partition.o io.o filevault.o dmgfile.o dmglib.o zlib-1.2.3/libz.a openssl-0.9.8g/libcrypto.a
HFSOBJS=../hfs/volume.o ../hfs/btree.o ../hfs/extents.o ../hfs/rawfile.o ../hfs/catalog.o ../hfs/flatfile.o ../hfs/utility.o ../hfs/fastunicodecompare.o
COMMONOBJS=../common/abstractfile.o
CFLAGS=-D_FILE_OFFSET_BITS=64 -DHAVE_CRYPT
LIBRARIES=`if $(CC) win32test.c -o /dev/null 2>/dev/null ; then echo ""; else echo "-lgdi32"; fi`
INCLUDES=-I../includes -Izlib-1.2.3 -Iopenssl-0.9.8g/include

all: dmg

dmg: $(DMGOBJS) $(HFSOBJS) $(COMMONOBJS)
	$(CC) $(CFLAGS) $(DMGOBJS) $(HFSOBJS) $(LIBRARIES) $(COMMONOBJS) -o dmg

%.o:	%.c
	$(CC) $(CFLAGS) $(INCLUDES) -c $< -o $@
	
zlib-1.2.3/Makefile:
	cd zlib-1.2.3; ./configure

zlib-1.2.3/libz.a:	zlib-1.2.3/Makefile
	cd zlib-1.2.3; make
	
openssl-0.9.8g/libcrypto.a:
	touch openssl-0.9.8g/Makefile
	cd openssl-0.9.8g/crypto; make

clean:
	-rm *.o
	-rm dmg
	-rm dmg.exe
