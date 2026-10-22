#!/usr/bin/env bash
# Turns an upstream Gadgetbridge checkout into the Monolith app.
# Usage: scripts/apply-branding.sh <path-to-gadgetbridge-checkout>
set -euo pipefail

GB_DIR="${1:?path to Gadgetbridge checkout}"
REPO_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_ID="${MONOLITH_APP_ID:-com.dikranon.monolith}"
APP_NAME="${MONOLITH_APP_NAME:-Monolith}"

gradle_file="$GB_DIR/app/build.gradle"
strings_file="$GB_DIR/app/src/mainline/res/values/strings.xml"

# Own applicationId, so Monolith installs next to stock Gadgetbridge.
grep -q 'applicationId "nodomain.freeyourgadget.gadgetbridge"' "$gradle_file" \
  || { echo "applicationId line not found in $gradle_file" >&2; exit 1; }
sed -i "s/applicationId \"nodomain.freeyourgadget.gadgetbridge\"/applicationId \"$APP_ID\"/" "$gradle_file"

# Own launcher name for the mainline flavor.
grep -q 'name="app_name"' "$strings_file" \
  || { echo "app_name not found in $strings_file" >&2; exit 1; }
sed -i "s|<string translatable=\"false\" name=\"app_name\">[^<]*</string>|<string translatable=\"false\" name=\"app_name\">$APP_NAME</string>|" "$strings_file"

# Fixed content-provider authorities clash with an installed stock Gadgetbridge
# ("conflicts with another package"), so move the Pebble one under our id.
grep -q 'name="pebble_content_provider"' "$strings_file" \
  || { echo "pebble_content_provider not found in $strings_file" >&2; exit 1; }
sed -i "s|<string translatable=\"false\" name=\"pebble_content_provider\">[^<]*</string>|<string translatable=\"false\" name=\"pebble_content_provider\">$APP_ID.pebble_provider</string>|" "$strings_file"
if grep -n 'android:authorities="[a-z]' "$GB_DIR/app/src/main/AndroidManifest.xml"; then
  echo "warning: hard-coded provider authorities above may clash with stock Gadgetbridge" >&2
fi

# Our own screens and resources.
python3 "$REPO_DIR/scripts/apply-overlay.py" "$GB_DIR"

# Build tag on the dashboard header, so a screenshot tells which build is installed.
build_tag="v0.2 · $(git -C "$REPO_DIR" rev-parse --short HEAD 2>/dev/null || echo dev)"
cat > "$GB_DIR/app/src/main/res/values/monolith_build.xml" <<EOF
<?xml version="1.0" encoding="utf-8"?>
<resources>
    <string name="monolith_build" translatable="false">$build_tag</string>
</resources>
EOF

# Monolith name, palette and mark on every upstream screen.
python3 "$REPO_DIR/scripts/apply-theme.py" "$GB_DIR"

# Our own changes to upstream code, applied in name order.
shopt -s nullglob
for patch in "$REPO_DIR"/patches/*.patch; do
  echo "Applying $(basename "$patch")"
  git -C "$GB_DIR" apply --whitespace=nowarn "$patch"
done

echo "Branded as $APP_NAME ($APP_ID)"
