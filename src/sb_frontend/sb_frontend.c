#include "sb_frontend.h"
#include <SDL3/SDL.h>
#include <stdio.h>

// NES Controller Bitmask
// Matches NES register layout: bit = button pressed
#define BTN_RIGHT 0x01
#define BTN_LEFT 0x02
#define BTN_DOWN 0x04
#define BTN_UP 0x08
#define BTN_START 0x10
#define BTN_SELECT 0x20
#define BTN_B 0x40
#define BTN_A 0x80

// Key Mapping
static const struct {
  SDL_Scancode sc;
  uint8_t btn;
} keymap[] = {
  { SDL_SCANCODE_W, BTN_UP },         { SDL_SCANCODE_A, BTN_LEFT },
  { SDL_SCANCODE_S, BTN_DOWN },       { SDL_SCANCODE_D, BTN_RIGHT },
  { SDL_SCANCODE_J, BTN_B },          { SDL_SCANCODE_K, BTN_A },
  { SDL_SCANCODE_SPACE, BTN_SELECT }, { SDL_SCANCODE_RETURN, BTN_START },
};

static uint8_t read_controller(void) {
  const bool* keys = SDL_GetKeyboardState(NULL);
  uint8_t btns = 0;
  for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
    if (keys[keymap[i].sc])
      btns |= keymap[i].btn;
  }
  return btns;
}

// Render
static void render_frame(SDL_Renderer* renderer, sb_nes_t* nes, int scale) {
  // Dark background
  SDL_SetRenderDrawColor(renderer, 0x1A, 0x1A, 0x1A, 0xFF);
  SDL_RenderClear(renderer);

  // NES screen area (blue placeholder for now)
  SDL_FRect screen_rect = {
    .x = 0,
    .y = 0,
    .w = 256.0f * scale,
    .h = 240.0f * scale,
  };
  SDL_SetRenderDrawColor(renderer, 0x10, 0x10, 0x30, 0xFF);
  SDL_RenderFillRect(renderer, &screen_rect);

  // Draw CPU state as colored bars
  // TODO: later PPU framebuffer replaces this entirely
  // Register display (colored rectangles at bottom):
  // A register bar
  float bar_w = (float)nes->cpu.a / 255.0f * screen_rect.w;
  SDL_FRect a_bar = { .x = 0, .y = screen_rect.h - 40, .w = bar_w, .h = 8 };
  SDL_SetRenderDrawColor(renderer, 0xFF, 0x33, 0x33, 0xFF);
  SDL_RenderFillRect(renderer, &a_bar);

  // X register bar
  bar_w = (float)nes->cpu.x / 255.0f * screen_rect.w;
  SDL_FRect x_bar = { .x = 0, .y = screen_rect.h - 28, .w = bar_w, .h = 8 };
  SDL_SetRenderDrawColor(renderer, 0x33, 0xFF, 0x33, 0xFF);
  SDL_RenderFillRect(renderer, &x_bar);

  // Y register bar
  bar_w = (float)nes->cpu.y / 255.0f * screen_rect.w;
  SDL_FRect y_bar = { .x = 0, .y = screen_rect.h - 16, .w = bar_w, .h = 8 };
  SDL_SetRenderDrawColor(renderer, 0x33, 0x33, 0xFF, 0xFF);
  SDL_RenderFillRect(renderer, &y_bar);

  SDL_RenderPresent(renderer);
}

int sb_frontend_run(sb_frontend_config_t* config) {
  // Init SDL3
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    return 1;
  }

  int w = 256 * config->window_scale;
  int h = 240 * config->window_scale;

  SDL_Window* window =
    SDL_CreateWindow("sb_nes — SiputBiru NES Emulator", w, h, SDL_WINDOW_RESIZABLE);

  if (!window) {
    fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
    SDL_Quit();
    return 1;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Init NES
  sb_nes_t nes;
  sb_nes_init(&nes);

  if (!sb_nes_load_rom(&nes, config->rom_path)) {
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 1;
  }

  // Main Loop
  bool quit = false;
  while (!quit) {
    // Process events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT)
        quit = true;
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
        quit = true;
    }

    // Read controller
    sb_nes_set_buttons(&nes, read_controller());

    // Run one frame
    sb_nes_frame(&nes);

    // Render
    render_frame(renderer, &nes, config->window_scale);

    // ~60 FPS cap
    SDL_Delay(16);
  }

  // Cleanup
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return 0;
}
