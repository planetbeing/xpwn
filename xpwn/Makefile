#
# Simple make for ibooter/libibooter
#
CC=g++

CFLAGS=

LIBUSB=`if $(CC) ../win32test.c -o /dev/null 2>/dev/null ; then echo "../libusb-0.1.12/.libs/libusb.a"; else echo "../libusb-win32/libusb.a"; fi`
LIBUSBH=`if $(CC) win32test.c -o /dev/null 2>/dev/null ; then echo "-I./libusb-0.1.12/"; else echo "-I./libusb-win32/src/"; fi`
INC=-I./include/ $(LIBUSBH)

SRCDIR=src
OBJDIR=build

OBJS=xpwn.o
LIBOBJS=libibooter.o
IPSWOBJS=../../ipsw-patch/plist.o ../../ipsw-patch/outputstate.o ../../dmg/zlib-1.2.3/contrib/minizip/ioapi.o ../../dmg/zlib-1.2.3/contrib/minizip/unzip.o ../../dmg/zlib-1.2.3/contrib/minizip/zip.o
IMGOBJS=../../ipsw-patch/nor_files.o ../../ipsw-patch/8900.o ../../ipsw-patch/img2.o ../../ipsw-patch/ibootim.o ../../ipsw-patch/lzss.o ../../ipsw-patch/lzssfile.o ../../ipsw-patch/libpng-1.2.28/libpng.a
DMGOBJS=../../dmg/checksum.o ../../dmg/zlib-1.2.3/libz.a ../../dmg/openssl-0.9.8g/libcrypto.a
HFSOBJS=../../hfs/volume.o ../../hfs/btree.o ../../hfs/extents.o ../../hfs/rawfile.o ../../hfs/catalog.o ../../hfs/flatfile.o ../../hfs/utility.o ../../hfs/fastunicodecompare.o ../../hfs/abstractfile.o ../../hfs/hfslib.o
LIBRARIES=`if $(CC) ../win32test.c -o /dev/null 2>/dev/null ; then echo ""; else echo "-lgdi32"; fi` `if [ \`uname\` = "Darwin" ]; then echo "-framework CoreFoundation -framework IOKit"; else echo ""; fi`

all: prepare xpwn

VPATH=$(SRCDIR)

.cpp.o:
	$(CC) $(CFLAGS) -c $< $(INC) -o $(OBJDIR)/$@

xpwn: $(OBJS) $(LIBOBJS)
	cd $(OBJDIR); $(CC) $(LDFLAGS) $(IPSWOBJS) $(IMGOBJS) $^ $(LIBUSB) $(DMGOBJS) $(HFSOBJS) $(LIBRARIES) -o $@

prepare:
	mkdir -p $(OBJDIR)

clean:
	 rm -rf $(OBJDIR)

