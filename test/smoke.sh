#!/usr/bin/env bash
set -euo pipefail

PLUGIN_PATH="$1"
TOOL_PATH="$2"
SOURCE_DIR="$3"

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "Vernier Smoke Test: Testing plugin at $PLUGIN_PATH..."

# 1. Compile example to LLVM bitcode/IR
clang -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$SOURCE_DIR/test/example.c" -o "$TMPDIR/input.ll"

# 2. Run opt with vernier plugin
opt -load-pass-plugin="$PLUGIN_PATH" -passes='default<O2>' \
    -vn-diff -vn-remarks -vn-out="$TMPDIR/run.vernier.json" \
    "$TMPDIR/input.ll" -disable-output

# 3. Check that JSON was produced and is valid
test -f "$TMPDIR/run.vernier.json"

# 4. Run vernier-tool report
"$TOOL_PATH" report "$TMPDIR/run.vernier.json" --out "$TMPDIR/report.txt"
test -f "$TMPDIR/report.txt"
grep -q "VERNIER PIPELINE PROFILE REPORT" "$TMPDIR/report.txt"

# 5. Run vernier-tool ablate
"$TOOL_PATH" ablate --json "$TMPDIR/run.vernier.json" --top 2 --cmd "clang -O2 $SOURCE_DIR/test/example.c -c"

echo "Vernier Smoke Test: PASSED!"
