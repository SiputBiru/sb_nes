#include "./sb_bus.h"

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr) {

  if (addr >= 0x0000 && addr <= 0xFFFF) { // just safe guard
    if (addr < 0x2000) {
      // 2 KB wram-mirrored every 2KB
      bus->last_read = bus->wram[addr & 0x07FF];
    } else if (addr < 0x4000) {
      // bus->last_read = sb_ppu_read(bus->ppu, addr & 0x2007);
    } else if (addr < 0x4020) {
      if (addr == 0x4014) {
        // DMA trigger
      } else {
        // oam dma. controller reads
      }
    } else {
      // bus->last_read = sb_mapper_read(&bus->mapper, bus->prg_rom, addr);
    }
    return bus->last_read;
  }

  return 0x00;
}

uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val) {

  if (addr >= 0x0000 && addr <= 0xFFFF) { // just safe guard
    if (addr == 0x4014) {
      bus->dma_page = val;
      bus->dma_active = true;
    }
    // other memory map
  }

  return 0x00;
}
