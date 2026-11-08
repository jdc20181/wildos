"""Draws docs/assets/windows-mode-flow-{dark,light}.svg, the README's picture of how a Windows program finds its way.

    python docs/assets/make-windows-mode-flow.py

A plain image rather than a Mermaid diagram, because GitHub shows Mermaid in a frame with pan and zoom controls.
"""
from pathlib import Path

HERE = Path(__file__).resolve().parent

THEMES = {
    'dark': dict(bg='#0b1020', card='#141b31', line='#2a3456', text='#eef1f8', muted='#9aa3bd', accent='#8fb4ff',
                 accent_fill='#1a2547', ok='#a8ecc8', ok_fill='#13291f', warn='#f3c6bc', warn_fill='#2c1a18', arrow='#5b6789'),
    'light': dict(bg='#ffffff', card='#f4f6fb', line='#d9dfeb', text='#141a2e', muted='#5b647d', accent='#3a56c4',
                  accent_fill='#e8edfc', ok='#1f7a4d', ok_fill='#e3f4ea', warn='#a34a3a', warn_fill='#fbeae6', arrow='#9aa3bd'),
}

W, H = 960, 380
FONT = "-apple-system, BlinkMacSystemFont, 'Segoe UI', Inter, Helvetica, Arial, sans-serif"


def box(x, y, w, h, title, sub, fill, stroke, title_colour, sub_colour, rx=14):
    out = [f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>']
    cy = y + h / 2
    if sub:
        out.append(f'<text x="{x + w / 2}" y="{cy - 4}" text-anchor="middle" font-size="16" font-weight="600" fill="{title_colour}">{title}</text>')
        out.append(f'<text x="{x + w / 2}" y="{cy + 17}" text-anchor="middle" font-size="13" fill="{sub_colour}">{sub}</text>')
    else:
        out.append(f'<text x="{x + w / 2}" y="{cy + 6}" text-anchor="middle" font-size="16" font-weight="600" fill="{title_colour}">{title}</text>')
    return out


def arrow(points, colour, label=None, label_at=None, label_colour=None):
    d = 'M' + ' L'.join(f'{x} {y}' for x, y in points)
    out = [f'<path d="{d}" fill="none" stroke="{colour}" stroke-width="1.8" stroke-linejoin="round" marker-end="url(#head)"/>']
    if label:
        lx, ly = label_at
        out.append(f'<text x="{lx}" y="{ly}" font-size="12.5" fill="{label_colour}">{label}</text>')
    return out


def draw(t):
    c = THEMES[t]
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="{FONT}" role="img" '
         f'aria-label="How Wild OS handles a Windows program: wild-run sends most programs to Windows mode, which shows them as normal windows; programs with a Linux version install that from the Store; games with kernel anti-cheat are not supported, and Wild OS says so.">',
         f'<defs><marker id="head" viewBox="0 0 10 10" refX="9" refY="5" markerWidth="7" markerHeight="7" orient="auto-start-reverse"><path d="M0 0 L10 5 L0 10 z" fill="{c["arrow"]}"/></marker></defs>',
         f'<rect width="{W}" height="{H}" rx="18" fill="{c["bg"]}"/>']
    # The start and the router, on the middle row.
    s += box(24, 154, 150, 72, 'You double-click', 'Setup.exe', c['card'], c['line'], c['text'], c['muted'])
    s += arrow([(174, 190), (206, 190)], c['arrow'])
    s += box(210, 154, 140, 72, 'wild-run', 'checks who made it', c['accent_fill'], c['accent'], c['accent'], c['muted'])
    # Three ways out of the router, each labelled on its own straight run so the words never meet a box.
    s += arrow([(350, 180), (366, 180), (366, 72), (520, 72)], c['arrow'], 'Most programs', (378, 63), c['muted'])
    s += arrow([(350, 190), (520, 190)], c['arrow'], 'Has a Linux version', (378, 181), c['muted'])
    s += arrow([(350, 200), (366, 200), (366, 308), (520, 308)], c['arrow'], 'Kernel anti-cheat', (378, 299), c['muted'])
    s += box(524, 36, 200, 72, 'Windows mode', 'Windows 11 Pro, out of sight', c['accent_fill'], c['accent'], c['accent'], c['muted'])
    s += arrow([(724, 72), (756, 72)], c['arrow'])
    s += box(760, 36, 176, 72, 'A normal window', 'on your desktop', c['ok_fill'], c['ok'], c['ok'], c['muted'])
    s += box(524, 154, 200, 72, 'Linux app from the Store', 'Zoom, Slack, Steam', c['card'], c['line'], c['text'], c['muted'])
    s += box(524, 272, 200, 72, 'Not supported', 'and we say so plainly', c['warn_fill'], c['warn'], c['warn'], c['muted'])
    s.append('</svg>')
    return '\n'.join(s) + '\n'


for theme in THEMES:
    (HERE / f'windows-mode-flow-{theme}.svg').write_text(draw(theme), encoding='utf-8', newline='\n')
print('written')
