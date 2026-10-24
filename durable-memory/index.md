# lmplot durable memory

Design and investigation notes for the lmplot host (gnuplot embedded in one
executable, using dynamic glibc >= 2.35 on Linux and scripted by lmscript `_dsl_` bytecode). Read in numeric
order.

- `01-purpose-and-place.md` — what lmplot is; relation to gnuplot,
  lmcode-cpp, lmscript and the sibling hosts; scope boundary.
- `02-gnuplot-embedding.md` — license compatibility and why MIT; the pinned
  checkout plus `patches/`; what is compiled; the embedding hook; Windows
  approach.
- `03-schema-v0.md` — the `Plot` API (12 methods) and its rules.
- `04-png-terminal.md` — the native PNG terminal and rasterizer: pieces,
  options, fidelity decisions, known gaps.
- `05-build-run-test.md` — measured build times and sizes, run modes, the 25
  checks of `test.sh`.
- `06-ci.md` — GitHub Actions matrix, release flow, what the platforms caught.
- `07-examples.md` — the 36 standalone examples, 2 combo examples (`lmcad`/`lmdraw`),
  4 video/GIF examples, `examples/render-all.sh`, and `docs/images/`.
- `08-open-items.md` — open items by area.
- `09-resource-limits.md` — the total memory budget (`LMPLOT_MAX_MEMORY_MB`), how gnuplot's malloc is counted.

- `10-linux-glibc.md` — dynamic system glibc baseline, dependency policy and rollout evidence.
