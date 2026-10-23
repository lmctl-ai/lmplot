# Schema v0 (`schema/lmplot.dsl.json`)

`"dsl.schema": 2`, one type `Plot`, one context `plot`, no free functions. 12
methods. The host is the single gnuplot instance of the process; every method
runs immediately.

| method | returns | what it does |
| --- | --- | --- |
| `cmd(string)` | void | run one gnuplot command line (`;` separated commands work) |
| `script(string)` | void | run multi-line text (lines separated by `\n` escapes in the DSL string); written to a temp file and run with `load`, so datablocks and `if`/`do for` clauses behave exactly as in a `.gp` file |
| `load(string)` | void | `load "file"` |
| `data(string, string)` | void | define datablock `$name` (name without `$`) from text |
| `png(string, int, int)` | void | `set terminal png size W,H; set output "file"` |
| `svg(string, int, int)` | void | same for svg |
| `close()` | void | `set output` (flush and close the current output file) |
| `num(string)` | float | evaluate a gnuplot expression, number result |
| `str(string)` | string | evaluate, string result (a number comes back formatted `%.15g`) |
| `terminals()` | string | gnuplot's `GPVAL_TERMINALS` |
| `echo(string)` | void | print a line on stdout |
| `version()` | string | `lmplot X.Y.Z` |
| `assert_close(actual, expected, tol)` | void | run error (exit 3) `assert_close failed: actual=…, expected=…, tol=…` when `|actual-expected| > tol`; non-finite arguments or a negative tolerance are errors |

Rules and gotchas:

- Any gnuplot error stops the program: `RUN_HOST` error `gnuplot: <GPVAL_ERRMSG>`,
  JSON `{"ok": false, "phase": "run", ...}` on stdout, exit code 3. gnuplot's
  own diagnostic with the caret is on stderr.
- `lmplot --dry-run --dsl F` runs the whole program (values, `stats`, `fit`) but
  writes no file: each would-be output is reported as `[dry-run] would write F`
  on stderr and sent to the null device; pipes and `system()` are errors.
- Images are inputs too: `plot 'r.png' binary filetype=png with rgbimage` (or
  `rgbalpha`) and `set pixmap N 'r.png' at screen X,Y width screen W` read PNG,
  JPEG, GIF (first frame), BMP, TGA and PNM. A sibling host's render can be
  placed into an lmplot multiplot dashboard this way. `stats` on an image file
  does not see the pixel columns (a gnuplot quirk); go through `set table $T`.
- Every plot writes one complete PNG into the *current output*. For several
  images use one `plot.png(...)`/`plot.close()` pair per image; plotting twice
  into one output concatenates PNG files.
- `_dsl_` strings are single-line; build multi-line gnuplot text with `\n`
  escapes or several `plot.cmd` calls. Inside a gnuplot command use `'...'`
  or `\"...\"`; gnuplot does not understand `\'` inside single quotes.
- gnuplot state (variables, functions, datablocks, terminal settings) persists
  between calls and between bytecode files of one invocation.
- `plot.num("STATS_slope")` after `stats` etc. is the way to get results back
  into the script; `plot.str` for strings (`sprintf`, `GPVAL_*`, `strftime`).
  A complex result with a non-zero imaginary part (`sqrt(-1)` is `{0,1}` in
  gnuplot) or an array/datablock value is an error, not its real part; NaN
  is returned as NaN (gnuplot uses it for "no value", e.g. empty `stats`).
- `data(name, text)` picks a terminator that no line of `text` starts with
  (gnuplot ends a datablock at the first line *starting with* the marker), so
  data lines beginning with `EOD` stay data. The name must start with a letter.
- After every command the host flushes the output file and stops the program
  on a write error (disk full, `/dev/full`); the terminals only warn.
- Output paths are relative to the current directory; the host does not
  sandbox them (gnuplot has `system` and `set output "|cmd"` as well).

Why no richer objects (Plot handles, Series, Style): gnuplot's command language
already is the object model, and the DSL has no lists or maps to carry such
objects (see lmhtml `06-open-items.md`). `data()` + `cmd()` + `num()` cover
generate-plot-read-back loops without a second description of gnuplot.

Deliberate omissions for v0: no per-call error recovery (`try`), no reading of
whole datablocks back into the script, no image post-processing, no `replot`
helper (just `plot.cmd("replot")`).

## How programs bind to the schema

Programs use `_dsl_ cli="lmplot" install="github:lmctl-ai/lmplot@latest";`, not a
schema file: `lmscript` runs `lmplot --dsl-schema --json` (searching `PATH`
first, then its tool cache, filled on first use from the release asset
`lmplot-<os>-<arch>[.exe]` with its SHA-256 checked), so the bytecode is bound
to the binary that runs it and a downloaded release is all a user needs.
`dsl_cli(p)` gives the argv prefix for running it. `test.sh` puts the build
directory first on `PATH` so examples bind to the binary under test.
`schema/lmplot.dsl.json` remains the source the binary embeds (and
`select-compiler.sh` still probes with `schema=`).
