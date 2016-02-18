#include "gmb.h"
#include "gmb_vec.h"

#include "stdio.h" // needed for sprintf_s
#include "win32_gmb.h"

GMBPLATFORMREADENTIREFILE(DEBUGPlatformReadEntireFile) {
  HANDLE f = CreateFile(filename, GENERIC_READ, FILE_SHARE_READ, 0,
                        OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
  if (f == INVALID_HANDLE_VALUE) {
    errord((char *)"DEBUGPlatformReadEntireFile");
    return 0;
  }
  LARGE_INTEGER size;
  if (!GetFileSizeEx(f, &size)) {
    errord((char *)"DEBUGPlatformReadEntireFile");
    return 0;
  }
  void *buf = VirtualAlloc(0, (DWORD)size.QuadPart, MEM_RESERVE | MEM_COMMIT,
                           PAGE_READWRITE);
  DWORD bytesRead;
  if (!ReadFile(f, buf, (DWORD)size.QuadPart, &bytesRead, 0)) {
    CloseHandle(f);
    VirtualFree(buf, 0, 0);
    errord((char *)"DEBUGPlatformReadEntireFile");
    return 0;
  }
  CloseHandle(f);
  if (!(size.QuadPart == bytesRead)) {
    VirtualFree(buf, 0, 0);
    errord((char *)"DEBUGPlatformReadEntireFile");
    return 0;
  }
  return buf;
}

GMBPLATFORMFREEFILE(DEBUGPlatformFreeFile) {
  if (memory) {
    VirtualFree(memory, 0, 0);
  }
}

GMBPLATFORMWRITEENTIREFILE(DEBUGPlatformWriteEntireFile) { return 0; }

typedef struct win32_gmbdll {
  gmb_main_loop *gmbMainLoop;
  HMODULE Handle;
  FILETIME lastCreationDate; // at the time of loading
} win32_gmbdll;

internal void win32_InitGmbDll(win32_gmbdll *dll) {
  // note(caf): we make a copy because otherwise visual studio's debugger holds
  // the dll and pdb open and we can't compile a new one over it
  if (!CopyFileA("\\build\\gmb.dll", "\\build\\gmb_copy.dll", 0)) {
    // todo(caf): handle errors
    return;
  }
  // this isn't great. we should deal with this differently so we don't have to
  // delete the original
  // DeleteFileA("\\build\\gmb.dll");
  dll->Handle = LoadLibraryA("\\build\\gmb_copy.dll");
  if (!dll->Handle) {
    dll->gmbMainLoop = &gmbMainLoopStub;
    errord((char *)"Failed to LoadLibraryA()");
  } else {
    gmb_main_loop *ml =
        (gmb_main_loop *)GetProcAddress(dll->Handle, "gmbMainLoop");
    if (!ml) {
      dll->gmbMainLoop = &gmbMainLoopStub;
      errord((char *)"Failed to GetProcAddress()");
    }
    dll->gmbMainLoop = ml;
    HANDLE lastDllHandle;
    lastDllHandle = CreateFileA("\\build\\gmb_copy.dll", 0, FILE_SHARE_WRITE, 0,
                                OPEN_EXISTING, 0, 0);
    if (lastDllHandle != INVALID_HANDLE_VALUE) {
    }
    // note(caf): we check the _modified_ date because silly me, creation date
    // is literally the time the file with that name first existed
    GetFileTime(lastDllHandle, 0, 0, &dll->lastCreationDate);
    CloseHandle(lastDllHandle);
  }
}

internal void win32_UnloadGmbDll(win32_gmbdll *dll) {
  assert(dll->Handle);
  FreeLibrary(dll->Handle);
}

internal const int bitDepth = 32;
internal const real32 targetMS = real32(1000.0 / 60.0);
global bool32 running = true;
global WIN32SCREENBUFFER screenBuffer = {0};
int WinMain(HINSTANCE instance, HINSTANCE previnstance, LPSTR cmdline,
            int cmdshow) {
  win32_gmbdll gmbDLL;
  win32_InitGmbDll(&gmbDLL);
  WNDCLASS tclass = {0};
  tclass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  tclass.lpfnWndProc = &gmbWindowProc;
  tclass.hInstance = 0;
  tclass.lpszClassName = "gmb_class_lol";
  tclass.hCursor = LoadCursorA(0, IDC_ARROW);

  ATOM windowClass = RegisterClassA(&tclass);
  if (windowClass == 0) {
    errord((char *)"RegisterClassA");
  }
  HWND window = CreateWindow("gmb_class_lol", "gmb window",
                             WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                             CW_USEDEFAULT, 960, 540, 0, 0, instance, 0);
  if (window == 0) {
    errord((char *)"CreateWindow");
  }
  WIN32WINDOWSIZE size = Win32GetWindowSize(window);
  Win32ResizeWindow(&screenBuffer, size.height, size.width);

// note(caf): use a specific starting address range so we can do some fancy
// stuff in debug mode
#ifdef WIN32_GMB_INTERNAL
  LPVOID StartAddress = (LPVOID)Tibibytes(2);
#else
  LPVOID StartAddress = 0;
#endif
  void *freshmemory = VirtualAlloc(StartAddress, Mibibytes(64),
                                   MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (!freshmemory) {
    errord((char *)"VirtualAlloc permanent mem");
  }
  gmbstate state = {0};
  gmbmemory memory = {0};
  state.memory = &memory;
  state.memory->permanent = freshmemory;
  state.memory->permanentBytes = Mibibytes(64);
  state.DEBUGPlatformReadEntireFile = &DEBUGPlatformReadEntireFile;
  state.DEBUGPlatformFreeFile = &DEBUGPlatformFreeFile;
  state.DEBUGPlatformWriteEntireFile = &DEBUGPlatformWriteEntireFile;

  MSG msg;
  HDC hdc = GetDC(window);
  int win32ticks = 0;
  LARGE_INTEGER lastpf;
  LARGE_INTEGER curpf;
  LARGE_INTEGER startFrame;
  QueryPerformanceCounter(&curpf);
  lastpf = curpf;
  startFrame = curpf;
  LARGE_INTEGER freq;
  QueryPerformanceFrequency(&freq);
  uint64 elapsed = curpf.QuadPart - lastpf.QuadPart;
  POINT p;
  real32 ms = 0;
  framebuffer fb = {0};
  inputbuffer ib = {0};
  // note(caf): to watch for live reloading our main dll
  uint64 t = GetTickCount64();
  while (running) {
    if (GetTickCount64() - 1000 > t) {
      t = GetTickCount64();
      DWORD att;
      att = GetFileAttributesA("\\build\\gmb.dll");
      if (att != INVALID_FILE_ATTRIBUTES) {
        HANDLE DllHandle;
        DllHandle = CreateFileA("\\build\\gmb.dll", 0, FILE_SHARE_WRITE, 0,
                                OPEN_EXISTING, 0, 0);
        if (DllHandle == INVALID_HANDLE_VALUE) {
          // TODO(caf): handle errors
        }
        FILETIME DllTime;
        GetFileTime(DllHandle, 0, 0, &DllTime);
        CloseHandle(DllHandle);
        if (DllTime.dwHighDateTime != gmbDLL.lastCreationDate.dwHighDateTime ||
            DllTime.dwLowDateTime != gmbDLL.lastCreationDate.dwLowDateTime) {
          // it existed and was newer, so unload our current copy and load the
          // new one
          win32_UnloadGmbDll(&gmbDLL);
          win32_InitGmbDll(&gmbDLL);
          // TODO(caf): use a lockfile to avoid the race condition of compiler
          // creating file but buffer not flushed when we try to load it
        }
      }
    }
    QueryPerformanceCounter(&startFrame);
    while (PeekMessageA(&msg, window, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT || msg.message == WM_CLOSE) {
        running = false;
      }
      if (msg.message == WM_KEYDOWN) {
        switch (msg.wParam) {
        case VK_RETURN:
          ib.enter.endedDown = true;
          break;
        case VK_SPACE:
          ib.space.endedDown = true;
          break;
        case VK_DOWN:
          ib.down.endedDown = true;
          break;
        case VK_UP:
          ib.up.endedDown = true;
          break;
        case VK_LEFT:
          ib.left.endedDown = true;
          break;
        case VK_RIGHT:
          ib.right.endedDown = true;
          break;
        default:
          break;
        }
      }
      TranslateMessage(&msg);
      DispatchMessageA(&msg);
    }
    fb.height = screenBuffer.height;
    fb.width = screenBuffer.width;
    fb.stride = 4;
    fb.pixels = screenBuffer.buf;
    GetCursorPos(&p);
    ScreenToClient(window, &p); // takes care of window/cursor offsets
    ib.mousex = p.x;
    ib.mousey = p.y;
    QueryPerformanceCounter(&curpf);
    elapsed = curpf.QuadPart - lastpf.QuadPart;
    lastpf = curpf;
    ms = (real32)((real32)elapsed / (real32)freq.QuadPart) * 1000;
    gmbDLL.gmbMainLoop(&state, &fb, &ib, ms); //(elapsed / freq.QuadPart));
    WIN32WINDOWSIZE size = Win32GetWindowSize(window);
    Win32BlitScreen(hdc, &screenBuffer, size.height, size.width);
    ++win32ticks;
    // QueryPerformanceCounter(&curpf);
    elapsed = curpf.QuadPart - startFrame.QuadPart;
    ms = (real32)((real32)elapsed / (real32)freq.QuadPart) * 1000;
    int msToSleep = int(targetMS) - (int)ms; // intentionally truncating to int
    if (msToSleep > 0) {
      Sleep(msToSleep);
    }
    while (ms < targetMS && ms > 0) {
      QueryPerformanceCounter(&curpf);
      elapsed = curpf.QuadPart - startFrame.QuadPart;
      ms = (real32)((real32)elapsed / (real32)freq.QuadPart) * 1000;
      Sleep(0); // do nothing sleep
    }
    ib = {0};
    // Win32DebugDrawFrameTime(hdc, ms, win32ticks);
  }
  win32_UnloadGmbDll(&gmbDLL);
  return 0;
}

// note(caf); expects elapsed to already be turned into ms
internal void Win32DebugDrawFrameTime(HDC hdc, real32 elapsedms, int ticks) {
  RECT pos = {0};
  // pos.top = 48;
  // pos.right = 96;
  POINT pt;
  GetCursorPos(&pt);
  char ms[256];
  sprintf_s(ms, sizeof(ms), "%2.1f ms\n", elapsedms);
  OutputDebugStringA(ms);
  int res = DrawText(hdc, ms, -1, &pos,
                     DT_BOTTOM | DT_NOCLIP | DT_NOPREFIX | DT_VCENTER);
  if (res == 0) {
    errord((char *)"DrawText");
  }
}

internal void Win32BlitScreen(HDC hdc, WIN32SCREENBUFFER *buf, int height,
                              int width) {
  int res =
      StretchDIBits(hdc, 0, 0, width, height, 0, 0, buf->width, buf->height,
                    buf->buf, &buf->info, DIB_RGB_COLORS, SRCCOPY);
  if (res == 0) {
    errord((char *)"StretchDIBits");
  }
}

internal void Win32ResizeWindow(WIN32SCREENBUFFER *buf, int height, int width) {
  if (buf->buf) {
    VirtualFree(buf->buf, 0, MEM_RELEASE);
  }
  // TODO(CAF): just always assume 32bit depth and stop caring?
  buf->depth = bitDepth;
  buf->height = height;
  buf->width = width;
  buf->info.bmiHeader.biSize = sizeof(buf->info.bmiHeader);
  buf->info.bmiHeader.biWidth = buf->width;
  buf->info.bmiHeader.biHeight = -buf->height;
  buf->info.bmiHeader.biPlanes = 1;
  buf->info.bmiHeader.biBitCount = (WORD)buf->depth;
  buf->info.bmiHeader.biCompression = BI_RGB;
  int size = buf->depth * buf->height * buf->width;
  buf->info.bmiHeader.biSizeImage = size;

  void *newbuf =
      (void *)VirtualAlloc(0, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (newbuf == 0) {
    errord((char *)"VirtualAlloc fbuffer");
  }
  buf->buf = newbuf;
}

LRESULT CALLBACK gmbWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam,
                               LPARAM lParam) {
  LRESULT res = 0;
  switch (uMsg) {
  case WM_QUIT:
  case WM_CLOSE:
    running = 0;
    break;
  case WM_KEYUP:
    switch (wParam) {
    case VK_ESCAPE:
    case 'Q':
      running = 0;
      break;
    }
  case WM_SIZE: {
    WIN32WINDOWSIZE size = Win32GetWindowSize(hwnd);
    if (size.height > 540) {
      size.height = 540;
    }
    if (size.width > 960) {
      size.width = 960;
    }
    Win32ResizeWindow(&screenBuffer, size.height, size.width);
    RECT r;
    GetWindowRect(hwnd, &r);
    SetWindowPos(hwnd, 0, r.left, r.top, size.width, size.height, 0);
    break;
  }
  default:
    res = DefWindowProc(hwnd, uMsg, wParam, lParam);
  }
  return res;
}

internal WIN32WINDOWSIZE Win32GetWindowSize(HWND window) {
  RECT rect;
  if (!GetWindowRect(window, &rect)) {
    errord((char *)"GetWindowRect");
  }
  WIN32WINDOWSIZE size = {0};
  size.width = rect.right - rect.left;
  size.height = rect.bottom - rect.top;
  return size;
}

// hacky and assumes caller doesn't send more than a few characters
void errord(char *reason) {
  char errtext[512];
  char *out;
  DWORD lerr = GetLastError();
  if (FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                         FORMAT_MESSAGE_FROM_SYSTEM,
                     0, lerr, 0, (LPTSTR)&out, 256, 0)) {
    sprintf_s(errtext, sizeof(errtext), "%s: error: %ld: %s", reason, lerr,
              out);
    MessageBoxA(0, errtext, 0, MB_OK);
  }
  assert(!*errtext);
  exit(-1);
}
