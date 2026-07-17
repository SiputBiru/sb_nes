#include "sb_bus.h"
#include "../sb_cartridge/sb_cartridge.h"
#include "../sb_nes.h"
#include "../sb_ppu/sb_ppu.h"

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr) {
  // split into 8KB regions
  switch (addr >> 13) {
  case 0: // $0000-$1FFF: WRAM (2KB, mirrored every 2KB)
    bus->last_read = bus->wram[addr & 0x07FF];
    break;

  case 1: // $2000-$3FFF: PPU registers (mirrored every 8)
    if (bus->ppu) {
      bus->last_read = sb_ppu_read(bus->ppu, addr);
    } else
      bus->last_read = 0;
    break;

  case 2: // $4000-$5FFF: APU + I/O registers
    if (addr < 0x4020) {
      // APU status ($4015)
      if (addr == 0x4015) {
        bus->last_read = sb_apu_read(bus->apu, bus, addr);
      } else
        // Controller port 1 ($4016)
        if (addr == 0x4016) {
          if (bus->controller_strobe) {
            // Strobe active: always return A button (bit 0), no advance
            bus->last_read = bus->controller_bits & 1;
          } else if (bus->controller_index > 7) {
            // After 8 reads, open bus returns 1
            bus->last_read = 1;
          } else {
            // Return current bit (LSB first: A=bit0, B=bit1, ...)
            bus->last_read = (bus->controller_bits >> bus->controller_index) & 1;
            bus->controller_index++;
          }
        } else {
          bus->last_read = 0;
        }
    } else {
      if (bus->cartridge)
        // bus->last_read = sb_cartridge_read(bus->cartridge, addr);
        bus->last_read = bus->cartridge->mapper.read(&bus->cartridge->mapper, addr);
      else
        bus->last_read = 0;
    }
    break;

  case 3: // $6000-$7FFF: Cartridge (SRAM)
    if (bus->cartridge)
      // bus->last_read = sb_cartridge_read(bus->cartridge, addr);
      bus->last_read = bus->cartridge->mapper.read(&bus->cartridge->mapper, addr);
    else
      bus->last_read = 0;
    break;

  default: // $8000-$FFFF: Cartridge (PRG-ROM)
    if (bus->cartridge)
      // bus->last_read = sb_cartridge_read(bus->cartridge, addr);
      bus->last_read = bus->cartridge->mapper.read(&bus->cartridge->mapper, addr);
    else
      bus->last_read = 0;
    break;
  }

  return bus->last_read;
}

uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val) {
  // split into 8KB regions
  switch (addr >> 13) {
  case 0: // $0000-$1FFF: WRAM
    bus->wram[addr & 0x07FF] = val;
    break;

  case 1: // $2000-$3FFF: PPU registers
    if (bus->ppu)
      sb_ppu_write(bus->ppu, addr, val);
    break;

  case 2: // $4000-$5FFF: APU + I/O
    if (addr == 0x4014) {
      if (bus->ppu)
        sb_ppu_oam_dma_start(bus->ppu, val);
    } else if (addr == 0x4017) {
      sb_apu_write(bus->apu, bus, addr, val);
    } else if (addr == 0x4016) {
      // Strobe: writing bit0=1 resets index and keeps strobe active
      // writing bit0=0 ends strobe, allowing reads to advance
      bus->controller_strobe = (val & 0x01) != 0;
      if (bus->controller_strobe)
        bus->controller_index = 0;
    }
    break;

  default: // $6000-$FFFF: Cartridge
    if (bus->cartridge)
      bus->cartridge->mapper.write(&bus->cartridge->mapper, addr, val);
    break;
  }

  return 0;
}
