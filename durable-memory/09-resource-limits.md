# Resource limits

Why: on 2026-10-23 a runaway `lmnum` (a DSL loop creating objects) reached 16 GB RSS and filled the
24 GB WSL machine. lmplot has the same exposure through gnuplot: datablocks (`Plot.data`,
`set print $d append`), `array A[N]`, `set samples`, `do for` loops inside one `Plot.cmd`, png
canvases, and DSL strings in the VM all grow without bound.

- **Total memory budget**, default 4096 MiB, set with `LMPLOT_MAX_MEMORY_MB` (positive whole MiB;
  anything else exits 2 with the usage text). Applies to `--dsl` runs and to `lmplot gnuplot`.
- `cpp/memory_budget.{hpp,cpp}` (identical copy in every lm* host) replaces global
  `operator new`/`delete` and counts allocator block sizes: the DSL VM, the host, and the C++
  png rasterizer (`cpp/raster`).
- **gnuplot is C** (malloc, errors by `int_error` = longjmp), so nothing may throw inside it.
  `cpp/gp_memory.h` is force-included (`-include`, C sources of `gnuplot_core` only, no gnuplot
  patch needed) and maps `malloc/calloc/realloc/strdup/free` to counting wrappers in
  `cpp/gp_memory.cpp` (block sizes via `malloc_usable_size`/`malloc_size`/`_msize`, same as
  operator new, so frees balance). `alloc.c` is compiled with `LMPLOT_MEM_CHECKED`: only
  `gp_alloc`/`gp_realloc` can be refused; they get NULL and gnuplot reports "out of memory for
  ..." through its own error path, and `lmplot_gp_do` returns -1 as for any gnuplot error. All big
  gnuplot buffers (arrays, sample/point arrays, datablock line tables, string concatenation) use
  them. Bare `malloc`/`strdup` (≈200 sites, mostly unchecked for NULL) are counted but never
  refused, so they cannot crash gnuplot; a budget trip there is noted.
- Every allocating `lmr_*` entry point of the rasterizer catches exceptions (they would otherwise
  unwind through gnuplot's C frames and abort); a failing `lmr_create` makes the png terminal
  `int_error`. `lmr_create` frees its partial canvas (unique_ptr).
- The host checks `lmplot_mem_failure()` after every gnuplot call (`Host::run`, `Host::eval`,
  `terminals`): if a budget check failed during the command, the run error is the budget message
  (`memory budget exceeded: ... (set LMPLOT_MAX_MEMORY_MB to raise it)`) rather than gnuplot's
  "out of memory", and the program stops (exit 3). The first trip also prints that line on stderr
  (plus gnuplot's own "out of memory" line). `main` catches `std::exception` around load and run
  (budget hit in the VM). Strings gnuplot returns to the host are freed with `lmplot_mem_free`.
- Not covered: frees of blocks that libc allocated internally (only skew the count down);
  overshoot from bare mallocs inside one command is bounded by the next `gp_alloc`.
- No `setrlimit` (not portable to macOS static builds).
- Measured (x86_64 Linux, WSL): the test runaway (`plot.cmd("array A<i>[1000000]")` in a DSL loop,
  ~23 MiB per iteration) stops at 64 MiB in 0.04 s, and at the default 4096 MiB after 3 s with
  4.18 GB peak RSS (4078 MiB, tracked bytes match RSS within 1%). A 10000x5000 png canvas at
  64 MiB fails before drawing; `set samples 1e8` asks for 6104 MiB at once and is refused.
  `set print $d append` + `do for [i=1:1e8] { print ... }` also stops at 64 MiB (in 40 s: the
  append path is slow, not the budget). `test.sh` runs the runaway at 64 MiB and the canvas
  everywhere, the default-budget run on Linux only, under `ulimit -v 6291456`.
