#include "fetch.h"

uint16_t Fetcher::read16BE(uint64_t addr) const {
    return static_cast<uint16_t>(
        (static_cast<uint16_t>(cpu_.memory[addr])     << 8) |
         static_cast<uint16_t>(cpu_.memory[addr + 1]));
}

bool Fetcher::fetch(uint32_t& raw_inst, int& inst_size) const {
    // Need at least 2 bytes available from PC.
    if (cpu_.PC > static_cast<uint64_t>(MEM_SIZE - 2)) {
        return false;
    }

    const uint16_t first_halfword = read16BE(cpu_.PC);

    // Calypso16/32 encoding rule:
    //   bit[15] of first fetched byte == 0  → 16-bit (CS / CI)
    //   bit[15] of first fetched byte == 1  → 32-bit (R  / I )
    if (first_halfword & 0x8000u) {
        // Need 4 bytes total.
        if (cpu_.PC > static_cast<uint64_t>(MEM_SIZE - 4)) {
            return false;
        }
        const uint16_t second_halfword = read16BE(cpu_.PC + 2);
        raw_inst  = (static_cast<uint32_t>(first_halfword) << 16) |
                     static_cast<uint32_t>(second_halfword);
        inst_size = 4;
    } else {
        raw_inst  = first_halfword;
        inst_size = 2;
    }

    return true;
}
