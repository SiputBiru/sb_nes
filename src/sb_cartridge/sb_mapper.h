#ifndef SB_MAPPER_H
#define SB_MAPPER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
  SB_MIRROR_HORIZONTAL = 0,
  SB_MIRROR_VERTICAL = 1,
  SB_MIRROR_FOUR_SCREEN = 2,
} sb_mirroring_t;

struct sb_mapper_t;

typedef uint8_t (*sb_mapper_read_fn)(struct sb_mapper_t*, uint16_t addr); // $6000-$FFFF
typedef void (*sb_mapper_write_fn)(struct sb_mapper_t*, uint16_t addr, uint8_t val);
typedef uint8_t (*sb_mapper_read_chr_fn)(struct sb_mapper_t*, uint16_t addr); // PPU $0000-$1FFF
typedef void (*sb_mapper_write_chr_fn)(struct sb_mapper_t*, uint16_t addr, uint8_t val);
typedef void (*sb_mapper_on_scanline_fn)(struct sb_mapper_t*); // for MMC3 IRQ
typedef void (*sb_mapper_reset_fn)(struct sb_mapper_t*);

typedef struct sb_mapper_t {
  // Pointers to cartridge ROM data. Set by cartridge loader after ROM is
  // copied into the static arrays. Mappers read through these pointers.
  uint8_t* prg_rom;
  size_t prg_rom_size;
  uint8_t* chr_rom;
  size_t chr_rom_size;
  uint8_t* prg_ram;
  size_t prg_ram_size;

  // Cartridge properties (parsed from iNES header, cached here for mappers).
  uint8_t mapper_id;

  // Nametable mirroring mode (horizontal, vertical, or four-screen).
  // Used by PPU address translation via ppu_real_addr().
  sb_mirroring_t mirroring;
  bool chr_ram;

  // Bank pointer tables for bank-switching mappers
  // PRG_ptrs[0..1] cover $8000-$BFFF to select which bank is at each window
  // CHR_ptrs[0..7] cover PPU $0000-$1FFF in 1KB banks.
  uint8_t* PRG_ptrs[8];
  uint8_t* CHR_ptrs[8];

  // bank regs ( written by mappers, consumed by read/write functions ).
  uint8_t PRG_reg[8];
  uint8_t CHR_reg[8];
  uint16_t PRG_banks; // Number of 16KB PRG banks in ROM
  uint16_t CHR_banks; // Number of 8KB CHR banks in ROM

  // Func Pointers / mapper interfaces
  sb_mapper_read_fn read;               // CPU $6000-$FFFF
  sb_mapper_write_fn write;             // CPU $6000-$FFFF
  sb_mapper_read_chr_fn read_chr;       // PPU $0000-1FFFF
  sb_mapper_write_chr_fn write_chr;     // PPU $0000-1FFFF
  sb_mapper_on_scanline_fn on_scanline; // Called each scanline (MMC3 IRQ)
  sb_mapper_reset_fn reset;             // Called on mapper reset

  // Mapper-specific state. Each mapper's init function allocates this.
  // Freed by sb_cartridge when the cartridge is unloaded.
  void* extension;
} sb_mapper_t;

// init functions for each mapper, called by sb_cartridge_load()
void sb_mapper_nrom_init(sb_mapper_t* m);
void sb_mapper_uxrom_init(sb_mapper_t* m);
void sb_mapper_mmc1_init(sb_mapper_t* m);

#endif // !SB_MAPPER
