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
  run_test("CPU Timing Test 6", "test/blargg/roms/cpu_timing_test.nes", 5000000);

  printf("\nCPU Timing: %d/%d passed\n", passed, total);
  return total - passed;
}
