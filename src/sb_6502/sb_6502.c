#include "sb_6502.h"
#include "../sb_ppu/sb_ppu.h"
#include <assert.h>

static sb_6502_opcode_t opcodes[256];

static int cycle_illegal(struct sb_6502_t* cpu, sb_bus_t* bus) {
  (void)cpu;
  (void)bus;
  // Illegal opcode fallback, no valid cycle handler assigned.
  return SB_ERR_CPU;
}

// helper macro
#define SET_Z(cpu, val) \
  do { \
    if ((val) == 0) \
      (cpu)->p |= SB_6502_ZERO; \
    else \
      (cpu)->p &= ~SB_6502_ZERO; \
  } while (0)

#define SET_N(cpu, val) \
  do { \
    if ((val) & 0x80) \
      (cpu)->p |= SB_6502_NEGATIVE; \
    else \
      (cpu)->p &= ~SB_6502_NEGATIVE; \
  } while (0)

#define SET_ZN(cpu, val) \
  do { \
    SET_Z(cpu, val); \
    SET_N(cpu, val); \
  } while (0)

// forward declaration of all instruction functions
static void op_LDA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_LDX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_LDY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_STA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_STX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_STY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ADC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SBC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_AND(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ORA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_EOR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CMP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CPX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CPY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BIT(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ASL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_LSR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ROL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ROR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_INC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_DEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_INX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_INY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_DEX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_DEY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TAY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TXA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TYA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TSX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_TXS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_PHA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_PLA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_PHP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_PLP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_JMP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_JSR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_RTS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_RTI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BRK(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BCC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BCS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BEQ(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BNE(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BMI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BPL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BVC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_BVS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CLC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CLI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SEI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CLV(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_CLD(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SED(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_NOP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
// Illegal opcodes
static void op_LAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_DCP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ISB(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SLO(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_RLA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SRE(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_RRA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ANC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ALR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_ARR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_LXA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SBX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SHY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);
static void op_SHX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode);

// status flag instruction
static void op_CLC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p &= ~SB_6502_CARRY; }
static void op_SEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p |= SB_6502_CARRY; }
static void op_CLI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
  cpu->p &= ~SB_6502_INTERRUPT;
  cpu->irq_delay_next = true; // one-instruction delay
}
static void op_SEI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
  cpu->p |= SB_6502_INTERRUPT;
  cpu->irq_delay_next = true; // one-instruction delay
}
static void op_CLV(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p &= ~SB_6502_OVERFLOW; }
static void op_CLD(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p &= ~SB_6502_DECIMAL; }
static void op_SED(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p |= SB_6502_DECIMAL; }
static void op_NOP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { /* do nothing */ }

// register transfer
static void op_TAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x = cpu->a;
  SET_ZN(cpu, cpu->x);
}
static void op_TAY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->y = cpu->a;
  SET_ZN(cpu, cpu->y);
}
static void op_TXA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = cpu->x;
  SET_ZN(cpu, cpu->a);
}
static void op_TYA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = cpu->y;
  SET_ZN(cpu, cpu->a);
}
static void op_TSX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x = cpu->s;
  SET_ZN(cpu, cpu->x);
}
static void op_TXS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s = cpu->x; // TXS does NOT set flags!
}

// increment decrement
static void op_INX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x++;
  SET_ZN(cpu, cpu->x);
}
static void op_INY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->y++;
  SET_ZN(cpu, cpu->y);
}
static void op_DEX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x--;
  SET_ZN(cpu, cpu->x);
}
static void op_DEY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->y--;
  SET_ZN(cpu, cpu->y);
}
static void op_INC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->fetched++;
  SET_ZN(cpu, cpu->fetched);
}
static void op_DEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->fetched--;
  SET_ZN(cpu, cpu->fetched);
}

// load store
static void op_LDA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_LDX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x = cpu->fetched;
  SET_ZN(cpu, cpu->x);
}
static void op_LDY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->y = cpu->fetched;
  SET_ZN(cpu, cpu->y);
}
static void op_STA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->fetched = cpu->a; }
static void op_STX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->fetched = cpu->x; }
static void op_STY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->fetched = cpu->y; }

// Stack things
static void op_PHA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, cpu->a);
  cpu->s--;
}
static void op_PLA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s++;
  cpu->a = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  SET_ZN(cpu, cpu->a);
}
static void op_PHP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, cpu->p | SB_6502_BRK | SB_6502_UNUSED);
  cpu->s--;
}
static void op_PLP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s++;
  cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
  cpu->p = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->p &= ~SB_6502_BRK;
  cpu->p |= SB_6502_UNUSED;
  cpu->irq_delay_next = true; // I flag might have changed
}

// branches
static void op_BCC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (!(cpu->p & SB_6502_CARRY))
    cpu->pc = cpu->result.addr;
}
static void op_BCS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->p & SB_6502_CARRY)
    cpu->pc = cpu->result.addr;
}
static void op_BEQ(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->p & SB_6502_ZERO)
    cpu->pc = cpu->result.addr;
}
static void op_BNE(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (!(cpu->p & SB_6502_ZERO))
    cpu->pc = cpu->result.addr;
}
static void op_BMI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->p & SB_6502_NEGATIVE)
    cpu->pc = cpu->result.addr;
}
static void op_BPL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (!(cpu->p & SB_6502_NEGATIVE))
    cpu->pc = cpu->result.addr;
}
static void op_BVC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (!(cpu->p & SB_6502_OVERFLOW))
    cpu->pc = cpu->result.addr;
}
static void op_BVS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->p & SB_6502_OVERFLOW)
    cpu->pc = cpu->result.addr;
}

// jump & subroutine
static void op_JMP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->pc = cpu->result.addr; }
static void op_JSR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint16_t return_addr = cpu->pc - 1;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)(return_addr >> 8));
  cpu->s--;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)return_addr);
  cpu->s--;
  cpu->pc = cpu->result.addr;
}
static void op_RTS(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s++;
  uint8_t lo = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->s++;
  uint8_t hi = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->pc = ((uint16_t)hi << 8) | lo;
  cpu->pc++;
}
static void op_RTI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s++;
  cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
  cpu->p = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  // cpu->p &= ~(SB_6502_BRK | SB_6502_UNUSED);
  cpu->p &= ~(SB_6502_BRK);
  cpu->p |= SB_6502_UNUSED;
  cpu->irq_delay_next = true; // I flag might have changed
  cpu->s++;
  uint8_t lo = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->s++;
  uint8_t hi = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->pc = ((uint16_t)hi << 8) | lo;
}

// ALU operations
static void op_AND(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a &= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_ORA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a |= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_EOR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a ^= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_CMP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  uint16_t diff = (uint16_t)cpu->a - val;
  if (cpu->a >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_CPX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  uint16_t diff = (uint16_t)cpu->x - val;
  if (cpu->x >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_CPY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  uint16_t diff = (uint16_t)cpu->y - val;
  if (cpu->y >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_BIT(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  if (cpu->a & val)
    cpu->p &= ~SB_6502_ZERO;
  else
    cpu->p |= SB_6502_ZERO;
  if (val & 0x80)
    cpu->p |= SB_6502_NEGATIVE;
  else
    cpu->p &= ~SB_6502_NEGATIVE;
  if (val & 0x40)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;
}

// ADC and SBC
static void op_ADC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  uint16_t sum = (uint16_t)cpu->a + val + (cpu->p & SB_6502_CARRY);

  if (sum & 0xFF00)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;

  // Overflow: sign of both operands != sign of result
  if (((cpu->a ^ (uint8_t)sum) & (val ^ (uint8_t)sum) & 0x80) != 0)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;

  cpu->a = (uint8_t)sum;
  SET_ZN(cpu, cpu->a);
}

static void op_SBC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = cpu->fetched;
  // A - val - (1 - C) = A + (~val) + C
  uint16_t diff = (uint16_t)cpu->a + (val ^ 0xFF) + (cpu->p & SB_6502_CARRY);

  if (diff & 0xFF00)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;

  if (((cpu->a ^ (uint8_t)diff) & ((val ^ 0xFF) ^ (uint8_t)diff) & 0x80) != 0)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;

  cpu->a = (uint8_t)diff;
  SET_ZN(cpu, cpu->a);
}

// Shift / rotate
static void op_ASL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (opcode == 0x0A) {
    if (cpu->a & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a <<= 1;
    SET_ZN(cpu, cpu->a);
  } else {
    if (cpu->fetched & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->fetched <<= 1;
    SET_ZN(cpu, cpu->fetched);
  }
}
static void op_LSR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (opcode == 0x4A) {
    if (cpu->a & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a >>= 1;
    SET_ZN(cpu, cpu->a);
  } else {
    if (cpu->fetched & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->fetched >>= 1;
    SET_ZN(cpu, cpu->fetched);
  }
}
static void op_ROL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (opcode == 0x2A) {
    if (cpu->a & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a = (cpu->a << 1) | old_c;
    SET_ZN(cpu, cpu->a);
  } else {
    if (cpu->fetched & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->fetched = (cpu->fetched << 1) | old_c;
    SET_ZN(cpu, cpu->fetched);
  }
}
static void op_ROR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (opcode == 0x6A) {
    if (cpu->a & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a = (cpu->a >> 1) | (old_c << 7);
    SET_ZN(cpu, cpu->a);
  } else {
    if (cpu->fetched & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->fetched = (cpu->fetched >> 1) | (old_c << 7);
    SET_ZN(cpu, cpu->fetched);
  }
}

// BRK (software interrupt)
static void op_BRK(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  // BRK is 2 bytes; the second byte is ignored (padding)
  // Push PC+1 (after the padding byte) and status
  uint16_t addr = cpu->pc + 1; // skip padding byte
  cpu->pc = addr;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)(addr >> 8));
  cpu->s--;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)addr);
  cpu->s--;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, cpu->p | SB_6502_BRK | SB_6502_UNUSED);
  cpu->s--;
  cpu->p |= SB_6502_INTERRUPT;
  uint8_t lo = sb_bus_read(bus, 0xFFFE);
  uint8_t hi = sb_bus_read(bus, 0xFFFF);
  cpu->pc = ((uint16_t)hi << 8) | lo;
}

// illegal / undocumented opcodes
static void op_LAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = cpu->x = cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_SAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->fetched = cpu->a & cpu->x;
}
static void op_DCP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->fetched--;
  if (cpu->a >= cpu->fetched)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)((uint16_t)cpu->a - cpu->fetched));
}
static void op_ISB(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->fetched++;
  uint16_t diff = (uint16_t)cpu->a + (cpu->fetched ^ 0xFF) + (cpu->p & SB_6502_CARRY);
  if (diff & 0xFF00)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  if (((cpu->a ^ (uint8_t)diff) & ((cpu->fetched ^ 0xFF) ^ (uint8_t)diff) & 0x80) != 0)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;
  cpu->a = (uint8_t)diff;
  SET_ZN(cpu, cpu->a);
}
static void op_SLO(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->fetched & 0x80)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  cpu->fetched <<= 1;
  cpu->a |= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_RLA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (cpu->fetched & 0x80)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  cpu->fetched = (cpu->fetched << 1) | old_c;
  cpu->a &= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_SRE(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->fetched & 1)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  cpu->fetched >>= 1;
  cpu->a ^= cpu->fetched;
  SET_ZN(cpu, cpu->a);
}
static void op_RRA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (cpu->fetched & 1)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  cpu->fetched = (cpu->fetched >> 1) | (old_c << 7);
  uint16_t sum = (uint16_t)cpu->a + cpu->fetched + (cpu->p & SB_6502_CARRY);
  if (sum & 0xFF00)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  if (((cpu->a ^ (uint8_t)sum) & (cpu->fetched ^ (uint8_t)sum) & 0x80) != 0)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;
  cpu->a = (uint8_t)sum;
  SET_ZN(cpu, cpu->a);
}

static void op_ANC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a &= cpu->fetched;
  SET_ZN(cpu, cpu->a);
  if (cpu->a & 0x80)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
}

static void op_ALR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a &= cpu->fetched;
  if (cpu->a & 1)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  cpu->a >>= 1;
  SET_ZN(cpu, cpu->a);
}

static void op_ARR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a &= cpu->fetched;
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 0x80 : 0;
  cpu->a = (cpu->a >> 1) | old_c;
  SET_ZN(cpu, cpu->a);

  if (cpu->a & 0x40)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;

  if (((cpu->a >> 6) ^ (cpu->a >> 5)) & 1)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;
}

static void op_LXA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = cpu->x = cpu->fetched;
  SET_ZN(cpu, cpu->a);
}

static void op_SBX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t temp = cpu->a & cpu->x;
  uint16_t diff = (uint16_t)temp - cpu->fetched;
  cpu->x = (uint8_t)diff;
  SET_ZN(cpu, cpu->x);
  if (temp >= cpu->fetched)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
}

// SHY ($9C): Store Y & (base_hi + 1) at (base_hi << 8) | ((base_lo + X) & 0xFF)
// Low byte of address wraps, no carry to high byte. Always uses H+1.
static void op_SHY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint16_t base = cpu->addr_abs - cpu->x;
  uint8_t base_hi = (uint8_t)(base >> 8);
  uint8_t lo_sum = (uint8_t)((uint8_t)base + cpu->x);
  cpu->fetched = cpu->y & (uint8_t)(base_hi + 1);
  cpu->addr_abs = ((uint16_t)base_hi << 8) | lo_sum;
}

// SHX ($9E): Store X & (base_hi + 1) at (base_hi << 8) | ((base_lo + Y) & 0xFF)
static void op_SHX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint16_t base = cpu->addr_abs - cpu->y;
  uint8_t base_hi = (uint8_t)(base >> 8);
  uint8_t lo_sum = (uint8_t)((uint8_t)base + cpu->y);
  cpu->fetched = cpu->x & (uint8_t)(base_hi + 1);
  cpu->addr_abs = ((uint16_t)base_hi << 8) | lo_sum;
}

// Helper type for category-based opcode table definitions.
// Each entry bundles the opcode byte with its full instruction description,
// including the cycle handler — so every definition is self-contained.
typedef struct {
  uint8_t opcode;
  const char* mnemonic;
  uint8_t bytes;
  uint8_t cycles;
  bool page_penalty;
  sb_6502_result_t (*addr_mode)(sb_6502_t*, sb_bus_t*);
  void (*func)(sb_6502_t*, sb_bus_t*, uint8_t);
  sb_6502_cycle_fn cycle;
} opcode_def_t;

static void assign_defs(const opcode_def_t* defs, int count) {
  for (int i = 0; i < count; i++)
    opcodes[defs[i].opcode] = (sb_6502_opcode_t){
      .mnemonic = defs[i].mnemonic,
      .bytes = defs[i].bytes,
      .cycles = defs[i].cycles,
      .page_penalty = defs[i].page_penalty,
      .addr_mode = defs[i].addr_mode,
      .func = defs[i].func,
      .cycle = defs[i].cycle,
    };
}

// Forward declarations for cycle handlers (defined later in this file).
static int cycle_implied(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_immediate(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_zp(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_absolute(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_zpx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_absx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_absy(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_idrx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_read_idry(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_zp(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_absolute(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_zpx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_absx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_absy(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_idrx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_write_idry(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_branch(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_jmp_abs(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_jmp_ind(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_jsr(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rts(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rti(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_brk(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_push(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_pull(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_zp(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_abs(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_zpx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_absx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_absy(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_idrx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_idry(sb_6502_t* cpu, sb_bus_t* bus);

void sb_6502_init_opcodes(void) {
  // Fill all 256 slots with illegal fallback first.
  for (int i = 0; i < 256; i++)
    opcodes[i] = (sb_6502_opcode_t){
      .mnemonic = "XX",
      .addr_mode = addr_implied,
      .func = op_NOP,
      .cycle = cycle_illegal,
    };

  // Each category table is a complete, self-documenting list.
  // Every entry includes its cycle handler — no second pass needed.

  static const opcode_def_t lda[] = {
    { 0xA9, "LDA", 2, 2, false, addr_immediate, op_LDA, cycle_read_immediate },
    { 0xA5, "LDA", 2, 3, false, addr_zero_page, op_LDA, cycle_read_zp },
    { 0xB5, "LDA", 2, 4, false, addr_zero_page_x, op_LDA, cycle_read_zpx },
    { 0xAD, "LDA", 3, 4, false, addr_absolute, op_LDA, cycle_read_absolute },
    { 0xBD, "LDA", 3, 4, true, addr_absolute_x, op_LDA, cycle_read_absx },
    { 0xB9, "LDA", 3, 4, true, addr_absolute_y, op_LDA, cycle_read_absy },
    { 0xA1, "LDA", 2, 6, false, addr_indexed_indirect, op_LDA, cycle_read_idrx },
    { 0xB1, "LDA", 2, 5, true, addr_indirect_indexed, op_LDA, cycle_read_idry },
  };
  assign_defs(lda, sizeof(lda) / sizeof(lda[0]));

  static const opcode_def_t ldx[] = {
    { 0xA2, "LDX", 2, 2, false, addr_immediate, op_LDX, cycle_read_immediate },
    { 0xA6, "LDX", 2, 3, false, addr_zero_page, op_LDX, cycle_read_zp },
    { 0xB6, "LDX", 2, 4, false, addr_zero_page_y, op_LDX, cycle_read_zpx },
    { 0xAE, "LDX", 3, 4, false, addr_absolute, op_LDX, cycle_read_absolute },
    { 0xBE, "LDX", 3, 4, true, addr_absolute_y, op_LDX, cycle_read_absy },
  };
  assign_defs(ldx, sizeof(ldx) / sizeof(ldx[0]));

  static const opcode_def_t ldy[] = {
    { 0xA0, "LDY", 2, 2, false, addr_immediate, op_LDY, cycle_read_immediate },
    { 0xA4, "LDY", 2, 3, false, addr_zero_page, op_LDY, cycle_read_zp },
    { 0xB4, "LDY", 2, 4, false, addr_zero_page_x, op_LDY, cycle_read_zpx },
    { 0xAC, "LDY", 3, 4, false, addr_absolute, op_LDY, cycle_read_absolute },
    { 0xBC, "LDY", 3, 4, true, addr_absolute_x, op_LDY, cycle_read_absx },
  };
  assign_defs(ldy, sizeof(ldy) / sizeof(ldy[0]));

  static const opcode_def_t sta[] = {
    { 0x85, "STA", 2, 3, false, addr_zero_page, op_STA, cycle_write_zp },
    { 0x95, "STA", 2, 4, false, addr_zero_page_x, op_STA, cycle_write_zpx },
    { 0x8D, "STA", 3, 4, false, addr_absolute, op_STA, cycle_write_absolute },
    { 0x9D, "STA", 3, 5, false, addr_absolute_x, op_STA, cycle_write_absx },
    { 0x99, "STA", 3, 5, false, addr_absolute_y, op_STA, cycle_write_absy },
    { 0x81, "STA", 2, 6, false, addr_indexed_indirect, op_STA, cycle_write_idrx },
    { 0x91, "STA", 2, 6, false, addr_indirect_indexed, op_STA, cycle_write_idry },
  };
  assign_defs(sta, sizeof(sta) / sizeof(sta[0]));

  static const opcode_def_t stx[] = {
    { 0x86, "STX", 2, 3, false, addr_zero_page, op_STX, cycle_write_zp },
    { 0x96, "STX", 2, 4, false, addr_zero_page_y, op_STX, cycle_write_zpx },
    { 0x8E, "STX", 3, 4, false, addr_absolute, op_STX, cycle_write_absolute },
  };
  assign_defs(stx, sizeof(stx) / sizeof(stx[0]));

  static const opcode_def_t sty[] = {
    { 0x84, "STY", 2, 3, false, addr_zero_page, op_STY, cycle_write_zp },
    { 0x94, "STY", 2, 4, false, addr_zero_page_x, op_STY, cycle_write_zpx },
    { 0x8C, "STY", 3, 4, false, addr_absolute, op_STY, cycle_write_absolute },
  };
  assign_defs(sty, sizeof(sty) / sizeof(sty[0]));

  static const opcode_def_t adc[] = {
    { 0x69, "ADC", 2, 2, false, addr_immediate, op_ADC, cycle_read_immediate },
    { 0x65, "ADC", 2, 3, false, addr_zero_page, op_ADC, cycle_read_zp },
    { 0x75, "ADC", 2, 4, false, addr_zero_page_x, op_ADC, cycle_read_zpx },
    { 0x6D, "ADC", 3, 4, false, addr_absolute, op_ADC, cycle_read_absolute },
    { 0x7D, "ADC", 3, 4, true, addr_absolute_x, op_ADC, cycle_read_absx },
    { 0x79, "ADC", 3, 4, true, addr_absolute_y, op_ADC, cycle_read_absy },
    { 0x61, "ADC", 2, 6, false, addr_indexed_indirect, op_ADC, cycle_read_idrx },
    { 0x71, "ADC", 2, 5, true, addr_indirect_indexed, op_ADC, cycle_read_idry },
  };
  assign_defs(adc, sizeof(adc) / sizeof(adc[0]));

  static const opcode_def_t sbc[] = {
    { 0xE9, "SBC", 2, 2, false, addr_immediate, op_SBC, cycle_read_immediate },
    { 0xE5, "SBC", 2, 3, false, addr_zero_page, op_SBC, cycle_read_zp },
    { 0xF5, "SBC", 2, 4, false, addr_zero_page_x, op_SBC, cycle_read_zpx },
    { 0xED, "SBC", 3, 4, false, addr_absolute, op_SBC, cycle_read_absolute },
    { 0xFD, "SBC", 3, 4, true, addr_absolute_x, op_SBC, cycle_read_absx },
    { 0xF9, "SBC", 3, 4, true, addr_absolute_y, op_SBC, cycle_read_absy },
    { 0xE1, "SBC", 2, 6, false, addr_indexed_indirect, op_SBC, cycle_read_idrx },
    { 0xF1, "SBC", 2, 5, true, addr_indirect_indexed, op_SBC, cycle_read_idry },
  };
  assign_defs(sbc, sizeof(sbc) / sizeof(sbc[0]));
  opcodes[0xEB] = opcodes[0xE9]; // undocumented SBC alias

  static const opcode_def_t and_op[] = {
    { 0x29, "AND", 2, 2, false, addr_immediate, op_AND, cycle_read_immediate },
    { 0x25, "AND", 2, 3, false, addr_zero_page, op_AND, cycle_read_zp },
    { 0x35, "AND", 2, 4, false, addr_zero_page_x, op_AND, cycle_read_zpx },
    { 0x2D, "AND", 3, 4, false, addr_absolute, op_AND, cycle_read_absolute },
    { 0x3D, "AND", 3, 4, true, addr_absolute_x, op_AND, cycle_read_absx },
    { 0x39, "AND", 3, 4, true, addr_absolute_y, op_AND, cycle_read_absy },
    { 0x21, "AND", 2, 6, false, addr_indexed_indirect, op_AND, cycle_read_idrx },
    { 0x31, "AND", 2, 5, true, addr_indirect_indexed, op_AND, cycle_read_idry },
  };
  assign_defs(and_op, sizeof(and_op) / sizeof(and_op[0]));

  static const opcode_def_t ora[] = {
    { 0x09, "ORA", 2, 2, false, addr_immediate, op_ORA, cycle_read_immediate },
    { 0x05, "ORA", 2, 3, false, addr_zero_page, op_ORA, cycle_read_zp },
    { 0x15, "ORA", 2, 4, false, addr_zero_page_x, op_ORA, cycle_read_zpx },
    { 0x0D, "ORA", 3, 4, false, addr_absolute, op_ORA, cycle_read_absolute },
    { 0x1D, "ORA", 3, 4, true, addr_absolute_x, op_ORA, cycle_read_absx },
    { 0x19, "ORA", 3, 4, true, addr_absolute_y, op_ORA, cycle_read_absy },
    { 0x01, "ORA", 2, 6, false, addr_indexed_indirect, op_ORA, cycle_read_idrx },
    { 0x11, "ORA", 2, 5, true, addr_indirect_indexed, op_ORA, cycle_read_idry },
  };
  assign_defs(ora, sizeof(ora) / sizeof(ora[0]));

  static const opcode_def_t eor[] = {
    { 0x49, "EOR", 2, 2, false, addr_immediate, op_EOR, cycle_read_immediate },
    { 0x45, "EOR", 2, 3, false, addr_zero_page, op_EOR, cycle_read_zp },
    { 0x55, "EOR", 2, 4, false, addr_zero_page_x, op_EOR, cycle_read_zpx },
    { 0x4D, "EOR", 3, 4, false, addr_absolute, op_EOR, cycle_read_absolute },
    { 0x5D, "EOR", 3, 4, true, addr_absolute_x, op_EOR, cycle_read_absx },
    { 0x59, "EOR", 3, 4, true, addr_absolute_y, op_EOR, cycle_read_absy },
    { 0x41, "EOR", 2, 6, false, addr_indexed_indirect, op_EOR, cycle_read_idrx },
    { 0x51, "EOR", 2, 5, true, addr_indirect_indexed, op_EOR, cycle_read_idry },
  };
  assign_defs(eor, sizeof(eor) / sizeof(eor[0]));

  static const opcode_def_t cmp[] = {
    { 0xC9, "CMP", 2, 2, false, addr_immediate, op_CMP, cycle_read_immediate },
    { 0xC5, "CMP", 2, 3, false, addr_zero_page, op_CMP, cycle_read_zp },
    { 0xD5, "CMP", 2, 4, false, addr_zero_page_x, op_CMP, cycle_read_zpx },
    { 0xCD, "CMP", 3, 4, false, addr_absolute, op_CMP, cycle_read_absolute },
    { 0xDD, "CMP", 3, 4, true, addr_absolute_x, op_CMP, cycle_read_absx },
    { 0xD9, "CMP", 3, 4, true, addr_absolute_y, op_CMP, cycle_read_absy },
    { 0xC1, "CMP", 2, 6, false, addr_indexed_indirect, op_CMP, cycle_read_idrx },
    { 0xD1, "CMP", 2, 5, true, addr_indirect_indexed, op_CMP, cycle_read_idry },
  };
  assign_defs(cmp, sizeof(cmp) / sizeof(cmp[0]));

  static const opcode_def_t cpx[] = {
    { 0xE0, "CPX", 2, 2, false, addr_immediate, op_CPX, cycle_read_immediate },
    { 0xE4, "CPX", 2, 3, false, addr_zero_page, op_CPX, cycle_read_zp },
    { 0xEC, "CPX", 3, 4, false, addr_absolute, op_CPX, cycle_read_absolute },
  };
  assign_defs(cpx, sizeof(cpx) / sizeof(cpx[0]));

  static const opcode_def_t cpy[] = {
    { 0xC0, "CPY", 2, 2, false, addr_immediate, op_CPY, cycle_read_immediate },
    { 0xC4, "CPY", 2, 3, false, addr_zero_page, op_CPY, cycle_read_zp },
    { 0xCC, "CPY", 3, 4, false, addr_absolute, op_CPY, cycle_read_absolute },
  };
  assign_defs(cpy, sizeof(cpy) / sizeof(cpy[0]));

  static const opcode_def_t bit_op[] = {
    { 0x24, "BIT", 2, 3, false, addr_zero_page, op_BIT, cycle_read_zp },
    { 0x2C, "BIT", 3, 4, false, addr_absolute, op_BIT, cycle_read_absolute },
  };
  assign_defs(bit_op, sizeof(bit_op) / sizeof(bit_op[0]));

  // Shift / rotate / increment / decrement — all read-modify-write.
  static const opcode_def_t asl[] = {
    { 0x0A, "ASL", 1, 2, false, addr_accumulator, op_ASL, cycle_implied },
    { 0x06, "ASL", 2, 5, false, addr_zero_page, op_ASL, cycle_rmw_zp },
    { 0x16, "ASL", 2, 6, false, addr_zero_page_x, op_ASL, cycle_rmw_zpx },
    { 0x0E, "ASL", 3, 6, false, addr_absolute, op_ASL, cycle_rmw_abs },
    { 0x1E, "ASL", 3, 7, false, addr_absolute_x, op_ASL, cycle_rmw_absx },
  };
  assign_defs(asl, sizeof(asl) / sizeof(asl[0]));

  static const opcode_def_t lsr[] = {
    { 0x4A, "LSR", 1, 2, false, addr_accumulator, op_LSR, cycle_implied },
    { 0x46, "LSR", 2, 5, false, addr_zero_page, op_LSR, cycle_rmw_zp },
    { 0x56, "LSR", 2, 6, false, addr_zero_page_x, op_LSR, cycle_rmw_zpx },
    { 0x4E, "LSR", 3, 6, false, addr_absolute, op_LSR, cycle_rmw_abs },
    { 0x5E, "LSR", 3, 7, false, addr_absolute_x, op_LSR, cycle_rmw_absx },
  };
  assign_defs(lsr, sizeof(lsr) / sizeof(lsr[0]));

  static const opcode_def_t rol[] = {
    { 0x2A, "ROL", 1, 2, false, addr_accumulator, op_ROL, cycle_implied },
    { 0x26, "ROL", 2, 5, false, addr_zero_page, op_ROL, cycle_rmw_zp },
    { 0x36, "ROL", 2, 6, false, addr_zero_page_x, op_ROL, cycle_rmw_zpx },
    { 0x2E, "ROL", 3, 6, false, addr_absolute, op_ROL, cycle_rmw_abs },
    { 0x3E, "ROL", 3, 7, false, addr_absolute_x, op_ROL, cycle_rmw_absx },
  };
  assign_defs(rol, sizeof(rol) / sizeof(rol[0]));

  static const opcode_def_t ror[] = {
    { 0x6A, "ROR", 1, 2, false, addr_accumulator, op_ROR, cycle_implied },
    { 0x66, "ROR", 2, 5, false, addr_zero_page, op_ROR, cycle_rmw_zp },
    { 0x76, "ROR", 2, 6, false, addr_zero_page_x, op_ROR, cycle_rmw_zpx },
    { 0x6E, "ROR", 3, 6, false, addr_absolute, op_ROR, cycle_rmw_abs },
    { 0x7E, "ROR", 3, 7, false, addr_absolute_x, op_ROR, cycle_rmw_absx },
  };
  assign_defs(ror, sizeof(ror) / sizeof(ror[0]));

  static const opcode_def_t inc[] = {
    { 0xE6, "INC", 2, 5, false, addr_zero_page, op_INC, cycle_rmw_zp },
    { 0xF6, "INC", 2, 6, false, addr_zero_page_x, op_INC, cycle_rmw_zpx },
    { 0xEE, "INC", 3, 6, false, addr_absolute, op_INC, cycle_rmw_abs },
    { 0xFE, "INC", 3, 7, false, addr_absolute_x, op_INC, cycle_rmw_absx },
  };
  assign_defs(inc, sizeof(inc) / sizeof(inc[0]));

  static const opcode_def_t dec[] = {
    { 0xC6, "DEC", 2, 5, false, addr_zero_page, op_DEC, cycle_rmw_zp },
    { 0xD6, "DEC", 2, 6, false, addr_zero_page_x, op_DEC, cycle_rmw_zpx },
    { 0xCE, "DEC", 3, 6, false, addr_absolute, op_DEC, cycle_rmw_abs },
    { 0xDE, "DEC", 3, 7, false, addr_absolute_x, op_DEC, cycle_rmw_absx },
  };
  assign_defs(dec, sizeof(dec) / sizeof(dec[0]));

  // Implied / register: all 1 byte, 2 cycles, no operand.
  static const opcode_def_t implied_ops[] = {
    { 0xAA, "TAX", 1, 2, false, addr_implied, op_TAX, cycle_implied },
    { 0x8A, "TXA", 1, 2, false, addr_implied, op_TXA, cycle_implied },
    { 0xA8, "TAY", 1, 2, false, addr_implied, op_TAY, cycle_implied },
    { 0x98, "TYA", 1, 2, false, addr_implied, op_TYA, cycle_implied },
    { 0xBA, "TSX", 1, 2, false, addr_implied, op_TSX, cycle_implied },
    { 0x9A, "TXS", 1, 2, false, addr_implied, op_TXS, cycle_implied },
    { 0xE8, "INX", 1, 2, false, addr_implied, op_INX, cycle_implied },
    { 0xC8, "INY", 1, 2, false, addr_implied, op_INY, cycle_implied },
    { 0xCA, "DEX", 1, 2, false, addr_implied, op_DEX, cycle_implied },
    { 0x88, "DEY", 1, 2, false, addr_implied, op_DEY, cycle_implied },
    { 0x18, "CLC", 1, 2, false, addr_implied, op_CLC, cycle_implied },
    { 0x38, "SEC", 1, 2, false, addr_implied, op_SEC, cycle_implied },
    { 0x58, "CLI", 1, 2, false, addr_implied, op_CLI, cycle_implied },
    { 0x78, "SEI", 1, 2, false, addr_implied, op_SEI, cycle_implied },
    { 0xB8, "CLV", 1, 2, false, addr_implied, op_CLV, cycle_implied },
    { 0xD8, "CLD", 1, 2, false, addr_implied, op_CLD, cycle_implied },
    { 0xF8, "SED", 1, 2, false, addr_implied, op_SED, cycle_implied },
    { 0xEA, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0x1A, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0x3A, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0x5A, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0x7A, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0xDA, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
    { 0xFA, "NOP", 1, 2, false, addr_implied, op_NOP, cycle_implied },
  };
  assign_defs(implied_ops, sizeof(implied_ops) / sizeof(implied_ops[0]));

  // Stack operations.
  static const opcode_def_t stack_ops[] = {
    { 0x48, "PHA", 1, 3, false, addr_implied, op_PHA, cycle_push },
    { 0x08, "PHP", 1, 3, false, addr_implied, op_PHP, cycle_push },
    { 0x68, "PLA", 1, 4, false, addr_implied, op_PLA, cycle_pull },
    { 0x28, "PLP", 1, 4, false, addr_implied, op_PLP, cycle_pull },
  };
  assign_defs(stack_ops, sizeof(stack_ops) / sizeof(stack_ops[0]));

  // Control transfer.
  static const opcode_def_t jumps[] = {
    { 0x4C, "JMP", 3, 3, false, addr_absolute, op_JMP, cycle_jmp_abs },
    { 0x6C, "JMP", 3, 5, false, addr_indirect, op_JMP, cycle_jmp_ind },
    { 0x20, "JSR", 3, 6, false, addr_absolute, op_JSR, cycle_jsr },
    { 0x60, "RTS", 1, 6, false, addr_implied, op_RTS, cycle_rts },
    { 0x40, "RTI", 1, 6, false, addr_implied, op_RTI, cycle_rti },
    { 0x00, "BRK", 2, 7, false, addr_implied, op_BRK, cycle_brk },
  };
  assign_defs(jumps, sizeof(jumps) / sizeof(jumps[0]));

  // Branches: all 2 bytes, page_penalty = true.
  static const opcode_def_t branches[] = {
    { 0x90, "BCC", 2, 2, true, addr_relative, op_BCC, cycle_branch },
    { 0xB0, "BCS", 2, 2, true, addr_relative, op_BCS, cycle_branch },
    { 0xF0, "BEQ", 2, 2, true, addr_relative, op_BEQ, cycle_branch },
    { 0xD0, "BNE", 2, 2, true, addr_relative, op_BNE, cycle_branch },
    { 0x30, "BMI", 2, 2, true, addr_relative, op_BMI, cycle_branch },
    { 0x10, "BPL", 2, 2, true, addr_relative, op_BPL, cycle_branch },
    { 0x50, "BVC", 2, 2, true, addr_relative, op_BVC, cycle_branch },
    { 0x70, "BVS", 2, 2, true, addr_relative, op_BVS, cycle_branch },
  };
  assign_defs(branches, sizeof(branches) / sizeof(branches[0]));

  // NOP immediate: 2-byte NOPs that read and discard an operand byte.
  static const opcode_def_t nop_imm[] = {
    { 0x80, "NOP", 2, 2, false, addr_immediate, op_NOP, cycle_read_immediate },
    { 0x82, "NOP", 2, 2, false, addr_immediate, op_NOP, cycle_read_immediate },
    { 0x89, "NOP", 2, 2, false, addr_immediate, op_NOP, cycle_read_immediate },
    { 0xC2, "NOP", 2, 2, false, addr_immediate, op_NOP, cycle_read_immediate },
    { 0xE2, "NOP", 2, 2, false, addr_immediate, op_NOP, cycle_read_immediate },
    { 0x04, "NOP", 2, 3, false, addr_zero_page, op_NOP, cycle_read_zp },
    { 0x44, "NOP", 2, 3, false, addr_zero_page, op_NOP, cycle_read_zp },
    { 0x64, "NOP", 2, 3, false, addr_zero_page, op_NOP, cycle_read_zp },
    { 0x14, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0x34, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0x54, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0x74, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0xD4, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0xF4, "NOP", 2, 4, false, addr_zero_page_x, op_NOP, cycle_read_zpx },
    { 0x0C, "NOP", 3, 4, false, addr_absolute, op_NOP, cycle_read_absolute },
    { 0x1C, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
    { 0x3C, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
    { 0x5C, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
    { 0x7C, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
    { 0xDC, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
    { 0xFC, "NOP", 3, 4, true, addr_absolute_x, op_NOP, cycle_read_absx },
  };
  assign_defs(nop_imm, sizeof(nop_imm) / sizeof(nop_imm[0]));

  // Illegal / undocumented opcodes used by games and nestest.
  static const opcode_def_t illegal_ops[] = {
    { 0xA7, "LAX", 2, 3, false, addr_zero_page, op_LAX, cycle_read_zp },
    { 0xB7, "LAX", 2, 4, false, addr_zero_page_y, op_LAX, cycle_read_zpx },
    { 0xAF, "LAX", 3, 4, false, addr_absolute, op_LAX, cycle_read_absolute },
    { 0xBF, "LAX", 3, 4, true, addr_absolute_y, op_LAX, cycle_read_absy },
    { 0xA3, "LAX", 2, 6, false, addr_indexed_indirect, op_LAX, cycle_read_idrx },
    { 0xB3, "LAX", 2, 5, true, addr_indirect_indexed, op_LAX, cycle_read_idry },
    { 0x87, "SAX", 2, 3, false, addr_zero_page, op_SAX, cycle_write_zp },
    { 0x97, "SAX", 2, 4, false, addr_zero_page_y, op_SAX, cycle_write_zpx },
    { 0x8F, "SAX", 3, 4, false, addr_absolute, op_SAX, cycle_write_absolute },
    { 0x83, "SAX", 2, 6, false, addr_indexed_indirect, op_SAX, cycle_write_idrx },
    { 0xC7, "DCP", 2, 5, false, addr_zero_page, op_DCP, cycle_rmw_zp },
    { 0xD7, "DCP", 2, 6, false, addr_zero_page_x, op_DCP, cycle_rmw_zpx },
    { 0xCF, "DCP", 3, 6, false, addr_absolute, op_DCP, cycle_rmw_abs },
    { 0xDF, "DCP", 3, 7, false, addr_absolute_x, op_DCP, cycle_rmw_absx },
    { 0xDB, "DCP", 3, 7, false, addr_absolute_y, op_DCP, cycle_rmw_absy },
    { 0xC3, "DCP", 2, 8, false, addr_indexed_indirect, op_DCP, cycle_rmw_idrx },
    { 0xD3, "DCP", 2, 8, false, addr_indirect_indexed, op_DCP, cycle_rmw_idry },
    { 0xE7, "ISB", 2, 5, false, addr_zero_page, op_ISB, cycle_rmw_zp },
    { 0xF7, "ISB", 2, 6, false, addr_zero_page_x, op_ISB, cycle_rmw_zpx },
    { 0xEF, "ISB", 3, 6, false, addr_absolute, op_ISB, cycle_rmw_abs },
    { 0xFF, "ISB", 3, 7, false, addr_absolute_x, op_ISB, cycle_rmw_absx },
    { 0xFB, "ISB", 3, 7, false, addr_absolute_y, op_ISB, cycle_rmw_absy },
    { 0xE3, "ISB", 2, 8, false, addr_indexed_indirect, op_ISB, cycle_rmw_idrx },
    { 0xF3, "ISB", 2, 8, false, addr_indirect_indexed, op_ISB, cycle_rmw_idry },
    { 0x07, "SLO", 2, 5, false, addr_zero_page, op_SLO, cycle_rmw_zp },
    { 0x17, "SLO", 2, 6, false, addr_zero_page_x, op_SLO, cycle_rmw_zpx },
    { 0x0F, "SLO", 3, 6, false, addr_absolute, op_SLO, cycle_rmw_abs },
    { 0x1F, "SLO", 3, 7, false, addr_absolute_x, op_SLO, cycle_rmw_absx },
    { 0x1B, "SLO", 3, 7, false, addr_absolute_y, op_SLO, cycle_rmw_absy },
    { 0x03, "SLO", 2, 8, false, addr_indexed_indirect, op_SLO, cycle_rmw_idrx },
    { 0x13, "SLO", 2, 8, false, addr_indirect_indexed, op_SLO, cycle_rmw_idry },
    { 0x27, "RLA", 2, 5, false, addr_zero_page, op_RLA, cycle_rmw_zp },
    { 0x37, "RLA", 2, 6, false, addr_zero_page_x, op_RLA, cycle_rmw_zpx },
    { 0x2F, "RLA", 3, 6, false, addr_absolute, op_RLA, cycle_rmw_abs },
    { 0x3F, "RLA", 3, 7, false, addr_absolute_x, op_RLA, cycle_rmw_absx },
    { 0x3B, "RLA", 3, 7, false, addr_absolute_y, op_RLA, cycle_rmw_absy },
    { 0x23, "RLA", 2, 8, false, addr_indexed_indirect, op_RLA, cycle_rmw_idrx },
    { 0x33, "RLA", 2, 8, false, addr_indirect_indexed, op_RLA, cycle_rmw_idry },
    { 0x47, "SRE", 2, 5, false, addr_zero_page, op_SRE, cycle_rmw_zp },
    { 0x57, "SRE", 2, 6, false, addr_zero_page_x, op_SRE, cycle_rmw_zpx },
    { 0x4F, "SRE", 3, 6, false, addr_absolute, op_SRE, cycle_rmw_abs },
    { 0x5F, "SRE", 3, 7, false, addr_absolute_x, op_SRE, cycle_rmw_absx },
    { 0x5B, "SRE", 3, 7, false, addr_absolute_y, op_SRE, cycle_rmw_absy },
    { 0x43, "SRE", 2, 8, false, addr_indexed_indirect, op_SRE, cycle_rmw_idrx },
    { 0x53, "SRE", 2, 8, false, addr_indirect_indexed, op_SRE, cycle_rmw_idry },
    { 0x67, "RRA", 2, 5, false, addr_zero_page, op_RRA, cycle_rmw_zp },
    { 0x77, "RRA", 2, 6, false, addr_zero_page_x, op_RRA, cycle_rmw_zpx },
    { 0x6F, "RRA", 3, 6, false, addr_absolute, op_RRA, cycle_rmw_abs },
    { 0x7F, "RRA", 3, 7, false, addr_absolute_x, op_RRA, cycle_rmw_absx },
    { 0x7B, "RRA", 3, 7, false, addr_absolute_y, op_RRA, cycle_rmw_absy },
    { 0x63, "RRA", 2, 8, false, addr_indexed_indirect, op_RRA, cycle_rmw_idrx },
    { 0x73, "RRA", 2, 8, false, addr_indirect_indexed, op_RRA, cycle_rmw_idry },
    { 0x0B, "ANC", 2, 2, false, addr_immediate, op_ANC, cycle_read_immediate },
    { 0x2B, "ANC", 2, 2, false, addr_immediate, op_ANC, cycle_read_immediate },
    { 0x4B, "ALR", 2, 2, false, addr_immediate, op_ALR, cycle_read_immediate },
    { 0x6B, "ARR", 2, 2, false, addr_immediate, op_ARR, cycle_read_immediate },
    { 0xAB, "LXA", 2, 2, false, addr_immediate, op_LXA, cycle_read_immediate },
    { 0xCB, "SBX", 2, 2, false, addr_immediate, op_SBX, cycle_read_immediate },
    // SHY/SHX: undocumented stores with address-bus AND effect
    { 0x9C, "SHY", 3, 5, false, addr_absolute_x, op_SHY, cycle_write_absx },
    { 0x9E, "SHX", 3, 5, false, addr_absolute_y, op_SHX, cycle_write_absy },
  };
  assign_defs(illegal_ops, sizeof(illegal_ops) / sizeof(illegal_ops[0]));
}

static void push_interrupt(sb_6502_t* cpu, sb_bus_t* bus, uint16_t vector) {
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)(cpu->pc >> 8));
  cpu->s--;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, (uint8_t)cpu->pc);
  cpu->s--;
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, cpu->p & ~SB_6502_BRK);
  cpu->s--;
  cpu->p |= SB_6502_INTERRUPT;
  uint8_t lo = sb_bus_read(bus, vector);
  uint8_t hi = sb_bus_read(bus, vector + 1);
  cpu->pc = ((uint16_t)hi << 8) | lo;
  cpu->cycles += 7;
}

// DEPRECATED: legacy batch execution. Use sb_6502_cycle for cycle-interleaved mode.
void sb_6502_step(sb_6502_t* cpu, sb_bus_t* bus) {
  // Handle interrupts
  if (cpu->nmi_pending) {
    cpu->nmi_pending = false;
    push_interrupt(cpu, bus, 0xFFFA);
    return;
  }
  if (cpu->irq_pending && !(cpu->p & SB_6502_INTERRUPT)) {
    cpu->irq_pending = false;
    push_interrupt(cpu, bus, 0xFFFE);
    return;
  }

  // Fetch opcode
  uint8_t opcode = sb_bus_read(bus, cpu->pc++);

  // Decode
  sb_6502_opcode_t* info = &opcodes[opcode];

  // Execute addressing mode to get effective address
  cpu->result = info->addr_mode(cpu, bus);

  // Pre-fetch the value at the effective address into cpu->fetched,
  // then write-back after func completes. FUNC reads from cpu->fetched
  // (for READ) or sets cpu->fetched (for WRITE/RMW). The write-back
  // stores the result. Stack instructions (PHA, PHP, JSR, interrupt)
  // write to the stack directly and are NOT affected by this mechanism.
  cpu->fetched = sb_bus_read(bus, cpu->result.addr);
  info->func(cpu, bus, opcode);
  sb_bus_write(bus, cpu->result.addr, cpu->fetched);

  // Add cycles
  cpu->cycles += info->cycles;
  if (info->page_penalty && cpu->result.page_crossed) {
    cpu->cycles++;
  }
}

// Category: Implied/Register (TAX, TAY, TXA, TYA, TSX, TXS, DEX, DEY,
// INX, INY, NOP, CLC, SEC, CLI, SEI, CLV, CLD, SED, ASL A, LSR A, ROL A, ROR A)
// Cycle pattern: 2 cycles (opcode fetch + internal op).
static int cycle_implied(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Immediate (LDA #imm, LDX #imm, LDY #imm, AND #imm,
// ORA #imm, EOR #imm, ADC #imm, SBC #imm, CMP #imm, CPX #imm, CPY #imm)
// Cycle pattern: 2 cycles (opcode fetch + read value).
static int cycle_read_immediate(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->fetched = sb_bus_read(bus, cpu->pc++);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Zero Page (LDA zp, LDX zp, LDY zp, AND zp, ORA zp,
// EOR zp, ADC zp, SBC zp, CMP zp, CPX zp, CPY zp, BIT zp, LAX zp)
// Cycle pattern: 3 cycles (opcode + addr + read value).
static int cycle_read_zp(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Absolute (LDA abs, LDX abs, LDY abs, AND abs, ORA abs,
// EOR abs, ADC abs, SBC abs, CMP abs, BIT abs)
// Cycle pattern: 4 cycles (opcode + lo + hi + read value).
static int cycle_read_absolute(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Zero Page,X (LDA zp,X, AND zp,X, etc.)
// Cycle pattern: 4 cycles (opcode + addr + add X + read value).
static int cycle_read_zpx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2: {
    uint8_t idx = (cpu->opcode == 0xB6 || cpu->opcode == 0xB7) ? cpu->y : cpu->x;
    cpu->addr_abs = (uint8_t)(cpu->addr_abs + idx);
    rc = SB_IN_PROGRESS;
    break;
  }
  case 3:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Absolute,X (LDA abs,X, AND abs,X, etc.) with page cross.
// Cycle pattern: 4 cycles + 1 if page cross.
static int cycle_read_absx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3: {
    uint16_t base = cpu->addr_abs;
    cpu->addr_abs = base + cpu->x;
    if ((base & 0xFF00) != (cpu->addr_abs & 0xFF00)) {
      // Page crossed: dummy read with wrong high byte, then real read in case 4.
      sb_bus_read(bus, (base & 0xFF00) | (cpu->addr_abs & 0x00FF));
      rc = SB_IN_PROGRESS;
      break;
    }
    // No page cross: read value directly.
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  }
  case 4:
    // Page-cross penalty cycle: read from correct address.
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Zero Page (STA zp, STX zp, STY zp, SAX zp)
// Cycle pattern: 3 cycles (opcode + addr + write).
static int cycle_write_zp(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Absolute (STA abs, STX abs, STY abs, SAX abs)
// Cycle pattern: 4 cycles (opcode + lo + hi + write).
static int cycle_write_absolute(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Branch (BCC, BCS, BEQ, BNE, BMI, BPL, BVC, BVS)
// Cycle pattern: 2/3/4 cycles depending on taken/page cross.
static const struct {
  uint8_t mask; // flag bit to test
  bool expect;  // true = branch when set, false = branch when clear
} branch_tests[8] = {
  { SB_6502_NEGATIVE, false }, // BPL: N=0
  { SB_6502_NEGATIVE, true },  // BMI: N=1
  { SB_6502_OVERFLOW, false }, // BVC: V=0
  { SB_6502_OVERFLOW, true },  // BVS: V=1
  { SB_6502_CARRY, false },    // BCC: C=0
  { SB_6502_CARRY, true },     // BCS: C=1
  { SB_6502_ZERO, false },     // BNE: Z=0
  { SB_6502_ZERO, true },      // BEQ: Z=1
};

static int cycle_branch(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);

  uint8_t bi = (cpu->opcode >> 5); // branch rows: 1,3,5,7,9,11,13,15 → 0-7

  switch (cpu->phase++) {
  case 0:
    // Unreachable if phase starts at 1
    rc = SB_IN_PROGRESS;
    break;
  case 1: {
    cpu->addr_rel = (int8_t)sb_bus_read(bus, cpu->pc++);
    bool flag_set = (cpu->p & branch_tests[bi].mask) != 0;
    bool taken = (flag_set == branch_tests[bi].expect);
    if (!taken) {
      rc = SB_OK; // 2 cycles, branch not taken
    } else {
      cpu->addr_abs = cpu->pc + cpu->addr_rel;
      rc = SB_IN_PROGRESS; // taken, need at least 3 cycles
    }
    break;
  }
  case 2:
    if ((cpu->pc & 0xFF00) != (cpu->addr_abs & 0xFF00)) {
      cpu->pc = cpu->addr_abs;
      rc = SB_IN_PROGRESS; // page crossed, need 4 cycles
    } else {
      cpu->pc = cpu->addr_abs;
      rc = SB_OK; // 3 cycles, taken same page
    }
    break;
  case 3:
    rc = SB_OK; // 4 cycles, page cross penalty completed
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: JMP Absolute (3 cycles)
static int cycle_jmp_abs(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2: {
    uint8_t hi = sb_bus_read(bus, cpu->pc);
    cpu->pc = ((uint16_t)hi << 8) | cpu->addr_abs;
    rc = SB_OK;
    break;
  }
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: JMP Indirect (5 cycles) with 6502 page-wrap bug
static int cycle_jmp_ind(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3: {
    uint16_t pointer = cpu->addr_abs;
    cpu->fetched = sb_bus_read(bus, pointer);
    cpu->addr_abs = sb_bus_read(bus, (pointer & 0xFF00) | (uint8_t)(pointer + 1));
    rc = SB_IN_PROGRESS;
    break;
  }
  case 4:
    cpu->pc = ((uint16_t)cpu->addr_abs << 8) | cpu->fetched;
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: JSR (6 cycles)
static int cycle_jsr(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    sb_bus_read(bus, 0x0100 | cpu->s);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc >> 8);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc & 0xFF);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 5: {
    uint8_t hi = sb_bus_read(bus, cpu->pc);
    cpu->pc = ((uint16_t)hi << 8) | cpu->addr_abs;
    rc = SB_OK;
    break;
  }
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RTS (6 cycles)
static int cycle_rts(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->s++;
    cpu->fetched = sb_bus_read(bus, 0x0100 | cpu->s);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->s++;
    cpu->addr_abs = (uint16_t)sb_bus_read(bus, 0x0100 | cpu->s) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->pc = (cpu->addr_abs | cpu->fetched) + 1;
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RTI (6 cycles)
static int cycle_rti(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->s++;
    cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
    cpu->p = sb_bus_read(bus, 0x0100 | cpu->s);
    cpu->p &= ~SB_6502_BRK;
    cpu->p |= SB_6502_UNUSED;
    cpu->irq_delay_next = true; // I flag might have changed
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->s++;
    cpu->fetched = sb_bus_read(bus, 0x0100 | cpu->s);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->s++;
    cpu->pc = ((uint16_t)sb_bus_read(bus, 0x0100 | cpu->s) << 8) | cpu->fetched;
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

static int cycle_brk(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    // Cycle 2: Read padding byte and increment PC.
    (void)sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    // Cycle 3: Push PCH.
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc >> 8);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    // Cycle 4: Push PCL.
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc & 0xFF);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    // Cycle 5: Push P with B flag and Unused flag set.
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->p | SB_6502_BRK | SB_6502_UNUSED);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    // Cycle 6: Read vector low byte.
    cpu->fetched = sb_bus_read(bus, 0xFFFE);
    rc = SB_IN_PROGRESS;
    break;
  case 6: {
    // Cycle 7: Read vector high byte, jump, set Interrupt Disable flag.
    uint8_t hi = sb_bus_read(bus, 0xFFFF);
    cpu->pc = ((uint16_t)hi << 8) | cpu->fetched;
    cpu->p |= SB_6502_INTERRUPT;
    rc = SB_OK;
    break;
  }
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Stack Push (PHA, PHP) — 3 cycles.
// Self-contained: no func call (stack ops bypass pre-fetch mechanism).
static int cycle_push(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    if (cpu->opcode == 0x48) { // PHA
      sb_bus_write(bus, 0x0100 | cpu->s, cpu->a);
    } else { // PHP
      sb_bus_write(bus, 0x0100 | cpu->s, cpu->p | SB_6502_BRK | SB_6502_UNUSED);
    }
    cpu->s--;
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Stack Pull (PLA, PLP) — 4 cycles.
// Self-contained: no func call (stack ops bypass pre-fetch mechanism).
static int cycle_pull(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->s++;
    if (cpu->opcode == 0x68) { // PLA
      cpu->a = sb_bus_read(bus, 0x0100 | cpu->s);
      SET_ZN(cpu, cpu->a);
    } else { // PLP
      uint8_t new_p = sb_bus_read(bus, 0x0100 | cpu->s);
      cpu->i_flag_saved = cpu->p & SB_6502_INTERRUPT; // save old I
      cpu->p = new_p;
      cpu->p &= ~SB_6502_BRK;
      cpu->p |= SB_6502_UNUSED;
      cpu->irq_delay_next = true; // I flag might have changed
    }
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Zero Page (ASL zp, LSR zp, ROL zp, ROR zp, INC zp, DEC zp)
// Cycle pattern: 5 cycles (opcode + addr + read + modify + write).
static int cycle_rmw_zp(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Absolute (ASL abs, LSR abs, ROL abs, ROR abs, INC abs, DEC abs)
// Cycle pattern: 6 cycles (opcode + lo + hi + read + modify + write).
static int cycle_rmw_abs(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);

  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Absolute,Y (same pattern as rmw_absx but with Y).
static int cycle_rmw_absy(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs += cpu->y;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 6:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Indexed Indirect (ASL ($E6,X), etc.)
// Cycle pattern: 8 cycles.
static int cycle_rmw_idrx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1: {
    uint8_t zp = sb_bus_read(bus, cpu->pc++);
    cpu->addr_abs = (uint8_t)(zp + cpu->x);
    rc = SB_IN_PROGRESS;
    break;
  }
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = (uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->addr_abs |= cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 6:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 7:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Indirect Indexed (ASL ($E6),Y, etc.)
// Cycle pattern: 8 cycles.
static int cycle_rmw_idry(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = ((uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8) | cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->addr_abs += cpu->y;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 6:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 7:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Absolute,Y (STA abs,Y)
static int cycle_write_absy(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs += cpu->y;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Zero Page,X (STA zp,X, STY zp,X, STX zp,Y, SAX zp,Y)
// Cycle pattern: 4 cycles (opcode + addr + add X + write).
static int cycle_write_zpx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2: {
    uint8_t idx = (cpu->opcode == 0x96 || cpu->opcode == 0x97) ? cpu->y : cpu->x;
    cpu->addr_abs = (uint8_t)(cpu->addr_abs + idx);
    rc = SB_IN_PROGRESS;
    break;
  }
  case 3:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Absolute,X/Y (STA abs,X, STA abs,Y)
// Cycle pattern: 5 cycles (opcode + lo + hi + add index + write).
static int cycle_write_absx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs += cpu->x; // abs,X — no page wrap for writes
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Absolute,Y (LDA abs,Y, AND abs,Y, etc.)
// Cycle pattern: 4 cycles + 1 if page cross (same as abs,X but with Y).
static int cycle_read_absy(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3: {
    uint16_t base = cpu->addr_abs;
    cpu->addr_abs = base + cpu->y;
    if ((base & 0xFF00) != (cpu->addr_abs & 0xFF00)) {
      sb_bus_read(bus, (base & 0xFF00) | (cpu->addr_abs & 0x00FF));
      rc = SB_IN_PROGRESS;
      break;
    }
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  }
  case 4:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Indexed Indirect (LDA ($E6,X), AND ($E6,X), etc.)
// Cycle pattern: 6 cycles (opcode + zp addr + read ptr lo + read ptr hi + read value).
static int cycle_read_idrx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1: {
    uint8_t zp = sb_bus_read(bus, cpu->pc++);
    cpu->addr_abs = (uint8_t)(zp + cpu->x); // zero-page wrap
    rc = SB_IN_PROGRESS;
    break;
  }
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = (uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->addr_abs |= cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Read Indirect Indexed (LDA ($E6),Y, AND ($E6),Y, etc.)
// Cycle pattern: 5 cycles + 1 if page cross.
static int cycle_read_idry(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = ((uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8) | cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 4: {
    uint16_t base = cpu->addr_abs;
    cpu->addr_abs = base + cpu->y;
    if ((base & 0xFF00) != (cpu->addr_abs & 0xFF00)) {
      sb_bus_read(bus, (base & 0xFF00) | (cpu->addr_abs & 0x00FF));
      rc = SB_IN_PROGRESS;
      break;
    }
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  }
  case 5:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Indexed Indirect (STA ($E6,X), SAX ($E6,X))
// Cycle pattern: 6 cycles (opcode + zp+X + read ptr lo + read ptr hi + write).
static int cycle_write_idrx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1: {
    uint8_t zp = sb_bus_read(bus, cpu->pc++);
    cpu->addr_abs = (uint8_t)(zp + cpu->x);
    rc = SB_IN_PROGRESS;
    break;
  }
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = (uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->addr_abs |= cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: Write Indirect Indexed (STA ($E6),Y)
// Cycle pattern: 6 cycles (opcode + zp + read ptr lo + read ptr hi + add Y + write).
static int cycle_write_idry(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs = ((uint16_t)sb_bus_read(bus, (uint8_t)(cpu->addr_abs + 1)) << 8) | cpu->fetched;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->addr_abs += cpu->y;
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Zero Page,X (ASL zp,X, LSR zp,X, etc.)
// Cycle pattern: 6 cycles (opcode + addr + add X + read + modify + write).
static int cycle_rmw_zpx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs = (uint8_t)(cpu->addr_abs + cpu->x);
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Category: RMW Absolute,X (ASL abs,X, LSR abs,X, etc.)
// Cycle pattern: 7 cycles (opcode + lo + hi + add X + read + modify + write).
static int cycle_rmw_absx(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0 && bus != 0);
  assert(cpu->opcode < 0x100);
  assert(opcodes[cpu->opcode].func != 0);
  switch (cpu->phase++) {
  case 0:
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    cpu->addr_abs = sb_bus_read(bus, cpu->pc++);
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    cpu->addr_abs |= (uint16_t)sb_bus_read(bus, cpu->pc++) << 8;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    cpu->addr_abs += cpu->x;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->fetched = sb_bus_read(bus, cpu->addr_abs);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    opcodes[cpu->opcode].func(cpu, bus, (uint8_t)cpu->opcode);
    rc = SB_IN_PROGRESS;
    break;
  case 6:
    sb_bus_write(bus, cpu->addr_abs, cpu->fetched);
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// Fetch the next instruction opcode. Called when cpu->phase == 0 and
// cpu->opcode == 0 (no instruction in progress).
// Returns SB_OK after the opcode is read (or an interrupt is taken).
// On the NEXT sb_6502_cycle() call, the cycle handler runs phase 0.
static int fetch_opcode(sb_6502_t* cpu, sb_bus_t* bus) {
  assert(cpu != 0);
  assert(bus != 0);

  // NMI has higher priority than IRQ.
  // Check PPU nmi_pending through bus->ppu (PPU is off-bus in real HW).
  // Reading $2002 clears ppu->nmi_pending, suppressing NMI for this fetch.
  if (bus->ppu && bus->ppu->nmi_pending) {
    bus->ppu->nmi_pending = false;
    cpu->opcode = 0x100; // NMI sentinel (not a real opcode)
    cpu->phase = 0;
    return SB_OK;
  }

  // IRQ: 6502 one-instruction delay for I flag changes.
  bool check_i;
  if (cpu->irq_delay_next) {
    check_i = cpu->i_flag_saved; // use I flag from BEFORE the change
    cpu->irq_delay_next = false; // only applies to this one fetch
  } else {
    check_i = cpu->p & SB_6502_INTERRUPT; // use current I flag
  }

  // IRQ check: bus->irq_pending (set by APU frame counter, cleared
  // by $4015 read or $4017 write). I flag prevents re-entry.
  if (bus->irq_pending && !check_i) {
    cpu->opcode = 0x101; // IRQ sentinel (not a real opcode)
    cpu->phase = 0;
    return SB_OK;
  }

  // Read the opcode byte from memory. bus_read returns uint8_t,
  // so opcode is always 0x00-0xFF after this point.
  cpu->opcode = sb_bus_read(bus, cpu->pc++);

  // Zero-initialize temporaries for the new instruction.
  cpu->phase = 1;
  cpu->addr_abs = 0;
  cpu->addr_rel = 0;
  cpu->fetched = 0;

  return SB_OK;
}

// NMI sequence: 7 cycles. Uses sentinel opcode 0x100.
static int cycle_nmi(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU; // fail closed
  assert(cpu != 0);
  assert(bus != 0);

  switch (cpu->phase++) {
  case 0:
    // Dummy read (internal).
    (void)sb_bus_read(bus, cpu->pc);
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    // Push PCH to stack.
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc >> 8);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    // Push PCL to stack.
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc & 0xFF);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    // Push P (with B flag clear for NMI).
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->p & ~SB_6502_BRK);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    // Read low byte of NMI vector ($FFFA).
    cpu->fetched = sb_bus_read(bus, 0xFFFA);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    // Read high byte of NMI vector, jump.
    cpu->pc = ((uint16_t)sb_bus_read(bus, 0xFFFB) << 8) | cpu->fetched;
    cpu->p |= SB_6502_INTERRUPT;
    cpu->opcode = 0; // signal: fetch_opcode on next cycle
    rc = SB_OK;      // sequence complete (sb_6502_cycle resets phase)
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

// IRQ sequence: 7 cycles. Uses sentinel opcode 0x101.
static int cycle_irq(sb_6502_t* cpu, sb_bus_t* bus) {
  int rc = SB_ERR_CPU;
  assert(cpu != 0);
  assert(bus != 0);

  switch (cpu->phase++) {
  case 0:
    (void)sb_bus_read(bus, cpu->pc);
    rc = SB_IN_PROGRESS;
    break;
  case 1:
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc >> 8);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 2:
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->pc & 0xFF);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 3:
    sb_bus_write(bus, 0x0100 | cpu->s, cpu->p & ~SB_6502_BRK);
    cpu->s--;
    rc = SB_IN_PROGRESS;
    break;
  case 4:
    cpu->fetched = sb_bus_read(bus, 0xFFFE);
    rc = SB_IN_PROGRESS;
    break;
  case 5:
    cpu->pc = ((uint16_t)sb_bus_read(bus, 0xFFFF) << 8) | cpu->fetched;
    cpu->p |= SB_6502_INTERRUPT;
    cpu->opcode = 0;
    rc = SB_OK;
    break;
  default:
    rc = SB_ERR_CPU;
    break;
  }
  return rc;
}

int sb_6502_cycle(sb_6502_t* cpu, sb_bus_t* bus) {
  // From c-defensive: validate every external input at the boundary.
  if (!cpu || !bus)
    return SB_ERR_INVAL;

  // Track one master cycle per call.
  cpu->cycles++;

  // If no instruction is in progress, fetch the next one.
  if (cpu->phase == 0 && cpu->opcode == 0) {
    return fetch_opcode(cpu, bus);
  }

  int rc;
  if (cpu->opcode == 0x100) {
    rc = cycle_nmi(cpu, bus);
  } else if (cpu->opcode == 0x101) {
    rc = cycle_irq(cpu, bus);
  } else {
    // From c-defensive: assert invariant before dispatch.
    assert(cpu->opcode < 0x100);
    assert(opcodes[cpu->opcode].cycle != 0);

    // Dispatch to the current instruction's cycle handler.
    rc = opcodes[cpu->opcode].cycle(cpu, bus);
  }

  // On completion or error, reset state so the next sb_6502_cycle call
  // enters fetch_opcode to read the next instruction (or retry after error).
  // SB_IN_PROGRESS leaves opcode/phase unchanged for multi-cycle instructions.
  if (rc == SB_OK || rc < 0) {
    cpu->opcode = 0;
    cpu->phase = 0;
  }

  return rc;
}

void sb_6502_reset(sb_6502_t* cpu, sb_bus_t* bus) {
  cpu->a = 0;
  cpu->x = 0;
  cpu->y = 0;
  cpu->s = 0xFD;
  cpu->p = SB_6502_UNUSED | SB_6502_INTERRUPT;
  cpu->cycles = 0;
  cpu->nmi_pending = false;
  cpu->irq_pending = false;
  cpu->irq_delay_next = false;
  // Zero cycle-interleave state (from c-defensive: NULL/zero-init all state)
  cpu->opcode = 0;
  cpu->phase = 0;
  cpu->addr_abs = 0;
  cpu->addr_rel = 0;
  cpu->fetched = 0;
  uint8_t lo = sb_bus_read(bus, 0xFFFC);
  uint8_t hi = sb_bus_read(bus, 0xFFFD);
  cpu->pc = ((uint16_t)hi << 8) | lo;
}

void sb_6502_nmi(sb_6502_t* cpu, sb_bus_t* bus) { cpu->nmi_pending = true; }

void sb_6502_irq(sb_6502_t* cpu, sb_bus_t* bus) {
  if (!(cpu->p & SB_6502_INTERRUPT)) {
    cpu->irq_pending = true;
  }
}
