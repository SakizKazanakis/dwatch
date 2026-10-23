#!/usr/bin/env bash
# Offline-only verification of SiFli SDK public ABI field layout on ARM32.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
obj="${TMPDIR:-/tmp}/monolith_sifli_sdk_resource_abi_$$.o"
trap 'rm -f "$obj"' EXIT
clang --target=armv7a-none-eabihf -march=armv7-a -mthumb \
  -mfloat-abi=hard -mfpu=vfpv3-d16 -std=c11 -Wall -Wextra -Werror \
  -ffreestanding -c "$here/test_sifli_sdk_resource_abi.c" \
  -I "$here" -o "$obj"
readelf -h "$obj" | grep -q 'ARM'
readelf -h "$obj" | grep -q 'hard-float ABI'
echo 'PASS: Public SiFli resource manager has 44-byte ARM32 ABI and expected offsets.'
echo 'NOTICE: This does not prove source-version parity with HK8 code 3407 firmware 2.09.'
