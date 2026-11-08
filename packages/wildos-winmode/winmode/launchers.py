"""Start entries for Windows apps, their icons, and the file types they
open (brief 5.5 and 5.7).

Every app gets one name, wildos-winmode-<id>, for its desktop file, its
icon and its window class: the agent gives each RemoteApp window that
class and the same desktop file name (windows.py), which is how Plasma
puts it under the right Start entry on the taskbar.
"""

import base64
import io
import os
import re
import subprocess

from . import config

PREFIX = "wildos-winmode-"

# What Wild OS knows about well-known programs that Windows' Start menu
# does not say. The file types are the old builder's.
WORD = ["application/vnd.openxmlformats-officedocument.wordprocessingml.document", "application/msword",
        "application/vnd.ms-word.document.macroEnabled.12", "application/vnd.openxmlformats-officedocument.wordprocessingml.template",
        "application/rtf", "application/vnd.oasis.opendocument.text"]
EXCEL = ["application/vnd.openxmlformats-officedocument.spreadsheetml.sheet", "application/vnd.ms-excel",
         "application/vnd.ms-excel.sheet.macroEnabled.12", "application/vnd.openxmlformats-officedocument.spreadsheetml.template",
         "text/csv", "application/vnd.oasis.opendocument.spreadsheet"]
POWERPOINT = ["application/vnd.openxmlformats-officedocument.presentationml.presentation", "application/vnd.ms-powerpoint",
              "application/vnd.openxmlformats-officedocument.presentationml.slideshow",
              "application/vnd.ms-powerpoint.presentation.macroEnabled.12", "application/vnd.oasis.opendocument.presentation"]
KNOWN = {
    "winword": ("word", WORD, "Office;WordProcessor;"),
    "excel": ("excel", EXCEL, "Office;Spreadsheet;"),
    "powerpnt": ("powerpoint", POWERPOINT, "Office;Presentation;"),
    "outlook": ("outlook", ["application/vnd.ms-outlook"], "Office;Email;"),
    "onenote": ("onenote", ["application/onenote"], "Office;"),
    "msaccess": ("access", ["application/vnd.ms-access", "application/x-msaccess"], "Office;Database;"),
    "mspub": ("publisher", ["application/x-mspublisher"], "Office;Publishing;"),
    "visio": ("visio", ["application/vnd.visio", "application/vnd.ms-visio.drawing"], "Office;"),
}


def make_id(name):
    s = re.sub(r"[^a-z0-9]+", "-", name.lower()).strip("-")
    return s[:40] or "app"


def exe_stem(windows_path):
    base = (windows_path or "").replace("/", "\\").rsplit("\\", 1)[-1]
    return base[:-4].lower() if base.lower().endswith(".exe") else base.lower()


def describe(app):
    """An app from wild-guest's Start menu list with Wild OS's id, file
    types and category."""
    stem = exe_stem(app.get("target", ""))
    known = KNOWN.get(stem)
    out = {"name": app.get("name", stem).strip()[:80], "target": app.get("target", ""), "arguments": app.get("arguments", "")}
    if known:
        out["id"], out["mime"], out["categories"] = known
    else:
        out["id"], out["mime"], out["categories"] = make_id(out["name"]), [], "Utility;"
    return out


# Windows' own RemoteApp plumbing, which owns hidden windows in every session
# and is never an app someone opened. wild-guest leaves the same out.
SHELL = {"rdpshell", "rdpinit", "rdpclip", "wild-guest", "textinputhost", "ctfmon", "dwm", "explorer",
         "shellexperiencehost", "startmenuexperiencehost", "searchhost", "lockapp", "applicationframehost",
         "shellappruntime"}


def is_shell(image):
    return exe_stem(image) in SHELL


def app_id_for_image(image, registry):
    """The app a Windows process belongs to, from its image path: the Start
    entry that starts that program, or one named after the program."""
    if image:
        key = image.lower()
        for app_id, app in registry.items():
            if app.get("target", "").lower() == key:
                return app_id
        stem = exe_stem(image)
        if stem in KNOWN:
            return KNOWN[stem][0]
        for app_id, app in registry.items():
            if exe_stem(app.get("target", "")) == stem:
                return app_id
        return make_id(stem)
    return None


def registry_file():
    return os.path.join(config.user_data_dir(), "apps.json")


def load_registry():
    data = config.read_json(registry_file(), {})
    return data if isinstance(data, dict) else {}


def icon_png(ico_b64):
    """The largest image in a program's .ico, as PNG bytes."""
    if not ico_b64:
        return None
    try:
        from PIL import Image
        img = Image.open(io.BytesIO(base64.b64decode(ico_b64)))
        sizes = img.info.get("sizes") or [img.size]
        img.size = max(sizes, key=lambda s: s[0] * s[1])
        img = img.convert("RGBA")
        if img.size[0] < 256:
            img = img.resize((256, 256), Image.LANCZOS)
        out = io.BytesIO()
        img.save(out, "PNG")
        return out.getvalue()
    except Exception:
        return None


def _escape_exec(value):
    return value.replace("\\", "\\\\").replace('"', '\\"')


def entry_text(app, icon_name):
    lines = [
        "[Desktop Entry]",
        "Type=Application",
        "Name=" + app["name"].replace("\n", " "),
        "Comment=Runs in Windows mode",
        f"Exec=wildos-winmode launch {_escape_exec(app['id'])} %F",
        "Icon=" + icon_name,
        "StartupWMClass=" + PREFIX + app["id"],
        "StartupNotify=true",
        "Terminal=false",
        "Categories=" + app["categories"],
        # Start and the launch feedback (board 5.3) read this. There is no
        # stand-in window while the app starts (brief 5.5a).
        "X-WildOS-Engine=windows-mode",
    ]
    if app["mime"]:
        lines.append("MimeType=" + ";".join(app["mime"]) + ";")
    return "\n".join(lines) + "\n"


def write_all(guest_apps):
    """Writes a Start entry for each app wild-guest listed, removes entries
    for apps that have gone, and saves the id to program map that window
    grouping uses. Returns the registry."""
    apps_dir, icons_dir = config.applications_dir(), config.icons_dir()
    os.makedirs(apps_dir, exist_ok=True)
    os.makedirs(icons_dir, exist_ok=True)
    registry = {}
    for raw in guest_apps:
        if not isinstance(raw, dict) or not raw.get("target"):
            continue
        app = describe(raw)
        if app["id"] in registry:
            continue
        icon_name = "wildos-windows-app"
        png = icon_png(raw.get("icon"))
        if png:
            with open(os.path.join(icons_dir, PREFIX + app["id"] + ".png"), "wb") as f:
                f.write(png)
            icon_name = PREFIX + app["id"]
        path = os.path.join(apps_dir, PREFIX + app["id"] + ".desktop")
        with open(path + ".new", "w", encoding="utf-8") as f:
            f.write(entry_text(app, icon_name))
        os.replace(path + ".new", path)
        registry[app["id"]] = app
    for name in os.listdir(apps_dir):
        if name.startswith(PREFIX) and name.endswith(".desktop") and name[len(PREFIX):-8] not in registry:
            os.remove(os.path.join(apps_dir, name))
            try:
                os.remove(os.path.join(icons_dir, name[:-8] + ".png"))
            except OSError:
                pass
    config.write_json(registry_file(), registry)
    refresh_menus()
    return registry


def remove_all():
    write_all([])
    set_defaults({}, False)


def refresh_menus():
    for cmd in (["update-desktop-database", config.applications_dir()], ["kbuildsycoca6"]):
        try:
            subprocess.run(cmd, capture_output=True, timeout=60)
        except (OSError, subprocess.TimeoutExpired):
            pass


def mimeapps_path():
    base = os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config")
    return os.path.join(base, "mimeapps.list")


def shipped_defaults(path=None):
    """Wild OS's list of the file types Windows apps open by default, when
    "Open Windows files in Windows apps" is on (brief 5.7)."""
    wanted = {}
    try:
        with open(path or os.path.join(config.DATA, "mimeapps.list"), encoding="utf-8") as f:
            section = None
            for line in f:
                line = line.strip()
                if line.startswith("["):
                    section = line
                elif section == "[Default Applications]" and "=" in line and not line.startswith("#"):
                    mime, desktop = line.split("=", 1)
                    wanted[mime.strip()] = desktop.strip().rstrip(";")
    except OSError:
        pass
    return wanted


def set_defaults(registry, on, shipped=None, path=None):
    """Makes the Windows apps the default for their file types in the
    person's mimeapps.list, or takes them out again, leaving every other
    line as it was."""
    path = path or mimeapps_path()
    shipped = shipped if shipped is not None else shipped_defaults()
    installed = {PREFIX + i + ".desktop" for i in registry}
    wanted = {m: d for m, d in shipped.items() if d in installed} if on else {}
    try:
        with open(path, encoding="utf-8") as f:
            lines = f.read().splitlines()
    except OSError:
        lines = []
    out, section, seen_default = [], None, False
    for line in lines:
        s = line.strip()
        if s.startswith("["):
            if section == "[Default Applications]":
                out.extend(f"{m}={d};" for m, d in sorted(wanted.items()))
                wanted = {}
            section = s
            seen_default = seen_default or s == "[Default Applications]"
            out.append(line)
            continue
        if section == "[Default Applications]" and "=" in s:
            mime, value = s.split("=", 1)
            mime = mime.strip()
            if mime in wanted or value.strip().startswith(PREFIX):
                continue
        out.append(line)
    if section == "[Default Applications]":
        out.extend(f"{m}={d};" for m, d in sorted(wanted.items()))
    elif wanted:
        if not seen_default:
            out.append("[Default Applications]")
        out.extend(f"{m}={d};" for m, d in sorted(wanted.items()))
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path + ".new", "w", encoding="utf-8") as f:
        f.write("\n".join(out) + "\n")
    os.replace(path + ".new", path)
