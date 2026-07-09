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
    "01-basics",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/01-basics.nes",
    5000000
  );

  run_test(
    "02-implied",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/02-implied.nes",
    5000000
  );

  run_test(
    "03-immediate",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/03-immediate.nes",
    5000000
  );

  run_test(
    "04-zero_page",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/04-zero_page.nes",
    5000000
  );

  run_test(
    "05-zp_xy",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/05-zp_xy.nes",
    5000000
  );

  run_test(
    "06-absolute",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/06-absolute.nes",
    5000000
  );

  run_test(
    "07-abs_xy",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/07-abs_xy.nes",
    5000000
  );

  run_test(
    "08-ind_x",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/08-ind_x.nes",
    5000000
  );

  run_test(
    "09-ind_y",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/09-ind_y.nes",
    5000000
  );

  run_test(
    "10-branches",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/10-branches.nes",
    5000000
  );

  run_test(
    "11-stack",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/11-stack.nes",
    5000000
  );

  run_test(
    "12-jmp_jsr",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/12-jmp_jsr.nes",
    5000000
  );

  run_test("13-rts", "../reference/nes-test-roms/instr_test-v5/rom_singles/13-rts.nes", 5000000);

  run_test("14-rti", "../reference/nes-test-roms/instr_test-v5/rom_singles/14-rti.nes", 5000000);

  run_test("15-brk", "../reference/nes-test-roms/instr_test-v5/rom_singles/15-brk.nes", 5000000);

  run_test(
    "16-special",
    "../reference/nes-test-roms/instr_test-v5/rom_singles/16-special.nes",
    5000000
  );

  printf("\ninstr_test-v5: %d/%d passed\n", passed, total);
  return total - passed;
}
