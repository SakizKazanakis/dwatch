#!/usr/bin/env python3
"""Restyles an upstream Gadgetbridge checkout as Monolith.

Every screen inherited from upstream (onboarding, device search, settings,
charts) gets the Monolith name, the DIKRANON palette and the DKRNN mark.
Upstream is changed in place, so every edit checks that its target still
exists and fails the build loudly when upstream moves something.

Usage: scripts/apply-theme.py <path-to-gadgetbridge-checkout>
"""
import glob
import os
import re
import sys

INK = "#FF0D0F11"
SLAB = "#FF15181B"
SEAM = "#FF2A2F34"
STEEL = "#FF9AA3AB"
BONE = "#FFE8E9E6"
SIGNAL = "#FFCFE3EF"
SIGNAL_DIM = "#FF6F8794"
PLATINUM = "#FFC6CACB"

# Upstream colors.xml entries that carry the Gadgetbridge orange/blue.
COLORS = {
    "primary_light": SEAM,
    "primary_dark": SEAM,
    "primarydark_light": SLAB,
    "primarydark_dark": SLAB,
    "accent": SIGNAL_DIM,
    "primarytext_dark": BONE,
    "secondarytext": STEEL,
}

# Material 3 roles for the one theme Monolith uses.
DARK_THEME = {
    "colorPrimary": PLATINUM,
    "colorOnPrimary": INK,
    "colorPrimaryDark": SLAB,
    "colorPrimaryVariant": STEEL,
    "colorPrimaryContainer": SEAM,
    "colorOnPrimaryContainer": BONE,
    "colorSecondary": STEEL,
    "colorOnSecondary": INK,
    "colorSecondaryContainer": "#FF1B1F22",
    "colorOnSecondaryContainer": BONE,
    "colorTertiary": SIGNAL,
    "colorOnTertiary": INK,
    "colorTertiaryContainer": "#FF22282C",
    "colorOnTertiaryContainer": SIGNAL,
    "colorSurface": INK,
    "colorOnSurface": BONE,
    "colorSurfaceVariant": SLAB,
    "colorOnSurfaceVariant": STEEL,
    "colorSurfaceInverse": BONE,
    "colorOnSurfaceInverse": INK,
    "colorOutline": "#FF3A4046",
    "colorOutlineVariant": SEAM,
    "colorOnBackground": BONE,
    "colorAccent": SIGNAL,
    "colorControlActivated": SIGNAL,
    "android:colorBackground": INK,
    "android:windowBackground": INK,
    "android:statusBarColor": SLAB,
    "android:navigationBarColor": INK,
    "drawer_header_color": SLAB,
    "drawer_title_color": BONE,
    "cardview_background_color": SLAB,
    "alternate_row_background": "#FF121518",
    "deviceIconPrimary": STEEL,
    "deviceIconOnPrimary": INK,
    "deviceIconLight": SIGNAL_DIM,
    "deviceIconDark": SIGNAL,
    "android:fontFamily": "@font/plex_sans",
    "fontFamily": "@font/plex_sans",
}

# The DKRNN two-blade mark, 200 wide, centred in a 288 square.
MARK_PATHS = (
    "M21 22 L79 76 L79 179 L90 189 L90 266 L82 266 L21 200 Z",
    "M179 22 L121 76 L121 179 L110 189 L110 266 L118 266 L179 200 Z",
)

ABOUT = {
    "values": "Monolith by DIKRANON: the companion app for your watch.\\n\\n"
              "Built on the free Gadgetbridge project and distributed under the GNU AGPLv3.",
    "values-ru": "Monolith от DIKRANON: приложение для ваших часов.\\n\\n"
                 "Основано на свободном проекте Gadgetbridge и распространяется по лицензии GNU AGPLv3.",
}


def fail(msg):
    sys.exit(f"apply-theme: {msg}")


def warn(msg):
    print(f"apply-theme: warning: {msg}")


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def write(path, text):
    with open(path, "w", encoding="utf-8") as f:
        f.write(text)


def replace_once(path, old, new, required=True):
    text = read(path)
    if old not in text:
        (fail if required else warn)(f"{old!r} not found in {path}")
        return
    write(path, text.replace(old, new, 1))


def mark_vector(width, height, fill, alpha="1"):
    paths = "\n".join(
        f'        <path android:fillColor="{fill}" android:pathData="{d}" />' for d in MARK_PATHS)
    return f"""<?xml version="1.0" encoding="utf-8"?>
<!-- Monolith: DKRNN mark (replaces the upstream Gadgetbridge logo). -->
<vector xmlns:android="http://schemas.android.com/apk/res/android"
    android:width="{width}"
    android:height="{height}"
    android:alpha="{alpha}"
    android:viewportWidth="288"
    android:viewportHeight="288">
    <group android:translateX="44">
{paths}
    </group>
</vector>
"""


def rename_strings(gb):
    # A \n escape right before the name still counts as a word break.
    word = re.compile(r"(?:(?<=\\n)|(?<![A-Za-z0-9_./:@-]))Gadgetbridge(?![A-Za-z0-9_])")
    files = glob.glob(os.path.join(gb, "app/src/*/res/values*/strings.xml"))
    if not files:
        fail("no strings.xml found")
    total = 0
    for path in files:
        text = read(path)
        # Only element text: attributes such as name="..." stay as they are.
        new, n = re.subn(r">([^<]*)<", lambda m: ">" + word.sub("Monolith", m.group(1)) + "<", text)
        if n and new != text:
            total += len(word.findall(text))
            write(path, new)
    print(f"apply-theme: renamed {total} mentions in {len(files)} string files")

    # The license line Monolith keeps (AGPLv3 notice), in the about screen.
    for folder, about in ABOUT.items():
        path = os.path.join(gb, "app/src/main/res", folder, "strings.xml")
        text = read(path)
        new, n = re.subn(r'(<string name="about_description_generic">)[^<]*(</string>)',
                         lambda m: m.group(1) + about + m.group(2), text)
        if n:
            write(path, new)
        elif folder == "values":
            fail("about_description_generic not found")


def recolor(gb):
    path = os.path.join(gb, "app/src/main/res/values/colors.xml")
    text = read(path)
    for name, value in COLORS.items():
        text, n = re.subn(rf'(<color name="{name}"[^>]*>)[^<]*(</color>)',
                          lambda m: m.group(1) + value + m.group(2), text)
        if n != 1:
            fail(f"color {name} not found in {path}")
    write(path, text)


def set_style_items(text, style, items):
    m = re.search(rf'<style name="{re.escape(style)}"[^>]*>(.*?)</style>', text, re.S)
    if not m:
        fail(f"style {style} not found")
    body = m.group(1)
    for name, value in items.items():
        body, n = re.subn(rf'(<item name="{re.escape(name)}"[^>]*>)[^<]*(</item>)',
                          lambda mm: mm.group(1) + value + mm.group(2), body)
        if not n:
            body += f'    <item name="{name}">{value}</item>\n    '
    return text[:m.start(1)] + body + text[m.end(1):]


def restyle(gb):
    path = os.path.join(gb, "app/src/main/res/values/styles.xml")
    text = read(path)
    text = set_style_items(text, "GadgetbridgeThemeDark", DARK_THEME)
    text = set_style_items(text, "GadgetbridgeThemeBlack", {
        "android:windowBackground": "#FF000000",
        "android:navigationBarColor": "#FF000000",
    })
    write(path, text)

    # Splash and any screen that skips the theme switch also start dark.
    splash = 0
    for other in glob.glob(os.path.join(gb, "app/src/main/res/values*/*.xml")):
        text = read(other)
        new, n = re.subn(r'(<style name="SplashTheme" parent=")[^"]*"',
                         r'\1GadgetbridgeThemeDark.NoActionBar"', text)
        new, m = re.subn(r'(<item name="(?:android:)?windowSplashScreenBackground"[^>]*>)[^<]*(</item>)',
                         lambda mm: mm.group(1) + INK + mm.group(2), new)
        if new != text:
            write(other, new)
        splash += n + m
    if not splash:
        warn("SplashTheme not found")

    manifest = os.path.join(gb, "app/src/main/AndroidManifest.xml")
    replace_once(manifest, 'android:theme="@style/GadgetbridgeTheme"',
                 'android:theme="@style/GadgetbridgeThemeDark"', required=False)

    # Monolith is always dark: ignore the light, system and dynamic choices.
    app = os.path.join(gb, "app/src/main/java/nodomain/freeyourgadget/gadgetbridge/GBApplication.java")
    replace_once(app, "public static boolean isDarkThemeEnabled() {",
                 "public static boolean isDarkThemeEnabled() {\n        if (true) return true; // Monolith")
    replace_once(app, "public static boolean areDynamicColorsEnabled() {",
                 "public static boolean areDynamicColorsEnabled() {\n        if (true) return false; // Monolith")


def relogo(gb):
    res = os.path.join(gb, "app/src/main/res")

    # The big grey logo (empty device list, about, splash) and the drawer header.
    write(os.path.join(res, "drawable/gadgetbridge_img.xml"), mark_vector("200dp", "200dp", "#FFC6CACB", "0.55"))
    swaps = {
        "@drawable/ic_launcher_foreground": "@drawable/monolith_launcher_foreground",
        "@drawable/ic_launcher\"": "@drawable/monolith_launcher\"",
        "@mipmap/ic_launcher\"": "@drawable/monolith_launcher\"",
    }
    for path in glob.glob(os.path.join(res, "layout*/*.xml")):
        text = read(path)
        new = text
        for old, rep in swaps.items():
            new = new.replace(old, rep)
        if new != text:
            write(path, new)

    # Welcome screen: mark on ink instead of the blue tile, and no theme picker.
    intro = os.path.join(res, "layout/fragment_welcome_intro.xml")
    replace_once(intro, 'android:background="@color/accent"', 'android:background="@color/monolith_ink"',
                 required=False)
    replace_once(intro, 'android:id="@+id/app_theme_dropdown_input"',
                 'android:id="@+id/app_theme_dropdown_input"\n            android:visibility="gone"',
                 required=False)

    # Status bar icons: the mark instead of the Gadgetbridge silhouette.
    names = set()
    for png in glob.glob(os.path.join(res, "drawable-*dpi/ic_notification*.png")):
        names.add(os.path.basename(png)[:-4])
        os.remove(png)
    if "ic_notification" not in names:
        fail("ic_notification.png not found")
    for name in names:
        alpha = "0.5" if "disconnected" in name else "1"
        write(os.path.join(res, f"drawable/{name}.xml"), mark_vector("24dp", "24dp", "#FFFFFFFF", alpha))


def main():
    if len(sys.argv) != 2:
        fail("usage: apply-theme.py <gadgetbridge checkout>")
    gb = sys.argv[1]
    rename_strings(gb)
    recolor(gb)
    restyle(gb)
    relogo(gb)
    print("apply-theme: Monolith look applied")


if __name__ == "__main__":
    main()
