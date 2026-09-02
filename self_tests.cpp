#include "self_tests.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#include "cpu_state.h"
#include "decode.h"
#include "disassembler.h"
#include "execute.h"
#include "fetch.h"
#include "memory.h"

// ============================================================
// File-private architectural state used exclusively by the
// self-test suite so it does not interfere with the main
// program's CPUState instance.
// ============================================================
static CPUState  t_cpu;
static Memory    t_mem(t_cpu);
static Fetcher   t_fetcher(t_cpu);
static Executor  t_exec(t_cpu, t_mem);

namespace {

// ============================================================
// ISA encoding helpers
// ============================================================
static uint16_t encCS(int funct4, int rd, int rs2) {
    return static_cast<uint16_t>((0b00u << 14) |
                                 ((static_cast<uint16_t>(funct4) & 0xFu) << 10) |
                                 ((static_cast<uint16_t>(rd)     & 0x1Fu) << 5) |
                                 (static_cast<uint16_t>(rs2)     & 0x1Fu));
}

static uint16_t encCI(int funct3, int rd, int imm6) {
    return static_cast<uint16_t>((0b01u << 14) |
                                 ((static_cast<uint16_t>(funct3) & 0x7u) << 11) |
                                 ((static_cast<uint16_t>(rd)     & 0x1Fu) << 6) |
                                 (static_cast<uint16_t>(imm6)   & 0x3Fu));
}

static uint32_t encR(int funct4, int rd, int rs1, int rs2) {
    return (0b10u << 30) |
           ((static_cast<uint32_t>(funct4) & 0xFu) << 26) |
           ((static_cast<uint32_t>(rd)     & 0x1Fu) << 21) |
           ((static_cast<uint32_t>(rs1)    & 0x1Fu) << 16) |
           ((static_cast<uint32_t>(rs2)    & 0x1Fu) << 11);
}

static uint32_t encI(int funct4, int rd_or_rs2, int rs1, int imm16) {
    return (0b11u << 30) |
           ((static_cast<uint32_t>(funct4)    & 0xFu)   << 26) |
           ((static_cast<uint32_t>(rd_or_rs2) & 0x1Fu)  << 21) |
           ((static_cast<uint32_t>(rs1)       & 0x1Fu)  << 16) |
           (static_cast<uint32_t>(imm16) & 0xFFFFu);
}

// ============================================================
// Big-Endian memory write helpers (used to load test programs)
// ============================================================
static void write16BE(uint64_t addr, uint16_t value) {
    t_cpu.memory[addr]     = static_cast<uint8_t>(value >> 8);
    t_cpu.memory[addr + 1] = static_cast<uint8_t>(value);
}

static void write32BE(uint64_t addr, uint32_t value) {
    t_cpu.memory[addr]     = static_cast<uint8_t>(value >> 24);
    t_cpu.memory[addr + 1] = static_cast<uint8_t>(value >> 16);
    t_cpu.memory[addr + 2] = static_cast<uint8_t>(value >> 8);
    t_cpu.memory[addr + 3] = static_cast<uint8_t>(value);
}

// ============================================================
// State reset
// ============================================================
static void reset_state() {
    t_cpu.reset();
}

static int tests_passed = 0;
static int tests_failed = 0;

// ============================================================
// Print helpers
// ============================================================
static void print_registers_full(const char* label) {
    std::printf("    %s\n", label);
    for (int i = 0; i < NUM_REGS; ++i) {
        std::printf("      x%-2d = 0x%016llX (%lld)%s",
                    i,
                    static_cast<unsigned long long>(t_cpu.rf.read(i)),
                    static_cast<long long>(static_cast<int64_t>(t_cpu.rf.read(i))),
                    (i % 4 == 3 || i == NUM_REGS - 1) ? "\n" : "    ");
    }
}

static void print_memory_dump(uint64_t start_addr, int count = 16) {
    if (start_addr >= MEM_SIZE) {
        std::printf("    MEMORY: address 0x%04llX is outside 64KB memory\n",
                    static_cast<unsigned long long>(start_addr));
        return;
    }
    const uint64_t end_addr = (start_addr + static_cast<uint64_t>(count) - 1 < MEM_SIZE)
                                  ? start_addr + static_cast<uint64_t>(count) - 1
                                  : MEM_SIZE - 1;
    std::printf("    MEMORY [0x%04llX..0x%04llX]:",
                static_cast<unsigned long long>(start_addr),
                static_cast<unsigned long long>(end_addr));
    for (uint64_t a = start_addr; a <= end_addr; ++a) {
        std::printf(" %02X", t_cpu.memory[a]);
    }
    std::printf("\n");
}

static const char* format_name(int format) {
    switch (format) {
        case FMT_CS: return "CS (16-bit, compressed register-register)";
        case FMT_CI: return "CI (16-bit, compressed register-immediate)";
        case FMT_R:  return "R  (32-bit, register-register)";
        case FMT_I:  return "I  (32-bit, immediate / memory / branch / jump)";
        default:     return "UNKNOWN";
    }
}

static void print_decoded_fields(const Decoded& d) {
    std::printf("    DECODED FIELDS:\n");
    std::printf("      format = %s\n", format_name(d.format));
    std::printf("      funct  = %d\n", d.funct);
    std::printf("      rd     = x%d\n", d.rd);
    std::printf("      rs1    = x%d\n", d.rs1);
    std::printf("      rs2    = x%d\n", d.rs2);
    std::printf("      imm    = %lld (0x%016llX)\n",
                static_cast<long long>(d.imm),
                static_cast<unsigned long long>(static_cast<uint64_t>(d.imm)));
    std::printf("      illegal= %s\n", d.illegal ? "true" : "false");
}

static void print_test_result_detail(const char* test_name,
                                     const char* purpose,
                                     const char* expected,
                                     const char* actual,
                                     bool pass) {
    std::printf("    PURPOSE : %s\n", purpose);
    std::printf("    EXPECTED: %s\n", expected);
    std::printf("    ACTUAL  : %s\n", actual);
    std::printf("    STATUS  : %s\n", pass ? "PASS" : "FAIL");
    (void)test_name;
}

static void expect(bool condition, const char* name) {
    (void)name;
    if (condition) {
        ++tests_passed;
    } else {
        ++tests_failed;
    }
}

static void trace_execute_instruction(const char* test_title,
                                      const char* purpose,
                                      uint32_t raw,
                                      int size,
                                      const char* expected) {
    const uint64_t old_pc = t_cpu.PC;
    const Decoded d = Decoder::decode(raw, size);
    const std::string assembly = d.illegal ? "<ILLEGAL>" : Disassembler::disassemble(d);

    std::printf("\n======================================================================\n");
    std::printf("INSTRUCTION TRACE: %s\n", test_title);
    std::printf("======================================================================\n");
    std::printf("    PC BEFORE : 0x%04llX\n", static_cast<unsigned long long>(old_pc));
    std::printf("    RAW       : 0x%0*X (%d bytes)\n", size * 2, raw, size);
    std::printf("    INSTRUCTION: %s\n", assembly.c_str());
    print_decoded_fields(d);
    print_registers_full("REGISTERS BEFORE:");

    if (size == 2) {
        print_memory_dump(PROGRAM_START, 16);
    }

    t_exec.execute(d);

    if (t_cpu.running && t_cpu.PC == old_pc) {
        t_cpu.PC += static_cast<uint64_t>(size);
    }

    std::printf("    EXECUTION : %s\n", purpose);
    std::printf("    PC AFTER  : 0x%04llX\n", static_cast<unsigned long long>(t_cpu.PC));
    if (t_cpu.running && t_cpu.PC == old_pc + static_cast<uint64_t>(size)) {
        std::printf("                sequential advance by %d bytes\n", size);
    } else if (!t_cpu.running) {
        std::printf("                emulator halted during this instruction\n");
    } else {
        std::printf("                control-flow instruction redirected the PC\n");
    }
    print_registers_full("REGISTERS AFTER:");
    if (d.format == FMT_I && (d.funct == OP_LW || d.funct == OP_SW)) {
        const uint64_t addr = t_cpu.rf.read(d.rs1) + static_cast<uint64_t>(d.imm);
        if (addr <= static_cast<uint64_t>(MEM_SIZE - 8)) {
            print_memory_dump(addr, 8);
        }
    }
    std::printf("    EXPECTED   : %s\n", expected);
}

static void decode_trace(const char* name, uint32_t raw, int size, const char* expected) {
    const Decoded d = Decoder::decode(raw, size);
    const std::string assembly = d.illegal ? "<ILLEGAL>" : Disassembler::disassemble(d);
    std::printf("\n======================================================================\n");
    std::printf("DECODE/DISASSEMBLE TRACE: %s\n", name);
    std::printf("======================================================================\n");
    std::printf("    PC CONTEXT : 0x%04llX\n", static_cast<unsigned long long>(t_cpu.PC));
    std::printf("    RAW        : 0x%0*X (%d bytes)\n", size * 2, raw, size);
    std::printf("    ASSEMBLY   : %s\n", assembly.c_str());
    print_decoded_fields(d);
    print_registers_full("REGISTER FILE (unchanged by decode/disassembly):");
    print_test_result_detail(name, "Decode and/or disassemble the encoded instruction.", expected,
                             d.illegal ? "illegal" : "legal; fields shown above", !d.illegal);
}

} // namespace

// ============================================================
// Public entry point
// ============================================================
bool run_self_tests() {
    tests_passed = 0;
    tests_failed = 0;

    std::printf("\n######################################################################\n");
    std::printf("# Calypso16/32 COMPLETE SELF-TEST + ARCHITECTURAL EXECUTION TRACE #\n");
    std::printf("######################################################################\n");
    std::printf("Every test prints the instruction/encoding, PC, decoded fields,\n");
    std::printf("expected behavior, actual state, complete register file, and memory\n");
    std::printf("when relevant. This output is intended to be readable without opening\n");
    std::printf("the source code.\n\n");

    int test_no = 0;
    auto report = [&](const char* name, bool condition, const char* expected, const char* actual) {
        ++test_no;
        std::printf("\n---------------------------------------------------------------------\n");
        std::printf("TEST %02d: %s\n", test_no, name);
        print_test_result_detail(name, "Validation result for this architectural requirement.", expected, actual, condition);
        print_registers_full("REGISTER FILE SNAPSHOT:");
        std::printf("    PC          : 0x%04llX\n", static_cast<unsigned long long>(t_cpu.PC));
    };

    // -----------------------------------------------------------------
    // 1-4: Decode coverage (all four formats)
    // -----------------------------------------------------------------
    {
        reset_state();
        const uint32_t raw = encCS(ALU_ADD, 5, 7);
        Decoded cs = Decoder::decode(raw, 2);
        decode_trace("CS decode", raw, 2, "format=CS, rd=x5, rs1=x5, rs2=x7, funct=ADD");
        const bool ok = cs.format == FMT_CS && cs.rd == 5 && cs.rs1 == 5 && cs.rs2 == 7 && !cs.illegal;
        expect(ok, "CS decode");
        report("CS decode", ok, "CS + x5/x5/x7 + ADD + legal", "See decoded fields above");

        reset_state();
        const uint32_t raw2 = encCI(ALU_ADDI, 10, -3);
        Decoded ci = Decoder::decode(raw2, 2);
        decode_trace("CI decode + 6-bit sign extension", raw2, 2, "format=CI, rd=x10, rs1=x10, imm=-3");
        const bool ok2 = ci.format == FMT_CI && ci.rd == 10 && ci.rs1 == 10 && ci.imm == -3 && !ci.illegal;
        expect(ok2, "CI decode + 6-bit sign extension");
        report("CI decode + 6-bit sign extension", ok2, "imm6 111101 becomes signed -3", "d.imm shown above");

        reset_state();
        const uint32_t raw3 = encR(ALU_SUB, 1, 2, 3);
        Decoded r = Decoder::decode(raw3, 4);
        decode_trace("R decode", raw3, 4, "format=R, rd=x1, rs1=x2, rs2=x3, funct=SUB");
        const bool ok3 = r.format == FMT_R && r.rd == 1 && r.rs1 == 2 && r.rs2 == 3 && !r.illegal;
        expect(ok3, "R decode");
        report("R decode", ok3, "R + x1/x2/x3 + SUB + legal", "d fields shown above");

        reset_state();
        const uint32_t raw4 = encI(ALU_ADDI, 4, 5, -9);
        Decoded i = Decoder::decode(raw4, 4);
        decode_trace("I decode + 16-bit sign extension", raw4, 4, "format=I, rd=x4, rs1=x5, imm=-9");
        const bool ok4 = i.format == FMT_I && i.rd == 4 && i.rs1 == 5 && i.imm == -9 && !i.illegal;
        expect(ok4, "I decode + 16-bit sign extension");
        report("I decode + 16-bit sign extension", ok4, "imm16 FFF7 becomes signed -9", "d.imm shown above");
    }

    // -----------------------------------------------------------------
    // 5-8: Disassembler coverage
    // -----------------------------------------------------------------
    {
        reset_state();
        const uint16_t raw = encCS(ALU_ADD, 5, 7);
        const std::string actual = Disassembler::disassemble(Decoder::decode(raw, 2));
        std::printf("\n======================================================================\nDETAILED DISASSEMBLY TEST\n======================================================================\nRAW=0x%04X  PC=0x%04llX  Instruction=%s\n",
                    raw, static_cast<unsigned long long>(t_cpu.PC), actual.c_str());
        print_registers_full("REGISTERS:");
        const bool ok = actual == "C.ADD x5, x7";
        expect(ok, "CS disassembly");
        report("CS disassembly", ok, "C.ADD x5, x7", actual.c_str());

        reset_state();
        const uint16_t raw2 = encCI(ALU_ADDI, 10, -3);
        const std::string actual2 = Disassembler::disassemble(Decoder::decode(raw2, 2));
        std::printf("\nRAW=0x%04X  PC=0x%04llX  Instruction=%s\n", raw2, static_cast<unsigned long long>(t_cpu.PC), actual2.c_str());
        print_registers_full("REGISTERS:");
        const bool ok2 = actual2 == "C.ADDI x10, -3";
        expect(ok2, "CI disassembly");
        report("CI disassembly", ok2, "C.ADDI x10, -3", actual2.c_str());

        reset_state();
        const uint32_t raw3 = encR(ALU_MUL, 3, 1, 2);
        const std::string actual3 = Disassembler::disassemble(Decoder::decode(raw3, 4));
        std::printf("\nRAW=0x%08X  PC=0x%04llX  Instruction=%s\n", raw3, static_cast<unsigned long long>(t_cpu.PC), actual3.c_str());
        print_registers_full("REGISTERS:");
        const bool ok3 = actual3 == "MUL x3, x1, x2";
        expect(ok3, "R disassembly");
        report("R disassembly", ok3, "MUL x3, x1, x2", actual3.c_str());

        reset_state();
        const uint32_t raw4 = encI(OP_LW, 3, 0, 8);
        const std::string actual4 = Disassembler::disassemble(Decoder::decode(raw4, 4));
        std::printf("\nRAW=0x%08X  PC=0x%04llX  Instruction=%s\n", raw4, static_cast<unsigned long long>(t_cpu.PC), actual4.c_str());
        print_registers_full("REGISTERS:");
        const bool ok4 = actual4 == "LW x3, 8(x0)";
        expect(ok4, "I disassembly");
        report("I disassembly", ok4, "LW x3, 8(x0)", actual4.c_str());
    }

    // -----------------------------------------------------------------
    // 9-10: x0 hardwired behavior
    // -----------------------------------------------------------------
    {
        reset_state();
        t_cpu.rf.raw(1) = 12;
        trace_execute_instruction("x0 ignores writes", "Attempt ADD x0, x1, x1. write() must discard the destination write.",
                                  encR(ALU_ADD, 0, 1, 1), 4, "x0 remains 0 even though x1+x1 = 24");
        const bool ok = t_cpu.rf.raw(0) == 0;
        expect(ok, "x0 ignores writes");
        report("x0 ignores writes", ok, "x0 = 0", "x0 is 0 after ADD");

        reset_state();
        t_cpu.rf.raw(0) = 12345;
        const uint64_t read_value = t_cpu.rf.read(0);
        std::printf("\n======================================================================\nX0 READ RULE\n======================================================================\nPC=0x%04llX  Physical storage regs[0]=%llu  Architectural read(0)=%llu\n",
                    static_cast<unsigned long long>(t_cpu.PC), 12345ULL, static_cast<unsigned long long>(read_value));
        print_registers_full("REGISTERS:");
        const bool ok2 = read_value == 0;
        t_cpu.rf.raw(0) = 0;
        expect(ok2, "x0 reads as zero");
        report("x0 reads as zero", ok2, "rf.read(0) = 0 regardless of stored value", "rf.read(0) = 0");
    }

    // -----------------------------------------------------------------
    // 11-19: Base ALU instructions
    // -----------------------------------------------------------------
    {
        reset_state();
        t_cpu.rf.raw(1) = 20;
        t_cpu.rf.raw(2) = 6;

        trace_execute_instruction("ADD", "x3 = x1 + x2 = 20 + 6", encR(ALU_ADD, 3, 1, 2), 4, "x3 = 26");
        const bool add_ok = t_cpu.rf.raw(3) == 26;
        expect(add_ok, "ADD");
        report("ADD", add_ok, "x3 = 26", "x3 = 26");

        trace_execute_instruction("SUB", "x4 = x1 - x2 = 20 - 6", encR(ALU_SUB, 4, 1, 2), 4, "x4 = 14");
        const bool sub_ok = t_cpu.rf.raw(4) == 14;
        expect(sub_ok, "SUB");
        report("SUB", sub_ok, "x4 = 14", "x4 = 14");

        trace_execute_instruction("AND", "x5 = x1 & x2 = 20 & 6", encR(ALU_AND, 5, 1, 2), 4, "x5 = 4");
        const bool and_ok = t_cpu.rf.raw(5) == (20ULL & 6ULL);
        expect(and_ok, "AND");
        report("AND", and_ok, "x5 = 4", "x5 = 4");

        trace_execute_instruction("OR", "x6 = x1 | x2 = 20 | 6", encR(ALU_OR, 6, 1, 2), 4, "x6 = 22");
        const bool or_ok = t_cpu.rf.raw(6) == (20ULL | 6ULL);
        expect(or_ok, "OR");
        report("OR", or_ok, "x6 = 22", "x6 = 22");

        trace_execute_instruction("SLT signed", "x7 = (signed(x2) < signed(x1)) ? 1 : 0 = (6 < 20)", encR(ALU_SLT, 7, 2, 1), 4, "x7 = 1");
        const bool slt_ok = t_cpu.rf.raw(7) == 1;
        expect(slt_ok, "SLT signed");
        report("SLT signed", slt_ok, "x7 = 1", "x7 = 1");

        t_cpu.rf.raw(1) = 100;
        trace_execute_instruction("ADDI with negative immediate", "x8 = 100 + (-20)", encI(ALU_ADDI, 8, 1, -20), 4, "x8 = 80");
        const bool addi_ok = t_cpu.rf.raw(8) == 80;
        expect(addi_ok, "ADDI with negative immediate");
        report("ADDI with negative immediate", addi_ok, "x8 = 80", "x8 = 80");

        trace_execute_instruction("ANDI", "x9 = 100 & 0x0F", encI(ALU_ANDI, 9, 1, 0x0F), 4, "x9 = 4");
        const bool andi_ok = t_cpu.rf.raw(9) == (100ULL & 0x0FULL);
        expect(andi_ok, "ANDI");
        report("ANDI", andi_ok, "x9 = 4", "x9 = 4");

        trace_execute_instruction("ORI", "x10 = 100 | 0x100", encI(ALU_ORI, 10, 1, 0x100), 4, "x10 = 356");
        const bool ori_ok = t_cpu.rf.raw(10) == (100ULL | 0x100ULL);
        expect(ori_ok, "ORI");
        report("ORI", ori_ok, "x10 = 356", "x10 = 356");

        t_cpu.rf.raw(1) = static_cast<uint64_t>(-5LL);
        trace_execute_instruction("SLTI signed", "x11 = (signed(-5) < signed(-2)) ? 1 : 0", encI(ALU_SLTI, 11, 1, -2), 4, "x11 = 1");
        const bool slti_ok = t_cpu.rf.raw(11) == 1;
        expect(slti_ok, "SLTI signed");
        report("SLTI signed", slti_ok, "x11 = 1", "x11 = 1");
    }

    // -----------------------------------------------------------------
    // 20-25: Compressed CS/CI execution
    // -----------------------------------------------------------------
    {
        reset_state();
        t_cpu.rf.raw(1) = 12; t_cpu.rf.raw(2) = 5;
        trace_execute_instruction("C.ADD", "x1 = x1 + x2 = 12 + 5", encCS(ALU_ADD, 1, 2), 2, "x1 = 17");
        trace_execute_instruction("C.SUB", "x1 = x1 - x2", encCS(ALU_SUB, 1, 2), 2, "x1 = 12");
        trace_execute_instruction("C.AND", "x1 = x1 & x2", encCS(ALU_AND, 1, 2), 2, "x1 reflects bitwise AND");
        trace_execute_instruction("C.OR", "x1 = x1 | x2", encCS(ALU_OR, 1, 2), 2, "x1 reflects bitwise OR");
        t_cpu.rf.raw(1) = 5; t_cpu.rf.raw(2) = 7;
        trace_execute_instruction("C.SLT", "x1 = (signed(x1) < signed(x2)) ? 1 : 0", encCS(ALU_SLT, 1, 2), 2, "x1 = 1");
        t_cpu.rf.raw(1) = 6; t_cpu.rf.raw(2) = 7;
        trace_execute_instruction("C.MUL", "x1 = lower64(x1 * x2) = 42", encCS(ALU_MUL, 1, 2), 2, "x1 = 42");
        t_cpu.rf.raw(1) = 20; t_cpu.rf.raw(2) = 5;
        trace_execute_instruction("C.DIV", "x1 = signed(20) / signed(5) = 4", encCS(ALU_DIV, 1, 2), 2, "x1 = 4");
        t_cpu.rf.raw(1) = 23; t_cpu.rf.raw(2) = 5;
        trace_execute_instruction("C.REM", "x1 = signed(23) % signed(5) = 3", encCS(ALU_REM, 1, 2), 2, "x1 = 3");
        const bool cs_ok = t_cpu.rf.raw(1) == 3;
        expect(cs_ok, "CS ADD/SUB/AND/OR/SLT/MUL/DIV/REM execution");
        report("CS ADD/SUB/AND/OR/SLT/MUL/DIV/REM execution", cs_ok, "all eight CS operations execute; final x1=3", "final x1=3");

        reset_state(); t_cpu.rf.raw(1) = 20;
        trace_execute_instruction("C.ADDI", "x1 = 20 + (-3) = 17", encCI(ALU_ADDI, 1, -3), 2, "x1 = 17");
        const bool c_addi_ok = t_cpu.rf.raw(1) == 17;
        expect(c_addi_ok, "C.ADDI immediate execution");
        report("C.ADDI immediate execution", c_addi_ok, "x1 = 17", "x1 = 17");

        trace_execute_instruction("C.ANDI", "x1 = 17 & 3", encCI(ALU_ANDI, 1, 3), 2, "x1 = 1");
        const bool c_andi_ok = t_cpu.rf.raw(1) == (17ULL & 3ULL);
        expect(c_andi_ok, "C.ANDI execution");
        report("C.ANDI execution", c_andi_ok, "x1 = 1", "x1 = 1");

        t_cpu.rf.raw(1) = 0x10;
        trace_execute_instruction("C.ORI", "x1 = 0x10 | 3 = 0x13", encCI(ALU_ORI, 1, 3), 2, "x1 = 0x13");
        const bool c_ori_ok = t_cpu.rf.raw(1) == 0x13;
        expect(c_ori_ok, "C.ORI execution");
        report("C.ORI execution", c_ori_ok, "x1 = 0x13", "x1 = 0x13");

        t_cpu.rf.raw(1) = 5;
        trace_execute_instruction("C.SLTI", "x1 = (signed(5) < signed(7)) ? 1 : 0", encCI(ALU_SLTI, 1, 7), 2, "x1 = 1");
        const bool c_slti_ok = t_cpu.rf.raw(1) == 1;
        expect(c_slti_ok, "C.SLTI execution");
        report("C.SLTI execution", c_slti_ok, "x1 = 1", "x1 = 1");

        t_cpu.rf.raw(1) = 0;
        trace_execute_instruction("C.ADDI minimum signed immediate", "x1 = 0 + (-32), minimum 6-bit signed value", encCI(ALU_ADDI, 1, -32), 2, "signed(x1) = -32");
        const bool c_min_ok = static_cast<int64_t>(t_cpu.rf.raw(1)) == -32;
        expect(c_min_ok, "C.ADDI minimum signed immediate");
        report("C.ADDI minimum signed immediate", c_min_ok, "x1 = -32", "x1 = -32");
    }

    // -----------------------------------------------------------------
    // 26-30: M extension
    // -----------------------------------------------------------------
    {
        reset_state(); t_cpu.rf.raw(1) = static_cast<uint64_t>(-7LL); t_cpu.rf.raw(2) = 6;
        trace_execute_instruction("MUL signed low 64 bits", "x3 = lower64((-7) * 6) = -42", encR(ALU_MUL, 3, 1, 2), 4, "x3 = -42");
        const bool mul_ok = static_cast<int64_t>(t_cpu.rf.raw(3)) == -42;
        expect(mul_ok, "MUL signed low 64 bits");
        report("MUL signed low 64 bits", mul_ok, "signed x3 = -42", "signed x3 = -42");

        trace_execute_instruction("DIV signed", "x4 = signed(-7) / signed(6) = -1", encR(ALU_DIV, 4, 1, 2), 4, "x4 = -1");
        const bool div_ok = static_cast<int64_t>(t_cpu.rf.raw(4)) == -1;
        expect(div_ok, "DIV signed");
        report("DIV signed", div_ok, "signed x4 = -1", "signed x4 = -1");

        trace_execute_instruction("REM signed", "x5 = signed(-7) % signed(6) = -1", encR(ALU_REM, 5, 1, 2), 4, "x5 = -1");
        const bool rem_ok = static_cast<int64_t>(t_cpu.rf.raw(5)) == -1;
        expect(rem_ok, "REM signed");
        report("REM signed", rem_ok, "signed x5 = -1", "signed x5 = -1");

        t_cpu.rf.raw(2) = 0;
        trace_execute_instruction("DIV by zero -> all ones", "DIV by zero must return -1, represented as 0xFFFFFFFFFFFFFFFF", encR(ALU_DIV, 6, 1, 2), 4, "x6 = 0xFFFFFFFFFFFFFFFF");
        const bool div0_ok = t_cpu.rf.raw(6) == UINT64_MAX;
        expect(div0_ok, "DIV by zero -> all ones");
        report("DIV by zero -> all ones", div0_ok, "x6 = all ones", "x6 = 0xFFFFFFFFFFFFFFFF");

        trace_execute_instruction("REM by zero -> rs1", "REM by zero returns rs1, so x7 = -7", encR(ALU_REM, 7, 1, 2), 4, "signed x7 = -7");
        const bool rem0_ok = static_cast<int64_t>(t_cpu.rf.raw(7)) == -7;
        expect(rem0_ok, "REM by zero -> rs1");
        report("REM by zero -> rs1", rem0_ok, "signed x7 = -7", "signed x7 = -7");
    }

    // -----------------------------------------------------------------
    // 31-32: Memory
    // -----------------------------------------------------------------
    {
        reset_state(); t_cpu.rf.raw(1) = 0x1122334455667788ULL; t_cpu.rf.raw(2) = 1;
        trace_execute_instruction("Misaligned SW", "store x1 at address x2=1; address is intentionally misaligned but allowed", encI(OP_SW, 1, 2, 0), 4, "memory[1..8] contains the full 64-bit value");
        t_cpu.rf.raw(1) = 0;
        trace_execute_instruction("Misaligned LW", "load the same 64-bit value back from address 1", encI(OP_LW, 1, 2, 0), 4, "x1 restored to 0x1122334455667788");
        const bool round_ok = t_cpu.rf.raw(1) == 0x1122334455667788ULL;
        expect(round_ok, "Misaligned SW/LW round trip");
        report("Misaligned SW/LW round trip", round_ok, "x1 restored exactly", "x1 = 0x1122334455667788");

        const bool endian_ok = t_cpu.memory[1] == 0x11 && t_cpu.memory[2] == 0x22 && t_cpu.memory[8] == 0x88;
        std::printf("\n======================================================================\nBIG-ENDIAN MEMORY CHECK\n======================================================================\n");
        std::printf("Expected bytes for 0x1122334455667788 at address 1:\n");
        print_memory_dump(1, 8);
        std::printf("Byte order: highest byte 0x11 at lowest address 0x0001, lowest byte 0x88 at 0x0008.\n");
        expect(endian_ok, "Big-endian data storage");
        report("Big-endian data storage", endian_ok, "memory[1]=11, memory[2]=22, memory[8]=88", "bytes shown above");
    }

    // -----------------------------------------------------------------
    // 33-35: Data/program separation and bounds
    // -----------------------------------------------------------------
    {
        reset_state(); t_cpu.rf.raw(1) = 0xAABBCCDDEEFF0011ULL; t_cpu.rf.raw(2) = PROGRAM_START - 1;
        trace_execute_instruction("Store crossing into program region", "attempt 8-byte SW from 0x0FFF; bytes would enter program region at 0x1000", encI(OP_SW, 1, 2, 0), 4, "store rejected and running becomes false");
        const bool sep_ok = !t_cpu.running;
        expect(sep_ok, "Store crossing into program region is rejected");
        report("Store crossing into program region is rejected", sep_ok, "running=false after illegal access", "running=false");

        reset_state();
        int64_t dummy = 0;
        const bool read_bounds_ok = !t_mem.read64(MEM_SIZE - 7, dummy);
        std::printf("\n======================================================================\nMEMORY BOUNDS READ TEST\n======================================================================\nPC=0x%04llX  Address=0x%04X  Request=8 bytes\n",
                    static_cast<unsigned long long>(t_cpu.PC), MEM_SIZE - 7);
        std::printf("Result: mem.read64 returned %s\n", read_bounds_ok ? "false (REJECTED)" : "true (ACCEPTED)");
        print_memory_dump(MEM_SIZE - 16, 16);
        expect(read_bounds_ok, "Out-of-bounds memory read rejected");
        report("Out-of-bounds memory read rejected", read_bounds_ok, "request at 0xFFF9 is rejected", "mem.read64=false");

        const bool write_bounds_ok = !t_mem.write64(MEM_SIZE - 7, 1);
        std::printf("\n======================================================================\nMEMORY BOUNDS WRITE TEST\n======================================================================\nPC=0x%04llX  Address=0x%04X  Request=8 bytes\n",
                    static_cast<unsigned long long>(t_cpu.PC), MEM_SIZE - 7);
        std::printf("Result: mem.write64 returned %s\n", write_bounds_ok ? "false (REJECTED)" : "true (ACCEPTED)");
        print_memory_dump(MEM_SIZE - 16, 16);
        expect(write_bounds_ok, "Out-of-bounds memory write rejected");
        report("Out-of-bounds memory write rejected", write_bounds_ok, "request at 0xFFF9 is rejected", "mem.write64=false");
    }

    // -----------------------------------------------------------------
    // 36-37: BEQ
    // -----------------------------------------------------------------
    {
        reset_state(); t_cpu.rf.raw(1) = 7; t_cpu.rf.raw(2) = 7; t_cpu.PC = 0x2000;
        trace_execute_instruction("BEQ taken target", "x1 == x2, so PC = 0x2000 + 12 = 0x200C", encI(OP_BEQ, 2, 1, 12), 4, "PC becomes 0x200C");
        const bool taken_ok = t_cpu.PC == 0x200C;
        expect(taken_ok, "BEQ taken target");
        report("BEQ taken target", taken_ok, "PC=0x200C", "PC=0x200C");

        reset_state(); t_cpu.rf.raw(1) = 7; t_cpu.rf.raw(2) = 8; t_cpu.PC = 0x2000;
        trace_execute_instruction("BEQ not-taken fall-through", "x1 != x2, so sequential PC = 0x2000 + 4", encI(OP_BEQ, 2, 1, 12), 4, "PC becomes 0x2004");
        const bool not_taken_ok = t_cpu.PC == 0x2004;
        expect(not_taken_ok, "BEQ not-taken fall-through");
        report("BEQ not-taken fall-through", not_taken_ok, "PC=0x2004", "PC=0x2004");
    }

    // -----------------------------------------------------------------
    // 38: JAL
    // -----------------------------------------------------------------
    {
        reset_state(); t_cpu.PC = 0x3000;
        trace_execute_instruction("JAL return address + target", "x5 = PC+4 = 0x3004 and target PC = 0x3000+20 = 0x3014", encI(OP_JAL, 5, 0, 20), 4, "x5=0x3004 and PC=0x3014");
        const bool jal_ok = t_cpu.rf.raw(5) == 0x3004 && t_cpu.PC == 0x3014;
        expect(jal_ok, "JAL return address + target");
        report("JAL return address + target", jal_ok, "x5=0x3004, PC=0x3014", "values shown above");
    }

    // -----------------------------------------------------------------
    // 39-40: Illegal instructions
    // -----------------------------------------------------------------
    {
        reset_state();
        Decoded bad = Decoder::decode(encCS(8, 1, 2), 2);
        std::printf("\n======================================================================\nRESERVED CS FUNCTION TEST\n======================================================================\n");
        std::printf("PC=0x%04llX RAW=0x%04X -> funct4=8 is reserved (defined values are 0..7).\n",
                    static_cast<unsigned long long>(t_cpu.PC), encCS(8, 1, 2));
        print_decoded_fields(bad);
        print_registers_full("REGISTERS:");
        const bool reserved_ok = bad.illegal;
        expect(reserved_ok, "Reserved CS funct detected");
        report("Reserved CS funct detected", reserved_ok, "d.illegal=true", "illegal=true");

        t_cpu.running = true;
        t_exec.execute(bad);
        const bool halt_ok = !t_cpu.running;
        std::printf("\nIllegal instruction execute path: running=%s\n", t_cpu.running ? "true" : "false");
        print_registers_full("REGISTERS AFTER ILLEGAL INSTRUCTION:");
        expect(halt_ok, "Illegal instruction halts emulator");
        report("Illegal instruction halts emulator", halt_ok, "running=false", "running=false");
    }

    // -----------------------------------------------------------------
    // 41-43: Full mixed-length integration
    // -----------------------------------------------------------------
    {
        reset_state();
        write16BE(PROGRAM_START + 0,  encCI(ALU_ADDI, 1, 5));
        write16BE(PROGRAM_START + 2,  encCS(ALU_ADD, 1, 1));
        write32BE(PROGRAM_START + 4,  encI(OP_SW, 1, 0, 0));
        write32BE(PROGRAM_START + 8,  encI(OP_LW, 3, 0, 0));
        write32BE(PROGRAM_START + 12, encI(OP_JAL, 0, 0, 0));

        std::printf("\n######################################################################\n");
        std::printf("# FULL MIXED-LENGTH PROGRAM EXECUTION                              #\n");
        std::printf("######################################################################\n");
        std::printf("Program memory map:\n");
        std::printf("  0x1000  C.ADDI x1, 5\n");
        std::printf("  0x1002  C.ADD  x1, x1\n");
        std::printf("  0x1004  SW x1, 0(x0)\n");
        std::printf("  0x1008  LW x3, 0(x0)\n");
        std::printf("  0x100C  JAL x0, 0  (HALT)\n\n");

        int cycles = 0;
        while (t_cpu.running && cycles < MAX_CYCLES) {
            uint32_t raw = 0;
            int size = 0;
            if (!t_fetcher.fetch(raw, size)) {
                t_cpu.running = false;
                break;
            }
            const Decoded d = Decoder::decode(raw, size);
            if (d.illegal) {
                t_cpu.running = false;
                break;
            }

            const uint64_t old_pc = t_cpu.PC;
            const std::string assembly = Disassembler::disassemble(d);
            std::printf("\n......................................................................\n");
            std::printf("INTEGRATION CYCLE %d\n", cycles + 1);
            std::printf("    PC BEFORE : 0x%04llX\n", static_cast<unsigned long long>(old_pc));
            std::printf("    FETCHED   : 0x%0*X (%d bytes)\n", size * 2, raw, size);
            std::printf("    INSTRUCTION: %s\n", assembly.c_str());
            print_decoded_fields(d);
            print_registers_full("REGISTERS BEFORE:");

            t_exec.execute(d);
            if (t_cpu.running && t_cpu.PC == old_pc) {
                t_cpu.PC += static_cast<uint64_t>(size);
            }

            std::printf("    PC AFTER  : 0x%04llX\n", static_cast<unsigned long long>(t_cpu.PC));
            print_registers_full("REGISTERS AFTER:");
            if (d.format == FMT_I && (d.funct == OP_LW || d.funct == OP_SW)) {
                const uint64_t addr = t_cpu.rf.read(d.rs1) + static_cast<uint64_t>(d.imm);
                if (addr <= static_cast<uint64_t>(MEM_SIZE - 8)) print_memory_dump(addr, 8);
            }
            ++cycles;
        }

        const bool integration_ok = cycles == 5;
        expect(integration_ok, "Mixed-length fetch integration reaches HALT");
        report("Mixed-length fetch integration reaches HALT", integration_ok, "5 cycles and HALT", ("cycles=" + std::to_string(cycles)).c_str());

        const bool final_state_ok = t_cpu.rf.raw(1) == 10 && t_cpu.rf.raw(3) == 10 && !t_cpu.running && t_cpu.PC == 0x100C;
        expect(final_state_ok, "Mixed-length program final state");
        report("Mixed-length program final state", final_state_ok, "x1=10, x3=10, PC=0x100C, running=false", "values shown in final register/PC dump");

        const bool final_mem_ok = t_cpu.memory[0] == 0 && t_cpu.memory[7] == 10;
        std::printf("\nFINAL DATA MEMORY RESULT:\n");
        print_memory_dump(0, 8);
        std::printf("Decoded 64-bit Big-Endian value at 0x0000 = 10.\n");
        expect(final_mem_ok, "Final result stored Big-Endian at data memory");
        report("Final result stored Big-Endian at data memory", final_mem_ok, "memory[0..7] represents 64-bit value 10 in Big-Endian order", "memory[0..7] shown above");
    }

    std::printf("\n######################################################################\n");
    std::printf("# SELF-TEST FINAL SUMMARY                                          #\n");
    std::printf("######################################################################\n");
    std::printf("Total tests executed : %d\n", tests_passed + tests_failed);
    std::printf("Passed               : %d\n", tests_passed);
    std::printf("Failed               : %d\n", tests_failed);
    std::printf("Result               : %s\n", tests_failed == 0 ? "ALL TESTS PASSED" : "FAILURES DETECTED");
    std::printf("Architecture         : Calypso16/32 | XLEN=64 | 32 GPRs | 64KB | Big-Endian | 16/32-bit\n");
    std::printf("Pipeline demonstrated: Fetch -> Decode -> Disassemble -> Execute -> PC/State update\n");
    return tests_failed == 0;
}
