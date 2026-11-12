#!/usr/bin/env python3
"""Builds a Plasma style for each theme's glass from the Wild OS one.

    python3 tools/theme-variants.py files/desktoptheme files/themes <desktoptheme dir>

Start, the flyouts, tooltips and every Plasma popup are drawn from the
frames in files/desktoptheme, whose glass is Dawn's. A theme with glass of its
own names a Plasma style in its "desktopTheme" (wildos-overclock, ...); this
writes that style as a copy of the Wild OS one with the glass recoloured, the
same way WildOS.UI's Style.tinted recolours the app surfaces: each channel
scaled by the theme's share of the reference glass, the alpha moved by the
difference. "wildos-light" is the light glass of the light artboards.

Every style also gets a "-solid" twin for Settings > Accessibility > Stronger
text on glass, its glass most of the way to opaque by the same rule as
Style.solid, so text in Start, the flyouts and tooltips no longer sits over
whatever is behind them.

wildos-auto-theme switches the style to match the theme, the mode and that
setting.
"""

import json
import re
import shutil
import sys
from pathlib import Path

DARK_REFERENCE = ((24, 26, 34), 0.95)
LIGHT_REFERENCE = ((243, 243, 243), 0.96)

# The fills the frames are painted with (see glass-svg.py and
# desktoptheme-dialogs.py).
PANEL = (35, 37, 41)      # Start, flyouts, tooltips: glass-panel, Mica 0.96
BAR = (24, 26, 34)        # the floating taskbar: glass-bar, Mica 0.92
SHADOW = (10, 12, 16)       # the flyout shadow, at many alphas
WHITE = (255, 255, 255)   # the panels' 0.093 edge and the taskbar's 0.12 edge

# Style.solid in WildOS.UI: the glass goes this far towards opaque.
SOLID = 0.8

FILL = re.compile(r'fill="rgb\((\d+),(\d+),(\d+)\)" fill-opacity="([0-9.]+)"')


def hex_colour(value):
    """{"colour": "#rrggbb", "alpha": a} as ((r, g, b), a)."""
    h = value["colour"].lstrip("#")
    return tuple(int(h[i:i + 2], 16) for i in (0, 2, 4)), float(value.get("alpha", 1.0))


def tint(rgb, alpha, glass, reference, shift_alpha=True):
    (tr, tg, tb), ta = glass
    (rr, rg, rb), ra = reference
    r, g, b = rgb
    out = (min(255, round(r * tr / rr)), min(255, round(g * tg / rg)), min(255, round(b * tb / rb)))
    if shift_alpha:
        alpha = max(0.0, min(1.0, alpha + ta - ra))
    return out, alpha


def fmt(rgb, alpha):
    a = ("%.3f" % alpha).rstrip("0").rstrip(".") or "0"
    return 'fill="rgb(%d,%d,%d)" fill-opacity="%s"' % (rgb + (a,))


def recolour(svg, light, glass, taskbar, panel=None, solid=False):
    reference = LIGHT_REFERENCE if light else DARK_REFERENCE

    def glass_fill(rgb, alpha):
        if solid:
            alpha = alpha + (1.0 - alpha) * SOLID
        return fmt(rgb, alpha)

    def swap(match):
        rgb = tuple(int(match.group(i)) for i in (1, 2, 3))
        alpha = float(match.group(4))
        if rgb == PANEL:
            # As Style.glassPanel: a theme's own panel glass as it is given,
            # otherwise Dawn's tinted by the theme's window glass.
            if panel:
                return glass_fill(*panel)
            if light:
                rgb, alpha = (240, 240, 240), 0.96
            return glass_fill(*tint(rgb, alpha, glass, reference))
        if rgb == BAR:
            if taskbar:
                # As Style.taskbarFloating: the theme's taskbar glass, as
                # much lighter as the stock pills are than the docked bar.
                return glass_fill(taskbar[0], max(0.0, taskbar[1] - (0.04 if light else 0.02)))
            if light:
                rgb, alpha = (243, 243, 243), 0.94
            return glass_fill(*tint(rgb, alpha, glass, reference))
        if rgb == SHADOW:
            if light:
                # Shadows keep their shape at about 40% strength in light
                # mode (Style.shadowFlyout).
                return fmt((36, 38, 42), alpha * 0.4)
            return fmt(*tint(rgb, alpha, glass, reference, shift_alpha=False))
        if rgb == WHITE and light:
            # The edge becomes the dark hairline (Style.tone), the top
            # highlight stays white and grows to show on pale glass.
            if abs(alpha - 0.12) < 1e-6:
                return fmt((46, 48, 52), 0.09)
            # The panels' Mica edge is black at 7.3% (Style.borderGlass).
            if abs(alpha - 0.093) < 1e-6:
                return fmt((0, 0, 0), 0.073)
            if abs(alpha - 0.1) < 1e-6:
                return fmt(WHITE, 0.7)
        return match.group(0)

    return FILL.sub(swap, svg)


def build(source, dest, name, title, light, glass, taskbar, panel=None, solid=False):
    target = dest / name
    if target.exists():
        shutil.rmtree(target)
    shutil.copytree(source, target)
    for svg in target.rglob("*.svg"):
        # The task manager's frames are Plasma's own, which Wild OS does not
        # show; they keep their colours.
        if svg.name == "tasks.svg":
            continue
        svg.write_text(recolour(svg.read_text(), light, glass, taskbar, panel, solid))
    metadata = json.loads((source / "metadata.json").read_text())
    metadata["KPlugin"]["Id"] = name
    metadata["KPlugin"]["Name"] = title
    metadata["KPlugin"]["Description"] = ("Light" if light else "Dark") + (" solid" if solid else " glass") + " panels and popups"
    (target / "metadata.json").write_text(json.dumps(metadata, indent=4) + "\n")
    print(target)


def main():
    source, themes, dest = (Path(p) for p in sys.argv[1:4])
    dest.mkdir(parents=True, exist_ok=True)
    styles = [
        ("wildos", "Wild OS", False, DARK_REFERENCE, None, None),
        ("wildos-light", "Wild OS Light", True, LIGHT_REFERENCE, None, None),
    ]
    named = {"wildos", "wildos-light"}
    for path in sorted(themes.glob("*.json")):
        theme = json.loads(path.read_text())
        name = theme.get("desktopTheme", "wildos")
        if name in named:
            continue
        light = theme["mode"] == "light"
        glass = hex_colour(theme["glass"])
        taskbar = hex_colour(theme["taskbarGlass"]) if "taskbarGlass" in theme else None
        panel = hex_colour(theme["panelGlass"]) if "panelGlass" in theme else None
        styles.append((name, "Wild OS " + theme["name"], light, glass, taskbar, panel))
        named.add(name)

    for name, title, light, glass, taskbar, panel in styles:
        # The plain dark style is the source itself, installed as it is.
        if name != "wildos":
            build(source, dest, name, title, light, glass, taskbar, panel)
        build(source, dest, name + "-solid", title + " Solid", light, glass, taskbar, panel, solid=True)


if __name__ == "__main__":
    main()
