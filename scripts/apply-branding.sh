#!/usr/bin/env bash
# Turns an upstream Gadgetbridge checkout into the Dwatch app.
# Usage: scripts/apply-branding.sh <path-to-gadgetbridge-checkout>
set -euo pipefail

GB_DIR="${1:?path to Gadgetbridge checkout}"
DWATCH_DIR="$(cd "$(dirname "$0")/.." && pwd)"
APP_ID="${DWATCH_APP_ID:-com.dwatch.app}"
APP_NAME="${DWATCH_APP_NAME:-Dwatch}"

gradle_file="$GB_DIR/app/build.gradle"
strings_file="$GB_DIR/app/src/mainline/res/values/strings.xml"

# Own applicationId, so Dwatch installs next to stock Gadgetbridge.
grep -q 'applicationId "nodomain.freeyourgadget.gadgetbridge"' "$gradle_file" \
  || { echo "applicationId line not found in $gradle_file" >&2; exit 1; }
sed -i "s/applicationId \"nodomain.freeyourgadget.gadgetbridge\"/applicationId \"$APP_ID\"/" "$gradle_file"

# Own launcher name for the mainline flavor.
grep -q 'name="app_name"' "$strings_file" \
  || { echo "app_name not found in $strings_file" >&2; exit 1; }
sed -i "s|<string translatable=\"false\" name=\"app_name\">[^<]*</string>|<string translatable=\"false\" name=\"app_name\">$APP_NAME</string>|" "$strings_file"

# Our own changes, applied in name order.
shopt -s nullglob
for patch in "$DWATCH_DIR"/patches/*.patch; do
  echo "Applying $(basename "$patch")"
  git -C "$GB_DIR" apply --whitespace=nowarn "$patch"
done

echo "Branded as $APP_NAME ($APP_ID)"
