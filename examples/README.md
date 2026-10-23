# Examples

Scientific, engineering, financial and biological showcase programs plus
lmscript ports of demos from the [gnuplot demo
gallery](https://www.gnuplot.info/screenshots/index.html#demos). Each `.lms`
file compiles to bytecode and writes PNG images into `build/examples/`:

```sh
lmscript examples/kepler_rv_fit.lms build/kepler_rv_fit.dslb && build/lmplot --dsl build/kepler_rv_fit.dslb
```

`lmscript` uses `lmplot` from `PATH` when present, otherwise it downloads the
release binary into its tool cache; no schema file is needed. Run from the
repository root (data files are loaded by repo-relative paths).
`./test.sh` compiles and runs all of them, validates the PNGs and checks that
a second run produces identical bytes. `./examples/render-all.sh` regenerates
every image in `docs/images/` and `examples/renders/`.

## Showcase examples (computation + visual story)

| Program | Domain | Story & features |
| --- | --- | --- |
| `kepler_rv_fit.lms` | Astrophysics / Statistics | 44 phase-folded Doppler radial-velocity measurements of a warm-Jupiter host star, `stats` round-trip (`plot.num`), derived planet mass $M_p \sin i$ and $\chi^2_\nu$, 2-panel fit + O-C residuals |
| `blackbody_planck.lms` | Physics / Spectroscopy | Planck spectral radiance $B_\lambda(\lambda, T)$ across stellar photospheres (3800–6500 K), visible band highlight, Wien displacement locus and bolometric flux $\sigma T^4$ |
| `potential_vortex_surface.lms` | Condensed Matter / Surfaces | 3D Ginzburg–Landau double-well free-energy landscape $F(q_x, q_y)$ (`pm3d depthorder`), base contour projection and minimum-energy transition pathway |
| `urban_heat_island.lms` | Meteorology / Heatmaps | 24×24 km nocturnal urban heat island anomaly field $\Delta T(x, y)$ (`with image`) paired with West–East and South–North thermal dome transects and river cooling dip |
| `phased_array_polar.lms` | Electromagnetics / Polar | 8-element uniform linear antenna array ($d = \lambda/2$) steered to $\theta_0 = 30^\circ$, comparing uniform excitation vs cosine-pedestal side-lobe suppression in polar dB gain |
| `quant_trading_dashboard.lms` | Quantitative Finance | 60-day equity regime shift: OHLC candlesticks, 5-day moving average, $\pm 2\sigma$ Bollinger Band envelope, daily volume bars and underwater peak-to-trough drawdown |
| `enzyme_michaelis_menten.lms` | Biochemistry / Regression | Michaelis–Menten enzyme saturation curves under competitive and non-competitive inhibition paired with Lineweaver–Burk double-reciprocal OLS regression (`stats`) |
| `damped_driven_resonance.lms` | Nonlinear Dynamics | Driven damped harmonic oscillator steady-state magnification $M(\omega/\omega_0)$, resonance peak backbone locus, half-power bandwidth and quadrature phase lag $\phi(\omega/\omega_0)$ |

## Multi-host combo examples (`examples/combo/`)

Several `_dsl_` blocks in one `.lms` program combine `lmplot` with sibling hosts (`lmcad`, `lmdraw`):

```sh
lmscript examples/combo/nozzle_cad_plot.lms
lmscript examples/combo/sir_epidemic_draw_plot.lms
```

| Program | Hosts | Shows |
| --- | --- | --- |
| `combo/nozzle_cad_plot.lms` | `lmcad` + `lmplot` | OpenCASCADE revolved de Laval supersonic nozzle with boolean 90° cutaway (`render_ppm`) embedded alongside quasi-1D isentropic Mach, pressure and temperature curves |
| `combo/sir_epidemic_draw_plot.lms` | `lmdraw` + `lmplot` | Retained 2D vector compartmental schematic ($S \to E \to I \to R$) paired with RK4 epidemic trajectories and $I$-vs-$S$ herd-immunity phase portrait |

## Classic gnuplot gallery examples

| Program | Demo | Shows |
| --- | --- | --- |
| `simple.lms` | simple.dem | functions with points |
| `fillcrvs.lms` | fillcrvs.dem | every `filledcurve` mode, parametric fills |
| `fillbetween.lms` | fillbetween.dem | area between two data curves |
| `transparent.lms` | transparent.dem | alpha fills and pattern fills |
| `errorbars.lms` | errorbars.dem | x/y error bars, boxxyerror, log axis |
| `candlesticks.lms` | candlesticks.dem | candlesticks and box-and-whisker |
| `boxplot.lms` | boxplot.dem | boxplots, factor colouring |
| `histograms.lms` | histograms.dem | clustered, stacked and grouped histograms |
| `steps.lms` | steps.dem | steps, fsteps, histeps |
| `param.lms` | param.dem | parametric curves and a pm3d sphere |
| `polar.lms` | polar.dem | polar functions and grids |
| `surface1.lms` | surface1.dem | hidden-line surfaces, labels, arrows |
| `hidden.lms` | hidden.dem | hidden3d on explicit surfaces |
| `contours.lms` | contours.dem | contour lines on base and surface |
| `pm3d.lms` | pm3d.dem | pm3d surfaces, palette, colorbox, depth order |
| `heatmaps.lms` | heatmaps.dem | images from matrix and XYZ data |
| `vector.lms` | vector.dem | contours into a table, vector field |
| `image.lms` | image.dem | binary RGB image, rotated copies |
| `layout.lms` | layout.dem | multiplot layouts and stacked panels |
| `spiderplot.lms` | spiderplot.dem | spider (radar) plots |
| `parallel.lms` | parallel.dem | parallel-axis plots |
| `dashtypes.lms` | dashtypes.dem | native and custom dash patterns |
| `lines_arrows.lms` | lines_arrows.dem | line, point and arrow styles |
| `enhanced_utf8.lms` | enhanced_utf8.dem | enhanced text, Symbol and Unicode |
| `rotate_labels.lms` | textbox.dem, rotate_labels.dem | boxed and rotated labels |
| `nonlinear2.lms` | nonlinear2.dem | logscale versus `set nonlinear` |
| `stats.lms` | (lmplot) | generate data in lmscript, `stats`, compute, label and plot |
| `turntable.lms` | (lmplot) | a loop rendering a rotating surface as 8 frames |

## Video & animations (`examples/video/`)

Because a program can write any number of images, a movie or animated GIF is a
loop plus `ffmpeg`. The programs in `video/` render PNG frames from a `_dsl_`
loop, then (in plain lmscript after the block) run `lmplot` on the bytecode and
`ffmpeg` on the frames, so one command does it all (needs `ffmpeg` on `PATH`):

```sh
lmscript examples/video/quantum_packet.lms   # -> build/video/quantum_packet.{mp4,gif}
lmscript examples/video/wave.lms             # -> build/video/wave.mp4
```

| Program | Shows |
| --- | --- |
| `video/quantum_packet.lms` | dispersive quantum wave packet $\psi(x, t)$ scattering on a potential barrier, 60 frames, MP4 + GIF |
| `video/wave.lms` | a wave packet travelling along a string, 96 frames, 800x450 |
| `video/ripple.lms` | an expanding pm3d ripple with an orbiting camera, 90 frames |
| `video/lissajous.lms` | a Lissajous curve drawn progressively with a moving head, 120 frames |

`system` runs a program without a shell and returns `{code, stdout, ...}`; the
programs check it with `assert(r.code == 0)`. Write the ffmpeg command as plain
words, and the frame names as `frame-%d.png` (no zero padding needed).

`data/` holds the small demo data files the programs read, copied unchanged
from the gnuplot demo directory (gnuplot license, see `../THIRD_PARTY_NOTICES`).

## Writing your own

```
out = ARGV.len() > 0 ? ARGV[0] : "build/mine.dslb";   # compile-time argv: output path
mkdir("build/examples");                              # ordinary lmscript, outside the block
p = _dsl_ cli="lmplot" install="github:lmctl-ai/lmplot@latest";  # host binary supplies the schema
plot.png("build/examples/mine.png", 800, 600);        # inside the block: the Plot API
plot.cmd("plot sin(x)");
plot.close();
end _dsl_
savedsl(p, out);
```

Inside the block strings are single-line, so several gnuplot lines go in
several `plot.cmd` calls or one `plot.script("a\nb\nc")`. Loops, `if`,
arithmetic and `+` on strings work; the block cannot see variables defined
outside it.
