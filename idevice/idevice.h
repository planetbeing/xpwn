#ifndef IDEVICE_H
#define IDEVICE_H

typedef void (*LogMessageCallback) (const char * Message);
 
extern LogMessageCallback logCallback;
 
#if __STDC_VERSION__ < 199901L
# if __GNUC__ >= 2
# define __func__ __FUNCTION__
# else
# define __func__ "<unknown>"
# endif
#endif
 
#define XLOG(level, format, ...) Log(level, __FILE__, __LINE__, __func__, format, ## __VA_ARGS__)
void Log(int level, const char* file, unsigned int line, const char* function, const char* format, ...);
void SetLogCallback(LogMessageCallback callback);
void SetLogLevel(int logLevel);

#ifdef WIN32
#include <windows.h>
#define sleep(x) Sleep(x * 1000)
#define msleep(x) Sleep(x)
#define usleep(x) Sleep(x / 1000)
#endif 

int LoadWindowsDLL();
int initWindowsPrivateFunctions();

#endif
