#ifndef DISASSEMBLER_H
#define DISASSEMBLER_H

#include <cstdint>
#include <string>

#include "decode.h"

class Disassembler {
public:
    static std::string disassembleCS(uint16_t instruction);
    static std::string disassembleCI(uint16_t instruction);
    static std::string disassembleR(uint32_t instruction);
    static std::string disassembleI(uint32_t instruction);

    // Pipeline-friendly dispatcher for an already-decoded instruction.
    static std::string disassemble(const Decoded& d);
};

#endif
