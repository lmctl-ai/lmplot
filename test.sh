#!/usr/bin/env bash
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
cd "$HERE"
BUILD_DIR="${BUILD_DIR:-build}"
HOST="$BUILD_DIR/lmplot"
UNAME_S="$(uname -s)"
case "$UNAME_S" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac
WORK="$(mktemp -d "${TMPDIR:-/tmp}/lmplot-test-XXXXXX")"
trap 'rm -rf "$WORK" "$HERE/images"' EXIT
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
if [ -d "${HOME:-}/.cache/lmscript/tools/github/lmctl-ai" ]; then
  for tool_dir in "$HOME/.cache/lmscript/tools/github/lmctl-ai"/*/latest; do
    case "$tool_dir" in */lmplot/latest) continue;; esac  # the binary under test stays first
    [ -d "$tool_dir" ] && export PATH="$tool_dir:$PATH"
  done
fi
export PATH="$(cd "$BUILD_DIR" 2>/dev/null && pwd || echo "$HERE/$BUILD_DIR"):$PATH"
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
"$HOST" --notices | grep -q "stb_truetype" && "$HOST" --notices | grep -q "stb_image" || fail "--notices missing third-party text"
ok "--notices prints the third-party license texts"

# Linux: only system glibc is dynamic; macOS/Windows retain their existing checks.
case "$UNAME_S" in
  Linux)
    python3 scripts/test-linux-linkage.py || fail "Linux linkage checker regressions"
    python3 scripts/check-linux-linkage.py "$HOST" || fail "Linux dependency allowlist or glibc baseline"
    ok "only dynamic system glibc is needed; readelf audits the glibc 2.35 ABI baseline";;
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

# README gallery verification: every autorun .lms URL and every referenced image in docs/images/ exists and has valid magic bytes.
readme_lms_count=0
while read -r lms_path; do
  [ -z "$lms_path" ] && continue
  [ -f "$lms_path" ] || fail "README references missing example: $lms_path"
  readme_lms_count=$((readme_lms_count+1))
done < <(grep -oE 'examples/[A-Za-z0-9_/.-]+\.lms' README.md | sort -u)
[ "$readme_lms_count" -ge 15 ] || fail "expected >= 15 unique example URLs in README.md, got $readme_lms_count"
autorun_count="$(grep -oE 'lmctl( autorun)? https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/' README.md | wc -l | tr -d ' ')"
[ "$autorun_count" -ge 21 ] || fail "expected >= 21 lmctl commands in README.md, got $autorun_count"
ok "all $readme_lms_count README autorun .lms URLs map to existing examples"

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

# Examples: compile + self-execute into images/, validate the artifacts, and check byte-identical rerun.
rm -rf build/examples images
mkdir -p build/examples images
n=0
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$LMSCRIPT" "$src" "build/$name.dslb" >"$WORK/$name.log" 2>&1 || { cat "$WORK/$name.log" >&2; fail "$name compile/autorun"; }
  n=$((n+1))
done
ok "$n examples compile as bytecode v5 and self-execute"
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  [ -f "images/$name.png" ] || fail "$name did not write images/$name.png"
  found=0
  for f in images/"$name"*.png; do
    [ -f "$f" ] || continue
    found=1
    size="$(png_size "$f")"
    [ "$size" != "notpng" ] || fail "$f is not a PNG"
  done
  [ "$found" = 1 ] || fail "$name wrote no PNG"
done
ok "every example wrote valid PNG files into images/"

# Determinism: identical bytes on rerun, for every image of every example.
for f in images/*.png; do echo "$(sha "$f") $f"; done >"$WORK/before.txt"
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$HOST" --dsl "build/$name.dslb" >"$WORK/$name.out" 2>"$WORK/$name.err" || { cat "$WORK/$name.out" "$WORK/$name.err" >&2; fail "$name run"; }
  grep -q '"ok": true' "$WORK/$name.out" || { cat "$WORK/$name.out" >&2; fail "$name did not report ok"; }
done
ok "$n examples run directly via --dsl"
for f in images/*.png; do echo "$(sha "$f") $f"; done >"$WORK/after.txt"
cmp -s "$WORK/before.txt" "$WORK/after.txt" || { diff "$WORK/before.txt" "$WORK/after.txt" >&2 || true; fail "images differ on rerun"; }
ok "all $(wc -l <"$WORK/after.txt" | tr -d ' ') example images are byte-identical on rerun"

mv images/*.png build/examples/
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

# Multi-host combo examples (lmcad + lmplot, lmdraw + lmplot).
if command -v lmcad >/dev/null 2>&1; then
  rm -f images/nozzle_cad_plot.png build/examples/nozzle_cad_plot.png
  "$LMSCRIPT" examples/combo/nozzle_cad_plot.lms >"$WORK/c-nozzle.log" 2>&1 || { cat "$WORK/c-nozzle.log" >&2; fail "combo nozzle_cad_plot"; }
  [ "$(png_size images/nozzle_cad_plot.png)" != "notpng" ] || fail "nozzle_cad_plot.png is not a valid PNG"
  mv images/nozzle_cad_plot.png build/examples/nozzle_cad_plot.png
  ok "combo example nozzle_cad_plot (lmcad + lmplot) renders valid PNG"
else
  ok "combo example nozzle_cad_plot skipped (sibling host lmcad not on PATH)"
fi

if command -v lmdraw >/dev/null 2>&1 && command -v ffmpeg >/dev/null 2>&1; then
  rm -f images/sir_epidemic_draw_plot.png build/examples/sir_epidemic_draw_plot.png
  "$LMSCRIPT" examples/combo/sir_epidemic_draw_plot.lms >"$WORK/c-seir.log" 2>&1 || { cat "$WORK/c-seir.log" >&2; fail "combo sir_epidemic_draw_plot"; }
  [ "$(png_size images/sir_epidemic_draw_plot.png)" != "notpng" ] || fail "sir_epidemic_draw_plot.png is not a valid PNG"
  mv images/sir_epidemic_draw_plot.png build/examples/sir_epidemic_draw_plot.png
  ok "combo example sir_epidemic_draw_plot (lmdraw + lmplot) renders valid PNG"
else
  ok "combo example sir_epidemic_draw_plot skipped (sibling host lmdraw or ffmpeg not on PATH)"
fi

# lmdraw diagram placed into the lmplot figure with `set pixmap` (no ffmpeg).
if command -v lmdraw >/dev/null 2>&1; then
  rm -f images/projectile_drag.png
  "$LMSCRIPT" examples/combo/projectile_drag.lms >"$WORK/c-proj.log" 2>&1 || { cat "$WORK/c-proj.log" >&2; fail "combo projectile_drag"; }
  [ "$(png_size images/projectile_drag.png)" = "900x560" ] || fail "projectile_drag.png is not a 900x560 PNG"
  mv images/projectile_drag.png build/examples/projectile_drag.png
  ok "combo example projectile_drag (lmdraw diagram as pixmap, RK4 sweep, vacuum range asserted)"
else
  ok "combo example projectile_drag skipped (sibling host lmdraw not on PATH)"
fi

# Video examples: frames via lmplot, encoding via ffmpeg (skipped when ffmpeg is absent).
if command -v ffmpeg >/dev/null 2>&1; then
  mkdir -p build/video
  for src in examples/video/*.lms; do
    name="$(basename "$src" .lms)"
    rm -f "images/$name.mp4" "images/$name.png" "build/video/$name.mp4"
    "$LMSCRIPT" "$src" >"$WORK/v-$name.log" 2>&1 || { cat "$WORK/v-$name.log" >&2; fail "video $name"; }
    [ -s "images/$name.mp4" ] || fail "video $name wrote no mp4"
    [ "$(png_size "images/$name.png")" != "notpng" ] || fail "video $name wrote no preview PNG"
    mv "images/$name.mp4" "build/video/$name.mp4"
    mv "images/$name.png" "build/video/$name.png"
  done
  [ "$(gif_magic images/quantum_packet.gif)" = "gif" ] || fail "quantum_packet.gif is not a valid GIF"
  mv images/quantum_packet.gif build/video/quantum_packet.gif
  ok "video examples render frames and encode mp4/gif with ffmpeg"
else
  ok "video examples skipped (no ffmpeg)"
fi
rm -rf images

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
  set +e; out="$("$HOST" --dsl "$WORK/$1.dslb" 2>"$WORK/$1.rerr")"; code=$?; set -e
  [ "$code" = 3 ] || fail "$1: exit code $code, expected 3 ($out)"
  echo "$out" | grep -q "$3" || fail "$1: message lacks '$3': $out"
  grep -q "$3" "$WORK/$1.rerr" || fail "$1: stderr lacks '$3': $(cat "$WORK/$1.rerr")"
}
expect_run_error complex 'let x = plot.num("sqrt(-1)");' "complex"
expect_run_error array 'plot.cmd("array A[2] = [1, 2]"); let x = plot.num("A");' "not a number"
expect_run_error assertfail 'plot.assert_close(1.0, 1.0, 0.0); plot.assert_close(2.5, 2.0, 0.1);' "assert_close failed: actual=2.5"
expect_run_error asserttol 'plot.assert_close(1.0, 1.0, -1.0);' "tolerance is negative"
expect_run_error dataname 'plot.data("1x", "1 2");' "Plot.data: name"
if [ "$UNAME_S" = Linux ] && [ -w /dev/full ]; then
  expect_run_error writefail 'plot.png("/dev/full", 200, 100); plot.cmd("plot x");' "error writing the output"
fi
ok "Plot.num rejects complex/array values, assert_close checks known answers, datablocks keep EOD lines, write errors stop the program"

# Duplicated font options, gnuplot warnings, and empty/incomplete output files fail with exit 3 + stderr and leave no PNG behind.
expect_run_error dupfont_term "plot.cmd(\"set terminal png size 400,300 font 'Sans,12' font ',10'\");" "duplicate font option"
expect_run_error dupfont_termsize "plot.cmd(\"set terminal png size 400,300 font 'Sans,12' 10\");" "duplicate font option"
expect_run_error dupfont_svg "plot.cmd(\"set terminal svg size 400,300 font 'Sans,12' font ',10'\");" "duplicate font option"
expect_run_error dupfont_title "plot.png(\"$WORK/dupfont.png\", 600, 400); plot.cmd(\"set multiplot layout 2,2 title 'Main' font ',13'\"); plot.cmd(\"set title 'Sub 1' font ',11' font ',11'\");" "duplicate font option"
[ ! -e "$WORK/dupfont.png" ] || fail "failed multiplot left behind $WORK/dupfont.png"
expect_run_error dupfont_title_split "plot.cmd(\"set title font ',11' 'Sub 1' font ',12'\");" "duplicate font option"
expect_run_error dupfont_label_split "plot.cmd(\"set label 1 font ',11' 'Note' font ',12'\");" "duplicate font option"
expect_run_error dupfont_key "plot.cmd(\"set key font ',11' font ',9'\");" "duplicate font option"
expect_run_error dupfont_tics "plot.cmd(\"set xtics font ',11' font ',9'\");" "duplicate font option"
expect_run_error warn_keypos "plot.cmd(\"set key top bottom\");" "warning:"
expect_run_error warn_missing "plot.png(\"$WORK/warn.png\", 400, 300); plot.cmd(\"plot '$WORK/no_such_file.dat' u 1:2 w l, sin(x)\");" "warning:"
[ ! -e "$WORK/warn.png" ] || fail "warning during plot left behind $WORK/warn.png"
expect_run_error empty_close "plot.png(\"$WORK/empty1.png\", 400, 300); plot.close();" "output file closed with no plots drawn"
[ ! -e "$WORK/empty1.png" ] || fail "empty plot.close() left behind $WORK/empty1.png"
expect_run_error empty_eof "plot.png(\"$WORK/empty2.png\", 400, 300);" "output file closed with no plots drawn"
[ ! -e "$WORK/empty2.png" ] || fail "empty output at EOF left behind $WORK/empty2.png"
expect_run_error empty_mp "plot.png(\"$WORK/empty_mp.png\", 400, 300); plot.cmd(\"set multiplot layout 2,2 title 'Only Title'\"); plot.cmd(\"unset multiplot\");" "multiplot ended with no plots drawn"
[ ! -e "$WORK/empty_mp.png" ] || fail "empty multiplot left behind $WORK/empty_mp.png"
expect_run_error unclosed_mp "plot.png(\"$WORK/unclosed_mp.png\", 400, 300); plot.cmd(\"set multiplot layout 2,2 title 'Unclosed'\"); plot.cmd(\"plot sin(x)\");" "multiplot not closed"
[ ! -e "$WORK/unclosed_mp.png" ] || fail "unclosed multiplot left behind $WORK/unclosed_mp.png"
ok "duplicate font options, gnuplot warnings, and empty/unclosed outputs fail with stderr + exit 3 and leave no truncated PNG"

# Memory budget: a runaway loop stops with a run error (exit 3, message on stderr and in the JSON
# line) instead of exhausting the machine. Each iteration makes a gnuplot array of 1e6 values
# (~23 MiB through gp_alloc, refused by the counting malloc shim). A small budget everywhere, a png
# canvas over the budget (C++ raster called from C), and on Linux the default 4096 MiB under a
# 6 GiB address-space limit (ulimit only here, never in the host).
cat >"$WORK/runaway.lms" <<LMS
p = _dsl_ cli="lmplot";
let i = 0;
while (i < 1000000) {
  plot.cmd("array A" + str(i) + "[1000000]");
  i = i + 1;
}
end _dsl_
savedsl(p, "$WORK/runaway.dslb");
LMS
"$LMSCRIPT" "$WORK/runaway.lms" >/dev/null || fail "runaway.lms compile"
set +e; out="$(LMPLOT_MAX_MEMORY_MB=64 "$HOST" --dsl "$WORK/runaway.dslb" 2>"$WORK/runaway.err")"; code=$?; set -e
[ "$code" = 3 ] || fail "runaway loop under LMPLOT_MAX_MEMORY_MB=64: exit $code, expected 3 ($out)"
echo "$out" | grep -q "memory budget exceeded" || fail "runaway loop: JSON lacks the budget message: $out"
grep -q "LMPLOT_MAX_MEMORY_MB" "$WORK/runaway.err" || fail "runaway loop: no budget message on stderr"
printf 'p = _dsl_ cli="lmplot";\nplot.png("%s", 10000, 5000);\nplot.cmd("plot x");\nend _dsl_\nsavedsl(p, "%s");\n' \
  "$WORK/big.png" "$WORK/bigcanvas.dslb" >"$WORK/bigcanvas.lms"
"$LMSCRIPT" "$WORK/bigcanvas.lms" >/dev/null || fail "bigcanvas.lms compile"
set +e; out="$(LMPLOT_MAX_MEMORY_MB=64 "$HOST" --dsl "$WORK/bigcanvas.dslb" 2>/dev/null)"; code=$?; set -e
[ "$code" = 3 ] && echo "$out" | grep -q "memory budget exceeded" || fail "10000x5000 png canvas under LMPLOT_MAX_MEMORY_MB=64: exit $code ($out)"
set +e; LMPLOT_MAX_MEMORY_MB=-5 "$HOST" --dsl "$WORK/runaway.dslb" >/dev/null 2>"$WORK/badenv.err"; code=$?; set -e
[ "$code" = 2 ] && grep -q "positive whole number" "$WORK/badenv.err" || fail "bad LMPLOT_MAX_MEMORY_MB: exit $code"
if [ "$UNAME_S" = Linux ]; then
  set +e; out="$(ulimit -v 6291456; "$HOST" --dsl "$WORK/runaway.dslb" 2>/dev/null)"; code=$?; set -e
  [ "$code" = 3 ] && echo "$out" | grep -q "limit 4096 MiB" || fail "runaway loop with the default budget under ulimit -v 6 GiB: exit $code ($out)"
  ok "memory budget: runaway loop stops at 64 MiB and at the default 4096 MiB (under ulimit -v 6 GiB), png canvas too"
else
  ok "memory budget: runaway loop and png canvas stop at LMPLOT_MAX_MEMORY_MB=64 (default-budget run is Linux-only)"
fi

# Image input (stb_image in place of libgd): a PNG written by lmplot is read back as data
# (filetype=png: 4 columns r g b a per pixel) and placed as a pixmap; a non-image is an error.
cat >"$WORK/imgin.lms" <<LMS
p = _dsl_ cli="lmplot";
plot.png("$WORK/imgsrc.png", 200, 150);
plot.cmd("set object 1 rect from screen 0,0 to screen 1,1 behind fc rgb '#ff0000' fs solid noborder");
plot.cmd("unset border; unset tics; unset key; set xrange [0:1]; set yrange [0:1]; plot 2");
plot.close();
plot.cmd("set terminal unknown; set autoscale");
plot.cmd("set table \$px; plot '$WORK/imgsrc.png' binary filetype=png with rgbimage; unset table");
plot.cmd("stats \$px using 3:4 nooutput");
plot.echo("px=" + plot.str("STATS_records") + " r=" + plot.str("STATS_median_x") + " g=" + plot.str("STATS_median_y"));
plot.cmd("unset object 1");
plot.png("$WORK/imgdst.png", 120, 90);
plot.cmd("set pixmap 1 '$WORK/imgsrc.png' at screen 0.1,0.1 width screen 0.5");
plot.cmd("plot x");
plot.close();
end _dsl_
savedsl(p, "$WORK/imgin.dslb");
LMS
"$LMSCRIPT" "$WORK/imgin.lms" >/dev/null || fail "imgin.lms compile"
out="$("$HOST" --dsl "$WORK/imgin.dslb" 2>&1)" || fail "image input: $out"
echo "$out" | grep -q "px=30000 r=255 g=0" || fail "PNG read back as 200x150 mostly red pixels: $out"
[ "$(png_size "$WORK/imgdst.png")" = 120x90 ] || fail "pixmap output"
printf 'not an image' >"$WORK/notimg.png"
expect_run_error notimg "plot.cmd(\"set terminal unknown; plot '$WORK/notimg.png' binary filetype=png with rgbimage\");" "cannot read image"
ok "PNG input: filetype=png data and set pixmap read images (stb_image), bad files are errors"

# --dry-run: the program runs and returns values, but no file is written and shell commands fail.
mkdir -p "$WORK/dry"
cat >"$WORK/dry.lms" <<LMS
p = _dsl_ cli="lmplot";
plot.png("dry.png", 200, 100);
plot.cmd("plot sin(x)");
plot.close();
plot.cmd("set print 'dry.txt'; print 1; set print");
plot.cmd("save 'dry.gp'");
plot.cmd("set table 'dry.dat'; plot x; unset table");
plot.echo("v=" + plot.str("1+1"));
end _dsl_
savedsl(p, "$WORK/dry.dslb");
LMS
"$LMSCRIPT" "$WORK/dry.lms" >/dev/null || fail "dry.lms compile"
host_abs="$(cd "$(dirname "$HOST")" && pwd)/$(basename "$HOST")"
out="$(cd "$WORK/dry" && "$host_abs" --dry-run --dsl ../dry.dslb 2>&1)" || fail "dry run: $out"
echo "$out" | grep -q "v=2" || fail "dry run values: $out"
for f in dry.png dry.txt dry.gp dry.dat; do
  echo "$out" | grep -q "\[dry-run\] would write $f" || fail "dry run did not report $f: $out"
  [ ! -e "$WORK/dry/$f" ] || fail "dry run wrote $f"
done
printf 'p = _dsl_ cli="lmplot";\nplot.cmd("x = system(\\"echo hi\\")");\nend _dsl_\nsavedsl(p, "%s");\n' "$WORK/dryshell.dslb" >"$WORK/dryshell.lms"
"$LMSCRIPT" "$WORK/dryshell.lms" >/dev/null || fail "dryshell compile"
set +e; out="$("$HOST" --dry-run --dsl "$WORK/dryshell.dslb" 2>/dev/null)"; code=$?; set -e
[ "$code" = 3 ] || fail "dry run shell command exit $code, expected 3"
echo "$out" | grep -q "disabled by --dry-run" || fail "dry run shell message: $out"
set +e; "$HOST" --dry-run >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "--dry-run without --dsl exit $code, expected 2"
ok "--dry-run writes no files, reports them, and refuses shell commands"

set +e; "$HOST" --dsl "$WORK/does-not-exist.dslb" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 3 ] || fail "missing bytecode exit code $code, expected 3"
set +e; "$HOST" >/dev/null 2>&1; code=$?; set -e
[ "$code" = 2 ] || fail "usage exit code $code, expected 2"
ok "usage and load errors use exit codes 2 and 3"

# The published compiler injects std; the host embeds only its own schema bytes.
"$LMSCRIPT" --dsl-signatures schema/lmplot.dsl.json --without-std > "$WORK/std-host-only.sig"
"$LMSCRIPT" --dsl-signatures cpp/std/lmcode-std.schema.json > "$WORK/std-builtins.sig"
"$HOST" --dsl-signatures > "$WORK/std-registry.sig"
python3 - "$WORK" <<'PYSTD'
import pathlib,sys,json
w=pathlib.Path(sys.argv[1])
read=lambda name:set((w/name).read_text().splitlines())
assert read('std-registry.sig')-read('std-builtins.sig') == read('std-host-only.sig'), 'host-only registry drift'
schema=str(pathlib.Path('schema/lmplot.dsl.json').resolve()).replace('\\','/')
out=str(w/'std-probe.dslb').replace('\\','/')
(w/'std-probe.lms').write_text('p = _lmcode_ schema='+json.dumps(schema)+';\nlet values = str_list().add("abc");\nif (str_len(values.at(0)) != 3) { let bad = int_list().at(0); }\nend _lmcode_\nsavedsl(p, '+json.dumps(out)+');\n')
PYSTD
"$LMSCRIPT" "$WORK/std-probe.lms" > "$WORK/std-probe.log" 2>&1 || { cat "$WORK/std-probe.log"; fail "implicit std compile"; }
"$HOST" --dsl "$WORK/std-probe.dslb" > "$WORK/std-run.log" 2>&1 || { cat "$WORK/std-run.log"; fail "implicit std runtime"; }
ok "host-only signatures and implicit std runtime"

echo "all $pass checks passed"
