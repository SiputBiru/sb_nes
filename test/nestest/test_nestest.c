#include <stdio.h>
#include <string.h>

#include "../../src/sb_6502/sb_6502.h"
#include "../../src/sb_cartridge/sb_cartridge.h"

int main(void) {
  // Load nestest ROM through the cartridge loader
  sb_cartridge_t cart;
  sb_cartridge_result_t cr = sb_cartridge_load_from_file(&cart, "test/nestest/nestest.nes");
  if (cr != SB_CARTRIDGE_OK) {
    printf("ERROR: Could not load nestest.nes (error %d)\n", cr);
    return 1;
  }

  sb_6502_t cpu;
  sb_bus_t bus;

  memset(&cpu, 0, sizeof(cpu));
  memset(&bus, 0, sizeof(bus));

  // Wire cartridge into bus
  bus.cartridge = &cart;

  sb_6502_init_opcodes();

  sb_6502_reset(&cpu, &bus);

  // nestest starts at $C000, initial P = $24, cycles = 7
  cpu.pc = 0xC000;
  cpu.p = 0x24;
  cpu.cycles = 7;

  FILE* log = fopen("test/nestest/nestest.log", "r");
  if (!log) {
    printf("ERROR: Could not open nestest.log\n");
    return 1;
  }

  // Skip line 1 (initial state)
  char expected[256];
  fgets(expected, sizeof(expected), log);

  int passed = 0;
  int failed = 0;
  int line_num = 1;

  printf("Running nestest...\n");

  for (int i = 0; i < 10000; i++) {
    // Execute one CPU instruction by ticking cycles until the phase/opcode reset
    do {
      sb_6502_cycle(&cpu, &bus);
    } while (cpu.opcode != 0 || cpu.phase != 0);

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
