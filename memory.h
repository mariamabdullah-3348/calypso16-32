#ifndef MEMORY_H
#define MEMORY_H

#include <cstdint>
#include "cpu_state.h"

// ============================================================
// Memory — performs 64-bit Big-Endian loads and stores into
// the memory array owned by CPUState.
//
// Calypso16/32 LW/SW move one full 64-bit value (8 bytes).
// Big-Endian byte order is used.
// Misaligned addresses are allowed.
// read64  returns false for out-of-range accesses.
// write64 also rejects stores whose byte range would overwrite
//         program memory (any address above DATA_END).
// ============================================================
class Memory {
public:
    explicit Memory(CPUState& cpu) : cpu_(cpu) {}

    // Load 8 bytes Big-Endian from addr into out.
    // Returns false if addr is out of range.
    bool read64(uint64_t addr, int64_t& out) const;

    // Store 8 bytes Big-Endian at addr.
    // Returns false if addr is out of range or touches program memory.
    bool write64(uint64_t addr, int64_t value);

private:
    CPUState& cpu_;
};

#endif
