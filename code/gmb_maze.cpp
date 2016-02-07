#include "gmb_maze.h"

internal void pushPoint(struct pointStack *stack, struct point pt) {
  assert(stack);
  assert(stack->cur + 1 < stack->maxSize);
  stack->stack++;
  stack->cur++;
  *stack->stack = pt;
}

internal point popPoint(struct pointStack *stack) {
  assert(stack);
  assert(stack->cur >= 0);
  struct point t = *stack->stack;
  stack->stack--;
  stack->cur--;
  if (stack->cur < 0) {
    stack->stack = 0; // intentionally blowup later if we messed something up
    // assumption: the stack is used and emptied ONCE
  }
  return t;
}

internal void setMazeCell(struct maze *m, int x, int y, uint8 value) {
  assert((x < m->width && x >= 0) && (y < m->height && y >= 0));
  *(m->cells + ((y * m->width) + x)) = value;
}

internal uint8 getMazeCell(struct maze *m, int x, int y) {
  assert((x < m->width && x >= 0) && (y < m->height && y >= 0));
  return (*(m->cells + ((y * m->width) + x)));
}

internal bool32 mazeCellIsEmpty(struct maze *m, int x, int y) {
  if ((x < m->width && x >= 0) && (y < m->height && y >= 0)) {
    if (getMazeCell(m, x, y) == 0) {
      return 1;
    }
  }
  return 0;
}
