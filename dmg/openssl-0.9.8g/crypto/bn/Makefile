#
# OpenSSL/crypto/bn/Makefile
#

DIR=	bn
TOP=	../..
CC=	cc
CPP=    $(CC) -E
INCLUDES= -I.. -I$(TOP) -I../../include
CFLAG=-g
MAKEFILE=	Makefile
AR=		ar r

BN_ASM=		bn_asm.o
# or use
#BN_ASM=	bn86-elf.o

CFLAGS= $(INCLUDES) $(CFLAG)
ASFLAGS= $(INCLUDES) $(ASFLAG)
AFLAGS= $(ASFLAGS)

GENERAL=Makefile
TEST=bntest.c exptest.c
APPS=

LIB=$(TOP)/libcrypto.a
LIBSRC=	bn_add.c bn_div.c bn_exp.c bn_lib.c bn_ctx.c bn_mul.c bn_mod.c \
	bn_print.c bn_rand.c bn_shift.c bn_word.c bn_blind.c \
	bn_kron.c bn_sqrt.c bn_gcd.c bn_prime.c bn_err.c bn_sqr.c bn_asm.c \
	bn_recp.c bn_mont.c bn_mpi.c bn_exp2.c bn_gf2m.c bn_nist.c \
	bn_depr.c bn_const.c

LIBOBJ=	bn_add.o bn_div.o bn_exp.o bn_lib.o bn_ctx.o bn_mul.o bn_mod.o \
	bn_print.o bn_rand.o bn_shift.o bn_word.o bn_blind.o \
	bn_kron.o bn_sqrt.o bn_gcd.o bn_prime.o bn_err.o bn_sqr.o $(BN_ASM) \
	bn_recp.o bn_mont.o bn_mpi.o bn_exp2.o bn_gf2m.o bn_nist.o \
	bn_depr.o bn_const.o

SRC= $(LIBSRC)

EXHEADER= bn.h
HEADER=	bn_lcl.h bn_prime.h $(EXHEADER)

ALL=    $(GENERAL) $(SRC) $(HEADER)

top:
	(cd ../..; $(MAKE) DIRS=crypto SDIRS=$(DIR) sub_all)

all:	lib

bn_prime.h: bn_prime.pl
	$(PERL) bn_prime.pl >bn_prime.h

divtest: divtest.c ../../libcrypto.a
	cc -I../../include divtest.c -o divtest ../../libcrypto.a

bnbug: bnbug.c ../../libcrypto.a top
	cc -g -I../../include bnbug.c -o bnbug ../../libcrypto.a

lib:	$(LIBOBJ)
	$(AR) $(LIB) $(LIBOBJ)
	$(RANLIB) $(LIB) || echo Never mind.
	@touch lib

# ELF
bn86-elf.s:	asm/bn-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) bn-586.pl elf $(CFLAGS) > ../$@)
co86-elf.s:	asm/co-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) co-586.pl elf $(CFLAGS) > ../$@)
# COFF
bn86-cof.s: asm/bn-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) bn-586.pl coff $(CFLAGS) > ../$@)
co86-cof.s: asm/co-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) co-586.pl coff $(CFLAGS) > ../$@)
# a.out
bn86-out.s: asm/bn-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) bn-586.pl a.out $(CFLAGS) > ../$@)
co86-out.s: asm/co-586.pl ../perlasm/x86asm.pl
	(cd asm; $(PERL) co-586.pl a.out $(CFLAGS) > ../$@)

sparcv8.o:	asm/sparcv8.S
	$(CC) $(CFLAGS) -c asm/sparcv8.S
sparcv8plus.o:	asm/sparcv8plus.S
	$(CC) $(CFLAGS) -c asm/sparcv8plus.S

bn-mips3.o:	asm/mips3.s
	@if [ "$(CC)" = "gcc" ]; then \
		ABI=`expr "$(CFLAGS)" : ".*-mabi=\([n3264]*\)"` && \
		as -$$ABI -O -o $@ asm/mips3.s; \
	else	$(CC) -c $(CFLAGS) -o $@ asm/mips3.s; fi

x86_64-gcc.o:	asm/x86_64-gcc.c
	$(CC) $(CFLAGS) -c -o $@ asm/x86_64-gcc.c

bn-ia64.s:	asm/ia64.S
	$(CC) $(CFLAGS) -E asm/ia64.S > $@

# GNU assembler fails to compile PA-RISC2 modules, insist on calling
# vendor assembler...
pa-risc2W.o: asm/pa-risc2W.s
	/usr/ccs/bin/as -o pa-risc2W.o asm/pa-risc2W.s
pa-risc2.o: asm/pa-risc2.s
	/usr/ccs/bin/as -o pa-risc2.o asm/pa-risc2.s

# ppc - AIX, Linux, MacOS X...
linux_ppc32.s: asm/ppc.pl;	$(PERL) $< $@
linux_ppc64.s: asm/ppc.pl;	$(PERL) $< $@
aix_ppc32.s: asm/ppc.pl;	$(PERL) asm/ppc.pl $@
aix_ppc64.s: asm/ppc.pl;	$(PERL) asm/ppc.pl $@
osx_ppc32.s: asm/ppc.pl;	$(PERL) $< $@

files:
	$(PERL) $(TOP)/util/files.pl Makefile >> $(TOP)/MINFO

links:
	@$(PERL) $(TOP)/util/mklink.pl ../../include/openssl $(EXHEADER)
	@$(PERL) $(TOP)/util/mklink.pl ../../test $(TEST)
	@$(PERL) $(TOP)/util/mklink.pl ../../apps $(APPS)

install:
	@[ -n "$(INSTALLTOP)" ] # should be set by top Makefile...
	@headerlist="$(EXHEADER)"; for i in $$headerlist ; \
	do  \
	(cp $$i $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i; \
	chmod 644 $(INSTALL_PREFIX)$(INSTALLTOP)/include/openssl/$$i ); \
	done;

exptest:
	rm -f exptest
	gcc -I../../include -g2 -ggdb -o exptest exptest.c ../../libcrypto.a

div:
	rm -f a.out
	gcc -I.. -g div.c ../../libcrypto.a

tags:
	ctags $(SRC)

tests:

lint:
	lint -DLINT $(INCLUDES) $(SRC)>fluff

depend:
	@[ -n "$(MAKEDEPEND)" ] # should be set by upper Makefile...
	$(MAKEDEPEND) -- $(CFLAG) $(INCLUDES) $(DEPFLAG) -- $(PROGS) $(LIBSRC)

dclean:
	$(PERL) -pe 'if (/^# DO NOT DELETE THIS LINE/) {print; exit(0);}' $(MAKEFILE) >Makefile.new
	mv -f Makefile.new $(MAKEFILE)

clean:
	rm -f *.s *.o *.obj lib tags core .pure .nfs* *.old *.bak fluff

# DO NOT DELETE THIS LINE -- make depend depends on it.
