#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "idevice.h"

void defaultCallback(const char* Message) {
  printf(Message); fflush(stdout);
}

LogMessageCallback logCallback = defaultCallback;
int GlobalLogLevel = 0xFF;

void SetLogCallback(LogMessageCallback callback) {
	logCallback = callback;
}
 
void SetLogLevel(int logLevel) {
	GlobalLogLevel = logLevel;
}
 
void Log(int level, const char* file, unsigned int line, const char* function, const char* format, ...) {
	char mainBuffer[1024];
	char buffer[1024];
 
	if(level >= GlobalLogLevel) {
		return;
	}

	va_list args;
	va_start(args, format);
	vsnprintf(buffer, sizeof(buffer), format, args);
	va_end(args);

	switch(level) {
		case 0:
		case 1:
		strcpy(mainBuffer, buffer);
		break;
	default:
		snprintf(mainBuffer, sizeof(mainBuffer), "%s(%s:%d): %s\n", function, file, line, buffer);
	}
	logCallback(mainBuffer);
}

