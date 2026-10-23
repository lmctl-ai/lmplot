#!/usr/bin/env bash
# Regenerate every image in docs/images/ (and refresh examples/renders/) from the
# example .lms programs in examples/, examples/combo/ and examples/video/.
set -euo pipefail
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$HERE"

BUILD_DIR="${BUILD_DIR:-build}"
rm -rf images
mkdir -p "$BUILD_DIR/examples" "$BUILD_DIR/video" docs/images images
trap 'rm -rf "$HERE/images"' EXIT

# Put the local build and any cached sibling tools on PATH first.
export PATH="$(cd "$BUILD_DIR" 2>/dev/null && pwd || echo "$HERE/$BUILD_DIR"):$PATH"
for tool_dir in "$HOME/.cache/lmscript/tools/github/lmctl-ai"/*/latest; do
  case "$tool_dir" in */lmplot/latest) continue;; esac
  [ -d "$tool_dir" ] && export PATH="$tool_dir:$PATH"
done

LMSCRIPT="$(scripts/select-compiler.sh)"
HOST="$BUILD_DIR/lmplot"
case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*) HOST="$HOST.exe";; esac

if [ ! -x "$HOST" ]; then
  echo "building lmplot in $BUILD_DIR..."
  cmake -S . -B "$BUILD_DIR" -DCMAKE_BUILD_TYPE=Release >/dev/null
  cmake --build "$BUILD_DIR" >/dev/null
fi

# 1. Compile and self-execute all standalone lmplot examples in examples/*.lms
for src in examples/*.lms; do
  name="$(basename "$src" .lms)"
  "$LMSCRIPT" "$src" "$BUILD_DIR/$name.dslb" >/dev/null
done
mv images/*.png "$BUILD_DIR/examples/"

# Refresh examples/renders/ and copy classic gallery previews to docs/images/
scripts/update-renders.sh >/dev/null
for img in pm3d.png fillcrvs.png histograms.png contours.png heatmaps.png spiderplot.png stats.png enhanced.png transparent.png polar.png; do
  cp "examples/renders/$img" "docs/images/$img"
done

# Copy the 8 new standalone showcase renders to docs/images/
for showcase in \
  kepler_rv_fit.png \
  blackbody_planck.png \
  potential_vortex_surface.png \
  urban_heat_island.png \
  phased_array_polar.png \
  quant_trading_dashboard.png \
  enzyme_michaelis_menten.png \
  damped_driven_resonance.png
do
  cp "$BUILD_DIR/examples/$showcase" "docs/images/$showcase"
done

# 2. Multi-host combo examples (lmcad + lmplot, lmdraw + lmplot)
if command -v lmcad >/dev/null 2>&1; then
  "$LMSCRIPT" examples/combo/nozzle_cad_plot.lms >/dev/null
  mv images/nozzle_cad_plot.png "$BUILD_DIR/examples/nozzle_cad_plot.png"
  cp "$BUILD_DIR/examples/nozzle_cad_plot.png" docs/images/nozzle_cad_plot.png
else
  echo "note: skipping nozzle_cad_plot.lms regeneration (lmcad not on PATH)"
fi

if command -v lmdraw >/dev/null 2>&1 && command -v ffmpeg >/dev/null 2>&1; then
  "$LMSCRIPT" examples/combo/sir_epidemic_draw_plot.lms >/dev/null
  mv images/sir_epidemic_draw_plot.png "$BUILD_DIR/examples/sir_epidemic_draw_plot.png"
  cp "$BUILD_DIR/examples/sir_epidemic_draw_plot.png" docs/images/sir_epidemic_draw_plot.png
else
  echo "note: skipping sir_epidemic_draw_plot.lms regeneration (lmdraw or ffmpeg not on PATH)"
fi

# 3. Video / animation examples via ffmpeg
if command -v ffmpeg >/dev/null 2>&1; then
  "$LMSCRIPT" examples/video/quantum_packet.lms >/dev/null
  mv images/quantum_packet.gif "$BUILD_DIR/video/quantum_packet.gif"
  mv images/quantum_packet.mp4 "$BUILD_DIR/video/quantum_packet.mp4"
  mv images/quantum_packet.png "$BUILD_DIR/video/quantum_packet.png"
  cp "$BUILD_DIR/video/quantum_packet.gif" docs/images/quantum_packet.gif
else
  echo "note: skipping quantum_packet.gif regeneration (ffmpeg not on PATH)"
fi

rm -rf images
echo "docs/images/ updated ($(ls -1 docs/images | wc -l | tr -d ' ') files)"
