# Calypso16/32 Emulator

> A 64-bit variable-length instruction set architecture emulator written in C++17.  
> Implements CS, CI (16-bit compressed) and R, I (32-bit standard) formats with a full M-extension.

---

## Architecture at a Glance

| Property | Value |
|---|---|
| XLEN | 64 bits |
| Registers | 32 × 64-bit GPRs (x0–x31, x0 hardwired to 0) |
| Memory | 64 KiB byte-addressable, Big-Endian |
| Instruction sizes | 16-bit (CS, CI) and 32-bit (R, I) — mixed freely |
| Fetch rule | bit\[15\] of first halfword: `0` = 16-bit, `1` = 32-bit |
| Data region | 0x0000–0x0FFF (stores to 0x1000+ are rejected) |
| Program start | 0x1000 |
| HALT convention | `JAL x0, 0` |

---

## Class Architecture

```
┌──────────────────────────────────────────────────────────────────┐
│  CPUState                                                        │
│  ┌────────────────┐   memory[65536]   uint64_t PC   bool running │
│  │ RegisterFile   │                                              │
│  │  regs[32]      │   x0 always reads 0; writes to x0 discarded │
│  └────────────────┘                                              │
└──────────────────────────────────────────────────────────────────┘
        │                   │                    │
   Fetcher            Memory               Executor
   (reads raw     (read64/write64      (dispatches ALU,
    instruction)   with bounds +        memory, branch,
                   protection)          jump via Decoder)
                                              │
                                          Decoder
                                    (static decode())
```

| Class | Files | Responsibility |
|---|---|---|
| `RegisterFile` | `cpu_state.h` | 32 × 64-bit GPRs; x0 hardwired to zero |
| `CPUState` | `cpu_state.h` | Owns `RegisterFile`, `memory[65536]`, `PC`, `running` |
| `Memory` | `memory.h/.cpp` | Big-Endian 64-bit load/store with bounds & protection |
| `Decoder` | `decode.h/.cpp` | Static `decode(raw, size)` — all four formats |
| `Fetcher` | `fetch.h/.cpp` | Reads 16 or 32-bit instruction word at `cpu_.PC` |
| `Executor` | `execute.h/.cpp` | Executes one `Decoded` instruction |

---

## Instruction Set

### 16-bit formats

#### CS — Compressed Register-Register
```
[ 15:14 ] [ 13:10 ] [ 9:5 ] [ 4:0 ]
   00       funct4    rd/rs1   rs2
```

#### CI — Compressed Register-Immediate
```
[ 15:14 ] [ 13:11 ] [ 10:6 ] [ 5:0 ]
   01       funct3    rd/rs1   imm6
```

| funct | CS mnemonic | CI mnemonic | Operation |
|---|---|---|---|
| 0 | ADD | ADDI | `rd = rd OP imm/rs2` |
| 1 | SUB | ANDI | |
| 2 | AND | ORI  | |
| 3 | OR  | SLTI | |
| 4 | SLT | — | |
| 5 | MUL | — | |
| 6 | DIV | — | |
| 7 | REM | — | |

### 32-bit formats

#### R — Register-Register
```
[ 31:30 ] [ 29:26 ] [ 25:21 ] [ 20:16 ] [ 15:11 ] [ 10:0 ]
   10       funct4      rd       rs1        rs2     reserved
```

#### I — Register-Immediate / Memory / Control
```
[ 31:30 ] [ 29:26 ] [ 25:21 ] [ 20:16 ] [ 15:0 ]
   11       funct4    rd/rs2     rs1       imm16
```

| funct4 | R mnemonic | I mnemonic | Notes |
|---|---|---|---|
| 0 | ADD | ADDI | |
| 1 | SUB | ANDI | |
| 2 | AND | ORI | |
| 3 | OR | SLTI | |
| 4 | SLT | LW | 64-bit Big-Endian load |
| 5 | MUL | SW | 64-bit Big-Endian store |
| 6 | DIV | BEQ | div-by-zero → −1 |
| 7 | REM | JAL | rem-by-zero → rs1 |

For `SW` and `BEQ`, the `rd` field acts as `rs2` (second source register).

---

## Build

### One-liner (from the submission folder)

```bash
g++ -std=c++17 -Wall -Wextra -pedantic *.cpp -o calypso
```

### CMake

```bash
cmake -B build
cmake --build build
```

---

## Run

### Normal demo — shows only what changed each cycle

```bash
./calypso         # Linux / macOS
.\calypso.exe     # Windows
```

```
Calypso16/32 Emulator
============================================================
Program loaded at 0x1000  |  Memory: 64 KB  |  XLEN: 64

CYCLE 1   [0x1000 -> 0x1002]  C.ADDI x1, 5
  Changed registers:
    x1  : 0  ->  5  (0x0000000000000005)

CYCLE 2   [0x1002 -> 0x1004]  C.ADD x1, x1
  Changed registers:
    x1  : 5  ->  10  (0x000000000000000A)

CYCLE 3   [0x1004 -> 0x1008]  SW x1, 0(x0)
  (no register changes)
  Store : MEM[0x0000] <- x1 = 10

CYCLE 4   [0x1008 -> 0x100C]  LW x3, 0(x0)
  Changed registers:
    x3  : 0  ->  10  (0x000000000000000A)
  Load  : x3 <- MEM[0x0000] = 10

CYCLE 5   [0x100C -> 0x100C]  JAL x0, 0
  (no register changes)
  HALT  : emulator stopped (JAL x0, 0)

============================================================
Stopped after 5 cycles.
  PC      : 0x100C
  Running : false
  Non-zero registers:
    x1  = 10  (0x000000000000000A)
    x3  = 10  (0x000000000000000A)
```

### Full self-test suite (43 tests)

```bash
./calypso --self-test
.\calypso.exe --self-test     # Windows
```

```
...
Total tests executed : 43
Passed               : 43
Failed               : 0
Result               : ALL TESTS PASSED
```

The test suite is completely isolated from the demo — it has its own `CPUState`, `Memory`, `Fetcher`, and `Executor` instances. Every test prints a readable architectural trace (PC, raw encoding, decoded fields, expected vs actual, full register snapshot) before the PASS/FAIL verdict.

---

## Self-Test Coverage

| # | Test | What it verifies |
|---|---|---|
| 01–04 | Decode: CS / CI / R / I | Field extraction and sign extension |
| 05–08 | Disassembly: CS / CI / R / I | Human-readable output |
| 09–10 | x0 hardwired | Write discarded; read always 0 |
| 11–19 | Base ALU (R + I) | ADD, SUB, AND, OR, SLT, ADDI, ANDI, ORI, SLTI |
| 20–25 | Compressed ALU (CS + CI) | All 8 CS ops; C.ADDI, C.ANDI, C.ORI, C.SLTI |
| 26–30 | M-extension | MUL, DIV, REM; divide/rem-by-zero rules |
| 31–32 | Memory | Misaligned SW/LW round-trip; Big-Endian byte order |
| 33–35 | Memory protection | Program-region store rejected; out-of-bounds rejected |
| 36–37 | BEQ | Taken target; not-taken fall-through |
| 38 | JAL | Return address + jump target |
| 39–40 | Illegal instruction | Reserved funct detected; emulator halts |
| 41–43 | Full integration | Mixed 16/32-bit pipeline; final state; memory encoding |

---

## File Structure

```
calypso_submit/
├── cpu_state.h        RegisterFile + CPUState classes + architectural constants
├── decode.h / .cpp    Decoder class — CS, CI, R, I formats
├── disassembler.h/.cpp Disassembler class
├── execute.h / .cpp   Executor class
├── fetch.h / .cpp     Fetcher class
├── m_extension.h/.cpp MExtension — MUL, DIV, REM
├── memory.h / .cpp    Memory class — Big-Endian load/store + bounds checks
├── self_tests.h/.cpp  43 self-tests (isolated state)
├── main.cpp           Demo entry point
├── CMakeLists.txt     CMake build configuration
└── Calypso16_32.pdf   Complete reference manual (17 pages, unified format)
```

---

## Reference Manual

The PDF (`Calypso16_32.pdf`) is a 17-page unified reference covering:
- Full ISA field layouts and operation tables for all four formats
- Fetch, decode, execute, and PC-update rules
- Memory model, byte ordering, and protection rules
- Mixed-length execution demonstration (5-instruction program)
- 43-test verification matrix
- Build and run instructions
- Revision record

---

*Calypso16/32 — RISC-V inspired custom ISA emulator*
