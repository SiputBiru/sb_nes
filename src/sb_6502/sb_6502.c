#include "sb_6502.h"
#include <assert.h>

static sb_6502_opcode_t opcodes[256];

static int cycle_illegal(struct sb_6502_t* cpu, sb_bus_t* bus) {
  (void)cpu;
  (void)bus;
  // no real cycle handlers yet. Always fail.
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

// status flag instruction
static void op_CLC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p &= ~SB_6502_CARRY; }
static void op_SEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p |= SB_6502_CARRY; }
static void op_CLI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p &= ~SB_6502_INTERRUPT; }
static void op_SEI(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) { cpu->p |= SB_6502_INTERRUPT; }
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
  cpu->p = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  cpu->p &= ~SB_6502_BRK;
  cpu->p |= SB_6502_UNUSED;
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
  cpu->p = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  // cpu->p &= ~(SB_6502_BRK | SB_6502_UNUSED);
  cpu->p &= ~(SB_6502_BRK);
  cpu->p |= SB_6502_UNUSED;
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

// Opcode dispatch table initialization
// Forward declarations for cycle handlers (defined later)
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
static int cycle_write_absy(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_zpx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_absx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_absy(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_idrx(sb_6502_t* cpu, sb_bus_t* bus);
static int cycle_rmw_idry(sb_6502_t* cpu, sb_bus_t* bus);

void sb_6502_init_opcodes(void) {

  // default all 256 to NOP with cycle_illegal stub
  // (real cycle handlers assigned in Phase 3)
  for (int i = 0; i < 256; i++) {
    opcodes[i] = (sb_6502_opcode_t){ "XX", 0, 0, false, addr_implied, op_NOP, cycle_illegal };
  }

  // LDA: 8 variants
  opcodes[0xA9] = (sb_6502_opcode_t){ "LDA", 2, 2, false, addr_immediate, op_LDA };
  opcodes[0xA5] = (sb_6502_opcode_t){ "LDA", 2, 3, false, addr_zero_page, op_LDA };
  opcodes[0xB5] = (sb_6502_opcode_t){ "LDA", 2, 4, false, addr_zero_page_x, op_LDA };
  opcodes[0xAD] = (sb_6502_opcode_t){ "LDA", 3, 4, false, addr_absolute, op_LDA };
  opcodes[0xBD] = (sb_6502_opcode_t){ "LDA", 3, 4, true, addr_absolute_x, op_LDA };
  opcodes[0xB9] = (sb_6502_opcode_t){ "LDA", 3, 4, true, addr_absolute_y, op_LDA };
  opcodes[0xA1] = (sb_6502_opcode_t){ "LDA", 2, 6, false, addr_indexed_indirect, op_LDA };
  opcodes[0xB1] = (sb_6502_opcode_t){ "LDA", 2, 5, true, addr_indirect_indexed, op_LDA };

  // LDX 5 variants
  opcodes[0xA2] = (sb_6502_opcode_t){ "LDX", 2, 2, false, addr_immediate, op_LDX };
  opcodes[0xA6] = (sb_6502_opcode_t){ "LDX", 2, 3, false, addr_zero_page, op_LDX };
  opcodes[0xB6] = (sb_6502_opcode_t){ "LDX", 2, 4, false, addr_zero_page_y, op_LDX };
  opcodes[0xAE] = (sb_6502_opcode_t){ "LDX", 3, 4, false, addr_absolute, op_LDX };
  opcodes[0xBE] = (sb_6502_opcode_t){ "LDX", 3, 4, true, addr_absolute_y, op_LDX };

  // LDY: 5 variants
  opcodes[0xA0] = (sb_6502_opcode_t){ "LDY", 2, 2, false, addr_immediate, op_LDY };
  opcodes[0xA4] = (sb_6502_opcode_t){ "LDY", 2, 3, false, addr_zero_page, op_LDY };
  opcodes[0xB4] = (sb_6502_opcode_t){ "LDY", 2, 4, false, addr_zero_page_x, op_LDY };
  opcodes[0xAC] = (sb_6502_opcode_t){ "LDY", 3, 4, false, addr_absolute, op_LDY };
  opcodes[0xBC] = (sb_6502_opcode_t){ "LDY", 3, 4, true, addr_absolute_x, op_LDY };

  // STA: 7 variants (no immediate)
  opcodes[0x85] = (sb_6502_opcode_t){ "STA", 2, 3, false, addr_zero_page, op_STA };
  opcodes[0x95] = (sb_6502_opcode_t){ "STA", 2, 4, false, addr_zero_page_x, op_STA };
  opcodes[0x8D] = (sb_6502_opcode_t){ "STA", 3, 4, false, addr_absolute, op_STA };
  opcodes[0x9D] = (sb_6502_opcode_t){ "STA", 3, 5, false, addr_absolute_x, op_STA };
  opcodes[0x99] = (sb_6502_opcode_t){ "STA", 3, 5, false, addr_absolute_y, op_STA };
  opcodes[0x81] = (sb_6502_opcode_t){ "STA", 2, 6, false, addr_indexed_indirect, op_STA };
  opcodes[0x91] = (sb_6502_opcode_t){ "STA", 2, 6, false, addr_indirect_indexed, op_STA };

  // STX: 3 variants
  opcodes[0x86] = (sb_6502_opcode_t){ "STX", 2, 3, false, addr_zero_page, op_STX };
  opcodes[0x96] = (sb_6502_opcode_t){ "STX", 2, 4, false, addr_zero_page_y, op_STX };
  opcodes[0x8E] = (sb_6502_opcode_t){ "STX", 3, 4, false, addr_absolute, op_STX };

  // STY: 3 variants
  opcodes[0x84] = (sb_6502_opcode_t){ "STY", 2, 3, false, addr_zero_page, op_STY };
  opcodes[0x94] = (sb_6502_opcode_t){ "STY", 2, 4, false, addr_zero_page_x, op_STY };
  opcodes[0x8C] = (sb_6502_opcode_t){ "STY", 3, 4, false, addr_absolute, op_STY };

  // ADC: 8 variants
  opcodes[0x69] = (sb_6502_opcode_t){ "ADC", 2, 2, false, addr_immediate, op_ADC };
  opcodes[0x65] = (sb_6502_opcode_t){ "ADC", 2, 3, false, addr_zero_page, op_ADC };
  opcodes[0x75] = (sb_6502_opcode_t){ "ADC", 2, 4, false, addr_zero_page_x, op_ADC };
  opcodes[0x6D] = (sb_6502_opcode_t){ "ADC", 3, 4, false, addr_absolute, op_ADC };
  opcodes[0x7D] = (sb_6502_opcode_t){ "ADC", 3, 4, true, addr_absolute_x, op_ADC };
  opcodes[0x79] = (sb_6502_opcode_t){ "ADC", 3, 4, true, addr_absolute_y, op_ADC };
  opcodes[0x61] = (sb_6502_opcode_t){ "ADC", 2, 6, false, addr_indexed_indirect, op_ADC };
  opcodes[0x71] = (sb_6502_opcode_t){ "ADC", 2, 5, true, addr_indirect_indexed, op_ADC };

  // SBC: 8 variants
  opcodes[0xE9] = (sb_6502_opcode_t){ "SBC", 2, 2, false, addr_immediate, op_SBC };
  opcodes[0xE5] = (sb_6502_opcode_t){ "SBC", 2, 3, false, addr_zero_page, op_SBC };
  opcodes[0xF5] = (sb_6502_opcode_t){ "SBC", 2, 4, false, addr_zero_page_x, op_SBC };
  opcodes[0xED] = (sb_6502_opcode_t){ "SBC", 3, 4, false, addr_absolute, op_SBC };
  opcodes[0xFD] = (sb_6502_opcode_t){ "SBC", 3, 4, true, addr_absolute_x, op_SBC };
  opcodes[0xF9] = (sb_6502_opcode_t){ "SBC", 3, 4, true, addr_absolute_y, op_SBC };
  opcodes[0xE1] = (sb_6502_opcode_t){ "SBC", 2, 6, false, addr_indexed_indirect, op_SBC };
  opcodes[0xF1] = (sb_6502_opcode_t){ "SBC", 2, 5, true, addr_indirect_indexed, op_SBC };

  // undocumented SBC ( same as $E9 )
  opcodes[0xEB] = (sb_6502_opcode_t){ "SBC", 2, 2, false, addr_immediate, op_SBC };

  // AND: 8 variants
  opcodes[0x29] = (sb_6502_opcode_t){ "AND", 2, 2, false, addr_immediate, op_AND };
  opcodes[0x25] = (sb_6502_opcode_t){ "AND", 2, 3, false, addr_zero_page, op_AND };
  opcodes[0x35] = (sb_6502_opcode_t){ "AND", 2, 4, false, addr_zero_page_x, op_AND };
  opcodes[0x2D] = (sb_6502_opcode_t){ "AND", 3, 4, false, addr_absolute, op_AND };
  opcodes[0x3D] = (sb_6502_opcode_t){ "AND", 3, 4, true, addr_absolute_x, op_AND };
  opcodes[0x39] = (sb_6502_opcode_t){ "AND", 3, 4, true, addr_absolute_y, op_AND };
  opcodes[0x21] = (sb_6502_opcode_t){ "AND", 2, 6, false, addr_indexed_indirect, op_AND };
  opcodes[0x31] = (sb_6502_opcode_t){ "AND", 2, 5, true, addr_indirect_indexed, op_AND };

  // ORA: 8 variants
  opcodes[0x09] = (sb_6502_opcode_t){ "ORA", 2, 2, false, addr_immediate, op_ORA };
  opcodes[0x05] = (sb_6502_opcode_t){ "ORA", 2, 3, false, addr_zero_page, op_ORA };
  opcodes[0x15] = (sb_6502_opcode_t){ "ORA", 2, 4, false, addr_zero_page_x, op_ORA };
  opcodes[0x0D] = (sb_6502_opcode_t){ "ORA", 3, 4, false, addr_absolute, op_ORA };
  opcodes[0x1D] = (sb_6502_opcode_t){ "ORA", 3, 4, true, addr_absolute_x, op_ORA };
  opcodes[0x19] = (sb_6502_opcode_t){ "ORA", 3, 4, true, addr_absolute_y, op_ORA };
  opcodes[0x01] = (sb_6502_opcode_t){ "ORA", 2, 6, false, addr_indexed_indirect, op_ORA };
  opcodes[0x11] = (sb_6502_opcode_t){ "ORA", 2, 5, true, addr_indirect_indexed, op_ORA };

  // EOR: 8 variants
  opcodes[0x49] = (sb_6502_opcode_t){ "EOR", 2, 2, false, addr_immediate, op_EOR };
  opcodes[0x45] = (sb_6502_opcode_t){ "EOR", 2, 3, false, addr_zero_page, op_EOR };
  opcodes[0x55] = (sb_6502_opcode_t){ "EOR", 2, 4, false, addr_zero_page_x, op_EOR };
  opcodes[0x4D] = (sb_6502_opcode_t){ "EOR", 3, 4, false, addr_absolute, op_EOR };
  opcodes[0x5D] = (sb_6502_opcode_t){ "EOR", 3, 4, true, addr_absolute_x, op_EOR };
  opcodes[0x59] = (sb_6502_opcode_t){ "EOR", 3, 4, true, addr_absolute_y, op_EOR };
  opcodes[0x41] = (sb_6502_opcode_t){ "EOR", 2, 6, false, addr_indexed_indirect, op_EOR };
  opcodes[0x51] = (sb_6502_opcode_t){ "EOR", 2, 5, true, addr_indirect_indexed, op_EOR };

  // CMP: 8 variants
  opcodes[0xC9] = (sb_6502_opcode_t){ "CMP", 2, 2, false, addr_immediate, op_CMP };
  opcodes[0xC5] = (sb_6502_opcode_t){ "CMP", 2, 3, false, addr_zero_page, op_CMP };
  opcodes[0xD5] = (sb_6502_opcode_t){ "CMP", 2, 4, false, addr_zero_page_x, op_CMP };
  opcodes[0xCD] = (sb_6502_opcode_t){ "CMP", 3, 4, false, addr_absolute, op_CMP };
  opcodes[0xDD] = (sb_6502_opcode_t){ "CMP", 3, 4, true, addr_absolute_x, op_CMP };
  opcodes[0xD9] = (sb_6502_opcode_t){ "CMP", 3, 4, true, addr_absolute_y, op_CMP };
  opcodes[0xC1] = (sb_6502_opcode_t){ "CMP", 2, 6, false, addr_indexed_indirect, op_CMP };
  opcodes[0xD1] = (sb_6502_opcode_t){ "CMP", 2, 5, true, addr_indirect_indexed, op_CMP };

  // CPX: 3 variants
  opcodes[0xE0] = (sb_6502_opcode_t){ "CPX", 2, 2, false, addr_immediate, op_CPX };
  opcodes[0xE4] = (sb_6502_opcode_t){ "CPX", 2, 3, false, addr_zero_page, op_CPX };
  opcodes[0xEC] = (sb_6502_opcode_t){ "CPX", 3, 4, false, addr_absolute, op_CPX };

  // CPY: 3 variants
  opcodes[0xC0] = (sb_6502_opcode_t){ "CPY", 2, 2, false, addr_immediate, op_CPY };
  opcodes[0xC4] = (sb_6502_opcode_t){ "CPY", 2, 3, false, addr_zero_page, op_CPY };
  opcodes[0xCC] = (sb_6502_opcode_t){ "CPY", 3, 4, false, addr_absolute, op_CPY };

  // BIT: 2 variants
  opcodes[0x24] = (sb_6502_opcode_t){ "BIT", 2, 3, false, addr_zero_page, op_BIT };
  opcodes[0x2C] = (sb_6502_opcode_t){ "BIT", 3, 4, false, addr_absolute, op_BIT };

  // ASL: 5 variants
  opcodes[0x0A] = (sb_6502_opcode_t){ "ASL", 1, 2, false, addr_accumulator, op_ASL };
  opcodes[0x06] = (sb_6502_opcode_t){ "ASL", 2, 5, false, addr_zero_page, op_ASL };
  opcodes[0x16] = (sb_6502_opcode_t){ "ASL", 2, 6, false, addr_zero_page_x, op_ASL };
  opcodes[0x0E] = (sb_6502_opcode_t){ "ASL", 3, 6, false, addr_absolute, op_ASL };
  opcodes[0x1E] = (sb_6502_opcode_t){ "ASL", 3, 7, false, addr_absolute_x, op_ASL };

  // LSR: 5 variants
  opcodes[0x4A] = (sb_6502_opcode_t){ "LSR", 1, 2, false, addr_accumulator, op_LSR };
  opcodes[0x46] = (sb_6502_opcode_t){ "LSR", 2, 5, false, addr_zero_page, op_LSR };
  opcodes[0x56] = (sb_6502_opcode_t){ "LSR", 2, 6, false, addr_zero_page_x, op_LSR };
  opcodes[0x4E] = (sb_6502_opcode_t){ "LSR", 3, 6, false, addr_absolute, op_LSR };
  opcodes[0x5E] = (sb_6502_opcode_t){ "LSR", 3, 7, false, addr_absolute_x, op_LSR };

  // ROL: 5 variants
  opcodes[0x2A] = (sb_6502_opcode_t){ "ROL", 1, 2, false, addr_accumulator, op_ROL };
  opcodes[0x26] = (sb_6502_opcode_t){ "ROL", 2, 5, false, addr_zero_page, op_ROL };
  opcodes[0x36] = (sb_6502_opcode_t){ "ROL", 2, 6, false, addr_zero_page_x, op_ROL };
  opcodes[0x2E] = (sb_6502_opcode_t){ "ROL", 3, 6, false, addr_absolute, op_ROL };
  opcodes[0x3E] = (sb_6502_opcode_t){ "ROL", 3, 7, false, addr_absolute_x, op_ROL };

  // ROR: 5 variants
  opcodes[0x6A] = (sb_6502_opcode_t){ "ROR", 1, 2, false, addr_accumulator, op_ROR };
  opcodes[0x66] = (sb_6502_opcode_t){ "ROR", 2, 5, false, addr_zero_page, op_ROR };
  opcodes[0x76] = (sb_6502_opcode_t){ "ROR", 2, 6, false, addr_zero_page_x, op_ROR };
  opcodes[0x6E] = (sb_6502_opcode_t){ "ROR", 3, 6, false, addr_absolute, op_ROR };
  opcodes[0x7E] = (sb_6502_opcode_t){ "ROR", 3, 7, false, addr_absolute_x, op_ROR };

  // INC: 4 variants
  opcodes[0xE6] = (sb_6502_opcode_t){ "INC", 2, 5, false, addr_zero_page, op_INC };
  opcodes[0xF6] = (sb_6502_opcode_t){ "INC", 2, 6, false, addr_zero_page_x, op_INC };
  opcodes[0xEE] = (sb_6502_opcode_t){ "INC", 3, 6, false, addr_absolute, op_INC };
  opcodes[0xFE] = (sb_6502_opcode_t){ "INC", 3, 7, false, addr_absolute_x, op_INC };

  // DEC: 4 variants
  opcodes[0xC6] = (sb_6502_opcode_t){ "DEC", 2, 5, false, addr_zero_page, op_DEC };
  opcodes[0xD6] = (sb_6502_opcode_t){ "DEC", 2, 6, false, addr_zero_page_x, op_DEC };
  opcodes[0xCE] = (sb_6502_opcode_t){ "DEC", 3, 6, false, addr_absolute, op_DEC };
  opcodes[0xDE] = (sb_6502_opcode_t){ "DEC", 3, 7, false, addr_absolute_x, op_DEC };

  // Implied / register instructions (all 1 byte, implied)
  opcodes[0x8A] = (sb_6502_opcode_t){ "TXA", 1, 2, false, addr_implied, op_TXA };
  opcodes[0x9A] = (sb_6502_opcode_t){ "TXS", 1, 2, false, addr_implied, op_TXS };
  opcodes[0x98] = (sb_6502_opcode_t){ "TYA", 1, 2, false, addr_implied, op_TYA };
  opcodes[0xAA] = (sb_6502_opcode_t){ "TAX", 1, 2, false, addr_implied, op_TAX };
  opcodes[0xA8] = (sb_6502_opcode_t){ "TAY", 1, 2, false, addr_implied, op_TAY };
  opcodes[0xBA] = (sb_6502_opcode_t){ "TSX", 1, 2, false, addr_implied, op_TSX };
  opcodes[0xE8] = (sb_6502_opcode_t){ "INX", 1, 2, false, addr_implied, op_INX };
  opcodes[0xC8] = (sb_6502_opcode_t){ "INY", 1, 2, false, addr_implied, op_INY };
  opcodes[0xCA] = (sb_6502_opcode_t){ "DEX", 1, 2, false, addr_implied, op_DEX };
  opcodes[0x88] = (sb_6502_opcode_t){ "DEY", 1, 2, false, addr_implied, op_DEY };

  // Stack instructions
  opcodes[0x48] = (sb_6502_opcode_t){ "PHA", 1, 3, false, addr_implied, op_PHA };
  opcodes[0x68] = (sb_6502_opcode_t){ "PLA", 1, 4, false, addr_implied, op_PLA };
  opcodes[0x08] = (sb_6502_opcode_t){ "PHP", 1, 3, false, addr_implied, op_PHP };
  opcodes[0x28] = (sb_6502_opcode_t){ "PLP", 1, 4, false, addr_implied, op_PLP };

  // Jumps
  opcodes[0x4C] = (sb_6502_opcode_t){ "JMP", 3, 3, false, addr_absolute, op_JMP };
  opcodes[0x6C] = (sb_6502_opcode_t){ "JMP", 3, 5, false, addr_indirect, op_JMP };
  opcodes[0x20] = (sb_6502_opcode_t){ "JSR", 3, 6, false, addr_absolute, op_JSR };
  opcodes[0x60] = (sb_6502_opcode_t){ "RTS", 1, 6, false, addr_implied, op_RTS };
  opcodes[0x40] = (sb_6502_opcode_t){ "RTI", 1, 6, false, addr_implied, op_RTI };

  // Branches
  opcodes[0x90] = (sb_6502_opcode_t){ "BCC", 2, 2, true, addr_relative, op_BCC };
  opcodes[0xB0] = (sb_6502_opcode_t){ "BCS", 2, 2, true, addr_relative, op_BCS };
  opcodes[0xF0] = (sb_6502_opcode_t){ "BEQ", 2, 2, true, addr_relative, op_BEQ };
  opcodes[0xD0] = (sb_6502_opcode_t){ "BNE", 2, 2, true, addr_relative, op_BNE };
  opcodes[0x30] = (sb_6502_opcode_t){ "BMI", 2, 2, true, addr_relative, op_BMI };
  opcodes[0x10] = (sb_6502_opcode_t){ "BPL", 2, 2, true, addr_relative, op_BPL };
  opcodes[0x50] = (sb_6502_opcode_t){ "BVC", 2, 2, true, addr_relative, op_BVC };
  opcodes[0x70] = (sb_6502_opcode_t){ "BVS", 2, 2, true, addr_relative, op_BVS };

  // Status flag instructions
  opcodes[0x18] = (sb_6502_opcode_t){ "CLC", 1, 2, false, addr_implied, op_CLC };
  opcodes[0x38] = (sb_6502_opcode_t){ "SEC", 1, 2, false, addr_implied, op_SEC };
  opcodes[0x58] = (sb_6502_opcode_t){ "CLI", 1, 2, false, addr_implied, op_CLI };
  opcodes[0x78] = (sb_6502_opcode_t){ "SEI", 1, 2, false, addr_implied, op_SEI };
  opcodes[0xB8] = (sb_6502_opcode_t){ "CLV", 1, 2, false, addr_implied, op_CLV };
  opcodes[0xD8] = (sb_6502_opcode_t){ "CLD", 1, 2, false, addr_implied, op_CLD };
  opcodes[0xF8] = (sb_6502_opcode_t){ "SED", 1, 2, false, addr_implied, op_SED };

  // NOP variants (the official ones)
  opcodes[0xEA] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };

  // 1-byte NOPs
  opcodes[0x1A] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };
  opcodes[0x3A] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };
  opcodes[0x5A] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };
  opcodes[0x7A] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };
  opcodes[0xDA] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };
  opcodes[0xFA] = (sb_6502_opcode_t){ "NOP", 1, 2, false, addr_implied, op_NOP };

  // 2-byte NOPs (immediate — reads 1 byte and ignores it)
  opcodes[0xE2] = (sb_6502_opcode_t){ "NOP", 2, 2, false, addr_immediate, op_NOP };
  opcodes[0x04] = (sb_6502_opcode_t){ "NOP", 2, 3, false, addr_immediate, op_NOP };
  opcodes[0x44] = (sb_6502_opcode_t){ "NOP", 2, 3, false, addr_immediate, op_NOP };
  opcodes[0x64] = (sb_6502_opcode_t){ "NOP", 2, 3, false, addr_immediate, op_NOP };
  opcodes[0x14] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0x34] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0x54] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0x74] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0xD4] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0xF4] = (sb_6502_opcode_t){ "NOP", 2, 4, false, addr_immediate, op_NOP };
  opcodes[0x80] = (sb_6502_opcode_t){ "NOP", 2, 2, false, addr_immediate, op_NOP };
  opcodes[0x82] = (sb_6502_opcode_t){ "NOP", 2, 2, false, addr_immediate, op_NOP };
  opcodes[0x89] = (sb_6502_opcode_t){ "NOP", 2, 2, false, addr_immediate, op_NOP };
  opcodes[0xC2] = (sb_6502_opcode_t){ "NOP", 2, 2, false, addr_immediate, op_NOP };

  // 3-byte NOP (absolute)
  opcodes[0x0C] = (sb_6502_opcode_t){ "NOP", 3, 4, false, addr_absolute, op_NOP };

  // 3-byte NOP (absolute.x)
  opcodes[0x1C] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  opcodes[0x3C] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  opcodes[0x5C] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  opcodes[0x7C] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  opcodes[0xDC] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  opcodes[0xFC] = (sb_6502_opcode_t){ "NOP", 3, 4, true, addr_absolute_x, op_NOP };
  // BRK
  opcodes[0x00] = (sb_6502_opcode_t){ "BRK", 2, 7, false, addr_implied, op_BRK };

  // Illegal opcodes (used by games / nestest)
  opcodes[0xA7] = (sb_6502_opcode_t){ "LAX", 2, 3, false, addr_zero_page, op_LAX };
  opcodes[0xB7] = (sb_6502_opcode_t){ "LAX", 2, 4, false, addr_zero_page_y, op_LAX };
  opcodes[0xAF] = (sb_6502_opcode_t){ "LAX", 3, 4, false, addr_absolute, op_LAX };
  opcodes[0xBF] = (sb_6502_opcode_t){ "LAX", 3, 4, true, addr_absolute_y, op_LAX };
  opcodes[0xA3] = (sb_6502_opcode_t){ "LAX", 2, 6, false, addr_indexed_indirect, op_LAX };
  opcodes[0xB3] = (sb_6502_opcode_t){ "LAX", 2, 5, true, addr_indirect_indexed, op_LAX };

  opcodes[0x87] = (sb_6502_opcode_t){ "SAX", 2, 3, false, addr_zero_page, op_SAX };
  opcodes[0x97] = (sb_6502_opcode_t){ "SAX", 2, 4, false, addr_zero_page_y, op_SAX };
  opcodes[0x8F] = (sb_6502_opcode_t){ "SAX", 3, 4, false, addr_absolute, op_SAX };
  opcodes[0x83] = (sb_6502_opcode_t){ "SAX", 2, 6, false, addr_indexed_indirect, op_SAX };

  opcodes[0xC7] = (sb_6502_opcode_t){ "DCP", 2, 5, false, addr_zero_page, op_DCP };
  opcodes[0xD7] = (sb_6502_opcode_t){ "DCP", 2, 6, false, addr_zero_page_x, op_DCP };
  opcodes[0xCF] = (sb_6502_opcode_t){ "DCP", 3, 6, false, addr_absolute, op_DCP };
  opcodes[0xDF] = (sb_6502_opcode_t){ "DCP", 3, 7, false, addr_absolute_x, op_DCP };
  opcodes[0xDB] = (sb_6502_opcode_t){ "DCP", 3, 7, false, addr_absolute_y, op_DCP };
  opcodes[0xC3] = (sb_6502_opcode_t){ "DCP", 2, 8, false, addr_indexed_indirect, op_DCP };
  opcodes[0xD3] = (sb_6502_opcode_t){ "DCP", 2, 8, false, addr_indirect_indexed, op_DCP };

  opcodes[0xE7] = (sb_6502_opcode_t){ "ISB", 2, 5, false, addr_zero_page, op_ISB };
  opcodes[0xF7] = (sb_6502_opcode_t){ "ISB", 2, 6, false, addr_zero_page_x, op_ISB };
  opcodes[0xEF] = (sb_6502_opcode_t){ "ISB", 3, 6, false, addr_absolute, op_ISB };
  opcodes[0xFF] = (sb_6502_opcode_t){ "ISB", 3, 7, false, addr_absolute_x, op_ISB };
  opcodes[0xFB] = (sb_6502_opcode_t){ "ISB", 3, 7, false, addr_absolute_y, op_ISB };
  opcodes[0xE3] = (sb_6502_opcode_t){ "ISB", 2, 8, false, addr_indexed_indirect, op_ISB };
  opcodes[0xF3] = (sb_6502_opcode_t){ "ISB", 2, 8, false, addr_indirect_indexed, op_ISB };

  opcodes[0x07] = (sb_6502_opcode_t){ "SLO", 2, 5, false, addr_zero_page, op_SLO };
  opcodes[0x17] = (sb_6502_opcode_t){ "SLO", 2, 6, false, addr_zero_page_x, op_SLO };
  opcodes[0x0F] = (sb_6502_opcode_t){ "SLO", 3, 6, false, addr_absolute, op_SLO };
  opcodes[0x1F] = (sb_6502_opcode_t){ "SLO", 3, 7, false, addr_absolute_x, op_SLO };
  opcodes[0x1B] = (sb_6502_opcode_t){ "SLO", 3, 7, false, addr_absolute_y, op_SLO };
  opcodes[0x03] = (sb_6502_opcode_t){ "SLO", 2, 8, false, addr_indexed_indirect, op_SLO };
  opcodes[0x13] = (sb_6502_opcode_t){ "SLO", 2, 8, false, addr_indirect_indexed, op_SLO };

  opcodes[0x27] = (sb_6502_opcode_t){ "RLA", 2, 5, false, addr_zero_page, op_RLA };
  opcodes[0x37] = (sb_6502_opcode_t){ "RLA", 2, 6, false, addr_zero_page_x, op_RLA };
  opcodes[0x2F] = (sb_6502_opcode_t){ "RLA", 3, 6, false, addr_absolute, op_RLA };
  opcodes[0x3F] = (sb_6502_opcode_t){ "RLA", 3, 7, false, addr_absolute_x, op_RLA };
  opcodes[0x3B] = (sb_6502_opcode_t){ "RLA", 3, 7, false, addr_absolute_y, op_RLA };
  opcodes[0x23] = (sb_6502_opcode_t){ "RLA", 2, 8, false, addr_indexed_indirect, op_RLA };
  opcodes[0x33] = (sb_6502_opcode_t){ "RLA", 2, 8, false, addr_indirect_indexed, op_RLA };

  opcodes[0x47] = (sb_6502_opcode_t){ "SRE", 2, 5, false, addr_zero_page, op_SRE };
  opcodes[0x57] = (sb_6502_opcode_t){ "SRE", 2, 6, false, addr_zero_page_x, op_SRE };
  opcodes[0x4F] = (sb_6502_opcode_t){ "SRE", 3, 6, false, addr_absolute, op_SRE };
  opcodes[0x5F] = (sb_6502_opcode_t){ "SRE", 3, 7, false, addr_absolute_x, op_SRE };
  opcodes[0x5B] = (sb_6502_opcode_t){ "SRE", 3, 7, false, addr_absolute_y, op_SRE };
  opcodes[0x43] = (sb_6502_opcode_t){ "SRE", 2, 8, false, addr_indexed_indirect, op_SRE };
  opcodes[0x53] = (sb_6502_opcode_t){ "SRE", 2, 8, false, addr_indirect_indexed, op_SRE };

  opcodes[0x67] = (sb_6502_opcode_t){ "RRA", 2, 5, false, addr_zero_page, op_RRA };
  opcodes[0x77] = (sb_6502_opcode_t){ "RRA", 2, 6, false, addr_zero_page_x, op_RRA };
  opcodes[0x6F] = (sb_6502_opcode_t){ "RRA", 3, 6, false, addr_absolute, op_RRA };
  opcodes[0x7F] = (sb_6502_opcode_t){ "RRA", 3, 7, false, addr_absolute_x, op_RRA };
  opcodes[0x7B] = (sb_6502_opcode_t){ "RRA", 3, 7, false, addr_absolute_y, op_RRA };
  opcodes[0x63] = (sb_6502_opcode_t){ "RRA", 2, 8, false, addr_indexed_indirect, op_RRA };
  opcodes[0x73] = (sb_6502_opcode_t){ "RRA", 2, 8, false, addr_indirect_indexed, op_RRA };
  opcodes[0x0B] = (sb_6502_opcode_t){ "ANC", 2, 2, false, addr_immediate, op_ANC };
  opcodes[0x2B] = (sb_6502_opcode_t){ "ANC", 2, 2, false, addr_immediate, op_ANC };
  opcodes[0x4B] = (sb_6502_opcode_t){ "ALR", 2, 2, false, addr_immediate, op_ALR };
  opcodes[0x6B] = (sb_6502_opcode_t){ "ARR", 2, 2, false, addr_immediate, op_ARR };
  opcodes[0xAB] = (sb_6502_opcode_t){ "LXA", 2, 2, false, addr_immediate, op_LXA };
  opcodes[0xCB] = (sb_6502_opcode_t){ "SBX", 2, 2, false, addr_immediate, op_SBX };

  // Safety fill: every opcode needs a cycle handler.
  // Individually-assigned opcodes above don't set the cycle field
  // (C99 partial-zero-init leaves it NULL). Fill the illegal stub
  // so sb_6502_cycle always has a valid dispatch target.
  for (int i = 0; i < 256; i++) {
    if (opcodes[i].cycle == 0)
      opcodes[i].cycle = cycle_illegal;
  }

  // Phase 3 Step 1: Assign cycle handlers for Implied/Register instructions.
  // These are 2-cycle instructions: opcode fetch + internal register op.
  static const uint8_t implied_list[] = {
    0xAA, 0x8A, 0xA8, 0x98, 0xBA, 0x9A,       // TAX, TXA, TAY, TYA, TSX, TXS
    0xCA, 0x88, 0xE8, 0xC8,                   // DEX, DEY, INX, INY
    0x18, 0x38, 0x58, 0x78, 0xB8, 0xD8, 0xF8, // CLC, SEC, CLI, SEI, CLV, CLD, SED
    0xEA,                                     // NOP
    0x1A, 0x3A, 0x5A, 0x7A, 0xDA, 0xFA,       // extra NOPs (implied)
    0x0A, 0x4A, 0x2A, 0x6A,                   // ASL A, LSR A, ROL A, ROR A
  };
  for (int i = 0; i < (int)(sizeof(implied_list)); i++)
    opcodes[implied_list[i]].cycle = cycle_implied;

  // Phase 3 Step 2: Read Immediate instructions (2 cycles).
  static const uint8_t imm_list[] = {
    0xA9, 0xA2, 0xA0,                   // LDA, LDX, LDY
    0x09, 0x29, 0x49,                   // ORA, AND, EOR
    0x69, 0xE9,                         // ADC, SBC
    0xC9, 0xE0, 0xC0,                   // CMP, CPX, CPY
    0xEB,                               // SBC (illegal)
    0x80, 0x82, 0x89, 0xC2, 0xE2,       // immediate NOPs (illegal)
    0x0B, 0x2B, 0x4B, 0x6B, 0xAB, 0xCB, // ANC, ANC, ALR, ARR, LXA, SBX (illegal)
  };
  for (int i = 0; i < (int)(sizeof(imm_list)); i++)
    opcodes[imm_list[i]].cycle = cycle_read_immediate;

  // Phase 3 Step 3: Read Zero Page (3 cycles).
  // Phase 3 Step 4: Read Absolute (4 cycles).
  // Phase 3 Step 5: Read Zero Page,X (4 cycles).
  // Phase 3 Step 6: Read Absolute,X (4+1 cycles).
  static const uint8_t zp_list[] = {
    0xA5, 0xA6, 0xA4, // LDA, LDX, LDY zp
    0x25, 0x05, 0x45, // AND, ORA, EOR zp
    0x65, 0xE5,       // ADC, SBC zp
    0xC5, 0xE4, 0xC4, // CMP, CPX, CPY zp
    0x24,             // BIT zp
    0xA7,             // LAX zp (illegal)
    0x04, 0x44, 0x64, // zero-page NOPs (illegal)
  };
  static const uint8_t abs_list[] = {
    0xAD, 0xAE, 0xAC, // LDA, LDX, LDY abs
    0x2D, 0x0D, 0x4D, // AND, ORA, EOR abs
    0x6D, 0xED,       // ADC, SBC abs
    0xCD, 0xEC, 0xCC, // CMP, CPX, CPY abs
    0x2C,             // BIT abs
    0xAF,             // LAX abs (illegal)
    0x0C,             // abs NOP (illegal)
  };
  static const uint8_t zpx_list[] = {
    0xB5,                               // LDA zp,X
    0xB4,                               // LDY zp,X
    0x35, 0x15, 0x55,                   // AND, ORA, EOR zp,X
    0x75, 0xF5,                         // ADC, SBC zp,X
    0xD5,                               // CMP zp,X
    0xB6,                               // LDX zp,Y
    0x14, 0x34, 0x54, 0x74, 0xD4, 0xF4, // zp,X NOPs (illegal)
  };
  static const uint8_t absx_list[] = {
    0xBD, 0xBC, 0xB9,                   // LDA abs,X, LDY abs,X, LDA abs,Y
    0xBE,                               // LDX abs,Y
    0x3D, 0x1D, 0x5D,                   // AND abs,X, ORA abs,X, EOR abs,X
    0x39, 0x19, 0x59,                   // AND abs,Y, ORA abs,Y, EOR abs,Y
    0x7D, 0xFD,                         // ADC abs,X, SBC abs,X
    0x79, 0xF9,                         // ADC abs,Y, SBC abs,Y
    0xDD, 0xD9,                         // CMP abs,X, CMP abs,Y
    0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC, // abs,X NOPs (illegal)
  };
  for (int i = 0; i < (int)(sizeof(zp_list)); i++)
    opcodes[zp_list[i]].cycle = cycle_read_zp;
  for (int i = 0; i < (int)(sizeof(abs_list)); i++)
    opcodes[abs_list[i]].cycle = cycle_read_absolute;
  for (int i = 0; i < (int)(sizeof(zpx_list)); i++)
    opcodes[zpx_list[i]].cycle = cycle_read_zpx;
  for (int i = 0; i < (int)(sizeof(absx_list)); i++)
    opcodes[absx_list[i]].cycle = cycle_read_absx;

  // Phase 3 Step 7: Write Zero Page + Absolute (3/4 cycles).
  static const uint8_t write_zp_list[] = {
    0x85,
    0x86,
    0x84, // STA, STX, STY zp
    0x87, // SAX zp (illegal)
  };
  static const uint8_t write_abs_list[] = {
    0x8D,
    0x8E,
    0x8C, // STA, STX, STY abs
    0x8F, // SAX abs (illegal)
  };
  for (int i = 0; i < (int)(sizeof(write_zp_list)); i++)
    opcodes[write_zp_list[i]].cycle = cycle_write_zp;
  for (int i = 0; i < (int)(sizeof(write_abs_list)); i++)
    opcodes[write_abs_list[i]].cycle = cycle_write_absolute;

  // Phase 3 Step 8: Branch (2/3/4 cycles).
  static const uint8_t branch_list[] = {
    0x90, 0xB0, 0xF0, 0xD0, // BCC, BCS, BEQ, BNE
    0x30, 0x10, 0x50, 0x70, // BMI, BPL, BVC, BVS
  };
  for (int i = 0; i < (int)(sizeof(branch_list)); i++)
    opcodes[branch_list[i]].cycle = cycle_branch;

  // Phase 3 Step 9: JMP, JSR, RTS, RTI.
  opcodes[0x4C].cycle = cycle_jmp_abs; // JMP abs
  opcodes[0x6C].cycle = cycle_jmp_ind; // JMP ind
  opcodes[0x20].cycle = cycle_jsr;
  opcodes[0x60].cycle = cycle_rts;
  opcodes[0x40].cycle = cycle_rti;
  opcodes[0x00].cycle = cycle_brk; // BRK

  // Phase 3 Step 10: RMW Zero Page + Absolute (5/6 cycles).
  static const uint8_t rmw_zp_list[] = {
    0x06, 0x46, 0x26, 0x66,             // ASL, LSR, ROL, ROR zp
    0xE6, 0xC6,                         // INC, DEC zp
    0x07, 0x27, 0x47, 0x67, 0xC7, 0xE7, // SLO, RLA, SRE, RRA, DCP, ISB zp (illegal)
  };
  static const uint8_t rmw_abs_list[] = {
    0x0E, 0x4E, 0x2E, 0x6E,             // ASL, LSR, ROL, ROR abs
    0xEE, 0xCE,                         // INC, DEC abs
    0x0F, 0x2F, 0x4F, 0x6F, 0xCF, 0xEF, // SLO, RLA, SRE, RRA, DCP, ISB abs (illegal)
  };
  for (int i = 0; i < (int)(sizeof(rmw_zp_list)); i++)
    opcodes[rmw_zp_list[i]].cycle = cycle_rmw_zp;
  for (int i = 0; i < (int)(sizeof(rmw_abs_list)); i++)
    opcodes[rmw_abs_list[i]].cycle = cycle_rmw_abs;

  // Phase 3 Step 11: Stack push/pull.
  opcodes[0x48].cycle = cycle_push; // PHA
  opcodes[0x08].cycle = cycle_push; // PHP
  opcodes[0x68].cycle = cycle_pull; // PLA
  opcodes[0x28].cycle = cycle_pull; // PLP

  // Phase 3 Step 12: BRK (handled by fetch_opcode as interrupt).
  // Phase 3 Step 13: Write indexed/indirect and remaining indexed reads.
  static const uint8_t write_zpx_list[] = { 0x95, 0x94, 0x96, 0x97 };
  static const uint8_t write_absx_list[] = { 0x9D };
  static const uint8_t write_absy_list[] = { 0x99 };
  static const uint8_t write_idrx_list[] = { 0x81, 0x83 };
  static const uint8_t write_idry_list[] = { 0x91 };
  static const uint8_t read_absy_list[] = { 0xB9, 0xBF, 0x39, 0x19, 0x59, 0x79, 0xF9, 0xD9, 0xBE };
  static const uint8_t read_idrx_list[] = { 0xA1, 0xA3, 0x21, 0x01, 0x41, 0x61, 0xE1, 0xC1 };
  static const uint8_t read_idry_list[] = { 0xB1, 0xB3, 0x31, 0x11, 0x51, 0x71, 0xF1, 0xD1 };
  static const uint8_t rmw_zpx_list[] = { 0x16, 0x56, 0x36, 0x76, 0xF6, 0xD6,
                                          0x17, 0x57, 0x37, 0x77, 0xD7, 0xF7 };
  static const uint8_t rmw_absx_list[] = { 0x1E, 0x5E, 0x3E, 0x7E, 0xFE, 0xDE,
                                           0x1F, 0x5F, 0x3F, 0x7F, 0xDF, 0xFF };
  static const uint8_t rmw_absy_list[] = { 0x1B, 0x5B, 0x3B, 0x7B, 0xDB, 0xFB };
  static const uint8_t rmw_idrx_list[] = { 0x03, 0x43, 0x23, 0x63, 0xC3, 0xE3 };
  static const uint8_t rmw_idry_list[] = { 0x13, 0x53, 0x33, 0x73, 0xD3, 0xF3 };
  // NOP abs,X variants (harmless reads)
  static const uint8_t nop_absx_list[] = { 0x1C, 0x3C, 0x5C, 0x7C, 0xDC, 0xFC };
  // Remaining LAX variants (illegal reads)
  static const uint8_t lax_list[] = { 0xB7 };

  for (int i = 0; i < (int)(sizeof(write_zpx_list)); i++)
    opcodes[write_zpx_list[i]].cycle = cycle_write_zpx;
  for (int i = 0; i < (int)(sizeof(write_absx_list)); i++)
    opcodes[write_absx_list[i]].cycle = cycle_write_absx;
  for (int i = 0; i < (int)(sizeof(write_absy_list)); i++)
    opcodes[write_absy_list[i]].cycle = cycle_write_absy;
  for (int i = 0; i < (int)(sizeof(write_idrx_list)); i++)
    opcodes[write_idrx_list[i]].cycle = cycle_write_idrx;
  for (int i = 0; i < (int)(sizeof(write_idry_list)); i++)
    opcodes[write_idry_list[i]].cycle = cycle_write_idry;
  for (int i = 0; i < (int)(sizeof(read_absy_list)); i++)
    opcodes[read_absy_list[i]].cycle = cycle_read_absy;
  for (int i = 0; i < (int)(sizeof(read_idrx_list)); i++)
    opcodes[read_idrx_list[i]].cycle = cycle_read_idrx;
  for (int i = 0; i < (int)(sizeof(read_idry_list)); i++)
    opcodes[read_idry_list[i]].cycle = cycle_read_idry;
  for (int i = 0; i < (int)(sizeof(rmw_zpx_list)); i++)
    opcodes[rmw_zpx_list[i]].cycle = cycle_rmw_zpx;
  for (int i = 0; i < (int)(sizeof(rmw_absx_list)); i++)
    opcodes[rmw_absx_list[i]].cycle = cycle_rmw_absx;
  for (int i = 0; i < (int)(sizeof(rmw_absy_list)); i++)
    opcodes[rmw_absy_list[i]].cycle = cycle_rmw_absy;
  for (int i = 0; i < (int)(sizeof(rmw_idrx_list)); i++)
    opcodes[rmw_idrx_list[i]].cycle = cycle_rmw_idrx;
  for (int i = 0; i < (int)(sizeof(rmw_idry_list)); i++)
    opcodes[rmw_idry_list[i]].cycle = cycle_rmw_idry;
  for (int i = 0; i < (int)(sizeof(nop_absx_list)); i++)
    opcodes[nop_absx_list[i]].cycle = cycle_read_absx;
  for (int i = 0; i < (int)(sizeof(lax_list)); i++)
    opcodes[lax_list[i]].cycle = cycle_read_zpx; // LAX zp,Y uses zp,X-like timing
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
    cpu->p = sb_bus_read(bus, 0x0100 | cpu->s);
    cpu->p &= ~SB_6502_BRK;
    cpu->p |= SB_6502_UNUSED;
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
      cpu->p = sb_bus_read(bus, 0x0100 | cpu->s);
      cpu->p &= ~SB_6502_BRK;
      cpu->p |= SB_6502_UNUSED;
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
  if (cpu->nmi_pending) {
    cpu->nmi_pending = false;
    cpu->opcode = 0x100; // NMI sentinel (not a real opcode)
    cpu->phase = 0;
    return SB_OK;
  }

  // IRQ only fires if the interrupt disable flag is clear.
  if (cpu->irq_pending && !(cpu->p & SB_6502_INTERRUPT)) {
    cpu->irq_pending = false;
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
