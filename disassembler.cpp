#include "disassembler.h"

#include <cstdint>
#include <string>

namespace {

static std::string xreg(int r) {
    return "x" + std::to_string(r);
}

static int64_t sign_extend_6(uint16_t v) {
    const int64_t value = static_cast<int64_t>(v & 0x3Fu);
    return (value & 0x20) ? (value - 64) : value;
}

static int64_t sign_extend_16(uint32_t v) {
    const int64_t value = static_cast<int64_t>(v & 0xFFFFu);
    return (value & 0x8000) ? (value - 65536) : value;
}

} // namespace

std::string Disassembler::disassembleCS(uint16_t instruction) {
    static const char* names[8] = {
        "C.ADD", "C.SUB", "C.AND", "C.OR",
        "C.SLT", "C.MUL", "C.DIV", "C.REM"
    };

    const int funct = (instruction >> 10) & 0xF;
    if (funct > 7) return "UNKNOWN";

    const int rd  = (instruction >> 5) & 0x1F;
    const int rs2 = instruction & 0x1F;
    return std::string(names[funct]) + " " + xreg(rd) + ", " + xreg(rs2);
}

std::string Disassembler::disassembleCI(uint16_t instruction) {
    static const char* names[4] = { "C.ADDI", "C.ANDI", "C.ORI", "C.SLTI" };

    const int funct = (instruction >> 11) & 0x7;
    if (funct > 3) return "UNKNOWN";

    const int rd = (instruction >> 6) & 0x1F;
    const int64_t imm = sign_extend_6(instruction);
    return std::string(names[funct]) + " " + xreg(rd) + ", " + std::to_string(imm);
}

std::string Disassembler::disassembleR(uint32_t instruction) {
    static const char* names[8] = {
        "ADD", "SUB", "AND", "OR", "SLT", "MUL", "DIV", "REM"
    };

    const int funct = (instruction >> 26) & 0xF;
    if (funct > 7) return "UNKNOWN";

    const int rd  = (instruction >> 21) & 0x1F;
    const int rs1 = (instruction >> 16) & 0x1F;
    const int rs2 = (instruction >> 11) & 0x1F;

    return std::string(names[funct]) + " " + xreg(rd) + ", " + xreg(rs1) + ", " + xreg(rs2);
}

std::string Disassembler::disassembleI(uint32_t instruction) {
    const int funct = (instruction >> 26) & 0xF;
    const int rd    = (instruction >> 21) & 0x1F;
    const int rs1   = (instruction >> 16) & 0x1F;
    const int64_t imm = sign_extend_16(instruction);

    switch (funct) {
        case ALU_ADDI:
            return "ADDI " + xreg(rd) + ", " + xreg(rs1) + ", " + std::to_string(imm);
        case ALU_ANDI:
            return "ANDI " + xreg(rd) + ", " + xreg(rs1) + ", " + std::to_string(imm);
        case ALU_ORI:
            return "ORI " + xreg(rd) + ", " + xreg(rs1) + ", " + std::to_string(imm);
        case ALU_SLTI:
            return "SLTI " + xreg(rd) + ", " + xreg(rs1) + ", " + std::to_string(imm);
        case OP_LW:
            return "LW " + xreg(rd) + ", " + std::to_string(imm) + "(" + xreg(rs1) + ")";
        case OP_SW:
            return "SW " + xreg(rd) + ", " + std::to_string(imm) + "(" + xreg(rs1) + ")";
        case OP_BEQ:
            return "BEQ " + xreg(rs1) + ", " + xreg(rd) + ", " + std::to_string(imm);
        case OP_JAL:
            return "JAL " + xreg(rd) + ", " + std::to_string(imm);
        default:
            return "UNKNOWN";
    }
}

std::string Disassembler::disassemble(const Decoded& d) {
    if (d.illegal) {
        return "UNKNOWN";
    }

    switch (d.format) {
        case FMT_CS: {
            static const char* names[8] = {
                "C.ADD", "C.SUB", "C.AND", "C.OR",
                "C.SLT", "C.MUL", "C.DIV", "C.REM"
            };
            return std::string(names[d.funct]) + " " + xreg(d.rd) + ", " + xreg(d.rs2);
        }
        case FMT_CI: {
            static const char* names[4] = { "C.ADDI", "C.ANDI", "C.ORI", "C.SLTI" };
            return std::string(names[d.funct]) + " " + xreg(d.rd) + ", " + std::to_string(d.imm);
        }
        case FMT_R: {
            static const char* names[8] = {
                "ADD", "SUB", "AND", "OR", "SLT", "MUL", "DIV", "REM"
            };
            return std::string(names[d.funct]) + " " + xreg(d.rd) + ", " +
                   xreg(d.rs1) + ", " + xreg(d.rs2);
        }
        case FMT_I:
            switch (d.funct) {
                case ALU_ADDI: return "ADDI " + xreg(d.rd) + ", " + xreg(d.rs1) + ", " + std::to_string(d.imm);
                case ALU_ANDI: return "ANDI " + xreg(d.rd) + ", " + xreg(d.rs1) + ", " + std::to_string(d.imm);
                case ALU_ORI:  return "ORI "  + xreg(d.rd) + ", " + xreg(d.rs1) + ", " + std::to_string(d.imm);
                case ALU_SLTI: return "SLTI " + xreg(d.rd) + ", " + xreg(d.rs1) + ", " + std::to_string(d.imm);
                case OP_LW:    return "LW "   + xreg(d.rd) + ", " + std::to_string(d.imm) + "(" + xreg(d.rs1) + ")";
                case OP_SW:    return "SW "   + xreg(d.rd) + ", " + std::to_string(d.imm) + "(" + xreg(d.rs1) + ")";
                case OP_BEQ:   return "BEQ "  + xreg(d.rs1) + ", " + xreg(d.rd) + ", " + std::to_string(d.imm);
                case OP_JAL:   return "JAL "  + xreg(d.rd) + ", " + std::to_string(d.imm);
                default:       return "UNKNOWN";
            }
        default:
            return "UNKNOWN";
    }
}
