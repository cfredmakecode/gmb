#ifndef GMB_PLATFORM_H
#define GMB_PLATFORM_H
#include "stdint.h"

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

typedef struct memory_arena {
  uint32 size;
  uint32 *memory;
  uint32 curOffset;
} memory_arena;

#define assert(thing)                                                          \
  if (!(thing)) {                                                              \
    char *blowup = 0;                                                          \
    *blowup = 'Y';                                                             \
  }

#define Megabytes(n) ((uint32)n * 1024 * 1024)
#define PushStruct(arena, type, count) PushBytes(arena, sizeof(type) * (count))

void *DEBUGPlatformReadEntireFile(char *filename);
void DEBUGPlatformFreeMemory(void *memory);
bool32 DEBUGPlatformWriteEntireFile(char *filename, uint32 bytes, void *memory);

int findLeastBitSet(int haystack);
// todo(caf): should I be using size_t for representing stuff like this?
void *PushBytes(memory_arena *arena, int bytes);

int findLeastBitSet(int haystack) {
  for (int ioff = 0; ioff < 32; ioff++) {
    if (haystack & 1 << ioff) {
      return ioff;
    }
  }
  return 0;
}

// very quick and naive arena allocation
void *PushBytes(memory_arena *arena, int bytes) {
  if (!arena) {
    return 0;
  }
  assert(arena->curOffset + bytes < arena->size);
  // bump the offset by the size and return a pointer to the value before
  // increasing it
  void *ret = (uint8 *)arena->memory + arena->curOffset;
  arena->curOffset += bytes;
  return ret;
}

#endif
