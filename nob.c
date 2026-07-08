#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define TEST_FOLDER "test/"

// Common flags (no -Wconversion — too noisy for C99 integer promotion)
#define CFLAGS "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", \
               "-fsanitize=address", "-fsanitize=undefined", \
               "-g", "-O0"

static int build_nestest(void)
{
  Nob_Cmd cmd = { 0 };
  nob_cc(&cmd);
  nob_cc_flags(&cmd);
  nob_cmd_append(&cmd, CFLAGS);
  nob_cc_output(&cmd, BUILD_FOLDER "test_nestest");
  nob_cc_inputs(
    &cmd,
    SRC_FOLDER "sb_6502/sb_6502.c",
    SRC_FOLDER "sb_6502/sb_6502_addrmodes.c",
    SRC_FOLDER "sb_bus/sb_bus.c",
    TEST_FOLDER "nestest/test_nestest.c"
  );
  if (!nob_cmd_run(&cmd)) return 1;

  Nob_Cmd run = { 0 };
  nob_cmd_append(&run, BUILD_FOLDER "test_nestest");
  if (!nob_cmd_run(&run)) return 1;

  return 0;
}

static int build_cartridge_test(void)
{
  Nob_Cmd cmd = { 0 };
  nob_cc(&cmd);
  nob_cc_flags(&cmd);
  nob_cmd_append(&cmd, CFLAGS);
  nob_cc_output(&cmd, BUILD_FOLDER "test_cartridge");
  nob_cc_inputs(
    &cmd,
    SRC_FOLDER "sb_cartridge/sb_cartridge.c",
    TEST_FOLDER "cartridge/test_cartridge.c"
  );
  if (!nob_cmd_run(&cmd)) return 1;

  Nob_Cmd run = { 0 };
  nob_cmd_append(&run, BUILD_FOLDER "test_cartridge");
  if (!nob_cmd_run(&run)) return 1;

  return 0;
}

int main(int argc, char** argv)
{
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  int run_tests = (argc > 1 && strcmp(argv[1], "test") == 0);

  if (run_tests) {
    printf("=== Cartridge Loader Tests ===\n");
    if (build_cartridge_test()) return 1;

    printf("\n=== nestest CPU Tests ===\n");
    if (build_nestest()) return 1;

    return 0;
  }

  // Default: build nestest (existing behavior)
  Nob_Cmd cmd = { 0 };
  nob_cc(&cmd);
  nob_cc_flags(&cmd);
  nob_cmd_append(&cmd, CFLAGS);
  nob_cc_output(&cmd, BUILD_FOLDER "test_nestest");
  nob_cc_inputs(
    &cmd,
    SRC_FOLDER "sb_6502/sb_6502.c",
    SRC_FOLDER "sb_6502/sb_6502_addrmodes.c",
    SRC_FOLDER "sb_bus/sb_bus.c",
    TEST_FOLDER "nestest/test_nestest.c"
  );
  if (!nob_cmd_run(&cmd)) return 1;

  return 0;
}
