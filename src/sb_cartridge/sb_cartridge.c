#include "sb_cartridge.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

sb_cartridge_result_t sb_cartridge_load(sb_cartridge_t* cart,
                                         const uint8_t* data, size_t size)
{
    if (size < 16) return SB_CARTRIDGE_ERR_BAD_MAGIC;

    // Check magic
    if (data[0] != 'N' || data[1] != 'E' ||
        data[2] != 'S' || data[3] != 0x1A)
        return SB_CARTRIDGE_ERR_BAD_MAGIC;

    // Parse header
    uint8_t prg_units    = data[4];
    uint8_t chr_units    = data[5];
    uint8_t flags_byte1  = data[6];
    uint8_t flags_byte2  = data[7];
    uint8_t prg_ram_units = data[8];

    // Detect NES 2.0: bits 2-3 of flags_byte2 == 0x08
    if ((flags_byte2 & 0x0C) == 0x08)
        return SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER;

    // Mapper ID
    uint8_t mapper_id = (flags_byte2 & 0xF0) | (flags_byte1 >> 4);
    if (mapper_id != 0)
        return SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER;
    cart->mapper_id = 0;

    // Mirroring
    if (flags_byte1 & 0x08) {
        cart->mirroring = SB_MIRROR_FOUR_SCREEN;
    } else {
        cart->mirroring = (flags_byte1 & 0x01) ? SB_MIRROR_VERTICAL
                                                : SB_MIRROR_HORIZONTAL;
    }
    cart->battery_backed = (flags_byte1 & 0x02) != 0;

    // Skip trainer if present
    size_t offset = 16;
    if (flags_byte1 & 0x04) offset += 512;

    // PRG-RAM size
    cart->prg_ram_size = (prg_ram_units == 0) ? 8192
                        : (size_t)prg_ram_units * 8192;
    if (cart->prg_ram_size > SB_PRG_RAM_MAX)
        cart->prg_ram_size = SB_PRG_RAM_MAX;

    // Load PRG-ROM
    size_t prg_size = (size_t)prg_units * 16384;
    if (prg_size == 0) return SB_CARTRIDGE_ERR_NO_PRG;
    if (prg_size > SB_PRG_ROM_MAX) return SB_CARTRIDGE_ERR_TOO_LARGE;
    if (offset + prg_size > size) prg_size = size - offset;

    for (size_t i = 0; i < prg_size; i++)
        cart->prg_rom[i] = data[offset + i];
    cart->prg_rom_size = prg_size;
    offset += prg_size;

    // NROM mirror flag: if only 16KB PRG, mirror $8000-$BFFF -> $C000-$FFFF
    cart->prg_mirror = (prg_size <= 16384);

    // Load CHR-ROM (or allocate CHR-RAM)
    size_t chr_size = (size_t)chr_units * 8192;
    if (chr_size > 0) {
        if (chr_size > SB_CHR_ROM_MAX) chr_size = SB_CHR_ROM_MAX;
        if (offset + chr_size > size) chr_size = size - offset;

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

    return SB_CARTRIDGE_OK;
}

sb_cartridge_result_t sb_cartridge_load_from_file(sb_cartridge_t* cart,
                                                   const char* path)
{
    FILE* fp = fopen(path, "rb");
    if (!fp) return SB_CARTRIDGE_ERR_BAD_FILE;

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

uint8_t sb_cartridge_read(sb_cartridge_t* cart, uint16_t addr)
{
    // CPU address space $4020-$FFFF

    if (addr < 0x6000) {
        return 0;  // unmapped / expansion (open bus)
    }

    if (addr < 0x8000) {
        // SRAM $6000-$7FFF
        if (cart->prg_ram_size > 0) {
            size_t index = (addr - 0x6000) % cart->prg_ram_size;
            return cart->prg_ram[index];
        }
        return 0;
    }

    // PRG-ROM $8000-$FFFF
    size_t index = (addr - 0x8000);
    if (cart->prg_mirror) {
        index = index % 16384;  // mirror every 16KB
    }
    if (index < cart->prg_rom_size) {
        return cart->prg_rom[index];
    }
    return 0;
}

void sb_cartridge_write(sb_cartridge_t* cart, uint16_t addr, uint8_t val)
{
    // NROM writes are ignored for PRG-ROM, but SRAM writes work
    if (addr >= 0x6000 && addr < 0x8000 && cart->prg_ram_size > 0) {
        size_t index = (addr - 0x6000) % cart->prg_ram_size;
        cart->prg_ram[index] = val;
    }
    // Writes to $8000-$FFFF are no-ops for NROM (no mapper regs)
}

uint8_t sb_cartridge_read_chr(sb_cartridge_t* cart, uint16_t ppu_addr)
{
    // PPU address space $0000-$1FFF
    if (ppu_addr < cart->chr_rom_size) {
        return cart->chr_rom[ppu_addr];
    }
    return 0;
}

void sb_cartridge_write_chr(sb_cartridge_t* cart, uint16_t ppu_addr, uint8_t val)
{
    // Only writable if CHR-RAM
    if (cart->chr_ram && ppu_addr < cart->chr_rom_size) {
        cart->chr_rom[ppu_addr] = val;
    }
    // Writes to CHR-ROM are silently ignored (matches NES hardware behavior)
}
