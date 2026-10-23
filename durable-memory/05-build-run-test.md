# Build, run, test

Machine checked 2026-10-22 on x86_64 Linux (WSL2), 4 cores, GCC.

## Toolchain

- cpp-dsl-sdk header `cpp/dslvm.hpp` pinned by `sdk.lock` (commit
  `ef861844`, same bytes as lmhtml's copy, sha256 `6a537aa6...`).
  `scripts/sync-sdk.sh --verify` is the first `test.sh` check.
- gnuplot pinned by `gnuplot.lock` (`fbeb88ea`, 2023-09-08, 6.1.0-dev),
  shallow-fetched into git-ignored `third_party/gnuplot` and patched by
  `scripts/sync-gnuplot.sh` (`--verify` hashes the patch set).
- Compiler for `.lms`: `scripts/select-compiler.sh`; `LMSCRIPT_BIN` overrides.
  CI pins `@lmctl-ai/lmctl@0.2.369` (verified locally).
- Vendored, small: `vendor/stb_truetype.h`, four DejaVu fonts,
  `cpp/raster/deflate.hpp` (lmcad).

## Build

- Clean Release build (cmake + ninja, 4 cores): **~14 s**; that is the
  whole of gnuplot (about 60 C files) plus the host.
- Binary: **6.7 MB stripped** (7.4 MB unstripped), `-static` by default
  (`LMPLOT_FULL_STATIC=ON`). 2.4 MB of that is the embedded fonts.
  `ldd` reports "not a dynamic executable" (asserted in `test.sh`).
- Cross compile checks, not run locally: `zig cc -target x86_64-windows-gnu`
  (4.7 MB `lmplot.exe`) and `-target aarch64-macos` both link.
- CMake generates, into `build/generated/`: the embedded schema and notices
  (`*_embedded.hpp`), `fontdata.c`, `timestamp.h` (the pinned commit's date,
  so builds are reproducible) and `PostScript/prologues.h` (what upstream's
  `ps_header.sh` writes; the postscript terminal embeds its prologues instead
  of reading `GNUPLOT_PS_DIR` at run time).

## Run

- `lmplot --version` → `lmplot 0.1.0`; `--notices`; `--dsl-schema [--json]`;
  `--dsl-signatures`; `--dsl FILE...`; `gnuplot ...`.
- `--dsl` loads and verifies every bytecode file first, then runs them in
  order, printing `{"ok": true, "result": null}` after each; the first error
  prints a JSON error line and stops (exit 3).

## Tests

`./test.sh` → 17 checks, ~17 s with a warm build:

1. SDK pin; 2. gnuplot pin plus patches; 3. build; 4. `--version` exact;
5. embedded schema byte-identical to `schema/lmplot.dsl.json`; 6. host
registry equals the compiler's `--dsl-signatures`; 7. `--notices` text;
8. static linking (Linux `ldd`, macOS `otool -L`); 9. `lmplot gnuplot` dumb
plot; 10. png and svg terminals listed; 11. all examples compile to bytecode
v5; 12. all examples run and report ok; 13. every example wrote PNGs with a
valid signature; 14. rerun gives byte-identical images (all 75);
15. a failing gnuplot command exits 3 with a `run` error and stops the
program; 16. `plot.data` / `stats` / `plot.num` / `plot.str` round trip;
17. usage (2) and load (3) exit codes.

Gotchas found: `lmplot gnuplot -e 'set terminal'` waits on stdin for the
built-in pager when stdin is open (patch `0006` skips paging unless the
session is interactive; tests also pass `</dev/null`); `rand(0)` is not
seeded, so examples use an LCG in the DSL for random-looking data.
