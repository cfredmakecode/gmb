#include "gmb_platform.h"
#include "stdio.h"
#include "windows.h"

#define global static
#define internal static

// assumes 32-bit fb always
typedef struct framebuffer {
  int height;
  int width;
  int stride; // if the distance between horizontal lines is different than
              // simply (width * 4 bytes)
  void *pixels;
} framebuffer;

// assume 16-bit depth
typedef struct soundbuffer {
  int hz;
  int bufLen;
  uint16 *samples;
} soundbuffer;

typedef struct inputbuffer {
} inputbuffer;

typedef struct gmbstate {
  gmbmemory *memory; // REMOVEME after testing (caf)
  memory_arena arena;
  uint32 ticks;
  bool32 isInitialized;
  framebuffer fontBitmap;
} gmbstate;

#pragma pack(push, 1)
typedef struct bitmap {
  WORD FileType;      /* File type, always 4D42h ("BM") */
  DWORD FileSize;     /* Size of the file in bytes */
  WORD Reserved1;     /* Always 0 */
  WORD Reserved2;     /* Always 0 */
  DWORD BitmapOffset; /* Starting position of image data in bytes */
  DWORD Size;         /* Size of this header in bytes */
  LONG Width;         /* Image width in pixels */
  LONG Height;        /* Image height in pixels */
  WORD Planes;        /* Number of color planes */
  WORD BitsPerPixel;  /* Number of bits per pixel */
  /* Fields added for Windows 3.x follow this line */
  DWORD Compression;     /* Compression methods used */
  DWORD SizeOfBitmap;    /* Size of bitmap in bytes */
  LONG HorzResolution;   /* Horizontal resolution in pixels per meter */
  LONG VertResolution;   /* Vertical resolution in pixels per meter */
  DWORD ColorsUsed;      /* Number of colors in the image */
  DWORD ColorsImportant; /* Minimum number of important colors */
  DWORD RedMask;         /* Mask identifying bits of red component */
  DWORD GreenMask;       /* Mask identifying bits of green component */
  DWORD BlueMask;        /* Mask identifying bits of blue component */
  DWORD AlphaMask;       /* Mask identifying bits of alpha component */
  // note(caf): this isn't a fully correct representation! we're ignoring
  // anything between these struct members and the bitmap offset / start of
  // pixel data. oh well
} bitmap;
#pragma pack(pop)

internal void gmbMainLoop(gmbmemory *memory, framebuffer *fb,
                          real32 msElapsedSinceLast);
// internal void gmbDrawInfo(char *text, framebuffer *fb);
internal void gmbDrawWeirdTexture(gmbstate *state, framebuffer *fb);
internal void gmbInitFontBitmap(gmbstate *state);
internal void gmbCopyBitmap(gmbstate *state, framebuffer *source,
                            framebuffer *dest);
internal framebuffer *gmbLoadBitmap(memory_arena *arena, char *filename);
internal void gmbCopyBitmapOffset(gmbstate *state, framebuffer *src, int sx,
                                  int sy, int swidth, int sheight,
                                  framebuffer *dest, int dx, int dy, int dwidth,
                                  int dheight);
internal void gmbDrawText(gmbstate *state, framebuffer *dest, char *text, int x,
                          int y);

internal void gmbMainLoop(gmbmemory *memory, framebuffer *fb,
                          real32 msElapsedSinceLast) {
  assert(memory->permanentBytes >= sizeof(gmbstate) + sizeof(memory_arena));
  gmbstate *state = (gmbstate *)memory->permanent;
  // note(caf): we rely on the fact that we expect pre-zeroed memory buffers
  // from
  // the platform layer quite a bit. for example, right here.
  if (!state->isInitialized) {
    // init main arena
    state->arena.size = memory->permanentBytes - sizeof(gmbstate);
    state->arena.memory =
        (uint32 *)((uint8 *)memory->permanent + sizeof(gmbstate));
    state->memory = memory;
    state->ticks = 0;
    state->fontBitmap =
        *gmbLoadBitmap(&state->arena, (char *)"W:\\gmb\\data\\font.bmp");
    // gmbInitFontBitmap(state);
    state->isInitialized = true;
  }
  gmbDrawWeirdTexture(state, fb);
  char t[16];
  sprintf(t, "%2.1f MS", msElapsedSinceLast);
  gmbDrawText(state, fb, t, 0, 0);
  gmbDrawText(state, fb,
              (char *)"THIS IS ARBITRARY TEXT PRINTED FROM BITMAP FONT TILES!",
              10, 400);
  // gmbCopyBitmap(state, &state->fontBitmap, fb);
  ++state->ticks;
}

// note(caf): DEBUG ONLY
internal framebuffer *gmbLoadBitmap(memory_arena *arena, char *filename) {
  void *readIntoMem = DEBUGPlatformReadEntireFile(filename);
  bitmap *b = (bitmap *)readIntoMem;
  assert(b->Compression == 3);
  assert(b->Height > 0);
  assert(b->BitsPerPixel = 32);
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
    DEBUGPlatformFreeMemory(readIntoMem);
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
  const int pxBetweenLines = 10;
  // calc the size of our texture atlas. todo(caf): setup asset management
  int atlasWidth = state->fontBitmap.width / charWidth;
  int atlasHeight = state->fontBitmap.height / charHeight;
  // for (int y = 0; y < atlasHeight; y++) {
  //   for (int x = 0; x < atlasWidth; x++) {
  //     gmbCopyBitmapOffset(
  //         state, &state->fontBitmap, x * charWidth, y * charHeight, 8, 11,
  //         dest,
  //         200 + (x * (charWidth + 10)), 200 + (y * (charHeight + 10)), 8,
  //         11);
  //   }
  // }
  assert(text);
  uint32 currentPosX = x;
  uint32 currentPosY = y;
  uint32 len = strlen(text);
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
  // uint32 currentPosX = x;
  // uint32 currentPosY = y;
  // for (int i = 0; i < 61; i++, currentPosX += charWidth) {
  //   gmbCopyBitmapOffset(state, &state->fontBitmap, (i % atlasWidth) *
  //   charWidth,
  //                       (i / atlasWidth) * charHeight, 8, 11, dest,
  //                       currentPosX,
  //                       currentPosY, 8, 11);
  // }
}

//
// woff = 8
// hoff = 11
// wtiles = 10
// htiles = 8
//
//  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
// 10, 11, 12, 13, 14, 15, 16, 17, 18, 19,
// 20, 21, 22, 23, 24, 25, 26, 27, 28, 29,
// 30, 31, 32, 33, 34, 35, 36, 37, 38, 39,
// 40, 41, 42, 43, 44, 45, 46, 47, 48, 49,
// 50, 51, 52, 53, 54, 55, 56, 57, 58, 59,
// 60, 61, 62, 63, 64, 65, 66, 67, 68, 69,
// 70, 71, 72, 73, 74, 75, 76, 77, 78, 79,
//
// tile 33
// x = (tile%wtiles) * woff
// y = (tile/wtiles) * hoff

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
