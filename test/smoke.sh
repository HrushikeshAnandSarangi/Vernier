#!/usr/bin/env bash
set -euo pipefail

PLUGIN_PATH="$1"
TOOL_PATH="$2"
SOURCE_DIR="$3"
CLANG_BIN="${4:-clang}"
OPT_BIN="${5:-opt}"

# Fallback resolution if not found directly
if ! command -v "$OPT_BIN" &>/dev/null; then
  for candidate in /usr/lib/llvm-*/bin/opt /usr/bin/opt-*; do
    if [ -x "$candidate" ]; then
      OPT_BIN="$candidate"
      break
    fi
  done
fi

if ! command -v "$CLANG_BIN" &>/dev/null; then
  for candidate in /usr/lib/llvm-*/bin/clang /usr/bin/clang-*; do
    if [ -x "$candidate" ]; then
      CLANG_BIN="$candidate"
      break
    fi
  done
fi

TMPDIR=$(mktemp -d)
trap 'rm -rf "$TMPDIR"' EXIT

echo "Vernier Smoke Test: Testing plugin at $PLUGIN_PATH..."
echo "Using Clang: $CLANG_BIN"
echo "Using Opt:   $OPT_BIN"

# 1. Compile example to LLVM bitcode/IR
"$CLANG_BIN" -O0 -Xclang -disable-O0-optnone -emit-llvm -S "$SOURCE_DIR/test/example.c" -o "$TMPDIR/input.ll"

# 2. Run opt with vernier plugin
"$OPT_BIN" -load-pass-plugin="$PLUGIN_PATH" -passes='default<O2>' \
    -vn-diff -vn-remarks -vn-out="$TMPDIR/run.vernier.json" \
    "$TMPDIR/input.ll" -disable-output

# 3. Check that JSON was produced and is valid
test -f "$TMPDIR/run.vernier.json"

# 4. Run vernier-tool report
"$TOOL_PATH" report "$TMPDIR/run.vernier.json" --out "$TMPDIR/report.txt"
test -f "$TMPDIR/report.txt"
grep -q "VERNIER PIPELINE PROFILE REPORT" "$TMPDIR/report.txt"

# 5. Run vernier-tool ablate
"$TOOL_PATH" ablate --json "$TMPDIR/run.vernier.json" --top 2 --cmd "$CLANG_BIN -O2 $SOURCE_DIR/test/example.c -c"

echo "Vernier Smoke Test: PASSED!"
