#!/usr/bin/env python3
"""Generate the Wild OS icon theme from the design's drawings.

The app, folder and document icons are drawn as inline SVG in the design
mockups (Start, Taskbar, StartSearch and Main). They are copied here as source
data because the design folder lives outside the repository. This script
normalises each one to a standalone 48 x 48 SVG that Qt's SVG Tiny renderer
draws faithfully, turns the calendar and PDF lettering into outlines so no
font is needed at run time, and writes one file per icon name Plasma and the
shipped apps ask for. It also writes the design's line glyphs as symbolic
icons for the tray, popups and window controls.

    python3 tools/icon-theme.py files/icons/WildOS

Needs fontTools (python3-fonttools) and Inter (fonts-inter). Set INTER_DIR if
Inter is not in /usr/share/fonts/opentype/inter. The output is committed, so
the package build does not run this.

Aliases are written as real files rather than symlinks: the tree is edited on
Windows checkouts, where git turns symlinks into small text files that would
then ship as broken icons.
"""

import math
import re
import os
import sys
from pathlib import Path

from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.ttLib import TTFont

INTER_DIR = Path(os.environ.get("INTER_DIR", "/usr/share/fonts/opentype/inter"))


def text_path(text, font_file, size, x, baseline, spacing=0.0):
    """Outline `text` centred on x, as the design's text-anchor="middle"."""
    font = TTFont(INTER_DIR / font_file)
    glyphs = font.getGlyphSet()
    cmap = font.getBestCmap()
    scale = size / font["head"].unitsPerEm
    names = [cmap[ord(c)] for c in text]
    advances = [glyphs[n].width * scale for n in names]
    # Letter spacing goes between letters only, so the word is optically
    # centred instead of drifting left by half a space.
    width = sum(advances) + spacing * (len(names) - 1)
    pen_x = x - width / 2
    out = []
    for name, adv in zip(names, advances):
        pen = SVGPathPen(glyphs)
        glyphs[name].draw(TransformPen(pen, (scale, 0, 0, -scale, pen_x, baseline)))
        out.append(pen.getCommands())
        pen_x += adv + spacing
    return " ".join(out)


def rounded(d):
    """Trim fontTools' long decimals so the files stay readable."""
    return re.sub(r"-?\d*\.?\d+", lambda m: fmt(m.group()), d)


def fmt(n):
    v = round(float(n), 2)
    s = f"{v:.2f}".rstrip("0").rstrip(".")
    return "0" if s == "-0" else s


def grad(gid, top, bottom):
    return (f'<defs><linearGradient id="{gid}" x1="0" y1="0" x2="0" y2="1">'
            f'<stop offset="0" stop-color="{top}"/><stop offset="1" stop-color="{bottom}"/>'
            f'</linearGradient></defs>')


def sector(cx, cy, r, a0, a1):
    """Pie slice between two angles in degrees, 0 at twelve o'clock."""
    p0 = (cx + r * math.sin(math.radians(a0)), cy - r * math.cos(math.radians(a0)))
    p1 = (cx + r * math.sin(math.radians(a1)), cy - r * math.cos(math.radians(a1)))
    large = 1 if a1 - a0 > 180 else 0
    return (f"M{cx} {cy}L{p0[0]:.2f} {p0[1]:.2f}"
            f"A{r} {r} 0 {large} 1 {p1[0]:.2f} {p1[1]:.2f}z")


def arc(cx, cy, r, a0, a1):
    p0 = (cx + r * math.sin(math.radians(a0)), cy - r * math.cos(math.radians(a0)))
    p1 = (cx + r * math.sin(math.radians(a1)), cy - r * math.cos(math.radians(a1)))
    large = 1 if a1 - a0 > 180 else 0
    return f"M{p0[0]:.2f} {p0[1]:.2f}A{r} {r} 0 {large} 1 {p1[0]:.2f} {p1[1]:.2f}"


# Shared pieces of the design's document page (Start, StartSearch, Main).
PAGE = ('<path d="M11 4h18l9 9v29a2 2 0 0 1-2 2H11a2 2 0 0 1-2-2V6a2 2 0 0 1 2-2z" fill="#f3f5fa"/>'
        '<path d="M29 4v7a2 2 0 0 0 2 2h7z" fill="#c9d1e0"/>')

FOLDER_BACK = '<path d="M6 12a3 3 0 0 1 3-3h10l4 4h16a3 3 0 0 1 3 3v3H6z" fill="#c97f24"/>'
FOLDER_FRONT = 'M5 18a3 3 0 0 1 3-3h32a3 3 0 0 1 3 3v19a3 3 0 0 1-3 3H8a3 3 0 0 1-3-3z'

HEADPHONES = ('<path d="M9 30v-5a15 15 0 0 1 30 0v5" fill="none" stroke="#cdbfff" stroke-width="3.6" stroke-linecap="round"/>'
              '<rect x="5" y="26" width="11" height="16" rx="4.5" ry="4.5" fill="{fill}"/>'
              '<rect x="32" y="26" width="11" height="16" rx="4.5" ry="4.5" fill="{fill}"/>')

CLAPPER = ('<rect x="6" y="19" width="36" height="23" rx="3.5" ry="3.5" fill="#3a3f54"/>'
           '<g transform="rotate(-9 6 18)"><rect x="6" y="10" width="36" height="7.5" rx="1.5" ry="1.5" fill="#f2f3f7"/>'
           '<path d="M12 10l5 7.5h5L17 10zM22 10l5 7.5h5L27 10zM32 10l5 7.5h5L37 10z" fill="#3a3f54"/></g>'
           '<path d="M20.5 25.5l10 5-10 5z" fill="#ff5f6d"/>')

# Place glyphs from the Files sidebar in Main, on the 24 px UI grid.
PLACE_GLYPHS = {
    "home": '<path d="M4 11l8-7 8 7v9H4z"/>',
    "desktop": '<rect x="3" y="4" width="18" height="13" rx="2" ry="2"/><path d="M8 21h8M12 17v4"/>',
    "documents": '<path d="M6 3h9l4 4v14H6z"/><path d="M14 3v5h5"/>',
    "downloads": '<path d="M12 4v11M7 10l5 5 5-5M5 20h14"/>',
    "pictures": '<rect x="3" y="4" width="18" height="16" rx="2" ry="2"/><path d="M3 16l5-5 5 5 3-3 5 5"/>',
    "music": '<path d="M9 18V5l11-2v13"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/>',
    "videos": '<rect x="3" y="6" width="13" height="12" rx="2" ry="2"/><path d="M16 10.5l5-3v9l-5-3z"/>',
}


def folder(emblem=None):
    """The desktop folder from Main, optionally carrying a place glyph.

    The glyph is pressed into the front in the tab colour, the way the design
    tints every folder detail from the folder's own palette rather than
    adding a new colour."""
    body = ('<path d="M6 12a3 3 0 0 1 3-3h10l4 4h16a3 3 0 0 1 3 3v3H6z" fill="#d48a2a"/>'
            f'<path d="{FOLDER_FRONT}" fill="#f2b84d"/>'
            '<rect x="5" y="15" width="38" height="2.2" rx="1.1" ry="1.1" fill="#ffe2a0" fill-opacity="0.6"/>')
    if emblem:
        s = 0.62
        tx, ty = 24 - 12 * s, 28.5 - 12 * s
        body += (f'<g transform="translate({tx:.2f} {ty:.2f}) scale({s})" fill="none" stroke="#c97f24" '
                 f'stroke-width="{2.5 / s:.2f}" stroke-linecap="round" stroke-linejoin="round">'
                 f'{PLACE_GLYPHS[emblem]}</g>')
    return body


def drawings():
    nov = rounded(text_path("NOV", "Inter-Bold.otf", 7, 24, 15.2, 0.6))
    six = rounded(text_path("6", "Inter-SemiBold.otf", 17, 24, 36.5))
    pdf = rounded(text_path("PDF", "Inter-Bold.otf", 6.5, 24, 34.6))

    d = {}

    # Main desktop and taskbar versions, which carry the design's soft shading.
    d["files"] = (grad("g", "#f8cb64", "#e59c33") + FOLDER_BACK
                  + f'<path d="{FOLDER_FRONT}" fill="url(#g)"/>'
                  '<rect x="5" y="15" width="38" height="2.2" rx="1.1" ry="1.1" fill="#ffe7ad" fill-opacity="0.65"/>')
    d["browser"] = (grad("g", "#4fb0ff", "#2356cf")
                    + '<circle cx="24" cy="24" r="20" fill="url(#g)"/>'
                    '<g fill="none" stroke="#ffffff" stroke-opacity="0.85" stroke-width="2">'
                    '<ellipse cx="24" cy="24" rx="8" ry="19.5"/><path d="M4.5 24h39M7.5 14h33M7.5 34h33"/></g>')
    d["mail"] = (grad("g", "#ffffff", "#dbe3f2")
                 + '<rect x="4" y="10" width="40" height="28" rx="4.5" ry="4.5" fill="url(#g)"/>'
                 '<path d="M6 13l18 13.5L42 13" fill="none" stroke="#4d7fe0" stroke-width="2.6" stroke-linejoin="round"/>'
                 '<path d="M6 36l13-11M42 36L29 25" fill="none" stroke="#b9c6de" stroke-width="1.6"/>')
    d["music"] = grad("g", "#a98bff", "#6447dc") + HEADPHONES.format(fill="url(#g)")
    d["photos"] = (grad("g", "#8fd0ff", "#3c7fd8")
                   + '<rect x="6" y="9" width="32" height="28" rx="3" ry="3" fill="#c8d2e6" transform="rotate(-9 22 23)"/>'
                   '<g transform="rotate(5 26 26)"><rect x="9" y="11" width="32" height="30" rx="3" ry="3" fill="#ffffff"/>'
                   '<rect x="12" y="14" width="26" height="19" rx="1.5" ry="1.5" fill="url(#g)"/>'
                   '<path d="M12 33l7.5-8.5 5 5 4-4L38 33z" fill="#2c6b4d"/>'
                   '<circle cx="32" cy="19.5" r="2.6" fill="#ffd56b"/></g>')
    d["store"] = (grad("g", "#5ad897", "#1f8f55")
                  + '<path d="M9 16h30l-2.4 25.5a2.5 2.5 0 0 1-2.5 2.3H13.9a2.5 2.5 0 0 1-2.5-2.3z" fill="url(#g)"/>'
                  '<path d="M17.5 21v-6a6.5 6.5 0 0 1 13 0v6" fill="none" stroke="#d6f7e5" stroke-width="2.8" stroke-linecap="round"/>'
                  '<rect x="9" y="16" width="30" height="2.4" fill="#b6f0d0" fill-opacity="0.5"/>')

    # Start menu versions.
    d["calendar"] = ('<rect x="6" y="6" width="36" height="37" rx="6" ry="6" fill="#f6f7fb"/>'
                     '<path d="M6 12a6 6 0 0 1 6-6h24a6 6 0 0 1 6 6v5H6z" fill="#ec5546"/>'
                     f'<path d="{nov}" fill="#ffffff"/><path d="{six}" fill="#1d2233"/>')
    d["notes"] = ('<rect x="9" y="5" width="30" height="38" rx="4" ry="4" fill="#ffe58c"/>'
                  '<path d="M9 9a4 4 0 0 1 4-4h22a4 4 0 0 1 4 4v4H9z" fill="#f0b02f"/>'
                  '<path d="M14 20h20M14 26h20M14 32h14" fill="none" stroke="#d6ac45" stroke-width="1.8" stroke-linecap="round"/>')
    # The cog is drawn on the 24 px grid in the design; doubling it keeps the
    # same 2 to 22 extent as the 48 px icons.
    d["settings"] = ('<g transform="scale(2)"><path fill="#c3cad8" d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"/>'
                     '<circle cx="12" cy="12" r="3.2" fill="#2c3244"/></g>')
    d["terminal"] = ('<rect x="5" y="8" width="38" height="32" rx="6" ry="6" fill="#1a1e2b" stroke="#4a5266" stroke-width="1.5"/>'
                     '<path d="M13 19l6 5-6 5" fill="none" stroke="#7dffb0" stroke-width="2.6" stroke-linecap="round" stroke-linejoin="round"/>'
                     '<path d="M23 30h11" fill="none" stroke="#e8ecf6" stroke-width="2.6" stroke-linecap="round"/>')
    keys = "".join(f'<circle cx="{x}" cy="{y}" r="2.4"/>' for y in (24, 31, 38) for x in (17.5, 24))
    d["calculator"] = ('<rect x="10" y="4" width="28" height="40" rx="6" ry="6" fill="#2b303f"/>'
                       '<rect x="14" y="8.5" width="20" height="9" rx="2" ry="2" fill="#9fe0c4"/>'
                       f'<g fill="#6a7389">{keys}</g>'
                       '<g fill="#ff9a3c"><circle cx="30.5" cy="24" r="2.4"/><circle cx="30.5" cy="31" r="2.4"/>'
                       '<circle cx="30.5" cy="38" r="2.4"/></g>')
    d["videos"] = CLAPPER

    # Documents, from Start's recent files.
    d["document"] = PAGE + ('<path d="M14 20h20M14 25h20M14 30h20M14 35h12" fill="none" stroke="#3d72d6" '
                            'stroke-width="2" stroke-linecap="round"/>')
    d["text"] = PAGE + ('<path d="M14 20h12M14 25h20M14 30h20M14 35h16" fill="none" stroke="#7b8498" '
                        'stroke-width="1.8" stroke-linecap="round"/>')
    d["pdf"] = PAGE + ('<rect x="13" y="28" width="22" height="9" rx="2" ry="2" fill="#d9483b"/>'
                       f'<path d="{pdf}" fill="#ffffff"/>'
                       '<path d="M14 19h20M14 23.5h14" fill="none" stroke="#9aa5bb" stroke-width="1.8" stroke-linecap="round"/>')
    d["spreadsheet"] = PAGE + ('<rect x="14" y="20" width="20" height="14" rx="1.5" ry="1.5" fill="none" stroke="#2e9a5c" stroke-width="1.8"/>'
                               '<path d="M14 25h20M14 29.5h20M21 20v14" fill="none" stroke="#2e9a5c" stroke-width="1.8"/>')

    d["folder"] = folder()
    for place in PLACE_GLYPHS:
        d[f"folder-{place}"] = folder(place)

    # New drawings for apps and types the design does not show, built from
    # the same parts: the paper page, the Photos print, the Videos body and
    # the metal, ink and accent tones already in the palette.

    # Document viewer: the word-processor page under a reading glass.
    d["viewer"] = (PAGE + '<path d="M14 20h20M14 25h20M14 30h12" fill="none" stroke="#3d72d6" stroke-width="2" stroke-linecap="round"/>'
                   '<path d="M37.5 37.5l5.5 5.5" fill="none" stroke="#3a3f54" stroke-width="4.6" stroke-linecap="round"/>'
                   '<circle cx="31" cy="31" r="8.2" fill="#cfe0ff" fill-opacity="0.75" stroke="#3a3f54" stroke-width="3.2"/>'
                   '<path d="M27 28.5a5 5 0 0 1 3-2.6" fill="none" stroke="#ffffff" stroke-width="1.8" stroke-linecap="round"/>')

    # Archive: a kraft box with a zip down the lid, so it does not read as a
    # second folder at small sizes.
    teeth = "".join(f'<rect x="{x}" y="{y}" width="2.4" height="1.6" rx="0.5" ry="0.5"/>'
                    for i, y in enumerate((20.5, 23.5, 26.5, 29.5))
                    for x in ((22.2,) if i % 2 == 0 else (23.4,)))
    d["archive"] = ('<path d="M7 18h34v21a3 3 0 0 1-3 3H10a3 3 0 0 1-3-3z" fill="#d9a05a"/>'
                    '<rect x="5" y="9" width="38" height="10" rx="2.5" ry="2.5" fill="#b9793a"/>'
                    '<rect x="7" y="19" width="34" height="2.2" fill="#8f5a26" fill-opacity="0.35"/>'
                    '<rect x="21" y="9" width="6" height="25" fill="#f3e2c2"/>'
                    f'<g fill="#8f5a26">{teeth}</g>'
                    '<rect x="20.5" y="31" width="7" height="8" rx="2" ry="2" fill="#5b6378"/>'
                    '<rect x="22.6" y="33.2" width="2.8" height="3.6" rx="1.2" ry="1.2" fill="#d9a05a"/>')
    d["archive-file"] = (PAGE
                         + '<rect x="21" y="4" width="6" height="22" fill="#d9dfe9"/>'
                         + "".join(f'<rect x="{x}" y="{y}" width="2.6" height="1.6" rx="0.5" ry="0.5" fill="#7b8498"/>'
                                   for i, y in enumerate((6, 9, 12, 15, 18, 21))
                                   for x in ((21.4,) if i % 2 == 0 else (24.0,)))
                         + '<rect x="20" y="25" width="8" height="10" rx="2" ry="2" fill="#5b6378"/>'
                         '<rect x="22.6" y="28" width="2.8" height="4.4" rx="1.2" ry="1.2" fill="#f3f5fa"/>')

    # Screenshot: a camera in the Videos body colour with a metal lens ring.
    d["screenshot"] = ('<path d="M16 13l2.2-4.2a2 2 0 0 1 1.8-1h8a2 2 0 0 1 1.8 1L32 13z" fill="#2b303f"/>'
                       '<rect x="4" y="12" width="40" height="29" rx="5" ry="5" fill="#3a3f54"/>'
                       '<rect x="4" y="12" width="40" height="2.2" rx="1.1" ry="1.1" fill="#ffffff" fill-opacity="0.12"/>'
                       '<circle cx="24" cy="27" r="10.5" fill="#c3cad8"/>'
                       '<circle cx="24" cy="27" r="7.5" fill="#1a1e2b"/>'
                       '<circle cx="24" cy="27" r="4.8" fill="#5aa6f0"/>'
                       '<circle cx="21.8" cy="24.8" r="1.7" fill="#ffffff" fill-opacity="0.85"/>'
                       '<rect x="34" y="15.5" width="6" height="3.2" rx="1.6" ry="1.6" fill="#ff5f6d"/>')

    # Partition manager: a metal drive whose platter is split like a pie.
    d["partitions"] = ('<rect x="6" y="5" width="36" height="38" rx="5" ry="5" fill="#c3cad8"/>'
                       '<rect x="6" y="5" width="36" height="2.2" rx="1.1" ry="1.1" fill="#ffffff" fill-opacity="0.45"/>'
                       '<circle cx="24" cy="21" r="13" fill="#2c3244"/>'
                       f'<path d="{sector(24, 21, 11.5, 0, 200)}" fill="#3a83ea"/>'
                       f'<path d="{sector(24, 21, 11.5, 200, 290)}" fill="#f2b84d"/>'
                       f'<path d="{sector(24, 21, 11.5, 290, 360)}" fill="#2fae6b"/>'
                       '<circle cx="24" cy="21" r="3" fill="#2c3244"/>'
                       '<rect x="11" y="37" width="15" height="2.4" rx="1.2" ry="1.2" fill="#8f98ab"/>'
                       '<circle cx="35" cy="38.2" r="1.8" fill="#7dffb0"/>')

    # System monitor: a dial on the calculator's body colour.
    d["monitor"] = ('<circle cx="24" cy="25" r="20" fill="#2b303f"/>'
                    f'<path d="{arc(24, 25, 13.5, -120, -45)}" fill="none" stroke="#2fae6b" stroke-width="4.4"/>'
                    f'<path d="{arc(24, 25, 13.5, -40, 40)}" fill="none" stroke="#f2b84d" stroke-width="4.4"/>'
                    f'<path d="{arc(24, 25, 13.5, 45, 120)}" fill="none" stroke="#ec5546" stroke-width="4.4"/>'
                    '<path d="M24 25l7.5-8.5" fill="none" stroke="#f3f5fa" stroke-width="2.8" stroke-linecap="round"/>'
                    '<circle cx="24" cy="25" r="3.6" fill="#c3cad8"/>'
                    '<rect x="17" y="34" width="14" height="4" rx="2" ry="2" fill="#3d4459"/>')

    # Hardware information: a chip with metal pins and the accent "i".
    pins = "".join(
        f'<rect x="{c - 1.5}" y="{y}" width="3" height="5" rx="1" ry="1"/>' for c in (15, 21, 27, 33) for y in (5, 38)
    ) + "".join(
        f'<rect x="{x}" y="{c - 1.5}" width="5" height="3" rx="1" ry="1"/>' for c in (15, 21, 27, 33) for x in (5, 38)
    )
    d["hardware"] = (f'<g fill="#c3cad8">{pins}</g>'
                     '<rect x="9" y="9" width="30" height="30" rx="5" ry="5" fill="#2b303f"/>'
                     '<rect x="14" y="14" width="20" height="20" rx="3" ry="3" fill="#3d4459"/>'
                     '<circle cx="24" cy="19.2" r="2.2" fill="#8fb4ff"/>'
                     '<rect x="22.2" y="22.6" width="3.6" height="8.4" rx="1.8" ry="1.8" fill="#8fb4ff"/>')

    d["image"] = PAGE + ('<rect x="13" y="19" width="22" height="17" rx="1.5" ry="1.5" fill="#5aa6f0"/>'
                         '<path d="M13 36l6.5-7.5 4.5 4.5 3.5-3.5L35 36z" fill="#2c6b4d"/>'
                         '<circle cx="29.5" cy="23.5" r="2.4" fill="#ffd56b"/>')
    d["audio"] = PAGE + ('<g transform="translate(12.5 16) scale(0.48)">'
                         + HEADPHONES.format(fill="#7d5fe6").replace('stroke="#cdbfff"', 'stroke="#a995f5"')
                         + '</g>')
    d["video"] = PAGE + f'<g transform="translate(12.5 16) scale(0.48)">{CLAPPER}</g>'
    return d


# Simplified drawings for 16 px, where the full ones turn to mush. Each keeps
# the silhouette and colours but drops detail below a pixel.
def small_drawings():
    six = rounded(text_path("6", "Inter-Bold.otf", 22, 24, 40))
    s = {}
    s["calendar"] = ('<rect x="5" y="5" width="38" height="39" rx="6" ry="6" fill="#f6f7fb"/>'
                     '<path d="M5 11a6 6 0 0 1 6-6h26a6 6 0 0 1 6 6v6H5z" fill="#ec5546"/>'
                     f'<path d="{six}" fill="#1d2233"/>')
    s["calculator"] = ('<rect x="9" y="3" width="30" height="42" rx="6" ry="6" fill="#2b303f"/>'
                       '<rect x="13" y="7" width="22" height="10" rx="2" ry="2" fill="#9fe0c4"/>'
                       '<g fill="#6a7389"><rect x="13" y="21" width="9" height="9" rx="2" ry="2"/>'
                       '<rect x="13" y="33" width="9" height="8" rx="2" ry="2"/></g>'
                       '<rect x="26" y="21" width="9" height="20" rx="2" ry="2" fill="#ff9a3c"/>')
    s["pdf"] = (PAGE + '<rect x="11" y="24" width="26" height="14" rx="2.5" ry="2.5" fill="#d9483b"/>'
                '<path d="M15 18h18" fill="none" stroke="#9aa5bb" stroke-width="3" stroke-linecap="round"/>')
    s["document"] = PAGE + ('<path d="M15 20h18M15 27h18M15 34h12" fill="none" stroke="#3d72d6" '
                            'stroke-width="3.2" stroke-linecap="round"/>')
    s["text"] = PAGE + ('<path d="M15 20h12M15 27h18M15 34h15" fill="none" stroke="#7b8498" '
                        'stroke-width="3.2" stroke-linecap="round"/>')
    s["spreadsheet"] = PAGE + ('<rect x="13.5" y="19" width="21" height="17" rx="1.5" ry="1.5" fill="none" stroke="#2e9a5c" stroke-width="3"/>'
                               '<path d="M13.5 27.5h21M22 19v17" fill="none" stroke="#2e9a5c" stroke-width="3"/>')
    s["notes"] = ('<rect x="8" y="4" width="32" height="40" rx="4" ry="4" fill="#ffe58c"/>'
                  '<path d="M8 8a4 4 0 0 1 4-4h24a4 4 0 0 1 4 4v6H8z" fill="#f0b02f"/>'
                  '<path d="M14 23h20M14 32h14" fill="none" stroke="#c99a2e" stroke-width="3.2" stroke-linecap="round"/>')
    s["terminal"] = ('<rect x="4" y="7" width="40" height="34" rx="6" ry="6" fill="#1a1e2b" stroke="#4a5266" stroke-width="2"/>'
                     '<path d="M12 17l7 7-7 7" fill="none" stroke="#7dffb0" stroke-width="3.6" stroke-linecap="round" stroke-linejoin="round"/>'
                     '<path d="M23 32h12" fill="none" stroke="#e8ecf6" stroke-width="3.6" stroke-linecap="round"/>')
    s["browser"] = (grad("g", "#4fb0ff", "#2356cf")
                    + '<circle cx="24" cy="24" r="21" fill="url(#g)"/>'
                    '<g fill="none" stroke="#ffffff" stroke-opacity="0.9" stroke-width="3">'
                    '<ellipse cx="24" cy="24" rx="8.5" ry="20"/><path d="M4 24h40"/></g>')
    s["photos"] = ('<rect x="4" y="7" width="40" height="34" rx="4" ry="4" fill="#ffffff"/>'
                   + grad("g", "#8fd0ff", "#3c7fd8")
                   + '<rect x="8" y="11" width="32" height="22" rx="1.5" ry="1.5" fill="url(#g)"/>'
                   '<path d="M8 33l9-10 6 6 5-5 12 9z" fill="#2c6b4d"/>'
                   '<circle cx="33" cy="17" r="3.4" fill="#ffd56b"/>')
    s["viewer"] = (PAGE + '<path d="M15 20h18" fill="none" stroke="#3d72d6" stroke-width="3.2" stroke-linecap="round"/>'
                   '<path d="M37 37l6 6" fill="none" stroke="#3a3f54" stroke-width="6" stroke-linecap="round"/>'
                   '<circle cx="30" cy="30" r="9" fill="#cfe0ff" stroke="#3a3f54" stroke-width="4"/>')
    s["image"] = PAGE + ('<rect x="12" y="18" width="24" height="19" rx="1.5" ry="1.5" fill="#5aa6f0"/>'
                         '<path d="M12 37l8-9 5 5 4-4 7 8z" fill="#2c6b4d"/>')
    s["archive-file"] = (PAGE + '<rect x="20.5" y="4" width="7" height="22" fill="#9aa5bb"/>'
                         '<rect x="19" y="24" width="10" height="12" rx="2.5" ry="2.5" fill="#5b6378"/>')
    s["audio"] = PAGE + ('<g transform="translate(9.5 13) scale(0.6)">'
                         + HEADPHONES.format(fill="#7d5fe6").replace('stroke="#cdbfff"', 'stroke="#a995f5"').replace('stroke-width="3.6"', 'stroke-width="5"')
                         + '</g>')
    s["videos"] = ('<rect x="5" y="19" width="38" height="24" rx="4" ry="4" fill="#3a3f54"/>'
                   '<g transform="rotate(-9 5 17)"><rect x="5" y="8" width="38" height="9" rx="2" ry="2" fill="#f2f3f7"/>'
                   '<path d="M17 8l5 9h7L24 8z" fill="#3a3f54"/></g>'
                   '<path d="M19 25l12 6-12 6z" fill="#ff5f6d"/>')
    s["video"] = PAGE + ('<rect x="12" y="20" width="24" height="17" rx="3" ry="3" fill="#3a3f54"/>'
                         '<path d="M21 24.5l7.5 4-7.5 4z" fill="#ff5f6d"/>')
    s["hardware"] = ('<g fill="#c3cad8"><rect x="12" y="4" width="24" height="40" rx="2" ry="2"/>'
                     '<rect x="4" y="12" width="40" height="24" rx="2" ry="2"/></g>'
                     '<rect x="9" y="9" width="30" height="30" rx="5" ry="5" fill="#2b303f"/>'
                     '<circle cx="24" cy="17.5" r="3" fill="#8fb4ff"/>'
                     '<rect x="21.5" y="22.5" width="5" height="10.5" rx="2.5" ry="2.5" fill="#8fb4ff"/>')
    s["monitor"] = ('<circle cx="24" cy="25" r="21" fill="#2b303f"/>'
                    f'<path d="{arc(24, 25, 13.5, -120, -45)}" fill="none" stroke="#2fae6b" stroke-width="6"/>'
                    f'<path d="{arc(24, 25, 13.5, -40, 40)}" fill="none" stroke="#f2b84d" stroke-width="6"/>'
                    f'<path d="{arc(24, 25, 13.5, 45, 120)}" fill="none" stroke="#ec5546" stroke-width="6"/>'
                    '<path d="M24 25l8-9" fill="none" stroke="#f3f5fa" stroke-width="4" stroke-linecap="round"/>')
    for place in PLACE_GLYPHS:
        # The pressed-in glyph is under a pixel at 16 px; the plain folder
        # reads better than a smudge.
        s[f"folder-{place}"] = folder()
    return s


# Every name the shipped apps and Plasma ask for, grouped by drawing. The
# first group in each context is what Wild OS ships; the rest are the generic
# and alternative names Plasma falls back to, or that a later install of a
# KDE player would use.
APPS = {
    "files": ["org.wildos.files", "org.kde.dolphin", "dolphin", "system-file-manager"],
    "browser": ["web-browser", "internet-web-browser"],
    "mail": ["internet-mail"],
    "calendar": ["office-calendar", "x-office-calendar"],
    "photos": ["gwenview", "org.kde.gwenview", "multimedia-photo-viewer"],
    "music": ["multimedia-audio-player", "elisa", "org.kde.elisa"],
    "notes": ["kwrite", "org.kde.kwrite", "accessories-text-editor"],
    "store": ["org.wildos.store", "plasmadiscover", "org.kde.discover"],
    "settings": ["org.wildos.settings", "preferences-system", "systemsettings", "org.kde.systemsettings"],
    "terminal": ["org.wildos.terminal", "utilities-terminal", "org.kde.konsole", "konsole"],
    "calculator": ["accessories-calculator", "org.kde.kcalc", "kcalc"],
    "videos": ["multimedia-video-player", "dragonplayer", "org.kde.dragonplayer", "haruna", "org.kde.haruna"],
    "viewer": ["okular", "org.kde.okular"],
    "archive": ["ark", "org.kde.ark", "utilities-file-archiver"],
    "screenshot": ["spectacle", "org.kde.spectacle", "accessories-screenshot", "applets-screenshooter"],
    "partitions": ["partitionmanager", "org.kde.partitionmanager"],
    "monitor": ["utilities-system-monitor", "org.kde.plasma-systemmonitor", "ksysguard"],
    "hardware": ["hwinfo", "org.kde.kinfocenter", "kinfocenter"],
}

PLACES = {
    "folder": ["folder", "folder-open", "inode-directory"],
    "folder-home": ["user-home", "folder-home"],
    "folder-desktop": ["user-desktop", "folder-desktop"],
    "folder-documents": ["folder-documents", "folder-text"],
    "folder-downloads": ["folder-download", "folder-downloads"],
    "folder-pictures": ["folder-pictures", "folder-image", "folder-images"],
    "folder-music": ["folder-music", "folder-sound", "folder-audio"],
    "folder-videos": ["folder-videos", "folder-video"],
}

# KIconLoader tries the full mime name through every inherited theme before
# shortening it, so Breeze's specific icons would beat our generic ones.
# Each common specific name is listed for that reason.
MIMETYPES = {
    "pdf": ["application-pdf"],
    "document": ["x-office-document", "application-vnd.oasis.opendocument.text",
                 "application-vnd.openxmlformats-officedocument.wordprocessingml.document",
                 "application-msword", "application-rtf"],
    "spreadsheet": ["x-office-spreadsheet", "application-vnd.oasis.opendocument.spreadsheet",
                    "application-vnd.openxmlformats-officedocument.spreadsheetml.sheet",
                    "application-vnd.ms-excel", "text-csv"],
    "text": ["text-plain", "text-x-generic"],
    "image": ["image-x-generic", "image-png", "image-jpeg", "image-gif", "image-webp",
              "image-bmp", "image-tiff", "image-heif"],
    "audio": ["audio-x-generic", "audio-mpeg", "audio-flac", "audio-x-wav", "audio-x-vorbis+ogg", "audio-mp4"],
    "video": ["video-x-generic", "video-mp4", "video-webm", "video-x-matroska", "video-quicktime"],
    "archive-file": ["package-x-generic", "application-zip", "application-x-tar", "application-x-compressed-tar",
                     "application-x-bzip-compressed-tar", "application-x-xz-compressed-tar",
                     "application-x-zstd-compressed-tar", "application-x-7z-compressed",
                     "application-x-rar", "application-gzip"],
    "calendar": ["text-calendar"],
}

CONTEXTS = (("apps", "Applications", APPS), ("places", "Places", PLACES), ("mimetypes", "MimeTypes", MIMETYPES))


# Symbolic UI glyphs: the design's line icons on a 24 px grid, round caps and
# joins, drawn in currentColor. KIconLoader swaps the "current-color-scheme"
# stylesheet for the active colour scheme, so the same files work on light
# and dark. Faded parts (unlit signal arcs) use stroke opacity so they follow
# the colour too.

FADE = ' stroke-opacity="0.32"'
WIFI = ["M8.6 16a5 5 0 0 1 6.8 0", "M5.5 12.5a9.5 9.5 0 0 1 13 0", "M2.5 9a14 14 0 0 1 19 0"]
SPEAKER = "M2.5 9.5h4l5-4v13l-5-4h-4z"
VOLUME_ARCS = ["M14.5 9a4.5 4.5 0 0 1 0 6", "M17 6.5a8 8 0 0 1 0 11", "M19.6 4.5a12 12 0 0 1 0 15"]
SHIELD = "M12 3l7 3v6c0 4.4-3 7.6-7 9-4-1.4-7-4.6-7-9V6z"
MOON = "M20 14.5A8.5 8.5 0 0 1 9.5 4 8.5 8.5 0 1 0 20 14.5z"
SUN = ('<circle cx="12" cy="12" r="4"/><path d="M12 2v2M12 20v2M4.9 4.9l1.4 1.4M17.7 17.7l1.4 1.4'
       'M2 12h2M20 12h2M4.9 19.1l1.4-1.4M17.7 6.3l1.4-1.4"/>')
PLANE = "M12 3v6M8.5 9h7M12 9l-9 6v2l9-3 9 3v-2zM12 14v5M9 21h6"
BLUETOOTH = "M7 7l10 10-5 5V2l5 5L7 17"
BELL = '<path d="M6 16.5V11a6 6 0 0 1 12 0v5.5l1.5 1.5h-15z"/><path d="M10 20.5a2 2 0 0 0 4 0"/>'
# An Ethernet socket rather than a plug: its outline survives at 16 px.
PLUG = '<rect x="3" y="4.5" width="18" height="15" rx="2.5"/><path d="M8 16v-4.5h2V9h4v2.5h2V16z"/>'
AC = '<path d="M9 3v4.5M15 3v4.5M6.5 7.5h11v3a5.5 5.5 0 0 1-11 0zM12 16v5"/>'
MIC = '<rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/>'
FOLDER_LINE = '<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>'
BIN = '<path d="M4 7h16M10 11v6M14 11v6M6 7l1 13h10l1-13M9 7V4h6v3"/>'
COG = ('<path d="M12.22 2h-.44a2 2 0 0 0-2 2v.18a2 2 0 0 1-1 1.73l-.43.25a2 2 0 0 1-2 0l-.15-.08a2 2 0 0 0-2.73.73'
       'l-.22.38a2 2 0 0 0 .73 2.73l.15.1a2 2 0 0 1 1 1.72v.51a2 2 0 0 1-1 1.74l-.15.09a2 2 0 0 0-.73 2.73l.22.38'
       'a2 2 0 0 0 2.73.73l.15-.08a2 2 0 0 1 2 0l.43.25a2 2 0 0 1 1 1.73V20a2 2 0 0 0 2 2h.44a2 2 0 0 0 2-2v-.18'
       'a2 2 0 0 1 1-1.73l.43-.25a2 2 0 0 1 2 0l.15.08a2 2 0 0 0 2.73-.73l.22-.39a2 2 0 0 0-.73-2.73l-.15-.08'
       'a2 2 0 0 1-1-1.74v-.5a2 2 0 0 1 1-1.74l.15-.09a2 2 0 0 0 .73-2.73l-.22-.38a2 2 0 0 0-2.73-.73l-.15.08'
       'a2 2 0 0 1-2 0l-.43-.25a2 2 0 0 1-1-1.73V4a2 2 0 0 0-2-2z"/><circle cx="12" cy="12" r="3"/>')
SLASH = '<path d="M4 4l16 16"/>'
X_BADGE = '<path d="M17 16l4 4M21 16l-4 4"/>'
LOCK_BADGE = '<rect x="16" y="17.5" width="6" height="4.5" rx="1"/><path d="M17.5 17.5v-1.2a1.5 1.5 0 0 1 3 0v1.2"/>'
ALERT_BADGE = '<path d="M20 13.5v3.5"/><path d="M20 20.5h0" stroke-width="2.8"/>'
FILLED = ' fill="currentColor"'


def wifi(lit, extra=""):
    """lit counts the dot and then the arcs from the inside out."""
    dot = '<path d="M12 19.5h0" stroke-width="3"' + ("" if lit >= 1 else FADE) + "/>"
    arcs = "".join(f'<path d="{d}"' + ("" if lit >= i + 2 else FADE) + "/>" for i, d in enumerate(WIFI))
    return arcs + dot + extra


def bars(lit, extra=""):
    out = ""
    for i, (x, h) in enumerate(((4.5, 3), (9.5, 7), (14.5, 11), (19.5, 15))):
        out += f'<path d="M{x} 20v-{h}" stroke-width="2.6"' + ("" if lit > i else FADE) + "/>"
    return out + extra


def volume(lit, muted=False, body_filled=False, danger=False):
    body = f'<path d="{SPEAKER}"' + (FILLED if body_filled else "") + "/>"
    if muted:
        return body + '<path d="M15.5 9.5l5 5M20.5 9.5l-5 5"/>'
    arcs = ""
    for i, d in enumerate(VOLUME_ARCS):
        attrs = "" if lit > i else FADE
        if danger and i == 2:
            attrs = ' class="ColorScheme-NegativeText"'
        arcs += f'<path d="{d}"{attrs}/>'
    return body + arcs


def battery(level, charging=False):
    """Horizontal battery like the taskbar's, fill proportional to charge."""
    out = '<rect x="2" y="7" width="17.5" height="10" rx="2.6"/><path d="M22 10.5v3" stroke-width="2.2"/>'
    if level is None:
        return out + '<path d="M8 14.5l5-5"/>'
    w = max(1.6, 12.7 * level / 100) if level else 0
    low = level <= 10 and not charging
    if w:
        cls = "ColorScheme-NegativeText" if low else "ColorScheme-Text"
        op = ' fill-opacity="0.45"' if charging else ""
        out += (f'<rect x="4.4" y="9.4" width="{w:.1f}" height="5.2" rx="1" fill="currentColor" '
                f'stroke="none" class="{cls}"{op}/>')
    elif not charging:
        out = out.replace('<rect x="2"', '<rect class="ColorScheme-NegativeText" x="2"', 1)
    if charging:
        out += '<path d="M11.8 7.8l-3.6 4.6h3l-1 3.8 3.6-4.6h-3z" fill="currentColor" stroke-width="1"/>'
    return out


GAUGE = '<path d="M4 17a8.6 8.6 0 1 1 16 0"/><circle cx="12" cy="14.5" r="1.6" fill="currentColor"/>'


def symbolic_glyphs():
    g = {}
    for lit in range(5):
        g[f"wifi-{lit}"] = wifi(lit)
        g[f"wifi-{lit}-locked"] = wifi(lit, LOCK_BADGE)
        g[f"wifi-{lit}-limited"] = wifi(lit, ALERT_BADGE)
        g[f"bars-{lit}"] = bars(lit)
    g["wifi-disconnected"] = wifi(0, X_BADGE)
    g["wifi-off"] = wifi(4, SLASH)
    g["hotspot"] = ('<path d="M12 13h0" stroke-width="3"/><path d="M8.5 9.5a5 5 0 0 0 0 7M15.5 9.5a5 5 0 0 1 0 7'
                    'M5.5 6.5a9.2 9.2 0 0 0 0 13M18.5 6.5a9.2 9.2 0 0 1 0 13"/>')
    g["bars-off"] = bars(0, '<path d="M3 4l4 4M7 4l-4 4"/>')
    g["wired"] = PLUG
    g["wired-faded"] = PLUG.replace("<path", "<path" + FADE).replace("<rect", "<rect" + FADE)
    g["wired-disconnected"] = g["wired-faded"] + X_BADGE
    g["wired-limited"] = PLUG + ALERT_BADGE
    g["wired-locked"] = PLUG + LOCK_BADGE
    g["no-network"] = '<circle cx="12" cy="12" r="9"/><path d="M5.6 5.6l12.8 12.8"/>'
    g["vpn"] = f'<path d="{SHIELD}"/><path d="M9.5 12h5"/>'
    g["plane"] = f'<path d="{PLANE}"/>'
    g["plane-off"] = f'<path d="{PLANE}"{FADE}/>' + SLASH
    g["connect"] = AC
    g["disconnect"] = AC + SLASH
    g["bluetooth"] = f'<path d="{BLUETOOTH}"/>'
    g["bluetooth-active"] = f'<path d="{BLUETOOTH}"/><path d="M3 12h0M21 12h0" stroke-width="3"/>'
    g["bluetooth-off"] = f'<path d="{BLUETOOTH}"{FADE}/>' + SLASH

    g["volume-muted"] = volume(0, muted=True)
    g["volume-low"] = volume(1)
    g["volume-medium"] = volume(2)
    g["volume-high"] = volume(3)
    g["volume-warning"] = volume(3, body_filled=True)
    g["volume-danger"] = volume(3, body_filled=True, danger=True)
    g["mic"] = MIC
    g["mic-muted"] = MIC + SLASH
    g["headphones"] = ('<path d="M4 17v-4a8 8 0 0 1 16 0v4"/><rect x="3" y="14" width="4.5" height="7" rx="2"/>'
                       '<rect x="16.5" y="14" width="4.5" height="7" rx="2"/>')
    g["headset"] = g["headphones"] + '<path d="M18.75 21c0 1-1.6 1.5-4.25 1.5"/>'
    g["speaker-box"] = ('<rect x="6" y="3" width="12" height="18" rx="2.5"/><circle cx="12" cy="14.5" r="3"/>'
                        '<path d="M12 7.5h0" stroke-width="2.6"/>')

    for lvl in range(0, 101, 10):
        g[f"battery-{lvl:03d}"] = battery(lvl)
        g[f"battery-{lvl:03d}-charging"] = battery(lvl, charging=True)
    g["battery-missing"] = battery(None)
    g["ac"] = AC
    g["speedometer"] = GAUGE + '<path d="M12 14.5V8.5"/>'
    g["speedometer-fast"] = GAUGE + '<path d="M12 14.5l4.2-4.2"/>'
    g["leaf"] = '<path d="M5 19c0-8 5-14 14-14 0 9-6 14-14 14zM5 19l7-7"/>'
    g["moon"] = f'<path d="{MOON}"/>'
    g["moon-off"] = f'<path d="{MOON}"/>' + SLASH

    g["sun"] = SUN
    g["sun-low"] = ('<circle cx="12" cy="12" r="4"/>'
                    '<path d="M12 5h0M12 19h0M5 12h0M19 12h0M7 7h0M17 17h0M7 17h0M17 7h0" stroke-width="2.4"/>')
    g["sunset"] = '<path d="M3 18h18M7.5 18a4.5 4.5 0 0 1 9 0M12 8v2.5M6 10.5l1.6 1.6M18 10.5l-1.6 1.6M8 21.5h8"/>'
    g["sunset-off"] = g["sunset"] + SLASH
    g["display-brightness"] = ('<rect x="3" y="4" width="18" height="12" rx="2"/><path d="M8 20h8M12 16v4"/>'
                               '<circle cx="12" cy="10" r="2.2"/>')
    g["keyboard"] = ('<rect x="2" y="6" width="20" height="12" rx="2"/>'
                     '<path d="M6 10h0M10 10h0M14 10h0M18 10h0M7 14h10"/>')
    g["keyboard-brightness"] = ('<rect x="2" y="10" width="20" height="11" rx="2"/>'
                                '<path d="M6 14h0M10 14h0M14 14h0M18 14h0M7 17.5h10M12 2.5v2M6.5 4l1.2 1.2M17.5 4l-1.2 1.2"/>')

    g["bell"] = BELL
    g["bell-new"] = BELL + ('<circle cx="18.5" cy="5" r="3" fill="currentColor" stroke="none" '
                            'class="ColorScheme-Highlight"/>')
    g["dnd"] = f'<path d="{MOON}"/>'

    g["chevron-up"] = '<path d="M6 15l6-6 6 6"/>'
    g["chevron-down"] = '<path d="M6 9l6 6 6-6"/>'
    g["chevron-left"] = '<path d="M15 6l-6 6 6 6"/>'
    g["chevron-right"] = '<path d="M9 6l6 6-6 6"/>'
    g["arrow-left"] = '<path d="M19 12H5M11 6l-6 6 6 6"/>'
    g["arrow-right"] = '<path d="M5 12h14M13 6l6 6-6 6"/>'
    g["arrow-up"] = '<path d="M12 19V5M6 11l6-6 6 6"/>'
    g["arrow-down"] = '<path d="M12 5v14M6 13l6 6 6-6"/>'

    g["play"] = '<path d="M8 5.5v13l10.5-6.5z" fill="currentColor"/>'
    g["pause"] = ('<rect x="6.5" y="5" width="3.5" height="14" rx="1" fill="currentColor"/>'
                  '<rect x="14" y="5" width="3.5" height="14" rx="1" fill="currentColor"/>')
    g["stop"] = '<rect x="6" y="6" width="12" height="12" rx="2.5" fill="currentColor"/>'
    g["skip-forward"] = '<path d="M5 6v12l9-6z" fill="currentColor"/><path d="M18 6v12"/>'
    g["skip-backward"] = '<path d="M19 6v12l-9-6z" fill="currentColor"/><path d="M6 6v12"/>'
    repeat = ('<path d="M17 2.5l3 3-3 3M4 11.5V10a4.5 4.5 0 0 1 4.5-4.5H20M7 21.5l-3-3 3-3'
              'M20 12.5V14a4.5 4.5 0 0 1-4.5 4.5H4"/>')
    g["repeat"] = repeat
    g["repeat-one"] = repeat + '<path d="M12 10v4"/>'
    g["shuffle"] = '<path d="M16 3h5v5M4 20L21 3M21 16v5h-5M15 15l6 6M4 4l5 5"/>'
    g["note"] = '<path d="M9 18V5l11-2v13"/><circle cx="6.5" cy="18" r="2.5"/><circle cx="17.5" cy="16" r="2.5"/>'

    g["usb"] = '<rect x="7" y="8" width="10" height="13" rx="2"/><path d="M9 8V3h6v5M11 5.5h0M13 5.5h0"/>'
    g["harddisk"] = ('<rect x="3" y="6" width="18" height="12" rx="2"/><path d="M7 14h2"/>'
                     '<path d="M16.5 14h0" stroke-width="2.4"/>')
    g["eject"] = '<path d="M5 14h14L12 6z"/><path d="M5 18.5h14"/>'
    g["paste"] = '<rect x="5" y="5" width="14" height="16" rx="2"/><rect x="9" y="3" width="6" height="4" rx="1"/>'
    g["copy"] = ('<rect x="8" y="8" width="12" height="13" rx="2"/>'
                 '<path d="M16 8V5a2 2 0 0 0-2-2H6a2 2 0 0 0-2 2v10a2 2 0 0 0 2 2h2"/>')
    g["cut"] = ('<circle cx="6" cy="18" r="3"/><circle cx="18" cy="18" r="3"/>'
                '<path d="M8.2 16L18 3M15.8 16L6 3"/>')
    g["rename"] = '<rect x="3" y="7" width="14" height="10" rx="2"/><path d="M20 4v16M18 4h4M18 20h4"/>'
    g["share"] = '<path d="M12 15V3M7 8l5-5 5 5"/><path d="M5 13v6a2 2 0 0 0 2 2h10a2 2 0 0 0 2-2v-6"/>'
    g["bin"] = BIN
    g["printer"] = '<path d="M6 9V3h12v6M6 17H4v-8h16v8h-2"/><rect x="7" y="14" width="10" height="7"/>'
    g["printer-printing"] = g["printer"] + '<path d="M9.5 16.8h5M9.5 18.8h3"/>'
    g["printer-error"] = g["printer"] + '<path d="M10.5 15.5l3 3M13.5 15.5l-3 3"/>'

    g["shield-tick"] = f'<path d="{SHIELD}"/><path d="M9 12l2 2 4-4"/>'
    g["shield"] = f'<path d="{SHIELD}"/>'
    g["shield-alert"] = f'<path d="{SHIELD}"/><path d="M12 8v4.5"/><path d="M12 16h0" stroke-width="2.6"/>'

    g["search"] = '<circle cx="11" cy="11" r="7"/><path d="M20 20l-3.5-3.5"/>'
    g["menu"] = '<path d="M4 6h16M4 12h16M4 18h16"/>'
    g["cog"] = COG
    g["power"] = '<path d="M12 3v9"/><path d="M6.3 6.8a8 8 0 1 0 11.4 0"/>'
    g["restart"] = '<path d="M20 12a8 8 0 1 1-2.3-5.7"/><path d="M20 4v5h-5"/>'
    g["sign-out"] = '<path d="M15 4h4v16h-4"/><path d="M10 8l-4 4 4 4M6 12h10"/>'
    g["padlock"] = '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/>'
    g["close"] = '<path d="M6 6l12 12M18 6L6 18"/>'
    g["minus"] = '<path d="M5 12h14"/>'
    g["maximise"] = '<rect x="5" y="5" width="14" height="14" rx="3"/>'
    g["restore"] = ('<rect x="4" y="8" width="12" height="12" rx="2.5"/>'
                    '<path d="M8 8V6.5A2.5 2.5 0 0 1 10.5 4h7A2.5 2.5 0 0 1 20 6.5v7a2.5 2.5 0 0 1-2.5 2.5H16"/>')
    g["pin"] = '<path d="M12 17v5M8 3h8l-1 7 3 3v2H6v-2l3-3z"/>'
    g["plus"] = '<path d="M12 5v14M5 12h14"/>'
    g["tick"] = '<path d="M5 12l5 5 9-10"/>'
    g["cancel"] = '<circle cx="12" cy="12" r="9"/><path d="M5.6 5.6l12.8 12.8"/>'
    g["folder"] = FOLDER_LINE
    g["more"] = ('<circle cx="5" cy="12" r="1.8" fill="currentColor" stroke="none"/>'
                 '<circle cx="12" cy="12" r="1.8" fill="currentColor" stroke="none"/>'
                 '<circle cx="19" cy="12" r="1.8" fill="currentColor" stroke="none"/>')
    g["globe"] = ('<circle cx="12" cy="12" r="9"/><path d="M3 12h18"/>'
                  '<path d="M12 3c2.6 2.8 3.8 5.8 3.8 9s-1.2 6.2-3.8 9c-2.6-2.8-3.8-5.8-3.8-9s1.2-6.2 3.8-9z"/>')
    g["home"] = '<path d="M4 11l8-7 8 7v9H4z"/>'
    g["camera"] = '<rect x="3" y="7" width="13" height="11" rx="2"/><path d="M16 11l5-3v9l-5-3z"/>'
    g["phone"] = '<rect x="7" y="2.5" width="10" height="19" rx="2.5"/><path d="M11 18.5h2"/>'
    g["monitor"] = '<rect x="3" y="4" width="18" height="13" rx="2"/><path d="M8 21h8M12 17v4"/>'
    g["laptop"] = '<rect x="4" y="5" width="16" height="11" rx="1.5"/><path d="M2 19.5h20"/>'
    g["accessibility"] = '<circle cx="12" cy="4.5" r="1.8"/><path d="M5 8.5l7 1.5 7-1.5M12 10v5M9 21l3-6 3 6"/>'
    g["user"] = '<circle cx="12" cy="8" r="4"/><path d="M4 21c1.5-4 4.5-6 8-6s6.5 2 8 6"/>'
    g["select-all"] = ('<path d="M4 8V6a2 2 0 0 1 2-2h2M16 4h2a2 2 0 0 1 2 2v2M20 16v2a2 2 0 0 1-2 2h-2'
                       'M8 20H6a2 2 0 0 1-2-2v-2"/><rect x="8.5" y="8.5" width="7" height="7" rx="1.5"/>')
    g["details"] = '<path d="M9 6h11M9 12h11M9 18h11"/><path d="M4.5 6h0M4.5 12h0M4.5 18h0" stroke-width="2.6"/>'
    g["key"] = '<circle cx="8" cy="15.5" r="4.5"/><path d="M11.2 12.3L19.5 4M16 7.5l2.5 2.5M14 9.5l2 2"/>'
    g["info"] = '<circle cx="12" cy="12" r="9"/><path d="M12 11v5.5"/><path d="M12 7.5h0" stroke-width="2.6"/>'
    g["percent"] = '<path d="M19 5L5 19"/><circle cx="7" cy="7" r="2.5"/><circle cx="17" cy="17" r="2.5"/>'
    return g


WIFI_LEVELS = {"0": 1, "20": 2, "40": 2, "60": 3, "80": 4, "100": 4}
MOBILE_LEVELS = {"0": 0, "20": 1, "40": 2, "60": 3, "80": 4, "100": 4}
MOBILE_TECH = ["", "-5g", "-edge", "-gprs", "-hsdpa", "-hspa", "-hsupa", "-lte", "-umts"]


def symbolic_names():
    """Glyph key for every name, taken from the tray applets' QML and the
    plasma-nm, plasma-pa and workspace libraries in Debian trixie.

    Every name is written plain and with -symbolic. Breeze ships both, and
    KIconLoader tries the exact name in every inherited theme before
    shortening it, so a missing form would fall through to Breeze."""
    m = {}

    def add(key, *names):
        for n in names:
            m[n] = key

    for pct, lit in WIFI_LEVELS.items():
        add(f"wifi-{lit}", f"network-wireless-{pct}", f"network-wireless-connected-{pct.zfill(2)}")
        add(f"wifi-{lit}-locked", f"network-wireless-{pct}-locked")
        add(f"wifi-{lit}-limited", f"network-wireless-{pct}-limited")
    add("wifi-2", "network-wireless-connected-25", "network-wireless-signal-weak")
    add("wifi-3", "network-wireless-connected-50", "network-wireless-signal-ok")
    add("wifi-4", "network-wireless-connected-75", "network-wireless-signal-good",
        "network-wireless-signal-excellent", "network-wireless", "network-wireless-on")
    add("wifi-1", "network-wireless-signal-none")
    add("wifi-0", "network-wireless-acquiring", "network-wireless-available")
    add("wifi-disconnected", "network-wireless-disconnected", "network-wireless-offline")
    add("wifi-off", "network-wireless-off")
    add("hotspot", "network-wireless-hotspot")
    add("bluetooth", "network-wireless-bluetooth")
    for pct, lit in MOBILE_LEVELS.items():
        for tech in MOBILE_TECH:
            add(f"bars-{lit}", f"network-mobile-{pct}{tech}", f"network-mobile-{pct}{tech}-locked")
    add("bars-4", "network-mobile", "network-mobile-on")
    add("bars-0", "network-mobile-available")
    add("bars-off", "network-mobile-off")
    add("wired", "network-wired", "network-wired-activated")
    add("wired-limited", "network-wired-activated-limited")
    add("wired-locked", "network-wired-activated-locked")
    add("wired-faded", "network-wired-available")
    add("wired-disconnected", "network-wired-disconnected", "network-wired-unavailable", "network-offline")
    add("no-network", "network-unavailable")
    add("vpn", "network-vpn")
    add("plane", "network-flightmode-on", "airplane-mode")
    add("plane-off", "network-flightmode-off", "airplane-mode-disabled")
    add("connect", "network-connect")
    add("disconnect", "network-disconnect")

    add("bluetooth", "network-bluetooth", "bluetooth", "bluetooth-online")
    add("bluetooth-active", "network-bluetooth-activated", "network-bluetooth-activated-locked",
        "bluetooth-active", "bluetooth-paired")
    add("bluetooth-off", "network-bluetooth-inactive", "bluetooth-disabled", "bluetooth-offline")

    add("volume-muted", "audio-volume-muted")
    add("volume-low", "audio-volume-low")
    add("volume-medium", "audio-volume-medium")
    add("volume-high", "audio-volume-high")
    add("volume-warning", "audio-volume-high-warning", "audio-volume-overamplified")
    add("volume-danger", "audio-volume-high-danger")
    add("mic", "microphone-sensitivity-low", "microphone-sensitivity-medium", "microphone-sensitivity-high",
        "audio-input-microphone")
    add("mic-muted", "microphone-sensitivity-muted", "audio-input-microphone-muted")

    for lvl in range(0, 101, 10):
        for profile in ("", "-profile-balanced", "-profile-performance", "-profile-powersave"):
            add(f"battery-{lvl:03d}", f"battery-{lvl:03d}{profile}")
            add(f"battery-{lvl:03d}-charging", f"battery-{lvl:03d}-charging{profile}")
    add("battery-100", "battery-full", "battery-ups")
    add("battery-100-charging", "battery-full-charging", "battery-full-charged")
    add("battery-missing", "battery-missing")
    add("ac", "ac-adapter", "battery-ac-adapter")
    add("speedometer", "speedometer", "battery-profile-balanced")
    add("speedometer-fast", "battery-profile-performance")
    add("leaf", "battery-profile-powersave")
    add("moon", "system-suspend", "system-suspend-uninhibited")
    add("moon-off", "system-suspend-inhibited")

    add("sun", "brightness-high", "redshift-status-day")
    add("sun-low", "brightness-low")
    add("sunset", "redshift-status-on")
    add("sunset-off", "redshift-status-off")
    add("display-brightness", "video-display-brightness")
    add("keyboard", "input-keyboard")
    add("keyboard-brightness", "input-keyboard-brightness")

    add("bell", "notification-inactive", "notification-empty", "notifications")
    add("bell-new", "notification-active")
    add("dnd", "notifications-disabled", "notification-disabled")

    add("chevron-up", "arrow-up", "pan-up", "collapse")
    add("chevron-down", "arrow-down", "pan-down", "expand")
    add("chevron-left", "arrow-left", "pan-start")
    add("chevron-right", "arrow-right", "pan-end")
    add("arrow-left", "go-previous")
    add("arrow-right", "go-next")
    add("arrow-up", "go-up")
    add("arrow-down", "go-down")

    add("play", "media-playback-start", "media-playback-playing")
    add("pause", "media-playback-pause", "media-playback-paused")
    add("stop", "media-playback-stop", "media-playback-stopped")
    add("skip-forward", "media-skip-forward")
    add("skip-backward", "media-skip-backward")
    add("repeat", "media-playlist-repeat")
    add("repeat-one", "media-playlist-repeat-song")
    add("shuffle", "media-playlist-shuffle")
    add("note", "media-album-cover")

    add("usb", "device-notifier")
    add("eject", "media-eject")
    add("paste", "klipper", "edit-paste")
    add("copy", "edit-copy")
    add("cut", "edit-cut")
    add("rename", "edit-rename")
    add("share", "document-share")
    add("bin", "edit-delete", "edit-clear-history")
    add("printer", "printer")
    add("printer-printing", "printer-printing")
    add("printer-error", "printer-error", "printer-warning")

    add("shield-tick", "security-high")
    add("shield", "security-medium")
    add("shield-alert", "security-low")

    add("search", "edit-find", "system-search")
    add("menu", "application-menu", "open-menu")
    add("cog", "configure")
    add("power", "system-shutdown")
    add("restart", "system-reboot", "view-refresh")
    add("sign-out", "system-log-out")
    add("padlock", "system-lock-screen")
    add("close", "window-close", "edit-clear")
    add("minus", "window-minimize", "list-remove")
    add("maximise", "window-maximize")
    add("restore", "window-restore")
    add("pin", "window-pin")
    add("plus", "list-add")
    add("tick", "dialog-ok", "checkmark")
    add("cancel", "dialog-cancel")
    add("folder", "document-open-folder", "document-open")
    add("more", "view-more", "view-more-horizontal")
    add("home", "go-home")
    add("select-all", "edit-select-all")
    add("details", "view-list-details")
    add("key", "dialog-password")
    add("pause", "emblem-pause")
    add("info", "documentinfo")
    add("percent", "format-number-percent")
    return m


# Names whose plain form is a coloured System Settings, device or app icon
# in Breeze. Only the -symbolic name is replaced so those stay intact.
SYMBOLIC_ONLY = {
    "bluetooth": ["preferences-system-bluetooth-symbolic"],
    "bell": ["preferences-desktop-notification-bell-symbolic", "preferences-desktop-notification-symbolic"],
    "globe": ["internet-web-browser-symbolic", "preferences-system-network-symbolic"],
    "folder": ["folder-symbolic", "folder-open-symbolic"],
    "headphones": ["audio-headphones-symbolic"],
    "headset": ["audio-headset-symbolic"],
    "speaker-box": ["audio-speakers-symbolic", "audio-card-symbolic"],
    "camera": ["camera-web-symbolic"],
    "phone": ["phone-symbolic"],
    "monitor": ["computer-symbolic", "video-display-symbolic"],
    "laptop": ["computer-laptop-symbolic"],
    "accessibility": ["preferences-desktop-accessibility-symbolic"],
    "user": ["user-symbolic", "user-identity-symbolic"],
    "harddisk": ["drive-harddisk-symbolic"],
    "usb": ["drive-removable-media-symbolic", "drive-removable-media-usb-symbolic",
            "drive-removable-media-usb-pendrive-symbolic", "device-notifier-symbolic"],
    "bin": ["user-trash-symbolic", "user-trash-full-symbolic"],
}

# Names that need a line glyph in panels and lists but a coloured drawing on
# the desktop and in Dolphin's icon view. The line version goes in the
# symbolic directories, which stop at 32 px; the coloured one in
# scalable/places takes over above that.
DUAL = {
    "bin": ["user-trash", "user-trash-full"],
    "harddisk": ["drive-harddisk"],
    "usb": ["drive-removable-media", "drive-removable-media-usb", "drive-removable-media-usb-pendrive"],
}


def colour_extras():
    metal = grad("g", "#e6eaf1", "#9aa4b8")
    bin_body = ('<path d="M11 16h26l-2.3 25.2a3 3 0 0 1-3 2.8H16.3a3 3 0 0 1-3-2.8z" fill="url(#g)"/>'
                '<path d="M19 22v15M24 22v15M29 22v15" fill="none" stroke="#7b8498" stroke-width="2" '
                'stroke-linecap="round"/>')
    lid = ('<path d="M20 10V8a2 2 0 0 1 2-2h4a2 2 0 0 1 2 2v2" fill="none" stroke="#8f98ab" stroke-width="2.6"/>'
           '<rect x="7" y="10" width="34" height="5.5" rx="2.75" ry="2.75" fill="#c3cad8"/>')
    trash = metal + bin_body + lid
    full = (metal + bin_body
            + '<path d="M14 15.5l2.5-8.5 9 2.2-1.6 6.3z" fill="#f3f5fa"/>'
            '<path d="M24 15.5l6-9.5 7.5 5-2.8 4.5z" fill="#ffe58c"/>'
            '<g transform="rotate(-8 40 13)">' + lid + '</g>')
    disk = (metal + '<rect x="4" y="12" width="40" height="25" rx="5" ry="5" fill="url(#g)"/>'
            '<rect x="4" y="12" width="40" height="2.2" rx="1.1" ry="1.1" fill="#ffffff" fill-opacity="0.5"/>'
            '<path d="M10 30h15" fill="none" stroke="#7b8498" stroke-width="2.6" stroke-linecap="round"/>'
            '<circle cx="37" cy="30" r="2" fill="#7dffb0"/>')
    stick = (grad("b", "#4fb0ff", "#2356cf")
             + '<rect x="18" y="4" width="12" height="14" rx="1.5" ry="1.5" fill="#c3cad8"/>'
             '<rect x="20.5" y="8" width="2.6" height="3" fill="#7b8498"/>'
             '<rect x="24.9" y="8" width="2.6" height="3" fill="#7b8498"/>'
             '<rect x="14" y="16" width="20" height="28" rx="4.5" ry="4.5" fill="url(#b)"/>'
             '<rect x="14" y="16" width="20" height="2.2" rx="1.1" ry="1.1" fill="#ffffff" fill-opacity="0.3"/>'
             '<circle cx="24" cy="37" r="2" fill="#ffffff" fill-opacity="0.85"/>')
    return {"user-trash": trash, "user-trash-full": full, "drive-harddisk": disk,
            "drive-removable-media": stick, "drive-removable-media-usb": stick,
            "drive-removable-media-usb-pendrive": stick}


INDEX = """[Icon Theme]
Name=Wild OS
Comment=App, folder, document and interface icons drawn for Wild OS
Inherits=breeze-dark,hicolor
Example=folder
DisplayDepth=32
# Lets KIconLoader swap in the colour scheme for the symbolic glyphs, and
# makes it honour -symbolic names instead of falling back to plain ones.
FollowsColorScheme=true

# Directories are listed small first: KIconLoader keeps the first of two
# equally good matches, so a 16 px lookup lands on the simplified drawings
# and the 2-unit glyph strokes before the scalable ones.
Directories={dirs}
"""

# Defaults for apps that do not recolour; KIconLoader replaces this block
# with the active colour scheme.
STYLE = ('<style type="text/css" id="current-color-scheme">'
         '.ColorScheme-Text{color:#eef1f8;}.ColorScheme-Highlight{color:#8fb4ff;}'
         '.ColorScheme-NegativeText{color:#ec5546;}</style>')


def svg(body):
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="48" height="48" viewBox="0 0 48 48">'
            f"{body}</svg>\n")


def symbolic_svg(body, px, stroke):
    """Give every shape the stroke and colour class unless it sets its own.

    Attributes go on each element rather than a group because Qt's SVG
    renderer resolves the class colour on the element that carries it."""
    def fix(m):
        tag, attrs = m.group(1), m.group(2)
        if "class=" not in attrs:
            attrs += ' class="ColorScheme-Text"'
        if "fill=" not in attrs:
            attrs += ' fill="none"'
        if "stroke=" not in attrs:
            attrs += ' stroke="currentColor"'
        if "stroke-width=" not in attrs:
            attrs += f' stroke-width="{stroke}"'
        attrs += ' stroke-linecap="round" stroke-linejoin="round"'
        return f"<{tag}{attrs}/>"
    body = re.sub(r"<(path|rect|circle|ellipse)([^>]*?)/>", fix, body)
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{px}" height="{px}" viewBox="0 0 24 24">'
            f"{STYLE}{body}</svg>\n")


def write(path, body):
    write_text(path, svg(body))


def write_text(path, text):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(text, encoding="utf-8", newline="\n")


def main(out):
    out = Path(out)
    full, small = drawings(), small_drawings()

    glyphs = symbolic_glyphs()
    names = {}
    for name, key in symbolic_names().items():
        names[name] = key
        names[name + "-symbolic"] = key
    for table in (SYMBOLIC_ONLY, DUAL):
        for key, extra in table.items():
            for name in extra:
                names[name] = key
    for name, key in sorted(names.items()):
        write_text(out / "16x16" / "symbolic" / f"{name}.svg", symbolic_svg(glyphs[key], 16, 2))
        write_text(out / "scalable" / "symbolic" / f"{name}.svg", symbolic_svg(glyphs[key], 24, 1.8))

    dirs = ["16x16/symbolic", "scalable/symbolic"]
    sections = [
        # Up to 20 px takes the 2-unit stroke, so the 18 px tray icons match
        # the design's taskbar.
        "[16x16/symbolic]\nSize=16\nMinSize=8\nMaxSize=20\nContext=Status\nType=Scalable\n",
        "[scalable/symbolic]\nSize=24\nMinSize=21\nMaxSize=32\nContext=Status\nType=Scalable\n",
    ]
    for ctx, ctx_name, table in CONTEXTS:
        for key, names_ in table.items():
            for name in names_:
                write(out / "scalable" / ctx / f"{name}.svg", full[key])
                if key in small:
                    write(out / "16x16" / ctx / f"{name}.svg", small[key])
        dirs_small = f"16x16/{ctx}"
        sections.append(f"[{dirs_small}]\nSize=16\nContext={ctx_name}\nType=Fixed\n")
        sections.append(f"[scalable/{ctx}]\nSize=48\nMinSize=16\nMaxSize=512\nContext={ctx_name}\nType=Scalable\n")
        dirs += [dirs_small, f"scalable/{ctx}"]
    for name, body in colour_extras().items():
        write(out / "scalable" / "places" / f"{name}.svg", body)
    index = INDEX.format(dirs=",".join(dirs)) + "\n" + "\n".join(sections)
    (out / "index.theme").write_text(index, encoding="utf-8", newline="\n")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
