# Examples

36 standalone programs in `examples/` (83 PNG images), 2 multi-host combo
programs in `examples/combo/` (`lmcad` + `lmplot`, `lmdraw` + `lmplot`), and 4
video/animation programs in `examples/video/` (`lmplot` + `ffmpeg`). All are
checked by `./test.sh` (valid PNG/GIF magic bytes, size budgets, byte-identical
reruns). `./examples/render-all.sh` regenerates the 22 checked-in gallery
assets in `docs/images/` and `examples/renders/`.

## Showcase examples (`examples/*.lms`)

Eight story-driven showcase programs compute domain results in the DSL, round-trip
summary statistics via gnuplot's `stats` and `plot.num`/`plot.str`, and render
900-px wide publication figures (`<= 300 KB` each) into `build/examples/` and
`docs/images/`:

1. `kepler_rv_fit.lms` — 44 synthetic HARPS-like Doppler radial-velocity
   measurements of a warm-Jupiter host star ($P = 18.4\text{ d}$, $K = 42.5\text{ m/s}$,
   $e = 0.14$), `stats` extraction of raw vs O-C residual RMS and reduced
   $\chi^2_\nu$, derived minimum planet mass $M_p \sin i = 0.55\,M_{\text{Jup}}$,
   and 2-panel phase-folded fit + residuals.
2. `blackbody_planck.lms` — Planck spectral radiance $B_\lambda(\lambda, T)$
   across stellar temperatures (3800–6500 K), shaded visible spectrum band
   (380–750 nm), Wien displacement locus ($\lambda_{\max} = b/T$), and solar
   bolometric flux $\sigma T^4$.
3. `potential_vortex_surface.lms` — 3D Ginzburg–Landau double-well free-energy
   surface $F(q_x, q_y)$ with symmetry-breaking field (`pm3d at bs depthorder`),
   projected base contours, and minimum-energy saddle transition pathway.
4. `urban_heat_island.lms` — 24×24 km nocturnal Urban Heat Island anomaly map
   $\Delta T(x, y)$ (`with image`) paired with West–East and South–North
   thermal dome transects and river/park cooling sinks.
5. `phased_array_polar.lms` — 8-element uniform linear phased array ($d = \lambda/2$)
   steered to $\theta_0 = 30^\circ$, comparing uniform illumination vs
   raised-cosine pedestal tapering in polar dB gain (`set polar`).
6. `quant_trading_dashboard.lms` — 60 trading days of equity regime shift:
   bullish/bearish OHLC candlesticks, 5-day moving average, $\pm 2\sigma$
   Bollinger Band envelope, volume bars, and underwater peak-to-trough drawdown.
7. `enzyme_michaelis_menten.lms` — Michaelis–Menten saturation kinetics under
   competitive and non-competitive inhibition paired with Lineweaver–Burk
   double-reciprocal OLS regression (`stats` recovers $V_{\max}$ and $K_m$).
8. `damped_driven_resonance.lms` — Driven damped harmonic oscillator amplitude
   magnification $M(\omega/\omega_0)$ across quality factors $Q \in \{1.5, 3, 6, 10\}$,
   peak backbone locus, half-power bandwidth, and quadrature phase lag $\phi(\omega/\omega_0)$.

## Multi-host combo examples (`examples/combo/`)

Several `_dsl_` blocks in a single `.lms` script bind to sibling hosts (`lmcad`,
`lmdraw`) alongside `lmplot`:

- `combo/nozzle_cad_plot.lms` (`lmcad` + `lmplot`) — `lmcad` revolves a contoured
  de Laval supersonic rocket nozzle with a concentric copper cooling collar in
  OpenCASCADE, cuts away a 90° quadrant (`Shape.cut`), verifies `Shape.volume()`,
  and writes a 380×420 raw PPM (`scene.render_ppm`). `lmplot` solves the
  quasi-1D isentropic area–Mach relation ($M_e = 3.10$) and embeds the 3D CAD
  render directly inside a 2-panel `multiplot` via `binary array=(380,420)
  skip=15 flipy format='%uchar' with rgbimage`.
- `combo/sir_epidemic_draw_plot.lms` (`lmdraw` + `lmplot`) — `lmdraw` renders a
  360×560 retained 2D vector schematic of the 4-compartment SEIR model
  ($S \to E \to I \to R$), while `lmplot` integrates the coupled ODEs with RK4
  and plots the epidemic trajectories and $I$-vs-$S$ herd-immunity phase plane
  ($S^* = 1/R_0$), combined side-by-side into a 900×560 figure.

## Classic gnuplot gallery examples

26 programs are lmscript versions of demos in the gnuplot gallery
(https://www.gnuplot.info/screenshots/index.html#demos; sources in
`third_party/gnuplot/demo/*.dem`), plus `stats.lms` and `turntable.lms`. The
complete table is in `examples/README.md`.

## What the `_dsl_` language does and does not allow (found writing these)

- Strictly typed, schema-driven: floats are written `1.0` (no `1.`), no
  f-strings inside the block, no list literals, no `let` of untyped values;
  `for (i in range(a, b))`, `if`, `switch`, `fn` helpers and string `+` with
  `str()` work; `return` must be the last top-level statement of a function.
- Variables defined outside the block are invisible inside it.
- String literals end on their line: multi-line gnuplot text needs `\n`
  escapes (`plot.script`) or several `plot.cmd` calls. gnuplot does not
  process `\n` inside single-quoted strings (use `\"...\\n...\"` for titles
  with line breaks).
- In gnuplot enhanced-text titles and axis labels, bare `&` is the enhanced
  whitespace/overprint operator; use `and`, `—`, or `noenhanced` when `&` is not
  intended as spacing.
- Random-looking data: gnuplot's `rand(0)` is unseeded across runs, so
  examples use a deterministic LCG written in the DSL, keeping all images
  byte-identical on rerun.

## Video & animated GIF examples (`examples/video/`)

`quantum_packet.lms`, `wave.lms`, `ripple.lms`, `lissajous.lms`: a `_dsl_` loop
writes `build/video/<name>/frame-<k>.png`, then plain lmscript runs
`system ${dsl_cli(p)} --dsl out.dslb;` and `ffmpeg` to produce
`build/video/<name>.mp4` (and `build/video/quantum_packet.gif`, copied to
`docs/images/quantum_packet.gif` by `examples/render-all.sh`). `test.sh` runs
them when `ffmpeg` is installed and verifies GIF/MP4 outputs.
