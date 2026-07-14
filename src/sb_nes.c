#include "sb_nes.h"
#include <stdio.h>
#include <string.h>
void sb_nes_init(sb_nes_t* nes) {

  memset(nes, 0, sizeof(*nes));

  // Wire the cartridge into the bus
  nes->bus.cartridge = &nes->cartridge;

  // Wire the PPU into the bus
  nes->bus.ppu = &nes->ppu;

  // Init PPU with cartridge reference (for CHR reads)
  sb_ppu_init(&nes->ppu, &nes->cartridge);

  // Init opcode dispatch table
  sb_6502_init_opcodes();
}

bool sb_nes_load_rom(sb_nes_t* nes, const char* path) {
  sb_cartridge_result_t result = sb_cartridge_load_from_file(&nes->cartridge, path);

  switch (result) {
  case SB_CARTRIDGE_OK:
    break;
  case SB_CARTRIDGE_ERR_BAD_FILE:
    fprintf(stderr, "Error: could not open '%s'\n", path);
    return false;
  case SB_CARTRIDGE_ERR_BAD_MAGIC:
    fprintf(stderr, "Error: '%s' is not a valid NES ROM (bad header)\n", path);
    return false;
  case SB_CARTRIDGE_ERR_NO_PRG:
    fprintf(stderr, "Error: '%s' has no PRG-ROM data\n", path);
    return false;
  case SB_CARTRIDGE_ERR_TOO_LARGE:
    fprintf(stderr, "Error: '%s' is too large (>512KB PRG)\n", path);
    return false;
  case SB_CARTRIDGE_ERR_NES20:
    fprintf(stderr, "Error: '%s' is NES 2.0 format (not supported yet)\n", path);
    return false;
  case SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER:
    fprintf(
      stderr,
      "Error: '%s' uses mapper %d (only NROM/mapper 0 supported)\n",
      path,
      nes->cartridge.mapper_id
    );
    return false;
  default:
    fprintf(stderr, "Error: unknown error loading '%s'\n", path);
    return false;
  }

  // Reset the CPU (reads reset vector from cartridge via bus)
  sb_6502_reset(&nes->cpu, &nes->bus);

  printf("Loaded: %s\n", path);
  printf(
    "  PRG-ROM: %zu KB (%s)\n",
    nes->cartridge.prg_rom_size / 1024,
    nes->cartridge.prg_mirror ? "mirrored" : "full"
  );
  printf(
    "  CHR:      %zu KB (%s)\n",
    nes->cartridge.chr_rom_size / 1024,
    nes->cartridge.chr_ram ? "RAM" : "ROM"
  );
  printf(
    "  Mirror:   %s\n",
    nes->cartridge.mirroring == SB_MIRROR_HORIZONTAL ? "horizontal"
    : nes->cartridge.mirroring == SB_MIRROR_VERTICAL ? "vertical"
                                                     : "four-screen"
  );
  printf("  Reset PC: $%04X\n", nes->cpu.pc);

  return true;
}

void sb_nes_frame(sb_nes_t* nes) {
  // Run one NTSC frame: 262 scanlines x 341 dots.
  // PPU ticks every dot. CPU timing uses a post-instruction wait counter:
  // each instruction consumes N CPU cycles (= N*3 PPU dots). After executing
  // an instruction, cpu_wait is set to (cycles * 3 - 1) PPU dots. Each dot
  // decrements cpu_wait. When cpu_wait <= 0, the next instruction runs.

  for (int scanline = 0; scanline < SB_PPU_NTSC_SCANLINES; scanline++) {
    int dots = SB_PPU_DOTS_PER_SCANLINE;
    if (
      scanline == SB_PPU_NTSC_SCANLINES - 1 && nes->ppu.odd_frame &&
      (nes->ppu.ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR))
    )
      dots = SB_PPU_DOTS_PER_SCANLINE - 1;

    for (int dot = 0; dot < dots; dot++) {
      sb_ppu_tick(&nes->ppu);

      // Each PPU dot brings the CPU closer to its next instruction
      nes->cpu_wait--;

      // When the wait expires, execute the next CPU instruction
      if (nes->cpu_wait <= 0) {
        // Check for pending NMI from PPU (must be serviced before instruction)
        if (nes->ppu.nmi_pending) {
          nes->ppu.nmi_pending = false;
          sb_6502_nmi(&nes->cpu, &nes->bus);
        }

        // Execute one instruction. The step adds its cycle count to cpu->cycles.
        uint64_t before = nes->cpu.cycles;
        sb_6502_step(&nes->cpu, &nes->bus);
        uint64_t spent = nes->cpu.cycles - before;

        // Wait for the instruction's cycles * 3 PPU dots (minus this dot)
        // A 2-cycle NOP waits 2*3-1 = 5 dots, spacing = 6 dots.
        // A 6-cycle JSR waits 6*3-1 = 17 dots, spacing = 18 dots.
        nes->cpu_wait = (int32_t)(spent * 3) - 1;
      }

      // Process OAM DMA when active (checked every dot, not tied to CPU step)
      if (nes->ppu.dma_active && nes->ppu.dma_offset == 0) {
        uint8_t page = nes->ppu.dma_page;
        for (int i = 0; i < 256; i++) {
          uint16_t src = ((uint16_t)page << 8) | (uint8_t)i;
          nes->ppu.oam[i] = sb_bus_read(&nes->bus, src);
        }
        nes->ppu.dma_active = false;
        // DMA steals ~513 CPU cycles from the CPU → hold off next instruction
        nes->cpu_wait += 513 * 3;
      }
    }
  }
  // cpu_wait persists across frame boundaries — the NMI handler may be
  // mid-execution when the frame ends, and resumes with correct spacing.
  // odd_frame is toggled inside sb_ppu_tick() at the natural frame boundary.
}

// Reverse bit order: frontend A=bit7..R=bit0 → NES R=bit7..A=bit0
static uint8_t reverse_bits(uint8_t b) {
  b = (b & 0xF0) >> 4 | (b & 0x0F) << 4;
  b = (b & 0xCC) >> 2 | (b & 0x33) << 2;
  b = (b & 0xAA) >> 1 | (b & 0x55) << 1;
  return b;
}

void sb_nes_set_buttons(sb_nes_t* nes, uint8_t mask) {
  nes->controller_mask = mask;
  // Convert from frontend bit order to NES protocol order
  // Frontend: A=bit7,R=bit0  →  NES: A=bit0,R=bit7
  nes->bus.controller_bits = reverse_bits(mask);
}
