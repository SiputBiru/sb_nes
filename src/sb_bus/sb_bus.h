#ifndef SB_BUS_H
#define SB_BUS_H

#include <stdbool.h>
#include <stdint.h>

// Forward declaration (defined in sb_cartridge.h)
struct sb_cartridge_t;

typedef struct {
  uint8_t wram[2048]; // Work RAM (2 KB, mirrored to 0x0800-0x1FFF)

  // last read value for open bus
  uint8_t last_read;

  // Pointers to other components (set during init)
  struct sb_ppu_t* ppu;
  struct sb_apu_t* apu;
  struct sb_cartridge_t* cartridge;

  // DMA state
  bool dma_active;
  uint8_t dma_page;
  uint8_t dma_offset;
  bool dma_dummy;
} sb_bus_t;

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr);
uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val);

#endif
