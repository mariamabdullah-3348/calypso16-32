# Calypso16/32 Submission Checklist

## Requirements from the course reference manual

- [x] 64-bit XLEN
- [x] 32 GPRs (x0-x31)
- [x] x0 hardwired to zero (`RegisterFile::write()` enforces this)
- [x] 64 KiB byte-addressable memory (`CPUState::memory[65536]`)
- [x] Big-Endian byte order
- [x] Misaligned memory access allowed
- [x] Variable 16/32-bit instruction length
- [x] Four formats: CS, CI, R, I
- [x] Base I: ADD / ADDI / SUB / AND / ANDI / OR / ORI / SLT / SLTI
- [x] Base I: LW / SW / BEQ / JAL
- [x] M: MUL / DIV / REM
- [x] DIV-by-zero → −1 / all ones
- [x] REM-by-zero → rs1
- [x] Fetch stage (`Fetcher` class in `fetch.h/.cpp`)
- [x] Decode stage (`Decoder` class in `decode.h/.cpp`)
- [x] Real-time disassembly (`Disassembler` class in `disassembler.h/.cpp`)
- [x] Execute stage (`Executor` class in `execute.h/.cpp`)
- [x] Instruction-size-dependent PC update
- [x] BEQ target update
- [x] JAL return address and target update
- [x] JAL x0,0 halt convention
- [x] Explicit out-of-bounds checks (in `Memory` class)
- [x] Built-in self-test suite (43 tests in `self_tests.h/.cpp`)

## OOP class architecture

- [x] `RegisterFile` — 32 × 64-bit GPRs; x0 always reads 0; writes to x0 silently discarded.
- [x] `CPUState` — owns `RegisterFile`, `memory[65536]`, `PC`, `running`; `reset()` clears all state.
- [x] `Memory` — holds `CPUState&`; `read64` / `write64` replace old free functions.
- [x] `Decoder` — stateless; `Decoder::decode(raw, size)` is a static method.
- [x] `Fetcher` — holds `CPUState&`; reads 16/32-bit Big-Endian instruction words.
- [x] `Executor` — holds `CPUState&` + `Memory&`; `execute(Decoded)` dispatches ALU/mem/branch.
- [x] `self_tests.cpp` — completely isolated state (`t_cpu`, `t_mem`, `t_fetcher`, `t_exec`).

## Additional completion work

- [x] Completed and documented the missing 32-bit R format.
- [x] Completed and documented the missing 32-bit I format.
- [x] Added safe signed DIV/REM handling for INT64_MIN / −1 (avoids C++ UB).
- [x] `main.cpp` simplified: shows only changed registers per cycle + non-zero summary.
- [x] Self-tests moved to `self_tests.cpp`; `main.cpp` and test suite are completely independent.
- [x] Added `fetch.h/.cpp` and `self_tests.h/.cpp` to the source package.
- [x] Updated `CMakeLists.txt` with all 8 source files.
- [x] Updated `README.md` to reflect OOP architecture, new file list, and demo output.
- [x] Updated PDF (`Calypso16_32_unified.pdf`): pages 10–17 reformatted to match pages 1–9
      (Times New Roman, same heading sizes/colors, consistent header/footer).
- [x] Verified clean build with `-std=c++17 -Wall -Wextra -pedantic`.
- [x] Verified 43 built-in tests pass.

## Important architectural note

The course reference manual explicitly requires four instruction formats but does not itself provide
a full R/I field table. The supplied team code already used the following 32-bit scheme, and this
completed package preserves that team scheme rather than inventing a different one:

R: `10 | funct4 | rd | rs1 | rs2 | reserved`

I: `11 | funct4 | rd/rs2 | rs1 | imm16`

I funct4 0–3 are ADDI/ANDI/ORI/SLTI, and 4–7 are LW/SW/BEQ/JAL.
