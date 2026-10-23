#!/usr/bin/env bash
# Creates test-only native binary. DO NOT INSTALL OR UPLOAD TO HK8.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
out="/tmp/monolith_event0_offline.so"
if [ "$#" -gt 0 ]; then out="$1"; fi

cc -std=c11 -Wall -Wextra -Werror -I "$here" \
  "$here/monolith_event0.c" "$here/test_event0.c" \
  -o /tmp/monolith_event0_host_test
cc -std=c11 -Wall -Wextra -Werror -I "$here" \
  "$here/monolith_design_resource.c" "$here/test_design_resource.c" \
  -o /tmp/monolith_design_host_test
/tmp/monolith_design_host_test
/tmp/monolith_event0_host_test

clang --target=armv7a-none-eabihf -march=armv7-a -mthumb \
  -mfloat-abi=hard -mfpu=vfpv3-d16 -Oz -fPIC -nostdlib -fuse-ld=lld \
  -Wl,-shared -Wl,-Bsymbolic -Wl,--hash-style=sysv \
  -o "$out" "$here/monolith_event0.c" "$here/monolith_design_resource.c" "$here/monolith_event0_module.c"

readelf -h "$out" | grep -q 'ELF32'
readelf -h "$out" | grep -q 'hard-float ABI'
readelf -Ws "$out" | grep -q 'module_init'
readelf -Ws "$out" | grep -q 'monolith_create_view'
readelf -Ws "$out" | grep -q 'lv_cont_create'
readelf -rW "$out" | grep -q 'R_ARM_GLOB_DAT'
if readelf -rW "$out" | grep -Eq 'R_ARM_JUMP_SLOT|R_ARM_COPY'; then
  echo >&2 'Unexpected PLT/COPY relocation; this differs from OEM transfer.'
  exit 1
fi
python3 "${here}/../../scripts/check_hk8_arm_module.py" "$out"
echo 'PASS: offline ARM32 event0 + GOT imports (NOT run on HK8)'
