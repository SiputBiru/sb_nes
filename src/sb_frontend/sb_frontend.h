#ifndef SB_FRONTEND_H
#define SB_FRONTEND_H

#include "../sb_nes.h"
#include <stdbool.h>

typedef struct {
  int window_scale;     // 1-4x (default 3)
  uint32_t frame_delay; // for SDL_delay() things
  const char* rom_path; // path to .nes file
} sb_frontend_config_t;

// Run the main loop. Blocks until window is closed.
// Returns 0 on success, 1 on error.
void sb_frontend_run(sb_nes_t* nes, sb_frontend_config_t* config);

void sb_frontend_init(sb_frontend_config_t* config);

#endif
