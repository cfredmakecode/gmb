#ifndef GMB_PLATFORM_H
#define GMB_PLATFORM_H
#include "stdint.h"

#define assert(thing) \
  if (!(thing)) {     \
    char *blowup = 0; \
    *blowup = 'Y';    \
  }

#define Megabytes(n) ((uint32)n * 1024 * 1024)

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

typedef struct gmbmemory {
  void *permanent;
  uint32 permanentBytes;
  void *temporary;
  uint32 temporaryBytes;
} gmbmemory;

void *DEBUGPlatformReadEntireFile(char *filename);
void DEBUGPlatformFreeMemory(void *memory);
bool32 DEBUGPlatformWriteEntireFile(char *filename, uint32 bytes, void *memory);

#endif
