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

// Advance fine X running counter and coarse X.
static void ppu_render_advance_x(sb_ppu_t* ppu) {
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
void sb_ppu_render_pixel(sb_ppu_t* ppu) {
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
      ppu->cached_tile_index = sb_ppu_vram_read(ppu, nt_addr);

      uint16_t attr_addr =
        0x23C0 | (ppu->v & 0x0C00) | ((ppu->v >> 4) & 0x38) | ((ppu->v >> 2) & 0x07);
      ppu->cached_attr = sb_ppu_vram_read(ppu, attr_addr);

      int shift = ((ppu->v >> 1) & 1) | ((ppu->v >> 5) & 2);
      ppu->cached_palette_id = (ppu->cached_attr >> (shift * 2)) & 0x03;

      uint16_t fine_y = (ppu->v >> 12) & 0x07;
      uint16_t pt_base = (ppu->ppuctrl & SB_PPUCTRL_BG_PT) ? 0x1000 : 0;
      uint16_t pattern_addr = pt_base + ppu->cached_tile_index * 16 + fine_y;
      ppu->cached_low = sb_ppu_vram_read(ppu, pattern_addr);
      ppu->cached_high = sb_ppu_vram_read(ppu, pattern_addr + 8);
    }
  }

  // Extract background pixel
  if (bg_enabled) {
    if (pixel_x >= 8 || show_bg_left) {
      int bit = 7 - ppu->fine_x_counter;
      bg_color_idx = ((ppu->cached_high >> bit) & 1) << 1 | ((ppu->cached_low >> bit) & 1);
      bg_palette_id = ppu->cached_palette_id;
      if (bg_color_idx != 0) {
        final_pixel = ppu->palette[bg_palette_id * 4 + bg_color_idx];
      }
    }
  }

  // Sprite rendering
  if (spr_enabled) {
    for (int c = 0; c < ppu->oam_cache_len; c++) {
      int si = ppu->oam_cache[c];
      int oi = si * 4;

      int y = ppu->oam[oi];
      uint8_t tile_index = ppu->oam[oi + 1];
      uint8_t attr = ppu->oam[oi + 2];
      int x = ppu->oam[oi + 3];

      if (pixel_x < x || pixel_x >= x + 8) {
        continue;
      }
      if (pixel_x < 8 && !show_spr_left) {
        continue;
      }

      bool vflip = (attr & 0x80) != 0;
      bool hflip = (attr & 0x40) != 0;
      bool behind_bg = (attr & 0x20) != 0;
      uint8_t sprite_palette_id = attr & 0x03;
      int sprite_height = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_SIZE) ? 16 : 8;

      uint16_t pt_base = (ppu->ppuctrl & SB_PPUCTRL_SPRITE_PT) ? 0x1000 : 0;
      if (sprite_height == 16) {
        pt_base = (tile_index & 1) ? 0x1000 : 0;
        tile_index &= 0xFE;
      }

      int sprite_row = ppu->scanline - y;
      if (vflip) {
        sprite_row = sprite_height - 1 - sprite_row;
      }

      int tile_row_offset = (sprite_height == 16 && sprite_row >= 8) ? 1 : 0;
      int fine_y = sprite_row & 7;
      uint16_t pattern_addr = pt_base + (uint16_t)(tile_index + tile_row_offset) * 16 + fine_y;

      uint8_t low = sb_ppu_vram_read(ppu, pattern_addr);
      uint8_t high = sb_ppu_vram_read(ppu, pattern_addr + 8);

      int bit = hflip ? (pixel_x - x) : (7 - (pixel_x - x));
      uint8_t spr_color_idx = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      if (spr_color_idx == 0) {
        continue;
      }

      // Sprite 0 hit: non-transparent sprite 0 pixel overlaps non-backdrop bg
      if (si == 0 && bg_color_idx != 0 && bg_enabled) {
        if (
          pixel_x >= 8 || (ppu->ppumask & (SB_PPUMASK_SHOW_BG_LEFT | SB_PPUMASK_SHOW_SPR_LEFT)) ==
                            (SB_PPUMASK_SHOW_BG_LEFT | SB_PPUMASK_SHOW_SPR_LEFT)
        ) {
          ppu->ppustatus |= SB_PPUSTATUS_SPRITE0_HIT;
        }
      }

      // Priority
      if (behind_bg && bg_color_idx != 0) {
        continue;
      }

      final_pixel = ppu->palette[0x10 + sprite_palette_id * 4 + spr_color_idx];
      break; // First non-transparent sprite wins
    }
  }

  ppu->framebuffer[fb_index] = final_pixel;

  // Advance VRAM address scroll registers
  if (bg_enabled || spr_enabled) {
    ppu_render_advance_x(ppu);
  }
}
