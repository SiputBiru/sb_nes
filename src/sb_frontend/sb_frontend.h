#ifndef SB_FRONTEND_H
#define SB_FRONTEND_H

#include "../sb_nes.h"
#include <stdbool.h>

typedef struct {
  int window_scale;     // 1-4x (default 3)
  const char *rom_path; // path to .nes file
} sb_frontend_config_t;

// Run the main loop. Blocks until window is closed.
// Returns 0 on success, 1 on error.
int sb_frontend_run(sb_nes_t *nes, sb_frontend_config_t *config);

#endif
