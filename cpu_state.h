#ifndef CPU_STATE_H
#define CPU_STATE_H

#include <cstdint>
#include <cstring>

// ============================================================
// Calypso16/32 Architectural Constants
// ============================================================
static constexpr int      XLEN           = 64;
static constexpr int      NUM_REGS       = 32;
static constexpr int      MEM_SIZE       = 65536;      // 64 KiB
static constexpr uint64_t PROGRAM_START  = 0x1000;    // first program byte
static constexpr uint64_t DATA_END       = PROGRAM_START - 1; // 0x0FFF
static constexpr int      WORD_BYTES     = 8;          // LW/SW move 64 bits
static constexpr int      MAX_CYCLES     = 1000;       // execution backstop

// ============================================================
// RegisterFile — 32 x 64-bit general-purpose registers.
// x0 is hardwired to zero: write() silently discards writes to
// index 0 and read() always returns 0 for index 0.
// ============================================================
class RegisterFile {
public:
    RegisterFile() { std::memset(regs_, 0, sizeof(regs_)); }

    // Architectural write — x0 writes are silently discarded.
    void write(int idx, uint64_t value) {
        if (idx != 0) {
            regs_[idx] = value;
        }
    }

    // Architectural read — x0 always reads as 0.
    uint64_t read(int idx) const {
        return (idx == 0) ? 0ULL : regs_[idx];
    }

    // Raw storage access needed by legacy test helpers that poke/peek
    // register physical storage directly (x0 hardwire bypassed).
    uint64_t& raw(int idx)             { return regs_[idx]; }
    uint64_t  raw(int idx) const       { return regs_[idx]; }

private:
    uint64_t regs_[NUM_REGS];
};

// ============================================================
// CPUState — owns all mutable architectural state.
// ============================================================
class CPUState {
public:
    CPUState() {
        std::memset(memory, 0, sizeof(memory));
        PC      = PROGRAM_START;
        running = false;
    }

    void reset() {
        rf      = RegisterFile{};
        std::memset(memory, 0, sizeof(memory));
        PC      = PROGRAM_START;
        running = true;
    }

    RegisterFile rf;
    uint8_t      memory[MEM_SIZE];
    uint64_t     PC;
    bool         running;
};

#endif
