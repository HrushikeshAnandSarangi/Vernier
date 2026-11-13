# Vernier

**LPTA (Vernier)** — track, measure, correlate, and bisect changes across the LLVM optimization pipeline.

C++17 · LLVM 18–20 · CMake + Ninja · Out-of-tree pass plugin & companion CLI

---

## What it does

LLVM already ships several diagnostic flags: `-time-passes` for timing,
`-print-changed=diff` for IR diffs, `-print-pipeline-passes` for pipeline
structure, and `-pass-remarks-missed=` for missed-optimization remarks. Each
is useful in isolation, but none of them correlate with each other — there is no
built-in way to ask:
- *"Which pass spent the most time changing hot-path code?"*
- *"Which passes claimed to preserve analyses but actually mutated IR, or invalidated analyses without touching anything?"*
- *"Where exactly did two optimization runs diverge?"*
- *"What did ablating a pass actually cost in the final machine code and binary size?"*

**Vernier** is an out-of-tree LLVM pass-manager instrumentation plugin (`vernier.so`) and companion analysis CLI (`vernier-tool`) that answers all of those questions. It attaches to `PassInstrumentationCallbacks`, observes every pass invocation across `opt` and `clang -fpass-plugin`, and produces correlated telemetry:

1. **Pass Tracing & Execution Tree**: records every pass invocation, IR unit (module, function, SCC, loop), and nesting depth across `opt` and `clang`.
2. **Change Detection & Analysis Auditing**: structural hashing of IR units before and after every pass detects real mutations vs. `PreservedAnalyses` under-reporting.
3. **Self & Total Timing**: monotonic frame stack subtracts nested adaptor durations (`ModuleToFunctionPassAdaptor`, `FunctionToLoopPassAdaptor`) so numbers reflect what the pass itself executed.
4. **Windowed LCS Unified Diffs**: common leading/trailing lines trimmed, bounded context, line cap fallback, avoiding whole-module diff blowups.
5. **Content-Addressed IR Snapshot Store**: function bodies stored once keyed by hash (`.store/objects/<hash>.ll`) and deduplicated across invocations.
6. **Multi-Dimensional Significance**: tracks structural deltas (instructions, blocks, CFG edges, loops), memory (loads, stores, allocas), call graph (calls removed, inlined), SIMD vector width, and TTI instruction cost weighted by Block Frequency Analysis (`BlockFrequencyAnalysis`).
7. **Missed-Optimization Remark Attribution**: custom `DiagnosticHandler` intercepts diagnostic remarks and maps them directly to the active pass on the frame stack.
8. **Codegen Impact by Ablation**: `vernier-tool ablate` re-compiles with candidate passes disabled (`VERNIER_DISABLE_PASS=<PassID>`) and measures delta binary size and machine instruction count using `llvm-objdump`.
9. **Sequence Alignment & Divergence Bisect**: `vernier-tool compare` runs Needleman-Wunsch global alignment over two pass sequences and pinpoints the first IR hash divergence point.
10. **Rich Formatted Plain-Text Report**: `vernier-tool report` formats the JSON into an executive summary, ASCII activity bar hotspots, significance rankings, hierarchical execution trees, remarks, and unified diffs.

---

## Building

### Prerequisites
- LLVM 18, 19, or 20 (headers and libraries)
- Clang
- CMake (>= 3.20) and Ninja
- C++17 compatible compiler
- Linux or Ubuntu on WSL

### Build Instructions
```bash
# Set path to LLVM if using non-default install (e.g. LLVM 18/20)
export PATH=/usr/lib/llvm-18/bin:$PATH

cmake -G Ninja -B build -DCMAKE_BUILD_TYPE=Release -DLLVM_DIR=$(llvm-config --cmakedir)
cmake --build build
```

This builds:
- `build/vernier.so` — The LLVM pass instrumentation plugin.
- `build/vernier-tool` — The companion analysis and reporting CLI.
- `build/test_diff`, `build/test_compare`, `build/test_report` — Test executables.

---

## Usage

### 1. Instrumentation Plugin (`vernier.so`)

Load `vernier.so` into `opt` or `clang`:

**Via `opt`:**
```bash
opt -load-pass-plugin=./build/vernier.so -passes='default<O2>' \
    -vn-diff -vn-remarks -vn-out=run.vernier.json input.ll -disable-output
```

**Via `clang`:**
```bash
clang -fpass-plugin=./build/vernier.so -O2 -c \
    -mllvm -vn-diff -mllvm -vn-remarks -mllvm -vn-out=run.vernier.json input.c -o output.o
```

#### Plugin Flags & Options:
| Flag | Description |
|---|---|
| `-vn-diff` | Emit windowed LCS unified diffs for all mutated IR units |
| `-vn-remarks` | Capture and attribute missed-optimization remarks to the active pass |
| `-vn-out=<path>` | Specify destination JSON path (default: `<input>.vernier.json`) |
| `-vn-disable-pass=<ID>` | Bypass a specific optional pass (or via `VERNIER_DISABLE_PASS=<ID>`) |

---

### 2. Formatted Plain-Text Report (`vernier-tool report`)

Generate a human-readable text profile report with ASCII activity bars, hotspots, significance tables, and unified diffs:

```bash
# Print to stdout:
./build/vernier-tool report run.vernier.json

# Save to file:
./build/vernier-tool report run.vernier.json --out run.vernier.txt
```

Example report snippet:
```
================================================================================
                        VERNIER PIPELINE PROFILE REPORT                         
================================================================================
Input Source      : examples/loop_vectorize.c
Target Triple     : x86_64-unknown-linux-gnu
Total Wall Time   : 14.82 ms (12.45 ms accounted self time)
Pass Invocations  : 148 total (IR Mutated: 38, Under-reported PA: 15)
Attributed Remarks: 1

--------------------------------------------------------------------------------
1. TOP HOTSPOTS (BY PASS SELF TIME)
--------------------------------------------------------------------------------
Rank  Self Time    % Total    Activity Bar      Pass ID (Target)
--------------------------------------------------------------------------------
  1.     3.24 ms    26.0%   [####...........]   PassManager<Function> (vector_add [function])
  2.     1.85 ms    14.9%   [##.............]   SimplifyCFGPass (vector_add [function])
  3.     1.42 ms    11.4%   [##.............]   LoopVectorizePass (vector_add [function])

--------------------------------------------------------------------------------
2. OPTIMIZATION SIGNIFICANCE (TOP IMPACT PASSES)
--------------------------------------------------------------------------------
Pass ID                    Target               dInst  dBB  dMem  dCall  Vec  TTI Cost
--------------------------------------------------------------------------------
LoopVectorizePass         vector_add             +34   +4    +6     +2    4       +22
SROAPass                  vector_add             -19    0   -20      0    1       -12
InstCombinePass           vector_add              -9    0     0      0    4        -2
...
```

---

### 3. Pipeline Divergence Bisect (`vernier-tool compare`)

Align two pipeline runs (e.g. `-O1` vs `-O2`, LLVM versions, or code edits) with Needleman-Wunsch sequence alignment and identify the exact pass where IR hashes diverge:

```bash
./build/vernier-tool compare run_O1.vernier.json run_O2.vernier.json --out divergence.txt
```

Output highlights:
```
=================================================================
               VERNIER PIPELINE DIVERGENCE REPORT                
=================================================================
Run A: run_O1.vernier.json (128 invocations)
Run B: run_O2.vernier.json (148 invocations)
Alignment Length: 148 steps (Exact Pass Matches: 126, Gaps: A=20, B=0)

-----------------------------------------------------------------
                      FIRST DIVERGENCE POINT                     
-----------------------------------------------------------------
Alignment Step   : #66
Pass ID          : MemCpyOptPass
IR Unit          : sum (function)

Run A State:
  Invocation ID  : #63
  IR Hash (After): a9539ecb11c7c812
  Self Time      : 278466 ns
  Delta Insts    : +0

Run B State:
  Invocation ID  : #72
  IR Hash (After): 350b4994a6f09ca8
  Self Time      : 885818 ns
  Delta Insts    : -1

Timing Delta (B - A): +607352 ns
=================================================================
```

---

### 4. Codegen Ablation Driver (`vernier-tool ablate`)

Automatically measure what individual passes bought in the final binary by re-compiling with each pass disabled and measuring object size and machine instruction deltas:

```bash
./build/vernier-tool ablate --json run.vernier.json --top 5 --cmd "clang -O2 input.c -c"
```

Appends the ablation results directly into the JSON and reports:
```
Pass ID                    Obj Size (B)   Delta (B)   Mach Insts   Delta Insts
--------------------------------------------------------------------------------
(Baseline)                         1496          +0           41           +0
LoopVectorizePass                  1824        +328           56          +15
SimplifyCFGPass                    1520         +24           44           +3
```

---

## Testing

Run the full automated CTest suite (LCS diff test, Needleman-Wunsch compare test, report generator test, and end-to-end plugin smoke test):

```bash
ctest --test-dir build --output-on-failure
```

---

## Repository Layout

```
CMakeLists.txt            CMake project configuration and CTest definitions
include/Vernier/          Public headers
  Diff.h                  Windowed LCS unified diff algorithm
  Hash.h                  Structural IR unit hashing
  JSON.h                  Versioned schema JSON serialization
  Remarks.h               Diagnostic remark interception
  Schema.h                Pure C++ schema types (vernier/v1)
  Session.h               Thread-safe frame stack, monotonic timing, child subtraction
  Significance.h          Multi-dimensional significance vector & TTI/BFI query
  Store.h                 Content-addressed IR snapshot store
  Trace.h                 PassInstrumentationCallbacks registration
lib/                      Plugin implementation files
tools/vernier-tool/       Companion CLI tool
  ablate.h / ablate.cpp   Pass ablation and binary diff driver
  compare.h / compare.cpp Needleman-Wunsch sequence alignment and divergence bisect
  report.h / report.cpp   Rich plain-text profile report generator
  main.cpp                CLI entry point and command-line parsing
examples/                 Sample C source files and generated Vernier profile reports
test/                     Unit tests and end-to-end integration smoke tests
```

---

## License

Apache-2.0 WITH LLVM-exception.
