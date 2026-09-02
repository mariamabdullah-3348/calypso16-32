#include "memory.h"

bool Memory::read64(uint64_t addr, int64_t& out) const {
    // Avoid addr + 8 overflow by comparing against the last legal start.
    if (addr > static_cast<uint64_t>(MEM_SIZE - WORD_BYTES)) {
        return false;
    }

    uint64_t value = 0;
    for (int i = 0; i < WORD_BYTES; ++i) {
        value = (value << 8) | cpu_.memory[addr + static_cast<uint64_t>(i)];
    }

    out = static_cast<int64_t>(value);
    return true;
}

bool Memory::write64(uint64_t addr, int64_t value) {
    if (addr > static_cast<uint64_t>(MEM_SIZE - WORD_BYTES)) {
        return false;
    }

    // The architectural layout places program memory at 0x1000..0xFFFF.
    // Stores are intended for the data region 0x0000..0x0FFF so the program
    // cannot overwrite its own instructions.
    if (addr > DATA_END || addr + (WORD_BYTES - 1) > DATA_END) {
        return false;
    }

    uint64_t bits = static_cast<uint64_t>(value);
    for (int i = 0; i < WORD_BYTES; ++i) {
        cpu_.memory[addr + static_cast<uint64_t>(i)] =
            static_cast<uint8_t>(bits >> (8 * (WORD_BYTES - 1 - i)));
    }

    return true;
}
