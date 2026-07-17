#ifndef SB_6502_H

#define SB_6502_H

#include <stdbool.h>
#include <stdint.h>

#include "../sb_bus/sb_bus.h"

// Error codes (returned by sb_6502_cycle and cycle handlers)
#define SB_OK 0          // phase sequence complete (instruction, interrupt, or fetch done)
#define SB_IN_PROGRESS 1 // more phases remain, call again
#define SB_ERR_CPU (-1)
#define SB_ERR_BUS (-2)
#define SB_ERR_DMA (-3)
#define SB_ERR_INVAL (-4)

// BKPT: line-numbered error site. In debug builds, captures the
// failure line. In release, returns a distinguishable negative code
// so callers can still propagate errors.
#ifdef NDEBUG
#define SB_FAIL_LINE(line) ((int)(-(line)))
#else
#define SB_FAIL_LINE(line) ((int)(-(line)))
#endif

// Status register flags (P)
#define SB_6502_CARRY (1 << 0)
#define SB_6502_ZERO (1 << 1)
#define SB_6502_INTERRUPT (1 << 2)
#define SB_6502_DECIMAL (1 << 3)
#define SB_6502_BRK (1 << 4)
#define SB_6502_UNUSED (1 << 5)
#define SB_6502_OVERFLOW (1 << 6)
#define SB_6502_NEGATIVE (1 << 7)

// Cycle-handler function type. Each phase of execution (instruction or interrupt)
// is one call to a cycle handler. Returns SB_OK when the phase sequence completes,
// SB_IN_PROGRESS for more phases, negative on error.
struct sb_6502_t;
typedef int (*sb_6502_cycle_fn)(struct sb_6502_t* cpu, sb_bus_t* bus);

typedef struct sb_6502_t {

  uint16_t pc;     // program counter
  uint8_t a, x, y; // accumulator, index registers
  uint8_t s;       // stack pointer
  uint8_t p;       // status register (bit flags above)
  uint64_t cycles; // total cycles

  // 6502 one-instruction delay for I flag changes.
  // After CLI/SEI/PLP/RTI, the I flag change takes effect one instruction
  // later. irq_delay_next = true means the next fetch uses the SAVED I flag
  // (the value before the change). After the fetch, the new I flag applies.
  bool irq_delay_next;
  bool i_flag_saved; // saved I flag value (before the change)

  // cycle-interleave execution state
  uint16_t opcode;   // opcode of the currently executing instruction
                     // (0x100 = NMI, 0x101 = IRQ sentinel)
  uint8_t phase;     // current micro-operation phase (0, 1, 2, ...)
  uint16_t addr_abs; // absolute address for read/write
  int8_t addr_rel;   // relative offset for branches
  uint8_t fetched;   // value read for ALU operation

} sb_6502_t;

// opcode
typedef struct {
  const char* mnemonic;
  uint8_t bytes;
  uint8_t cycles;
  void (*func)(sb_6502_t*, sb_bus_t*, uint8_t);
  sb_6502_cycle_fn cycle;
} sb_6502_opcode_t;

typedef void (*sb_6502_op_fn)(sb_6502_t* cpu, sb_bus_t* bus, uint8_t value);

void sb_6502_init_opcodes(void);
int sb_6502_cycle(sb_6502_t* cpu, sb_bus_t* bus);
void sb_6502_reset(sb_6502_t* cpu, sb_bus_t* bus);

#endif
