#ifndef SB_NES_H
#define SB_NES_H

#include "sb_6502/sb_6502.h"
#include "sb_bus/sb_bus.h"
#include "sb_cartridge/sb_cartridge.h"
#include "sb_ppu/sb_ppu.h"
#include <stdbool.h>
#include <stdint.h>

// Minimal APU: only frame counter IRQ (no audio).
// The frame counter ticks once per CPU cycle and generates IRQ
// at step 3 in 4-step mode (triggered by writing $00 to $4017).
#define SB_APU_FRAME_DIVIDER_NTSC 7457 // CPU cycles per frame counter step

typedef struct sb_apu_t {
  bool frame_irq_enabled; // writes to $4017 bit7 = 0 enables frame IRQ
  bool frame_5step;       // $4017 bit7 = 1 selects 5-step mode (no IRQ)
  bool frame_irq_pending; // IRQ triggered at step 3 (latched until $4015 read)
  int frame_divider;      // counts CPU cycles 0..7456 between steps
  int frame_step;         // current step (0-3 in 4-step, 0-4 in 5-step)
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
void sb_apu_frame_tick(sb_apu_t* apu);

// APU register access (called by bus on $4015/$4017).
// $4015 read:  returns status (bit6 = frame IRQ pending), clears pending IRQ.
// $4017 write: resets frame counter, sets 4/5-step mode, clears IRQ.
uint8_t sb_apu_read(sb_apu_t* apu, uint16_t addr);
void sb_apu_write(sb_apu_t* apu, uint16_t addr, uint8_t val);

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
