#!/usr/bin/env bash
# Build the browser host: build-wasm/lmplot.js + lmplot.wasm (Emscripten, pinned below).
# EMSDK=/path/to/emsdk reuses an installation; otherwise emsdk is cloned into third_party/emsdk.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
EMSDK_VERSION=7.0.0
EMSDK="${EMSDK:-$ROOT/third_party/emsdk}"
OUT="${WASM_BUILD_DIR:-$ROOT/build-wasm}"
if [ ! -x "$EMSDK/emsdk" ]; then
  git clone --depth 1 https://github.com/emscripten-core/emsdk.git "$EMSDK"
fi
"$EMSDK/emsdk" install "$EMSDK_VERSION" >/dev/null
"$EMSDK/emsdk" activate "$EMSDK_VERSION" >/dev/null
export EMSDK_QUIET=1
# shellcheck disable=SC1091
. "$EMSDK/emsdk_env.sh" >/dev/null
[ -f "$ROOT/third_party/gnuplot/src/plot.c" ] || "$ROOT/scripts/sync-gnuplot.sh"
GEN=""; command -v ninja >/dev/null 2>&1 && GEN="-G Ninja"
# shellcheck disable=SC2086
emcmake cmake -S "$ROOT" -B "$OUT" $GEN -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$OUT" --parallel "${JOBS:-4}" --target lmplot
wc -c "$OUT/lmplot.js" "$OUT/lmplot.wasm"
