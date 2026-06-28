#include "sb_6502.h"

#include "../sb_bus/sb_bus.h"
#include <stdint.h>

// LDA #$42 (no addr fetch)
sb_6502_result_t addr_immediate(sb_6502_t* cpu, sb_bus_t* bus) {
  // because the operand byte is right after the opcode in memory. just return its address (poiting
  // into ROM) and advance PC past it. The calle need to do bus_read(bus, r.addr) to get the actual
  // value 0x42.
  sb_6502_result_t r = { .addr = cpu->pc++, .page_crossed = false };
  return r;
}

// LDA $00 (single-byte addr)
sb_6502_result_t addr_zero_page(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  r.addr = sb_bus_read(bus, cpu->pc++); // read 1 byte, advance PC
  r.page_crossed = false;

  // r.addr will be 0x00-0xFF after masking by the bust read (through it natrually returns around
  // 0-255).
  return r;
}

// LDA $00,X ( (zp + X) & 0xFF) )
sb_6502_result_t addr_zero_page_x(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  uint8_t base = sb_bus_read(bus, cpu->pc++);
  // if base = 0xFF and X-0x01 the address is 0x00 not 0x0100
  r.addr = (base + cpu->x) & 0x00FF;
  r.page_crossed = false;

  return r;
}

// LDA $8000 (Two byte addr)
sb_6502_result_t addr_absolute(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  uint8_t lo = sb_bus_read(bus, cpu->pc++);
  uint8_t hi = sb_bus_read(bus, cpu->pc++);
  r.addr = ((uint16_t)hi << 8) | lo;
  r.page_crossed = false;
  return r;
}

// LDA $8000,X ( (abs + X), check page )
sb_6502_result_t addr_absolute_x(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  uint8_t lo = sb_bus_read(bus, cpu->pc++);
  uint8_t hi = sb_bus_read(bus, cpu->pc++);
  uint16_t base = ((uint16_t)hi << 8) | lo;
  r.addr = base + cpu->x;
  r.page_crossed =
    (base & 0xFF00) != (r.addr & 0xFF00); // page crossed if the high byte changed when adding x
  return r;
}

// LDA $8000,Y ( (abs + y), check page )
sb_6502_result_t addr_absolute_y(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  uint8_t lo = sb_bus_read(bus, cpu->pc++);
  uint8_t hi = sb_bus_read(bus, cpu->pc++);
  uint16_t base = ((uint16_t)hi << 8) | lo;
  r.addr = base + cpu->y;
  r.page_crossed =
    (base & 0xFF00) != (r.addr & 0xFF00); // page crossed if the high byte changed when adding x
  return r;
}

// JMP ($1234) only (([addr]))
sb_6502_result_t addr_indirect(sb_6502_t* cpu, sb_bus_t* bus) {

  sb_6502_result_t r = { 0 };

  uint8_t lo = sb_bus_read(bus, cpu->pc++);
  uint8_t hi = sb_bus_read(bus, cpu->pc++);
  uint16_t pointer = ((uint16_t)hi << 8) | lo;

  // 6502 BUG: when pointer low byte is 0xFF, it wraps within the page
  // e.g., pointer = $12FF -> reads $12FF (lo), then $1200 (hi), NOT $1300
  uint16_t addr_lo = pointer;
  uint16_t addr_hi = (pointer & 0xFF00) | ((pointer + 1) & 0x00FF);
  uint8_t indirect_lo = sb_bus_read(bus, addr_lo);
  uint8_t indirect_hi = sb_bus_read(bus, addr_hi);
  r.addr = ((uint16_t)indirect_hi << 8) | indirect_lo;
  r.page_crossed = false;
  return r;
}

// LDA ($00,X): Indirect.X ((zp + X))
sb_6502_result_t addr_indexed_indirect(sb_6502_t* cpu, sb_bus_t* bus) {

  sb_6502_result_t r = { 0 };
  uint8_t zp = sb_bus_read(bus, cpu->pc++);
  uint8_t pointer_addr = zp + cpu->x;

  uint8_t lo = sb_bus_read(bus, pointer_addr);
  uint8_t hi = sb_bus_read(bus, (uint8_t)(pointer_addr + 1));
  r.addr = ((uint16_t)hi << 8) | lo;
  r.page_crossed = false;

  return r;
}

// LDA ($00),Y: Indirect.Y ((zp)) + Y, check page
sb_6502_result_t addr_indirect_indexed(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  uint8_t zp = sb_bus_read(bus, cpu->pc++);
  uint8_t lo = sb_bus_read(bus, zp);
  uint8_t hi = sb_bus_read(bus, (uint8_t)(zp + 1));
  uint16_t base = ((uint16_t)hi << 8) | lo;

  r.addr = base + cpu->y;
  r.page_crossed = (base & 0xFF00) != (r.addr & 0xFF00);

  return r;
}

// bEQ, BNE, etc: branch instruction
sb_6502_result_t addr_relative(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { 0 };

  int8_t offset = (int8_t)sb_bus_read(bus, cpu->pc);
  cpu->pc++; // no ++ in the read to keep it clean
             // the target address = PC + offset, but the cycle penalty depends on whetere the
             // branch crosses a page. will be handle the PC update in the branch logic.

  r.addr = cpu->pc + offset;
  r.page_crossed = (cpu->pc & 0xFF00) != (r.addr & 0xFF00);

  return r;
}

// CLC, INX, TAX, etc
sb_6502_result_t addr_implied(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { .addr = 0, .page_crossed = false };
  return r;
}

// ASL A, ROL A, stc. (opeate on A register directly)
sb_6502_result_t addr_accumulator(sb_6502_t* cpu, sb_bus_t* bus) {
  sb_6502_result_t r = { .addr = 0, .page_crossed = false };
  return r;
}
