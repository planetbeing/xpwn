@echo off

set TESTS_DIR=..\tests
set SRC_DIR=..\src

call make_clean.bat

copy sources_test sources
copy %TESTS_DIR%\testlibusb.c .
copy %SRC_DIR%\usb.h .
copy %SRC_DIR%\*.rc .

build

call make_clean.bat
