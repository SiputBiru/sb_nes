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
  // PPU ticks every dot. CPU ticks every 3 PPU dots.
  // APU ticks every CPU cycle (later).

  for (int scanline = 0; scanline < SB_PPU_NTSC_SCANLINES; scanline++) {
    int dots = SB_PPU_DOTS_PER_SCANLINE;

    // NTSC odd frame: pre-render scanline has 340 dots when rendering enabled.
    if (
      scanline == SB_PPU_NTSC_SCANLINES - 1 && nes->ppu.odd_frame &&
      (nes->ppu.ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR))
    )
      dots = SB_PPU_DOTS_PER_SCANLINE - 1;

    for (int dot = 0; dot < dots; dot++) {
      // PPU ticks every dot.
      sb_ppu_tick(&nes->ppu);

      // CPU ticks every 3 PPU dots.
      // Real NES: CPU runs at 1.79 MHz, PPU at 5.37 MHz (3:1 ratio).
      if (dot % 3 == 1) {
        // Route PPU NMI to CPU before stepping.
        if (nes->ppu.nmi_pending) {
          nes->ppu.nmi_pending = false;
          sb_6502_nmi(&nes->cpu, &nes->bus);
        }
        sb_6502_step(&nes->cpu, &nes->bus);
      }
    }
  }

  nes->ppu.odd_frame = !nes->ppu.odd_frame;
}

void sb_nes_set_buttons(sb_nes_t* nes, uint8_t mask) { nes->controller_mask = mask; }
