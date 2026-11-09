#!/usr/bin/env python3
"""Render the Wild OS start-up animation and disk unlock artwork.

Plymouth cannot blur, draw soft gradients or render text reliably inside the
initramfs, so every frame of the boot screen is drawn here, from the logo
masters in branding/logo, the gradients in design/BootIntro.dc.html and the
Inter typeface, and shipped as PNGs. The sign-in screen and the session splash
load the same files to open on Plymouth's last frame, so all three draw from
one set of pixels.

Needs python3, librsvg2-bin and fonts-inter. The package build runs it:

    python3 tools/render-boot-assets.py DEST HANDOFF

DEST (the Plymouth theme) receives one folder per screen scale (s080 ...
s240); HANDOFF receives the same folders with the frames only the sign-in
screen plays (the final trace), kept out of the initramfs. Every number below
is in the artboards' 1440x900 CSS pixels, measured from the centre of the
screen, and every canvas edge is a multiple of 5 so it lands on a whole device
pixel at every scale; wildos.script and BootHandoff.qml use the same canvases.

The W appears in two places: in the start-up animation's 240px logo box
(BootIntro.dc.html) and, after the disk is unlocked, smaller and lower, where
it lands after gliding out of the unlock screen's tile (AfterUnlockD.dc.html).
Assets for the first end in -i, for the second in -u.
"""

import concurrent.futures
import math
import os
import re
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
PKG = os.path.dirname(HERE)
REPO = os.path.dirname(os.path.dirname(PKG))
# The package build copies the repo's branding folder into the package.
LOGOS = next(p for p in (os.path.join(PKG, "branding", "logo"), os.path.join(REPO, "branding", "logo"))
             if os.path.isdir(p))
MARK = os.path.join(LOGOS, "wild-os-mark.svg")
TILE = os.path.join(LOGOS, "wild-os-icon.svg")

SCALES = (0.8, 1.0, 1.2, 1.6, 2.0, 2.4)

# The splash clock t against BootIntro's design time d: the spark and the W
# drawing itself (design 0.7 s to 3.1 s) play on a fixed 1.5 s, everything
# after at the design's own speed.
INTRO = 1.5


def design_time(t):
    return 0.7 + 1.6 * t if t <= INTRO else t + 1.6


def splash_time(d):
    return (d - 0.7) / 1.6 if d <= 3.1 else d - 1.6


def bezier(u, x1=0.4, y1=0.0, x2=0.2, y2=1.0):
    """CSS cubic-bezier(.4,0,.2,1), the design's timing for every keyframe."""
    if u <= 0:
        return 0.0
    if u >= 1:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(30):
        mid = (lo + hi) / 2
        x = 3 * (1 - mid) ** 2 * mid * x1 + 3 * (1 - mid) * mid ** 2 * x2 + mid ** 3
        if x < u:
            lo = mid
        else:
            hi = mid
    s = (lo + hi) / 2
    return 3 * (1 - s) ** 2 * s * y1 + 3 * (1 - s) * s ** 2 * y2 + s ** 3


def segment(d, d0, d1):
    return bezier((d - d0) / (d1 - d0))


def stops(spec, steps=12):
    """SVG stops for a CSS gradient. CSS blends colour stops with premultiplied
    alpha, SVG without, which darkens a colour fading to transparent; extra
    stops computed the CSS way make the two agree."""
    out = []
    for (o0, c0), (o1, c1) in zip(spec, spec[1:]):
        for i in range(steps):
            f = i / steps
            a = c0[3] + (c1[3] - c0[3]) * f
            rgb = [(c0[j] * c0[3] + (c1[j] * c1[3] - c0[j] * c0[3]) * f) / a if a > 0 else c1[j]
                   for j in range(3)]
            out.append((o0 + (o1 - o0) * f, rgb, a))
    o, c = spec[-1]
    out.append((o, c[:3], c[3]))
    return "".join(f'<stop offset="{o:.4f}" stop-color="rgb({r:.1f},{g:.1f},{b:.1f})" '
                   f'stop-opacity="{a:.4f}"/>' for o, (r, g, b), a in out)


# Inter's ascender and line gap give CSS "normal" line height of 1.2109em,
# with the baseline 0.96875em below the top of the line box.
LINE = 1.2109375
ASC = 0.96875

INK = "#ffffff"
LABEL = "rgba(232,236,248,0.85)"
MUTED = "rgba(232,236,248,0.72)"
ACCENT = "#8fb4ff"
ON_ACCENT = "#0f1530"

# The bare mark's paths and gradient, read from the master so a logo change
# is a one-file edit.
with open(MARK, encoding="utf-8") as f:
    _mark = f.read()
W_PATH, SPARK_PATH = re.findall(r'<path d="([^"]+)"', _mark)[:2]
STOPS = re.search(r'<linearGradient id="ka".*?</linearGradient>', _mark, re.S).group(0)
STOPS = "".join(re.findall(r"<stop [^>]*>(?:</stop>)?", STOPS))
GRADIENT = ('<linearGradient id="bia" gradientUnits="userSpaceOnUse" x1="0" y1="17" '
            f'x2="0" y2="113">{STOPS}</linearGradient>')

# The 240px logo box (artboard 600,230) and the spark's centre inside the mark
# (148.5,40 in the mark's 150 unit viewBox).
BOX_X, BOX_Y = -120, -220
SPARK_CX, SPARK_CY = 148.5, 40


def logo(body):
    """Place mark-space content in the 240px logo box."""
    return (f'<svg x="{BOX_X}" y="{BOX_Y}" width="240" height="240" viewBox="18 -10.5 150 150" '
            f'overflow="visible"><defs>{GRADIENT}</defs>{body}</svg>')


def at(pose, body, opacity=1.0):
    """Mark-space content at a pose (scale, dx, dy)."""
    s, dx, dy = pose
    op = "" if opacity >= 1 else f' opacity="{opacity:.4f}"'
    return f'<g transform="translate({dx:.4f} {dy:.4f}) scale({s:.6f})"{op}>{body}</g>'


# The light's gradient across the mark and its two-step glow, as the design's
# chained drop-shadows: a tight bright one, then a wide soft one around both.
LIGHT_DEFS = ('<linearGradient id="lt" gradientUnits="userSpaceOnUse" x1="20" y1="0" x2="165" y2="0">'
              '<stop offset="0" stop-color="#cfdcff"/><stop offset="0.5" stop-color="#ffffff"/>'
              '<stop offset="1" stop-color="#ffd2c8"/></linearGradient>')


def glow(name, near, far, far_colour="#8fb4ff", far_opacity=0.6):
    out = (f'<filter id="{name}" filterUnits="userSpaceOnUse" x="-20" y="-40" width="230" height="200" '
           f'color-interpolation-filters="sRGB">'
           f'<feDropShadow dx="0" dy="0" stdDeviation="{near}" flood-color="#b9cfff" flood-opacity="0.95" result="a"/>')
    if far:
        out += (f'<feDropShadow in="a" dx="0" dy="0" stdDeviation="{far}" flood-color="{far_colour}" '
                f'flood-opacity="{far_opacity}"/>')
    return out + '</filter>'


def light_path(width, dasharray, offset, filt):
    return (f'<path d="{W_PATH}" fill="none" stroke="url(#lt)" stroke-width="{width}" '
            f'stroke-linecap="round" stroke-linejoin="round" stroke-dasharray="{dasharray}" '
            f'stroke-dashoffset="{offset:.3f}" filter="url(#{filt})"/>')


# Canvases: (x, y, width, height) from the screen centre.
W_CANVAS = (-115, -180, 185, 160)
SPARK_CANVAS = (35, -195, 110, 110)
FLASH_SIZE = 200
WORD_CANVAS = (-150, 45, 300, 45)

# Where the mark's own units land on screen: x' = scale * x + dx. POSE_I is the
# 240px logo box at artboard 600,230 (viewBox 18 -10.5 150 150); POSE_U the
# W after unlocking; TILE the W inside the unlock screen's 88px tile, where
# the glide starts (the icon's own layout, 0.4933 of a 100 unit tile).
POSE_I = (1.6, -120 - 18 * 1.6, -220 + 10.5 * 1.6)
POSE_U = (1.1287, 615.22 - 720, 367.04 - 450)
TILE_POSE = (0.4933333 * 0.88, -44 + 4.12 * 0.88, -200 + 18.18 * 0.88)

# The glass W, its running light and the final trace, with room for the glow.
GLASS = {"i": (-135, -200, 225, 200), "u": (-100, -85, 190, 150)}
SPARK_U_CANVAS = (40, -75, 75, 75)
# The stroke widths and glows of each artboard, in the mark's units: CSS
# drop-shadow(0 0 3px) is a Gaussian with a 1.5 unit deviation.
LIGHT = {"i": (1.3, 1.5, 5.0), "u": (1.8, 1.5, 4.5)}
BASE_WIDTH = {"i": 0.9, "u": 1.2}
# One lap of the light: 1.75 s in the start-up animation, 1.6 s after
# unlocking, at 50 frames a second.
LAP_FRAMES = {"i": 88, "u": 80}
DASH = 64
COMPLETE_FRAMES = 46

# "Starting securely" where it comes to rest: in the start-up animation it
# rises 62px from under the wordmark (BootIntro's column: the 240px box, 30px
# gaps, Inter 30px and 14px lines); after unlocking it sits at artboard 548.
STATUS_TOP = {"i": 50 + 30 * 1.2109375 + 30 - 62, "u": 548 - 450}
STATUS_CANVAS = {"i": (-100, 50, 200, 25), "u": (-100, 95, 200, 25)}

# The glide from the tile to the glass W: 1 s at 50 frames a second. The
# canvas spans where the W ends up (78px either side of centre), or its left
# side was cut off until the finished W took over.
GLIDE_CANVAS = (-85, -205, 170, 255)
GLIDE_FRAMES = 50

TILE_CANVAS = (-45, -200, 90, 90)
NAME_CANVAS = (-100, -95, 200, 30)
PROMPT_CANVAS = (-200, -35, 400, 25)
FIELD_CANVAS = (-160, 0, 320, 50)
BULLET_CANVAS = (0, 0, 10, 15)
HINT_CANVAS = (-200, 70, 400, 20)
CAPS_CANVAS = (-200, 95, 400, 20)

SPARK_FRAMES = 26
STROKE_FRAMES = 54
WORD_FRAMES = 41
STROKE_DASH = 441


def svg_doc(canvas, scale, body, defs=""):
    x, y, w, h = canvas
    return (f'<svg xmlns="http://www.w3.org/2000/svg" width="{round(w * scale)}" '
            f'height="{round(h * scale)}" viewBox="{x} {y} {w} {h}"><defs>{defs}</defs>{body}</svg>')


def rsvg(svg, path):
    with tempfile.NamedTemporaryFile("w", suffix=".svg", delete=False, encoding="utf-8") as f:
        f.write(svg)
        src = f.name
    try:
        subprocess.run(["rsvg-convert", "-o", path, src], check=True)
    finally:
        os.unlink(src)


def text_el(text, size, weight, fill, top, tracking=0.0, anchor="middle", x=0.0):
    """Text laid out like a CSS block: a line box from `top`, centred on x.

    rsvg centres glyphs plus the spacing between them, CSS also counts the
    spacing after the last glyph, so the run sits half a gap to the left.
    """
    spacing = tracking * size
    base = top + size * ASC + (round(size * LINE) - size * LINE) / 2
    if anchor == "middle":
        x -= spacing / 2
    attr = f' letter-spacing="{spacing:.3f}"' if spacing else ""
    return (f'<text x="{x:.3f}" y="{base:.3f}" font-family="Inter" font-size="{size}" '
            f'font-weight="{weight}" fill="{fill}" text-anchor="{anchor}"{attr}>{text}</text>')


def spark_state(d):
    if d < 0.7:
        return 0.0, 0.0, -45.0
    if d < 1.2:
        e = segment(d, 0.7, 1.2)
        return e, 1.35 * e, -45.0 * (1 - e)
    if d < 1.5:
        return 1.0, 1.35 - 0.35 * segment(d, 1.2, 1.5), 0.0
    return 1.0, 1.0, 0.0


def spark_svg(opacity, scale_, rotate):
    t = (f"translate({SPARK_CX} {SPARK_CY}) rotate({rotate:.4f}) scale({scale_:.5f}) "
         f"translate({-SPARK_CX} {-SPARK_CY})")
    return logo(f'<path d="{SPARK_PATH}" fill="url(#bia)" opacity="{opacity:.4f}" transform="{t}"/>')


def jobs_for_scale(scale, dest):
    out = os.path.join(dest, f"s{round(scale * 100):03d}")
    os.makedirs(out, exist_ok=True)
    jobs = []

    def add(name, canvas, body, defs=""):
        jobs.append((svg_doc(canvas, scale, body, defs), os.path.join(out, name + ".png")))

    add("fill", W_CANVAS, logo(f'<path d="{W_PATH}" fill="url(#bia)"/>'))

    # The W's outline traced from its first corner, as the design's
    # stroke-dashoffset animation draws it. The first frame is empty.
    t0, t1 = splash_time(1.4), splash_time(3.1)
    for k in range(STROKE_FRAMES):
        d = design_time(t0 + (t1 - t0) * k / (STROKE_FRAMES - 1))
        offset = STROKE_DASH * (1 - segment(d, 1.4, 3.1))
        add(f"stroke-{k:02d}", W_CANVAS, logo(
            f'<path d="{W_PATH}" fill="none" stroke="#c9d8ff" stroke-width="1.6" '
            f'stroke-linejoin="round" stroke-dasharray="{STROKE_DASH}" '
            f'stroke-dashoffset="{offset:.3f}"/>'))

    for k in range(SPARK_FRAMES):
        d = design_time(splash_time(0.7) + (splash_time(1.5) - splash_time(0.7)) * k / (SPARK_FRAMES - 1))
        add(f"spark-{k:02d}", SPARK_CANVAS, spark_svg(*spark_state(d)))

    # One flash at its largest; the script shrinks it per frame, which is
    # harmless for a glow with no edges.
    half = FLASH_SIZE / 2
    add("flash", (-half, -half, FLASH_SIZE, FLASH_SIZE),
        '<circle cx="0" cy="0" r="99" fill="url(#fl)"/>',
        '<radialGradient id="fl" cx="0" cy="0" r="99" gradientUnits="userSpaceOnUse">'
        + stops([(0, (255, 255, 255, 0.9)), (0.45, (185, 207, 255, 0.35)), (1, (185, 207, 255, 0))])
        + '</radialGradient>')

    # "Wild OS" settling from wide to its final tracking as it fades in.
    t0, t1 = splash_time(3.6), splash_time(4.4)
    for k in range(WORD_FRAMES):
        e = segment(design_time(t0 + (t1 - t0) * k / (WORD_FRAMES - 1)), 3.6, 4.4)
        tracking = 0.3 + (0.02 - 0.3) * e
        add(f"word-{k:02d}", WORD_CANVAS,
            f'<g opacity="{e:.4f}">{text_el("Wild OS", 30, 500, INK, 50, tracking)}</g>')

    # The glass W and its running light, for each place the W appears.
    for g, pose in (("i", POSE_I), ("u", POSE_U)):
        canvas = GLASS[g]
        width, near, far = LIGHT[g]
        add(f"fill-{g}", canvas, at(pose, f'<path d="{W_PATH}" fill="url(#bia)"/>'), GRADIENT)
        add(f"base-{g}", canvas, at(pose,
            f'<path d="{W_PATH}" fill="none" stroke="rgb(201,216,255)" stroke-opacity="0.22" '
            f'stroke-width="{BASE_WIDTH[g]}" stroke-linejoin="round"/>'))
        # One lap: the 64 unit dash moves round the whole outline.
        n = LAP_FRAMES[g]
        for k in range(n):
            offset = STROKE_DASH * (1 - k / n)
            add(f"light-{g}-{k:02d}", canvas,
                at(pose, light_path(width, f"{DASH} {STROKE_DASH - DASH}", offset, "lg")),
                LIGHT_DEFS + glow("lg", near, far))
        add(f"status-{g}", STATUS_CANVAS[g], text_el("Starting securely", 14, 400, LABEL, STATUS_TOP[g]))
    add("spark-u", SPARK_U_CANVAS, at(POSE_U, f'<path d="{SPARK_PATH}" fill="url(#bia)"/>'), GRADIENT)

    # The glide out of the unlock screen's tile (AfterUnlockD, 1.6 s to
    # 2.6 s): the tile's ground fades, the W dims to glass as its outline
    # comes up, and the whole mark, spark included, moves to its place.
    tile_bg = ('<rect x="-4.3" y="-32.8" width="194.6" height="194.6" rx="46.6" fill="#121a33" '
               'opacity="{op:.4f}"/>')
    for k in range(GLIDE_FRAMES):
        d = 1.6 + k / (GLIDE_FRAMES - 1)
        e = bezier((d - 1.6) / 1.0, 0.5, 0.0, 0.15, 1.0)
        pose = tuple(a + (b - a) * e for a, b in zip(TILE_POSE, POSE_U))
        fill_op = 1 - 0.9 * segment(d, 1.6, 2.4)
        base_op = segment(d, 2.0, 2.6)
        body = (tile_bg.format(op=1 - segment(d, 1.6, 2.2))
                + f'<path d="{W_PATH}" fill="url(#bia)" opacity="{fill_op:.4f}"/>'
                + f'<path d="{W_PATH}" fill="none" stroke="rgb(201,216,255)" stroke-opacity="{0.22 * base_op:.4f}" '
                  f'stroke-width="{BASE_WIDTH["u"]}" stroke-linejoin="round"/>'
                + f'<path d="{SPARK_PATH}" fill="url(#bia)"/>')
        add(f"glide-{k:02d}", GLIDE_CANVAS, at(pose, body), GRADIENT)

    # The disk unlock column from Boot.dc.html, its top at artboard 250.
    with open(TILE, encoding="utf-8") as f:
        tile = f.read()
    tile = re.sub(r'<svg xmlns="http://www.w3.org/2000/svg" width="\d+" height="\d+"',
                  '<svg x="-44" y="-200" width="88" height="88"', tile, count=1)
    add("tile", TILE_CANVAS, tile)
    add("name", NAME_CANVAS, text_el("Wild OS", 22, 600, "#eef1f8", -94, -0.02))
    add("prompt", PROMPT_CANVAS, text_el("Enter your disk password to start", 14, 400, LABEL, -31))
    add("prompt-retry", PROMPT_CANVAS,
        text_el("That password did not work. Try again.", 14, 400, LABEL, -31))
    add("field", FIELD_CANVAS,
        '<rect x="-159.5" y="4.5" width="319" height="45" rx="22.5" '
        'fill="#ffffff" fill-opacity="0.07" stroke="#ffffff" stroke-opacity="0.18"/>'
        f'<circle cx="136" cy="27" r="18" fill="{ACCENT}"/>'
        '<g transform="translate(128 19) scale(0.6667)" fill="none" '
        f'stroke="{ON_ACCENT}" stroke-width="2.4" stroke-linecap="round" stroke-linejoin="round">'
        '<path d="M5 12h14M13 6l6 6-6 6"/></g>')
    add("bullet", BULLET_CANVAS, '<circle cx="5" cy="7" r="3" fill="#ffffff"/>')

    hint = "Your files are encrypted until this is entered"
    row = 14 + 8 + text_width(hint, 12, 400)
    x0 = -row / 2
    add("hint", HINT_CANVAS,
        f'<g transform="translate({x0:.3f} {74 + (round(12 * LINE) - 14) / 2:.3f}) scale({14 / 24})" '
        f'fill="none" stroke="{ACCENT}" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">'
        '<rect x="5" y="11" width="14" height="10" rx="2"/><path d="M8 11V8a4 4 0 0 1 8 0v3"/></g>'
        + text_el(hint, 12, 400, MUTED, 74, anchor="start", x=x0 + 22))
    add("caps", CAPS_CANVAS, text_el("Caps Lock is on", 12, 400, MUTED, 98))
    return jobs


_widths = {}


def text_width(text, size, weight):
    """Advance width of a run, from where rsvg centres it."""
    key = (text, size, weight)
    if key not in _widths:
        doc = svg_doc((-400, 0, 800, 40), 1, '<rect x="-400" width="800" height="40" fill="#000"/>'
                      + text_el(text, size, weight, "#fff", 5))
        with tempfile.TemporaryDirectory() as d:
            png = os.path.join(d, "m.png")
            rsvg(doc, png)
            left = int(subprocess.run(["convert", png, "-trim", "-format", "%X", "info:"],
                                      check=True, capture_output=True, text=True).stdout.lstrip("+"))
        _widths[key] = 2 * (400 - left)
    return _widths[key]


def handoff_jobs_for_scale(scale, dest):
    """The final trace, which only the sign-in screen plays: the whole
    outline drawn once from its first corner (frame k shows k/(n-1) of it)."""
    out = os.path.join(dest, f"s{round(scale * 100):03d}")
    os.makedirs(out, exist_ok=True)
    jobs = []
    for g, pose in (("i", POSE_I), ("u", POSE_U)):
        width = LIGHT[g][0]
        for k in range(COMPLETE_FRAMES):
            offset = STROKE_DASH * (1 - k / (COMPLETE_FRAMES - 1))
            doc = svg_doc(GLASS[g], scale,
                          at(pose, light_path(width, STROKE_DASH, offset, "cg")),
                          LIGHT_DEFS + glow("cg", 2.0, 0))
            jobs.append((doc, os.path.join(out, f"complete-{g}-{k:02d}.png")))
    return jobs


def main():
    if len(sys.argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    dest, handoff = sys.argv[1], sys.argv[2]
    jobs = []
    for scale in SCALES:
        jobs += jobs_for_scale(scale, dest)
        jobs += handoff_jobs_for_scale(scale, handoff)
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 2) as pool:
        for future in [pool.submit(rsvg, svg, path) for svg, path in jobs]:
            future.result()
    print(f"{len(jobs)} boot frames written to {dest}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
