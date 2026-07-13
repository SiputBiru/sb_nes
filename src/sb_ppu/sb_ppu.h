#ifndef SB_PPU_H
#define SB_PPU_H

#include "../sb_cartridge/sb_cartridge.h"

#define SB_PPU_VISIBLE_SCANLINES 240
#define SB_PPU_NTSC_SCANLINES 262
#define SB_PPU_DOTS_PER_SCANLINE 341

#define SB_PPU_FB_WIDTH 256
#define SB_PPU_FB_HEIGHT 240

// PPUCTRL ($2000) bit masks
#define SB_PPUCTRL_NAMETABLE 0x03
#define SB_PPUCTRL_VRAM_INC 0x04
#define SB_PPUCTRL_SPRITE_PT 0x08
#define SB_PPUCTRL_BG_PT 0x10
#define SB_PPUCTRL_SPRITE_SIZE 0x20
#define SB_PPUCTRL_MASTER_SLAVE 0x40
#define SB_PPUCTRL_NMI 0x80

// PPUMASK ($2001) bit masks
#define SB_PPUMASK_GRAYSCALE 0x01
#define SB_PPUMASK_SHOW_BG_LEFT 0x02
#define SB_PPUMASK_SHOW_SPR_LEFT 0x04
#define SB_PPUMASK_SHOW_BG 0x08
#define SB_PPUMASK_SHOW_SPR 0x10
#define SB_PPUMASK_EMPHASIZE_R 0x20
#define SB_PPUMASK_EMPHASIZE_G 0x40
#define SB_PPUMASK_EMPHASIZE_B 0x80

// PPUSTATUS ($2002) bit masks
#define SB_PPUSTATUS_OVERFLOW 0x20
#define SB_PPUSTATUS_SPRITE0_HIT 0x40
#define SB_PPUSTATUS_VBLANK 0x80

typedef struct {
  // Per-tile shift registers / caches
  uint8_t cached_tile_index;
  uint8_t cached_attr;
  uint8_t cached_low;
  uint8_t cached_high;
  uint8_t cached_palette_id;

} sb_ppu_tile_cache_t;

typedef struct sb_ppu_t {
  // Registers ($2000-$2007)
  uint8_t ppuctrl;
  uint8_t ppumask;
  uint8_t ppustatus;
  uint8_t oamaddr;
  uint8_t oamdata;
  uint8_t ppuscroll;
  uint8_t ppuaddr;
  uint8_t ppudata;

  // Internal PPU state
  int scanline; // 0-261
  int dot;      // 0-340
  bool odd_frame;

  // Address / scroll state (v, t, x, w from NESDev)
  uint16_t v;             // current VRAM address (15-bit)
  uint16_t t;             // temporary VRAM address
  uint8_t x;              // fine X scroll (3-bit)
  uint8_t w;              // write toggle
  uint8_t fine_x_counter; // fine X running counter during rendering

  // Memory
  uint8_t vram[2048];    // 2 KB nametable VRAM
  uint8_t palette[32];   // Palette RAM ($3F00-$3F1F)
  uint8_t oam[256];      // Object Attribute Memory (64 sprites x 4 bytes)
  uint8_t oam_cache[8];  // Sprite indices on current scanline (max 8)
  uint8_t oam_cache_len; // Number of sprites on current scanline

  uint8_t spr_low[8];  // pre-feched pattern low byte
  uint8_t spr_high[8]; // pre-feched pattern high byte
  uint8_t spr_x[8];    // Screen x position
  uint8_t spr_pal[8];  // Pallete ID (attr & 0x03)
  bool spr_behind[8];  // Behind BG flag
  bool spr_hflip[8];   // Horizontal flip flag

  // Per-tile shift registers / caches
  sb_ppu_tile_cache_t tile_cache;

  // Frame buffer (256x240, palette indices)
  uint8_t framebuffer[SB_PPU_FB_WIDTH * SB_PPU_FB_HEIGHT];

  // PPU read buffer (for PPUDATA buffered read)
  uint8_t read_buffer;

  // NMI state
  bool nmi_previous;
  bool nmi_pending;

  // VBlank read deferral for sync timing
  bool vblank_clear_pending;

  // OAM DMA state
  bool dma_active;
  uint8_t dma_page;
  uint8_t dma_offset;
  bool dma_dummy;

  struct sb_cartridge_t* cartridge;
} sb_ppu_t;

// Core API
void sb_ppu_init(sb_ppu_t* ppu, sb_cartridge_t* cart);

// Register access (called by bus)
uint8_t sb_ppu_read(sb_ppu_t* ppu, uint16_t addr);
void sb_ppu_write(sb_ppu_t* ppu, uint16_t addr, uint8_t val);

// Tick the PPU by one dot.
void sb_ppu_tick(sb_ppu_t* ppu);

// OAM DMA (triggered by CPU write to $4014)
void sb_ppu_oam_dma_start(sb_ppu_t* ppu, uint8_t page);

// Get the framebuffer (palette indices)
uint8_t* sb_ppu_get_framebuffer(sb_ppu_t* ppu);

// Render one pixel of background and sprites
void sb_ppu_render_pixel(sb_ppu_t* ppu);

// VRAM access (internal helpers, exposed for testing)
uint8_t sb_ppu_vram_read(sb_ppu_t* ppu, uint16_t addr);
void sb_ppu_vram_write(sb_ppu_t* ppu, uint16_t addr, uint8_t val);

#endif
