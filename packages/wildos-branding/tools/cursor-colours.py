#!/usr/bin/env python3
"""Builds the coloured pointer themes Settings > Accessibility offers.

Usage: cursor-colours.py SOURCE_THEME_DIR OUTPUT_ICONS_DIR

Breeze Light (white with a dark edge) is copied once per colour with its
white fill turned into that colour, in both forms KWin and apps read: the
SVG cursors (cursors_scalable, which KWin draws at any size) and the
X cursor images (cursors, for X11 apps under Xwayland). The dark edge,
the shadow and anything already coloured, such as the busy spinner, are
kept, so every shape stays as easy to see as Breeze's own.

Generated at build time from the Breeze Light the package is built
against, so the pointers follow Breeze's shapes as it is updated.
"""

import os
import re
import shutil
import struct
import sys

THEMES = {
    "WildOS-Yellow": ("Wild OS Yellow", (0xF2, 0xD3, 0x4A)),
    "WildOS-SeaGlass": ("Wild OS Sea Glass", (0x7F, 0xD6, 0xC8)),
}

XCURSOR_IMAGE = 0xFFFD0002

# White and the near whites Breeze uses for a fill, in an SVG attribute or
# style declaration.
SVG_WHITE = re.compile(
    r'((?:fill|stop-color)\s*[:=]\s*"?\s*)#(?:fff|ffffff|fcfcfc|fdfdfd|fefefe)\b',
    re.IGNORECASE,
)


def smoothstep(edge0, edge1, x):
    t = min(1.0, max(0.0, (x - edge0) / (edge1 - edge0)))
    return t * t * (3 - 2 * t)


def recolour_pixel(argb, colour):
    a = argb >> 24
    if a == 0:
        return argb
    # Xcursor pixels are premultiplied.
    r = min(255, ((argb >> 16) & 0xFF) * 255 // a)
    g = min(255, ((argb >> 8) & 0xFF) * 255 // a)
    b = min(255, (argb & 0xFF) * 255 // a)
    if max(r, g, b) - min(r, g, b) > 24:
        return argb
    # Light greys move towards the colour, the dark edge stays; the
    # anti-aliased pixels in between blend, so edges stay smooth.
    t = smoothstep(0.35, 0.95, (r + g + b) / 765.0)
    nr, ng, nb = (round(c + (k - c) * t) for c, k in zip((r, g, b), colour))
    return (a << 24) | ((nr * a // 255) << 16) | ((ng * a // 255) << 8) | (nb * a // 255)


def recolour_xcursor(path, colour):
    with open(path, "rb") as f:
        data = bytearray(f.read())
    if data[:4] != b"Xcur":
        return
    header, _version, ntoc = struct.unpack_from("<III", data, 4)
    for i in range(ntoc):
        chunk_type, _subtype, position = struct.unpack_from("<III", data, header + i * 12)
        if chunk_type != XCURSOR_IMAGE:
            continue
        chunk_header, _t, _s, _v, width, height = struct.unpack_from("<IIIIII", data, position)
        start = position + chunk_header
        for p in range(width * height):
            offset = start + p * 4
            (pixel,) = struct.unpack_from("<I", data, offset)
            struct.pack_into("<I", data, offset, recolour_pixel(pixel, colour))
    with open(path, "wb") as f:
        f.write(data)


def recolour_svg(path, colour):
    with open(path, encoding="utf-8") as f:
        text = f.read()
    hex_colour = "#%02x%02x%02x" % colour
    changed = SVG_WHITE.sub(lambda m: m.group(1) + hex_colour, text)
    if changed != text:
        with open(path, "w", encoding="utf-8") as f:
            f.write(changed)


def build(source, output, name, title, colour):
    target = os.path.join(output, name)
    if os.path.exists(target):
        shutil.rmtree(target)
    os.makedirs(target)
    for sub in ("cursors", "cursors_scalable"):
        src = os.path.join(source, sub)
        if os.path.isdir(src):
            # Aliases are symbolic links in Breeze and stay links here.
            shutil.copytree(src, os.path.join(target, sub), symlinks=True)
    for root, _dirs, files in os.walk(target):
        for file in files:
            path = os.path.join(root, file)
            if os.path.islink(path):
                continue
            if os.path.basename(root) == "cursors":
                recolour_xcursor(path, colour)
            elif file.endswith(".svg"):
                recolour_svg(path, colour)
    with open(os.path.join(target, "index.theme"), "w", encoding="utf-8") as f:
        f.write(
            "[Icon Theme]\n"
            f"Name={title}\n"
            "Comment=Breeze Light pointers in a colour that stands out, from Settings > Accessibility\n"
            "Inherits=Breeze_Light\n"
        )
    # Some toolkits only look for cursor.theme.
    shutil.copyfile(os.path.join(target, "index.theme"), os.path.join(target, "cursor.theme"))


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    source, output = sys.argv[1], sys.argv[2]
    if not os.path.isdir(os.path.join(source, "cursors")):
        sys.exit(f"{source} has no cursors")
    for name, (title, colour) in THEMES.items():
        build(source, output, name, title, colour)


if __name__ == "__main__":
    main()
