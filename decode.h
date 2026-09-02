#ifndef DECODE_H
#define DECODE_H

#include <cstdint>

// ------------------------------------------------------------
// 4 architectural instruction formats
// 16-bit: CS / CI
// 32-bit: R / I
// ------------------------------------------------------------
enum { FMT_CS = 0, FMT_CI = 1, FMT_R = 2, FMT_I = 3 };

// Register-register funct4 values used by CS and R.
enum {
    ALU_ADD = 0,
    ALU_SUB = 1,
    ALU_AND = 2,
    ALU_OR  = 3,
    ALU_SLT = 4,
    ALU_MUL = 5,
    ALU_DIV = 6,
    ALU_REM = 7
};

// Register-immediate funct3 values for CI and funct4 low values for I.
enum {
    ALU_ADDI = 0,
    ALU_ANDI = 1,
    ALU_ORI  = 2,
    ALU_SLTI = 3
};

// I-format non-ALU operations.
enum {
    OP_LW   = 4,
    OP_SW   = 5,
    OP_BEQ  = 6,
    OP_JAL  = 7
};

// Plain data type — holds all decoded fields of one instruction.
struct Decoded {
    int     format  = -1;
    int     funct   = 0;
    int     rd      = 0;
    int     rs1     = 0;
    int     rs2     = 0;
    int64_t imm     = 0;
    bool    illegal = false;
};

// ============================================================
// Decoder — stateless instruction decoder.
// decode() is a static method so callers do not need an
// instance; Decoder::decode(raw, size) mirrors the old
// free-function interface exactly.
// ============================================================
class Decoder {
public:
    // Decode a raw instruction word of 'size' bytes (2 or 4).
    // Returns a fully-populated Decoded struct.
    static Decoded decode(uint32_t raw, int size);

private:
    // Sign-extend the low 'bits' bits of value.
    static int64_t sign_extend(uint32_t value, int bits);
};

#endif
