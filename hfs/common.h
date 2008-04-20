#ifndef COMMON_H
#define COMMON_H

#ifdef WIN32
#define fseeko fseeko64
#define ftello ftello64
#define mkdir(x, y) mkdir(x)
#endif

#define TRUE 1
#define FALSE 0

#define FLIPENDIAN(x) flipEndian((unsigned char *)(&(x)), sizeof(x))

#define IS_BIG_ENDIAN      0
#define IS_LITTLE_ENDIAN   1

#define TIME_OFFSET_FROM_UNIX 2082844800L
#define APPLE_TO_UNIX_TIME(x) ((x) - TIME_OFFSET_FROM_UNIX)
#define UNIX_TO_APPLE_TIME(x) ((x) + TIME_OFFSET_FROM_UNIX)

#define ASSERT(x, m) if(!(x)) { fflush(stdout); fprintf(stderr, "error: %s\n", m); fflush(stderr); exit(1); }

extern char endianness;

static inline void flipEndian(unsigned char* x, int length) {
  int i;
  unsigned char tmp;

  if(endianness == IS_BIG_ENDIAN) {
    return;
  } else {
    for(i = 0; i < (length / 2); i++) {
      tmp = x[i];
      x[i] = x[length - i - 1];
      x[length - i - 1] = tmp;
    }
  }
}

#endif
