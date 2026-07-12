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
  // PPU ticks every dot. The CPU runs at a ratio of ~1 CPU cycle per 3 PPU
  // dots. Each instruction takes N CPU cycles (2-7). To avoid the CPU
  // perpetually staying ahead of the PPU, we use a running cycle debt:
  // each PPU dot reduces the debt by 1, each CPU cycle adds 3 back.
  // The CPU steps only when debt is <= 0.

  for (int scanline = 0; scanline < SB_PPU_NTSC_SCANLINES; scanline++) {
    int dots = SB_PPU_DOTS_PER_SCANLINE;
    if (scanline == SB_PPU_NTSC_SCANLINES - 1 && nes->ppu.odd_frame &&
        (nes->ppu.ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR)))
      dots = SB_PPU_DOTS_PER_SCANLINE - 1;

    for (int dot = 0; dot < dots; dot++) {
      sb_ppu_tick(&nes->ppu);

      // Process pending NMI before CPU step
      if (nes->ppu.nmi_pending) {
        // NMI takes 7 CPU cycles from the CPU
        nes->ppu.nmi_pending = false;
        sb_6502_nmi(&nes->cpu, &nes->bus);
        // NMI handler will be processed in sb_6502_step below
      }

      // CPU step: targets ~32 instructions per scanline (like real hardware).
      // At 341 dots/scanline, stepping every 9 dots gives ~38 steps/scanline.
      // dot%3==0 would give 114 steps (too fast). dot%9==0 is closer.
      if (dot % 9 == 0) {
        sb_6502_step(&nes->cpu, &nes->bus);
      }

      // Process OAM DMA when active
      if (nes->ppu.dma_active && nes->ppu.dma_offset == 0) {
        uint8_t page = nes->ppu.dma_page;
        for (int i = 0; i < 256; i++) {
          uint16_t src = ((uint16_t)page << 8) | (uint8_t)i;
          nes->ppu.oam[i] = sb_bus_read(&nes->bus, src);
        }
        nes->ppu.dma_active = false;
      }
    }
  }
  // odd_frame is toggled inside sb_ppu_tick() at the natural frame boundary
  // (when scanline wraps from 261 back to 0). No toggle needed here.
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
