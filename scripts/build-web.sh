#!/usr/bin/env bash
# Build the browser demo end to end: wasm host, precompiled examples, wasm test run, static site.
#   scripts/build-web.sh [OUT_DIR]      (default build-site/)
# Needs the native build (build/lmplot) for precompiling, lmscript on PATH, node, python3 + Pillow.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:-$ROOT/build-site}"
WORK="${WEB_WORK_DIR:-$ROOT/build-web-work}"
"$ROOT/scripts/build-wasm.sh"
rm -rf "$WORK"; mkdir -p "$WORK"
"$ROOT/scripts/precompile-examples.sh" "$WORK/pre" >/dev/null
node "$ROOT/tests/wasm-examples.mjs" "$ROOT/build-wasm" "$WORK/pre" "$WORK/renders" | tail -2
python3 "$ROOT/scripts/build-site.py" --wasm "$ROOT/build-wasm" --pre "$WORK/pre" --renders "$WORK/renders/png" --out "$OUT"
