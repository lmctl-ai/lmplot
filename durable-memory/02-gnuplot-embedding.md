# Embedding gnuplot

Machine checked 2026-10-22 against gnuplot commit
`fbeb88eadedf927a4d778b41dd118e373f33eacb` (6.1.0-dev, "svg: track color of
pattern fill").

## License compatibility

gnuplot's license (`third_party/gnuplot/Copyright`, reproduced in
`THIRD_PARTY_NOTICES`) is permissive. It allows use, copying and distribution
with the notice kept. It allows modification, but *not* distributing the
complete modified source: modifications are distributed as **patches against
the released version**. Binaries built from modified sources may be
distributed if the distributor (1) ships the corresponding patch file with the
binaries, (2) adds special version identification, (3) gives their own contact
for support, and (4) retains gnuplot's contact information.

lmplot's choices, which are why the repository is **MIT** (the same license as
every sibling host) and not GPL:

- No gnuplot file is committed. `scripts/sync-gnuplot.sh` fetches the pinned
  commit and applies `patches/*.patch`; `--verify` (run by `test.sh`) fails if
  the tree is anything other than "pin plus those patches".
- Releases attach `lmplot-gnuplot-patches.tar.gz` (patches + `gnuplot.lock`)
  next to the binaries, and the binary embeds the notices (`lmplot --notices`).
- `Version 6.1 patchlevel 0 (lmplot X.Y.Z)` (patch `0004`) is the special
  version identification; `DIST_CONTACT` in `cpp/gpconfig/config.h` gives
  lmplot's contact and `show version` keeps gnuplot's.
- lmplot's own code (the host, the DSL schema, the rasterizer in `cpp/raster`,
  the embedding hook) is MIT. The one file that follows gnuplot's own driver
  code, the PNG terminal driver `lmpng.trm` (its enhanced-text handling mirrors
  `gd.trm`/`svg.trm`), is treated as a modification of gnuplot: it is committed
  only as `patches/0005-lmpng-terminal.patch`, under gnuplot's terms.
- GPL-compatibility: gnuplot's license is GPL-compatible, MIT is too; nothing
  here links GPL code (the libstdc++/glibc/mingw runtime notes are in the
  notices file).

## What is compiled

About 60 gnuplot sources from `src/` (the list is `GP_SRC` in
`CMakeLists.txt`, taken from upstream's `gnuplot_SOURCES` minus the GUI
files). `plot.c` is compiled with `-Dmain=gnuplot_cli_main`, which makes
upstream's own `main` the implementation of `lmplot gnuplot ...`.

`cpp/gpconfig/config.h` replaces autoconf's `config.h`: one hand-written
header with `HAVE_*` per platform (`__linux__`, macOS, `LMPLOT_WINDOWS`). It
enables only dependency-free features and never defines `HAVE_LIBGD`,
`HAVE_CAIROPDF`, `HAVE_LIBZ`, `HAVE_LIBREADLINE`, `X11`, `WXWIDGETS`, `QTTERM`,
`HAVE_LUA`, `HAVE_ICONV`, `HAVE_LIBCERF`. Without a configure step the build is
the same CMake call on all platforms.

Terminals come from `src/term.h` (`#include "x.trm"` per feature macro), so
the available set follows from the macros above plus patch `0002`.

## Patches

| patch | purpose |
| --- | --- |
| `0001-post-deflate-brace.patch` | upstream bug at this commit: `post.trm` has an `} else {` whose `{` is inside `#ifdef HAVE_DEFLATE_ENCODER`; without it the postscript terminal does not compile when no deflate encoder exists. Adds the missing `{`. |
| `0002-term-list.patch` | `term.h` includes `lmpng.trm` (`#ifdef LMPLOT_EMBED`) and no longer includes the Windows GUI `win.trm` in embedded builds. |
| `0003-embed-hook.patch` | one line at the end of `plot.c` includes `cpp/lmplot_embed.inc`, giving host code access to `plot.c`'s statics; `restrict_popen()` also refuses pipes and shell commands under `--dry-run`. |
| `0004-version-id.patch` | `version.c`: patchlevel `0 (lmplot X.Y.Z)`. |
| `0005-lmpng-terminal.patch` | adds `term/lmpng.trm`, the `png`/`pngcairo` terminal driver (see `04-png-terminal.md`). |
| `0006-no-pager-when-scripted.patch` | `help.c`: the built-in pager (`set terminal`, `help`) reads stdin for "Press return for more"; skip it unless `interactive`, so a script never blocks. |
| `0007-dry-run.patch` | `--dry-run`: every file gnuplot opens for writing (`set output`, `set print`, `save`, `set table`, the fit log) goes through `lmplot_dry_fopen()` (in `lmplot_embed.inc`), which, when `lmplot_dry_run` is set, prints `[dry-run] would write F` on stderr and opens the null device instead. Reads are untouched. |

Workflow: `scripts/sync-gnuplot.sh` (checkout + apply), edit files under
`third_party/gnuplot`, then `scripts/save-patches.sh` regenerates the patch
files (one logical change per patch, mapping inside the script).

## The embedding hook (`cpp/lmplot_embed.inc`)

gnuplot is a program, not a library: global state, `exit()` on fatal errors,
`longjmp` to `command_line_env` on `int_error`. The hook, compiled inside
`plot.c`, adds a small C API:

- `lmplot_gp_init()` repeats what `main` does before it reads arguments (user
  variables, constants, `init_memory`, `init_terminal`, `init_session`),
  non-interactive, with `~/.gnuplot` skipped.
- `lmplot_gp_do(cmd)` runs `do_string(cmd)` under `SETJMP(command_line_env)`.
  An error longjmps back here, the loop/clause state is reset
  (`clause_reset_after_error`, `lf_reset_after_error`) and the function returns
  -1; the message is in `GPVAL_ERRMSG`, which the host puts into the thrown
  `dslvm::Error` (`gnuplot: <message>`; gnuplot also prints its own caret
  diagnostic on stderr).
- `lmplot_gp_eval(expr)` evaluates `lmplot_result__ = (expr)` and returns the
  number or string (`Plot.num`, `Plot.str`).
- `lmplot_gp_shutdown()` is `gp_exit_cleanup()` (runs exit handlers; calling
  `term_reset` directly leaves "exiting abnormally" noise).
- `exit` / `quit` inside `Plot.cmd` just end that command; the process does not
  exit and later statements run.

One gnuplot instance exists per process, shared by all bytecode files in one
`lmplot --dsl a.dslb b.dslb` invocation (state carries over deliberately).

## Windows (cross-compiled, tested in CI)

gnuplot's own Windows port is a GUI program (GDI+, Direct2D, console window,
HTML help). lmplot does not use it. `config.h` includes the system headers
first (they need `_WIN32`), then `#undef _WIN32 __MINGW32__ __MINGW64__`, so
gnuplot's own sources take their portable POSIX/stdio branches, with
`HAVE_*` set to what mingw-w64 has. `READLINE` and `GNUPLOT_HISTORY` are off on
Windows (the built-in readline needs termios). `main.cpp` keeps `_WIN32` and
sets stdout/stderr to binary mode.

Checked locally by cross compiling with `zig cc -target x86_64-windows-gnu`
(links to `lmplot.exe`, 4.7 MB) and `-target aarch64-macos`; the Windows
binary itself is only run in CI.
