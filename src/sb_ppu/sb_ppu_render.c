#include "sb_ppu.h"

// Evaluate which sprites are visible on the current scanline.
// Stores up to 8 sprite indices in ppu->oam_cache.
static void evaluate_sprites(sb_ppu_t* ppu) {
  ppu->oam_cache_len = 0;
  int sprite_height = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_SIZE) ? 16 : 8;

  for (int i = 0; i < 64; i++) {
    int oi = i * 4;
    int y = ppu->oam[oi];
    // Y position is scanline + 1 (sprite appears at y+1)
    if (ppu->scanline >= y && ppu->scanline < y + sprite_height) {
      if (ppu->oam_cache_len < 8) {
        ppu->oam_cache[ppu->oam_cache_len++] = (uint8_t)i;
      } else {
        ppu->ppustatus |= SB_PPUSTATUS_OVERFLOW;
      }
    }
  }
}

static void ppu_render_advance_x(sb_ppu_t* ppu) {
  if (ppu->x == 7) {
    ppu->x = 0;
    if ((ppu->v & 0x001F) == 31) {
      ppu->v &= ~0x001F;
      ppu->v ^= 0x0400;
    } else {
      ppu->v++;
    }
  } else {
    ppu->x++;
  }
}

void sb_ppu_render_scanline(sb_ppu_t* ppu) {
  if (ppu->scanline >= SB_PPU_VISIBLE_SCANLINES)
    return;

  bool bg_enabled = (ppu->ppumask & SB_PPUMASK_SHOW_BG) != 0;
  bool spr_enabled = (ppu->ppumask & SB_PPUMASK_SHOW_SPR) != 0;

  // Background Rendering (uses v register per hardware spec)
  if (bg_enabled) {
    bool show_left = (ppu->ppumask & SB_PPUMASK_SHOW_BG_LEFT) != 0;
    uint16_t pt_base = (ppu->ppuctrl & SB_PPUCTRL_BG_PT) ? 0x1000 : 0;

    // Save v and x so dot based updates in sb_ppu_tick see the correct state
    uint16_t v_saved = ppu->v;
    uint8_t  x_saved = ppu->x;

    for (int pixel = 0; pixel < 256; pixel++) {
      if (pixel < 8 && !show_left) {
        ppu_render_advance_x(ppu);
        ppu->framebuffer[ppu->scanline * 256 + pixel] = ppu->palette[0];
        continue;
      }

      // Nametable byte: v bits 0 to 11 index into $2000 to $2FFF
      uint16_t nt_addr = 0x2000 | (ppu->v & 0x0FFF);
      uint8_t tile_index = sb_ppu_vram_read(ppu, nt_addr);

      // Attribute byte: computed from v per hardware formula
      uint16_t attr_addr = 0x23C0 | (ppu->v & 0x0C00) |
                           ((ppu->v >> 4) & 0x38) | ((ppu->v >> 2) & 0x07);
      uint8_t attr = sb_ppu_vram_read(ppu, attr_addr);
      int shift = ((ppu->v >> 1) & 1) | ((ppu->v >> 5) & 2);
      uint8_t palette_id = (attr >> (shift * 2)) & 0x03;

      // Pattern table: fine Y from v bits 12 to 14
      uint16_t fine_y = (ppu->v >> 12) & 0x07;
      uint16_t pattern_addr = pt_base + tile_index * 16 + fine_y;
      uint8_t low = sb_ppu_vram_read(ppu, pattern_addr);
      uint8_t high = sb_ppu_vram_read(ppu, pattern_addr + 8);

      // Extract pixel using ppu->x as the bit selector within the tile
      int bit = 7 - ppu->x;
      uint8_t color = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      int fb_index = ppu->scanline * 256 + pixel;
      if (color != 0)
        ppu->framebuffer[fb_index] = ppu->palette[palette_id * 4 + color];
      else
        ppu->framebuffer[fb_index] = ppu->palette[0];

      // Advance to next pixel, updating v coarse X via ppu->x tracking
      ppu_render_advance_x(ppu);
    }

    // Restore so sb_ppu_tick dot 256 and 257 updates work correctly
    ppu->v = v_saved;
    ppu->x = x_saved;
  } else {
    // Background disabled: fill scanline with backdrop color
    for (int x = 0; x < 256; x++) {
      ppu->framebuffer[ppu->scanline * 256 + x] = ppu->palette[0];
    }
  }

  // Sprite Evaluation
  if (!spr_enabled)
    return;

  evaluate_sprites(ppu);
  if (ppu->oam_cache_len == 0)
    return;

  int sprite_height = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_SIZE) ? 16 : 8;
  uint16_t spr_pt_base = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_PT) ? 0x1000 : 0;
  bool show_left = (ppu->ppumask & SB_PPUMASK_SHOW_SPR_LEFT) != 0;

  // --- Sprite Rendering ---
  // Sprites are rendered in OAM order (lower index = higher priority).
  // For each pixel, the first (lowest-index) non-transparent sprite wins.
  for (int c = 0; c < ppu->oam_cache_len; c++) {
    int si = ppu->oam_cache[c];
    int oi = si * 4;

    int y = ppu->oam[oi];
    uint8_t tile_index = ppu->oam[oi + 1];
    uint8_t attr = ppu->oam[oi + 2];
    int x = ppu->oam[oi + 3];

    bool vflip = (attr & 0x80) != 0;
    bool hflip = (attr & 0x40) != 0;
    bool behind_bg = (attr & 0x20) != 0;
    uint8_t palette_id = attr & 0x03;

    uint16_t pt_base = spr_pt_base;

    // 8x16 sprite mode: tile_index bit 0 selects pattern table
    if (sprite_height == 16) {
      pt_base = (tile_index & 1) ? 0x1000 : 0;
      tile_index &= 0xFE;
    }

    int sprite_row = ppu->scanline - y;
    if (vflip)
      sprite_row = sprite_height - 1 - sprite_row;

    int tile_row_in_sprite = (sprite_height == 16 && sprite_row >= 8) ? 1 : 0;
    int fine_y = sprite_row & 7;
    uint16_t pattern_addr = pt_base + (uint16_t)(tile_index + tile_row_in_sprite) * 16 + fine_y;

    uint8_t low = sb_ppu_vram_read(ppu, pattern_addr);
    uint8_t high = sb_ppu_vram_read(ppu, pattern_addr + 8);

    for (int pixel = 0; pixel < 8; pixel++) {
      int screen_x = x + pixel;

      if (screen_x < 8 && !show_left)
        continue;
      if (screen_x >= 256)
        continue;

      int bit = hflip ? pixel : 7 - pixel;
      uint8_t color = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      if (color == 0)
        continue; // transparent sprite pixel

      int fb_index = ppu->scanline * 256 + screen_x;
      uint8_t bg_color = ppu->framebuffer[fb_index];

      // Sprite 0 hit: non-transparent sprite 0 pixel overlaps non-backdrop bg
      if (si == 0 && bg_color != ppu->palette[0] &&
          (ppu->ppumask & SB_PPUMASK_SHOW_BG)) {
        ppu->ppustatus |= SB_PPUSTATUS_SPRITE0_HIT;
      }

      // Priority: if behind_bg, sprite only shows on transparent bg pixels
      if (behind_bg && bg_color != ppu->palette[0])
        continue;

      // Draw sprite pixel using sprite palettes ($3F11-$3F1F)
      uint8_t entry = 0x10 + palette_id * 4 + color;
      ppu->framebuffer[fb_index] = ppu->palette[entry];
    }
  }
}
