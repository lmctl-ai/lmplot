# Purpose and place

## Purpose

**lmplot is gnuplot as one self-contained native executable, scriptable from
lmscript.** The binary has no runtime dependencies at all: no X11, no Cairo,
no libgd, no libpng, no Qt, no fontconfig, no readline, no zlib, no help or
share directories. `ldd lmplot` says "not a dynamic executable" on Linux and
`otool -L` shows only system libraries on macOS. Copy one file and plotting
works, including PNG output with TrueType text.

Two ways to drive it:

- `lmplot --dsl program.dslb` runs lmscript `_dsl_` bytecode against the
  embedded schema (`schema/lmplot.dsl.json`, type `Plot`, context `plot`).
  The script keeps control flow, loops, string building and numbers;
  gnuplot keeps drawing. `plot.cmd("...")` runs gnuplot commands,
  `plot.num("STATS_slope")` reads values back.
- `lmplot gnuplot [options] [files]` is the whole gnuplot command line,
  unchanged, for people who just want a static gnuplot.

## Place in the stack

- **gnuplot** (C, custom permissive license) is cloned from
  https://github.com/gnuplot/gnuplot at the commit pinned by `gnuplot.lock`
  into git-ignored `third_party/gnuplot` by `scripts/sync-gnuplot.sh`, then
  `patches/*.patch` are applied. Nothing of gnuplot is committed here; the
  license requires modifications to travel as patches (see
  `02-gnuplot-embedding.md`).
- **cpp-dsl-sdk** (`include/dslvm.hpp`, header-only, bytecode v5) is vendored
  as `cpp/dslvm.hpp`, pinned by `sdk.lock` exactly like lmhtml and lmcad. It
  executes `_dsl_` bytecode; it knows nothing about plotting.
- **lmscript**, published through npm as `@lmctl-ai/lmctl`, compiles `_dsl_`
  chunks against `schema/lmplot.dsl.json` into `.dslb`. Tests use the
  compiler selected by `scripts/select-compiler.sh` (probes for `DSLB\x00\x05`).
- **lmhtml / lmcad** are the sibling hosts whose conventions lmplot copies:
  one binary, schema embedded at build time, `--dsl` runs bytecode,
  `--dsl-schema [--json]` and `--dsl-signatures` for parity checks, JSON result
  line on stdout, exit codes 2 (usage) / 3 (host load or run failure).
  `deflate.hpp` comes from lmcad.

## Scope boundary

- The **script expresses policy**: which data, which ranges, how many frames,
  what to print. It can also compute (loops, arithmetic, f-strings) and then
  feed numbers to gnuplot as text.
- **gnuplot owns plotting**: parsing, fitting, statistics, layout, 2D/3D
  drawing, every terminal that needs no external library.
- **lmplot owns the host glue and the PNG terminal**: embedding, the DSL API,
  and a native rasterizer (`cpp/raster`, `cpp/term/lmpng.trm`).
- **Not interactive.** No X11/Qt/wxt windows, no mouse, no `pause mouse`. The
  terminals are file or text producers: `png`, `pngcairo` (alias), `svg`,
  `canvas`, `postscript`/`eps`, `dumb`, `block`, `epslatex`, `pstricks`,
  `tikz`-family LaTeX, `cgm`, `emf`, `fig`, `hpgl`, `dxf` and more
  (`lmplot gnuplot -e "set terminal"` lists them).

## Version identification

`lmplot --version` prints exactly `lmplot X.Y.Z` (lmscript's `install=`
mechanism compares that string with the release tag). The embedded gnuplot
reports `Version 6.1 patchlevel 0 (lmplot X.Y.Z)`, which is the version marking
the gnuplot license asks of modified binaries.

## Status

Early development; schema v0 may break. The first release is 0.1.0.
