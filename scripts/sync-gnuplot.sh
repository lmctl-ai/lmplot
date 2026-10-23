#!/usr/bin/env bash
# Fetch gnuplot into third_party/gnuplot (git-ignored) at the commit pinned in
# gnuplot.lock, then apply patches/*.patch. Gnuplot's license requires that
# modifications travel as patches against the released source, so the tree is
# always "pinned commit + these patches" and nothing else.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LOCK="$HERE/gnuplot.lock"
DIR="$HERE/third_party/gnuplot"
[ -f "$LOCK" ] || { echo "gnuplot.lock missing" >&2; exit 1; }
repo="$(sed -n 's/^repo=//p' "$LOCK")"
commit="$(sed -n 's/^commit=//p' "$LOCK")"
patched_marker="$DIR/.lmplot-patched"
sha() { if command -v sha256sum >/dev/null 2>&1; then sha256sum | cut -c1-64; else shasum -a 256 | cut -c1-64; fi; }
if [ "${1:-}" = "--verify" ]; then
  [ -d "$DIR/.git" ] || { echo "third_party/gnuplot missing: run scripts/sync-gnuplot.sh" >&2; exit 1; }
  have="$(git -C "$DIR" rev-parse HEAD)"
  [ "$have" = "$commit" ] || { echo "third_party/gnuplot at $have, gnuplot.lock pins $commit: run scripts/sync-gnuplot.sh" >&2; exit 1; }
  want="$(cat "$HERE"/patches/*.patch | sha)"
  [ -f "$patched_marker" ] && [ "$(cat "$patched_marker")" = "$want" ] || { echo "patches not applied: run scripts/sync-gnuplot.sh" >&2; exit 1; }
  echo "third_party/gnuplot matches gnuplot.lock ($commit) with patches applied"
  exit 0
fi
if [ ! -d "$DIR/.git" ]; then
  mkdir -p "$DIR"
  git -C "$DIR" init -q
  git -C "$DIR" remote add origin "$repo"
fi
git -C "$DIR" fetch -q --depth 1 origin "$commit"
git -C "$DIR" checkout -q -f FETCH_HEAD
git -C "$DIR" clean -fdxq
for p in "$HERE"/patches/*.patch; do
  git -C "$DIR" apply --whitespace=nowarn "$p"
done
cat "$HERE"/patches/*.patch | sha > "$patched_marker"
echo "synced third_party/gnuplot at $commit with $(ls "$HERE"/patches/*.patch | wc -l) patch(es)"
