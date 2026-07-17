
#include "sb_mapper.h"

// forward declaration of func that implement NROM behavior
static uint8_t nrom_read(struct sb_mapper_t* m, uint16_t addr);
static void nrom_write(struct sb_mapper_t* m, uint16_t addr, uint8_t val);
static uint8_t nrom_read_chr(struct sb_mapper_t* m, uint16_t addr);
static void nrom_write_chr(struct sb_mapper_t* m, uint16_t addr, uint8_t val);

// initialize mapper struct with NROM func pointers
void sb_mapper_nrom_init(sb_mapper_t* m) {
  m->read = nrom_read;
  m->write = nrom_write;
  m->read_chr = nrom_read_chr;
  m->write_chr = nrom_write_chr;
  m->on_scanline = NULL; // NROM has no scanline counter
  m->reset = NULL;

  // Pre-compute bank count for mirroring logic.
  m->PRG_banks = (uint16_t)(m->prg_rom_size / 0x4000);
  m->CHR_banks = (uint16_t)(m->chr_rom_size / 0x2000);
}

static uint8_t nrom_read(struct sb_mapper_t* m, uint16_t addr) {

  if (addr < 0x6000) {
    return 0; // open bus / expansion area
  }

  if (addr < 0x8000) {
    // SRAM $6000-$7FFF
    if (m->prg_ram_size > 0) {
      size_t index = (addr - 0x6000) % m->prg_ram_size;
      return m->prg_ram[index];
    }
    return 0;
  }

  // PRG-ROM $8000-$FFFF
  size_t index = (addr - 0x8000);
  if (m->PRG_banks <= 1) {
    // only 16KB: mirror $8000-$BFFF across $C000-$FFFF
    index = index % 0x4000;
  }

  return (index < m->prg_rom_size) ? m->prg_rom[index] : 0;
}

static void nrom_write(struct sb_mapper_t* m, uint16_t addr, uint8_t val) {
  // Only SRAM $6000-$7FFF is writable; other writes are ignored (no mapper regs).
  if (addr >= 0x6000 && addr < 0x8000 && m->prg_ram_size > 0) {
    size_t index = (addr - 0x6000) % m->prg_ram_size;
    m->prg_ram[index] = val;
  }
}
static uint8_t nrom_read_chr(struct sb_mapper_t* m, uint16_t addr) {
  return (addr < m->chr_rom_size) ? m->chr_rom[addr] : 0;
}
static void nrom_write_chr(struct sb_mapper_t* m, uint16_t addr, uint8_t val) {
  if (m->chr_ram && addr < m->chr_rom_size) {
    m->chr_rom[addr] = val;
  }
}
