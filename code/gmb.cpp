#include "gmb_platform.h"
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
  gmbmemory *memory;
  uint32 ticks;
  bool32 isInitialized;
  framebuffer fontBitmap;
} gmbstate;

internal void gmbMainLoop(gmbmemory *memory, framebuffer *fb,
                          uint32 usElapsedSinceLast);
// internal void gmbDrawInfo(char *text, framebuffer *fb);
internal void gmbDrawWeirdTexture(gmbstate *state, framebuffer *fb);
internal void gmbInitFontBitmap(gmbstate *state);
internal void gmbCopyBitmap(gmbstate *state, framebuffer *source,
                            framebuffer *dest);

internal void gmbMainLoop(gmbmemory *memory, framebuffer *fb,
                          uint32 usElapsedSinceLast) {
  assert(memory->permanentBytes >= sizeof(gmbstate));
  gmbstate *state = (gmbstate *)memory->permanent;
  // note(caf): we rely on the fact that we expect pre-zeroed memory buffers
  // from
  // the platform layer quite a bit. for example, right here.
  if (!state->isInitialized) {
    state->memory = memory;
    state->ticks = 0;
    gmbInitFontBitmap(state);
    state->isInitialized = true;
  }
  gmbDrawWeirdTexture(state, fb);
  gmbCopyBitmap(state, &state->fontBitmap, fb);
  ++state->ticks;
}

internal void gmbInitFontBitmap(gmbstate *state) {
  void *f = DEBUGPlatformReadEntireFile((char *)"W:/gmb/data/font.bmp");
  char *characters =
      (char *)"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ:;.,()!?=-+%*/{}<>'\"`_^\\|";
#pragma pack(push, 1)
  struct bitmap {
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
    DWORD CSType;          /* Color space type */
    LONG RedX;             /* X coordinate of red endpoint */
    LONG RedY;             /* Y coordinate of red endpoint */
    LONG RedZ;             /* Z coordinate of red endpoint */
    LONG GreenX;           /* X coordinate of green endpoint */
    LONG GreenY;           /* Y coordinate of green endpoint */
    LONG GreenZ;           /* Z coordinate of green endpoint */
    LONG BlueX;            /* X coordinate of blue endpoint */
    LONG BlueY;            /* Y coordinate of blue endpoint */
    LONG BlueZ;            /* Z coordinate of blue endpoint */
    DWORD GammaRed;        /* Gamma red coordinate scale value */
    DWORD GammaGreen;      /* Gamma green coordinate scale value */
    DWORD GammaBlue;       /* Gamma blue coordinate scale value */
    void *pixels;
  };
#pragma pack(pop)
  bitmap *b = (bitmap *)f;
  // if (f) {
  //   DEBUGPlatformFreeMemory(f);
  // }
  // note(caf): DEBUG ONLY
  assert(b->SizeOfBitmap + sizeof(gmbstate) < state->memory->permanentBytes);
  state->fontBitmap.width = b->Width;
  state->fontBitmap.height = b->Height;
  state->fontBitmap.stride = b->BitsPerPixel / 8;
  // important(caf): assumes 4 bytes per pixel always,
  // assumes sizeof operator returns count in bytes
  // copy the bitmap pixels only into game memory
  uint32 *s = (uint32 *)((uint8 *)b + b->BitmapOffset);
  uint32 *d = (uint32 *)((uint8 *)state->memory->permanent + sizeof(gmbstate));
  for (int i = 0; i < b->SizeOfBitmap / 4; i++) {
    *d = *s;
    d++;
    s++;
  }
  state->fontBitmap.pixels =
      (void *)((uint8 *)state->memory->permanent + sizeof(gmbstate));
  // todo(caf)      : ditch the manual memory management like this,
  // switch to arena
  // const char *ptr =
  // "0123456789"; // ABCDEFGHIJKLMNOPQRSTUVWXYZ.?!:;, ";
  // while (*ptr != '\0') {
  // todo:
  // create quick and dirty text tileset to blit from uint8 *pixel =
  //     (uint8 *)state->fontBitmap->pixels;
  // ptr++;
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
