#include "sb_ppu.h"

// Evaluate which sprites are visible on the current scanline.
// Stores up to 8 sprite indices in ppu->oam_cache.
static void evaluate_sprites(sb_ppu_t *ppu) {
  ppu->oam_cache_len = 0;
  int sprite_height = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_SIZE) ? 16 : 8;

  for (int i = 0; i < 64; i++) {
    int oi = i * 4;
    int y = ppu->oam[oi];
    if (ppu->scanline >= y && ppu->scanline < y + sprite_height) {
      if (ppu->oam_cache_len < 8) {
        int c = ppu->oam_cache_len;
        ppu->oam_cache[c] = (uint8_t)i;

        // cache OAM data
        uint8_t tile = ppu->oam[oi + 1];
        uint8_t attr = ppu->oam[oi + 2];
        ppu->spr_x[c] = ppu->oam[oi + 3];
        ppu->spr_pal[c] = attr & 0x03;
        ppu->spr_behind[c] = (attr & 0x20) != 0;
        ppu->spr_hflip[c] = (attr & 0x40) != 0;

        // Pre-fetch pattern data (the 2 VRAM reads that were per-pixel)
        bool vflip = (attr & 0x80) != 0;
        uint16_t pt = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_PT) ? 0x1000 : 0;
        if (sprite_height == 16) {
          pt = (tile & 1) ? 0x1000 : 0;
          tile &= 0xFE;
        }
        int row = ppu->scanline - y;
        if (vflip)
          row = sprite_height - 1 - row;
        int toff = (sprite_height == 16 && row >= 8) ? 1 : 0;
        uint16_t pa = pt + (uint16_t)(tile + toff) * 16 + (row & 7);

        ppu->spr_low[c] = sb_ppu_vram_read(ppu, pa);
        ppu->spr_high[c] = sb_ppu_vram_read(ppu, pa + 8);

        ppu->oam_cache_len++;
      } else {
        ppu->ppustatus |= SB_PPUSTATUS_OVERFLOW;
      }
    }
  }
}

// Advance fine X running counter and coarse X.
static void ppu_render_advance_x(sb_ppu_t *ppu) {
  if (ppu->fine_x_counter == 7) {
    ppu->fine_x_counter = 0;
    if ((ppu->v & 0x001F) == 31) {
      ppu->v &= ~0x001F;
      ppu->v ^= 0x0400;
    } else {
      ppu->v++;
    }
  } else {
    ppu->fine_x_counter++;
  }
}

// Render one pixel of background and sprites.
void sb_ppu_render_pixel(sb_ppu_t *ppu) {
  if (ppu->scanline >= SB_PPU_VISIBLE_SCANLINES) {
    return;
  }

  int pixel_x = ppu->dot - 1; // ppu->dot is 1-256 for visible pixels
  int fb_index = ppu->scanline * 256 + pixel_x;

  bool bg_enabled = (ppu->ppumask & SB_PPUMASK_SHOW_BG) != 0;
  bool spr_enabled = (ppu->ppumask & SB_PPUMASK_SHOW_SPR) != 0;
  bool show_bg_left = (ppu->ppumask & SB_PPUMASK_SHOW_BG_LEFT) != 0;
  bool show_spr_left = (ppu->ppumask & SB_PPUMASK_SHOW_SPR_LEFT) != 0;

  uint8_t bg_color_idx = 0;
  uint8_t bg_palette_id = 0;
  uint8_t final_pixel = ppu->palette[0];

  // Evaluate sprites once per scanline at the first pixel
  if (pixel_x == 0) {
    evaluate_sprites(ppu);
  }

  // Fetch background tile data at tile boundaries (every 8 pixels)
  if (bg_enabled || spr_enabled) {
    if (pixel_x == 0 || ppu->fine_x_counter == 0) {
      uint16_t nt_addr = 0x2000 | (ppu->v & 0x0FFF);
      ppu->tile_cache.tile_index = sb_ppu_vram_read(ppu, nt_addr);

      uint16_t attr_addr = 0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x38) |
                           ((ppu->v >> 2) & 0x07);
      ppu->tile_cache.attr = sb_ppu_vram_read(ppu, attr_addr);

      int shift = ((ppu->v >> 1) & 1) | ((ppu->v >> 5) & 2);
      ppu->tile_cache.palette_id = (ppu->tile_cache.attr >> (shift * 2)) & 0x03;

      uint16_t fine_y = (ppu->v >> 12) & 0x07;
      uint16_t pt_base = (ppu->ppuctrl & SB_PPUCTRL_BG_PT) ? 0x1000 : 0;
      uint16_t pattern_addr =
          pt_base + ppu->tile_cache.tile_index * 16 + fine_y;
      ppu->tile_cache.low = sb_ppu_vram_read(ppu, pattern_addr);
      ppu->tile_cache.high = sb_ppu_vram_read(ppu, pattern_addr + 8);
    }
  }

  // Extract background pixel
  if (bg_enabled) {
    if (pixel_x >= 8 || show_bg_left) {
      int bit = 7 - ppu->fine_x_counter;
      bg_color_idx = ((ppu->tile_cache.high >> bit) & 1) << 1 |
                     ((ppu->tile_cache.low >> bit) & 1);
      bg_palette_id = ppu->tile_cache.palette_id;
      if (bg_color_idx != 0) {
        final_pixel = ppu->palette[bg_palette_id * 4 + bg_color_idx];
      }
    }
  }

  // Sprite rendering (uses Pre-fetched cache)
  if (spr_enabled) {
    for (int c = 0; c < ppu->oam_cache_len; c++) {
      int si = ppu->oam_cache[c];
      int sx = ppu->spr_x[c];

      if (pixel_x < sx || pixel_x >= sx + 8)
        continue;
      if (pixel_x < 8 && !show_spr_left)
        continue;

      // Extract pixel from Pre-fetched pattern (NO VRAM reads here)
      int off = pixel_x - sx;
      int bit = ppu->spr_hflip[c] ? off : (7 - off);
      uint8_t spr =
          ((ppu->spr_high[c] >> bit) & 1) << 1 | ((ppu->spr_low[c] >> bit) & 1);

      if (spr == 0)
        continue;

      // Sprite 0 hit
      if (si == 0 && bg_color_idx != 0 && bg_enabled) {
        if (pixel_x >= 8 ||
            (ppu->ppumask &
             (SB_PPUMASK_SHOW_BG_LEFT | SB_PPUMASK_SHOW_SPR_LEFT)) ==
                (SB_PPUMASK_SHOW_BG_LEFT | SB_PPUMASK_SHOW_SPR_LEFT)) {
          ppu->ppustatus |= SB_PPUSTATUS_SPRITE0_HIT;
        }
      }

      if (ppu->spr_behind[c] && bg_color_idx != 0)
        continue;

      final_pixel = ppu->palette[0x10 + ppu->spr_pal[c] * 4 + spr];
      break;
    }
  }

  ppu->framebuffer[fb_index] = final_pixel;

  // Advance VRAM address scroll registers
  if (bg_enabled || spr_enabled) {
    ppu_render_advance_x(ppu);
  }
}
