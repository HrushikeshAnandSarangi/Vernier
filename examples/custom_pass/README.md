# Example Custom Pass Plugin (`custom_transform.so`)

This directory contains a standalone out-of-tree LLVM Pass Plugin that demonstrates how **Vernier** observes, traces, and diffs custom 3rd-party optimization and transformation passes.

## Why Source Code Instead of a Pre-Compiled `.so`?
Pre-compiled `.so` (shared object) binaries are **never** committed to version control because:
1. **ABI Incompatibility**: An ELF binary compiled against LLVM 18 on Ubuntu 24.04 cannot load on LLVM 19/20 or other architectures (x86_64 vs. ARM64, glibc versions, macOS Mach-O, Windows DLL).
2. **Security & Reproducibility**: Binary blobs bypass code review and trigger security scanners.
3. **Portability**: Compiling from source guarantees the pass matches your exact host LLVM installation.

---

## How It Works

`CustomTransform.cpp` implements a function pass named `custom-transform` that transforms integer additions with constants into canonical subtractions:
$$x + C \implies x - (-C)$$

When run through `opt` alongside Vernier, Vernier instruments the custom pass alongside LLVM's standard passes.

---

## Building and Running

1. **Build the custom pass**:
   ```bash
   cmake --build build --target custom_transform
   ```
   This generates `build/examples/custom_pass/custom_transform.so`.

2. **Run with Vernier in `opt`**:
   ```bash
   # Compile input to IR
   clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S examples/inlining.c -o /tmp/inlining.ll

   # Run opt with both Vernier and CustomTransform plugins loaded:
   opt -load-pass-plugin=./build/vernier.so \
       -load-pass-plugin=./build/examples/custom_pass/custom_transform.so \
       -passes='function(custom-transform),default<O2>' \
       -vn-diff \
       -vn-out=custom_run.vernier.json \
       /tmp/inlining.ll -disable-output

   # View Vernier's profile and diff of the custom pass:
   ./build/vernier-tool report custom_run.vernier.json
   ```

Vernier will attribute self-time, compute the structural significance vector (`dInst: +1`), detect the IR hash divergence, and print the exact unified diff produced by `CustomTransformPass`.
