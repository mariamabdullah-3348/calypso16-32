#include "m_extension.h"

#include <cstdint>
#include <limits>

uint64_t MExtension::MUL(uint64_t rs1, uint64_t rs2) {
    // Unsigned multiplication is defined modulo 2^64 in C++ and therefore
    // produces exactly the required low 64 bits of a signed product.
    return rs1 * rs2;
}

int64_t MExtension::DIV(int64_t rs1, int64_t rs2) {
    if (rs2 == 0) {
        return -1;
    }

    // Avoid C++ signed-overflow UB for INT64_MIN / -1. The quotient modulo
    // 2^64 is represented by INT64_MIN, which is the natural bit-pattern
    // result for a fixed-width 64-bit emulator.
    if (rs1 == std::numeric_limits<int64_t>::min() && rs2 == -1) {
        return std::numeric_limits<int64_t>::min();
    }

    return rs1 / rs2;
}

int64_t MExtension::REM(int64_t rs1, int64_t rs2) {
    if (rs2 == 0) {
        return rs1;
    }

    // The mathematical remainder for INT64_MIN % -1 is zero, but evaluating
    // it directly is tied to the same overflowing division edge case.
    if (rs1 == std::numeric_limits<int64_t>::min() && rs2 == -1) {
        return 0;
    }

    return rs1 % rs2;
}
