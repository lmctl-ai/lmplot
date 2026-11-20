#!/usr/bin/env bash
# Precompile every standalone example to lmcode bytecode for the browser host.
# Each example runs once natively through `lmscript` with a shim named lmplot first on PATH; the shim keeps
# the private program file the compiler hands the host (`${...}` captures are already evaluated), then runs
# the native host so the reference PNGs are rendered too.
#   OUT/<name>.dslb   bytecode of the example's _lmcode_ block (or <name>.<k>.dslb for several runs)
#   OUT/<name>.lms    the source, for display
#   OUT/ref/<name>/   native reference images
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="${1:?usage: precompile-examples.sh OUT_DIR}"
HOST="${LMPLOT_HOST:-$ROOT/build/lmplot}"
LMSCRIPT="${LMSCRIPT_BIN:-$(command -v lmscript)}"
mkdir -p "$OUT"; OUT="$(cd "$OUT" && pwd)"
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lmplot-precompile-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/shim"
cat >"$WORK/shim/lmplot" <<SHIM
#!/bin/sh
[ "\${1:-}" = --lmcode ] || exec "$HOST" "\$@"
for a in "\$@"; do case "\$a" in --*) ;; *) n=\$(ls "$WORK/progs" | wc -l); cp "\$a" "$WORK/progs/\$n.dslb";; esac; done
exec "$HOST" "\$@"
SHIM
chmod +x "$WORK/shim/lmplot"
for src in "$ROOT"/examples/*.lms; do
  name="$(basename "$src" .lms)"
  run="$WORK/run/$name"; rm -rf "$run" "$WORK/progs"; mkdir -p "$run/examples" "$WORK/progs"
  cp -r "$ROOT/examples/data" "$run/examples/data"
  ( cd "$run" && PATH="$WORK/shim:$PATH" "$LMSCRIPT" "$src" >"$run/stdout.txt" 2>"$run/stderr.txt" ) ||
    { echo "FAIL $name" >&2; cat "$run/stderr.txt" >&2; exit 1; }
  count=$(ls "$WORK/progs" | wc -l)
  [ "$count" -ge 1 ] || { echo "FAIL $name: no lmplot program was run" >&2; exit 1; }
  if [ "$count" = 1 ]; then cp "$WORK/progs/0.dslb" "$OUT/$name.dslb"
  else for k in $(seq 0 $((count - 1))); do cp "$WORK/progs/$k.dslb" "$OUT/$name.$k.dslb"; done; fi
  cp "$src" "$OUT/$name.lms"
  mkdir -p "$OUT/ref/$name"; cp "$run"/images/* "$OUT/ref/$name/" 2>/dev/null || true
  echo "ok $name ($count program(s), $(ls "$OUT/ref/$name" | wc -l) image(s))"
done
