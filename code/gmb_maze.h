#ifndef GMB_MAZE_H
#define GMB_MAZE_H
#include "gmb_basics.h"
#include "gmb_memory.h"

// we need to keep track of which "sides" of a maze's block
// have been travelled open during the generation process
#define upDoor 1
#define downDoor upDoor << 1
#define leftDoor downDoor << 1
#define rightDoor leftDoor << 1

// when building a maze we need a place to keep our in-progress worked block
// positions
#define MAXWORKINGPOINTS 128 * 128

struct point {
  int x, y;
};

struct pointStack {
  int cur;
  struct point *stack;
  int maxSize;
};

struct maze {
  int height;
  int width;
  uint8 *cells;
};

internal void pushPoint(struct pointStack *stack, struct point pt);
internal struct point popPoint(struct pointStack *stack);
internal bool32 mazeCellIsEmpty(struct maze *m, int x, int y);
internal uint8 getMazeCell(struct maze *m, int x, int y);
internal void setMazeCell(struct maze *m, int x, int y, uint8 value);
internal struct maze initMaze(memory_arena *arena, int width, int height);

#endif
