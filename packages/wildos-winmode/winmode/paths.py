"""Host paths and the paths Windows sees for them (brief 5.7).

Windows sees one drive with each shared folder at <drive>:\\<Name>, so
/home/alex/Documents/Budget.xlsx is Z:\\Documents\\Budget.xlsx.
"""

import os
import shutil

XDG_KEYS = {"Documents": "XDG_DOCUMENTS_DIR", "Downloads": "XDG_DOWNLOAD_DIR",
            "Pictures": "XDG_PICTURES_DIR", "Desktop": "XDG_DESKTOP_DIR",
            "Music": "XDG_MUSIC_DIR", "Videos": "XDG_VIDEOS_DIR"}


def user_dirs(home=None, config_home=None):
    if home is None:
        home = os.path.expanduser("~")
        config_home = config_home or os.environ.get("XDG_CONFIG_HOME")
    path = os.path.join(config_home or os.path.join(home, ".config"), "user-dirs.dirs")
    found = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                key, sep, value = line.strip().partition("=")
                if sep and key.startswith("XDG_"):
                    found[key] = value.strip('"').replace("$HOME", home)
    except OSError:
        pass
    return {name: found.get(key, os.path.join(home, name)) for name, key in XDG_KEYS.items()}


def folder_map(folders, share_root, home=None, config_home=None):
    """Each shared folder's place on this PC. A share root inside the home
    folder (a test rig) holds the folders themselves; otherwise they are
    the person's own folders, which the system service binds into it."""
    home = home or os.path.expanduser("~")
    if share_root and os.path.realpath(share_root).startswith(os.path.realpath(home) + os.sep):
        return {name: os.path.join(share_root, name) for name in folders}
    dirs = user_dirs(home, config_home)
    return {name: dirs.get(name, os.path.join(home, name)) for name in folders}


def to_guest(host_path, mapping, drive="Z"):
    """The Windows path for a file in a shared folder, or None."""
    path = os.path.realpath(host_path)
    for name, root in mapping.items():
        root = os.path.realpath(root)
        if path == root or path.startswith(root + os.sep):
            rest = path[len(root):].replace("/", "\\")
            return f"{drive}:\\{name}{rest}"
    return None


def copy_into(host_path, folder):
    """Copies a file into a shared folder under a name that is free there,
    and returns the copy's path."""
    os.makedirs(folder, exist_ok=True)
    base = os.path.basename(host_path)
    stem, ext = os.path.splitext(base)
    target = os.path.join(folder, base)
    n = 2
    while os.path.exists(target):
        target = os.path.join(folder, f"{stem} ({n}){ext}")
        n += 1
    shutil.copy2(host_path, target)
    return target


def used_bytes(path):
    """What a file really takes on disk (Windows' disk image is sparse), or 0
    when it isn't there."""
    try:
        return os.stat(path).st_blocks * 512
    except OSError:
        return 0


def drive_folder(folders, data_dir, home=None, config_home=None):
    """The folder FreeRDP shares into Windows as the "wildos" redirected
    drive (Z: in Windows): one link per shared folder to the folder itself, kept in step
    with user-dirs.dirs. Shared by the user's own RemoteApp connection, so
    Windows reaches exactly what the user can, with no mounts and nothing
    running as root."""
    root = os.path.join(data_dir, "drive")
    os.makedirs(root, mode=0o700, exist_ok=True)
    mapping = folder_map(folders, None, home, config_home)
    for name in os.listdir(root):
        if name not in mapping and os.path.islink(os.path.join(root, name)):
            os.unlink(os.path.join(root, name))
    for name, target in mapping.items():
        hide_windows_clutter(target)
        link = os.path.join(root, name)
        if os.path.islink(link) and os.readlink(link) == target:
            continue
        if os.path.lexists(link):
            os.unlink(link)
        os.symlink(target, link)
    return root


# What Windows keeps in folders it can see (its recycle bin on a redirected
# Desktop, folder settings, thumbnail caches). Windows hides them from its
# own users; .hidden does the same in Files and on the desktop.
WINDOWS_CLUTTER = ("$RECYCLE.BIN", "desktop.ini", "Thumbs.db")


def hide_windows_clutter(folder):
    path = os.path.join(folder, ".hidden")
    try:
        with open(path, encoding="utf-8") as f:
            text = f.read()
    except FileNotFoundError:
        text = ""
    except OSError:
        return
    missing = [n for n in WINDOWS_CLUTTER if n not in text.splitlines()]
    if not missing:
        return
    # Added to whatever the person listed there, never written over it.
    lead = "\n" if text and not text.endswith("\n") else ""
    try:
        with open(path, "a", encoding="utf-8") as f:
            f.write(lead + "".join(name + "\n" for name in missing))
    except OSError:
        pass
