#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build}"
HOST="$BUILD_DIR/lmplot"
UNAME_S="$(uname -s)"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lmplot-test-XXXXXX")"
trap 'rm -rf "$WORK"' EXIT
ROOT="$HERE"
# Native Windows tools (node, the host) cannot address git-bash /tmp or /d/a paths.
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) WORK="$(cd "$WORK" && pwd -W | tr '\\' '/')"; ROOT="$(pwd -W | tr '\\' '/')";; esac
pass=0
ok(){ pass=$((pass+1)); echo "ok $pass - $1"; }
fail(){ echo "FAIL: $*" >&2; exit 1; }
sha(){ if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1" | cut -c1-64; else shasum -a 256 "$1" | cut -c1-64; fi; }
# PNG header: signature + IHDR width/height (big-endian u32 at offsets 16 and 20).
png_size(){
  local sig; sig="$(head -c 8 "$1" | od -An -tx1 | tr -d ' \n')"
  [ "$sig" = "89504e470d0a1a0a" ] || { echo "notpng"; return; }
  local b; b=($(od -An -tu1 -j16 -N8 "$1"))
  echo "$(( (b[0]<<24)+(b[1]<<16)+(b[2]<<8)+b[3] ))x$(( (b[4]<<24)+(b[5]<<16)+(b[6]<<8)+b[7] ))"
}

LMSCRIPT="$(scripts/select-compiler.sh)"
scripts/sync-sdk.sh --verify >/dev/null || fail "SDK header pin mismatch"
ok "SDK header matches sdk.lock"
scripts/sync-gnuplot.sh --verify >/dev/null || fail "gnuplot pin or patches mismatch"
ok "third_party/gnuplot is gnuplot.lock plus patches/"

GEN=""
command -v ninja >/dev/null 2>&1 && GEN="-G Ninja"
# shellcheck disable=SC2086
cmake -S . -B "$BUILD_DIR" $GEN -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build "$BUILD_DIR" >"$WORK/build.log" 2>&1 || { tail -40 "$WORK/build.log" >&2; fail "build"; }
ok "build"

want_version="lmplot $(sed -n 's/^project(lmplot VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ "$("$HOST" --version)" = "$want_version" ] || fail "--version is not exactly '$want_version'"
ok "--version prints exactly '$want_version'"
"$HOST" --dsl-schema --json >"$WORK/schema.json" || fail "embedded schema print"
cmp -s "$WORK/schema.json" schema/lmplot.dsl.json || fail "embedded schema differs from schema/lmplot.dsl.json"
ok "--dsl-schema --json prints byte-identical embedded schema bytes"
diff -u <("$LMSCRIPT" --dsl-signatures schema/lmplot.dsl.json) <("$HOST" --dsl-signatures) || fail "host/compiler signatures differ"
ok "host registry matches compiler signatures"
"$HOST" --notices | grep -q "stb_truetype" || fail "--notices missing third-party text"
ok "--notices prints the third-party license texts"

# Static linking: nothing but the executable itself.
case "$UNAME_S" in
  Linux) if ldd "$HOST" >/dev/null 2>&1; then fail "executable is dynamically linked"; fi; ok "executable is fully static (no dynamic loader)";;
  Darwin) bad="$(otool -L "$HOST" | tail -n +2 | grep -v -E '/usr/lib/(libSystem|libc\+\+|libresolv)' || true)"
          [ -z "$bad" ] || fail "non-system dylib dependencies: $bad"; ok "only system libraries linked";;
  *) ok "static-link check skipped on $UNAME_S";;
esac

# Embedded gnuplot, command-line passthrough and the dumb terminal.
out="$("$HOST" gnuplot -e 'set terminal dumb size 60,15; plot sin(x)' </dev/null)"
echo "$out" | grep -q "sin(x)" || fail "gnuplot passthrough dumb plot"
ok "lmplot gnuplot runs the embedded gnuplot"
"$HOST" gnuplot -e 'set terminal' </dev/null 2>&1 | grep -q -E "^ +png " || fail "png terminal missing"
"$HOST" gnuplot -e 'set terminal' </dev/null 2>&1 | grep -q -E "^ +svg " || fail "svg terminal missing"
ok "png and svg terminals are built in"

# Examples: compile, run, validate the artifacts.
mkdir -p build/examples
n=0
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$LMSCRIPT" "$src" "build/$name.dslb" >"$WORK/$name.log" 2>&1 || { cat "$WORK/$name.log" >&2; fail "$name compile"; }
  n=$((n+1))
done
ok "$n examples compile as bytecode v5"
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --dsl "build/$name.dslb" >"$WORK/$name.out" 2>"$WORK/$name.err" || { cat "$WORK/$name.out" "$WORK/$name.err" >&2; fail "$name run"; }
  grep -q '"ok": true' "$WORK/$name.out" || { cat "$WORK/$name.out" >&2; fail "$name did not report ok"; }
done
ok "$n examples run"
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  found=0
  for f in build/examples/"$name"*.png; do
    [ -f "$f" ] || continue
    found=1
    size="$(png_size "$f")"
    [ "$size" != "notpng" ] || fail "$f is not a PNG"
  done
  [ "$found" = 1 ] || fail "$name wrote no PNG"
done
ok "every example wrote valid PNG files"
if [ -f examples/expected.txt ]; then
  while read -r file want; do
    [ -z "${file:-}" ] && continue
    case "$file" in \#*) continue;; esac
    [ -f "build/examples/$file" ] || fail "missing build/examples/$file"
    got="$(png_size "build/examples/$file")"
    [ "$got" = "$want" ] || fail "$file is $got, expected $want"
  done < examples/expected.txt
  ok "example PNG dimensions match examples/expected.txt"
fi

# Determinism: identical bytes on rerun, for every image of every example.
for f in build/examples/*.png; do echo "$(sha "$f") $f"; done >"$WORK/before.txt"
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --dsl "build/$name.dslb" >/dev/null 2>&1 || fail "$name rerun"
done
for f in build/examples/*.png; do echo "$(sha "$f") $f"; done >"$WORK/after.txt"
cmp -s "$WORK/before.txt" "$WORK/after.txt" || { diff "$WORK/before.txt" "$WORK/after.txt" >&2 || true; fail "images differ on rerun"; }
ok "all $(wc -l <"$WORK/after.txt" | tr -d ' ') example images are byte-identical on rerun"

# DSL error paths.
cat >"$WORK/bad.lms" <<LMS
p = _dsl_ schema="$ROOT/schema/lmplot.dsl.json";
plot.cmd("this is not a gnuplot command");
plot.echo("unreachable");
end _dsl_
savedsl(p, "$WORK/bad.dslb");
LMS
"$LMSCRIPT" "$WORK/bad.lms" >/dev/null || fail "bad.lms compile"
set +e; out="$("$HOST" --dsl "$WORK/bad.dslb" 2>/dev/null)"; code=$?; set -e
[ "$code" = 3 ] || fail "bad gnuplot command exit code $code, expected 3"
echo "$out" | grep -q '"phase": "run"' || fail "bad command did not report a run error: $out"
echo "$out" | grep -q 'unreachable' && fail "statements after the failing command ran"
ok "a failing gnuplot command stops the program (exit 3, JSON error)"

cat >"$WORK/values.lms" <<LMS
p = _dsl_ schema="$ROOT/schema/lmplot.dsl.json";
plot.data("d", "1 2\n2 4\n3 6\n4 8");
plot.cmd("set terminal unknown");
plot.cmd("stats \$d u 1:2 nooutput");
plot.echo("slope=" + str(plot.num("STATS_slope")));
plot.echo(plot.str("sprintf('%d points', STATS_records)"));
end _dsl_
savedsl(p, "$WORK/values.dslb");
LMS
"$LMSCRIPT" "$WORK/values.lms" >/dev/null || fail "values.lms compile"
out="$("$HOST" --dsl "$WORK/values.dslb")"
echo "$out" | grep -q "slope=2" || fail "stats slope: $out"
echo "$out" | grep -q "4 points" || fail "stats records: $out"
ok "Plot.num / Plot.str / Plot.data return gnuplot values"

set +e; "$HOST" --dsl "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

echo "all $pass checks passed"
