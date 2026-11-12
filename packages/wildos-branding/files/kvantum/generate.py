#!/usr/bin/env python3
"""Builds the Wild OS Kvantum themes from the design tokens.

Kvantum slices every control into nine pieces looked up by id. Drawing them by
hand in Inkscape drifts from the tokens, so the pieces are computed here and
the files are regenerated whenever a token changes:

    python3 generate.py

It writes WildOS/WildOS.svg (dark) and the light variant WildOSLight/ (its
SVG, and its kvconfig derived from WildOS/WildOS.kvconfig with the light
colours swapped in). The patched Kvantum picks between the two by the colour
scheme applied, and switches open apps when it changes.

Only plain paths, rects and gradients are used. Qt 5's SVG renderer ignores
clip paths, masks and filters, and Qt 5 apps load this same file.

The patched Kvantum Wild OS ships (build/patches/kvantum) reads three extra
things from these files: the per-state default button pieces, [General]
menu_padding, and [General] accent_colors, the exact accent colours drawn
here, which it swaps for the accent picked in Settings.
"""

import colorsys
import os
import re

HERE = os.path.dirname(os.path.abspath(__file__))


def hex_rgb(h):
    h = h.lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4))


def rgb_hex(rgb):
    return "#%02x%02x%02x" % tuple(max(0, min(255, int(round(c)))) for c in rgb)


def qt_lighter(h, factor):
    """QColor::lighter, so the hover shade matches what the patch computes."""
    r, g, b = (c / 255 for c in hex_rgb(h))
    hh, s, v = colorsys.rgb_to_hsv(r, g, b)
    v = v * factor / 100
    if v > 1:
        s = max(0.0, s - (v - 1))
        v = 1.0
    return rgb_hex(c * 255 for c in colorsys.hsv_to_rgb(hh, s, v))


def qt_darker(h, factor):
    r, g, b = (c / 255 for c in hex_rgb(h))
    hh, s, v = colorsys.rgb_to_hsv(r, g, b)
    return rgb_hex(c * 255 for c in colorsys.hsv_to_rgb(hh, s, v * 100 / factor))


# The two schemes, from the Wild OS Design System (dark) and the README's
# light mode rules. "tone" is the colour translucent surfaces and edges are
# drawn in: white in dark mode, rgba(46,48,52) at three quarters of the
# alpha in light mode.
DARK = {
    "name": "WildOS",
    "tone": "#ffffff",
    "tone_scale": 1.0,
    "ink": "#ffffff",
    "strong": "#ffffff",
    "on_accent": "#0f1530",
    # The checked radio's centre is a fill, so it takes the neutral dark
    # (design v3); the ink stays for text and marks on the accent.
    "radio_dot": "#1a1c20",
    "accent": "#8fb4ff",
    "accent_hover": qt_lighter("#8fb4ff", 108),
    "accent_pressed": qt_darker("#8fb4ff", 108),
    "sunken": ("#0a0b0e", 0.35),
    "menu": ("#232529", 0.96),
    "window": "#181a22",
    "toggle_off": ("#ffffff", 0.55),
    "radio_ring": ("#ffffff", 0.60),
    "shadow": "#08090c",
    "shadow_alpha": 0.60,
}
LIGHT = {
    "name": "WildOSLight",
    "tone": "#202020",
    "tone_scale": 0.75,
    "ink": "#1a1a1a",
    "strong": "#111111",
    "on_accent": "#ffffff",
    "radio_dot": "#ffffff",
    "accent": "#3563d8",
    "accent_hover": qt_darker("#3563d8", 108),
    "accent_pressed": qt_darker("#3563d8", 118),
    "sunken": ("#fbfbfb", 0.55),
    "menu": ("#f0f0f0", 0.96),
    "window": "#f3f3f3",
    "toggle_off": ("#202020", 0.413),
    "radio_ring": ("#202020", 0.45),
    "shadow": "#1a1a1a",
    "shadow_alpha": 0.24,
}

# The light kvconfig is the dark one with these colours swapped.
LIGHT_KVCONFIG = {
    "#181a22": "#f3f3f3",
    "#14151b": "#fbfbfb",
    "#1a1c23": "#eeeeee",
    "#26282f": "#ebebeb",
    "#30323a": "#ffffff",
    "#272930": "#f3f3f3",
    "#0e0f13": "#c6c6c6",
    "#1c1e26": "#dfdfdf",
    "#8fb4ff": "#3563d8",
    "#232529": "#f0f0f0",
    "#ffffff8b": "#1a1a1a71",
    "#ffffffc8": "#1a1a1a9b",
    "#0f1530": "#ffffff",  # the ink on the accent
    "#b9cfff": "#2a55c4",
    "#c4b2ff": "#6a4bc4",
    "#ffffff": "#1a1a1a",
}

# Menus: 14 px corners, 6 px between the edge and the rows, and a shadow
# drawn inside the popup (Kvantum gives popups no shadow of its own).
MENU_RADIUS = 14
MENU_PADDING = 6
MENU_SHADOW = 12


def n(v):
    s = ("%.3f" % v).rstrip("0").rstrip(".")
    return s if s not in ("", "-0") else "0"


class Sheet:
    """Lays elements out left to right in rows so the SVG is readable in an editor."""

    def __init__(self):
        self.parts = []
        self.x = 10
        self.y = 10
        self.row_h = 0
        self.width = 1200
        self.max_x = 0

    def place(self, w, h):
        if self.x + w > self.width:
            self.x = 10
            self.y += self.row_h + 10
            self.row_h = 0
        pos = (self.x, self.y)
        self.x += w + 10
        self.row_h = max(self.row_h, h)
        self.max_x = max(self.max_x, self.x)
        return pos

    def group(self, gid, body):
        self.parts.append('<g id="%s">%s</g>' % (gid, body))

    def svg(self):
        h = self.y + self.row_h + 10
        return (
            '<?xml version="1.0" encoding="UTF-8"?>\n'
            '<svg xmlns="http://www.w3.org/2000/svg" '
            'xmlns:xlink="http://www.w3.org/1999/xlink" '
            'width="%d" height="%d" viewBox="0 0 %d %d">\n'
            "<defs>\n</defs>\n%s\n</svg>\n"
            % (self.max_x, h, self.max_x, h, "\n".join(self.parts))
        )


def paint(p):
    """p is (colour, opacity) or None for nothing."""
    if p is None:
        return 'fill="#000000" fill-opacity="0"'
    c, a = p
    if a >= 1:
        return 'fill="%s"' % c
    return 'fill="%s" fill-opacity="%s"' % (c, n(a))


def rect(x, y, w, h, p):
    return '<rect x="%s" y="%s" width="%s" height="%s" %s/>' % (n(x), n(y), n(w), n(h), paint(p))


def bbox(x, y, w, h):
    # Fixes an element's bounds: Kvantum scales each id by its bounding box.
    return rect(x, y, w, h, None)


def contour(f, r, o):
    """Top-left corner outline at inset o as nodes (x, y, arc radius or None).

    The arc radius on a node describes the segment arriving at that node.
    """
    ro = r - o
    if ro <= 0:
        return [(o, f, None), (o, o, None), (f, o, None)]
    return [(o, f, None), (o, r, None), (r, o, ro), (f, o, None)]


def corner_path(x0, y0, f, r, which, o_out, o_in=None):
    """Corner piece: the fill (o_in None) or a ring between two insets."""
    mx = which in ("topright", "bottomright")
    my = which in ("bottomleft", "bottomright")
    flip = mx != my

    def pt(px, py):
        return "%s,%s" % (n(x0 + (f - px if mx else px)), n(y0 + (f - py if my else py)))

    def seg(node, arc, reverse):
        if arc is None:
            return "L" + pt(node[0], node[1])
        sweep = 0 if reverse else 1
        if flip:
            sweep = 1 - sweep
        return "A%s,%s 0 0 %d %s" % (n(arc), n(arc), sweep, pt(node[0], node[1]))

    outer = contour(f, r, o_out)
    if o_in is None:
        d = ["M" + pt(f, f)] + [seg(nd, nd[2], False) for nd in outer]
        return " ".join(d + ["Z"])
    inner = contour(f, r, o_in)
    d = ["M" + pt(outer[0][0], outer[0][1])]
    d += [seg(nd, nd[2], False) for nd in outer[1:]]
    d.append("L" + pt(inner[-1][0], inner[-1][1]))
    for k in range(len(inner) - 1, 0, -1):
        d.append(seg(inner[k - 1], inner[k][2], True))
    return " ".join(d + ["Z"])


def round_rect_path(x, y, w, h, r):
    return (
        "M%s,%s H%s A%s,%s 0 0 1 %s,%s V%s A%s,%s 0 0 1 %s,%s H%s A%s,%s 0 0 1 %s,%s V%s A%s,%s 0 0 1 %s,%s Z"
        % (
            n(x + r), n(y), n(x + w - r), n(r), n(r), n(x + w), n(y + r),
            n(y + h - r), n(r), n(r), n(x + w - r), n(y + h),
            n(x + r), n(r), n(r), n(x), n(y + h - r),
            n(y + r), n(r), n(r), n(x + r), n(y),
        )
    )


def build(P, office=False):
    """The SVG for one palette. office=True draws the Write and Tables
    variant: the DialogKit board's 6 px corners on buttons, drop-downs and
    fields, and fields that mark focus with a 2 px accent line along their
    foot instead of an accent edge."""
    S = Sheet()
    RADIUS = 6 if office else 8
    INK, STRONG, ON_ACCENT = P["ink"], P["strong"], P["on_accent"]
    ACCENT, ACCENT_HOVER, ACCENT_PRESSED = P["accent"], P["accent_hover"], P["accent_pressed"]

    def W(a):
        return (P["tone"], a * P["tone_scale"])

    def frame_element(name, f, fill=None, rings=(), r=None, w=16, interior=True, foot=()):
        """A nine-slice element with corner radius r (defaults to f).

        rings is a list of (inset_out, inset_in, paint) drawn over the fill, so
        a 1px border at the edge is (0, 1, paint) exactly like a CSS border
        over a background. foot is the same, drawn along the bottom edge only.
        """
        r = f if r is None else r
        if f == 0:
            return
        x, y = S.place(2 * f + w, 2 * f + w)
        L, T = x, y
        C = x + f
        R = x + f + w
        B = y + f + w
        M = y + f

        for which, (cx, cy) in {
            "topleft": (L, T),
            "topright": (R, T),
            "bottomleft": (L, B),
            "bottomright": (R, B),
        }.items():
            body = bbox(cx, cy, f, f)
            if fill:
                body += '<path d="%s" %s/>' % (corner_path(cx, cy, f, r, which, 0), paint(fill))
            for o1, o2, p in rings:
                body += '<path d="%s" %s/>' % (corner_path(cx, cy, f, r, which, o1, o2), paint(p))
            S.group("%s-%s" % (name, which), body)

        def edge(side, ex, ey, ew, eh):
            body = bbox(ex, ey, ew, eh)
            if fill:
                body += rect(ex, ey, ew, eh, fill)
            for o1, o2, p in rings:
                t = o2 - o1
                if side == "top":
                    body += rect(ex, ey + o1, ew, t, p)
                elif side == "bottom":
                    body += rect(ex, ey + eh - o2, ew, t, p)
                elif side == "left":
                    body += rect(ex + o1, ey, t, eh, p)
                else:
                    body += rect(ex + ew - o2, ey, t, eh, p)
            if side == "bottom":
                for o1, o2, p in foot:
                    body += rect(ex, ey + eh - o2, ew, o2 - o1, p)
            S.group("%s-%s" % (name, side), body)

        edge("top", C, T, w, f)
        edge("bottom", C, B, w, f)
        edge("left", L, M, f, w)
        edge("right", R, M, f, w)
        if interior:
            S.group(name, bbox(C, M, w, w) + (rect(C, M, w, w, fill) if fill else ""))

    def border(p, width=1):
        return [(0, width, p)]

    # Buttons: the design's secondary button at rest, 0.09 on hover and 0.12
    # pressed. Checked buttons take the active chip (accent fill and edge) so
    # ink text keeps its contrast; Kvantum keeps a checked button "toggled"
    # even under the mouse.
    BUTTON = {
        "normal": (W(0.06), border(W(0.16))),
        "focused": (W(0.09), border(W(0.16))),
        "pressed": (W(0.12), border(W(0.16))),
        "toggled": ((ACCENT, 0.20), border((ACCENT, 0.55))),
        "disabled": (W(0.03), border(W(0.08))),
    }
    for state, (fill, rings) in BUTTON.items():
        frame_element("button-" + state, RADIUS, fill, rings)
        # Drop-downs have the slightly quieter 0.14 edge.
        frame_element("combo-" + state, RADIUS, fill, [(0, 1, W(0.14) if state != "disabled" else W(0.08))]
                      if state != "toggled" else rings)

    # The default (primary) button is solid accent with its label in the
    # text-on-accent colour. Hover lightens it (darkens in light mode) and
    # pressing darkens it; it never turns grey.
    for state, fill in {
        "normal": ACCENT,
        "focused": ACCENT_HOVER,
        "pressed": ACCENT_PRESSED,
        "toggled": ACCENT_PRESSED,
    }.items():
        frame_element("button-default-" + state, RADIUS, (fill, 1))
    # Stock Kvantum (and the Qt 5 build) only knows one default overlay laid
    # over the button and keeps the label in ink, so for it the default
    # button is the active chip, which reads with ink text.
    frame_element("button-default", RADIUS, (ACCENT, 0.24), border((ACCENT, 0.60)))
    # An empty indicator stops Kvantum drawing its corner glyph on that button.
    gx, gy = S.place(8, 8)
    S.group("button-default-indicator", bbox(gx, gy, 8, 8))

    # Keyboard focus: a 2px accent ring with a 1px gap of window colour inside
    # it, on the control's full outline, so it shows on a primary button too.
    # The design draws the ring 2px outside the control; Kvantum can only
    # paint inside the control's rectangle.
    FOCUS = [(0, 2, (ACCENT, 1)), (2, 3, (P["window"], 1))]
    for name in ("button", "combo", "tbutton", "tab"):
        frame_element(name + "-focus", 8, None, FOCUS, interior=False)
    frame_element("focus", 6, None, [(0, 2, (ACCENT, 1))], interior=False)

    # Flat tool buttons, used in toolbars.
    for state, fill in {
        "normal": None,
        "focused": W(0.09),
        "pressed": W(0.12),
        "toggled": W(0.09),
        "disabled": None,
    }.items():
        frame_element("tbutton-" + state, 8, fill)

    # Text fields: sunken fill, quiet edge; editing turns the edge accent. In
    # the office variant (DialogKit) a field is a shade lighter than the
    # dialog and editing draws a 2 px accent line along its foot.
    if office:
        # Solid, not see-through: LibreOffice paints a field's fill twice, for
        # the border window and again for the text area inside it, and two
        # see-through layers showed as a box within a box.
        def solid(a):
            w, t = P["window"].lstrip("#"), P["tone"].lstrip("#")
            k = a * P["tone_scale"]
            mix = [round(int(w[i:i + 2], 16) * (1 - k) + int(t[i:i + 2], 16) * k) for i in (0, 2, 4)]
            return ("#%02x%02x%02x" % tuple(mix), 1)

        fields = {
            "normal": (solid(0.05), border(W(0.10)), ()),
            "focused": (solid(0.05), border(W(0.10)), [(0, 2, (ACCENT, 1))]),
            "disabled": (solid(0.03), border(W(0.06)), ()),
        }
    else:
        fields = {
            "normal": (P["sunken"], border(W(0.10)), ()),
            "focused": (P["sunken"], border((ACCENT, 0.55)), ()),
            "disabled": ((P["sunken"][0], P["sunken"][1] * 0.6), border(W(0.06)), ()),
        }
    for state, (fill, rings, foot) in fields.items():
        frame_element("lineedit-" + state, RADIUS, fill, rings, foot=foot)

    # Tabs: the current tab lifted with surface-selected. In the office
    # variant (DialogKit) tabs are underline tabs: no fill, the current one
    # marked by a 2 px accent line at its foot, over a page with no frame.
    tab_foot = [(0, 2, (ACCENT, 1))] if office else ()
    for state, fill in {
        "normal": None,
        "focused": W(0.05),
        "pressed": W(0.08),
        "toggled": None if office else W(0.10),
        "disabled": None,
    }.items():
        frame_element("tab-" + state, 8, fill, foot=tab_foot if state == "toggled" else ())
        frame_element("floating-tab-" + state, 8, fill, foot=tab_foot if state == "toggled" else ())

    # Cards: tab pages and group boxes use surface-raised with border-subtle.
    if office:
        frame_element("tabframe-normal", 12, None)
    else:
        frame_element("tabframe-normal", 12, W(0.05), border(W(0.07)))
    frame_element("group-normal", 12, W(0.05), border(W(0.07)))

    # Generic frames around views and scroll areas: a quiet edge, no fill so
    # the view's own base colour shows.
    frame_element("common-normal", 4, None, border(W(0.09)), interior=False)
    frame_element("common-focused", 4, None, border(W(0.09)), interior=False)

    # Item views: hover is surface-hover; selection is a solid accent tile.
    # KDE views paint the selected label themselves in the colour scheme's
    # on-accent text, which is only readable on solid accent. Kvantum calls
    # the selection in a focused view "pressed" and in an unfocused one
    # "toggled".
    for state, fill in {
        "normal": None,
        "focused": W(0.07),
        "pressed": (ACCENT, 1.0),
        "toggled": (ACCENT, 0.75),
        "disabled": None,
    }.items():
        frame_element("itemview-" + state, 8, fill)

    # Header sections: flat with a divider underneath.
    for state, fill in {"normal": None, "focused": W(0.05), "pressed": W(0.07), "toggled": None}.items():
        x, y = S.place(36, 36)
        f = 2
        for side, (ex, ey, ew, eh) in {
            "topleft": (x, y, f, f),
            "top": (x + f, y, 32, f),
            "topright": (x + 34, y, f, f),
            "left": (x, y + f, f, 32),
            "right": (x + 34, y + f, f, 32),
            "bottomleft": (x, y + 34, f, f),
            "bottom": (x + f, y + 34, 32, f),
            "bottomright": (x + 34, y + 34, f, f),
        }.items():
            body = bbox(ex, ey, ew, eh)
            if fill:
                body += rect(ex, ey, ew, eh, fill)
            if side.startswith("bottom"):
                body += rect(ex, ey + eh - 1, ew, 1, W(0.08))
            S.group("header-%s-%s" % (state, side), body)
        S.group("header-%s" % state, bbox(x + f, y + f, 32, 32) + (rect(x + f, y + f, 32, 32, fill) if fill else ""))
    x, y = S.place(4, 20)
    S.group("header-separator", bbox(x, y, 4, 20) + rect(x + 1.5, y + 4, 1, 12, W(0.10)))

    # Menus: glass-menu with its 1px edge, 14px corners. "menu-normal" is the
    # popup without a compositor; "menu-shadow" is the same panel with a soft
    # shadow fading out over the popup's outer MENU_SHADOW pixels (Kvantum
    # gives popups no shadow of its own).
    frame_element("menu-normal", MENU_RADIUS, P["menu"], border(W(0.14)))
    D = MENU_SHADOW
    F = MENU_RADIUS + D
    steps = 6
    rings = []
    for i in range(steps):
        a = P["shadow_alpha"] * 0.5 * ((i + 1) / float(steps)) ** 2
        rings.append((D * i / float(steps), D * (i + 1) / float(steps), (P["shadow"], a)))
    rings.append((D, F, P["menu"]))
    rings.append((D, D + 1, W(0.14)))
    frame_element("menu-shadow", F, None, rings, interior=False)
    # Kvantum measures the shadow from these: their size over the piece
    # size, times the menu margin (padding plus shadow), comes to D.
    hint = D * F / float(MENU_PADDING + D)
    for side in ("left", "top", "right", "bottom"):
        hx, hy = S.place(int(hint) + 1, int(hint) + 1)
        S.group("menu-shadow-hint-%s" % side, bbox(hx, hy, hint, hint))
    frame_element("tooltip-normal", 6, P["menu"], border(W(0.14)))
    for state, fill in {"normal": None, "focused": W(0.09), "pressed": W(0.12), "toggled": W(0.09)}.items():
        frame_element("menuitem-" + state, 8, fill)
    for state, fill in {"normal": None, "focused": W(0.09), "pressed": W(0.12), "toggled": W(0.12)}.items():
        frame_element("menubaritem-" + state, 6, fill)

    # Toolbars, menu bars and status bars sit on the window with only a divider.
    for name in ("toolbar-normal", "menubar-normal"):
        x, y = S.place(36, 36)
        for side, (ex, ey, ew, eh) in {
            "top": (x + 1, y, 34, 0.001),
            "left": (x, y + 1, 0.001, 34),
            "right": (x + 35, y + 1, 0.001, 34),
            "bottom": (x + 1, y + 35, 34, 1),
            "bottomleft": (x, y + 35, 1, 1),
            "bottomright": (x + 35, y + 35, 1, 1),
        }.items():
            body = bbox(ex, ey, ew, eh)
            if side.startswith("bottom"):
                body += rect(ex, ey, ew, eh, W(0.07))
            S.group("%s-%s" % (name, side), body)
        S.group(name, bbox(x + 1, y + 1, 34, 34))

    def pill(name, thickness, fill, rings=()):
        frame_element(name, thickness // 2, fill, rings, w=8)

    # Progress: 8px track at surface-track strength, accent fill.
    pill("progress-normal", 8, W(0.12))
    pill("progress-pattern-normal", 8, (ACCENT, 1))
    pill("progress-pattern-disabled", 8, W(0.25))

    # Slider: the design's 8px track with a 1px edge, accent before the handle.
    pill("slider-normal", 8, W(0.12), [(0, 1, W(0.12))])
    pill("slider-toggled", 8, (ACCENT, 1))
    pill("slider-disabled", 8, W(0.06))
    pill("slider-toggled-disabled", 8, W(0.25))

    # Scroll bars: thin pills that only appear over the view on hover or scroll.
    pill("scrollbarslider-normal", 10, W(0.30))
    pill("scrollbarslider-focused", 10, W(0.45))
    pill("scrollbarslider-pressed", 10, W(0.45))
    pill("scrollbarslider-disabled", 10, W(0.12))
    pill("scrollbargroove-normal", 10, W(0.05))
    pill("scrollbargroove-focused", 10, W(0.07))
    pill("scrollbargroove-disabled", 10, None)

    def glyph(gid, size, body, extra=""):
        x, y = S.place(size, size)
        S.group(gid, bbox(x, y, size, size) + body(x, y) + extra)

    def stroke(d, colour, width=1.8, opacity=1.0, cap="round"):
        op = "" if opacity >= 1 else ' stroke-opacity="%s"' % n(opacity)
        return (
            '<path d="%s" fill="none" stroke="%s" stroke-width="%s"%s '
            'stroke-linecap="%s" stroke-linejoin="round"/>' % (d, colour, n(width), op, cap)
        )

    STATE_INK = {
        "normal": (INK, 1.0),
        "focused": (STRONG, 1.0),
        "pressed": (STRONG, 1.0),
        "toggled": (STRONG, 1.0),
        "disabled": (INK, 0.4),
    }

    # Chevrons in the design's line style: 24px grid scaled down, round caps.
    CHEVRONS = {
        "down": "M6,9 L12,15 L18,9",
        "up": "M6,15 L12,9 L18,15",
        "left": "M15,6 L9,12 L15,18",
        "right": "M9,6 L15,12 L9,18",
    }

    def chevron(dirn, colour, opacity, width=2.2):
        def body(x, y):
            out = []
            for tok in CHEVRONS[dirn].split(" "):
                px, py = tok[1:].split(",")
                out.append("%s%s,%s" % (tok[0], n(x + float(px) * 0.5), n(y + float(py) * 0.5)))
            return stroke(" ".join(out), colour, width * 0.5 * 1.25, opacity)

        return body

    for dirn in CHEVRONS:
        for state, (c, a) in STATE_INK.items():
            glyph("arrow-%s-%s" % (dirn, state), 12, chevron(dirn, c, a))
            glyph("flat-arrow-%s-%s" % (dirn, state), 12, chevron(dirn, c, a))
    for state, (c, a) in STATE_INK.items():
        glyph("carrow-%s" % state, 12, chevron("down", c, a))

    # Tree expanders: a right chevron when closed, down when open.
    for state, (c, a) in {"normal": (INK, 0.72), "focused": (INK, 1.0), "disabled": (INK, 0.35)}.items():
        glyph("tree-plus-%s" % state, 12, chevron("right", c, a))
        glyph("tree-minus-%s" % state, 12, chevron("down", c, a))

    # Spin box steppers drawn inline in the field as minus and plus.
    for state, (c, a) in STATE_INK.items():
        glyph("spin-plus-%s" % state, 12, lambda x, y, c=c, a=a: stroke(
            "M%s,%s L%s,%s M%s,%s L%s,%s" % (n(x + 6), n(y + 2), n(x + 6), n(y + 10), n(x + 2), n(y + 6), n(x + 10), n(y + 6)), c, 1.6, a))
        glyph("spin-minus-%s" % state, 12, lambda x, y, c=c, a=a: stroke(
            "M%s,%s L%s,%s" % (n(x + 2), n(y + 6), n(x + 10), n(y + 6)), c, 1.6, a))
        glyph("spin-up-%s" % state, 12, chevron("up", c, a))
        glyph("spin-down-%s" % state, 12, chevron("down", c, a))

    def box_body(fill, edge, mark=None, size=20, radius=6):
        def body(x, y):
            out = ""
            if fill:
                out += '<path d="%s" %s/>' % (round_rect_path(x, y, size, size, radius), paint(fill))
            if edge:
                c, a, wdt = edge
                h = wdt / 2.0
                out += (
                    '<path d="%s" fill="none" stroke="%s" stroke-opacity="%s" stroke-width="%s"/>'
                    % (round_rect_path(x + h, y + h, size - wdt, size - wdt, radius - h), c, n(a), n(wdt))
                )
            if mark:
                out += mark(x, y)
            return out

        return body

    # The design's tick: M5 12 l5 5 9-10 on a 24 grid, drawn 13px wide in
    # the middle of the box with a stroke of 3 grid units.
    def check_mark(colour, opacity=1.0, box=20):
        k = 13 / 24.0
        o = (box - 13) / 2.0

        def body(x, y):
            def pt(px, py):
                return "%s,%s" % (n(x + o + px * k), n(y + o + py * k))

            return stroke("M%s L%s L%s" % (pt(5, 12), pt(10, 17), pt(19, 7)), colour, 3 * k, opacity)

        return body

    def dash_mark(colour, opacity=1.0, box=20):
        return lambda x, y: stroke("M%s,%s L%s,%s" % (n(x + 6), n(y + box / 2), n(x + box - 6), n(y + box / 2)), colour, 1.8, opacity)

    # Check boxes: 20px, radius 6, the design's 1.5px border; checked fills
    # with accent and a text-on-accent tick.
    toff_c, toff_a = P["toggle_off"]
    CB_OFF = {
        "normal": (None, (toff_c, toff_a, 1.5)),
        "focused": (W(0.07), (toff_c, min(1, toff_a + 0.2), 1.5)),
        "pressed": (W(0.12), (toff_c, min(1, toff_a + 0.2), 1.5)),
        "disabled": (None, (toff_c, toff_a * 0.4, 1.5)),
    }
    CB_ON = {
        "normal": (ACCENT, 1.0),
        "focused": (ACCENT_HOVER, 1.0),
        "pressed": (ACCENT_PRESSED, 1.0),
        "disabled": (ACCENT, 0.4),
    }
    for state, (fill, edge) in CB_OFF.items():
        glyph("checkbox-%s" % state, 20, box_body(fill, edge))
    for state, (c, a) in CB_ON.items():
        glyph("checkbox-checked-%s" % state, 20, box_body((c, a), None, check_mark(ON_ACCENT, 1.0 if a >= 1 else 0.6)))
        glyph("checkbox-tristate-%s" % state, 20, box_body((c, a), None, dash_mark(ON_ACCENT, 1.0 if a >= 1 else 0.6)))

    def circle(cx, cy, r, p=None, edge=None):
        s = '<circle cx="%s" cy="%s" r="%s" ' % (n(cx), n(cy), n(r))
        s += paint(p) if p else 'fill="none"'
        if edge:
            c, a, wdt = edge
            s += ' stroke="%s" stroke-opacity="%s" stroke-width="%s"' % (c, n(a), n(wdt))
        return s + "/>"

    # Radio buttons: the design's 16px ring in the 20px box; checked is a
    # 5px accent ring around a dark centre.
    ring_c, ring_a = P["radio_ring"]
    for state, (fill, edge) in CB_OFF.items():
        ea = {"normal": ring_a, "disabled": ring_a * 0.4}.get(state, min(1, ring_a + 0.2))
        glyph("radio-%s" % state, 20, lambda x, y, fill=fill, ea=ea: circle(
            x + 10, y + 10, 8, fill) + circle(x + 10, y + 10, 7.25, None, (ring_c, ea, 1.5)))
    for state, (c, a) in CB_ON.items():
        glyph("radio-checked-%s" % state, 20, lambda x, y, c=c, a=a: circle(
            x + 10, y + 10, 8, (c, a)) + circle(x + 10, y + 10, 3, (P["radio_dot"], 1.0 if a >= 1 else 0.6)))

    # In menus only the mark shows, Windows style: no empty box for unchecked items.
    for state, (c, a) in {"normal": (INK, 1.0), "focused": (STRONG, 1.0), "disabled": (INK, 0.4)}.items():
        glyph("menu-checkbox-%s" % state, 20, lambda x, y: "")
        glyph("menu-checkbox-checked-%s" % state, 20, check_mark(c, a))
        glyph("menu-checkbox-tristate-%s" % state, 20, dash_mark(c, a))
        glyph("menu-radio-%s" % state, 20, lambda x, y: "")
        glyph("menu-radio-checked-%s" % state, 20, lambda x, y, c=c, a=a: circle(x + 10, y + 10, 3.5, (c, a)))

    # Submenu arrows in menus.
    for state, (c, a) in {"normal": (INK, 0.8), "focused": (STRONG, 1.0), "disabled": (INK, 0.35)}.items():
        glyph("menuitem-right-%s" % state, 12, chevron("right", c, a))
        glyph("menuitem-left-%s" % state, 12, chevron("left", c, a))

    # Slider handle: the design's 16px accent disc, lighter on hover.
    for state, fill in {"normal": ACCENT, "focused": ACCENT_HOVER, "pressed": ACCENT_PRESSED}.items():
        glyph("slidercursor-%s" % state, 16, lambda x, y, fill=fill: circle(x + 8, y + 8, 8, (fill, 1)))
    glyph("slidercursor-disabled", 16, lambda x, y: circle(x + 8, y + 8, 8, (INK, 0.4)))
    glyph("slider-tick-normal", 4, lambda x, y: rect(x + 1.5, y, 1, 4, W(0.25)))

    # Tab close buttons: the title bar's thin cross, with a hover chip.
    for state, (chip, c, a) in {
        "normal": (None, INK, 0.75),
        "focused": (W(0.12), STRONG, 1.0),
        "pressed": (W(0.18), STRONG, 1.0),
        "disabled": (None, INK, 0.35),
    }.items():
        glyph("tab-close-%s" % state, 16, lambda x, y, chip=chip, c=c, a=a: (
            ('<path d="%s" %s/>' % (round_rect_path(x, y, 16, 16, 5), paint(chip)) if chip else "")
            + stroke("M%s,%s L%s,%s M%s,%s L%s,%s" % (
                n(x + 5), n(y + 5), n(x + 11), n(y + 11), n(x + 11), n(y + 5), n(x + 5), n(y + 11)), c, 1.4, a)))

    # Splitters: a 1px border-subtle line in a wider grab area.
    for state, a in {"normal": 0.07, "focused": 0.20, "pressed": 0.30}.items():
        x, y = S.place(5, 30)
        S.group("splitter-%s" % state, bbox(x, y, 5, 30) + rect(x + 2, y, 1, 30, W(a)))
    for state in ("normal", "focused", "pressed"):
        glyph("splitter-grip-%s" % state, 4, lambda x, y: "")

    # Toolbar handles and size grips stay out of the way.
    glyph("toolbar-handle", 6, lambda x, y: "")
    glyph("toolbar-separator", 6, lambda x, y: rect(x + 2.5, y, 1, 6, W(0.12)))
    glyph("resize-grip-normal", 12, lambda x, y: "")

    # MDI title bar buttons share the window title bar glyphs.
    MDI = {
        "minimize": lambda x, y, c, a: stroke("M%s,%s H%s" % (n(x + 1.5), n(y + 6), n(x + 10.5)), c, 1.3, a, "butt"),
        "maximize": lambda x, y, c, a: '<rect x="%s" y="%s" width="9" height="9" rx="1.8" fill="none" stroke="%s" stroke-opacity="%s" stroke-width="1.2"/>' % (n(x + 1.5), n(y + 1.5), c, n(a)),
        "restore": lambda x, y, c, a: '<rect x="%s" y="%s" width="7" height="7" rx="1.5" fill="none" stroke="%s" stroke-opacity="%s" stroke-width="1.2"/>' % (n(x + 1.5), n(y + 3.5), c, n(a)) + stroke("M%s,%s V%s A1.5,1.5 0 0 1 %s,%s H%s A1.5,1.5 0 0 1 %s,%s V%s" % (n(x + 3.5), n(y + 3.5), n(y + 3), n(x + 5), n(y + 1.5), n(x + 9), n(x + 10.5), n(y + 3), n(y + 7)), c, 1.2, a),
        "close": lambda x, y, c, a: stroke("M%s,%s L%s,%s M%s,%s L%s,%s" % (n(x + 2), n(y + 2), n(x + 10), n(y + 10), n(x + 10), n(y + 2), n(x + 2), n(y + 10)), c, 1.3, a),
        "shade": lambda x, y, c, a: stroke("M%s,%s H%s" % (n(x + 1.5), n(y + 3), n(x + 10.5)), c, 1.3, a, "butt"),
    }
    for kind, fn in MDI.items():
        for state, (c, a) in STATE_INK.items():
            if state == "toggled":
                continue
            glyph("mdi-%s-%s" % (kind, state), 12, lambda x, y, fn=fn, c=c, a=a: fn(x, y, c, a))
    glyph("mdi-menu-normal", 12, lambda x, y: "")
    for state in ("normal", "focused"):
        x, y = S.place(20, 20)
        S.group("titlebar-%s" % state, bbox(x, y, 20, 20) + rect(x, y, 20, 20, (P["menu"][0], 1)))

    # Dock widgets have no chrome of their own.
    for state in ("normal", "focused"):
        x, y = S.place(20, 20)
        S.group("dock-%s" % state, bbox(x, y, 20, 20))
    x, y = S.place(20, 20)
    S.group("window-normal", bbox(x, y, 20, 20))
    return S.svg()


def light_kvconfig(text):
    def swap(m):
        return LIGHT_KVCONFIG.get(m.group(0).lower(), m.group(0))

    text = re.sub(r"#[0-9a-fA-F]{8}\b|#[0-9a-fA-F]{6}\b", swap, text)
    text = text.replace("comment=Wild OS controls: dark glass", "comment=Wild OS controls: light glass")
    text = text.replace("dark_titlebar=true", "dark_titlebar=false")
    return text


def accent_line(P):
    return "accent_colors=%s,%s,%s" % (P["accent"], P["accent_hover"], P["accent_pressed"])


# The office variant: the Write and Tables ribbon on the boards uses 28 px
# tool buttons and drop-downs, denser than the rest of Wild OS. LibreOffice
# sizes every toolbar row to the taller of the two, so both shrink, and only
# for LibreOffice ([Applications] in kvantum.kvconfig).
OFFICE = {
    # LibreOffice draws its own scroll bars, which stay on screen: thinner
    # ones keep the page and the sheet as quiet as the boards. DialogKit's
    # checkboxes and radio buttons are 18 px.
    "%General": {"scroll_width": "8", "check_size": "18"},
    # DialogKit: dialog buttons are 32 px tall. Fields get their 32 px in
    # LibreOffice itself: it reads a taller line edit as a thicker frame and
    # adds it again around the text. LineEdit inherits PanelButtonCommand,
    # so it takes its minimum back off.
    "PanelButtonCommand": {"min_height": "32"},
    "LineEdit": {"min_height": "0", "frame.top": "3", "frame.bottom": "3", "frame.left": "6",
                 "frame.right": "6", "text.margin.top": "0", "text.margin.bottom": "0"},
    "ToolbarButton": {"frame.top": "6", "frame.bottom": "6", "frame.left": "6", "frame.right": "6",
                      "text.margin.top": "0", "text.margin.bottom": "0",
                      "min_width": "28", "min_height": "28"},
    # LibreOffice's ribbon buttons reach Kvantum as tool buttons outside a
    # toolbar, which it draws as panel buttons: a checked one (the current
    # alignment) took the accent chip. The boards keep them flat.
    "PanelButtonTool": {"frame.element": "tbutton", "interior.element": "tbutton"},
    "ComboBox": {"frame.top": "5", "frame.bottom": "5",
                 "text.margin.top": "0", "text.margin.bottom": "0", "min_height": "28"},
}


def office_kvconfig(text):
    for section, keys in OFFICE.items():
        m = re.search(r"(?ms)^\[" + re.escape(section) + r"\]\n(.*?)(?=^\[|\Z)", text)
        body = m.group(1)
        for key, value in keys.items():
            line = re.compile(r"(?m)^" + re.escape(key) + r"=.*$")
            if line.search(body):
                body = line.sub(key + "=" + value, body)
            else:
                body = body.rstrip("\n") + "\n" + key + "=" + value + "\n\n"
        text = text[:m.start(1)] + body + text[m.end(1):]
    return text.replace("comment=Wild OS controls:", "comment=Wild OS office controls:")


def write(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w", encoding="utf-8", newline="\n") as fh:
        fh.write(text)
    print("wrote", path)


def main():
    dark_cfg = os.path.join(HERE, "WildOS", "WildOS.kvconfig")
    with open(dark_cfg, encoding="utf-8") as fh:
        cfg = fh.read()
    # Keep the accent the patched Kvantum swaps in step with the drawing.
    cfg = re.sub(r"(?m)^accent_colors=.*$", accent_line(DARK), cfg)
    write(dark_cfg, cfg)
    write(os.path.join(HERE, "WildOS", "WildOS.svg"), build(DARK))
    light = light_kvconfig(cfg)
    light = re.sub(r"(?m)^accent_colors=.*$", accent_line(LIGHT), light)
    write(os.path.join(HERE, "WildOSLight", "WildOSLight.kvconfig"), light)
    write(os.path.join(HERE, "WildOSLight", "WildOSLight.svg"), build(LIGHT))
    write(os.path.join(HERE, "WildOSOffice", "WildOSOffice.kvconfig"), office_kvconfig(cfg))
    write(os.path.join(HERE, "WildOSOffice", "WildOSOffice.svg"), build(DARK, office=True))
    write(os.path.join(HERE, "WildOSOfficeLight", "WildOSOfficeLight.kvconfig"), office_kvconfig(light))
    write(os.path.join(HERE, "WildOSOfficeLight", "WildOSOfficeLight.svg"), build(LIGHT, office=True))


if __name__ == "__main__":
    main()
