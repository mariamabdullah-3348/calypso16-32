#include "decode.h"

#include <cstdint>

// private static helper
int64_t Decoder::sign_extend(uint32_t value, int bits) {
    const uint32_t mask = 1u << (bits - 1);
    const uint64_t extended = (static_cast<uint64_t>(value) ^ mask) - mask;
    return static_cast<int64_t>(extended);
}

Decoded Decoder::decode(uint32_t raw, int size) {
    Decoded d{};

    if (size == 2) {
        // --------------------------------------------------------
        // 16-bit formats: prefix bits [15:14]
        // CS = 00 | funct4 | rd/rs1 | rs2
        // CI = 01 | funct3 | rd/rs1 | imm6
        // --------------------------------------------------------
        const uint16_t inst = static_cast<uint16_t>(raw & 0xFFFFu);
        const int prefix = (inst >> 14) & 0x3;

        if (prefix == 0b00) {
            d.format = FMT_CS;
            d.funct  = (inst >> 10) & 0xF;
            d.rd     = (inst >> 5) & 0x1F;
            d.rs1    = d.rd;                    // destructive addressing
            d.rs2    = inst & 0x1F;
            d.illegal = (d.funct > ALU_REM);
        } else if (prefix == 0b01) {
            d.format = FMT_CI;
            d.funct  = (inst >> 11) & 0x7;
            d.rd     = (inst >> 6) & 0x1F;
            d.rs1    = d.rd;                    // destructive addressing
            d.imm    = sign_extend(inst & 0x3Fu, 6);
            d.illegal = (d.funct > ALU_SLTI);
        } else {
            // Fetch only selects 16-bit instructions whose MSB is 0,
            // making prefixes 10/11 unreachable under the architecture.
            d.illegal = true;
        }
    } else if (size == 4) {
        // --------------------------------------------------------
        // 32-bit formats: prefix bits [31:30]
        // R = 10 | funct4 | rd | rs1 | rs2 | reserved
        // I = 11 | funct4 | rd/rs2 | rs1 | imm16
        // --------------------------------------------------------
        const int prefix = (raw >> 30) & 0x3;

        if (prefix == 0b10) {
            d.format = FMT_R;
            d.funct  = (raw >> 26) & 0xF;
            d.rd     = (raw >> 21) & 0x1F;
            d.rs1    = (raw >> 16) & 0x1F;
            d.rs2    = (raw >> 11) & 0x1F;
            d.illegal = (d.funct > ALU_REM);
        } else if (prefix == 0b11) {
            d.format = FMT_I;
            d.funct  = (raw >> 26) & 0xF;
            d.rd     = (raw >> 21) & 0x1F;       // rd, or rs2 for SW/BEQ
            d.rs1    = (raw >> 16) & 0x1F;
            d.imm    = sign_extend(raw & 0xFFFFu, 16);
            d.illegal = (d.funct > OP_JAL);
        } else {
            // Fetch only selects 32-bit instructions whose bit[31] is 1,
            // so 00/01 do not occur here under the architectural fetch rule.
            d.illegal = true;
        }
    } else {
        d.illegal = true;
    }

    return d;
}
