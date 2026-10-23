#!/usr/bin/env bash
# Regenerate patches/*.patch from the working tree of third_party/gnuplot
# (pinned commit plus your edits). Each patch is one logical change; the
# mapping below is the single place that decides which files go where.
set -eu
HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DIR="$HERE/third_party/gnuplot"
[ -d "$DIR/.git" ] || { echo "run scripts/sync-gnuplot.sh first" >&2; exit 1; }
emit() { # name path...
  local name="$1"; shift
  git -C "$DIR" add -N -- "$@" 2>/dev/null || true
  git -C "$DIR" diff -- "$@" > "$HERE/patches/$name.patch"
  [ -s "$HERE/patches/$name.patch" ] || { echo "empty patch $name" >&2; exit 1; }
}
emit 0001-post-deflate-brace term/post.trm
emit 0002-term-list src/term.h
emit 0003-embed-hook src/plot.c
emit 0004-version-id src/version.c
emit 0005-lmpng-terminal term/lmpng.trm
emit 0006-no-pager-when-scripted src/help.c
emit 0007-dry-run src/command.c src/fit.c src/plot.h src/set.c src/term.c
emit 0008-image-reader src/breaders.c
git -C "$DIR" reset -q
ls -la "$HERE"/patches
