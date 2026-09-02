#ifndef M_EXTENSION_H
#define M_EXTENSION_H

#include <cstdint>

class MExtension {
public:
    // Lower XLEN bits of signed multiplication are required.
    static uint64_t MUL(uint64_t rs1, uint64_t rs2);

    // Signed division. Divide-by-zero returns all 1s (-1).
    static int64_t DIV(int64_t rs1, int64_t rs2);

    // Signed remainder. Divide-by-zero returns rs1.
    static int64_t REM(int64_t rs1, int64_t rs2);
};

#endif
