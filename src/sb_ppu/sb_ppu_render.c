#include "sb_ppu.h"

void sb_ppu_render_scanline(sb_ppu_t* ppu) {
  if (ppu->scanline >= SB_PPU_VISIBLE_SCANLINES)
    return;
  if (!(ppu->ppumask & SB_PPUMASK_SHOW_BG))
    return;

  int fine_y = (ppu->scanline & 7);
  int tile_row = ppu->scanline >> 3;
  bool show_left = (ppu->ppumask & SB_PPUMASK_SHOW_BG_LEFT) != 0;

  uint16_t nt_base = 0x2000 | ((ppu->t >> 10) & 0x03) * 0x400;
  uint16_t pt_base = (ppu->ppuctrl & SB_PPUCTRL_BG_PT) ? 0x1000 : 0;

  for (int tile_col = 0; tile_col < 32; tile_col++) {
    uint16_t tile_addr = nt_base + tile_row * 32 + tile_col;
    uint8_t tile_index = sb_ppu_vram_read(ppu, tile_addr);

    uint16_t attr_addr = nt_base + 0x03C0 +
                         (tile_row / 4) * 8 + (tile_col / 4);
    uint8_t attr = sb_ppu_vram_read(ppu, attr_addr);

    int shift = ((tile_col % 4) / 2) * 2 + ((tile_row % 4) / 2) * 4;
    uint8_t palette_id = (attr >> shift) & 0x03;

    uint16_t pattern_addr = pt_base + (uint16_t)tile_index * 16 + fine_y;
    uint8_t low = sb_ppu_vram_read(ppu, pattern_addr);
    uint8_t high = sb_ppu_vram_read(ppu, pattern_addr + 8);

    for (int pixel = 0; pixel < 8; pixel++) {
      int screen_x = tile_col * 8 + pixel;

      if (screen_x < 8 && !show_left)
        continue;
      if (screen_x >= 256)
        continue;

      int bit = 7 - pixel;
      uint8_t color = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      int fb_index = ppu->scanline * 256 + screen_x;

      if (color != 0) {
        uint8_t entry = palette_id * 4 + color;
        ppu->framebuffer[fb_index] = ppu->palette[entry];
      } else {
        ppu->framebuffer[fb_index] = ppu->palette[0];
      }
    }
  }
}
