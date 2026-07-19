#include "../sb_ppu/sb_ppu_palette.h"
#include "sb_frontend.h"

#define WIN32_LEAN_AND_MEAN
#include <stdio.h>
#include <windows.h>

// NES button bitmask
#define BTN_RIGHT 0x01
#define BTN_LEFT 0x02
#define BTN_DOWN 0x04
#define BTN_UP 0x08
#define BTN_START 0x10
#define BTN_SELECT 0x20
#define BTN_B 0x40
#define BTN_A 0x80

// Forward declarations
static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp);
static uint8_t read_controller(void);
static void render_frame(HWND hwnd, sb_nes_t* nes);

void sb_frontend_init(sb_frontend_config_t* config) {
  config->window_scale = 3;
  config->rom_path = NULL;
  config->frame_delay = 16;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
  switch (msg) {
  case WM_CLOSE:
    DestroyWindow(hwnd);
    return 0;
  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }
  return DefWindowProc(hwnd, msg, wp, lp);
}

static uint8_t read_controller(void) {
  uint8_t btns = 0;
  if (GetAsyncKeyState('W') & 0x8000)
    btns |= BTN_UP;
  if (GetAsyncKeyState('A') & 0x8000)
    btns |= BTN_LEFT;
  if (GetAsyncKeyState('S') & 0x8000)
    btns |= BTN_DOWN;
  if (GetAsyncKeyState('D') & 0x8000)
    btns |= BTN_RIGHT;
  if (GetAsyncKeyState('J') & 0x8000)
    btns |= BTN_B;
  if (GetAsyncKeyState('K') & 0x8000)
    btns |= BTN_A;
  if (GetAsyncKeyState(VK_SPACE) & 0x8000)
    btns |= BTN_SELECT;
  if (GetAsyncKeyState(VK_RETURN) & 0x8000)
    btns |= BTN_START;
  return btns;
}

static void render_frame(HWND hwnd, sb_nes_t* nes) {
  // Get current client area size
  RECT rect;
  GetClientRect(hwnd, &rect);
  int win_w = rect.right - rect.left;
  int win_h = rect.bottom - rect.top;

  // Convert PPU palette indices to ARGB pixels
  uint8_t* fb = sb_ppu_get_framebuffer(&nes->ppu);
  static uint32_t pixels[256 * 240];

  for (int i = 0; i < 256 * 240; i++)
    pixels[i] = sb_ppu_palette[fb[i] & 0x3F];

  // Blit to window using StretchDIBits
  BITMAPINFO bmi = { 0 };
  bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  bmi.bmiHeader.biWidth = 256;
  bmi.bmiHeader.biHeight = -240; // top-down DIB
  bmi.bmiHeader.biPlanes = 1;
  bmi.bmiHeader.biBitCount = 32;
  bmi.bmiHeader.biCompression = BI_RGB;

  HDC hdc = GetDC(hwnd);
  StretchDIBits(
    hdc,
    0,
    0,
    win_w,
    win_h, // dest rect (scaled to window)
    0,
    0,
    256,
    240, // src rect (full framebuffer)
    pixels,
    &bmi,
    DIB_RGB_COLORS,
    SRCCOPY
  );
  ReleaseDC(hwnd, hdc);
}

void sb_frontend_run(sb_nes_t* nes, sb_frontend_config_t* config) {
  // Register window class
  const char CLASS_NAME[] = "sb_nes_win32";

  WNDCLASS wc = { 0 };
  wc.lpfnWndProc = wndproc;
  wc.hInstance = GetModuleHandle(NULL);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = CLASS_NAME;
  wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);

  if (!RegisterClass(&wc)) {
    fprintf(stderr, "Failed to register window class\n");
    return;
  }

  // Create window
  int w = 256 * config->window_scale;
  int h = 240 * config->window_scale;

  RECT rect = { 0, 0, w, h };
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);

  HWND hwnd = CreateWindowEx(
    0,
    CLASS_NAME,
    "sb_nes — SiputBiru NES Emulator",
    WS_OVERLAPPEDWINDOW | WS_VISIBLE,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    rect.right - rect.left,
    rect.bottom - rect.top,
    NULL,
    NULL,
    wc.hInstance,
    NULL
  );

  if (!hwnd) {
    fprintf(stderr, "Failed to create window\n");
    return;
  }

  // Load ROM
  if (!sb_nes_load_rom(nes, config->rom_path)) {
    DestroyWindow(hwnd);
    return;
  }

  // Main loop
  BOOL running = TRUE;
  while (running) {
    // Process Windows messages
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
      if (msg.message == WM_QUIT) {
        running = FALSE;
        break;
      }
      TranslateMessage(&msg);
      DispatchMessage(&msg);
    }
    if (!running)
      break;

    // Escape quits
    if (GetAsyncKeyState(VK_ESCAPE) & 0x8000) {
      DestroyWindow(hwnd);
      continue; // let WM_DESTROY + WM_QUIT break the loop next iteration
    }

    // Read controller
    sb_nes_set_buttons(nes, read_controller());

    // Run one frame
    sb_nes_frame(nes);

    // Render
    render_frame(hwnd, nes);

    // ~60 FPS cap
    Sleep(config->frame_delay);
  }
}
