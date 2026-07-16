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
  run_test("01-basics", "test/blargg/roms/01-basics.nes", 5000000);

  run_test("02-implied", "test/blargg/roms/02-implied.nes", 5000000);

  run_test("03-immediate", "test/blargg/roms/03-immediate.nes", 5000000);

  run_test("04-zero_page", "test/blargg/roms/04-zero_page.nes", 5000000);

  run_test("05-zp_xy", "test/blargg/roms/05-zp_xy.nes", 5000000);

  run_test("06-absolute", "test/blargg/roms/06-absolute.nes", 5000000);

  run_test("07-abs_xy", "test/blargg/roms/07-abs_xy.nes", 5000000);

  run_test("08-ind_x", "test/blargg/roms/08-ind_x.nes", 5000000);

  run_test("09-ind_y", "test/blargg/roms/09-ind_y.nes", 5000000);

  run_test("10-branches", "test/blargg/roms/10-branches.nes", 5000000);

  run_test("11-stack", "test/blargg/roms/11-stack.nes", 5000000);

  run_test("12-jmp_jsr", "test/blargg/roms/12-jmp_jsr.nes", 5000000);

  run_test("13-rts", "test/blargg/roms/13-rts.nes", 5000000);

  run_test("14-rti", "test/blargg/roms/14-rti.nes", 5000000);

  run_test("15-brk", "test/blargg/roms/15-brk.nes", 5000000);

  run_test("16-special", "test/blargg/roms/16-special.nes", 5000000);

  printf("\ninstr_test-v5: %d/%d passed\n", passed, total);
  return total - passed;
}
