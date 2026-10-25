# Build, run, test

Machine checked 2026-10-22 on x86_64 Linux (WSL2), 4 cores, GCC.

## Toolchain

- lmcode-cpp header `cpp/dslvm.hpp` pinned by `sdk.lock` (commit
  `ef861844`, same bytes as lmhtml's copy, sha256 `6a537aa6...`).
  `scripts/sync-sdk.sh --verify` is the first `test.sh` check.
- gnuplot pinned by `gnuplot.lock` (`fbeb88ea`, 2023-09-08, 6.1.0-dev),
  shallow-fetched into git-ignored `third_party/gnuplot` and patched by
  `scripts/sync-gnuplot.sh` (`--verify` hashes the patch set).
- Compiler for `.lms`: `scripts/select-compiler.sh`; `LMSCRIPT_BIN` overrides.
  CI pins `@lmctl-ai/lmctl@0.2.420` (verified locally).
- Vendored, small: `vendor/stb_truetype.h`, four DejaVu fonts,
  `cpp/raster/deflate.hpp` (lmcad).

## Build

- Clean Release build (cmake + ninja, 4 cores): **~14 s**; that is the
  whole of gnuplot (about 60 C files) plus the host.
- Local v0.2.1 Linux x64 binary: 5,584,200 bytes stripped.
- Linux binaries use dynamic glibc >= 2.35 and static libstdc++/libgcc.
  All third-party code is static, including the embedded fonts. test.sh checks
  the ELF NEEDED allowlist and rejects GLIBC symbol requirements above 2.35.
  Published builds use Ubuntu 22.04 for both Linux architectures.
- Cross compile checks, not run locally: `zig cc -target x86_64-windows-gnu`
  (4.7 MB `lmplot.exe`) and `-target aarch64-macos` both link.
- CMake generates, into `build/generated/`: the embedded schema and notices
  (`*_embedded.hpp`), `fontdata.c`, `timestamp.h` (the pinned commit's date,
  so builds are reproducible) and `PostScript/prologues.h` (what upstream's
  `ps_header.sh` writes; the postscript terminal embeds its prologues instead
  of reading `GNUPLOT_PS_DIR` at run time).

## Run

- `lmplot --version` → `lmplot 0.2.1`; `--notices`; `--dsl-schema [--json]`;
  `--dsl-signatures`; `--dsl FILE...`; `gnuplot ...`.
- `--dsl` loads and verifies every bytecode file first, then runs them in
  order, printing `{"ok": true, "result": null}` after each; the first error
  prints a JSON error line and stops (exit 3).

## Tests

`./test.sh` → 30 checks, ~20 s with a warm build:

1. SDK pin; 2. gnuplot pin plus patches; 3. build; 4. `--version` exact;
5. embedded schema byte-identical to `schema/lmplot.dsl.json`; 6. host
registry equals the compiler's `--dsl-signatures`; 7. `--notices` text;
8. system-library policy (Linux ELF/glibc allowlist, macOS `otool -L`, Windows `objdump -p` shows
no MinGW runtime DLLs); 9. `lmplot gnuplot` dumb plot; 10. `lmplot gnuplot`
defaults to the dumb terminal (no window system exists); 11. png and svg
terminals listed; 12. all README `.lms` links exist; 13. all README
`docs/images/*` files exist with valid PNG/GIF magic bytes and size budgets;
14. all 45 standalone examples compile to bytecode v5; 15. all 45 examples run
and report ok; 16. every example wrote PNGs with a valid signature; 17. rerun
gives byte-identical images (all 118); 18–19. multi-host combo examples
(`lmcad` + `lmplot`, `lmdraw` + `lmplot`) run when sibling hosts are on `PATH`
(or skip with a clear message); 20. video examples render frames and encode
MP4/GIF with `ffmpeg`; 21. a failing gnuplot command exits 3 with a `run` error
and stops the program; 22. `plot.data` / `stats` / `plot.num` / `plot.str`
round trip; 23. `Plot.num` complex/array rejection, `EOD` datablock lines, and
write error handling; 24. memory budget (runaway loop and an oversized png canvas
at `LMPLOT_MAX_MEMORY_MB=64`, bad value exits 2, default 4096 MiB under
`ulimit -v` on Linux; see `09-resource-limits.md`); 25. PNG input: an lmplot PNG read back with
`binary filetype=png` (pixel values via `set table`) and placed with `set
pixmap`, a non-image file is a run error; 26. `--dry-run` runs the program, reports
and writes no files (`set output`/`print`/`save`/`table`), and refuses
`system()`; 27. usage (2) and load (3) exit codes.

Gotchas found: `lmplot gnuplot -e 'set terminal'` waits on stdin for the
built-in pager when stdin is open (patch `0006` skips paging unless the
session is interactive; tests also pass `</dev/null`); `rand(0)` is not
seeded, so examples use an LCG in the DSL for random-looking data.
