#ifndef GMB_MAZE_H
#define GMB_MAZE_H
#include "gmb_basics.h"

// we need to keep track of which "sides" of a maze's block
// have been travelled open during the generation process
#define upDoor 1
#define downDoor upDoor << 1
#define leftDoor downDoor << 1
#define rightDoor leftDoor << 1

// when building a maze we need a place to keep our in-progress worked block
// positions
#define MAXWORKINGPOINTS 512

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

void pushPoint(struct pointStack *stack, struct point pt);
struct point popPoint(struct pointStack *stack);

#endif
