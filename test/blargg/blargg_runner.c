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
  int cpu_subcycle = 0;

  // Run cycle-interleaved loop (mirrors sb_nes_frame() logic — keep in sync!)
  while (nes.cpu.cycles - start_cycles < max_cycles) {
    // Tick PPU
    sb_ppu_tick(&nes.ppu);

    // CPU advances 1 cycle every 3 PPU dots
    if (++cpu_subcycle == 3) {
      cpu_subcycle = 0;

      // Tick APU frame counter (once per CPU cycle slot, even during DMA).
      sb_apu_frame_tick(&nes.apu);

      if (nes.ppu.dma_active) {
        // DMA steals the CPU cycle
        if (nes.ppu.dma_dummy) {
          nes.ppu.dma_dummy = false;
        } else {
          uint16_t src = ((uint16_t)nes.ppu.dma_page << 8) | nes.ppu.dma_offset;
          if (!nes.ppu.dma_write) {
            nes.ppu.dma_value = sb_bus_read(&nes.bus, src);
            nes.ppu.dma_write = true;
          } else {
            nes.ppu.oam[nes.ppu.dma_offset] = nes.ppu.dma_value;
            nes.ppu.dma_offset++;
            nes.ppu.dma_write = false;
            if (nes.ppu.dma_offset == 0) {
              nes.ppu.dma_active = false;
            }
          }
        }
        // Increment cycles manually because CPU is halted
        nes.cpu.cycles++;
      } else {
        // NO bridges needed! sb_6502_cycle's fetch_opcode reads PPU/APU
        // pending flags directly from bus->ppu and bus->apu
        sb_6502_cycle(&nes.cpu, &nes.bus);
      }
    }

    // Periodically check test status (every 20000 CPU cycles)
    if (cpu_subcycle == 0 && nes.cpu.cycles % 20000 == 0) {
      uint8_t status = nes.cartridge.prg_ram[0];
      if (status != 0 && status != 0x80) {
        result.passed = false;
        result.error_code = status;
        result.cycles_run = (size_t)(nes.cpu.cycles - start_cycles);
        if (nes.cartridge.prg_ram[4] != 0) {
          printf("  [DEBUG] Test output: %s\n", (char*)&nes.cartridge.prg_ram[4]);
        }
        return result;
      }
      if (status == 0 && (nes.cpu.cycles - start_cycles) > 50000) {
        result.passed = true;
        result.error_code = 0;
        result.cycles_run = (size_t)(nes.cpu.cycles - start_cycles);
        return result;
      }
    }
  }

  // Timed out. Check final state.
  uint8_t status = nes.cartridge.prg_ram[0];
  result.error_code = status;
  result.passed = (status == 0);
  result.cycles_run = (size_t)(nes.cpu.cycles - start_cycles);
  if (!result.passed && nes.cartridge.prg_ram[4] != 0) {
    printf("  [DEBUG] Test output: %s\n", (char*)&nes.cartridge.prg_ram[4]);
  }

  return result;
}
