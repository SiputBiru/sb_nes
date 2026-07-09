#include "blargg_runner.h"
#include "../../src/sb_nes.h"
#include <stdio.h>

sb_blargg_result_t sb_blargg_run(const sb_blargg_config_t* config) {
  sb_nes_t nes;
  sb_nes_init(&nes);

  sb_blargg_result_t result = { .passed = false, .error_code = -1, .cycles_run = 0 };

  if (!sb_nes_load_rom(&nes, config->rom_path)) {
    printf("  FAILED to load ROM: %s\n", config->rom_path);
    return result;
  }

  size_t max_cycles = config->max_cycles ? config->max_cycles : 20000000;
  uint64_t start_cycles = nes.cpu.cycles;

  // Run CPU until reach max_cycles or detect a failure
  while (nes.cpu.cycles - start_cycles < max_cycles) {
    // Run one frame's worth of instructions
    for (int i = 0; i < 20000; i++) {
      sb_6502_step(&nes.cpu, &nes.bus);
    }

    result.cycles_run = (size_t)(nes.cpu.cycles - start_cycles);

    // Check if test wrote a non-zero result (immediate failure)
    // blargg tests write error count to $6000, 0 = pass
    if (nes.cartridge.prg_ram[0] != 0) {
      result.passed = false;
      result.error_code = nes.cartridge.prg_ram[0];
      return result;
    }
  }

  // Timed out or completed. Check final state.
  // blargg tests write 0 to $6000 on pass.
  uint8_t status = nes.cartridge.prg_ram[0];
  result.error_code = status;
  result.passed = (status == 0);

  return result;
}
