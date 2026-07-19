#include "sb_cartridge.h"
#include "sb_mapper.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sb_cartridge_result_t sb_cartridge_load(sb_cartridge_t* cart, const uint8_t* data, size_t size) {
  if (size < 16)
    return SB_CARTRIDGE_ERR_BAD_MAGIC;

  // Check magic
  if (data[0] != 'N' || data[1] != 'E' || data[2] != 'S' || data[3] != 0x1A)
    return SB_CARTRIDGE_ERR_BAD_MAGIC;

  // Parse header
  uint8_t prg_units = data[4];
  uint8_t chr_units = data[5];
  uint8_t flags_byte1 = data[6];
  uint8_t flags_byte2 = data[7];
  uint8_t prg_ram_units = data[8];

  // Mapper ID
  uint8_t mapper_id = (flags_byte2 & 0xF0) | (flags_byte1 >> 4);
  cart->mapper_id = mapper_id;

  // Detect NES 2.0: bits 2-3 of flags_byte2 == 0x08
  if ((flags_byte2 & 0x0C) == 0x08)
    return SB_CARTRIDGE_ERR_NES20;

  // if (mapper_id != 0)
  //   return SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER;

  // Mirroring
  if (flags_byte1 & 0x08) {
    cart->mapper.mirroring = SB_MIRROR_FOUR_SCREEN;
  } else {
    cart->mapper.mirroring = (flags_byte1 & 0x01) ? SB_MIRROR_VERTICAL : SB_MIRROR_HORIZONTAL;
  }
  cart->battery_backed = (flags_byte1 & 0x02) != 0;

  // Skip trainer if present
  size_t offset = 16;
  if (flags_byte1 & 0x04)
    offset += 512;

  // PRG-RAM size
  cart->prg_ram_size = (prg_ram_units == 0) ? 8192 : (size_t)prg_ram_units * 8192;
  if (cart->prg_ram_size > SB_PRG_RAM_MAX)
    cart->prg_ram_size = SB_PRG_RAM_MAX;

  // Load PRG-ROM
  size_t prg_size = (size_t)prg_units * 16384;
  if (prg_size == 0)
    return SB_CARTRIDGE_ERR_NO_PRG;
  if (prg_size > SB_PRG_ROM_MAX)
    return SB_CARTRIDGE_ERR_TOO_LARGE;
  if (offset + prg_size > size)
    prg_size = size - offset;

  for (size_t i = 0; i < prg_size; i++)
    cart->prg_rom[i] = data[offset + i];
  cart->prg_rom_size = prg_size;
  offset += prg_size;

  // NROM mirror flag: if only 16KB PRG, mirror $8000-$BFFF -> $C000-$FFFF
  // (handled by nrom_read() via PRG_banks <= 1)

  // Load CHR-ROM (or allocate CHR-RAM)
  size_t chr_size = (size_t)chr_units * 8192;
  if (chr_size > 0) {
    if (chr_size > SB_CHR_ROM_MAX)
      chr_size = SB_CHR_ROM_MAX;
    if (offset + chr_size > size)
      chr_size = size - offset;

    for (size_t i = 0; i < chr_size; i++)
      cart->chr_rom[i] = data[offset + i];
    cart->chr_rom_size = chr_size;
    cart->chr_ram = false;
  } else {
    // No CHR-ROM -> CHR-RAM, zero-initialize (SMB1, Zelda, Metroid)
    cart->chr_rom_size = SB_CHR_RAM_SIZE;
    cart->chr_ram = true;
    for (size_t i = 0; i < SB_CHR_RAM_SIZE; i++)
      cart->chr_rom[i] = 0;
  }

  cart->mapper.prg_rom = cart->prg_rom;
  cart->mapper.prg_rom_size = cart->prg_rom_size;
  cart->mapper.chr_rom = cart->chr_rom;
  cart->mapper.chr_rom_size = cart->chr_rom_size;
  cart->mapper.prg_ram = cart->prg_ram;
  cart->mapper.prg_ram_size = cart->prg_ram_size;
  cart->mapper.mapper_id = cart->mapper_id;
  cart->mapper.chr_ram = cart->chr_ram;

  switch (cart->mapper_id) {
  case 0:
    sb_mapper_nrom_init(&cart->mapper);
    break;
    // TODO: case 1 (sb_mapper_mmc1_init)
  case 2:
    sb_mapper_uxrom_init(&cart->mapper);
    break;
  default:
    return SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER;
  }

  return SB_CARTRIDGE_OK;
}

sb_cartridge_result_t sb_cartridge_load_from_file(sb_cartridge_t* cart, const char* path) {
  FILE* fp = fopen(path, "rb");
  if (!fp)
    return SB_CARTRIDGE_ERR_BAD_FILE;

  // Get file size
  fseek(fp, 0, SEEK_END);
  long fsize = ftell(fp);
  rewind(fp);

  if (fsize <= 0) {
    fclose(fp);
    return SB_CARTRIDGE_ERR_BAD_FILE;
  }

  // Read entire file into temp buffer
  uint8_t* buf = malloc((size_t)fsize);
  if (!buf) {
    fclose(fp);
    return SB_CARTRIDGE_ERR_BAD_FILE;
  }

  size_t bytes_read = fread(buf, 1, (size_t)fsize, fp);
  fclose(fp);

  if (bytes_read != (size_t)fsize) {
    free(buf);
    return SB_CARTRIDGE_ERR_BAD_FILE;
  }

  sb_cartridge_result_t result = sb_cartridge_load(cart, buf, (size_t)fsize);
  free(buf);
  return result;
}
