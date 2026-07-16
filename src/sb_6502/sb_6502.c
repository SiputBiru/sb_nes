#include "sb_6502.h"

static sb_6502_opcode_t opcodes[256];

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
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  val++;
  sb_bus_write(bus, cpu->result.addr, val);
  SET_ZN(cpu, val);
}
static void op_DEC(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  val--;
  sb_bus_write(bus, cpu->result.addr, val);
  SET_ZN(cpu, val);
}

// load store
static void op_LDA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a = sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->a);
}
static void op_LDX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->x = sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->x);
}
static void op_LDY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->y = sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->y);
}
static void op_STA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, cpu->result.addr, cpu->a);
}
static void op_STX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, cpu->result.addr, cpu->x);
}
static void op_STY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, cpu->result.addr, cpu->y);
}

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
  // Push status with BRK and UNUSED bits SET
  sb_bus_write(bus, 0x0100 | (uint16_t)cpu->s, cpu->p | SB_6502_BRK | SB_6502_UNUSED);
  cpu->s--;
}
static void op_PLP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->s++;
  cpu->p = sb_bus_read(bus, 0x0100 | (uint16_t)cpu->s);
  // cpu->p &= ~(SB_6502_BRK | SB_6502_UNUSED);
  cpu->p &= ~(SB_6502_BRK);
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
  cpu->a &= sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->a);
}
static void op_ORA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a |= sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->a);
}
static void op_EOR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  cpu->a ^= sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->a);
}
static void op_CMP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  uint16_t diff = (uint16_t)cpu->a - val;
  if (cpu->a >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_CPX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  uint16_t diff = (uint16_t)cpu->x - val;
  if (cpu->x >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_CPY(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  uint16_t diff = (uint16_t)cpu->y - val;
  if (cpu->y >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)diff);
}
static void op_BIT(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
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
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
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
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
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
  if (cpu->result.addr == 0) {
    if (cpu->a & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a <<= 1;
    SET_ZN(cpu, cpu->a);
  } else {
    uint8_t val = sb_bus_read(bus, cpu->result.addr);
    if (val & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    val <<= 1;
    sb_bus_write(bus, cpu->result.addr, val);
    SET_ZN(cpu, val);
  }
}
static void op_LSR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  if (cpu->result.addr == 0) {
    if (cpu->a & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a >>= 1;
    SET_ZN(cpu, cpu->a);
  } else {
    uint8_t val = sb_bus_read(bus, cpu->result.addr);
    if (val & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    val >>= 1;
    sb_bus_write(bus, cpu->result.addr, val);
    SET_ZN(cpu, val);
  }
}
static void op_ROL(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (cpu->result.addr == 0) {
    if (cpu->a & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a = (cpu->a << 1) | old_c;
    SET_ZN(cpu, cpu->a);
  } else {
    uint8_t val = sb_bus_read(bus, cpu->result.addr);
    if (val & 0x80)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    val = (val << 1) | old_c;
    sb_bus_write(bus, cpu->result.addr, val);
    SET_ZN(cpu, val);
  }
}
static void op_ROR(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  if (cpu->result.addr == 0) {
    if (cpu->a & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    cpu->a = (cpu->a >> 1) | (old_c << 7);
    SET_ZN(cpu, cpu->a);
  } else {
    uint8_t val = sb_bus_read(bus, cpu->result.addr);
    if (val & 1)
      cpu->p |= SB_6502_CARRY;
    else
      cpu->p &= ~SB_6502_CARRY;
    val = (val >> 1) | (old_c << 7);
    sb_bus_write(bus, cpu->result.addr, val);
    SET_ZN(cpu, val);
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
  cpu->a = cpu->x = sb_bus_read(bus, cpu->result.addr);
  SET_ZN(cpu, cpu->a);
}
static void op_SAX(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  sb_bus_write(bus, cpu->result.addr, cpu->a & cpu->x);
}
static void op_DCP(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  val--;
  sb_bus_write(bus, cpu->result.addr, val);
  if (cpu->a >= val)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  SET_ZN(cpu, (uint8_t)((uint16_t)cpu->a - val));
}
static void op_ISB(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  val++;
  sb_bus_write(bus, cpu->result.addr, val);
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
static void op_SLO(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  if (val & 0x80)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  val <<= 1;
  sb_bus_write(bus, cpu->result.addr, val);
  cpu->a |= val;
  SET_ZN(cpu, cpu->a);
}
static void op_RLA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  if (val & 0x80)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  val = (val << 1) | old_c;
  sb_bus_write(bus, cpu->result.addr, val);
  cpu->a &= val;
  SET_ZN(cpu, cpu->a);
}
static void op_SRE(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  if (val & 1)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  val >>= 1;
  sb_bus_write(bus, cpu->result.addr, val);
  cpu->a ^= val;
  SET_ZN(cpu, cpu->a);
}
static void op_RRA(sb_6502_t* cpu, sb_bus_t* bus, uint8_t opcode) {
  uint8_t old_c = (cpu->p & SB_6502_CARRY) ? 1 : 0;
  uint8_t val = sb_bus_read(bus, cpu->result.addr);
  if (val & 1)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  val = (val >> 1) | (old_c << 7);
  sb_bus_write(bus, cpu->result.addr, val);
  uint16_t sum = (uint16_t)cpu->a + val + (cpu->p & SB_6502_CARRY);
  if (sum & 0xFF00)
    cpu->p |= SB_6502_CARRY;
  else
    cpu->p &= ~SB_6502_CARRY;
  if (((cpu->a ^ (uint8_t)sum) & (val ^ (uint8_t)sum) & 0x80) != 0)
    cpu->p |= SB_6502_OVERFLOW;
  else
    cpu->p &= ~SB_6502_OVERFLOW;
  cpu->a = (uint8_t)sum;
  SET_ZN(cpu, cpu->a);
}

// Opcode dispatch table initialization
void sb_6502_init_opcodes(void) {

  // default all 256 to NOP (illegal opcodes do nothing)
  for (int i = 0; i < 256; i++) {
    opcodes[i] = (sb_6502_opcode_t){ "XX", 0, 0, false, addr_implied, op_NOP };
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

  // Execute instruction
  info->func(cpu, bus, opcode);

  // Add cycles
  cpu->cycles += info->cycles;
  if (info->page_penalty && cpu->result.page_crossed) {
    cpu->cycles++;
  }
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
