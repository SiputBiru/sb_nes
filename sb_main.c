#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/sb_frontend/sb_frontend.h"

static void print_usage(const char* prog) {
  printf("sb_nes — SiputBiru NES Emulator\n");
  printf("Usage: %s [options] <rom.nes>\n\n", prog);
  printf("Options:\n");
  printf("  --scale N    Window scale factor (1-4, default: 3)\n");
  printf("  --help       Show this help\n");
  printf("\nControls:\n");
  printf("  WASD  = D-Pad      J = B button\n");
  printf("  Space = Select     K = A button\n");
  printf("  Enter = Start     ESC = Quit\n");
}

int main(int argc, char** argv) {
  sb_frontend_config_t config = {
    .window_scale = 3,
    .rom_path = NULL,
  };

  for (int i = 1; i < argc; i++) {
    if (strcmp(argv[i], "--help") == 0) {
      print_usage(argv[0]);
      return 0;
    } else if (strcmp(argv[i], "--scale") == 0 && i + 1 < argc) {
      int s = atoi(argv[++i]);
      if (s < 1) s = 1;
      if (s > 4) s = 4;
      config.window_scale = s;
    } else if (argv[i][0] != '-') {
      config.rom_path = argv[i];
    }
  }

  if (!config.rom_path) {
    print_usage(argv[0]);
    return 1;
  }

  return sb_frontend_run(&config);
}
