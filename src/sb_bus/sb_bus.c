#include "sb_bus.h"
#include "../sb_cartridge/sb_cartridge.h"
#include "../sb_ppu/sb_ppu.h"

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr) {
  // Dispatch based on high 3 bits of address (8KB regions)
  switch (addr >> 13) {
  case 0: // $0000-$1FFF: WRAM (2KB, mirrored every 2KB)
  case 1:
    bus->last_read = bus->wram[addr & 0x07FF];
    break;

  case 2: // $2000-$3FFF: PPU registers (mirrored every 8)
  case 3:
    if (bus->ppu)
      bus->last_read = sb_ppu_read(bus->ppu, addr);
    else
      bus->last_read = 0;
    break;

  case 4: // $4000-$5FFF: APU + I/O registers
    if (addr < 0x4020) {
      // APU / controller / DMA
      bus->last_read = 0;
    } else {
      // $4020-$5FFF: Cartridge space (not PRG-ROM)
      if (bus->cartridge)
        bus->last_read = sb_cartridge_read(bus->cartridge, addr);
      else
        bus->last_read = 0;
    }
    break;

  default: // $6000-$FFFF: Cartridge (SRAM + PRG-ROM)
    if (bus->cartridge)
      bus->last_read = sb_cartridge_read(bus->cartridge, addr);
    else
      bus->last_read = 0;
    break;
  }

  return bus->last_read;
}

uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val) {
  switch (addr >> 13) {
  case 0: // $0000-$1FFF: WRAM
  case 1:
    bus->wram[addr & 0x07FF] = val;
    break;

  case 2: // $2000-$3FFF: PPU registers
  case 3:
    if (bus->ppu)
      sb_ppu_write(bus->ppu, addr, val);
    break;

  case 4: // $4000-$5FFF: APU + I/O
    if (addr == 0x4014) {
      // OAM DMA trigger
      if (bus->ppu)
        sb_ppu_oam_dma_start(bus->ppu, val);
    }
    break;

  default: // $6000-$FFFF: Cartridge
    if (bus->cartridge)
      sb_cartridge_write(bus->cartridge, addr, val);
    break;
  }

  return 0;
}
