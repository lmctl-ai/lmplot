# lmplot durable memory

Design and investigation notes for the lmplot host (gnuplot embedded in one
static executable, scripted by lmscript `_dsl_` bytecode). Read in numeric
order.

- `01-purpose-and-place.md` — what lmplot is; relation to gnuplot,
  cpp-dsl-sdk, lmscript and the sibling hosts; scope boundary.
- `02-gnuplot-embedding.md` — license compatibility and why MIT; the pinned
  checkout plus `patches/`; what is compiled; the embedding hook; Windows
  approach.
- `03-schema-v0.md` — the `Plot` API (12 methods) and its rules.
- `04-png-terminal.md` — the native PNG terminal and rasterizer: pieces,
  options, fidelity decisions, known gaps.
- `05-build-run-test.md` — measured build times and sizes, run modes, the 23
  checks of `test.sh`.
- `06-ci.md` — GitHub Actions matrix, release flow, what the platforms caught.
- `07-examples.md` — the 36 standalone examples, 2 combo examples (`lmcad`/`lmdraw`),
  4 video/GIF examples, `examples/render-all.sh`, and `docs/images/`.
- `08-open-items.md` — open items by area.
