#!/usr/bin/env python3
"""Generate the Wild OS Plasma style's glass frames from design tokens.

Plasma draws panels and popups from nine-patch SVGs: four corners, four edges
and a centre, each a separately named element, plus "mask-" copies that tell
KWin where to blur. This writes those files so the frames match the design's
glass tokens exactly instead of approximating them by hand.

    python3 tools/glass-svg.py files/desktoptheme

Rerun it after changing a token below; the output is committed.

Plasma sizes every element by its bounding box and scales the edge and centre
pieces to fit, so each piece here is a solid tile of exactly its own size with
no strokes. A stroke reaching half a pixel outside a tile shifts its bounding
box, and the scaled pieces then leave hairline seams at every join.
"""

import math
import sys
from pathlib import Path

# From the Wild OS Design System tokens.
GLASS_BAR = ((24, 26, 34), 0.92)      # glass-bar: the floating taskbar (Mica)
GLASS_PANEL = ((35, 37, 41), 0.96)    # glass-panel: Start, flyouts, tooltips (Mica)
BORDER = ((255, 255, 255), 0.12)      # the taskbar's 1px edge
PANEL_BORDER = ((255, 255, 255), 0.093)  # border-glass on Mica: the panels' 1px edge
HIGHLIGHT = ((255, 255, 255), 0.10)   # an inset top highlight; none on dark Mica

# (fill, radius, content padding, top highlight)
FRAMES = {
    "widgets/panel-background.svg": (GLASS_BAR, 20, 8, False),
    "dialogs/background.svg": (GLASS_PANEL, 22, 14, False),
    "widgets/tooltip.svg": (GLASS_PANEL, 12, 10, False),
}

STEPS = 24  # points per quarter circle; plenty at these radii


def paint(colour):
    (r, g, b), a = colour
    return f'fill="rgb({r},{g},{b})" fill-opacity="{a}"'


def arc(cx, cy, radius, start, end):
    """Points along a circle from angle start to end (degrees, y down)."""
    pts = []
    for i in range(STEPS + 1):
        t = math.radians(start + (end - start) * i / STEPS)
        pts.append((cx + radius * math.cos(t), cy + radius * math.sin(t)))
    return pts


def poly(points):
    return "M" + " L".join(f"{x:.3f},{y:.3f}" for x, y in points) + " Z"


def corner(r, which):
    """Return (body path, border ring path) for one r x r corner tile.

    The tile's origin is its own top-left; the caller translates it.
    """
    # Centre of the rounding circle and the arc's angle range per corner.
    cx, cy, a0, a1, outer = {
        "topleft": (r, r, 180, 270, (0, 0)),
        "topright": (0, r, 270, 360, (r, 0)),
        "bottomright": (0, 0, 0, 90, (r, r)),
        "bottomleft": (r, 0, 90, 180, (0, r)),
    }[which]
    outer_arc = arc(cx, cy, r, a0, a1)
    inner_arc = arc(cx, cy, r - 1, a0, a1)
    # Body: the quarter disc, i.e. the arc plus the circle's centre.
    body = poly(outer_arc + [(cx, cy)])
    # Border: the 1 px band between the outer and inner arcs.
    ring = poly(outer_arc + list(reversed(inner_arc)))
    return body, ring


def frame(colour, r, pad, highlight):
    s = r
    fill = paint(colour)
    edge = paint(PANEL_BORDER if colour == GLASS_PANEL else BORDER)
    hi = paint(HIGHLIGHT)
    out = []

    def group(name, x, y, inner, mask):
        # The transparent rect pins the element's bounding box to the tile.
        box = f'<rect x="0" y="0" width="{s}" height="{s}" fill="#000" fill-opacity="0"/>'
        out.append(f'<g id="{name}" transform="translate({x},{y})">{box}{inner}</g>')

    for mask in (False, True):
        p = "mask-" if mask else ""
        m = 'fill="#000"'
        for which, (x, y) in {
            "topleft": (0, 0), "topright": (2 * s, 0),
            "bottomleft": (0, 2 * s), "bottomright": (2 * s, 2 * s),
        }.items():
            body, ring = corner(r, which)
            if mask:
                group(p + which, x, y, f'<path d="{body}" {m}/>', mask)
            else:
                extra = ""
                if highlight and which.startswith("top"):
                    hb, hr = corner(r - 1, which)
                    shift = (1, 1) if which == "topleft" else (0, 1)
                    extra = f'<path d="{hr}" {hi} transform="translate({shift[0]},{shift[1]})"/>'
                group(p + which, x, y, f'<path d="{body}" {fill}/><path d="{ring}" {edge}/>{extra}', mask)

        for name, (x, y), line in [
            ("top", (s, 0), (0, 0, s, 1)),
            ("bottom", (s, 2 * s), (0, s - 1, s, 1)),
            ("left", (0, s), (0, 0, 1, s)),
            ("right", (2 * s, s), (s - 1, 0, 1, s)),
        ]:
            if mask:
                group(p + name, x, y, f'<rect x="0" y="0" width="{s}" height="{s}" {m}/>', mask)
            else:
                lx, ly, lw, lh = line
                inner = f'<rect x="0" y="0" width="{s}" height="{s}" {fill}/>'
                inner += f'<rect x="{lx}" y="{ly}" width="{lw}" height="{lh}" {edge}/>'
                if highlight and name == "top":
                    inner += f'<rect x="0" y="1" width="{s}" height="1" {hi}/>'
                group(p + name, x, y, inner, mask)

        group(p + "center", s, s,
              f'<rect x="0" y="0" width="{s}" height="{s}" {m if mask else fill}/>', mask)

    for side in ("top", "bottom", "left", "right"):
        out.append(f'<rect id="hint-{side}-margin" x="0" y="0" width="{pad}" height="{pad}" fill="none"/>')

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{3 * s}" height="{3 * s}" '
        f'viewBox="0 0 {3 * s} {3 * s}">' + "".join(out) + "</svg>\n"
    )


def tasks():
    """Taskbar button states, from the design's taskbar.

    Running apps get a short line under the icon (6x3, white at 70%); the
    focused app's is wider and in the accent colour; hover is a soft rounded
    fill. Plasma stretches only the edge and centre pieces, so the width of
    the corner pieces sets how long the line is on a button of known width.
    TASK_W is the button width Plasma gives a 54 px bar.
    """
    TASK_W = 60
    lines = {
        "normal": (6, ((255, 255, 255), 0.70)),
        "minimized": (6, ((255, 255, 255), 0.40)),
        "attention": (6, ((242, 196, 107), 1.0)),
        "focus": (16, ((143, 180, 255), 1.0)),
    }
    out = []

    def tile(name, x, y, w, h, inner=""):
        box = f'<rect x="0" y="0" width="{w}" height="{h}" fill="#000" fill-opacity="0"/>'
        out.append(f'<g id="{name}" transform="translate({x},{y})">{box}{inner}</g>')

    y0 = 0
    for state, (length, colour) in lines.items():
        c = (TASK_W - length) // 2
        e = 6                       # edge thickness; the line sits in the bottom one
        line = f'<rect x="0" y="1" width="{{w}}" height="3" rx="1.5" {paint(colour)}/>'
        for prefix in (f"{state}-", f"south-{state}-"):
            tile(prefix + "topleft", 0, y0, c, e)
            tile(prefix + "top", c, y0, length, e)
            tile(prefix + "topright", c + length, y0, c, e)
            tile(prefix + "left", 0, y0 + e, c, e)
            tile(prefix + "center", c, y0 + e, length, e)
            tile(prefix + "right", c + length, y0 + e, c, e)
            tile(prefix + "bottomleft", 0, y0 + 2 * e, c, e)
            # Rounded ends on the line need the whole line in one piece.
            tile(prefix + "bottom", c, y0 + 2 * e, length, e, line.format(w=length))
            tile(prefix + "bottomright", c + length, y0 + 2 * e, c, e)
            y0 += 3 * e + 4

    # Pinned apps that are not running use the unprefixed frame: draw
    # nothing, but it must exist or Plasma has no margins for the icon.
    for prefix in ("", "south-"):
        for i, name in enumerate(("topleft", "top", "topright", "left", "center",
                                  "right", "bottomleft", "bottom", "bottomright")):
            tile(prefix + name, (i % 3) * 4, y0 + (i // 3) * 4, 4, 4)
        y0 += 16

    hover = paint(((255, 255, 255), 0.09))
    r = 12
    for prefix in ("hover-", "south-hover-", "launcher-hover-", "south-launcher-hover-"):
        for which, (x, y) in {
            "topleft": (0, 0), "topright": (2 * r, 0),
            "bottomleft": (0, 2 * r), "bottomright": (2 * r, 2 * r),
        }.items():
            body, _ = corner(r, which)
            tile(prefix + which, x, y0 + y, r, r, f'<path d="{body}" {hover}/>')
        for name, (x, y) in {
            "top": (r, 0), "bottom": (r, 2 * r), "left": (0, r), "right": (2 * r, r), "center": (r, r),
        }.items():
            tile(prefix + name, x, y0 + y, r, r, f'<rect width="{r}" height="{r}" {hover}/>')
        y0 += 3 * r + 4

    # Plasma insets the icon by the frame's margins, which default to the
    # corner pieces' size; without explicit hints the long indicator corners
    # would squeeze the icon to nothing. Every prefix needs its own hints
    # because Plasma looks them up per prefix.
    states = list(lines) + ["hover", "launcher-hover"]
    for prefix in [p for st in states for p in (f"{st}-", f"south-{st}-")] + ["", "south-"]:
        if True:
            for side in ("top", "bottom", "left", "right"):
                out.append(f'<rect id="{prefix}hint-{side}-margin" x="0" y="0" width="4" height="4" fill="none"/>')

    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{TASK_W}" height="{y0}" '
        f'viewBox="0 0 {TASK_W} {y0}">' + "".join(out) + "</svg>\n"
    )


def tabbar():
    """No "expanded" line on panel buttons.

    Plasma marks an open panel popup (Start, the security chip, tray items)
    with a line drawn from widgets/tabbar. The design shows an open button
    with a tinted fill instead, which each button draws itself, so every
    active-tab piece here is empty.
    """
    out = []
    pieces = ("topleft", "top", "topright", "left", "center",
              "right", "bottomleft", "bottom", "bottomright")
    y = 0
    for edge in ("north-", "south-", "east-", "west-", ""):
        for i, name in enumerate(pieces):
            out.append(f'<rect id="{edge}active-tab-{name}" x="{(i % 3) * 2}" '
                       f'y="{y + (i // 3) * 2}" width="2" height="2" fill="#000" fill-opacity="0"/>')
        y += 6
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="6" height="{y}" '
            f'viewBox="0 0 6 {y}">' + "".join(out) + "</svg>\n")


def main():
    root = Path(sys.argv[1])
    (root / "widgets/tabbar.svg").write_text(tabbar())
    target = root / "widgets/tasks.svg"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(tasks())
    print(target)
    for path, (colour, radius, pad, highlight) in FRAMES.items():
        svg = frame(colour, radius, pad, highlight)
        # Plasma prefers translucent/ when blur is available; ship both so
        # the same glass is used either way.
        for prefix in ("", "translucent/"):
            target = root / prefix / path
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_text(svg)
            print(target)


if __name__ == "__main__":
    main()
