#ifndef BLARGG_RUNNER_H
#define BLARGG_RUNNER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// Configuration for running a test ROM
typedef struct {
  const char* rom_path; // Path to the .nes file
  size_t max_cycles;    // Max CPU cycles before timeout (0 = default 20M)
} sb_blargg_config_t;

// Result from running a test ROM
typedef struct {
  bool passed;
  int error_code; // Value read from $6000 (0 = pass)
  size_t cycles_run;
} sb_blargg_result_t;

// Run a test ROM and return the result.
// The ROM must be mapper 0 (NROM) and write results to $6000.
sb_blargg_result_t sb_blargg_run(const sb_blargg_config_t* config);

#endif
