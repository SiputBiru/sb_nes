#include "blargg_runner.h"
#include <stdio.h>
#include <string.h>

static int total = 0, passed = 0;

static void run_test(const char* name, const char* path, size_t max_cycles) {
  total++;
  sb_blargg_config_t cfg;
  memset(&cfg, 0, sizeof(cfg));
  cfg.rom_path = path;
  cfg.max_cycles = max_cycles;

  sb_blargg_result_t r = sb_blargg_run(&cfg);

  if (r.passed) {
    printf("PASS: %s (%zu cycles)\n", name, r.cycles_run);
    passed++;
  } else {
    printf("FAIL: %s (error=%d, cycles=%zu)\n", name, r.error_code, r.cycles_run);
  }
}

int main(void) {
  run_test(
    "CLI Latency",
    "../reference/nes-test-roms/cpu_interrupts_v2/rom_singles/1-cli_latency.nes",
    5000000
  );

  run_test(
    "NMI and BRK",
    "../reference/nes-test-roms/cpu_interrupts_v2/rom_singles/2-nmi_and_brk.nes",
    5000000
  );

  run_test(
    "NMI and IRQ",
    "../reference/nes-test-roms/cpu_interrupts_v2/rom_singles/3-nmi_and_irq.nes",
    5000000
  );

  run_test(
    "IRQ and DMA",
    "../reference/nes-test-roms/cpu_interrupts_v2/rom_singles/4-irq_and_dma.nes",
    5000000
  );

  run_test(
    "Branch Delays IRQ",
    "../reference/nes-test-roms/cpu_interrupts_v2/rom_singles/5-branch_delays_irq.nes",
    5000000
  );

  printf("\nCPU Interrupts: %d/%d passed\n", passed, total);
  return total - passed;
}
