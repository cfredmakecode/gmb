#include "gmb.h"

#include "win32_gmb.h"

#include "stdio.h"
#include "windows.h"

// we need to keep track of which "sides" of a maze's block
// have been travelled open during the generation process
#define upDoor 1
#define downDoor upDoor << 1
#define leftDoor downDoor << 1
#define rightDoor leftDoor << 1

// when building a maze we need a place to keep our in-progress worked block
// positions
#define MAXWORKINGPOINTS 512

void pushPoint(struct pointStack *stack, struct point pt);
point popPoint(struct pointStack *stack);

void pushPoint(struct pointStack *stack, struct point pt) {
  assert(stack);
  assert(stack->cur + 1 < stack->maxSize);
  stack->stack++;
  stack->cur++;
  *stack->stack = pt;
}

point popPoint(struct pointStack *stack) {
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

extern "C" __declspec(dllexport) GMBMAINLOOP(gmbMainLoop) {
  // note(caf): we rely on the fact that we expect pre-zeroed memory buffers
  // from the platform layer quite a bit. for example, right here.
  if (!state->isInitialized) {
    // init main arena
    state->arena.size = state->memory->permanentBytes - sizeof(gmbstate);
    state->arena.memory =
        (uint32 *)((uint8 *)state->memory->permanent + sizeof(gmbstate));
    state->ticks = 0;
    state->fontBitmap =
        *gmbLoadBitmap(state, &state->arena, (char *)"W:\\gmb\\data\\font.bmp");
    // gmbInitFontBitmap(state);

    // setup our maze once
    state->Maze.width = 32;
    state->Maze.height = 16;
    state->Maze.cells = (uint8 *)PushBytes(
        &state->arena, sizeof(uint8) * state->Maze.width * state->Maze.height);

    for (int i = 0; i < 256; i++) {
      // state->maze[i] |= 1 << (rand() % 4);
    }

    state->pts.maxSize = MAXWORKINGPOINTS;
    state->pts.stack =
        (point *)PushBytes(&state->arena, sizeof(point) * MAXWORKINGPOINTS);

    // pick random point at which to start
    srand(2341);
    point cur = {(rand() + 32768) % state->Maze.width,
                 (rand() + 32768) % state->Maze.height};
    pushPoint(&state->pts, cur);

    struct maze m = state->Maze; // only so we don't have to type th etnire
    // damn thing every time
    struct keepTrackOfDirectionThing {
      point p;
      uint8 direction;
    };
    // until all are popped
    while (state->pts.cur > 0) {
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
        // TODO figure out which direction we came from
        // which is the above inverted. somehow
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
        pushPoint(&state->pts, cur);
      } else {
        cur = popPoint(&state->pts);
      }
    }

    state->isInitialized = true;
  }
  // gmbDrawWeirdTexture(state, fb);
  char t[16];
  sprintf_s(t, sizeof(t), "%2.1f MS", msElapsedSinceLast);
  gmbDrawText(state, fb,
              (char *)"THIS IS ARBITRARY TEXT PRINTED FROM BITMAP FONT TILES!",
              10, 400);
  // and some floaty moving updatey text
  // gmbDrawText(state, fb, t, 0, 0);
  // sprintf_s(t, sizeof(t), "%u", state->ticks);
  // gmbDrawText(state, fb, t, state->ticks % (fb->width - 50),
  //             state->ticks % (fb->height - 50));
  // gmbDrawText(state, fb, (char *)"TICKS", state->ticks % (fb->width - 50),
  //             (state->ticks % (fb->height - 50)) + 11);
  // gmbCopyBitmap(state, &state->fontBitmap, fb);
  ++state->ticks;

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
  // done

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

  // MAZE TEST

  // state->maze[54] |= upDoor;
  // state->maze[71] |= downDoor;
  // state->maze[88] |= leftDoor;
  // state->maze[105] |= rightDoor;

  framebuffer cell = {0};
  cell.height = 16;
  cell.width = 16;
  cell.pixels = (void *)PushBytes(&state->arena, cell.height * cell.width * 4);

  struct maze m = state->Maze;

  for (int y = 0; y < m.height; y++) {
    for (int x = 0; x < m.width; x++) {
      // slowly as possible clear to 0 first
      for (int i = 0; i < 256; i++) {
        if ((getMazeCell(&m, x, y) & upDoor && upMask[i] == 1) ||
            (getMazeCell(&m, x, y) & downDoor && dnMask[i] == 1) ||
            (getMazeCell(&m, x, y) & leftDoor && lMask[i] == 1) ||
            (getMazeCell(&m, x, y) & rightDoor && rMask[i] == 1)) {
          *((uint32 *)cell.pixels + i) = 0xFFFFFFFF;
        } else {
          *((uint32 *)cell.pixels + i) = 0;
        }
      }
      gmbCopyBitmapOffset(state, &cell, 0, 0, 16, 16, fb, (x * 16) + 100,
                          (fb->height - (y * 16) + -300), 16, 16);
    }
  }
}

// note(caf): DEBUG ONLY
internal framebuffer *gmbLoadBitmap(gmbstate *state, memory_arena *arena,
                                    char *filename) {
  void *readIntoMem = state->DEBUGPlatformReadEntireFile(filename);
  bitmap *b = (bitmap *)readIntoMem;
  assert(b->Compression == 3);
  assert(b->Height > 0);
  assert(b->BitsPerPixel == 32);
  assert(b->AlphaMask == 0);
  assert(b->SizeOfBitmap < arena->size - arena->curOffset);

  // find the first bit that is set for each of the masks
  int redOffsetShift = findLeastBitSet(b->RedMask);
  int blueOffsetShift = findLeastBitSet(b->BlueMask);
  int greenOffsetShift = findLeastBitSet(b->GreenMask);
  int alphaOffsetShift = findLeastBitSet(b->AlphaMask);

  // important(caf): assumes 4 bytes per pixel always!
  framebuffer *f = (framebuffer *)PushStruct(arena, framebuffer, 1);
  assert(f);
  f->width = b->Width;
  f->height = b->Height;
  f->stride = b->BitsPerPixel / 8;
  uint32 a, bl, g, r;
  uint32 *src = (uint32 *)((uint8 *)b + b->BitmapOffset);
  uint32 *dest = (uint32 *)PushBytes(arena, b->SizeOfBitmap);
  assert(dest);
  f->pixels = (void *)dest;

  // copy the bitmap pixels into memory
  // note(caf): assumes target bitmap is top-down and source is bottom up
  // start walking source pixels at the beginning of the last row
  for (int y = b->Height - 1; y >= 0; y--) {
    for (int x = 0; x < b->Width; x++) {
      uint32 *s = src + (y * b->Width) + x;
      // note(caf): we automatically truncate to the bits we want by casting to
      // uint8 first
      a = (uint32)(uint8)(*s >> alphaOffsetShift);
      bl = (uint32)(uint8)(*s >> blueOffsetShift);
      g = (uint32)(uint8)(*s >> greenOffsetShift);
      r = (uint32)(uint8)(*s >> redOffsetShift);
      // note(caf): assumes target/internal bitmap format ABGR
      *dest = uint32(a << 24 | bl << 16 | g << 8 | r << 0);
      dest++;
    }
  }

  if (readIntoMem) {
    state->DEBUGPlatformFreeFile(readIntoMem);
  }
  return f;
}

// note(caf): x,y is top left corner of where to start text
internal void gmbDrawText(gmbstate *state, framebuffer *dest, char *text, int x,
                          int y) {
  char *characters =
      (char
           *)"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:;.,()!?=-+%*/{}<>'\"`_^\\| ";
  const int charWidth = 8;
  const int charHeight = 11;
  // const int pxBetweenLines = 10;
  // calc the size of our texture atlas. todo(caf): setup asset management
  int atlasWidth = state->fontBitmap.width / charWidth;
  // int atlasHeight = state->fontBitmap.height / charHeight;
  assert(text);
  uint32 currentPosX = x;
  uint32 currentPosY = y;
  // size_t len = strlen(text);
  for (char *i = text; *i != '\0'; i++) {
    // for each letter in passed text, match the tile offset manually mapped
    // to our bitmap font tiles already setup
    for (int j = 0; j < strlen(characters); j++) {
      if (*i == characters[j]) {
        gmbCopyBitmapOffset(state, &state->fontBitmap,
                            (j % atlasWidth) * charWidth,
                            (j / atlasWidth) * charHeight, 8, 11, dest,
                            currentPosX, currentPosY, 8, 11);
        currentPosX += charWidth;
        break;
      }
    }
  }
}

// NOTE(caf): does not do any scaling yet
internal void gmbCopyBitmapOffset(gmbstate *state, framebuffer *src, int sx,
                                  int sy, int swidth, int sheight,
                                  framebuffer *dest, int dx, int dy, int dwidth,
                                  int dheight) {
  assert(swidth == dwidth);
  assert(sheight == dheight);
  assert(src->width >= sx + swidth);
  assert(src->height >= sy + sheight);
  assert(dest->width >= dx + dwidth);
  assert(dest->height >= dy + dheight);

  uint32 *spixel = (uint32 *)src->pixels + (sy * src->width) + sx;
  uint32 *dpixel = (uint32 *)dest->pixels + (dy * dest->width) + dx;
  for (int y = 0; y < sheight; ++y) {
    for (int x = 0; x < swidth; ++x) {
      // uint32 r1 = sy * src->width;
      // uint32 r2 = sx + x;
      // uint32 r3 = y * src->width;
      // uint32 r4 = r1 + r2 + r3;
      spixel =
          (uint32 *)src->pixels + (sy * src->width) + sx + (y * src->width) + x;
      // uint32 h2 = dy * dest->width;
      // uint32 h3 = dx + x;
      // uint32 h4 = y * dest->width;
      // uint32 h5 = blah2 + blah3 + blah4;
      dpixel = (uint32 *)dest->pixels + (dy * dest->width) + dx +
               (y * dest->width) + x;
      *dpixel = *spixel;
    }
  }
}

// note(caf): needs to have a way to choose areas to copy to/from, needs to
// handle clipping,
internal void gmbCopyBitmap(gmbstate *state, framebuffer *source,
                            framebuffer *dest) {
  uint32 *spixel = (uint32 *)source->pixels;
  uint32 *dpixel = (uint32 *)dest->pixels;
  for (int y = 0; y < source->height; ++y) {
    dpixel = (uint32 *)dest->pixels;
    dpixel += (dest->width * y);
    for (int x = 0; x < source->width; ++x) {
      *dpixel = *spixel;
      spixel++;
      dpixel++;
    }
  }
}

internal void gmbDrawWeirdTexture(gmbstate *state, framebuffer *fb) {
  uint32 *pixel = (uint32 *)fb->pixels;
  for (int y = 0; y < fb->height; ++y) {
    for (int x = 0; x < fb->width; ++x) {
      *pixel = (uint8)(x + y + state->ticks) |
               (uint8)((x / 24) + state->ticks) << 8 | (uint8)y << 16;
      pixel++;
    }
  }
}

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
