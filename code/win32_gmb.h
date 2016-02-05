#ifndef GMB_PLATFORM_H
#define GMB_PLATFORM_H
#include "stdio.h"
#include "windows.h"

// note(caf): re-def'ing these to shut up clang-llvm
// it'd make more sense to figure out how to do it properly, oh well
#define global static
#define local_global static
#define internal static
typedef float real32;
//

typedef struct WIN32SCREENBUFFER {
  void *buf;
  int height;
  int width;
  int depth;
  BITMAPINFO info;
} WIN32SCREENBUFFER;
typedef struct WIN32WINDOWSIZE {
  int height;
  int width;
} WIN32WINDOWSIZE;

LRESULT CALLBACK gmbWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam);
// read it in the strongbad TGS voice
void errord(char *reason);
internal void Win32ResizeWindow(WIN32SCREENBUFFER *buf, int height, int width);
internal WIN32WINDOWSIZE Win32GetWindowSize(HWND window);
internal void Win32BlitScreen(HDC hdc, WIN32SCREENBUFFER *buf, int height,
                              int width);
internal void Win32DebugDrawFrameTime(HDC hdc, real32 elapsed, int ticks);

#endif