#ifndef SB_6502_H

#define SB_6502_H

#include <stdbool.h>
#include <stdint.h>

// Status register flags (P)
#define SB_6502_CARRY (1 << 0)
#define SB_6502_ZERO (1 << 1)
#define SB_6502_INTERRUPT (1 << 2)
#define SB_6502_DECIMAL (1 << 3)
#define SB_6502_BRK (1 << 4)
#define SB_6502_UNUSED (1 << 5)
#define SB_6502_OVERFLOW (1 << 6)
#define SB_6502_NEGATIVE (1 << 7)

// addressing mode result
typedef struct {
  uint16_t addr;
  bool page_crossed;
} sb_6502_result;

typedef struct {

  uint16_t pc;     // program counter
  uint8_t a, x, y; // accumulator, index registers
  uint8_t s;       // stack pointer
  uint8_t p;       // status register (bit flags above)
  uint64_t cycles; // total cycles

  // interrupt state
  bool nmi_pending;
  bool irq_pending;

  sb_6502_result result;

} sb_6502_t;

typedef void (*sb_6502_op_fn)(sb_6502_t* cpu, struct sb_bus_t* bus, uint8_t value);

#endif
