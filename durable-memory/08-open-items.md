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
