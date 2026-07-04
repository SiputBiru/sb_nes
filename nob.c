#define NOB_IMPLEMENTATION
#include "nob.h"

#define BUILD_FOLDER "build/"
#define SRC_FOLDER "src/"
#define TEST_FOLDER "test/"

int main(int argc, char** argv) {
  NOB_GO_REBUILD_URSELF(argc, argv);

  if (!nob_mkdir_if_not_exists(BUILD_FOLDER))
    return 1;

  Nob_Cmd cmd = { 0 };

  nob_cc(&cmd);
  nob_cc_flags(&cmd);
  nob_cmd_append(
    &cmd,
    "-std=c99",
    "-Wall",
    "-Wextra",
    "-Wpedantic",
    "-Wshadow",
    "-Wconversion",
    "-fsanitize=address",
    "-fsanitize=undefined",
    "-g",
    "-O0" // for now just dont do any optimization
  );
  nob_cc_output(&cmd, BUILD_FOLDER "test_nestest");
  nob_cc_inputs(
    &cmd,
    SRC_FOLDER "sb_6502/sb_6502.c",
    SRC_FOLDER "sb_6502/sb_6502_addrmodes.c",
    SRC_FOLDER "sb_bus/sb_bus.c",
    TEST_FOLDER "nestest/test_nestest.c"
  );
  if (!nob_cmd_run(&cmd))
    return 1;

  return 0;
}
