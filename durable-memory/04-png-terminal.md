# The native PNG terminal

gnuplot's PNG terminals need libgd (+ libpng, zlib, freetype) or Cairo/Pango
(+ glib, fontconfig). None can be linked into a dependency-free static
executable without vendoring dozens of libraries per platform, so lmplot has
its own: terminal names `png` and `pngcairo` (the latter an alias so existing
scripts run unchanged) in `third_party/gnuplot/term/lmpng.trm` (shipped as
`patches/0005-lmpng-terminal.patch`), backed by an independent rasterizer in
`cpp/raster/`.

## Pieces

- `cpp/raster/lmraster.{h,cpp}`: C interface over a C++ rasterizer.
  - **Fill**: scanline conversion with exact horizontal coverage and 16
    vertical sub-scanlines, nonzero or even-odd rule, one coverage mask per
    shape, then straight-alpha compositing onto an RGBA canvas.
  - **Crisp fill** (`lmr_fill_polygon_crisp`): one sample per pixel centre,
    half-open, so neighbouring polygons partition the plane exactly (no seams
    between pm3d quads, boxes land on pixel boundaries).
  - **Stroke**: polylines become convex pieces (one quad per segment, bevel /
    miter / round joins, butt / round / square caps) oriented identically so
    the nonzero union has no holes; dashes are applied along the polyline first.
  - **Pattern fills** (gnuplot's `fs pattern 0..9`): hatch lines masked by the
    polygon coverage.
  - **Text**: `vendor/stb_truetype.h` outlines (quadratic and cubic Béziers
    flattened) filled by the same rasterizer, so any angle and any size is
    anti-aliased and uses the same compositing; italics are a 0.2 shear.
    Glyph outlines are cached per font. No hinting; a 12 % coverage boost
    ("stem darkening") offsets the thin look of unhinted small text.
  - **Fonts**: DejaVu Sans, Sans Bold, Serif and Sans Mono embedded (2.4 MB of
    the 7.4 MB binary; generated into `fontdata.c` by CMake). Font names pick
    a family by keyword: `mono|courier|consol|fixed` → Mono,
    `serif|times|georgia|palatino|bookman|garamond|century` (without `sans`)
    → Serif, `symbol` → Greek/math from the Adobe Symbol encoding mapped to
    Unicode (`{/Symbol a}` gives α), anything else Sans; `bold`, `italic`,
    `oblique` in the name set the style.
  - **PNG writer**: per-row adaptive filtering (none/sub/up/avg/paeth by
    minimum absolute sum), `deflate_zlib` from lmcad (fixed Huffman LZ77,
    deterministic), RGB when fully opaque, RGBA otherwise. Output is byte
    deterministic.
- `lmpng.trm`: the gnuplot terminal driver. Coordinates map to pixel centres
  (`X = x + 0.5`, `Y = H - 1 - y + 0.5`) so 1 px lines at integer
  coordinates are crisp. Line segments from `move/vector` are accumulated into
  a path and stroked as one polyline when the pen state changes, so joins and
  dashes continue across segments.

## Options (`set terminal png ...`)

`size W,H` (default 640,480), `font "name,size"`, `fontscale`, `enhanced` /
`noenhanced` (default enhanced), `transparent`, `crop`, `background <rgb>`,
`linewidth`/`lw`, `dashlength`/`dl`, `butt`/`rounded`/`square`, `color` /
`monochrome`, `tiny small medium large giant`, and a bare number as font
size. `truecolor`, `interlace`, `pointscale` are accepted and ignored.
Text size is points at 96 dpi (12 pt = 16 px), like pngcairo.

## Fidelity decisions

- Colors: `TC_RGB` carries alpha in the top byte (0 = opaque); palettes are
  continuous (`make_palette` returns 0); `TC_LT` maps through gnuplot's
  classic pen table (`SVG`-style) for old-style linetypes.
- `fillbox` and axis-aligned rectangles use crisp fills; `FS_SOLID` with a
  density below 100 blends toward white, `FS_TRANSPARENT_SOLID` uses alpha.
- Images are drawn nearest-neighbour from the pixel grid (gnuplot images are
  pixel-exact).
- Points use the same 15 shapes as the svg terminal, scaled by
  `pointsize * h_tic / 2`.
- Text anchoring follows the terminal convention (vertical centre at the given
  y: the baseline sits `0.36 em` below it); enhanced text runs gnuplot's
  `enhanced_recursion` with measured advances, two passes for centered and
  right-justified strings, overprint save/restore supported.

## Known gaps

- No glyph hinting; text is a little softer than FreeType-hinted output.
- Only the four embedded fonts; font *files* (`font "/path/x.ttf,12"`) are
  not loaded and fall back to Sans.
- No `animate` (gif) or `jpeg`; use `svg` or a PNG series.
- Pattern hatch is drawn in device space with a fixed 8 px tile.
- `tscale` is 1: there is no sub-pixel oversampling of terminal coordinates
  (svg, which uses 1/100 px, is more precise for hairline geometry).
- `hypertext`, `layer`-driven grouping and mouse features do not apply.
