Welcome to XPwn!
----------------

The X is for "cross", because unlike PwnageTool, this utility has no
dependencies on proprietary, closed-source software and can potentially be
compiled and used on any platform.

This is a special proof-of-concept version available only on Linux,
compiled with static libraries to minimize potential issues (which is why the
executables are a bit on the heavy side).

Please note that the source will be released to the public in a few weeks,
though if you would like to see the source for some reason, you may contact me
individually at planetbeing@gmail.com. It's not polished yet, and I want to
retain close control over it for now.

What XPwn is
------------

A barebones pwnagetool implementation that is easily portable.

What XPwn is *NOT*
------------------

An easy-to-use tool suitable for beginners. While it is possible easy to use
user interfaces will be developed for it eventually, it's mostly meant to be
a toy for *nix geeks. Absolutely no support should be expected or will be
given.

XPwn is also NOT winpwn. Winpwn will have things like easy package management,
an actual, non-lame system of installing stuff. I've seen the code for it, and
it will be pretty awesome when finished. Those tasks are outside the scope of
XPwn.

Credits
-------

This utility is merely an implementation of Pwnage, which is the work of
roxfan, Turbo, wizdaz, bgm, and pumpkin. Those guys are the real heroes.

XPwn attempts to use all the same data files and patches as PwnageTool to
avoid duplication of present and future labor. I believe that wizdaz probably
put the most sweat into PwnageTool, and the pwnage ramdisk is the work of
Turbo.

XPwn on Linux would not have been possible without libibooter, which was
written by cmw, based on the Linux iPhone recovery driver written by geohot.

A special shout-out to cmw, who I have been helping with winpwn. He's put a
lot of hard work into winpwn, and should also be credited with doing some of
the initial exploratory work with the undocumented DMG format.

Usage
-----

There are two utilities in this package, as well as the InternalPackages and
FirmwareBundles folders from PwnageTool, and Turbo's autopwn ramdisk.

## xpwn

xpwn will use libibooter to bootstrap the autopwn ramdisk. This will patch
NOR so that unsigned IPSWs can subsequently be used. The vulnerability used
is only available in firmware version 1.1.4, so this step has to be done with
that version.

	./xpwn <input.ipsw> [-b <bootlogo.png>] [-r <recoverylogo.png>]

Specifying a boot logo and a recovery logo is optional. You can specify both,
or just one. If you do not specify a particular boot logo, the logo will
remain the same as the one you currently have.

The input IPSW should correspond with CURRENT version on the device you are
trying to jailbreak. NOT the one you want to upgrade to. The reason it is
necessary is to provide a kernel for the ramdisk to boot and to provide
template boot logos to replace.

Note that the input IPSW must have the same name as the one on Apple's
download site! That is, it will not be recognized if you have renamed it after
downloading it.

*Note that xpwn is not currently known to work for firmware other than 1.1.4.*

The boot and recovery logos need to be PNG formatted files that less than or
equal to 320x480 in dimension. Although automatic conversion will be attempted
for you, the preferred format is an ARGB PNG with 8 bits per channel. *NOT* a
paletted RGB, and an alpha channel must be present *NOT* binary transparency.

If you save in PNG-24 and have at least one semi-transparent (not fully
transparent) pixel in your file, you ought to be in good shape.

It is safe to use xpwn multiple times consecutively, and that method can be
used to swap boot logos without restoring.

A restore with a non-customized IPSW will undo what xpwn did (the NOR will be
reflashed with Apple's image that does have signature checking)

## ipsw

ipsw is a more complex tool to generate custom IPSWs that you can restore
after using xpwn (or any other pwnage-based utility). This is important, since
that's how the jailbreak actually occurs.

	./ipsw <input.ipsw> <output.ipsw> [-b <bootimage.png>] [-nobbupdate] \
		[-r <recoveryimage.png>] [-e "<action to exclude>"] \
		[[-unlock] [-use39] [-use46] [-cleanup] \
		-3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] \
		<path/to/merge1> <path/to/merge2>...

Yes, I know, confusing syntax. The first two options are the IPSW you want to
modify, and where you want to save the modified IPSW respectively. -b and -r
have the same semantics and requirements as for xpwn. You can also specify
actions to exclude from the "FilesystemPatches" section of the Info.plist
for your particular IPSW (in FirmwareBundles/).

The most common use of the '-e' flag is to disable automatic activation, i.e.
'-e "Phone Activation"'. Note that the double-quotes are necessary.

-nobbupdate disables Apple's baseband upgrade program from running during
the restore. However, bbupdate must be enabled for unlocking with BootNeuter.

-unlock, -use39, -use46, -cleanup, -3, and -4 are valid only if you merge the
BootNeuter package. These provide instructions to BootNeuter (which provides
unlocking for iPhones). If you choose to use BootNeuter, you must specify the
location where the 3.9 and 4.9 bootloader can be found with the -3 and -4
options. These cannot be included with xpwn due to copyright restrictions.

-unlock specifies that you wish BootNeuter to unlock the phone (if it is not
already unlocked). -use39 and -use46 instructs BootNeuter to either upgrade
or downgrade your bootloader (if it is not already on the version you choose).
-cleanup instructs BootNeuter to delete itself off of the iPhone after it is
complete. If you do not specify -cleanup, BootNeuter will be accessible via
SpringBoard.

The last options are for directories to merge into the root filesystem of your
device. The included bundles can be merged by specifying something like
"bundles/Installer.bundle/files". Notice the "files" part must be specified.
It is also perfectly possible to set up your own files to merge.

/Applications/Installer.app/Installer will be given special setuid
permissions. All files that have the format /Applications/XXX.app/XXX will be
given execute permissions. All files in /sbin, /bin, /usr/bin, /usr/sbin,
/usr/libexec, /usr/local/bin, /usr/local/sbin, /usr/local/libexec will also be
given execute permissions. Special permissions are also given to BootNeuter.
Everything else will be non-executable, so a special LaunchDaemon task may need
to be constructed to properly set up your custom apps. Generally, however,
those permissions are already sufficient.

Told you it was a mess.

### Examples

Jailbreaking iPod 1.1.4:

	./ipsw iPod1,1_1.1.4_4A102_Restore.ipsw custom.ipsw \
		bundles/Installer.bundle/files

Jailbreaking iPhone 1.1.4:

	./ipsw iPhone1,1_1.1.4_4A102_Restore.ipsw custom.ipsw \
		-e "Phone Activation" bundles/Installer.bundle/files

Jailbreaking, activating, and unlocking iPhone 1.1.4:

	./ipsw iPhone1,1_1.1.4_4A102_Restore.ipsw custom.ipsw \
		-unlock -cleanup -3 bl39.bin -4 bl46.bin \
		bundles/Installer.bundle/files \
		bundles/BootNeuter.bundle/files \
		bundles/YoutubeActivation.bundle/files

Technical notes
---------------

Both xpwn and ipsw load the entire contents of the IPSW into memory before
manipulating it. This is especially useful for ipsw, because it allows all the
necessary transformations to be done without writing the intermediate steps to
disk and slowing the process down. ipsw is hence even faster than the Mac
pwnagetool.

However, hefty virtual memory requirements are necessary: 170 MB for xpwn and
500 MB for ipsw. Most modern computers should have that much to spare. Not all
of it needs to be free physical, as memory is accessed in a sequential manner
so thrashing should be kept to a minimum. In the worst case, it should be
equivalent to just writing intermediate results to disk. In essence, virtual
memory is used as an intelligent cache.

On the other hand, this also means that devices such as the iPhone itself
cannot run these utilities without modification. The necessary modifications
are actually relatively simple. Instead of using an AbstractFile backed by
memory, an AbstractFile backed by a physical file can be used again. Contact
me if this functionality is desired.

## Libraries used

- bsdiff
- libibooter
- libbzip2
- libcrypto (from OpenSSL)
- libpng
- libusb
- libz

These are all statically compiled in, but it should give you a good idea of
the program's dependencies.
