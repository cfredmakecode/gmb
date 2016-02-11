#ifndef GMB_MEMORY_H
#define GMB_MEMORY_H
#include "gmb_basics.h"

typedef struct memory_arena {
  uint32 size;
  uint32 *memory;
  uint32 curOffset;
} memory_arena;

internal void *PushBytes(memory_arena *arena, int bytes);
#define PushStruct(arena, type, count) PushBytes(arena, sizeof(type) * (count))
internal int findLeastBitSet(int haystack);

#endif
