#ifndef GMB_H
#define GMB_H

#include "gmb_basics.h"
#include "gmb_maze.h"

typedef struct memory_arena {
  uint32 size;
  uint32 *memory;
  uint32 curOffset;
} memory_arena;

internal void *PushBytes(memory_arena *arena, int bytes);
#define PushStruct(arena, type, count) PushBytes(arena, sizeof(type) * (count))
internal int findLeastBitSet(int haystack);

typedef struct gmbmemory {
  void *permanent;
  uint32 permanentBytes;
  void *temporary;
  uint32 temporaryBytes;
} gmbmemory;

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

// yes, it's a huge honking waste of space to alloc 8 bytes per key
typedef struct gmbkey {
  uint8 id;
  bool32 endedDown;
  int32 count;
} gmbkey;

// note(caf): for ease of use, we'll map the keys represented by A-Z to their
// correct position
// as they would be in ASCII. so starting at 0x41 (65);
typedef struct inputbuffer {
  uint32 kbutton;

  gmbkey mouse1;
  gmbkey mouse2;
  gmbkey ignored1; // note(caf): MSDN explains this as Control-Break
                   // processing. does that mean ctrl-c?
  gmbkey mouse3;
  gmbkey mouse4;
  gmbkey mouse5;
  gmbkey ignored2;
  gmbkey backspace;
  gmbkey tab;
  gmbkey ignored3[3];
  // 0x0D
  gmbkey enter;
  gmbkey ignored4[2];
  // 0x10
  gmbkey shift;
  gmbkey ctrl;
  gmbkey alt;
  gmbkey pauseBreak;
  gmbkey capslock;
  gmbkey ignored5[6]; // bunch of IME buttons? be careful, msdn docs don't
                      // immediately make it obvious multiple table rows have
                      // the same ID
  gmbkey esc;
  gmbkey ignored6[4];
  // 0x20
  gmbkey space;

  gmbkey keys[0xFF];
} inputbuffer;

#define GMBPLATFORMREADENTIREFILE(name) void *name(char *filename)
typedef GMBPLATFORMREADENTIREFILE(gmb_platform_read_entire_file);
#define GMBPLATFORMFREEFILE(name) void name(void *memory)
typedef GMBPLATFORMFREEFILE(gmb_platform_free_file);
#define GMBPLATFORMWRITEENTIREFILE(name)                                       \
  bool32 name(char *filename, uint32 bytes, void *memory)
typedef GMBPLATFORMWRITEENTIREFILE(gmb_platform_write_entire_file);

typedef struct gmbstate {
  gmbmemory *memory;
  memory_arena arena;
  uint32 ticks;
  bool32 isInitialized;
  framebuffer fontBitmap;
  struct maze Maze;
  struct pointStack pts;
  // some function pointers callable into the platform layer
  gmb_platform_read_entire_file *DEBUGPlatformReadEntireFile;
  gmb_platform_free_file *DEBUGPlatformFreeFile;
  gmb_platform_write_entire_file *DEBUGPlatformWriteEntireFile;
} gmbstate;

#pragma pack(push, 1)
typedef struct bitmap {
  uint16 FileType;     /* File type, always 4D42h ("BM") */
  uint32 FileSize;     /* Size of the file in bytes */
  uint16 Reserved1;    /* Always 0 */
  uint16 Reserved2;    /* Always 0 */
  uint32 BitmapOffset; /* Starting position of image data in bytes */
  uint32 Size;         /* Size of this header in bytes */
  int32 Width;         /* Image width in pixels */
  int32 Height;        /* Image height in pixels */
  uint16 Planes;       /* Number of color planes */
  uint16 BitsPerPixel; /* Number of bits per pixel */
  /* Fields added for Windows 3.x follow this line */
  uint32 Compression;     /* Compression methods used */
  uint32 SizeOfBitmap;    /* Size of bitmap in bytes */
  int32 HorzResolution;   /* Horizontal resolution in pixels per meter */
  int32 VertResolution;   /* Vertical resolution in pixels per meter */
  uint32 ColorsUsed;      /* Number of colors in the image */
  uint32 ColorsImportant; /* Minimum number of important colors */
  uint32 RedMask;         /* Mask identifying bits of red component */
  uint32 GreenMask;       /* Mask identifying bits of green component */
  uint32 BlueMask;        /* Mask identifying bits of blue component */
  uint32 AlphaMask;       /* Mask identifying bits of alpha component */
  // note(caf): this isn't a fully correct representation! we're
  // ignoring
  // anything between these struct members and the bitmap offset / start
  // of
  // pixel data. oh well
} bitmap;

#pragma pack(pop)

#define GMBMAINLOOP(name)                                                      \
  void name(gmbstate *state, framebuffer *fb, real32 msElapsedSinceLast)
typedef GMBMAINLOOP(gmb_main_loop);
GMBMAINLOOP(gmbMainLoopStub) {}

// internal void gmbDrawInfo(char *text, framebuffer *fb);
internal void gmbDrawWeirdTexture(gmbstate *state, framebuffer *fb);
internal void gmbInitFontBitmap(gmbstate *state);
internal void gmbCopyBitmap(gmbstate *state, framebuffer *source,
                            framebuffer *dest);
internal framebuffer *gmbLoadBitmap(gmbstate *state, memory_arena *arena,
                                    char *filename);
internal void gmbCopyBitmapOffset(gmbstate *state, framebuffer *src, int sx,
                                  int sy, int swidth, int sheight,
                                  framebuffer *dest, int dx, int dy, int dwidth,
                                  int dheight);
internal void gmbDrawText(gmbstate *state, framebuffer *dest, char *text, int x,
                          int y);

#endif
