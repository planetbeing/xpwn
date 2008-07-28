#ifndef IDEVICE_H
#define IDEVICE_H

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x * 1000)
#define msleep(x) Sleep(x)
#define usleep(x) Sleep(x / 1000)
#endif 

int LoadWindowsDLL();
int initWindowsPrivateFunctions();

#endif
