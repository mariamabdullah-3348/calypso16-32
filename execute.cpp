#include "execute.h"

#include "m_extension.h"

#include <cstdint>
#include <cstdio>

bool Executor::is_alu_op(const Decoded& d) const {
    if (d.format == FMT_CS || d.format == FMT_R) return true;
    if (d.format == FMT_CI) return d.funct <= ALU_SLTI;
    if (d.format == FMT_I)  return d.funct <= ALU_SLTI;
    return false;
}

void Executor::execute_alu(const Decoded& d) {
    const uint64_t rs1_val = cpu_.rf.read(d.rs1);
    uint64_t result = 0;

    if (d.format == FMT_CS || d.format == FMT_R) {
        const uint64_t rs2_val = cpu_.rf.read(d.rs2);
        switch (d.funct) {
            case ALU_ADD: result = rs1_val + rs2_val; break;
            case ALU_SUB: result = rs1_val - rs2_val; break;
            case ALU_AND: result = rs1_val & rs2_val; break;
            case ALU_OR:  result = rs1_val | rs2_val; break;
            case ALU_SLT: result = (static_cast<int64_t>(rs1_val) < static_cast<int64_t>(rs2_val)) ? 1ULL : 0ULL; break;
            case ALU_MUL: result = MExtension::MUL(rs1_val, rs2_val); break;
            case ALU_DIV: result = static_cast<uint64_t>(MExtension::DIV(static_cast<int64_t>(rs1_val), static_cast<int64_t>(rs2_val))); break;
            case ALU_REM: result = static_cast<uint64_t>(MExtension::REM(static_cast<int64_t>(rs1_val), static_cast<int64_t>(rs2_val))); break;
            default: cpu_.running = false; return;
        }
    } else {
        const uint64_t imm_val = static_cast<uint64_t>(d.imm);
        switch (d.funct) {
            case ALU_ADDI: result = rs1_val + imm_val; break;
            case ALU_ANDI: result = rs1_val & imm_val; break;
            case ALU_ORI:  result = rs1_val | imm_val; break;
            case ALU_SLTI: result = (static_cast<int64_t>(rs1_val) < d.imm) ? 1ULL : 0ULL; break;
            default: cpu_.running = false; return;
        }
    }

    cpu_.rf.write(d.rd, result);
}

void Executor::execute_mem_branch_jump(const Decoded& d) {
    switch (d.funct) {
        case OP_LW: {
            const uint64_t addr = cpu_.rf.read(d.rs1) + static_cast<uint64_t>(d.imm);
            int64_t value = 0;
            if (!mem_.read64(addr, value)) {
                std::printf("Illegal memory access (LW) at addr=0x%016llX, PC=0x%04llX\n",
                            static_cast<unsigned long long>(addr),
                            static_cast<unsigned long long>(cpu_.PC));
                cpu_.running = false;
                return;
            }
            cpu_.rf.write(d.rd, static_cast<uint64_t>(value));
            break;
        }

        case OP_SW: {
            // I-format rd field is the source register (rs2) for SW.
            const uint64_t addr  = cpu_.rf.read(d.rs1) + static_cast<uint64_t>(d.imm);
            const int64_t  value = static_cast<int64_t>(cpu_.rf.read(d.rd));
            if (!mem_.write64(addr, value)) {
                std::printf("Illegal memory access (SW) at addr=0x%016llX, PC=0x%04llX\n",
                            static_cast<unsigned long long>(addr),
                            static_cast<unsigned long long>(cpu_.PC));
                cpu_.running = false;
                return;
            }
            break;
        }

        case OP_BEQ:
            // I-format rd field is the second compare register for BEQ.
            if (cpu_.rf.read(d.rs1) == cpu_.rf.read(d.rd)) {
                cpu_.PC = static_cast<uint64_t>(static_cast<int64_t>(cpu_.PC) + d.imm);
            }
            // Not taken: caller advances by the instruction length (4).
            break;

        case OP_JAL:
            // Architectural halt convention: JAL x0, 0.
            if (d.rd == 0 && d.imm == 0) {
                cpu_.running = false;
                return;
            }

            // JAL is an I-format 32-bit instruction in Calypso16/32.
            cpu_.rf.write(d.rd, cpu_.PC + 4);
            cpu_.PC = static_cast<uint64_t>(static_cast<int64_t>(cpu_.PC) + d.imm);
            break;

        default:
            cpu_.running = false;
            break;
    }
}

void Executor::execute(const Decoded& d) {
    if (d.illegal) {
        cpu_.running = false;
        return;
    }

    if (is_alu_op(d)) {
        execute_alu(d);
    } else {
        execute_mem_branch_jump(d);
    }
}
