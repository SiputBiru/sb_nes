#include "sb_mapper.h"

// forward declaration of func that implement NROM behavior
static uint8_t uxrom_read(struct sb_mapper_t* m, uint16_t addr);
static void uxrom_write(struct sb_mapper_t* m, uint16_t addr, uint8_t val);
static uint8_t uxrom_read_chr(struct sb_mapper_t* m, uint16_t addr);
static void uxrom_write_chr(struct sb_mapper_t* m, uint16_t addr, uint8_t val);

void sb_mapper_uxrom_init(sb_mapper_t* m) {

  m->read = uxrom_read;
  m->write = uxrom_write;
  m->read_chr = uxrom_read_chr;
  m->write_chr = uxrom_write_chr;
  m->on_scanline = NULL; // NROM has no scanline counter
  m->reset = NULL;

  // Pre-compute bank count for mirroring logic.
  m->PRG_banks = (uint16_t)(m->prg_rom_size / 0x4000);

  // Initialise bank pointers.
  // $C000-$FFFF is fixed to the last bank.
  // $8000-$BFFF starts at bank 0.
  m->PRG_ptrs[0] = m->prg_rom; // $8000
                               //
  // Slot 1 explicitly anchors the fixed $C000-$FFFF region to the LAST bank
  if (m->PRG_banks > 0) {
    m->PRG_ptrs[1] = m->prg_rom + (size_t)(m->PRG_banks - 1) * 0x4000;
  } else {
    m->PRG_ptrs[1] = m->prg_rom; // Safe fallback path
  }
}

static uint8_t uxrom_read(struct sb_mapper_t* m, uint16_t addr) {

  if (addr < 0x6000) {
    return 0; // open bus
  }

  if (addr < 0x8000) {
    if (m->prg_ram_size > 0) {
      return m->prg_ram[(addr - 0x6000) % m->prg_ram_size];
    } else {
      return 0;
    }
  }

  if (addr >= 0xC000) {
    // fixed window ($C000-$FFFF) -> maps internal index 0x0000-0x3FFF via pointer array 1
    return m->PRG_ptrs[1][addr - 0xC000];
  } else {
    return m->PRG_ptrs[0][addr - 0x8000];
  }
}

static void uxrom_write(struct sb_mapper_t* m, uint16_t addr, uint8_t val) {

  if (addr < 0x8000) {
    if (addr >= 0x6000 && m->prg_ram_size > 0)
      m->prg_ram[(addr - 0x6000) % m->prg_ram_size] = val;
    return;
  }

  // bank select register access: Any write to $0000-$FFFF update the switchable window.
  if (m->PRG_banks > 0) {
    uint8_t target_bank = val % m->PRG_banks;
    m->PRG_ptrs[0] = m->prg_rom + (size_t)target_bank * 0x4000;
  }
}

static uint8_t uxrom_read_chr(struct sb_mapper_t* m, uint16_t addr) {
  // uXROM features standard 8KB unbanked VRAM/RAM
  return (addr < m->chr_rom_size) ? m->chr_rom[addr] : 0;
}

static void uxrom_write_chr(struct sb_mapper_t* m, uint16_t addr, uint8_t val) {
  if (m->chr_ram && addr < m->chr_rom_size)
    m->chr_rom[addr] = val;
}
