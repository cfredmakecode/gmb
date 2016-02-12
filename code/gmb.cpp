#include "gmb.h"
#include "gmb_maze.cpp"
#include "gmb_vec.h"

#include "stdio.h"
#include "stdlib.h"
#include "string.h"

global framebuffer mazeImage;
global vec2 position;

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
    state->Maze = initMaze(&state->arena, 128, 128);
    mazeImage = renderMaze(state, &state->Maze, &state->arena);

    position.x = 0.0f;
    position.y = 0.0f;
    state->isInitialized = true;
  }
  if (input->space.endedDown) {
    state->Maze = initMaze(&state->arena, 128, 128);
    mazeImage = renderMaze(state, &state->Maze, &state->arena);
  }
  if (input->down.endedDown) {
    position.y += 10.0f;
  }
  if (input->up.endedDown) {
    position.y += -10.0f;
  }
  if (input->left.endedDown) {
    position.x += -10.0f;
  }
  if (input->right.endedDown) {
    position.x += 10.0f;
  }
  // gmbDrawWeirdTexture(state, fb);
  gmbCopyBitmapOffset(state, &mazeImage, 0, 0, mazeImage.width,
                      mazeImage.height, fb, -50, -50);

  // TODO(CAF): i fucked up something to do with copying pixels to the first
  // 200ish? cols of the framebuffer

  char t[16];

  // TODO(CAF): crash if src x or y is > src width/height
  gmbCopyBitmapOffset(state, &state->fontBitmap, 0, 0, state->fontBitmap.width,
                      state->fontBitmap.height, fb, (int)position.x,
                      (int)position.y);
  gmbDrawText(state, fb,
              (char *)"THIS IS ARBITRARY TEXT PRINTED FROM BITMAP FONT TILES!",
              100, 400);
  // and some floaty moving updatey text
  sprintf_s(t, sizeof(t), "%u", state->ticks);
  gmbDrawText(state, fb, t, state->ticks % (fb->width - 50),
              state->ticks % (fb->height - 50));
  gmbDrawText(state, fb, (char *)"TICKS", state->ticks % (fb->width - 50),
              (state->ticks % (fb->height - 50)) + 11);
  // gmbCopyBitmap(state, &state->fontBitmap, fb);
  // gmbCopyBitmap(state, &mazeImage, fb);

  sprintf_s(t, sizeof(t), "%2.1f MS", msElapsedSinceLast);
  gmbDrawText(state, fb, t, 0, 0);
  ++state->ticks;
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
                            currentPosX, currentPosY);
        currentPosX += charWidth;
        break;
      }
    }
  }
}

// NOTE(caf): does not do any scaling yet
internal void gmbCopyBitmapOffset(gmbstate *state, framebuffer *src, int sx,
                                  int sy, int swidth, int sheight,
                                  framebuffer *dest, int dx, int dy) {
  // assert(swidth == dwidth);
  // assert(sheight == dheight);
  // assert(src->width >= sx + swidth);
  // assert(src->height >= sy + sheight);
  // assert(dest->width >= dx + dwidth);
  // assert(dest->height >= dy + dheight);

  uint32 *spixel; //= (uint32 *)src->pixels + (sy * src->width) + sx;
  uint32 *dpixel; //= (uint32 *)dest->pixels + (dy * dest->width) + dx;

  // calculate the effective areas of the source we'll iterate over
  // src's start y would be too low (starting below dest's start y)
  if (dy < 0) {
    sy += -dy;
    dy = 0;
  }
  // src's start x would be too far left (starting left dest's start x)
  if (dx < 0) {
    sx += -dx;
    dx = 0;
  }

  // src would be too tall to fit into dest
  if (sy + sheight > dy + sheight) {
    // source image will need less rows blit to dest
    sheight = ((dy + sheight) - sy);
  }

  if (sx + swidth > dx + swidth) {
    // source image will need less cols blit to dest
    swidth = ((dx + swidth) - sx);
  }

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

internal inline int findLeastBitSet(int haystack) {
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
