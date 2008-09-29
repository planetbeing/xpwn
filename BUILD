INSTRUCTIONS FOR BUILDING XPWN
------------------------------

These are very basic instructions on how to build xpwn related projects, they
are tailored to Debian based systems. They are not meant to be a substitute
for experience in programming in GNU/Linux environments, but it should be a
good starting point.

1. Install a basic build environment (compilers, etc.):

	sudo apt-get install build-essential

2. Install some prerequisites libraries required by xpwn:

	sudo apt-get install libcrypt-dev libz-dev libbz2-dev3 libusb-dev

3. Install cmake. It is recommended you download and build it from the
official cmake website, since versions >= 2.6.0 are recommended.

	wget http://www.cmake.org/files/v2.6/cmake-2.6.2.tar.gz
	tar zxvf cmake-2.6.2.tar.gz
	cd cmake-2.6.2
	./configure
	make
	sudo make install

Now you are ready to build xpwn. It is highly recommended that you build
out-of-source (that is, the build products are not placed into the same
folders as the sources). This is much neater and cleaning up is as simple as
deleting the build products folder.

Assuming xpwn sources are in ~/xpwn:

4. Create a build folder

	cd ~
	mkdir build
	cd build

5. Create Makefiles

	cmake ~/xpwn

6. Build

	make

7. Package

	make package

BUILDING USEFUL LIBRARIES
-------------------------

These command-lines can be substituted in for step 6. The products are in the
subfolders (make package will not include them).

xpwn library (for IPSW generation)

	make libXPwn.a

Windows pwnmetheus library (for QuickPwn)

	make libpwnmetheus.dll

HELPFUL MAKEFILE GENERATION COMMAND-LINES
-----------------------------------------

These command-lines can be substituted in for step 5.

Add debugging symbols:

	cmake ~/xpwn -DCMAKE_C_FLAGS=-g

Try to only use static libraries:

	cmake ~/xpwn -DBUILD_STATIC=YES


CROSS-COMPILING
---------------

This is a complex and advanced topic, but it is possible with the appropriate
CMake toolchain files and properly configured build environment. I have
crossbuilt Windows, OS X, Linux x86, Linux x64, and iPhone binaries from one
Ubuntu machine. The source trees are properly configured for this task.

MORE HELP
---------

Consult the CMake documentation and wiki and look in the CMakeLists.txt files
for hints on how things are supposed to work.
