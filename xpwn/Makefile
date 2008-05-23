#
# Simple make for ibooter/libibooter
#

CFLAGS=-O3

LIBUSB=`if $(CC) ../win32test.c -o /dev/null 2>/dev/null ; then echo "../libusb-0.1.12/.libs/libusb.a"; else echo "../libusb-win32/libusb.a"; fi`
LIBUSBH=`if $(CC) win32test.c -o /dev/null 2>/dev/null ; then echo "-I./libusb-0.1.12/"; else echo "-I./libusb-win32/src/"; fi`
INC=-I./include/ $(LIBUSBH) -I../includes

SRCDIR=src
OBJDIR=build

OBJS=xpwn.o
LIBOBJS=libibooter.o
IPSWOBJS=../../ipsw-patch/bspatch.o ../../ipsw-patch/bzip2-1.0.5/libbz2.a ../../ipsw-patch/pwnutil.o ../../ipsw-patch/plist.o ../../ipsw-patch/outputstate.o ../../dmg/zlib-1.2.3/contrib/minizip/ioapi.o ../../dmg/zlib-1.2.3/contrib/minizip/unzip.o ../../dmg/zlib-1.2.3/contrib/minizip/zip.o
IMGOBJS=../../ipsw-patch/nor_files.o ../../ipsw-patch/8900.o ../../ipsw-patch/img2.o ../../ipsw-patch/ibootim.o ../../ipsw-patch/lzss.o ../../ipsw-patch/lzssfile.o ../../ipsw-patch/libpng-1.2.28/libpng.a ../../dmg/zlib-1.2.3/libz.a ../../dmg/openssl-0.9.8g/libcrypto.a
HFSOBJS=../../hfs/volume.o ../../hfs/btree.o ../../hfs/extents.o ../../hfs/rawfile.o ../../hfs/catalog.o ../../hfs/flatfile.o ../../hfs/utility.o ../../hfs/fastunicodecompare.o ../../hfs/hfslib.o
LIBRARIES=`if $(CC) ../win32test.c -o /dev/null 2>/dev/null ; then echo ""; else echo "-lgdi32"; fi` `if [ \`uname\` = "Darwin" ]; then echo "-framework CoreFoundation -framework IOKit"; else echo ""; fi`
COMMONOBJS=../../common/abstractfile.o

all: prepare xpwn

VPATH=$(SRCDIR)

$(OBJDIR)/%.o:	%.cpp
	$(CXX) $(CFLAGS) -c $< $(INC) -o $@

../ipsw-patch/pch:
	cd ../ipsw-patch; make

xpwn: $(OBJDIR)/$(OBJS) $(OBJDIR)/$(LIBOBJS) $(COMMONOBJS) ../ipsw-patch/pch libusb
	cd $(OBJDIR); $(CXX) $(LDFLAGS) $(IPSWOBJS) $(IMGOBJS) $(OBJS) $(LIBOBJS) $(LIBUSB) $(HFSOBJS) $(COMMONOBJS) $(LIBRARIES) -o $@

libusb:
	if $(CC) win32test.c -o /dev/null 2>/dev/null ; then cd libusb-0.1.12; ./configure; make; else cd libusb-win32; make; fi

prepare:
	mkdir -p $(OBJDIR)

clean:
	 rm -rf $(OBJDIR)

