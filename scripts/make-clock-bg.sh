#!/usr/bin/env bash
# Converts a PNG into a SiFli eZIP .bin (4-byte LVGL header + eZIP stream)
# that the watch reads as dyn/resource/clock_bg.bin.
# Usage: scripts/make-clock-bg.sh input.png [output.bin]
set -euo pipefail

in="${1:?usage: $0 input.png [output.bin]}"
out="${2:-clock_bg.bin}"
tools="$(cd "$(dirname "$0")" && pwd)/.tools"
ezip="$tools/ezip_linux"
sdk_rev=a97dac0fe858a59165ec33e45cb5856cad76449e
ezip_sha256=216938bb55b095370d34395d283a283a1683b64d414c25c32058e3792ab66e85

if [ ! -x "$ezip" ]; then
  mkdir -p "$tools"
  curl -sSfL -o "$ezip.tmp" \
    "https://raw.githubusercontent.com/OpenSiFli/SiFli-SDK/$sdk_rev/tools/png2ezip/ezip_linux"
  echo "$ezip_sha256  $ezip.tmp" | sha256sum -c --quiet
  chmod +x "$ezip.tmp"
  mv "$ezip.tmp" "$ezip"
fi

work="$(mktemp -d)"
trap 'rm -rf "$work"' EXIT
cp "$in" "$work/clock_bg.png"
(cd "$work" && "$ezip" -convert clock_bg.png -rgb888 -binfile 2 >/dev/null)
cp "$work/output/clock_bg.bin" "$out"
echo "$out"
