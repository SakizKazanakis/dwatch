#!/usr/bin/env bash
# Reproducible HOST-only LVGL v8.3.11 integration test, never flashes HK8.
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"
root="$(cd "$here/../../.." && pwd)"
lvgl_ref="74d0a816a440eea53e030c4f1af842a94f7ce3d3"
base="${MONOLITH_LVGL_TEST_DIR:-${TMPDIR:-/tmp}/monolith-lvgl-v8}"
lvgl="$base/lvgl"
mkdir -p "$base/obj"
if [ ! -d "$lvgl/.git" ]; then
 git clone --depth 1 --branch v8.3.11 https://github.com/lvgl/lvgl.git "$lvgl"
fi
actual="$(git -C "$lvgl" rev-parse HEAD)"
if [ "$actual" != "$lvgl_ref" ]; then
 echo >&2 "LVGL version mismatch: expected $lvgl_ref got $actual"; exit 1
fi
sed 's/^#if 0 \/\*Set it to "1" to enable content\*\//#if 1 \/\*Enable host renderer test\*\//' "$lvgl/lv_conf_template.h" | \
 sed 's/^#define LV_COLOR_DEPTH 16$/#define LV_COLOR_DEPTH 32/' | \
 sed 's/^    #define LV_MEM_SIZE (48U \* 1024U)/    #define LV_MEM_SIZE (1024U * 1024U)/' > "$base/lv_conf.h"
grep -q "^#define LV_COLOR_DEPTH 32" "$base/lv_conf.h"
if [ ! -f "$base/obj/all-done" ]; then
 find "$lvgl/src" -name "*.c" | sort | while IFS= read -r file; do
   key="${file//\//_}"
   cc -O1 -std=c11 -DLV_CONF_INCLUDE_SIMPLE=1 \
     -I "$base" -I "$lvgl" -c "$file" -o "$base/obj/${key}.o"
 done
 touch "$base/obj/all-done"
fi
cc -O2 -std=c11 -DLV_CONF_INCLUDE_SIMPLE=1 \
  -I "$base" -I "$lvgl" -I "$root/watchface/native_renderer/include" \
  "$root/watchface/native_renderer/src/monolith_ezip_native.c" \
  "$root/watchface/native_renderer/src/monolith_lvgl_v8_adapter.c" \
  "$root/watchface/native_renderer/tests/test_lvgl_v8_headless.c" \
  "$base"/obj/*.o -lm -o "$base/monolith_lvgl_headless"
fixture="$root/overlay/app/src/main/assets/monolith/clock_bg.bin"
"$base/monolith_lvgl_headless" "$fixture" "$base/monolith_lvgl_frame.ppm"
python3 "$root/tests/check_lvgl_headless_frame.py" "$base/monolith_lvgl_frame.ppm" \
 "$root/watchface/ezip/fixtures/clock_bg_decoded_from_ezip.png"
echo "PASS: genuine LVGL v8.3.11 created a verified Monolith watchface frame"
