#ifndef SELF_TESTS_H
#define SELF_TESTS_H

// ============================================================
// Calypso16/32 complete self-test suite (43 tests).
// Covers: decode, disassembly, x0 hardwiring, all ALU ops,
// M-extension, memory, data/program separation, BEQ, JAL,
// illegal instructions, and full mixed-length integration.
//
// Returns true when every test passes, false otherwise.
// ============================================================
bool run_self_tests();

#endif
