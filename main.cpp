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
#include "self_tests.h"

// ============================================================
// Architectural state for the main (demo) program
// ============================================================
static CPUState cpu;
static Memory   mem(cpu);
static Fetcher  fetcher(cpu);
static Executor exec(cpu, mem);

// ============================================================
// ISA encoding helpers — used to build the demo program
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

static uint32_t encI(int funct4, int rd_or_rs2, int rs1, int imm16) {
    return (0b11u << 30) |
           ((static_cast<uint32_t>(funct4)    & 0xFu)  << 26) |
           ((static_cast<uint32_t>(rd_or_rs2) & 0x1Fu) << 21) |
           ((static_cast<uint32_t>(rs1)       & 0x1Fu) << 16) |
           (static_cast<uint32_t>(imm16) & 0xFFFFu);
}

// ============================================================
// Big-Endian memory write helpers (program loader)
// ============================================================
static void write16BE(uint64_t addr, uint16_t value) {
    cpu.memory[addr]     = static_cast<uint8_t>(value >> 8);
    cpu.memory[addr + 1] = static_cast<uint8_t>(value);
}

static void write32BE(uint64_t addr, uint32_t value) {
    cpu.memory[addr]     = static_cast<uint8_t>(value >> 24);
    cpu.memory[addr + 1] = static_cast<uint8_t>(value >> 16);
    cpu.memory[addr + 2] = static_cast<uint8_t>(value >> 8);
    cpu.memory[addr + 3] = static_cast<uint8_t>(value);
}

// ============================================================
// Demo program loader
// ============================================================
static void load_demo_program() {
    // --- Program listing ---
    //   0x1000  C.ADDI  x1, 5      ; x1 = 5
    //   0x1002  C.ADD   x1, x1     ; x1 = 10
    //   0x1004  SW      x1, 0(x0)  ; mem[0x0000] = 10
    //   0x1008  LW      x3, 0(x0)  ; x3 = mem[0x0000] = 10
    //   0x100C  JAL     x0, 0      ; HALT
    write16BE(PROGRAM_START + 0,  encCI(ALU_ADDI, 1, 5));
    write16BE(PROGRAM_START + 2,  encCS(ALU_ADD, 1, 1));
    write32BE(PROGRAM_START + 4,  encI(OP_SW, 1, 0, 0));
    write32BE(PROGRAM_START + 8,  encI(OP_LW, 3, 0, 0));
    write32BE(PROGRAM_START + 12, encI(OP_JAL, 0, 0, 0));
}

// ============================================================
// Changed-register reporter
// Print only registers whose value changed since the snapshot.
// ============================================================
static void print_changed_registers(const uint64_t* before) {
    bool any = false;
    for (int i = 1; i < NUM_REGS; ++i) {   // skip x0 (always 0)
        const uint64_t now = cpu.rf.read(i);
        if (now != before[i]) {
            if (!any) std::printf("  Changed registers:\n");
            std::printf("    x%-2d : %llu  ->  %llu  (0x%016llX)\n",
                        i,
                        static_cast<unsigned long long>(before[i]),
                        static_cast<unsigned long long>(now),
                        static_cast<unsigned long long>(now));
            any = true;
        }
    }
    if (!any) std::printf("  (no register changes)\n");
}

// ============================================================
// Main emulator loop
// ============================================================
static int run_demo() {
    cpu.reset();
    load_demo_program();

    std::printf("Calypso16/32 Emulator\n");
    std::printf("============================================================\n");
    std::printf("Program loaded at 0x%04llX  |  Memory: %d KB  |  XLEN: %d\n\n",
                static_cast<unsigned long long>(PROGRAM_START),
                MEM_SIZE / 1024, XLEN);

    int cycle = 0;
    while (cpu.running && cycle < MAX_CYCLES) {
        // Snapshot all register values before this instruction.
        uint64_t regs_before[NUM_REGS];
        for (int i = 0; i < NUM_REGS; ++i) {
            regs_before[i] = cpu.rf.read(i);
        }
        const uint64_t pc_before = cpu.PC;

        // ── 1. Fetch ──────────────────────────────────────────────
        uint32_t raw_inst = 0;
        int      inst_size = 0;
        if (!fetcher.fetch(raw_inst, inst_size)) {
            std::printf("PC 0x%04llX is out of the fetch window — halting.\n",
                        static_cast<unsigned long long>(cpu.PC));
            cpu.running = false;
            break;
        }

        // ── 2. Decode ─────────────────────────────────────────────
        const Decoded d = Decoder::decode(raw_inst, inst_size);
        if (d.illegal) {
            std::printf("Illegal instruction 0x%08X at PC=0x%04llX — halting.\n",
                        raw_inst, static_cast<unsigned long long>(cpu.PC));
            cpu.running = false;
            break;
        }

        const std::string mnemonic = Disassembler::disassemble(d);

        // ── 3. Execute ────────────────────────────────────────────
        exec.execute(d);

        // ── 4. PC advance (if Execute did not redirect / halt) ────
        if (cpu.running && cpu.PC == pc_before) {
            cpu.PC += static_cast<uint64_t>(inst_size);
        }

        ++cycle;

        // ── 5. Per-cycle output ───────────────────────────────────
        std::printf("CYCLE %-2d  [0x%04llX -> 0x%04llX]  %s\n",
                    cycle,
                    static_cast<unsigned long long>(pc_before),
                    static_cast<unsigned long long>(cpu.PC),
                    mnemonic.c_str());

        // Register changes
        print_changed_registers(regs_before);

        // Memory activity for load / store instructions
        if (d.format == FMT_I && d.funct == OP_SW) {
            const uint64_t addr = regs_before[d.rs1] + static_cast<uint64_t>(d.imm);
            std::printf("  Store : MEM[0x%04llX] <- x%d = %llu\n",
                        static_cast<unsigned long long>(addr),
                        d.rd,
                        static_cast<unsigned long long>(regs_before[d.rd]));
        }
        if (d.format == FMT_I && d.funct == OP_LW) {
            const uint64_t addr = regs_before[d.rs1] + static_cast<uint64_t>(d.imm);
            std::printf("  Load  : x%d <- MEM[0x%04llX] = %llu\n",
                        d.rd,
                        static_cast<unsigned long long>(addr),
                        static_cast<unsigned long long>(cpu.rf.read(d.rd)));
        }

        // Halt notice
        if (!cpu.running) {
            std::printf("  HALT  : emulator stopped (JAL x0, 0)\n");
        }

        std::printf("\n");
    }

    // ── Final summary ─────────────────────────────────────────────
    std::printf("============================================================\n");
    std::printf("Stopped after %d cycle%s.\n", cycle, cycle == 1 ? "" : "s");
    std::printf("  PC      : 0x%04llX\n",  static_cast<unsigned long long>(cpu.PC));
    std::printf("  Running : %s\n",         cpu.running ? "true" : "false");

    // Print non-zero registers only
    std::printf("  Non-zero registers:\n");
    bool any = false;
    for (int i = 1; i < NUM_REGS; ++i) {
        if (cpu.rf.read(i) != 0) {
            std::printf("    x%-2d = %llu  (0x%016llX)\n",
                        i,
                        static_cast<unsigned long long>(cpu.rf.read(i)),
                        static_cast<unsigned long long>(cpu.rf.read(i)));
            any = true;
        }
    }
    if (!any) std::printf("    (all zero)\n");

    return cpu.running ? 1 : 0;
}

// ============================================================
// Entry point
// ============================================================
int main(int argc, char** argv) {
    if (argc > 1 && std::string(argv[1]) == "--self-test") {
        return run_self_tests() ? 0 : 1;
    }
    return run_demo();
}
