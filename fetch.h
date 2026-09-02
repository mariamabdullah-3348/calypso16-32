#ifndef FETCH_H
#define FETCH_H

#include <cstdint>
#include "cpu_state.h"

// ============================================================
// Fetcher — reads one instruction from the memory array owned
// by CPUState at the current PC and classifies it as a 16-bit
// or 32-bit instruction using the Calypso16/32 encoding rule:
//   bit[15] == 0  →  16-bit instruction (CS / CI formats)
//   bit[15] == 1  →  32-bit instruction (R  / I  formats)
// Big-Endian byte order is assumed throughout.
// ============================================================
class Fetcher {
public:
    explicit Fetcher(CPUState& cpu) : cpu_(cpu) {}

    // Populates raw_inst and inst_size (2 or 4 bytes).
    // Returns false when the PC is outside the safe fetch window.
    bool fetch(uint32_t& raw_inst, int& inst_size) const;

private:
    // Read two bytes as a Big-Endian 16-bit halfword.
    uint16_t read16BE(uint64_t addr) const;

    CPUState& cpu_;
};

#endif
