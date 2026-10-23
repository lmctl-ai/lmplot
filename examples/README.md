# Examples

Scientific, engineering, financial and biological showcase programs plus
lmscript ports of demos from the [gnuplot demo
gallery](https://www.gnuplot.info/screenshots/index.html#demos). Every `.lms`
file is self-contained, compiles to bytecode, executes `lmplot` immediately via
`r = system ${dsl_cli(p)} --dsl ${out}; assert(r.code == 0);`, and writes its
output image(s) into `images/<name>.png` in the current working directory. Run
any example directly from any directory:

```sh
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/kepler_rv_fit.lms
```

`lmscript` / `lmctl` uses `lmplot` from `PATH` when present, otherwise it
downloads the release binary into its tool cache; no schema file is needed, and
any demo data files under `examples/data/` are fetched automatically when missing.
`./test.sh` compiles and runs all of them, validates the PNGs, checks that a
second run produces identical bytes, and moves the rendered outputs into
`build/examples/`. `./examples/render-all.sh` regenerates every image in
`docs/images/` and `examples/renders/`.

## Showcase examples (computation + visual story)

| Command | Domain | Story & features |
| --- | --- | --- |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/kepler_rv_fit.lms` | Astrophysics / Statistics | 44 phase-folded Doppler radial-velocity measurements of a warm-Jupiter host star, `stats` round-trip (`plot.num`), derived planet mass $M_p \sin i$ and $\chi^2_\nu$, 2-panel fit + O-C residuals |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/blackbody_planck.lms` | Physics / Spectroscopy | Planck spectral radiance $B_\lambda(\lambda, T)$ across stellar photospheres (3800–6500 K), visible band highlight, Wien displacement locus and bolometric flux $\sigma T^4$ |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/potential_vortex_surface.lms` | Condensed Matter / Surfaces | 3D Ginzburg–Landau double-well free-energy landscape $F(q_x, q_y)$ (`pm3d depthorder`), base contour projection and minimum-energy transition pathway |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/urban_heat_island.lms` | Meteorology / Heatmaps | 24×24 km nocturnal urban heat island anomaly field $\Delta T(x, y)$ (`with image`) paired with West–East and South–North thermal dome transects and river cooling dip |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/phased_array_polar.lms` | Electromagnetics / Polar | 8-element uniform linear antenna array ($d = \lambda/2$) steered to $\theta_0 = 30^\circ$, comparing uniform excitation vs cosine-pedestal side-lobe suppression in polar dB gain |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/quant_trading_dashboard.lms` | Quantitative Finance | 60-day equity regime shift: OHLC candlesticks, 5-day moving average, $\pm 2\sigma$ Bollinger Band envelope, daily volume bars and underwater peak-to-trough drawdown |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/enzyme_michaelis_menten.lms` | Biochemistry / Regression | Michaelis–Menten enzyme saturation curves under competitive and non-competitive inhibition paired with Lineweaver–Burk double-reciprocal OLS regression (`stats`) |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/damped_driven_resonance.lms` | Nonlinear Dynamics | Driven damped harmonic oscillator steady-state magnification $M(\omega/\omega_0)$, resonance peak backbone locus, half-power bandwidth and quadrature phase lag $\phi(\omega/\omega_0)$ |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/quadrature_convergence.lms` | Numerical analysis | Trapezoid, Simpson and Gauss–Legendre on $\int_0^{\pi/2} e^x\cos x\,dx$; `fit` measures orders 2 and 4 (asserted with `plot.assert_close`) and 8 Gauss points beat 1025 Simpson points |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/option_greeks_dashboard.lms` | Quantitative Finance | Black–Scholes call via gnuplot `norm()`: value surface, price vs spot, delta, gamma and vega; reproduces Hull's textbook 4.76 / 0.81 and put–call parity |

## Multi-host combo examples (`examples/combo/`)

Several `_dsl_` blocks in one `.lms` program combine `lmplot` with sibling hosts (`lmcad`, `lmdraw`):

```sh
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/combo/nozzle_cad_plot.lms
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/combo/sir_epidemic_draw_plot.lms
```

| Command | Hosts | Shows |
| --- | --- | --- |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/combo/nozzle_cad_plot.lms` | `lmcad` + `lmplot` | OpenCASCADE revolved de Laval supersonic nozzle with boolean 90° cutaway (`render_ppm`) embedded alongside quasi-1D isentropic Mach, pressure and temperature curves |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/combo/sir_epidemic_draw_plot.lms` | `lmdraw` + `lmplot` | Retained 2D vector compartmental schematic ($S \to E \to I \to R$) paired with midpoint-method (RK2) epidemic trajectories and $I$-vs-$S$ herd-immunity phase portrait |

## Classic gnuplot gallery examples

| Command | Demo | Shows |
| --- | --- | --- |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/simple.lms` | simple.dem | functions with points |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/fillcrvs.lms` | fillcrvs.dem | every `filledcurve` mode, parametric fills |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/fillbetween.lms` | fillbetween.dem | area between two data curves |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/transparent.lms` | transparent.dem | alpha fills and pattern fills |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/errorbars.lms` | errorbars.dem | x/y error bars, boxxyerror, log axis |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/candlesticks.lms` | candlesticks.dem | candlesticks and box-and-whisker |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/boxplot.lms` | boxplot.dem | boxplots, factor colouring |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/histograms.lms` | histograms.dem | clustered, stacked and grouped histograms |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/steps.lms` | steps.dem | steps, fsteps, histeps |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/param.lms` | param.dem | parametric curves and a pm3d sphere |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/polar.lms` | polar.dem | polar functions and grids |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/surface1.lms` | surface1.dem | hidden-line surfaces, labels, arrows |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/hidden.lms` | hidden.dem | hidden3d on explicit surfaces |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/contours.lms` | contours.dem | contour lines on base and surface |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/pm3d.lms` | pm3d.dem | pm3d surfaces, palette, colorbox, depth order |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/heatmaps.lms` | heatmaps.dem | images from matrix and XYZ data |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/vector.lms` | vector.dem | contours into a table, vector field |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/image.lms` | image.dem | binary RGB image, rotated copies |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/layout.lms` | layout.dem | multiplot layouts and stacked panels |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/spiderplot.lms` | spiderplot.dem | spider (radar) plots |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/parallel.lms` | parallel.dem | parallel-axis plots |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/dashtypes.lms` | dashtypes.dem | native and custom dash patterns |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/lines_arrows.lms` | lines_arrows.dem | line, point and arrow styles |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/enhanced_utf8.lms` | enhanced_utf8.dem | enhanced text, Symbol and Unicode |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/rotate_labels.lms` | textbox.dem, rotate_labels.dem | boxed and rotated labels |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/nonlinear2.lms` | nonlinear2.dem | logscale versus `set nonlinear` |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/stats.lms` | (lmplot) | generate data in lmscript, `stats`, compute, label and plot |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/turntable.lms` | (lmplot) | a loop rendering a rotating surface as 8 frames |

## Video & animations (`examples/video/`)

Because a program can write any number of images, a movie or animated GIF is a
loop plus `ffmpeg`. The programs in `video/` render PNG frames from a `_dsl_`
loop, then (in plain lmscript after the block) run `lmplot` on the bytecode and
`ffmpeg` on the frames, writing `images/<name>.mp4` (and `images/quantum_packet.gif`):

```sh
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/quantum_packet.lms
lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/wave.lms
```

| Command | Shows |
| --- | --- |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/quantum_packet.lms` | dispersive quantum wave packet $\psi(x, t)$ scattering on a potential barrier, 60 frames, MP4 + GIF + PNG |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/wave.lms` | a wave packet travelling along a string, 96 frames, 800x450 |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/ripple.lms` | an expanding pm3d ripple with an orbiting camera, 90 frames |
| `lmctl autorun https://raw.githubusercontent.com/lmctl-ai/lmplot/main/examples/video/lissajous.lms` | a Lissajous curve drawn progressively with a moving head, 120 frames |

`system` runs a program without a shell and returns `{code, stdout, ...}`; the
programs check it with `assert(r.code == 0)`. Write the ffmpeg command as plain
words, and the frame names as `frame-%d.png` (no zero padding needed).

`data/` holds the small demo data files the programs read, copied unchanged
from the gnuplot demo directory (gnuplot license, see `../THIRD_PARTY_NOTICES`).

## Writing your own

```
out = ARGV.len() > 0 ? ARGV[0] : "build/mine.dslb";   # compile-time argv: output path
mkdir("images");                                      # ordinary lmscript, outside the block
mkdir("build");
p = _dsl_ cli="lmplot" install="github:lmctl-ai/lmplot@latest";  # host binary supplies the schema
plot.png("images/mine.png", 800, 600);                # inside the block: the Plot API
plot.cmd("plot sin(x)");
plot.close();
end _dsl_
savedsl(p, out);
r = system ${dsl_cli(p)} --dsl ${out};
assert(r.code == 0);
println("wrote images/mine.png");
```

Inside the block strings are single-line, so several gnuplot lines go in
several `plot.cmd` calls or one `plot.script("a\nb\nc")`. Loops, `if`,
arithmetic and `+` on strings work; the block cannot see variables defined
outside it.
