#!/usr/bin/env python3
import argparse
import os
import subprocess
import statistics
import time

def run_command(cmd, iterations=5):
    timings = []
    for _ in range(iterations):
        start = time.perf_counter()
        res = subprocess.run(cmd, shell=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if res.returncode != 0:
            return None
        timings.append((time.perf_counter() - start) * 1000.0) # ms
    return {
        "mean": statistics.mean(timings),
        "stdev": statistics.stdev(timings) if len(timings) > 1 else 0.0,
        "min": min(timings),
        "max": max(timings),
    }

def main():
    parser = argparse.ArgumentParser(description="Benchmark Vernier plugin compilation overhead")
    parser.add_argument("input_file", help="Path to C/C++ source file or LLVM IR file")
    parser.add_argument("--runs", "-n", type=int, default=5, help="Number of benchmark iterations")
    parser.add_argument("--plugin", default="./build/vernier.so", help="Path to vernier.so")
    parser.add_argument("--opt-level", default="-O2", help="Optimization level (default: -O2)")
    args = parser.parse_args()

    if not os.path.exists(args.input_file):
        print(f"Error: input file '{args.input_file}' not found.")
        return 1

    clang_bin = "clang"
    for candidate in ["clang", "/usr/lib/llvm-18/bin/clang", "/usr/bin/clang-18"]:
        if subprocess.run(f"which {candidate}", shell=True, stdout=subprocess.DEVNULL).returncode == 0:
            clang_bin = candidate
            break

    print("=" * 80)
    print("                  VERNIER PLUGIN OVERHEAD BENCHMARK")
    print("=" * 80)
    print(f"Target Source : {args.input_file}")
    print(f"Compiler      : {clang_bin} {args.opt_level}")
    print(f"Iterations    : {args.runs} runs per configuration")
    print(f"Plugin Path   : {args.plugin}")
    print("-" * 80)

    configs = [
        ("1. Uninstrumented Baseline",
         f"{clang_bin} {args.opt_level} -c {args.input_file} -o /dev/null"),
        ("2. Vernier (Tracing & Hashing)",
         f"{clang_bin} -fpass-plugin={args.plugin} {args.opt_level} -c {args.input_file} -o /dev/null"),
        ("3. Vernier + Diffs (VERNIER_DIFF=1)",
         f"VERNIER_DIFF=1 {clang_bin} -fpass-plugin={args.plugin} {args.opt_level} -c {args.input_file} -o /dev/null"),
        ("4. Vernier Full (Diffs + Remarks)",
         f"VERNIER_DIFF=1 VERNIER_REMARKS=1 {clang_bin} -fpass-plugin={args.plugin} {args.opt_level} -c {args.input_file} -o /dev/null"),
    ]

    results = []
    baseline_mean = None

    for name, cmd in configs:
        stats = run_command(cmd, args.runs)
        if stats is None:
            print(f"Failed to run configuration: {name}")
            continue
        if baseline_mean is None:
            baseline_mean = stats["mean"]
            overhead_str = "    -- (Baseline)"
        else:
            diff_pct = ((stats["mean"] - baseline_mean) / baseline_mean) * 100.0
            overhead_str = f"{diff_pct:+6.2f}% overhead"

        results.append((name, stats, overhead_str))

    print(f"{'Configuration':<42} {'Mean (ms)':>10} {'StdDev':>10} {'Relative Overhead':>20}")
    print("-" * 84)
    for name, stats, overhead in results:
        print(f"{name:<42} {stats['mean']:>10.2f} {stats['stdev']:>10.2f} {overhead:>20}")
    print("=" * 84)

if __name__ == "__main__":
    main()
