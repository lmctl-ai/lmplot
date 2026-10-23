#!/usr/bin/env bash
# Refresh the checked-in preview images from a fresh `./test.sh` run.
# Usage: ./test.sh && scripts/update-renders.sh
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"
# source image (build/examples/) -> preview name (examples/renders/)
while read -r src dst; do
  [ -f "build/examples/$src" ] || { echo "missing build/examples/$src: run ./test.sh first" >&2; exit 1; }
  cp "build/examples/$src" "examples/renders/$dst"
done <<'LIST'
simple.png simple.png
fillcrvs-1.png fillcrvs.png
histograms-1.png histograms.png
errorbars-1.png errorbars.png
pm3d-2.png pm3d.png
contours-2.png contours.png
heatmaps-2.png heatmaps.png
polar-2.png polar.png
spiderplot-3.png spiderplot.png
parallel-3.png parallel.png
layout-1.png layout.png
enhanced_utf8-1.png enhanced.png
stats.png stats.png
transparent-3.png transparent.png
turntable-3.png turntable.png
LIST
ls examples/renders | wc -l
