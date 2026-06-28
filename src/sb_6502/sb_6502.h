#ifndef SB_6502_H

#define SB_6502_H

#include <stdbool.h>
#include <stdint.h>

#include "../sb_bus/sb_bus.h"

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
} sb_6502_result_t;

typedef struct {

  uint16_t pc;     // program counter
  uint8_t a, x, y; // accumulator, index registers
  uint8_t s;       // stack pointer
  uint8_t p;       // status register (bit flags above)
  uint64_t cycles; // total cycles

  // interrupt state
  bool nmi_pending;
  bool irq_pending;

  sb_6502_result_t result;

} sb_6502_t;

typedef void (*sb_6502_op_fn)(sb_6502_t* cpu, sb_bus_t* bus, uint8_t value);

sb_6502_result_t addr_immediate(sb_6502_t* cpu, sb_bus_t* bus);        // Implicit (no addr fetch)
sb_6502_result_t addr_zero_page(sb_6502_t* cpu, sb_bus_t* bus);        // Single byte addr
sb_6502_result_t addr_zero_page_x(sb_6502_t* cpu, sb_bus_t* bus);      // (zp + X) & 0xFF
sb_6502_result_t addr_absolute(sb_6502_t* cpu, sb_bus_t* bus);         // Two byte addr
sb_6502_result_t addr_absolute_x(sb_6502_t* cpu, sb_bus_t* bus);       // (abs + X), check page
sb_6502_result_t addr_absolute_y(sb_6502_t* cpu, sb_bus_t* bus);       // (abs + Y), check page
sb_6502_result_t addr_indirect(sb_6502_t* cpu, sb_bus_t* bus);         // JMP only (([addr]))
sb_6502_result_t addr_indexed_indirect(sb_6502_t* cpu, sb_bus_t* bus); // ((zp + X))
sb_6502_result_t addr_indirect_indexed(sb_6502_t* cpu, sb_bus_t* bus); // ((zp)) + Y, check page
sb_6502_result_t
addr_relative(sb_6502_t* cpu, sb_bus_t* bus);                 // branch instruction (bEQ, BNE, etc.)
sb_6502_result_t addr_implied(sb_6502_t* cpu, sb_bus_t* bus); // CLC, INX, TAX

sb_6502_result_t addr_accumulator(sb_6502_t* cpu, sb_bus_t* bus); // ASL A, ROL A, etc.

#endif
