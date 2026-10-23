#!/usr/bin/env bash
# Offline native ABI compatibility probe. Never flash the generated .so to a watch.
set -euo pipefail
src_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
out="${1:-${src_dir}/monolith_abi_probe.so}"
: "${CC:=clang}"
"${CC}" --target=armv7a-none-eabihf -march=armv7-a -mfloat-abi=hard \
  -mfpu=vfpv3-d16 -Oz -fPIC -nostdlib -fuse-ld=lld \
  -Wl,-shared -Wl,--hash-style=sysv -o "${out}" \
  "${src_dir}/monolith_abi_probe.c"
readelf -h "${out}" | grep -E 'Class:|Data:|Type:|Machine:|Flags:'
readelf -s "${out}" | grep -E 'module_init|module_cleanup|monolith_watchface_probe_version'
echo "ABI probe only; execution compatibility and loader registration NOT verified."
