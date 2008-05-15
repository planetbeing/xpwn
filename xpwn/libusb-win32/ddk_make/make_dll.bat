@echo off

set SRC_DIR=..\src

call make_clean.bat

copy sources_dll sources

copy %SRC_DIR%\*.c .
copy ..\*.def .
copy %SRC_DIR%\*.h .
copy %SRC_DIR%\*.rc .
copy %SRC_DIR%\driver\driver_api.h .

build

call make_clean.bat

if exist libusb0.lib move libusb0.lib libusb.lib
