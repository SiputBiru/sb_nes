#include "sb_nes.h"
#include <stdio.h>
#include <string.h>

void sb_apu_frame_tick(sb_apu_t* apu) {
  if (!apu || !apu->frame_irq_enabled) return;

  apu->frame_divider++;
  if (apu->frame_divider >= SB_APU_FRAME_DIVIDER_NTSC) {
    apu->frame_divider = 0;
    apu->frame_step++;
    int max_step = apu->frame_5step ? 5 : 4;
    if (apu->frame_step >= max_step) {
      apu->frame_step = 0;
      if (!apu->frame_5step) {
        // 4-step mode: IRQ fires at step 3→0 wrap
        apu->frame_irq_pending = true;
      }
    }
  }
}

uint8_t sb_apu_read(sb_apu_t* apu, uint16_t addr) {
  if (addr == 0x4015) {
    uint8_t status = 0;
    if (apu && apu->frame_irq_pending)
      status |= 0x40; // bit6 = frame IRQ pending
    if (apu)
      apu->frame_irq_pending = false; // cleared on read
    return status;
  }
  return 0; // other APU reads not implemented
}

void sb_apu_write(sb_apu_t* apu, uint16_t addr, uint8_t val) {
  if (addr == 0x4017) {
    if (!apu) return;
    // $4017 write: reset frame counter, clear IRQ, set mode
    apu->frame_irq_pending = false;
    apu->frame_divider = 0;
    apu->frame_step = 0;
    apu->frame_5step = (val & 0x80) != 0;
    apu->frame_irq_enabled = !apu->frame_5step;
  }
}

void sb_nes_init(sb_nes_t* nes) {

  memset(nes, 0, sizeof(*nes));

  // Wire the cartridge into the bus
  nes->bus.cartridge = &nes->cartridge;

  // Wire the PPU into the bus
  nes->bus.ppu = &nes->ppu;

  // Wire the APU into the bus
  nes->bus.apu = &nes->apu;

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

// Process one DMA sub-cycle (called once per CPU-cycle slot during active DMA).
// Each byte transfer takes 2 sub-cycles (read + write).
// Total: 1 dummy + 256 reads + 256 writes = 513 CPU cycles.
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
  // boundaries. DMA is cycle-stolen: one DMA sub-cycle per CPU cycle slot.
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

        // Tick APU frame counter (once per CPU cycle slot, even during DMA).
        sb_apu_frame_tick(&nes->apu);

        // DMA steals the CPU cycle slot when active.
        if (nes->ppu.dma_active) {
          process_dma_cycle(nes);
        } else {
          // Bridge APU IRQ to CPU (sb_6502_irq checks I flag).
          if (nes->apu.frame_irq_pending) {
            sb_6502_irq(&nes->cpu, &nes->bus);
          }

          // Advance CPU by one internal cycle.
          int rc = sb_6502_cycle(&nes->cpu, &nes->bus);

          // NMI bridge at INSTRUCTION BOUNDARY only.
          // This ensures that reading $2002 (which clears VBlank) during an
          // instruction prevents NMI from firing for that VBlank.
          if ((rc == SB_OK || rc < 0) && nes->ppu.nmi_pending) {
            nes->ppu.nmi_pending = false;
            sb_6502_nmi(&nes->cpu, &nes->bus);
          }
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
