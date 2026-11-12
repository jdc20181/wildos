#!/usr/bin/env python3
"""Draws the desktop theme's popup frame, desktoptheme/dialogs/background.svg.

Start, the flyouts and every Plasma popup sit on this frame: the design's
panel glass, Mica rgba(35,37,41,0.96), with a 1px edge at white 0.093 and 20px
corners, plus the shadow the design gives flyouts. Plasma hands the
"shadow-*" pieces to KWin, which draws them around the popup; each piece
leaves the popup's own shape clear so the shadow never shows through the
glass. Run it after changing a value:

    python3 desktoptheme-dialogs.py
"""

import math
import os

HERE = os.path.dirname(os.path.abspath(__file__))
RADIUS = 20
GLASS = "rgb(35,37,41)"
GLASS_ALPHA = 0.96
EDGE_ALPHA = 0.093
# The design's flyout shadow is 0 40px 100px rgba(10,12,16,0.55). KWin draws
# a shadow on all four sides at one size, so this keeps its softness and
# depth over a 40px reach.
SHADOW = 40
SHADOW_COLOUR = "rgb(10,12,16)"
SHADOW_ALPHA = 0.30
RINGS = 20


def n(v):
    s = ("%.3f" % v).rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


def arc_points(cx, cy, r, a0, a1, steps=24):
    return [(cx + r * math.cos(a0 + (a1 - a0) * i / steps), cy + r * math.sin(a0 + (a1 - a0) * i / steps)) for i in range(steps + 1)]


def poly(points):
    return "M" + " L".join("%s,%s" % (n(x), n(y)) for x, y in points) + " Z"


class Sheet:
    def __init__(self):
        self.parts = []

    def group(self, gid, x, y, w, h, body):
        self.parts.append('<g id="%s" transform="translate(%s,%s)"><rect x="0" y="0" width="%s" height="%s" fill="#000" fill-opacity="0"/>%s</g>'
                          % (gid, n(x), n(y), n(w), n(h), body))

    def hint(self, gid, size):
        self.parts.append('<rect id="%s" x="0" y="0" width="%s" height="%s" fill="none"/>' % (gid, n(size), n(size)))


def frame_body(which, f, r):
    """Fill and edge for one corner of the frame, drawn for the top-left
    corner and mirrored for the others."""
    fill = []
    # Quarter disc of radius r centred at (f, f) when f == r.
    pts = [(0, f)] + arc_points(r, r, r, math.pi, 1.5 * math.pi) + [(f, 0), (f, f)]
    fill.append('<path d="%s" fill="%s" fill-opacity="%s"/>' % (poly(pts), GLASS, n(GLASS_ALPHA)))
    outer = arc_points(r, r, r, math.pi, 1.5 * math.pi)
    inner = arc_points(r, r, r - 1, 1.5 * math.pi, math.pi)
    fill.append('<path d="%s" fill="rgb(255,255,255)" fill-opacity="%s"/>' % (
        poly([(0, f)] + outer + [(f, 0), (f, 1)] + inner + [(1, f)]), n(EDGE_ALPHA)))
    return mirror(which, f, "".join(fill))


def mask_body(which, f, r):
    pts = [(0, f)] + arc_points(r, r, r, math.pi, 1.5 * math.pi) + [(f, 0), (f, f)]
    return mirror(which, f, '<path d="%s" fill="#000"/>' % poly(pts))


def mirror(which, f, body):
    sx = -1 if which in ("topright", "bottomright") else 1
    sy = -1 if which in ("bottomleft", "bottomright") else 1
    if sx == 1 and sy == 1:
        return body
    return '<g transform="translate(%s,%s) scale(%s,%s)">%s</g>' % (n(f if sx < 0 else 0), n(f if sy < 0 else 0), sx, sy, body)


def shadow_alpha(d):
    """Shadow strength d pixels outside the popup's edge."""
    t = d / float(SHADOW)
    return SHADOW_ALPHA * math.exp(-4.5 * t * t) * (1 - t)


def shadow_corner(which):
    """A corner piece of the shadow: SHADOW + RADIUS square, with the popup's
    rounded corner at its inner (bottom-right) edge left clear."""
    size = SHADOW + RADIUS
    cx = cy = SHADOW + RADIUS  # centre of the popup's corner arc
    body = []
    for i in range(RINGS):
        d0 = SHADOW * i / float(RINGS)
        d1 = SHADOW * (i + 1) / float(RINGS)
        a = shadow_alpha((d0 + d1) / 2)
        if a < 0.002:
            continue
        r_in = RADIUS + d0
        r_out = RADIUS + d1
        # Band between two arcs around the corner centre, closed along the
        # piece's inner edges.
        outer = arc_points(cx, cy, r_out, math.pi, 1.5 * math.pi)
        inner = arc_points(cx, cy, r_in, 1.5 * math.pi, math.pi)
        pts = outer + inner
        body.append('<path d="%s" fill="%s" fill-opacity="%s"/>' % (poly(pts), SHADOW_COLOUR, n(a)))
    return mirror(which, size, "".join(body))


def shadow_edge(side, length):
    size = SHADOW + RADIUS
    body = []
    for i in range(RINGS):
        d0 = SHADOW * i / float(RINGS)
        d1 = SHADOW * (i + 1) / float(RINGS)
        a = shadow_alpha((d0 + d1) / 2)
        if a < 0.002:
            continue
        # Distance from the popup edge, which lies SHADOW in from the outside.
        lo, hi = SHADOW - d1, SHADOW - d0
        if side == "top":
            x, y, w, h = 0, lo, length, hi - lo
        elif side == "bottom":
            x, y, w, h = 0, size - hi, length, hi - lo
        elif side == "left":
            x, y, w, h = lo, 0, hi - lo, length
        else:
            x, y, w, h = size - hi, 0, hi - lo, length
        body.append('<rect x="%s" y="%s" width="%s" height="%s" fill="%s" fill-opacity="%s"/>' % (n(x), n(y), n(w), n(h), SHADOW_COLOUR, n(a)))
    return "".join(body)


def build():
    S = Sheet()
    f = RADIUS
    mid = 20
    corners = {"topleft": (0, 0), "topright": (f + mid, 0), "bottomleft": (0, f + mid), "bottomright": (f + mid, f + mid)}
    for which, (x, y) in corners.items():
        S.group(which, x, y, f, f, frame_body(which, f, f))
    edges = {
        "top": (f, 0, mid, f, '<rect x="0" y="0" width="%d" height="%d" fill="%s" fill-opacity="%s"/><rect x="0" y="0" width="%d" height="1" fill="rgb(255,255,255)" fill-opacity="%s"/>' % (mid, f, GLASS, n(GLASS_ALPHA), mid, n(EDGE_ALPHA))),
        "bottom": (f, f + mid, mid, f, '<rect x="0" y="0" width="%d" height="%d" fill="%s" fill-opacity="%s"/><rect x="0" y="%d" width="%d" height="1" fill="rgb(255,255,255)" fill-opacity="%s"/>' % (mid, f, GLASS, n(GLASS_ALPHA), f - 1, mid, n(EDGE_ALPHA))),
        "left": (0, f, f, mid, '<rect x="0" y="0" width="%d" height="%d" fill="%s" fill-opacity="%s"/><rect x="0" y="0" width="1" height="%d" fill="rgb(255,255,255)" fill-opacity="%s"/>' % (f, mid, GLASS, n(GLASS_ALPHA), mid, n(EDGE_ALPHA))),
        "right": (f + mid, f, f, mid, '<rect x="0" y="0" width="%d" height="%d" fill="%s" fill-opacity="%s"/><rect x="%d" y="0" width="1" height="%d" fill="rgb(255,255,255)" fill-opacity="%s"/>' % (f, mid, GLASS, n(GLASS_ALPHA), f - 1, mid, n(EDGE_ALPHA))),
    }
    for side, (x, y, w, h, body) in edges.items():
        S.group(side, x, y, w, h, body)
    S.group("center", f, f, mid, mid, '<rect x="0" y="0" width="%d" height="%d" fill="%s" fill-opacity="%s"/>' % (mid, mid, GLASS, n(GLASS_ALPHA)))

    for which, (x, y) in corners.items():
        S.group("mask-" + which, x, y, f, f, mask_body(which, f, f))
    for side, (x, y, w, h, _) in edges.items():
        S.group("mask-" + side, x, y, w, h, '<rect x="0" y="0" width="%s" height="%s" fill="#000"/>' % (w, h))
    S.group("mask-center", f, f, mid, mid, '<rect x="0" y="0" width="%d" height="%d" fill="#000"/>' % (mid, mid))

    # Content sits 14px in from the frame's edge.
    for side in ("top", "bottom", "left", "right"):
        S.hint("hint-%s-margin" % side, 14)

    # The shadow, laid out below the frame.
    size = SHADOW + RADIUS
    oy = 2 * f + mid + 10
    S.group("shadow-topleft", 0, oy, size, size, shadow_corner("topleft"))
    S.group("shadow-topright", size + mid, oy, size, size, shadow_corner("topright"))
    S.group("shadow-bottomleft", 0, oy + size + mid, size, size, shadow_corner("bottomleft"))
    S.group("shadow-bottomright", size + mid, oy + size + mid, size, size, shadow_corner("bottomright"))
    S.group("shadow-top", size, oy, mid, size, shadow_edge("top", mid))
    S.group("shadow-bottom", size, oy + size + mid, mid, size, shadow_edge("bottom", mid))
    S.group("shadow-left", 0, oy + size, size, mid, shadow_edge("left", mid))
    S.group("shadow-right", size + mid, oy + size, size, mid, shadow_edge("right", mid))
    # How far the shadow reaches outside the popup on each side; the rest of
    # each piece lies under the popup's corner, where it is left clear.
    for side in ("top", "bottom", "left", "right"):
        S.hint("shadow-hint-%s-margin" % side, SHADOW)

    width = 2 * size + mid
    height = oy + 2 * size + mid
    return ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" viewBox="0 0 %d %d">%s</svg>\n'
            % (width, height, width, height, "".join(S.parts)))


def main():
    svg = build()
    for rel in ("desktoptheme/dialogs/background.svg", "desktoptheme/translucent/dialogs/background.svg"):
        path = os.path.join(HERE, rel)
        with open(path, "w", encoding="utf-8", newline="\n") as fh:
            fh.write(svg)
        print("wrote", path)


if __name__ == "__main__":
    main()
