#include "sb_ppu.h"
#include <string.h>

// Apply NES palette mirroring: $3F10/$3F14/$3F18/$3F1C map to
// $3F00/$3F04/$3F08/$3F0C. Input is a 5-bit palette index (0-31).
uint8_t ppu_mirror_palette_idx(uint8_t idx) {
  if ((idx & 0x13) == 0x10)
    idx ^= 0x10;
  return idx;
}

static void ppu_check_nmi_edge(sb_ppu_t* ppu) {
  bool nmi_now =
    (ppu->ppuctrl & SB_PPUCTRL_NMI) != 0 && (ppu->ppustatus & SB_PPUSTATUS_VBLANK) != 0;
  if (nmi_now && !ppu->nmi_previous)
    ppu->nmi_pending = true;
  ppu->nmi_previous = nmi_now;
}

// Translate PPU address to real VRAM/palette index, handling mirroring.
static uint16_t ppu_real_addr(sb_ppu_t* ppu, uint16_t addr) {
  addr &= 0x3FFF; // 14-bit address space

  if (addr < 0x2000) {
    return addr; // Pattern table (CHR-ROM/RAM)
  }

  if (addr < 0x3F00) {
    // Nametable: $3000-$3FFF mirrors $2000-$2FFF
    addr &= 0x2FFF;
    addr -= 0x2000; // Now 0x000-0xFFF (4 nametable slots of 1KB each)

    // Apply cartridge mirroring to map 4 nametable slots into 2KB VRAM.
    // VRAM page 0: vram[0x000-0x3FF], page 1: vram[0x400-0x7FF]
    if (ppu->cartridge) {
      switch (ppu->cartridge->mapper.mirroring) {
      case SB_MIRROR_VERTICAL:
        // NT0/NT1 → page 0, NT2/NT3 → page 1
        // Two unique pages arranged vertically (good for horizontal scrolling)
        if (addr >= 0x800)
          addr -= 0x800;
        break;
      case SB_MIRROR_HORIZONTAL:
        // NT0/NT2 → page 0, NT1/NT3 → page 1
        // Two unique pages arranged horizontally (good for vertical scrolling)
        if (addr >= 0x400 && addr < 0x800)
          addr -= 0x400;
        else if (addr >= 0x800) {
          addr -= 0x400;
          if (addr >= 0x800)
            addr -= 0x400;
        }
        break;
      default:
        // Four-screen or unknown: keep all pages, clip overflow
        if (addr >= 0x800)
          addr -= 0x800;
        break;
      }
    } else {
      if (addr >= 0x800)
        addr -= 0x800;
    }

    // Return in 0x2000-0x27FF range so vram_read/write can route to vram[]
    return addr + 0x2000;
  }

  // Palette ($3F00-$3FFF)
  return 0x3F00 | ppu_mirror_palette_idx(addr & 0x1F);
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
    return ppu->cartridge->mapper.read_chr(&ppu->cartridge->mapper, addr);
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
    ppu->cartridge->mapper.write_chr(&ppu->cartridge->mapper, addr, val);
}

// PPU Register Read

uint8_t sb_ppu_read(sb_ppu_t* ppu, uint16_t addr) {
  switch (addr & 0x0007) {
  case 0:                // PPUCTRL (read-only, returns open bus)
  case 1:                // PPUMASK (read-only)
  case 3:                // OAMADDR (read-only)
  case 5:                // PPUSCROLL (read-only)
  case 6:                // PPUADDR (read-only)
    return ppu->ppudata; // open bus / last written value

  case 2: // PPUSTATUS
  {
    uint8_t val = ppu->ppustatus;
    // Clear VBlank flag: the first read after VBlank was set returns the
    // flag, but doesn't clear it yet. The SECOND read after VBlank was
    // set actually clears it. This matches the real hardware behavior
    // where two LDA $2002 in rapid succession can both see VBlank set,
    // which is needed by full_palette.nes's sync loop.
    if (ppu->vblank_clear_pending) {
      ppu->ppustatus &= ~SB_PPUSTATUS_VBLANK;
      ppu->vblank_clear_pending = false;
    }
    // If VBlank is set and we read it, mark clear as pending
    if (val & SB_PPUSTATUS_VBLANK)
      ppu->vblank_clear_pending = true;
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

// PPU Register Write

void sb_ppu_write(sb_ppu_t* ppu, uint16_t addr, uint8_t val) {
  switch (addr & 0x0007) {
  case 0: // PPUCTRL
  {
    ppu->ppuctrl = val;
    ppu->t = (ppu->t & 0xF3FF) | ((uint16_t)(val & 0x03) << 10);

    // Edge-triggered NMI detection during active VBlank
    ppu_check_nmi_edge(ppu);
    break;
  }

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
      ppu->fine_x_counter = val & 0x07;
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
  bool rendering =
    (ppu->scanline < SB_PPU_VISIBLE_SCANLINES || ppu->scanline == SB_PPU_NTSC_SCANLINES - 1) &&
    (ppu->ppumask & (SB_PPUMASK_SHOW_BG | SB_PPUMASK_SHOW_SPR)) != 0;

  // Compute max dots for this scanline early so pre-render reload can use it.
  // NTSC odd frame quirk: pre-render scanline runs 340 dots instead of 341
  // when rendering is enabled on odd frames.
  int max_dot = SB_PPU_DOTS_PER_SCANLINE;
  if (ppu->scanline == SB_PPU_NTSC_SCANLINES - 1 && ppu->odd_frame && rendering)
    max_dot = SB_PPU_DOTS_PER_SCANLINE - 1;

  // Render one pixel per dot (handles both rendering and non-rendering mode)
  if (ppu->scanline < SB_PPU_VISIBLE_SCANLINES && ppu->dot >= 1 && ppu->dot <= 256) {
    sb_ppu_render_pixel(ppu);
  }

  // VBlank start at scanline 241, dot 1.
  // Sets the VBlank flag in PPUSTATUS and triggers NMI if enabled.
  if (ppu->scanline == SB_PPU_VISIBLE_SCANLINES + 1 && ppu->dot == 1) {
    ppu->ppustatus |= SB_PPUSTATUS_VBLANK;
    ppu->vblank_clear_pending = false; // Reset for fresh VBlank reads

    // Edge-triggered NMI: fires when PPUCTRL NMI enable AND VBlank
    // are both set on the rising edge.
    ppu_check_nmi_edge(ppu);
  }

  // Pre-render scanline (261): clear flags at start
  if (ppu->scanline == SB_PPU_NTSC_SCANLINES - 1 && ppu->dot == 1) {
    ppu->ppustatus &= ~(SB_PPUSTATUS_VBLANK | SB_PPUSTATUS_SPRITE0_HIT | SB_PPUSTATUS_OVERFLOW);
    ppu->nmi_previous = false; // VBlank going low resets edge detection
    ppu->vblank_clear_pending = false;
  }

  // Y increment at dot 256 of visible scanlines
  if (rendering && ppu->scanline < SB_PPU_VISIBLE_SCANLINES && ppu->dot == 256) {
    if ((ppu->v & 0x7000) != 0x7000) {
      ppu->v += 0x1000;
    } else {
      ppu->v &= ~0x7000;
      int y = (ppu->v >> 5) & 0x1F;
      if (y == 29) {
        y = 0;
        ppu->v ^= 0x0800;
      } else if (y == 31) {
        y = 0;
      } else {
        y++;
      }
      ppu->v = (ppu->v & ~0x03E0) | (y << 5);
    }
  }

  // Horizontal reload at dot 257 from t to v
  if (rendering && ppu->dot == 257) {
    ppu->v = (ppu->v & ~0x041F) | (ppu->t & 0x041F);
    ppu->fine_x_counter = ppu->x;
  }

  // Pre-render scanline dots 280 to 304: reload vertical bits from t to v
  if (
    rendering && ppu->scanline == SB_PPU_NTSC_SCANLINES - 1 && ppu->dot >= 280 && ppu->dot <= 304
  ) {
    ppu->v = (ppu->v & ~0x7BE0) | (ppu->t & 0x7BE0);
  }

  // Advance dot.
  ppu->dot++;

  if (ppu->dot >= max_dot) {
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
