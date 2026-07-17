#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define TEST_FOLDER "test/"

// Common flags (no -Wconversion — too noisy for C99 integer promotion)
#define CFLAGS \
  "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wno-unused-parameter", "-g", "-O0"

// Flags for test builds (include sanitizers — no SDL linked here)
#define CFLAGS_TEST \
  "-std=c99", "-Wall", "-Wextra", "-Wpedantic", "-Wshadow", "-Wno-unused-parameter", \
    "-fsanitize=address", "-fsanitize=undefined", "-g", "-O0"

// All core source files needed to link a test binary
#define CORE_SOURCES \
  SRC_FOLDER "sb_6502/sb_6502.c", SRC_FOLDER "sb_bus/sb_bus.c", \
    SRC_FOLDER "sb_cartridge/sb_cartridge.c", SRC_FOLDER "sb_cartridge/sb_mapper_nrom.c", \
    SRC_FOLDER "sb_nes.c", SRC_FOLDER "sb_ppu/sb_ppu.c", SRC_FOLDER "sb_ppu/sb_ppu_render.c"

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

// Like build_and_run but captures the last line of test output for summaries.
// Pass last_line=NULL to run normally (equivalent to build_and_run).
static int build_and_run_capture(
  Nob_Cmd* cmd,
  const char* output,
  const char* extra_flags,
  char* last_line,
  int last_line_size
) {
  // Build (same as build_and_run)
  Nob_Cmd front = { 0 };
  nob_cc(&front);
  nob_cc_flags(&front);
  nob_cmd_append(&front, CFLAGS_TEST);
  if (extra_flags)
    nob_cmd_append(&front, extra_flags);
  nob_cc_output(&front, output);
  for (int i = 0; i < cmd->count; i++)
    nob_cmd_append(&front, cmd->items[i]);
  *cmd = front;

  if (!nob_cmd_run(cmd))
    return 1;

  if (last_line == NULL) {
    // No capture — run normally (used by single-test targets)
    Nob_Cmd run = { 0 };
    nob_cmd_append(&run, output);
    return nob_cmd_run(&run) ? 0 : 1;
  }

  // Capture mode — redirect output to a temp file
  char tmp_path[512];
  snprintf(tmp_path, sizeof(tmp_path), "%s.capture", output);

  Nob_Cmd run = { 0 };
  nob_cmd_append(&run, output);
  bool ok = nob_cmd_run(&run, .stdout_path = tmp_path, .stderr_path = tmp_path);

  // Read back the temp file: print it and capture last non-empty line
  FILE* f = fopen(tmp_path, "r");
  if (f) {
    char buf[1024];
    last_line[0] = '\0';
    while (fgets(buf, sizeof(buf), f)) {
      printf("%s", buf);
      size_t len = strlen(buf);
      while (len > 0 && (buf[len - 1] == '\n' || buf[len - 1] == '\r'))
        buf[--len] = '\0';
      if (len > 0) {
        strncpy(last_line, buf, last_line_size - 1);
        last_line[last_line_size - 1] = '\0';
      }
    }
    fclose(f);
  }
  remove(tmp_path);

  return ok ? 0 : 1;
}

static int build_nestest(char* last_line, int last_line_size) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, TEST_FOLDER "nestest/test_nestest.c");
  return build_and_run_capture(&cmd, BUILD_FOLDER "test_nestest", NULL, last_line, last_line_size);
}

static int build_cartridge_test(void) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, SRC_FOLDER "sb_cartridge/sb_cartridge.c");
  nob_cmd_append(&cmd, SRC_FOLDER "sb_cartridge/sb_mapper_nrom.c");
  nob_cmd_append(&cmd, TEST_FOLDER "cartridge/test_cartridge.c");
  return build_and_run(&cmd, BUILD_FOLDER "test_cartridge", NULL);
}

static int build_blargg_test(
  const char* test_name,
  const char* test_file,
  char* last_line,
  int last_line_size
) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, TEST_FOLDER "blargg/blargg_runner.c");
  nob_cmd_append(&cmd, test_file);

  char output[256];
  snprintf(output, sizeof(output), BUILD_FOLDER "%s", test_name);
  return build_and_run_capture(&cmd, output, NULL, last_line, last_line_size);
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

static int build_mingw(void) {
  Nob_Cmd cmd = { 0 };
  nob_cmd_append(&cmd, CORE_SOURCES);
  nob_cmd_append(&cmd, SRC_FOLDER "sb_frontend/sb_frontend.c");
  nob_cmd_append(&cmd, "sb_main.c");

  Nob_Cmd front = { 0 };
  nob_cmd_append(&front, "x86_64-w64-mingw32-gcc");
  nob_cmd_append(
    &front,
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wshadow",
    "-Wno-unused-parameter",
    "-O2"
  );
  nob_cmd_append(&front, "-I/usr/x86_64-w64-mingw32/include");
  nob_cc_output(&front, BUILD_FOLDER "sb_nes.exe");
  for (int i = 0; i < cmd.count; i++)
    nob_cmd_append(&front, cmd.items[i]);
  nob_cmd_append(&front, "-L/usr/x86_64-w64-mingw32/lib");
  nob_cmd_append(&front, "-lSDL3", "-mwindows");
  nob_cmd_append(
    &front,
    "-lm",
    "-lkernel32",
    "-luser32",
    "-lgdi32",
    "-lwinmm",
    "-limm32",
    "-lole32",
    "-loleaut32",
    "-lversion",
    "-luuid",
    "-ladvapi32",
    "-lsetupapi",
    "-lshell32",
    "-ldinput8"
  );
  cmd = front;

  if (!nob_cmd_run(&cmd))
    return 1;
  return 0;
}

static int build_branch_timing(char* last_line, int last_line_size) {
  return build_blargg_test(
    "test_branch_timing",
    TEST_FOLDER "blargg/test_branch_timing.c",
    last_line,
    last_line_size
  );
}

static int build_cpu_timing(char* last_line, int last_line_size) {
  return build_blargg_test(
    "test_cpu_timing",
    TEST_FOLDER "blargg/test_cpu_timing.c",
    last_line,
    last_line_size
  );
}

static int build_cpu_interrupts(char* last_line, int last_line_size) {
  return build_blargg_test(
    "test_cpu_interrupts",
    TEST_FOLDER "blargg/test_cpu_interrupts.c",
    last_line,
    last_line_size
  );
}

static int build_instr_v5(char* last_line, int last_line_size) {
  return build_blargg_test(
    "test_instr_v5",
    TEST_FOLDER "blargg/test_instr_v5.c",
    last_line,
    last_line_size
  );
}

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  if (argc > 1) {

    if (strcmp(argv[1], "test-all") == 0) {
      struct {
        const char* name;
        int result;
        char last_line[256];
      } tests[] = {
        { "branch_timing", 0, "" }, { "cpu_timing", 0, "" }, { "cpu_interrupts", 0, "" },
        { "instr_v5", 0, "" },      { "nestest", 0, "" },
      };
      int total = sizeof(tests) / sizeof(tests[0]);

      printf("Branch Timing Tests:\n");
      tests[0].result = build_branch_timing(tests[0].last_line, sizeof(tests[0].last_line));

      printf("\nCPU Timing Tests:\n");
      tests[1].result = build_cpu_timing(tests[1].last_line, sizeof(tests[1].last_line));

      printf("\nCPU Interrupt Tests:\n");
      tests[2].result = build_cpu_interrupts(tests[2].last_line, sizeof(tests[2].last_line));

      printf("\ninstr_test-v5:\n");
      tests[3].result = build_instr_v5(tests[3].last_line, sizeof(tests[3].last_line));

      printf("\nnestest CPU Tests:\n");
      tests[4].result = build_nestest(tests[4].last_line, sizeof(tests[4].last_line));

      int passed = 0, failed = 0;
      for (int i = 0; i < total; i++) {
        if (tests[i].result == 0)
          passed++;
        else
          failed++;
      }

      printf("\n===== Test Summary =====\n");
      for (int i = 0; i < total; i++) {
        printf(
          "  %-20s  %s  %s\n",
          tests[i].name,
          tests[i].result == 0 ? "PASS" : "FAIL",
          tests[i].last_line
        );
      }
      printf("========================\n");
      printf("  %d tests, %d passed, %d failed\n", total, passed, failed);

      return failed > 0 ? 1 : 0;
    }

    if (strcmp(argv[1], "nestest") == 0) {
      printf("\nnestest CPU Tests:\n");
      if (build_nestest(NULL, 0))
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
      if (build_branch_timing(NULL, 0))
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-cpu-timing") == 0) {
      if (build_cpu_timing(NULL, 0))
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-cpu-interrupts") == 0) {
      if (build_cpu_interrupts(NULL, 0))
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "test-instr-v5") == 0) {
      if (build_instr_v5(NULL, 0))
        return 1;
      return 0;
    }

    if (strcmp(argv[1], "mingw") == 0) {
      return build_mingw();
    }
  }

  // Default: build the emulator
  return build_emulator();
}
