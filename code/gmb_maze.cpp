#include "gmb_maze.h"
#include "gmb.h"
#include "stdlib.h"
#include "time.h"

internal framebuffer drawMaze(gmbstate *state, struct maze *m,
                              memory_arena *arena);

// TODO(caf): remove the requirement for *state, can't remember why it was
// initially added anyway
internal framebuffer drawMaze(gmbstate *state, struct maze *m,
                              memory_arena *arena) {
  // note(caf): this is naively just setting up what a maze cell
  // traversable from whichever direction will look like. multiple can/will
  // be OR'd together
  uint8 upMask[256] = {0};
  for (int y = 0; y < 14; y++) {
    for (int x = 2; x < 14; x++) {
      upMask[(y * 16) + x] = 1;
    }
  }

  uint8 dnMask[256] = {0};
  for (int y = 2; y < 16; y++) {
    for (int x = 2; x < 14; x++) {
      dnMask[(y * 16) + x] = 1;
    }
  }

  uint8 lMask[256] = {0};
  for (int y = 2; y < 14; y++) {
    for (int x = 0; x < 14; x++) {
      lMask[(y * 16) + x] = 1;
    }
  }

  uint8 rMask[256] = {0};
  for (int y = 2; y < 14; y++) {
    for (int x = 2; x < 16; x++) {
      rMask[(y * 16) + x] = 1;
    }
  }

  // setup somewhere to temporarily blit pixels of individual maze cells
  // better design would be to blit maze cells directly to the target obviously
  framebuffer cell = {0};
  cell.height = 16; // these are both sized in pixels, for the created image
  cell.width = 16;
  // TODO(caf): should allocate this in temporary memory, not the normal
  // permanent arena
  cell.pixels = (void *)PushBytes(arena, cell.height * cell.width * 4);

  // setup the "framebuffer" we're going to return as rendered
  framebuffer fb = {0};
  fb.height = m->height * cell.height;
  fb.width = m->width * cell.width;
  // alloc enough room for each pixel of each cell
  fb.pixels = (void *)PushBytes(arena, fb.height * fb.width * 4);

  for (int y = 0; y < m->height; y++) {
    for (int x = 0; x < m->width; x++) {
      // slowly as possible clear to 0 first
      for (int i = 0; i < 256; i++) {
        if ((getMazeCell(m, x, y) & upDoor && upMask[i] == 1) ||
            (getMazeCell(m, x, y) & downDoor && dnMask[i] == 1) ||
            (getMazeCell(m, x, y) & leftDoor && lMask[i] == 1) ||
            (getMazeCell(m, x, y) & rightDoor && rMask[i] == 1)) {
          *((uint32 *)cell.pixels + i) = 0xFFFFFFFF;
        } else {
          *((uint32 *)cell.pixels + i) = 0;
        }
      }
      gmbCopyBitmapOffset(state, &cell, 0, 0, 16, 16, &fb, (x * 16),
                          (fb.height - 16) - (y * 16));
    }
  }
  return fb;
}

internal struct maze initMaze(memory_arena *arena, int width, int height) {
  struct maze m = {0};
  m.width = width;
  m.height = height;
  m.cells = (uint8 *)PushBytes(arena, sizeof(uint8) * width * height);

  struct pointStack pts = {0};
  pts.maxSize = MAXWORKINGPOINTS;
  pts.stack = (point *)PushBytes(arena, sizeof(point) * pts.maxSize);

  // pick random point at which to start
  time_t tm;
  time(&tm);
  srand((uint32)tm);
  point cur = {(rand() + 32768) % m.width, (rand() + 32768) % m.height};
  pushPoint(&pts, cur);

  struct keepTrackOfDirectionThing {
    point p;
    uint8 direction;
  };

  // until all are popped
  while (pts.cur > 0) {
    struct keepTrackOfDirectionThing open[4] = {};
    int openCount = 0;
    if (mazeCellIsEmpty(&m, cur.x, cur.y + 1)) {
      // up is empty
      struct keepTrackOfDirectionThing p = {};
      p.p.x = cur.x;
      p.p.y = cur.y + 1;
      p.direction = upDoor;
      open[openCount] = p;
      openCount++;
    }
    if (mazeCellIsEmpty(&m, cur.x, cur.y - 1)) {
      // down is empty
      struct keepTrackOfDirectionThing p = {};
      p.p.x = cur.x;
      p.p.y = cur.y - 1;
      p.direction = downDoor;
      open[openCount] = p;
      openCount++;
    }
    if (mazeCellIsEmpty(&m, cur.x + 1, cur.y)) {
      // right is empty
      struct keepTrackOfDirectionThing p = {};
      p.p.x = cur.x + 1;
      p.p.y = cur.y;
      p.direction = rightDoor;
      open[openCount] = p;
      openCount++;
    }
    if (mazeCellIsEmpty(&m, cur.x - 1, cur.y)) {
      // left is empty
      struct keepTrackOfDirectionThing p = {};
      p.p.x = cur.x - 1;
      p.p.y = cur.y;
      p.direction = leftDoor;
      open[openCount] = p;
      openCount++;
    }
    if (openCount > 0) {
      point prev = cur;
      // randomly choose one to try next round
      struct keepTrackOfDirectionThing temp =
          open[(rand() + 32768) % openCount];
      cur = temp.p;

      setMazeCell(&m, prev.x, prev.y,
                  getMazeCell(&m, prev.x, prev.y) | temp.direction);

      // figure out which direction we came from
      uint8 from = 0;
      switch (temp.direction) {
      case upDoor:
        from = downDoor;
        break;
      case downDoor:
        from = upDoor;
        break;
      case rightDoor:
        from = leftDoor;
        break;
      case leftDoor:
        from = rightDoor;
        break;
      }
      setMazeCell(&m, cur.x, cur.y, getMazeCell(&m, cur.x, cur.y) | from);
      pushPoint(&pts, cur);
    } else {
      cur = popPoint(&pts);
    }
  }
  return m;
}

internal void pushPoint(struct pointStack *stack, struct point pt) {
  assert(stack);
  assert(stack->cur + 1 < stack->maxSize);
  stack->stack++;
  stack->cur++;
  *stack->stack = pt;
}

internal struct point popPoint(struct pointStack *stack) {
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
