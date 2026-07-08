#ifndef SB_NES_H
#define SB_NES_H

#include <stdint.h>
#include <stdbool.h>
#include "sb_6502/sb_6502.h"
#include "sb_bus/sb_bus.h"
#include "sb_cartridge/sb_cartridge.h"

typedef struct {
    sb_6502_t      cpu;
    sb_bus_t       bus;
    sb_cartridge_t cartridge;

    // Controller state (Phase 2: direct read)
    uint8_t controller_mask;
} sb_nes_t;

// Initialize all components, wire pointers, init opcode table
void sb_nes_init(sb_nes_t* nes);

// Load ROM from file path
// Returns true on success, prints error to stderr on failure.
bool sb_nes_load_rom(sb_nes_t* nes, const char* path);

// Run one frame of emulation (~29780 CPU cycles)
// Phase 2: CPU only. Phase 3+: CPU + PPU + APU.
void sb_nes_frame(sb_nes_t* nes);

// Set controller buttons for player 0
void sb_nes_set_buttons(sb_nes_t* nes, uint8_t mask);

#endif
