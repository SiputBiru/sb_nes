#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../src/sb_cartridge/sb_cartridge.h"

static int total = 0;
static int passed = 0;

#define TEST(name, cond) do { \
    total++; \
    if (!(cond)) { printf("FAIL: %s\n", name); } \
    else { passed++; printf("PASS: %s\n", name); } \
} while(0)

// Helper: build an iNES header + body
// Returns a heap-allocated buffer with header + PRG + CHR.
// Caller must free().
static uint8_t* build_rom(uint8_t prg_units, uint8_t chr_units,
                          uint8_t flags1, uint8_t flags2)
{
    size_t prg_bytes = (size_t)prg_units * 16384;
    size_t chr_bytes = (size_t)chr_units * 8192;
    size_t total_size = 16 + prg_bytes + chr_bytes;

    uint8_t* rom = (uint8_t*)malloc(total_size);
    if (!rom) return NULL;
    memset(rom, 0, total_size);

    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    rom[4] = prg_units;
    rom[5] = chr_units;
    rom[6] = flags1;
    rom[7] = flags2;

    // Fill PRG with a recognizable pattern
    for (size_t i = 0; i < prg_bytes; i++)
        rom[16 + i] = (uint8_t)(i & 0xFF);

    // Fill CHR
    for (size_t i = 0; i < chr_bytes; i++)
        rom[16 + prg_bytes + i] = (uint8_t)(0xFF - (i & 0xFF));

    return rom;
}

// Header parsing tests

static void test_valid_nrom_16kb(void)
{
    uint8_t* rom = build_rom(1, 0, 0x00, 0x00);
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);

    TEST("16KB NROM loads OK",                    r == SB_CARTRIDGE_OK);
    TEST("PRG size = 16384",                       cart.prg_rom_size == 16384);
    TEST("CHR size = 8192 (CHR-RAM default)",      cart.chr_rom_size == 8192);
    TEST("chr_ram = true",                         cart.chr_ram == true);
    TEST("mirroring = horizontal",                 cart.mirroring == SB_MIRROR_HORIZONTAL);
    TEST("mapper = 0",                             cart.mapper_id == 0);
    TEST("prg_mirror = true (only 16KB)",          cart.prg_mirror == true);
}

static void test_valid_nrom_32kb(void)
{
    uint8_t* rom = build_rom(2, 1, 0x01, 0x00);
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, 16 + 32768 + 8192);
    free(rom);

    TEST("32KB NROM loads OK",                     r == SB_CARTRIDGE_OK);
    TEST("PRG size = 32768",                       cart.prg_rom_size == 32768);
    TEST("CHR size = 8192",                        cart.chr_rom_size == 8192);
    TEST("chr_ram = false (CHR-ROM present)",      cart.chr_ram == false);
    TEST("mirroring = vertical",                   cart.mirroring == SB_MIRROR_VERTICAL);
    TEST("prg_mirror = false (32KB fills space)",  cart.prg_mirror == false);
}

static void test_bad_magic(void)
{
    uint8_t rom[16] = {0};  // no "NES\x1A"
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, sizeof(rom));
    TEST("Bad magic returns ERR_BAD_MAGIC", r == SB_CARTRIDGE_ERR_BAD_MAGIC);
}

static void test_unsupported_mapper(void)
{
    uint8_t* rom = build_rom(1, 0, 0x10, 0x00);  // mapper low nybble = 1 (MMC1)
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);
    TEST("MMC1 (mapper 1) returns ERR_UNSUPPORTED_MAPPER",
         r == SB_CARTRIDGE_ERR_UNSUPPORTED_MAPPER);
}

static void test_nes2_rejected(void)
{
    uint8_t* rom = build_rom(1, 0, 0x00, 0x08);  // NES 2.0 marker
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);
    TEST("NES 2.0 format returns ERR_NES20",
         r == SB_CARTRIDGE_ERR_NES20);
}

// ROM read tests

static void test_nrom_read(void)
{
    uint8_t* rom = build_rom(1, 0, 0x00, 0x00);
    sb_cartridge_t cart;
    sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);

    uint8_t v = sb_cartridge_read(&cart, 0x8000);
    TEST("Read $8000 = first byte of PRG", v == 0x00);

    v = sb_cartridge_read(&cart, 0xC000);
    TEST("Read $C000 = mirror of $8000 (16KB)", v == 0x00);

    v = sb_cartridge_read(&cart, 0xC001);
    TEST("Read $C001 = byte 1 (mirror)", v == 0x01);
}

static void test_nrom_read_32kb(void)
{
    uint8_t* rom = build_rom(2, 0, 0x00, 0x00);
    sb_cartridge_t cart;
    sb_cartridge_load(&cart, rom, 16 + 32768);
    free(rom);

    uint8_t v = sb_cartridge_read(&cart, 0x8000);
    TEST("32KB: Read $8000 = byte 0", v == 0x00);

    v = sb_cartridge_read(&cart, 0xC000);
    TEST("32KB: Read $C000 = byte 16384 (second bank)",
         v == (uint8_t)(16384 & 0xFF));
}

// SRAM test

static void test_sram(void)
{
    uint8_t* rom = build_rom(1, 0, 0x00, 0x00);
    sb_cartridge_t cart;
    sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);

    sb_cartridge_write(&cart, 0x6000, 0xAB);
    uint8_t v = sb_cartridge_read(&cart, 0x6000);
    TEST("SRAM write+read at $6000", v == 0xAB);

    v = sb_cartridge_read(&cart, 0x6001);
    TEST("SRAM $6001 = 0 (not written)", v == 0x00);

    // Write to PRG area ($8000) should be ignored
    sb_cartridge_write(&cart, 0x8000, 0xFF);
    v = sb_cartridge_read(&cart, 0x8000);
    TEST("Write to $8000 ignored (NROM no mapper regs)", v == 0x00);
}

// CHR tests

static void test_chr_read(void)
{
    uint8_t* rom = build_rom(1, 1, 0x00, 0x00);  // 8KB CHR-ROM
    sb_cartridge_t cart;
    sb_cartridge_load(&cart, rom, 16 + 16384 + 8192);
    free(rom);

    uint8_t v = sb_cartridge_read_chr(&cart, 0x0000);
    TEST("CHR-ROM read at $0000", v == 0xFF);

    sb_cartridge_write_chr(&cart, 0x0000, 0x00);
    v = sb_cartridge_read_chr(&cart, 0x0000);
    TEST("CHR-ROM write ignored", v == 0xFF);
}

static void test_chr_ram(void)
{
    uint8_t* rom = build_rom(1, 0, 0x00, 0x00);  // no CHR-ROM -> CHR-RAM
    sb_cartridge_t cart;
    sb_cartridge_load(&cart, rom, 16 + 16384);
    free(rom);

    uint8_t v = sb_cartridge_read_chr(&cart, 0x0000);
    TEST("CHR-RAM read at $0000 = 0 (zero-initialized)", v == 0x00);

    sb_cartridge_write_chr(&cart, 0x0123, 0xAB);
    v = sb_cartridge_read_chr(&cart, 0x0123);
    TEST("CHR-RAM write+read at $0123", v == 0xAB);
}

// Edge cases

static void test_no_prg(void)
{
    uint8_t rom[16];
    memset(rom, 0, sizeof(rom));
    rom[0] = 'N'; rom[1] = 'E'; rom[2] = 'S'; rom[3] = 0x1A;
    // prg_units = 0, should fail
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, rom, sizeof(rom));
    TEST("No PRG-ROM returns ERR_NO_PRG", r == SB_CARTRIDGE_ERR_NO_PRG);
}

static void test_small_file(void)
{
    uint8_t buf[4] = {0};
    sb_cartridge_t cart;
    sb_cartridge_result_t r = sb_cartridge_load(&cart, buf, 4);
    TEST("File < 16 bytes returns ERR_BAD_MAGIC", r == SB_CARTRIDGE_ERR_BAD_MAGIC);
}

int main(void)
{
    test_valid_nrom_16kb();
    test_valid_nrom_32kb();
    test_bad_magic();
    test_unsupported_mapper();
    test_nes2_rejected();
    test_nrom_read();
    test_nrom_read_32kb();
    test_sram();
    test_chr_read();
    test_chr_ram();
    test_no_prg();
    test_small_file();

    printf("\nResults\n");
    printf("Passed: %d / %d\n", passed, total);
    return total - passed;
}
