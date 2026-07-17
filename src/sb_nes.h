#ifndef SB_NES_H
#define SB_NES_H

#include "sb_6502/sb_6502.h"
#include "sb_bus/sb_bus.h"
#include "sb_cartridge/sb_cartridge.h"
#include "sb_ppu/sb_ppu.h"
#include <stdbool.h>
#include <stdint.h>

// Minimal APU: only frame counter IRQ (no audio).
// Frame counter IRQ fires at cycle 29829 (4-step) or 37281 (5-step).
// 4-step wraps at 29829, 5-step at 37281.
#define SB_APU_4STEP_CYCLES 29829 // CPU cycles per 4-step frame cycle (NTSC)
#define SB_APU_5STEP_CYCLES 37281 // CPU cycles per 5-step frame cycle (NTSC)

typedef struct sb_apu_t {
  bool frame_irq_enabled; // writes to $4017 bit7 = 0 enables frame IRQ
  bool frame_5step;       // $4017 bit7 = 1 selects 5-step mode (no IRQ)
  // frame_irq_pending moved to sb_bus_t (bus->irq_pending)
  int frame_cycles; // cycle counter: counts 0..29828 (4-step) or 0..37280 (5-step)
} sb_apu_t;

typedef struct {
  sb_6502_t cpu;
  sb_bus_t bus;
  sb_cartridge_t cartridge;
  sb_ppu_t ppu;
  sb_apu_t apu;

  // Frontend button mask, converted to NES bit order via reverse_bits()
  uint8_t controller_mask;

  // Cycle-interleave subcycle counter (0,1,2). Every 3 PPU dots = 1 CPU cycle.
  // Persists across frame boundaries for correct mid-instruction resume.
  int cpu_subcycle;
} sb_nes_t;

// Tick the APU frame counter by one CPU cycle.
// Must be called once per CPU cycle (including DMA cycles).
void sb_apu_frame_tick(sb_apu_t* apu, sb_bus_t* bus);

// APU register access (called by bus on $4015/$4017).
// $4015 read:  returns status (bit6 = frame IRQ pending), clears pending IRQ.
// $4017 write: resets frame counter, sets 4/5-step mode, clears IRQ.
uint8_t sb_apu_read(sb_apu_t* apu, sb_bus_t* bus, uint16_t addr);
void sb_apu_write(sb_apu_t* apu, sb_bus_t* bus, uint16_t addr, uint8_t val);

// Initialize all components, wire pointers, init opcode table
void sb_nes_init(sb_nes_t* nes);

// Load ROM from file path
// Returns true on success, prints error to stderr on failure.
bool sb_nes_load_rom(sb_nes_t* nes, const char* path);

// Run one frame of emulation (262 scanlines × 341 dots, cycle-interleaved CPU + PPU + DMA).
// CPU advances 1 cycle per 3 PPU dots via cpu_subcycle counter.
// NMI/IRQ bridged from PPU to CPU; DMA cycle-stolen during active transfers.
void sb_nes_frame(sb_nes_t* nes);

// Set controller buttons for player 0
void sb_nes_set_buttons(sb_nes_t* nes, uint8_t mask);

#endif
