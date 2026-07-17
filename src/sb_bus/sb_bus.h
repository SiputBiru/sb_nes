#ifndef SB_BUS_H
#define SB_BUS_H

#include <stdbool.h>
#include <stdint.h>

// Forward declaration (defined in sb_cartridge.h)
struct sb_cartridge_t;

typedef struct sb_bus_t {
  uint8_t wram[2048]; // Work RAM (2 KB, mirrored to 0x0800-0x1FFF)

  // last read value for open bus
  uint8_t last_read;

  // Pointers to other components (set during init)
  struct sb_ppu_t* ppu;
  struct sb_apu_t* apu; // FUTURE: APU not yet implemented, always NULL
  struct sb_cartridge_t* cartridge;

  // IRQ pending flag (set by APU frame counter, read by CPU at fetch).
  // Cleared by $4015 read or $4017 write. NMI pending is in PPU struct
  // (bus->ppu->nmi_pending), not here — PPU is on a separate bus in real HW.
  bool irq_pending;

  // Controller state (player 1, $4016)
  // NES protocol: A=bit0, B=bit1, Sel=bit2, Start=bit3, Up=bit4, Dn=bit5,
  // L=bit6, R=bit7 Frontend mask: A=bit7, B=bit6, Sel=bit5, Start=bit4,
  // Up=bit3, Dn=bit2, L=bit1, R=bit0
  uint8_t controller_bits;  // current button state (NES bit order)
  uint8_t controller_index; // current read position (0-8, >7 = open bus)
  bool controller_strobe;   // strobe active: reads return A without advancing
} sb_bus_t;

uint8_t sb_bus_read(sb_bus_t* bus, uint16_t addr);
uint8_t sb_bus_write(sb_bus_t* bus, uint16_t addr, uint8_t val);

#endif
