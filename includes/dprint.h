/*
* debugsup.h - sort of from ReactOS/Wine.
*/

#ifndef _DEBUG_H_
#define _DEBUG_H_

#ifdef _WIN32
#ifdef ERROR
#undef ERROR
#endif
#endif

#define DBGFLTR_MISC		0x8
#define DBGFLTR_TRACE		0x7
#define DBGFLTR_INFO		0x6
#define DBGFLTR_DPRINT		0x5
#define DBGFLTR_WARN		0x4
#define DBGFLTR_ERR		0x3
#define DBGFLTR_FATAL		0x2
#define DBGFLTR_RELEASE		0x1

#define debug_printf    printf

#define UNIMPLEMENTED    debug_printf"WARNING:  %s at %s:%d is UNIMPLEMENTED!\n",__FUNCTION__,__FILE__,__LINE__);
#ifdef _WIN32
#define FATAL(fmt, ...)  debug_printf("(%s:%d) FATAL ERROR (Aborting): " fmt, __FILE__, __LINE__, ##__VA_ARGS__), system("pause"), exit(-1);
#else
#define FATAL(fmt, ...)  debug_printf("(%s:%d) FATAL ERROR (Aborting): " fmt, __FILE__, __LINE__, ##__VA_ARGS__), exit(-1);
#endif
#define ERR(fmt, ...)    debug_printf("(%s:%d) ERROR: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define WARN(fmt, ...)   debug_printf("(%s:%d) WARNING: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define TRACE(fmt, ...)  debug_printf("(%s:%d) TRACE: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define INFO(fmt, ...)   debug_printf("(%s:%d) INFO: " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define DPRINT(fmt, ...) debug_printf("(%s:%d) " fmt, __FILE__, __LINE__, ##__VA_ARGS__)
#define STATUS(fmt, ...) debug_printf(fmt,  ##__VA_ARGS__)
#define ERROR	ERR
#endif
