#include "sb_frontend.h"
#include "../sb_ppu/sb_ppu_palette.h"
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
  { SDL_SCANCODE_W, BTN_UP },     { SDL_SCANCODE_A, BTN_LEFT },  { SDL_SCANCODE_S, BTN_DOWN },
  { SDL_SCANCODE_D, BTN_RIGHT },  { SDL_SCANCODE_J, BTN_B },     { SDL_SCANCODE_K, BTN_A },
  { SDL_SCANCODE_U, BTN_SELECT }, { SDL_SCANCODE_I, BTN_START },
};

void sb_frontend_init(sb_frontend_config_t* config) {
  config->window_scale = 3;
  config->rom_path[0] = '\0';
  config->rom_path_set = false;
  config->frame_delay = 16;
  config->rom_loaded = false;
}

static uint8_t read_controller(void) {
  const bool* keys = SDL_GetKeyboardState(NULL);
  uint8_t btns = 0;
  for (size_t i = 0; i < sizeof(keymap) / sizeof(keymap[0]); i++) {
    if (keys[keymap[i].sc])
      btns |= keymap[i].btn;
  }
  return btns;
}

// Render: convert PPU framebuffer palette indices to RGB and blit via SDL texture.
static void render_frame(SDL_Renderer* renderer, sb_nes_t* nes, int scale) {
  static SDL_Texture* texture = NULL;
  static uint32_t pixels[256 * 240];

  if (!texture) {
    texture =
      SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, 256, 240);
  }

  uint8_t* fb = sb_ppu_get_framebuffer(&nes->ppu);
  for (int i = 0; i < 256 * 240; i++)
    pixels[i] = sb_ppu_palette[fb[i] & 0x3F];

  SDL_UpdateTexture(texture, NULL, pixels, 256 * sizeof(uint32_t));
  SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
  SDL_RenderClear(renderer);

  SDL_FRect dst = { .x = 0, .y = 0, .w = 256.0f * scale, .h = 240.0f * scale };
  SDL_RenderTexture(renderer, texture, NULL, &dst);
  SDL_RenderPresent(renderer);
}

static void file_dialog_callback(void* userdata, const char* const* file_list, int filter) {
  sb_frontend_config_t* config = (sb_frontend_config_t*)userdata;
  if (file_list && file_list[0]) {
    SDL_strlcpy(config->rom_path, file_list[0], sizeof(config->rom_path));
    config->rom_path_set = true;
    config->rom_loaded = false;
  }
}

void sb_frontend_run(sb_nes_t* nes, sb_frontend_config_t* config) {
  // Init SDL3
  if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
    fprintf(stderr, "SDL_Init error: %s\n", SDL_GetError());
    return;
  }

  int w = 256 * config->window_scale;
  int h = 240 * config->window_scale;

  SDL_Window* window =
    SDL_CreateWindow("sb_nes - SiputBiru NES Emulator", w, h, SDL_WINDOW_RESIZABLE);
  if (!window) {
    fprintf(stderr, "SDL_CreateWindow error: %s\n", SDL_GetError());
    SDL_Quit();
    return;
  }

  SDL_Renderer* renderer = SDL_CreateRenderer(window, NULL);
  if (!renderer) {
    fprintf(stderr, "SDL_CreateRenderer error: %s\n", SDL_GetError());
    SDL_DestroyWindow(window);
    SDL_Quit();
    return;
  }

  if (config->rom_path_set) {
    if (sb_nes_load_rom(nes, config->rom_path))
      config->rom_loaded = true;
  }

  // Main Loop
  bool quit = false;
  while (!quit) {
    int current_w, current_h;
    SDL_GetWindowSize(window, &current_w, &current_h);

    float btn_w = 140.0f;
    float btn_h = 45.0f;
    SDL_FRect button_rect = { .x = ((float)current_w - btn_w) / 2.0f,
                              .y = ((float)current_h - btn_h) / 2.0f,
                              .w = btn_w,
                              .h = btn_h };

    // Process events
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      if (event.type == SDL_EVENT_QUIT)
        quit = true;
      if (event.type == SDL_EVENT_KEY_DOWN && event.key.key == SDLK_ESCAPE)
        quit = true;

      if (!config->rom_loaded && event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        if (event.button.button == SDL_BUTTON_LEFT) {
          SDL_FPoint mouse_pos = { .x = event.button.x, .y = event.button.y };
          if (SDL_PointInRectFloat(&mouse_pos, &button_rect)) {
            const SDL_DialogFileFilter filters[] = { { "NES ROMs (*.nes)", "nes" } };
            SDL_ShowOpenFileDialog(file_dialog_callback, config, window, filters, 1, NULL, false);
          }
        }
      }
    }

    if (config->rom_path_set && !config->rom_loaded) {
      if (sb_nes_load_rom(nes, config->rom_path)) {
        config->rom_loaded = true;
      } else {
        config->rom_path[0] = '\0';
        config->rom_path_set = false;
      }
    }

    if (config->rom_loaded) {
      // Run the standard emulator screen execution block
      sb_nes_set_buttons(nes, read_controller());
      sb_nes_frame(nes);
      render_frame(renderer, nes, config->window_scale);
    } else {
      SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
      SDL_RenderClear(renderer);

      SDL_SetRenderDrawColor(renderer, 200, 200, 205, 255);
      SDL_RenderFillRect(renderer, &button_rect);

      // Render a string structure using primitive vector strokes (Draws an
      // "O P E N" text look)
      SDL_SetRenderDrawColor(renderer, 40, 40, 45, 255);
      float center_x = button_rect.x + (btn_w / 2.0f);
      float center_y = button_rect.y + (btn_h / 2.0f);

      // 'O' Letter lines
      SDL_RenderLine(renderer, center_x - 35, center_y - 10, center_x - 25, center_y - 10);
      SDL_RenderLine(renderer, center_x - 35, center_y + 10, center_x - 25, center_y + 10);
      SDL_RenderLine(renderer, center_x - 35, center_y - 10, center_x - 35, center_y + 10);
      SDL_RenderLine(renderer, center_x - 25, center_y - 10, center_x - 25, center_y + 10);

      // 'P' Letter lines
      SDL_RenderLine(renderer, center_x - 15, center_y - 10, center_x - 15, center_y + 10);
      SDL_RenderLine(renderer, center_x - 15, center_y - 10, center_x - 5, center_y - 10);
      SDL_RenderLine(renderer, center_x - 15, center_y, center_x - 5, center_y);
      SDL_RenderLine(renderer, center_x - 5, center_y - 10, center_x - 5, center_y);

      // 'E' Letter lines
      SDL_RenderLine(renderer, center_x + 5, center_y - 10, center_x + 5, center_y + 10);
      SDL_RenderLine(renderer, center_x + 5, center_y - 10, center_x + 15, center_y - 10);
      SDL_RenderLine(renderer, center_x + 5, center_y, center_x + 15, center_y);
      SDL_RenderLine(renderer, center_x + 5, center_y + 10, center_x + 15, center_y + 10);

      // 'N' Letter lines
      SDL_RenderLine(renderer, center_x + 25, center_y - 10, center_x + 25, center_y + 10);
      SDL_RenderLine(renderer, center_x + 25, center_y - 10, center_x + 35, center_y + 10);
      SDL_RenderLine(renderer, center_x + 35, center_y - 10, center_x + 35, center_y + 10);

      SDL_RenderPresent(renderer);
    }

    // ~62.5 FPS cap (SDL_Delay(16) approximates 60 FPS; actual rate depends on frame compute time)
    SDL_Delay(config->frame_delay);
  }

  // Cleanup
  SDL_DestroyRenderer(renderer);
  SDL_DestroyWindow(window);
  SDL_Quit();
  return;
}
