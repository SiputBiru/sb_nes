#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define TEST_FOLDER "test/"

// Common flags (no -Wconversion — too noisy for C99 integer promotion)
#define CFLAGS "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-g", "-O0"

// Flags for test builds (include sanitizers — no SDL linked here)
#define CFLAGS_TEST \
  "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-fsanitize=address", \
    "-fsanitize=undefined", "-g", "-O0"

// All core source files needed to link a test binary
#define CORE_SOURCES \
  SRC_FOLDER "sb_6502/sb_6502.c", SRC_FOLDER "sb_6502/sb_6502_addrmodes.c", \
    SRC_FOLDER "sb_bus/sb_bus.c", SRC_FOLDER "sb_cartridge/sb_cartridge.c", SRC_FOLDER "sb_nes.c", \
    SRC_FOLDER "sb_ppu/sb_ppu.c"

static int build_and_run(Nob_Cmd* cmd, const char* output, const char* extra_flags) {
  // Build: insert compiler, flags, and extra flags at the front
  Nob_Cmd front = { 0 };
  nob_cc(&front);
  nob_cc_flags(&front);
  nob_cmd_append(&front, CFLAGS_TEST);
  if (extra_flags)
    nob_cmd_append(&front, extra_flags);
  nob_cc_output(&front, output);
  // Append existing inputs from cmd
  for (int i = 0; i < cmd->count; i++)
    nob_cmd_append(&front, cmd->items[i]);
  *cmd = front;

  if (!nob_cmd_run(cmd))
    return 1;

  Nob_Cmd run = { 0 };
  nob_cmd_append(&run, output);
  if (!nob_cmd_run(&run))
    return 1;
  return 0;
}

static int build_nestest(void) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, TEST_FOLDER "nestest/test_nestest.c");
  return build_and_run(&cmd, BUILD_FOLDER "test_nestest", NULL);
}

static int build_cartridge_test(void) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, SRC_FOLDER "sb_cartridge/sb_cartridge.c");
  nob_cmd_append(&cmd, TEST_FOLDER "cartridge/test_cartridge.c");
  return build_and_run(&cmd, BUILD_FOLDER "test_cartridge", NULL);
}

static int build_blargg_test(const char* test_name, const char* test_file) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, TEST_FOLDER "blargg/blargg_runner.c");
  nob_cmd_append(&cmd, test_file);

  char output[256];
  snprintf(output, sizeof(output), BUILD_FOLDER "%s", test_name);
  return build_and_run(&cmd, output, NULL);
}

static int build_emulator(void) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, SRC_FOLDER "sb_frontend/sb_frontend.c");
  nob_cmd_append(&cmd, "sb_main.c");

  Nob_Cmd front = { 0 };
  nob_cc(&front);
  nob_cc_flags(&front);
  nob_cmd_append(&front, CFLAGS);
  // nob_cmd_append(&front, "-I/usr/include/SDL3", "-lSDL3");
  nob_cmd_append(&front, "-lSDL3");
  nob_cc_output(&front, BUILD_FOLDER "sb_nes");
  for (int i = 0; i < cmd.count; i++)
    nob_cmd_append(&front, cmd.items[i]);
  cmd = front;

  if (!nob_cmd_run(&cmd))
    return 1;
  return 0;
}

static int build_branch_timing(void) {
  return build_blargg_test("test_branch_timing", TEST_FOLDER "blargg/test_branch_timing.c");
}

static int build_cpu_timing(void) {
  return build_blargg_test("test_cpu_timing", TEST_FOLDER "blargg/test_cpu_timing.c");
}

static int build_cpu_interrupts(void) {
  return build_blargg_test("test_cpu_interrupts", TEST_FOLDER "blargg/test_cpu_interrupts.c");
}

static int build_instr_v5(void) {
  return build_blargg_test("test_instr_v5", TEST_FOLDER "blargg/test_instr_v5.c");
}

static int build_all_blargg(void) {
  printf("Branch Timing Tests:\n");
  if (build_branch_timing())
    return 1;

  printf("\nCPU Timing Tests:\n");
  if (build_cpu_timing())
    return 1;

  printf("\nCPU Interrupt Tests:\n");
  if (build_cpu_interrupts())
    return 1;

  printf("\ninstr_test-v5:\n");
  if (build_instr_v5())
    return 1;

  return 0;
}

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  if (argc > 1) {

    if (strcmp(argv[1], "test-all") == 0) {
      if (build_all_blargg())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "nestest") == 0) {
      printf("\nnestest CPU Tests:\n");
      if (build_nestest())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-cartridge") == 0) {
      printf("Cartridge Loader Tests:\n");
      if (build_cartridge_test())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-branch-timing") == 0) {
      if (build_branch_timing())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-cpu-timing") == 0) {
      if (build_cpu_timing())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-cpu-interrupts") == 0) {
      if (build_cpu_interrupts())
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-instr-v5") == 0) {
      if (build_instr_v5())
        return 1;
      return 0;
    }
  }

  // Default: build the emulator
  return build_emulator();
}
