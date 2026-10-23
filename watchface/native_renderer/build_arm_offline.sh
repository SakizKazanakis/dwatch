#!/usr/bin/env bash
# ARM ELF ABI test only; never upload to HK8.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
: "${CLANG:=clang-18}"
: "${READELF:=readelf}"
out="${1:-/tmp/monolith_native_renderer_NOT_INSTALLABLE.so}"
builtins="$("$CLANG" -print-resource-dir)/include"
"$CLANG" --target=armv7a-none-eabihf -march=armv7-a -mthumb \
  -mfpu=vfpv3-d16 -mfloat-abi=hard -std=c11 -Oz -fPIC \
  -ffreestanding -nostdlib -nostdinc -fuse-ld=lld \
  -isystem "$builtins" -I"$here/arm_freestanding" -I"$here/include" \
  -Wl,-shared -Wl,--hash-style=sysv \
  "$here/src/monolith_ezip_native.c" "$here/src/monolith_framebuffer.c" \
  -o "$out"
header="$("$READELF" -h "$out")"
printf "%s\n" "$header" | grep -q "Class:.*ELF32"
printf "%s\n" "$header" | grep -q "Type:.*DYN"
printf "%s\n" "$header" | grep -q "Machine:.*ARM"
printf "%s\n" "$header" | grep -q "Flags:.*Version5 EABI"
printf "%s\n" "$header" | grep -q "Flags:.*hard-float ABI"
"$READELF" -Ws "$out" | grep -q "monolith_resource_decode"
"$READELF" -Ws "$out" | grep -q "monolith_renderer_draw"
echo "PASS: native C renderer built as ARM ELF32 ET_DYN EABI5 hard-float."
echo "WARNING: libc imports are UNRESOLVED, OEM ROM exports/loader unavailable."
echo "NOT INSTALLABLE. NO DEVICE WRITES."
