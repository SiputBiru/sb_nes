#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../../src/sb_6502/sb_6502.h"
#include "../../src/sb_bus/sb_bus.h"

int main(void) {
  FILE* rom = fopen("test/nestest/nestest.nes", "rb");
  if (!rom) {
    printf("ERROR: Could not open nestest.nes\n");
    return 1;
  }

  uint8_t header[16];
  fread(header, 1, 16, rom);

  if (header[0] != 'N' || header[1] != 'E' || header[2] != 'S' || header[3] != 0x1A) {
    printf("ERROR: Invalid iNES header\n");
    fclose(rom);
    return 1;
  }

  size_t prg_size = (size_t)header[4] * 16384;
  if (prg_size == 0) {
    printf("ERROR: No PRG-ROM");
    fclose(rom);
    return 1;
  }

  uint8_t* prg_rom = malloc(prg_size);
  fread(prg_rom, 1, prg_size, rom);
  fclose(rom);

  sb_6502_t cpu;
  sb_bus_t bus;

  memset(&cpu, 0, sizeof(cpu));
  memset(&bus, 0, sizeof(bus));

  bus.prg_rom = prg_rom;
  bus.prg_rom_size = prg_size;

  sb_6502_init_opcodes();

  sb_6502_reset(&cpu, &bus);

  // nestest starts at $C000, initial P = $24, cycles = 7
  cpu.pc = 0xC000;
  cpu.p = 0x24;
  cpu.cycles = 7;

  FILE* log = fopen("test/nestest/nestest.log", "r");
  if (!log) {
    printf("ERROR: Could not open nestest.log\n");
    free(prg_rom);
    return 1;
  }

  // Skip line 1 (initial state )
  char expected[256];
  fgets(expected, sizeof(expected), log);

  int passed = 0;
  int failed = 0;
  int line_num = 1;

  printf("Running nestest...\n");

  for (int i = 0; i < 10000; i++) {
    // Execute one CPU instruction FIRST
    sb_6502_step(&cpu, &bus);

    if (!fgets(expected, sizeof(expected), log)) {
      break;
    }
    line_num++;

    // Build output string (register portion only)
    char our_output[256];
    snprintf(
      our_output,
      sizeof(our_output),
      "A:%02X X:%02X Y:%02X P:%02X SP:%02X CYC:%llu",
      cpu.a,
      cpu.x,
      cpu.y,
      cpu.p,
      cpu.s,
      (unsigned long long)cpu.cycles
    );

    // Find "A:" in the expected line
    char* expected_regs = strstr(expected, "A:");
    if (expected_regs) {
      // Trim at "PPU:" (don't track PPU cycles yet)
      char* ppu_pos = strstr(expected_regs, "PPU:");
      if (ppu_pos) {
        char expected_trimmed[256];
        size_t len = (size_t)(ppu_pos - expected_regs);
        strncpy(expected_trimmed, expected_regs, len);
        expected_trimmed[len] = '\0';

        if (strncmp(our_output, expected_trimmed, strlen(expected_trimmed)) == 0) {
          passed++;
        } else {
          failed++;
          printf("FAIL line %d:\n", line_num);
          printf("  Expected: %s", expected);
          printf("  Got:      %s\n", our_output);
          if (failed >= 10) {
            printf("\nToo many failures, stopping.\n");
            break;
          }
        }
      }
    }
  }

  fclose(log);
  free(prg_rom);

  printf("\n---\n");
  printf("Passed: %d\n", passed);
  printf("Failed: %d\n", failed);
  printf("Total:  %d\n", passed + failed);

  if (failed == 0) {
    printf("\nAll instructions match! CPU is correct!\n");
  } else {
    printf("\nSome instructions don't match. Check the failures above.\n");
  }

  return failed;
}
