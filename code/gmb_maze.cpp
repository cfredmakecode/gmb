#include "gmb.h"
#include "malloc.h"
#include "stdlib.h"
#include "time.h"

internal void renderMaze(struct maze *m, memory_arena *arena) {
  // sized in pixels. for the resulting image
  const int pixelsEdgeLength = 64;
  const int wallWidth = pixelsEdgeLength / 16;

  if (!m->image) {
    // setup the "framebuffer" we're going to render
    m->image = (framebuffer *)PushStruct(arena, framebuffer, 1);
    m->image->height = m->height * pixelsEdgeLength;
    m->image->width = m->width * pixelsEdgeLength;

    // alloc enough room for each pixel of each cell
    m->image->pixels =
        (void *)PushBytes(arena, m->image->height * m->image->width * 4);
  }

  // somewhere to temporarily blit pixels of individual maze cells as
  // they're assembled. a better design would be to blit maze cells directly to
  // the target
  framebuffer cell = {0};
  cell.height = pixelsEdgeLength;
  cell.width = pixelsEdgeLength;
  // TODO(caf): allocate this in temporary memory, not the normal
  // permanent arena
  cell.pixels = alloca(cell.height * cell.width * 4);

  for (int mazey = 0; mazey < m->height; mazey++) {
    for (int mazex = 0; mazex < m->width; mazex++) {
      for (int pixelsy = 0; pixelsy < pixelsEdgeLength; pixelsy++) {
        for (int pixelsx = 0; pixelsx < pixelsEdgeLength; pixelsx++) {
          uint32 toSet = 0x0000000;
          if (getMazeCell(m, mazex, mazey) & downDoor) {
            if ((pixelsy >= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth) ||
                (pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth)) {
              toSet = 0x00FFFFFF;
            }
          }
          if (getMazeCell(m, mazex, mazey) & upDoor) {
            if ((pixelsy <= wallWidth && pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth) ||
                (pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth)) {
              toSet = 0x00FFFFFF;
            }
          }
          if (getMazeCell(m, mazex, mazey) & rightDoor) {
            if ((pixelsx >= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth) ||
                (pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth)) {
              toSet = 0x00FFFFFF;
            }
          }
          if (getMazeCell(m, mazex, mazey) & leftDoor) {
            if ((pixelsx <= wallWidth && pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth) ||
                (pixelsx >= wallWidth &&
                 pixelsx <= pixelsEdgeLength - 1 - wallWidth &&
                 pixelsy >= wallWidth &&
                 pixelsy <= pixelsEdgeLength - 1 - wallWidth)) {
              toSet = 0x00FFFFFF;
            }
          }

          *((uint32 *)cell.pixels + (pixelsy * pixelsEdgeLength) + pixelsx) =
              toSet;
        }
      }
      gmbCopyBitmapOffset(
          &cell, 0, 0, cell.width, cell.height, m->image, (mazex * cell.width),
          (m->image->height - cell.height) - (mazey * cell.height));
    }
  }
  return;
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
