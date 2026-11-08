"""Wild OS's light or dark look and accent colour, as Windows wants them
(brief 5.4 step 8). Read from kdeglobals, which Plasma's colour settings
and Wild OS's own Settings both write."""

import configparser
import os


def kdeglobals_path():
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(base, "kdeglobals")


def _rgb(text):
    try:
        parts = [int(p) for p in text.split(",")[:3]]
    except (ValueError, AttributeError):
        return None
    return parts if len(parts) == 3 and all(0 <= p <= 255 for p in parts) else None


def read(path=None):
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    cfg.optionxform = str
    try:
        cfg.read(path or kdeglobals_path(), encoding="utf-8")
    except (configparser.Error, UnicodeDecodeError):
        pass
    window = _rgb(cfg.get("Colors:Window", "BackgroundNormal", fallback=""))
    dark = False
    if window:
        r, g, b = window
        dark = (0.2126 * r + 0.7152 * g + 0.0722 * b) / 255 < 0.5
    else:
        dark = "dark" in cfg.get("General", "ColorScheme", fallback="").lower()
    accent = _rgb(cfg.get("General", "AccentColor", fallback="")) or _rgb(
        cfg.get("Colors:Selection", "BackgroundNormal", fallback=""))
    return {"dark": dark, "accent": "#%02x%02x%02x" % tuple(accent) if accent else ""}
