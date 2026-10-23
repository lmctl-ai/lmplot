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
gif_magic(){
  local sig; sig="$(head -c 6 "$1" | od -An -tx1 | tr -d ' \n')"
  case "$sig" in
    474946383961|474946383761) echo "gif";;
    *) echo "notgif";;
  esac
}

# lmscript resolves `cli="lmplot"` on PATH first, so the examples bind to the binary under test.
export PATH="$(cd "$BUILD_DIR" 2>/dev/null && pwd || echo "$HERE/$BUILD_DIR"):$PATH"
if [ -d "${HOME:-}/.cache/lmscript/tools/github/lmctl-ai" ]; then
  for tool_dir in "$HOME/.cache/lmscript/tools/github/lmctl-ai"/*/latest; do
    [ -d "$tool_dir" ] && export PATH="$tool_dir:$PATH"
  done
fi
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
  MINGW*|MSYS*|CYGWIN*)
    if command -v objdump >/dev/null 2>&1; then
      bad="$(objdump -p "$HOST" | grep 'DLL Name' | grep -i -E 'libstdc|libgcc|winpthread|libgomp' || true)"
      [ -z "$bad" ] || fail "MinGW runtime DLLs are imported: $bad"
      ok "no MinGW runtime DLL imports"
    else
      ok "static-link check skipped (no objdump)"
    fi;;
  *) ok "static-link check skipped on $UNAME_S";;
esac

# Embedded gnuplot, command-line passthrough and the dumb terminal.
out="$("$HOST" gnuplot -e 'set terminal dumb size 60,15; plot sin(x)' </dev/null)"
echo "$out" | grep -q "sin(x)" || fail "gnuplot passthrough dumb plot"
ok "lmplot gnuplot runs the embedded gnuplot"
out="$("$HOST" gnuplot -e 'plot cos(x)' </dev/null 2>/dev/null)"
echo "$out" | grep -q -- "+-----" || fail "lmplot gnuplot default terminal is not dumb"
ok "lmplot gnuplot defaults to the dumb terminal"
"$HOST" gnuplot -e 'set terminal' </dev/null 2>&1 | grep -q -E "^ +png " || fail "png terminal missing"
"$HOST" gnuplot -e 'set terminal' </dev/null 2>&1 | grep -q -E "^ +svg " || fail "svg terminal missing"
ok "png and svg terminals are built in"

# README gallery verification: every linked example .lms and every referenced image in docs/images/ exists and has valid magic bytes.
readme_lms_count=0
while read -r lms_path; do
  [ -z "$lms_path" ] && continue
  [ -f "$lms_path" ] || fail "README links to missing example: $lms_path"
  readme_lms_count=$((readme_lms_count+1))
done < <(grep -oE 'examples/[A-Za-z0-9_/.-]+\.lms' README.md | sort -u)
[ "$readme_lms_count" -ge 15 ] || fail "expected >= 15 unique example links in README.md, got $readme_lms_count"
ok "all $readme_lms_count README example .lms links exist"

readme_img_count=0
while read -r img_path; do
  [ -z "$img_path" ] && continue
  [ -s "$img_path" ] || fail "README references missing or empty image: $img_path"
  bytes="$(wc -c <"$img_path" | tr -d ' ')"
  case "$img_path" in
    *.png)
      [ "$(png_size "$img_path")" != "notpng" ] || fail "$img_path is not a valid PNG"
      [ "$bytes" -le 307200 ] || fail "$img_path exceeds 300 KB ($bytes bytes)"
      ;;
    *.gif)
      [ "$(gif_magic "$img_path")" = "gif" ] || fail "$img_path is not a valid GIF"
      [ "$bytes" -le 3145728 ] || fail "$img_path exceeds 3 MB ($bytes bytes)"
      ;;
  esac
  readme_img_count=$((readme_img_count+1))
done < <(grep -oE 'docs/images/[A-Za-z0-9_.-]+\.(png|gif)' README.md | sort -u)
[ "$readme_img_count" -ge 15 ] || fail "expected >= 15 gallery images in README.md, got $readme_img_count"
ok "all $readme_img_count README gallery images in docs/images/ exist and have valid PNG/GIF headers"

# Examples: compile, run, validate the artifacts.
rm -rf build/examples
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

# Multi-host combo examples (lmcad + lmplot, lmdraw + lmplot).
if command -v lmcad >/dev/null 2>&1; then
  rm -f build/examples/nozzle_cad_plot.png
  "$LMSCRIPT" examples/combo/nozzle_cad_plot.lms >"$WORK/c-nozzle.log" 2>&1 || { cat "$WORK/c-nozzle.log" >&2; fail "combo nozzle_cad_plot"; }
  [ "$(png_size build/examples/nozzle_cad_plot.png)" != "notpng" ] || fail "nozzle_cad_plot.png is not a valid PNG"
  ok "combo example nozzle_cad_plot (lmcad + lmplot) renders valid PNG"
else
  ok "combo example nozzle_cad_plot skipped (sibling host lmcad not on PATH)"
fi

if command -v lmdraw >/dev/null 2>&1 && command -v ffmpeg >/dev/null 2>&1; then
  rm -f build/examples/sir_epidemic_draw_plot.png
  "$LMSCRIPT" examples/combo/sir_epidemic_draw_plot.lms >"$WORK/c-seir.log" 2>&1 || { cat "$WORK/c-seir.log" >&2; fail "combo sir_epidemic_draw_plot"; }
  [ "$(png_size build/examples/sir_epidemic_draw_plot.png)" != "notpng" ] || fail "sir_epidemic_draw_plot.png is not a valid PNG"
  ok "combo example sir_epidemic_draw_plot (lmdraw + lmplot) renders valid PNG"
else
  ok "combo example sir_epidemic_draw_plot skipped (sibling host lmdraw or ffmpeg not on PATH)"
fi

# Video examples: frames via lmplot, encoding via ffmpeg (skipped when ffmpeg is absent).
if command -v ffmpeg >/dev/null 2>&1; then
  for src in examples/video/*.lms; do
    name="$(basename "$src" .lms)"
    rm -f "build/video/$name.mp4"
    "$LMSCRIPT" "$src" >"$WORK/v-$name.log" 2>&1 || { cat "$WORK/v-$name.log" >&2; fail "video $name"; }
    [ -s "build/video/$name.mp4" ] || fail "video $name wrote no mp4"
  done
  [ "$(gif_magic build/video/quantum_packet.gif)" = "gif" ] || fail "quantum_packet.gif is not a valid GIF"
  ok "video examples render frames and encode mp4/gif with ffmpeg"
else
  ok "video examples skipped (no ffmpeg)"
fi

# DSL error paths.
cat >"$WORK/bad.lms" <<LMS
p = _dsl_ cli="lmplot";
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
p = _dsl_ cli="lmplot";
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

# Values and writes that used to pass silently: complex or array results of Plot.num, a datablock
# line starting with EOD (ended the block and ran the rest as commands), output write errors.
cat >"$WORK/edge.lms" <<LMS
p = _dsl_ cli="lmplot";
plot.data("e", "1 2\nEOD\nEODX 5\n3 4");
plot.cmd("set terminal unknown");
plot.cmd("stats \$e u 1 nooutput");
plot.echo("lines=" + str(plot.num("|\$e|")) + " records=" + str(plot.num("STATS_records")));
plot.echo("real=" + str(plot.num("{3,0}")));
end _dsl_
savedsl(p, "$WORK/edge.dslb");
LMS
"$LMSCRIPT" "$WORK/edge.lms" >/dev/null || fail "edge.lms compile"
out="$("$HOST" --dsl "$WORK/edge.dslb" 2>&1)" || fail "edge.lms run: $out"
echo "$out" | grep -q "lines=4 records=2" || fail "datablock with EOD lines: $out"
echo "$out" | grep -q "real=3" || fail "complex with zero imaginary part: $out"
expect_run_error() { # name, snippet, expected message fragment
  printf 'p = _dsl_ cli="lmplot";\n%s\nend _dsl_\nsavedsl(p, "%s");\n' "$2" "$WORK/$1.dslb" >"$WORK/$1.lms"
  "$LMSCRIPT" "$WORK/$1.lms" >/dev/null || fail "$1 compile"
  set +e; out="$("$HOST" --dsl "$WORK/$1.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] || fail "$1: exit code $code, expected 3 ($out)"
  echo "$out" | grep -q "$3" || fail "$1: message lacks '$3': $out"
}
expect_run_error complex 'let x = plot.num("sqrt(-1)");' "complex"
expect_run_error array 'plot.cmd("array A[2] = [1, 2]"); let x = plot.num("A");' "not a number"
expect_run_error dataname 'plot.data("1x", "1 2");' "Plot.data: name"
if [ "$UNAME_S" = Linux ] && [ -w /dev/full ]; then
  expect_run_error writefail 'plot.png("/dev/full", 200, 100); plot.cmd("plot x");' "error writing the output"
fi
ok "Plot.num rejects complex/array values, datablocks keep EOD lines, write errors stop the program"

set +e; "$HOST" --dsl "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

echo "all $pass checks passed"
