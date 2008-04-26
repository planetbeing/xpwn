README
======

This project was first conceived to manipulate Apple's software restore
packages (IPSWs) and hence much of it is geared specifically toward that
format. Useful tools to read and manipulate the internal data structures of
those files have been created to that end, and with minor changes, more
generality can be achieved in the general utility. An inexhaustive list of
such changes would be selectively enabling folder counts in HFS+, switching
between case sensitivity and non-sensitivity, and more fine-grained control
over the layout of created dmgs.

**THE CODE HEREIN SHOULD BE CONSIDERED HIGHLY EXPERIMENTAL**

Extensive testing have not been done, but comparatively simple tasks like
adding and removing files from a mostly contiguous filesystem are well
proven.

Please note that these tools and routines are currently only suitable to be
accessed by other programs that know what they're doing. I.e., doing
something you "shouldn't" be able to do, like removing non-existent files is
probably not a very good idea.

LICENSE
-------

This work is copyright 2008 planetbeing, all rights reserved. It will be
released under an a free software license in the near future. Until then, it
can only be used with explicit permission (which I probably have given anyone
who is reading this right now) and is made available only for educational
purposes.

USING
-----

The targets of the current repository are two command-line utilities that
demonstrate the usage of the library functions (except cmd_grow, which really
ought to be moved to catalog.c). To make compilation simpler, a complete,
unmodified copy of the zlib distribution is included. The dmg portion of the
code has dependencies on the HFS+ portion of the code

The makefile in the root folder will make both utilities.

### HFS+

	cd hfs
	make

### DMG

	cd dmg/zlib-1.2.3
	./configure
	make
	cd ..
	make

