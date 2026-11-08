"""Taskbar grouping for RemoteApp windows (brief 5.5).

All Windows apps arrive through one FreeRDP connection, so one client
process owns every window, and FreeRDP's /wm-class would give them all one
class (phase 1, risk 10.4). Without it, FreeRDP names each window's class
"RAIL:<window handle in hex>", the handle Windows gave the window.
wild-guest reports which program owns each handle; from that this module
gives each X11 window its app's class and desktop file name,
wildos-winmode-<id>, matching the StartupWMClass of the app's Start entry.
KWin follows _KDE_NET_WM_DESKTOP_FILE when it changes, which is what
Plasma's taskbar groups by.

Runs under XWayland (FreeRDP's RemoteApp mode is X11 only).
"""

import re
import time

from gi.repository import GLib

RAIL = re.compile(r"^RAIL:([0-9A-Fa-f]{1,16})$")


def rail_handle(wm_class):
    """The Windows window handle from FreeRDP's class, or None."""
    if not wm_class:
        return None
    for part in wm_class:
        m = RAIL.match(part or "")
        if m:
            return int(m.group(1), 16)
    return None


class Tagger:
    """Watches the X server for FreeRDP's RemoteApp windows and tags each.
    lookup(handle, done) asks the agent which app id a handle belongs to
    and calls done(app_id or None)."""

    def __init__(self, lookup, on_tagged=None, on_seen=None):
        from Xlib import X, display
        self.X = X
        self.d = display.Display()
        self.root = self.d.screen().root
        self.lookup = lookup
        self.on_tagged = on_tagged or (lambda xid, handle, app_id, seen_at: None)
        self.on_seen = on_seen or (lambda seen_at: None)
        self.atoms = {name: self.d.intern_atom(name) for name in
                      ("WM_CLASS", "_KDE_NET_WM_DESKTOP_FILE", "_GTK_APPLICATION_ID", "UTF8_STRING", "_NET_CLIENT_LIST")}
        self.tagged = {}
        self.pending = set()
        self.skipped = set()
        # When each RemoteApp window was first seen, and its handle: what a
        # launch is timed against.
        self.seen_at = {}
        self.root.change_attributes(event_mask=X.SubstructureNotifyMask | X.PropertyChangeMask)
        self.d.flush()
        GLib.io_add_watch(self.d.fileno(), GLib.PRIORITY_DEFAULT, GLib.IO_IN, self._events)
        self.scan()

    def _events(self, fd, cond):
        X = self.X
        while self.d.pending_events():
            e = self.d.next_event()
            if e.type in (X.CreateNotify, X.MapNotify, X.ReparentNotify):
                win = getattr(e, "window", None)
                if win is not None:
                    self.consider(win, tries=6)
            elif e.type == X.PropertyNotify and e.window == self.root and e.atom == self.atoms["_NET_CLIENT_LIST"]:
                self.scan()
            elif e.type == X.DestroyNotify:
                self.tagged.pop(e.window.id, None)
                self.skipped.discard(e.window.id)
                self.seen_at.pop(e.window.id, None)
        return True

    def scan(self):
        """Every RAIL window there now, top level or inside a frame."""
        def walk(win, depth):
            try:
                children = win.query_tree().children
            except Exception:
                return
            for child in children:
                self.consider(child, tries=1)
                if depth < 2:
                    walk(child, depth + 1)
        walk(self.root, 0)

    def consider(self, win, tries):
        """FreeRDP sets the class right after creating the window, so a
        window with none yet is looked at again shortly."""
        xid = win.id
        if xid in self.tagged or xid in self.pending or xid in self.skipped:
            return
        try:
            cls = win.get_wm_class()
        except Exception:
            return
        handle = rail_handle(cls)
        if handle is None:
            if cls is None and tries > 1:
                GLib.timeout_add(50, lambda: (self.consider(win, tries - 1), False)[1])
            return
        if xid not in self.seen_at:
            self.seen_at[xid] = (time.monotonic(), handle)
            self.on_seen(self.seen_at[xid][0])
        self.pending.add(xid)

        # app_id is None while the handle is not known yet (it is asked
        # again when the window list changes), "" for Windows' own windows.
        def found(app_id):
            self.pending.discard(xid)
            if app_id:
                self.tag(win, handle, app_id)
            elif app_id == "":
                self.skipped.add(xid)

        self.lookup(handle, found)

    def tag(self, win, handle, app_id):
        name = "wildos-winmode-" + app_id
        try:
            win.set_wm_class(name, name)
            for prop in ("_KDE_NET_WM_DESKTOP_FILE", "_GTK_APPLICATION_ID"):
                win.change_property(self.atoms[prop], self.atoms["UTF8_STRING"], 8, name.encode())
            self.d.flush()
        except Exception:
            return
        self.tagged[win.id] = app_id
        self.on_tagged(win.id, handle, app_id, self.seen_at.get(win.id, (None,))[0])

    def seen_at_handle(self, handle):
        for xid, t in self.seen_at.items():
            if t[1] == handle:
                return t[0]
        return None

    def read_back(self, xid):
        """The class and desktop file name a window has now, for tests."""
        win = self.d.create_resource_object("window", xid)
        prop = win.get_full_property(self.atoms["_KDE_NET_WM_DESKTOP_FILE"], 0)
        return {"class": win.get_wm_class(), "desktop_file": prop.value.decode() if prop else None,
                "title": win.get_wm_name()}
