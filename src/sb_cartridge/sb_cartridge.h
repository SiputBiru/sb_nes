#ifndef SB_CARTRIDGE_H
#define SB_CARTRIDGE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Constants
#define SB_PRG_ROM_MAX (512U * 1024U) // 512 KB max
#define SB_CHR_ROM_MAX (256U * 1024U) // 256 KB max
#define SB_PRG_RAM_MAX (32U * 1024U)  // 32 KB max
#define SB_CHR_RAM_SIZE (8U * 1024U)  // 8KB standard CHR-RAM

typedef enum {
  SB_MIRROR_HORIZONTAL = 0,
  SB_MIRROR_VERTICAL = 1,
  SB_MIRROR_FOUR_SCREEN = 2,
} sb_mirroring_t;

typedef enum {
  SB_CARTRIDGE_OK = 0,
  SB_CARTRIDGE_ERR_BAD_MAGIC,
  SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER,
  SB_CARTRIDGE_ERR_NES20,
  SB_CARTRIDGE_ERR_NO_PRG,
  SB_CARTRIDGE_ERR_TOO_LARGE,
  SB_CARTRIDGE_ERR_BAD_FILE,
} sb_cartridge_result_t;

typedef struct sb_cartridge_t {
  // ROM data buffers
  uint8_t prg_rom[SB_PRG_ROM_MAX];
  size_t prg_rom_size;
  uint8_t chr_rom[SB_CHR_ROM_MAX]; // CHR-ROM data or CHR-RAM workspace
  size_t chr_rom_size;
  uint8_t prg_ram[SB_PRG_RAM_MAX];
  size_t prg_ram_size;

  // Cartridge properties (parsed from header)
  uint8_t mapper_id;
  sb_mirroring_t mirroring;
  bool battery_backed;
  bool chr_ram; // true: chr_rom buffer is CHR-RAM (writable)
                // false: chr_rom buffer is CHR-ROM (read-only)

  // PRG-ROM mirroring flag for NROM
  bool prg_mirror; // true: $C000-$FFFF mirrors $8000-$BFFF (16KB PRG)
                   // false: $C000-$FFFF is the second 16KB (32KB PRG)
} sb_cartridge_t;

// Load ROM from a raw byte buffer (for tests / embedded ROMs).
sb_cartridge_result_t sb_cartridge_load(sb_cartridge_t *cart,
                                        const uint8_t *data, size_t size);

// Load ROM from a file path.
sb_cartridge_result_t sb_cartridge_load_from_file(sb_cartridge_t *cart,
                                                  const char *path);

// NROM mapper operations (called by the bus)
uint8_t sb_cartridge_read(sb_cartridge_t *cart, uint16_t cpu_addr);
void sb_cartridge_write(sb_cartridge_t *cart, uint16_t cpu_addr, uint8_t val);

// PPU reads CHR through this (Phase 3)
uint8_t sb_cartridge_read_chr(sb_cartridge_t *cart, uint16_t ppu_addr);
void sb_cartridge_write_chr(sb_cartridge_t *cart, uint16_t ppu_addr,
                            uint8_t val);

#endif
