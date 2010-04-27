Welcome to XPwn!
----------------

The X is for "cross", because unlike PwnageTool, this utility has no
dependencies on proprietary, closed-source software and can potentially be
compiled and used on any platform.

This is a special proof-of-concept version available on any platform,
compiled with static libraries to minimize potential issues (which is why the
executables are a bit on the heavy side).

The source is released under the terms of the GNU General Public License
version 3. The full text of the license can be found in the file LICENSE. The
source code itself is available at: http://github.com/planetbeing/xpwn

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

Also, the new super-awesome bootrom exploit is courtesy of wizdaz.

MuscleNerd has put a lot of work into the 3G effort. The BootNeuter unlock
for first-generation iPhones packaged within is primarily his effort.

Thanks also go to gray and c1de0x for their RCE efforts. saurik is the author
of Cydia, included within. bugout was the lucky guy who did our first 3G tests.

Thanks to chris for his hardware wisdom, Zf for his French humor, and pytey
for the support on the serial stuff.

idevice's "ready for custom IPSW" art was graciously contributed by KinetiX

XPwn attempts to use all the same data files and patches as PwnageTool to
avoid duplication of present and future labor. I believe that wizdaz probably
put the most sweat into PwnageTool, and the pwnage ramdisk is the work of
Turbo.

XPwn on Linux would not have been possible without libibooter, which was
written by cmw, based on the Linux iPhone recovery driver written by geohot.

Dripwn was created by Munnerz, specifically for installing iDroid.

A special shout-out to cmw, who I have been helping with winpwn. He's put a
lot of hard work into winpwn, and should also be credited with doing some of
the initial exploratory work with the undocumented DMG format.

Usage
-----

There are three utilities in this package, as well as the bundles and
FirmwareBundles folders from PwnageTool, and Turbo's autopwn ramdisk.

## Overview

The general series of steps should be to use ipsw to create a custom IPSW with
the user's preferences (done once per custom ipsw required), then itunespwn
(done once per computer) so that future DFU restores will be made easier.
Finally, either dfu-util (Mac or Linux) or idevice (Windows) should be used as
necessary on the iPhone to perform the actual exploit necessary to allow it to
accept our code.

It is technically possible to skip itunespwn and just use idevice or skip
idevice and just use itunespwn, but I recommend doing both.

## ipsw

*NOTE: Important change for 2.0: (uncompressed) tarballs rather than paths are
now used for bundles*

ipsw is a more complex tool to generate custom IPSWs that you can restore
after using xpwn (or any other pwnage-based utility). This is important, since
that's how the jailbreak actually occurs.

	./ipsw <input.ipsw> <output.ipsw> [-b <bootimage.png>] [-nowipe] \
		[-bbupdate] [-s <disk0s1 size>]  [-r <recoveryimage.png>] \
		[-memory] [-e "<action to exclude>"] \
		[[-unlock] [-use39] [-use46] [-cleanup] \
		-3 <bootloader 3.9 file> -4 <bootloader 4.6 file>] \
		<package1.tar> <package2.tar>...


Yes, I know, confusing syntax. The first two options are the IPSW you want to
modify, and where you want to save the modified IPSW respectively. -b and -r
have the same semantics and requirements as for xpwn. You can also specify
actions to exclude from the "FilesystemPatches" section of the Info.plist
for your particular IPSW (in FirmwareBundles/).

The most common use of the '-e' flag is to disable automatic activation, i.e.
'-e "Phone Activation"'. Note that the double-quotes are necessary.

-s allows you to specify the size of the system partition. This value is
specified in megabytes (NOT mebibytes)

-memory allows you to specify that memory instead of temporary files should be
used whenever possible (no longer the default).

-nowipe disables Apple's wiping of the NAND (user data), before proceeding
with the restore. This allows the restore to happen much, much more quickly.

-bbupdate tells the restore ramdisk to attempt to upgrade your baseband. This
is disabled by default for unlock safety reasons.

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

The last options are for tar-files to merge. All permissions and ownership
will be preserved except for already directories that already exist. This is
to prevent accidental clobbering (we're guessing you don't really want to
alter permissions on existing directories). This behavior may change in the
future.

Told you it was a mess.

## itunespwn

This *Windows* utility will replace a file in your %APPDATA%\Apple Computer\
Device Support folder. Subsequently, if you place your phone into DFU mode
and iTunes recognizes it, Apple will automatically upload an exploit file onto
your phone that will allow it to accept custom firmware (until it is turned
off). This basically will allow you to restore any IPSW you want from that
version of iTunes (provided you connect your phone in DFU mode).

See the following section for detailed instructions on how to enter DFU mode.

Usage:

	itunespwn <custom.ipsw>

The custom.ipsw is needed for the exploited WTF that was generated during
IPSW generation.

## idevice

This utility replaces dfu-util for Windows, sidestepping the libusb
requirement and provides a more user-friendly way of guiding them through
DFU mode. Its arguments are analogous to dfu-util and more details can be
read in that section. The difference is that iTunes' libraries are used
rather than the non-proprietary dfu-util. Also, a user-friendly logo is
made to appear on the iPhone upon successful completion, so an unambiguous
cue can be given to the user that they are ready to use the IPSW they created.

Obviously, a CLI is by its very nature not very newbie friendly, so the
primary purpose of this utility is to serve a mock-up for GUI implementors.
All GUI implementors are *strongly encouraged* to reproduce this in their
applications.

Usage:

	idevice <custom.ipsw> <n82ap|m68ap|n45ap>


## dfu-util (Not recommended on Windows)

dfu-util is an utility adapted from OpenMoko that satisfies the "pwning" stage
of the process, that is, allowing the execution of our unsigned code. It
relies upon an exploit in the DFU mode of the iPhone/iPod touch bootrom. This
cannot be fixed by Apple on the current hardware revisions. If we can mess
with the device before iTunes sees it, we can have it load a WTF with
signature checking disabled with the exploit, and load an iBSS with signature
checking disabled over that WTF. iTunes will see the device as a regular
iPhone/iPod in recovery mode, and will happily send our custom firmware to it,
which will now be accepted.

YOU MUST COMPLETELY DISABLE iTUNES WITH TASK MANAGER OR EQUIVALENT BEFORE
PROCEEDING.

Only AFTERWARDS do you put your device into DFU mode. If you switch the order
of these steps, iTunes will be able to load software onto your device without
this vulnerability, rendering dfu-util useless.

AFTER you have disabled iTunes, iTunesHelper, etc., plug your device
into the computer. Shut down the device in the normal way if necessary
(Slide to shutdown). Hold down the Power and Home buttons simultaneously
and count slowly to ten. (You may need to push down on power an instant
before you push down on home). The iPhone will start. At around the time
you count to 6, the iPhone will shut down again. KEEP HOLDING BOTH
BUTTONS. Hold down both buttons until you reach 10. At this point,
release the power button ONLY.  Keep holding the stand-by button forever
(this may take up to two minutes). Note Windows: You will know when you
can stop holding the button when Windows notifies you via an audible
cue that a USB device has connected. Note Linux: In terms of Linux you
could do lsusb until it's seen. dfu-util is gradually being phased out
anyway. This is your device in DFU mode. The screen of the device will
remain completely powered off.

THEN, run dfu-util with the following syntax:

	sudo ./dfu-util <custom.ipsw> <n82ap|m68ap|n45ap>

Where n82ap = 3G iPhone, m68ap = First-generation iPhone, n45ap = iPod touch.
Note that you're using your CUSTOM IPSW for this stage, since we will need the
patched firmware, not the stock firmware. dfu-util will pick out the right
files from the ipsw and send them in the right order. If your screen powers on
and then turns white, then you know it worked. You can now restore with iTunes.

## xpwn *(DEPRECATED)*

If DFU mode is too complicated for you, and you have a first-generation phone,
you can still use the legacy xpwn ramdisk method on 1.1.4 to pwn your phone.
Then you can restore the custom IPSW without messing with DFU mode.

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


### Examples

Jailbreaking iPod 2.0:

	./ipsw iPod1,1_2.0_5A347.ipsw custom.ipsw \
		bundles/Cydia.tar

Jailbreaking iPhone 3G:

	./ipsw iPhone1,2_2.0_5A347.ipsw custom.ipsw \
		-e "Phone Activation" bundles/Cydia.tar

Jailbreaking, activating, and unlocking iPhone 2.0:

	./ipsw iPhone1,1_2.0_5A347.ipsw custom.ipsw \
		-unlock -cleanup -3 bl39.bin -4 bl46.bin \
		bundles/Cydia.tar \
		bundles/BootNeuter.tar \
		bundles/YoutubeActivation.tar

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

Linux Notes:
------------
Question: Is there a way to restore the iPhone from Linux?

Answer: There is currently no way to restore an IPSW directly from
Linux. The necessary reverse-engineering has already been done by
pumpkin, bushing and c1de0x, so that functionality will come in the
medium-term future.
