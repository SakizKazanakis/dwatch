#!/usr/bin/env bash
# OFFLINE STATIC ABI HYPOTHESIS. NEVER UPLOAD THE RESULT TO A WATCH.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
outfile="/tmp/monolith_registration_hypothesis.so"
if [ "$#" -gt 0 ]; then outfile="$1"; fi
clang --target=armv7a-none-eabihf -march=armv7-a -mthumb \
  -mfloat-abi=hard -mfpu=vfpv3-d16 -Oz -fPIC -nostdlib -fuse-ld=lld \
  -Wl,-shared -Wl,-Bsymbolic -Wl,--hash-style=sysv \
  -o "$outfile" "$here/monolith_registration_hypothesis.c"
readelf -h "$outfile" | grep -q 'ELF32'
readelf -h "$outfile" | grep -q 'ARM'
readelf -h "$outfile" | grep -q 'hard-float ABI'
readelf -Ws "$outfile" | grep -q 'module_init'
readelf -Ws "$outfile" | grep -q 'module_cleanup'
readelf -Ws "$outfile" | grep -q 'app_tileview_register_watchface'
readelf -rW "$outfile" | grep -q 'R_ARM_GLOB_DAT'
if readelf -rW "$outfile" | grep -Eq 'R_ARM_JUMP_SLOT|R_ARM_COPY'; then
  echo >&2 'FAIL: unexpected PLT or COPY relocation (not in OEM specimen)'
  exit 1
fi
echo 'PASS: offline ARM ELF32 EABI5 hard-float prototype, external imports via GLOB_DAT'
echo 'WARNING: prototypes, module loader acceptance, UI behavior and rollback NOT VERIFIED.'
echo 'NEVER install this ELF on the physical watch.'
