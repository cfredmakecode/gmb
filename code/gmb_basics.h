#ifndef GMB_BASICS_H
#define GMB_BASICS_H

#include "stdint.h"

#define global static
#define local_global static
#define internal static

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;
typedef int8_t int8;
typedef int16_t int16;
typedef int32_t int32;
typedef int64_t int64;

typedef int32 bool32;
typedef float real32;
typedef double real64;

#define assert(thing)                                                          \
  if (!(thing)) {                                                              \
    char *blowup = 0;                                                          \
    *blowup = 'Y';                                                             \
  }

#define Kibibytes(n) (n * 1024LL)
#define Mibibytes(n) Kibibytes(n * 1024LL)
#define Gibibytes(n) Mibibytes(n * 1024LL)
#define Tibibytes(n) Gibibytes(n * 1024LL)

#endif
