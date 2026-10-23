# Examples

28 programs in `examples/`, 75 PNG images, run by `./test.sh` (valid PNG
signature, byte-identical on rerun). 26 are lmscript versions of demos in the
gnuplot gallery (https://www.gnuplot.info/screenshots/index.html#demos; sources
in `third_party/gnuplot/demo/*.dem`); two (`stats.lms`, `turntable.lms`)
show what lmscript adds. The table is in `examples/README.md`.

## Pattern

Every program: header comment naming the demo, `out = ARGV...` (compile-time
output path), `mkdir("build/examples")`, a `_dsl_` block whose schema path is
relative to the `.lms` file, `savedsl(p, out)`. One `plot.png` /
`plot.close` pair per image. Data files, when needed, are small demo files
copied unchanged to `examples/data/` and referenced relative to the repository
root (programs are run from there).

## What the `_dsl_` language does and does not allow (found writing these)

- Strictly typed, schema-driven: floats are written `1.0` (no `1.`), no
  f-strings inside the block, no list literals, no `let` of untyped values;
  `for (i in range(a, b))`, `if`, `switch`, `fn` helpers and string `+` with
  `str()` work; `return` must be the last statement of a function.
- Variables defined outside the block are invisible inside it.
- String literals end on their line: multi-line gnuplot text needs `\n`
  escapes (`plot.script`) or several `plot.cmd` calls. gnuplot does not
  process `\n` inside single-quoted strings (use `\"...\\n...\"` for titles
  with line breaks).
- Random-looking data: gnuplot's `rand(0)` is unseeded across runs, so
  `stats.lms` uses a small LCG written in the DSL, which keeps the images
  byte-identical on rerun.

## Rendering notes from the example pass

- Faint seams between pm3d quads and blurry box edges were fixed in the
  terminal (crisp fills for pm3d layers and axis-aligned rectangles).
- A boxed enhanced-text label measured its box in both justification passes;
  fixed (only the drawing pass contributes to the box).
- 1 px key samples look darker than slanted 1 px curves: horizontal lines on
  pixel centres are fully covered, anti-aliased diagonals are not. Same
  geometry as gnuplot's svg output; not a bug.
- `parallel-1` axes 2 and 4 are drawn over the data extent, not the full
  `paxis` range; that is gnuplot's own paxis drawing, unchanged by the terminal.

## Video examples (`examples/video/`)

`wave.lms`, `ripple.lms`, `lissajous.lms`: a `_dsl_` loop writes
`build/video/<name>/frame-<k>.png`, then plain lmscript runs
`system ${dsl_cli(p)} --dsl out.dslb;` and `system ffmpeg ... -i frame-%d.png
-c:v libx264 -pix_fmt yuv420p build/video/<name>.mp4;`. Gotchas: `system`
spawns without a shell (a quoted command line is taken as a program name) and
ignores failures, so check the frame count; yuv420p needs even frame sizes;
gnuplot has no color named `firebrick`/`navy`-style names beyond its list
(use `#rrggbb`). `test.sh` runs them only when ffmpeg is installed.
