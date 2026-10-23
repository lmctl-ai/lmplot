# Open items

## PNG terminal

- Load TTF/OTF files named in `font "..."` (stb_truetype can; needs a lookup
  path policy and a decision about GDFONTPATH / system font directories).
- Optional hinting or a gamma-corrected blend if text looks too soft on
  real screens; the 12 % stem darkening is a measured guess, not tuned.
- `gif` (animation) and `jpeg` terminals; `webp`/`apng` for turntables.
- Sub-pixel terminal coordinates (`tscale` > 1) for hairline accuracy.
- Hyperlinked output is only available through `svg`.

## Host and schema

- `Plot` is a thin command channel. If lists/maps arrive in the DSL, a typed
  layer (series, styles, datablock read-back as numbers) becomes worthwhile.
- No way to catch a gnuplot error inside the program (every error stops it).
- `plot.num` of a complex or array value returns an error; no array access.
- Multiple bytecode files in one invocation share gnuplot state; there is no
  explicit `reset` between them (use `plot.cmd("reset session")`).

## Platform

- Windows is built with gnuplot's portable code paths (`_WIN32` hidden, see
  `02-gnuplot-embedding.md`); unicode file names and the console code page
  are untested; `pipe`/`system` use the C runtime shell.
- macOS cannot link fully static; only system libraries are linked.
- Windows ARM64 and musl Linux builds are not in the matrix.

## Upstream

- `post.trm` brace bug (patch 0001) and the help pager reading stdin
  (patch 0006) are candidates to report to gnuplot.
- Rebasing: bump `gnuplot.lock`, run `scripts/sync-gnuplot.sh`; patches 0001
  and 0006 may become unnecessary.

## From the 2026-10-23 review (fixed items are in git log; these remain)

- `--dry-run` (added, patch 0007) still creates and removes the temporary `.gp` file that
  `Plot.script`/`Plot.data` load, in the system temp directory.
- Determinism is checked on one machine (byte-identical reruns); there are no golden image hashes
  across platforms (the rasterizer uses libm sin/cos/sqrt), and `examples/expected.txt` does not
  exist, so the PNG dimension check in `test.sh` is skipped.
- `Plot.num` returns NaN as NaN (gnuplot's "no value"), so scripts must test for it.
- Temporary scripts for `Plot.script`/`Plot.data` are created exclusively (O_EXCL) on POSIX only;
  Windows uses the per-user temp directory with plain `fopen`.
