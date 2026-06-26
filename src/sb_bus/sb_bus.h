#ifndef SB_BUS_H

#define SB_BUS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {

  uint8_t wram[2048]; // Work RAM (2 KB, mirrored to 0x0800-0x1FFF)

  // last read value for open bus
  uint8_t last_read;

  // pointer to other components
  struct sb_ppu_t* ppu;
  struct sb_apu_t* apu;
  struct sb_mapper_t* mapper;

  const uint8_t* prg_rom;
  size_t prg_rom_size;

  // DMA state
  bool dma_active;    // High byte of source address
  uint8_t dma_page;   // current byte being transferred
  uint8_t dma_offset; // first dummy read cycle
  bool dma_dummy;

} sb_bus_t;

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr);
uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val);

#endif
