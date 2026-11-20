# Browser build (WebAssembly)

Added 2026-11-20. Published at https://lmctl.com/tools/lmplot/demo.html.

## Idea

Two layers. The system layer is `lmplot.wasm`: gnuplot (same pin and patches), the PNG rasterizer and
DejaVu fonts, lmcode-cpp and the `Plot` registry from `cpp/main.cpp`, compiled once. The application
layer is lmcode bytecode, loaded at run time from a URL or shipped as an asset. Changing or adding a
plot never rebuilds wasm. This matches the lm2d/lm3d TeaVM model (host exposes typed functions,
lmcode drives configuration and drawing), with C/C++ through Emscripten instead of Java through TeaVM.

## Pieces

- `scripts/build-wasm.sh`: `emcmake cmake` + build of the `lmplot` target into `build-wasm/`
  (`lmplot.js` + `lmplot.wasm`, about 4.6 MB). Emscripten 7.0.0 is pinned, as in lmlive; `EMSDK`
  reuses an install, otherwise emsdk is cloned into git-ignored `third_party/emsdk`.
- `CMakeLists.txt` `if(EMSCRIPTEN)`: `-fwasm-exceptions -sSUPPORT_LONGJMP=wasm` on everything
  (gnuplot recovers from errors with setjmp/longjmp, the host throws C++ exceptions; both use native
  wasm EH), `FE_UNDERFLOW=0` for complexfun.c (wasm has no FP exception flags), and link flags
  `MODULARIZE` (`createLmplot`), `INVOKE_RUN=0`, `FORCE_FILESYSTEM`, `ALLOW_MEMORY_GROWTH`,
  `MAXIMUM_MEMORY=4GB`, exported `callMain, FS, ENV`. No gnuplot patch was needed.
- `cpp/memory_budget.cpp`: default budget 2048 MiB under `__EMSCRIPTEN__`. The native default,
  4096 MiB, overflows wasm32's 32-bit size_t to 0 and refused every allocation.
- `cpp/main.cpp`: `LMPLOT_PNG_AS=svg` makes `Plot.png(FILE, w, h)` use `set terminal svg size w,h
  background '#ffffff'` and write FILE with `.png` replaced by `.svg`. Any other value but `png` is a
  run error. Native too; `test.sh` checks it.
- `web/lmplot-runner.js` (UMD, used by the Worker and by Node): instantiates the compiled module
  afresh per run (fresh gnuplot state; the compile is cached), sets ENV in `preRun` (the C
  environment is built at startup, so setting `ENV` later has no effect), writes data files and the
  program into MEMFS under `/work`, `callMain(['--lmcode', '/work/program.dslb'])`, parses the
  `LMCODE_RETURN_BEGIN/END` frame from stdout and returns the files under `/work/images`.
- `web/worker.js` owns the wasm; `web/boot.js` only posts `{program, output}`; images are shown as
  `data:` URLs (the site CSP allows `img-src data:` but not `blob:`).
- `scripts/precompile-examples.sh`: runs every example natively through lmscript with a shim named
  lmplot first on PATH that copies the private program file. The lmscript around the block (data
  generation, `${...}` captures) has already been evaluated, so all 45 examples, including the six
  that compute in lmscript, become complete programs. Each example is exactly one program.
- `scripts/build-site.py`: `build-site/demo.html` (gallery) and `build-site/browser/` (45
  `demo-<name>.html`, the host, `examples/<name>.bin`, `data/<file>.bin`, thumbnails under 30000 bytes,
  `examples.json`). Titles live in the script; descriptions and sections come from examples/README.md.
- `scripts/build-web.sh`: build-wasm, precompile, wasm test run, site.

## Verification (2026-11-20)

- `tests/wasm-examples.mjs` (Node): 45/45 examples succeed in SVG and PNG modes with the same image
  names as native. PNG mode: 84 of 92 images byte-identical to this machine's native build (85 in the glibc 2.28 CI image, whose libm differs again); the others (contours-3,
  hidden-1..3, option_greeks_dashboard, param-3, surface1-1, turntable-4) are 3D views differing in
  7 to 925 raw bytes out of about a million (libm sin/cos last-bit differences in projection).
- `tests/browser-site.mjs` (headless Chrome via Playwright, `tests/serve-site.py` sends site-like
  headers): example pages in both modes, then "Run all" on the gallery: 45/45, 92 SVG files, about
  830 ms total execution; host fetch and compile about 30 ms from localhost.
- With lmctl.com's CSP at the time (`default-src 'self'` and no `'wasm-unsafe-eval'`), Chrome refuses
  `WebAssembly.compile`. The site needs `script-src 'self' 'wasm-unsafe-eval'` (allows wasm
  compilation only, not JS eval) for the demo to work.
- `test.sh` runs the wasm check when `build-wasm/` exists, otherwise reports it skipped.

## Site integration (lmctl-ai/lmctl)

- Pages live in `tools/lmplot/`: `demo.html` plus `boot.js`, `demo.css`, `favicon.svg` top level
  (one "Demo" nav entry), everything else under `browser/`. The tools contract allows demo pages only
  `boot.js`/`teavm.js` scripts and `demo.css`/`favicon.svg` links from their own directory, so those
  three files exist at both levels.
- `scripts/tools/static-site.py` gained `.wasm` (`application/wasm`) for the tools area.
- `scripts/tools/decorate.lms`: demo pages in a tool subdirectory are no longer wrapped in the doc
  shell or listed in the nav (before, subdirectory pages were always decorated).

## Limits and next steps

- No shell in the browser: `system`, pipes and `set output "|cmd"` fail.
- Programs are precompiled. Next: compile lmcode in the page with the browser compiler
  (`@lmctl-ai/lmcode-browser`, vendored in lmctlhq/lmlive as `lmcode-browser.min.js`;
  `LmcodeCompiler.compileBody(schemaBytes, lines)`) so readers can edit and rerun. It compiles a
  `_lmcode_` body only; the six examples that compute in lmscript would need their math moved into
  lmcode (the std library has FloatList, math_pow and the str_ functions).
- Each run instantiates the module again (a few ms); a long-lived instance would need gnuplot
  `reset session` plus host state cleanup between programs.
