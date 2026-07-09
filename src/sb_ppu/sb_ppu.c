#include "sb_ppu.h"
#include <string.h>

// Translate PPU address to real VRAM/palette index, handling mirroring.
static uint16_t ppu_real_addr(sb_ppu_t* ppu, uint16_t addr) {
  addr &= 0x3FFF; // 14-bit address space

  if (addr < 0x2000) {
    return addr; // Pattern table (CHR-ROM/RAM)
  }

  if (addr < 0x3F00) {
    // Nametable: $3000-$3FFF mirrors $2000-$2FFF
    addr &= 0x2FFF;
    addr -= 0x2000;

    // Apply cartridge mirroring for nametables 1/2/3
    if (addr >= 0x400 && ppu->cartridge) {
      switch (ppu->cartridge->mirroring) {
      case SB_MIRROR_HORIZONTAL:
        addr &= 0x3FF; // map to nametable 0
        break;
      case SB_MIRROR_VERTICAL:
        if (addr >= 0x800)
          addr -= 0x400; // map $2800-$2BFF to $2400-$27FF
        break;
      default:
        addr &= 0x3FF; // fallback: mirror to page 0
        break;
      }
    } else if (addr >= 0x800) {
      addr -= 0x400;
    }

    if (addr >= 0x800)
      addr -= 0x800;
    return addr; // 0x0000-0x07FF within vram[2048]
  }

  // Palette ($3F00-$3FFF)
  addr = 0x3F00 | (addr & 0x1F);
  if ((addr & 0x13) == 0x10)
    addr ^= 0x10; // mirror $3F10/$3F14/$3F18/$3F1C to
                  // $3F00/$3F04/$3F08/$3F0C
  return addr;
}

uint8_t sb_ppu_vram_read(sb_ppu_t* ppu, uint16_t addr) {
  addr = ppu_real_addr(ppu, addr);

  if (addr >= 0x3F00) {
    return ppu->palette[addr & 0x1F];
  }

  if (addr >= 0x2000) {
    return ppu->vram[addr - 0x2000];
  }

  // Pattern table: read from cartridge CHR
  if (ppu->cartridge)
    return sb_cartridge_read_chr(ppu->cartridge, addr);
  return 0;
}

void sb_ppu_vram_write(sb_ppu_t* ppu, uint16_t addr, uint8_t val) {
  addr = ppu_real_addr(ppu, addr);

  if (addr >= 0x3F00) {
    ppu->palette[addr & 0x1F] = val;
    return;
  }

  if (addr >= 0x2000) {
    ppu->vram[addr - 0x2000] = val;
    return;
  }

  // Pattern table write: only if CHR-RAM
  if (ppu->cartridge)
    sb_cartridge_write_chr(ppu->cartridge, addr, val);
}

// ── PPU Register Read ──

uint8_t sb_ppu_read(sb_ppu_t* ppu, uint16_t addr) {
  switch (addr & 0x2007) {
  case 0:                // PPUCTRL (read-only, returns open bus)
  case 1:                // PPUMASK (read-only)
  case 3:                // OAMADDR (read-only)
  case 5:                // PPUSCROLL (read-only)
  case 6:                // PPUADDR (read-only)
    return ppu->ppudata; // open bus / last written value

  case 2: // PPUSTATUS
  {
    uint8_t val = ppu->ppustatus;
    // Reading status clears VBlank and write toggle
    ppu->ppustatus &= ~SB_PPUSTATUS_VBLANK;
    ppu->w = 0;
    ppu->nmi_pending = false;
    return val;
  }

  case 4: // OAMDATA
    return ppu->oam[ppu->oamaddr];

  case 7: // PPUDATA (buffered read)
  {
    uint16_t read_addr = ppu->v & 0x3FFF;
    uint8_t val = ppu->read_buffer;

    // Update buffer with next byte
    ppu->read_buffer = sb_ppu_vram_read(ppu, read_addr);

    // Palette reads are NOT buffered
    if (read_addr >= 0x3F00)
      val = ppu->read_buffer;

    ppu->v += (ppu->ppuctrl & SB_PPUCTRL_VRAM_INC) ? 32 : 1;
    ppu->v &= 0x7FFF;
    return val;
  }

  default:
    return 0;
  }
}

// ── PPU Register Write ──

void sb_ppu_write(sb_ppu_t* ppu, uint16_t addr, uint8_t val) {
  switch (addr & 0x2007) {
  case 0: // PPUCTRL
    ppu->ppuctrl = val;
    // Update nametable bits in t (bits 10-11)
    ppu->t = (ppu->t & 0xF3FF) | ((uint16_t)(val & 0x03) << 10);
    break;

  case 1: // PPUMASK
    ppu->ppumask = val;
    break;

  case 3: // OAMADDR
    ppu->oamaddr = val;
    break;

  case 4: // OAMDATA
    ppu->oam[ppu->oamaddr++] = val;
    break;

  case 5: // PPUSCROLL (two writes)
    if (ppu->w == 0) {
      // First write: coarse X scroll + fine X
      ppu->t = (ppu->t & 0xFFE0) | (val >> 3);
      ppu->x = val & 0x07;
      ppu->w = 1;
    } else {
      // Second write: coarse Y scroll + fine Y
      ppu->t = (ppu->t & 0x8C1F) | ((uint16_t)(val & 0x07) << 12) | ((uint16_t)(val & 0xF8) << 2);
      ppu->w = 0;
    }
    break;

  case 6: // PPUADDR (two writes)
    if (ppu->w == 0) {
      // High byte
      ppu->t = (ppu->t & 0x80FF) | ((uint16_t)(val & 0x3F) << 8);
      ppu->w = 1;
    } else {
      // Low byte
      ppu->t = (ppu->t & 0xFF00) | val;
      ppu->v = ppu->t;
      ppu->w = 0;
    }
    break;

  case 7: // PPUDATA
  {
    uint16_t write_addr = ppu->v & 0x3FFF;
    sb_ppu_vram_write(ppu, write_addr, val);
    ppu->v += (ppu->ppuctrl & SB_PPUCTRL_VRAM_INC) ? 32 : 1;
    ppu->v &= 0x7FFF;
    break;
  }
  }
}

// PPU Tick
void sb_ppu_tick(sb_ppu_t* ppu) {
  // later tick is a stub. Full implementation in 3.2.
  // For now, just advance dot/scanline and handle NMI generation
  // so games don't hang.

  bool rendering =
    (ppu->scanline < SB_PPU_VISIBLE_SCANLINES || ppu->scanline == SB_PPU_NTSC_SCANLINES - 1) &&
    (ppu->ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR));

  // VBlank start (scanline 241, dot 0)
  if (ppu->scanline == SB_PPU_VISIBLE_SCANLINES + 1 && ppu->dot == 0) {
    ppu->ppustatus |= SB_PPUSTATUS_VBLANK;

    // Edge-triggered NMI
    bool nmi_now = (ppu->ppuctrl & SB_PPUCTRL_NMI) && (ppu->ppustatus & SB_PPUSTATUS_VBLANK);
    if (nmi_now && !ppu->nmi_previous)
      ppu->nmi_pending = true;
    ppu->nmi_previous = nmi_now;
  }

  // Pre-render scanline: clear VBlank, sprite 0 hit, overflow
  if (ppu->scanline == SB_PPU_NTSC_SCANLINES - 1 && ppu->dot == 0) {
    ppu->ppustatus &= ~(SB_PPUSTATUS_VBLANK | SB_PPUSTATUS_SPRITE0_HIT | SB_PPUSTATUS_OVERFLOW);
  }

  // Pre-render scanline: reload scroll at end
  if (
    ppu->scanline == SB_PPU_NTSC_SCANLINES - 1 && ppu->dot == SB_PPU_DOTS_PER_SCANLINE - 1 &&
    rendering
  ) {
    ppu->v = ppu->t;
  }

  // Handle OAM DMA
  if (ppu->dma_active) {
    // DMA takes ~513 cycles (1 dummy + 256 reads + 256 writes)
    // Simplified: just copy now, timing handled in sb_nes_frame
  }

  // Advance dot
  ppu->dot++;
  if (ppu->dot >= SB_PPU_DOTS_PER_SCANLINE) {
    ppu->dot = 0;
    ppu->scanline++;

    if (ppu->scanline >= SB_PPU_NTSC_SCANLINES) {
      ppu->scanline = 0;
      ppu->odd_frame = !ppu->odd_frame;
    }
  }
}

// OAM DMA
void sb_ppu_oam_dma_start(sb_ppu_t* ppu, uint8_t page) {
  ppu->dma_page = page;
  ppu->dma_offset = 0;
  ppu->dma_dummy = true;
  ppu->dma_active = true;
}

// Init
void sb_ppu_init(sb_ppu_t* ppu, sb_cartridge_t* cart) {
  memset(ppu, 0, sizeof(*ppu));
  ppu->cartridge = cart;
  ppu->scanline = SB_PPU_NTSC_SCANLINES - 1; // start at pre-render
  ppu->dot = 0;
  ppu->ppustatus |= SB_PPUSTATUS_VBLANK; // power-up state
  ppu->nmi_previous = (ppu->ppuctrl & SB_PPUCTRL_NMI) != 0;
}

// Framebuffer Access
uint8_t* sb_ppu_get_framebuffer(sb_ppu_t* ppu) { return ppu->framebuffer; }
