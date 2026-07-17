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
      nes->cartridge.mapper.mapper_id
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
    nes->cartridge.mapper.PRG_banks <= 1 ? "mirrored" : "full"
  );
  printf(
    "  CHR:      %zu KB (%s)\n",
    nes->cartridge.chr_rom_size / 1024,
    nes->cartridge.chr_ram ? "RAM" : "ROM"
  );
  printf(
    "  Mirror:   %s\n",
    nes->cartridge.mapper.mirroring == SB_MIRROR_HORIZONTAL ? "horizontal"
    : nes->cartridge.mapper.mirroring == SB_MIRROR_VERTICAL ? "vertical"
                                                            : "four-screen"
  );
  printf("  Reset PC: $%04X\n", nes->cpu.pc);

  return true;
}

// Process one DMA cycle (called every CPU-cycle slot during active DMA).
// Real DMA takes 513+ cycles: 1 dummy + 256 reads + 256 writes.
// Each byte requires 2 CPU cycles (read from CPU memory, write to PPU OAM).
static bool process_dma_cycle(sb_nes_t* nes) {
  if (!nes) return true;

  // Cycle 1: dummy read.
  if (nes->ppu.dma_dummy) {
    nes->ppu.dma_dummy = false;
    return false;
  }

  // Cycle 2-513: read/write pairs for 256 bytes.
  uint16_t src = ((uint16_t)nes->ppu.dma_page << 8) | nes->ppu.dma_offset;

  if (!nes->ppu.dma_write) {
    // Read cycle: read byte from CPU memory into temporary storage.
    nes->ppu.dma_value = sb_bus_read(&nes->bus, src);
    nes->ppu.dma_write = true;
  } else {
    // Write cycle: write byte from temporary storage to PPU OAM.
    nes->ppu.oam[nes->ppu.dma_offset] = nes->ppu.dma_value;
    nes->ppu.dma_offset++;
    nes->ppu.dma_write = false;

    if (nes->ppu.dma_offset == 0) {
      // All 256 bytes transferred. DMA complete.
      nes->ppu.dma_active = false;
      return true;
    }
  }
  return false;
}

void sb_nes_frame(sb_nes_t* nes) {
  // Cycle-interleaved main loop.
  // PPU ticks every dot (1.79 MHz x 3 = 5.37 MHz).
  // CPU advances ONE cycle every 3 PPU dots via sb_6502_cycle().
  // NMI/IRQ are checked inside sb_6502_cycle's fetch_opcode at instruction
  // boundaries. DMA is cycle-stealed: one DMA step per CPU cycle slot.
  //
  // sb_ppu_tick handles:
  //   dots 1-256:  render pixels (calls sb_ppu_render_pixel)
  //   dot 256:     Y increment (fine Y / coarse Y advance)
  //   dot 257:     horizontal reload (v ← t horizontal bits)
  //   dots 280-304: vertical reload (v ← t vertical bits, pre-render only)
  //   scanline 241 dot 1: set VBlank flag, trigger NMI pending
  //   scanline 261 dot 1: clear VBlank, sprite 0 hit, overflow flags

  for (int scanline = 0; scanline < SB_PPU_NTSC_SCANLINES; scanline++) {
    int dots = SB_PPU_DOTS_PER_SCANLINE;
    if (
      scanline == SB_PPU_NTSC_SCANLINES - 1 && nes->ppu.odd_frame &&
      (nes->ppu.ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR))
    )
      dots = SB_PPU_DOTS_PER_SCANLINE - 1;

    for (int dot = 0; dot < dots; dot++) {
      sb_ppu_tick(&nes->ppu);

      // Every 3 PPU dots = 1 CPU cycle.
      if (++nes->cpu_subcycle == 3) {
        nes->cpu_subcycle = 0;

        // DMA steals the CPU cycle slot when active.
        if (nes->ppu.dma_active) {
          process_dma_cycle(nes);
        } else {
          // Bridge NMI from PPU to CPU (checked at instruction boundaries
          // inside sb_6502_cycle's fetch_opcode).
          if (nes->ppu.nmi_pending) {
            nes->ppu.nmi_pending = false;
            sb_6502_nmi(&nes->cpu, &nes->bus);
          }
          // Advance CPU by one internal cycle.
          // The cycle handler manages instruction phases internally;
          // sb_6502_cycle returns SB_OK when an instruction completes
          // and automatically starts the next via fetch_opcode.
          sb_6502_cycle(&nes->cpu, &nes->bus);
        }
      }
    }
  }
  // cpu_subcycle persists across frame boundaries — the CPU may be
  // mid-instruction when the frame ends, and resumes at the correct
  // subcycle on the next frame.
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
