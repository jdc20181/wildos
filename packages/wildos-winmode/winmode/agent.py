"""The Windows mode agent: one per person, in their session (brief section 3).

It owns what belongs to the person rather than the PC: the one RemoteApp
connection and its password (kept in their keyring), taskbar grouping of
the windows it brings, the Start entries and file types for Windows apps,
and pushing their light or dark look to Windows. Everything that touches
the guest itself goes through the system service (system.py).

Served on the session bus as org.wildos.WindowsMode1, the interface
Settings already reads (wildos-settings, windowsmode.h): Status is one JSON
object, Changed says to read it again, and calls that can fail answer with
a sentence to show, or nothing.
"""

import json
import os
import shutil
import subprocess
import sys
import time

from gi.repository import Gio, GLib

from . import config, dbusutil, launchers, msdl, paths, rdp, theme

XML = """
<node>
  <interface name="org.wildos.WindowsMode1">
    <method name="Status"><arg type="s" direction="out"/></method>
    <method name="Launch">
      <arg name="app" type="s" direction="in"/>
      <arg name="files" type="as" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="LaunchProgram">
      <arg name="path" type="s" direction="in"/>
      <arg name="args" type="as" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="RunInstaller">
      <arg name="path" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="OutsideSharedFolders">
      <arg name="files" type="as" direction="in"/>
      <arg type="as" direction="out"/>
    </method>
    <method name="CopyIntoDocuments">
      <arg name="files" type="as" direction="in"/>
      <arg type="as" direction="out"/>
    </method>
    <method name="RefreshApps"><arg type="s" direction="out"/></method>
    <method name="SetOption">
      <arg name="key" type="s" direction="in"/>
      <arg name="value" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="SetProductKey">
      <arg name="key" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="CheckUpdates"><arg type="s" direction="out"/></method>
    <method name="OpenDesktop"><arg type="s" direction="out"/></method>
    <method name="Repair"><arg type="s" direction="out"/></method>
    <method name="Remove"><arg type="s" direction="out"/></method>
    <method name="StartSetup">
      <arg name="request" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="PauseSetup"><arg type="s" direction="out"/></method>
    <method name="ResumeSetup"><arg type="s" direction="out"/></method>
    <method name="CancelSetup"><arg type="s" direction="out"/></method>
    <method name="ChooseIso">
      <arg name="path" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="OpenDownloadPage"><arg type="s" direction="out"/></method>
    <method name="Firmware"><arg type="s" direction="out"/></method>
    <signal name="Changed"/>
  </interface>
</node>
"""

NOT_YET = "This isn't available yet."
# How long a launch waits for a new connection's session to be up.
SESSION_WAIT = 90
SECRET_ATTRS = ["service", "org.wildos.WindowsMode1", "account"]


def log(*parts):
    print(*parts, file=sys.stderr, flush=True)


def split_arguments(text):
    """A Start menu shortcut's arguments as a list, Windows-style: quotes
    group, backslashes stay."""
    out, cur, quoted, any_char = [], [], False, False
    for ch in text or "":
        if ch == '"':
            quoted, any_char = not quoted, True
        elif ch.isspace() and not quoted:
            if cur or any_char:
                out.append("".join(cur))
            cur, any_char = [], False
        else:
            cur.append(ch)
    if cur or any_char:
        out.append("".join(cur))
    return out


def wake_key(came_from, via):
    return f"{came_from}/{via}"


# Until this PC has timed each path itself: a cold start of Windows and its
# sign-in, restoring from disk, and opening the connection to a Windows that
# is already awake.
DEFAULT_WAKE = {
    "stopped/new connection": 60,
    "saved/new connection": 15,
    "paused/new connection": 8,
    "running/new connection": 8,
}


def launch_state(rec):
    """The target a launch is held to (brief 5.5a): warm or cold when Windows
    was paused or running, saved or stopped otherwise."""
    came_from = rec.get("from")
    if came_from in ("saved", "stopped"):
        return came_from
    return ("warm" if rec.get("warm") else "cold") + "-" + (came_from or "running")


class Keyring:
    """The Windows account's password in the person's keyring (KWallet
    serves the Secret Service on Wild OS), through libsecret's secret-tool.
    Without a keyring it is only kept in memory, and a new one is set the
    next time the agent starts."""

    def __init__(self):
        self.memory = {}
        self.tool = shutil.which("secret-tool")

    def get(self, account):
        if account in self.memory:
            return self.memory[account]
        if self.tool:
            try:
                p = subprocess.run([self.tool, "lookup", *SECRET_ATTRS, account], capture_output=True, text=True, timeout=10)
                if p.returncode == 0 and p.stdout:
                    self.memory[account] = p.stdout.rstrip("\n")
                    return self.memory[account]
            except (OSError, subprocess.TimeoutExpired):
                pass
        return None

    def put(self, account, password):
        self.memory[account] = password
        if self.tool:
            try:
                subprocess.run([self.tool, "store", "--label", "Windows mode", *SECRET_ATTRS, account],
                               input=password, text=True, capture_output=True, timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                pass

    def forget(self, account):
        self.memory.pop(account, None)
        if self.tool:
            try:
                subprocess.run([self.tool, "clear", *SECRET_ATTRS, account], capture_output=True, timeout=10)
            except (OSError, subprocess.TimeoutExpired):
                pass


class Agent:
    def __init__(self, session, system):
        self.session = session
        self.system_bus = system
        self.options = config.read_json(config.user_config_file(), {}) or {}
        self.options.setdefault("open_files", True)
        self.registry = launchers.load_registry()
        self.keyring = Keyring()
        self.host = {}
        self.windows = []
        self.queue = []
        self.waking = False
        self.launches = {}
        self.last_launch = None
        self.refreshed = False
        # Installers handed over by RunInstaller, by program name: once their
        # windows have all closed, Windows' Start menu is read again so the
        # new app gets its Start entry.
        self.installers = {}
        self.history = config.read_json(os.path.join(config.user_data_dir(), "history.json"), {}) or {}
        self.wake_times = config.read_json(os.path.join(config.user_data_dir(), "wake-times.json"), {}) or {}
        # The windows already open when the "Starting Windows" card went up.
        self.starting = None
        self.starting_timer = None
        self.conn = rdp.Connection(self.connection_changed, self.auth_failed,
                                   os.path.join(config.user_data_dir(), "connection.log"))
        self.desktop = rdp.Connection(lambda is_open: None, self.auth_failed,
                                      os.path.join(config.user_data_dir(), "desktop.log"))
        self.export = dbusutil.Exported(session, config.OBJECT_PATH, XML, config.AGENT_INTERFACE, {
            "Status": (self.m_status, "s", False),
            "Launch": (self.m_launch, "s", False),
            "LaunchProgram": (self.m_launch_program, "s", False),
            "RunInstaller": (self.m_run_installer, "s", False),
            "OutsideSharedFolders": (self.m_outside, "as", False),
            "CopyIntoDocuments": (self.m_copy, "as", False),
            "RefreshApps": (self.m_refresh, "s", False),
            "SetOption": (self.m_set_option, "s", False),
            "SetProductKey": (self.m_set_product_key, "s", False),
            "CheckUpdates": (lambda sender: NOT_YET, "s", False),
            "OpenDesktop": (self.m_open_desktop, "s", False),
            "Repair": (lambda sender: NOT_YET, "s", False),
            "Remove": (lambda sender: NOT_YET, "s", False),
            "StartSetup": (self.m_start_setup, "s", False),
            "PauseSetup": (lambda sender, defer: self.setup_control("pause", "", defer), "s", False),
            "ResumeSetup": (lambda sender, defer: self.setup_control("resume", "", defer), "s", False),
            "CancelSetup": (lambda sender, defer: self.setup_control("cancel", "", defer), "s", False),
            "ChooseIso": (lambda sender, path, defer: self.setup_control("iso", path, defer), "s", False),
            "OpenDownloadPage": (self.m_open_download_page, "s", False),
            "Firmware": (self.m_firmware, "s", False),
        })
        system.signal_subscribe(config.BUS_NAME, config.HOST_INTERFACE, None, config.OBJECT_PATH, None,
                                Gio.DBusSignalFlags.NONE, self.host_signal)
        self.tagger = None
        if os.environ.get("DISPLAY"):
            try:
                from . import windows
                self.tagger = windows.Tagger(self.lookup_window, self.window_tagged, self.rail_seen)
            except Exception as e:
                log("window grouping is off:", e)
        # Asked before the first call is answered (none is until this
        # returns), so a just-started agent never reports "not set up" on a PC
        # that is.
        self.refresh_host_now()
        self.watch_theme()
        GLib.timeout_add_seconds(5, lambda: (self.push_warm(), False)[1])

    # The system service.
    def call(self, method, args=None, signature=None, done=None, timeout=600):
        params = GLib.Variant(signature, args) if signature else None

        def finished(conn, res):
            try:
                value = conn.call_finish(res).unpack()
                value = value[0] if value else None
            except GLib.Error as e:
                value = GLib.Error(e.message)
            if done:
                done(value)

        self.system_bus.call(config.BUS_NAME, config.OBJECT_PATH, config.HOST_INTERFACE, method, params, None,
                             Gio.DBusCallFlags.NONE, timeout * 1000, None, finished)

    def call_json(self, method, args=None, signature=None, done=None, timeout=600):
        def parsed(value):
            if isinstance(value, GLib.Error):
                done({"ok": False, "error": value.message})
                return
            try:
                done(json.loads(value) if isinstance(value, str) else {})
            except ValueError:
                done({"ok": False, "error": "unreadable answer"})
        self.call(method, args, signature, parsed if done else None, timeout)

    def host_signal(self, conn, sender, path, iface, signal, params):
        if signal == "WindowsChanged":
            try:
                self.set_windows(json.loads(params.unpack()[0]))
            except ValueError:
                pass
        self.refresh_host()

    def refresh_host_now(self):
        try:
            value = self.system_bus.call_sync(config.BUS_NAME, config.OBJECT_PATH, config.HOST_INTERFACE, "Status", None,
                                              GLib.VariantType("(s)"), Gio.DBusCallFlags.NONE, 10000, None).unpack()[0]
            st = json.loads(value)
        except (GLib.Error, ValueError) as e:
            log("Windows mode's service didn't answer:", e)
            self.refresh_host()
            return
        if isinstance(st, dict) and "state" in st:
            self.host = st
            if isinstance(st.get("windows"), list):
                self.windows = st["windows"]

    def refresh_host(self):
        def got(st):
            if isinstance(st, dict) and "state" in st:
                was = (self.host.get("setup") or {}).get("running")
                self.host = st
                if was and not (st.get("setup") or {}).get("running") and st.get("state") != "NotInstalled":
                    self.setup_finished()
                if isinstance(st.get("windows"), list):
                    self.set_windows(st["windows"])
            self.changed()
        self.call_json("Status", done=got, timeout=10)

    def changed(self):
        self.export.emit("Changed")

    # Windows and the connection.
    def set_windows(self, ws):
        self.windows = ws
        if self.tagger:
            self.tagger.scan()
        now = time.monotonic()
        # Any new window but the RemoteApp shell's own means Windows is up and
        # the app (or the installer, which has no app of its own) is showing.
        if self.starting is not None and any(w.get("hwnd") not in self.starting and self.app_for(w) != "" for w in ws):
            self.hide_starting()
        stems = {launchers.exe_stem(w.get("image")) for w in ws}
        for stem, seen in list(self.installers.items()):
            if stem in stems:
                self.installers[stem] = True
            elif seen:
                del self.installers[stem]
                GLib.timeout_add_seconds(5, lambda: (self.m_refresh(None), False)[1])
        # The guest lists windows once a second, so a window it lists may
        # well be on screen already: the screen's own time is used when the
        # window has been seen there.
        for w in ws:
            seen = self.tagger.seen_at_handle(w.get("hwnd")) if self.tagger else None
            self.window_appeared(self.app_for(w), w.get("hwnd"), seen or now, "window on screen" if seen else "window listed")

    # Per-launch timing (brief 5.5a). Each launch records, in seconds from
    # the click: connection_ready (Windows awake and the session able to
    # take it), process_started (wild-guest started or showed the app) and
    # first_window (its first new window on screen), with the state Windows
    # was in and whether the app was warm. One JSON object per line in
    # launches.jsonl; "wildos-winmode launch-stats" reads the percentiles.
    def start_timing(self, app_id, started, info):
        known = {w.get("hwnd") for w in self.windows if self.app_for(w) == app_id}
        self.launches[app_id] = {"t0": started, "known": known, "info": dict(info)}

    def mark(self, app_id, name, value=None, now=None):
        rec = self.launches.get(app_id)
        if rec and name not in rec["info"]:
            rec["info"][name] = value if value is not None else round((now or time.monotonic()) - rec["t0"], 3)

    def window_appeared(self, app_id, handle, now, how):
        rec = self.launches.get(app_id)
        if rec and handle not in rec["known"]:
            del self.launches[app_id]
            self.record_launch(app_id, rec, now, how)

    def connection_seen(self, now):
        """The first window of a new connection (RemoteApp's own) means the
        session is up."""
        for app_id, rec in self.launches.items():
            if rec["info"].get("via") == "new connection":
                self.mark(app_id, "connection_ready", now=now)

    def count_launch(self, app_id):
        if app_id and app_id in self.registry:
            self.history[app_id] = self.history.get(app_id, 0) + 1
            try:
                config.write_json(os.path.join(config.user_data_dir(), "history.json"), self.history)
            except OSError:
                pass
            self.push_warm()

    def push_warm(self):
        """The apps to keep warm: Office's first when installed, then the
        most launched, as many as Windows mode's memory allows (about one
        for each 2 GB, at most four)."""
        office = ["word", "excel", "powerpoint", "outlook"]
        ids = sorted(self.registry, key=lambda i: (i not in office, -self.history.get(i, 0), office.index(i) if i in office else 0))
        memory = self.host.get("memory_bytes") or 4 * 1024 ** 3
        limit = max(1, min(4, int(memory // (2 * 1024 ** 3))))
        apps = [{"id": i, "path": self.registry[i]["target"]} for i in ids[:limit]]
        self.call("SetWarm", (json.dumps({"apps": apps, "limit": limit}),), "(s)")
        return False

    def record_launch(self, app_id, rec, now, how):
        info = rec["info"]
        info.setdefault("connection_ready", info.get("wake_seconds"))
        out = dict(info, app=app_id, first_window=round(now - rec["t0"], 3), seen_by=how,
                   at=time.strftime("%Y-%m-%dT%H:%M:%S"))
        out["state"] = launch_state(out)
        self.last_launch = out
        self.hide_starting()
        self.wake_times[wake_key(info.get("from") or "running", info.get("via"))] = out["first_window"]
        try:
            config.write_json(os.path.join(config.user_data_dir(), "wake-times.json"), self.wake_times)
        except OSError:
            pass
        log("launch:", json.dumps(out))
        try:
            os.makedirs(config.user_data_dir(), exist_ok=True)
            with open(os.path.join(config.user_data_dir(), "launches.jsonl"), "a", encoding="utf-8") as f:
                f.write(json.dumps(out) + "\n")
        except OSError:
            pass
        self.changed()

    def app_for(self, window):
        image = window.get("image") if isinstance(window, dict) else None
        if not image:
            return None
        return "" if launchers.is_shell(image) else launchers.app_id_for_image(image, self.registry)

    def lookup_window(self, handle, done):
        for w in self.windows:
            if w.get("hwnd") == handle:
                done(self.app_for(w))
                return

        def got(reply):
            done(self.app_for(reply.get("window") if isinstance(reply, dict) else None))

        self.call_json("Window", (handle,), "(t)", got, timeout=10)

    def rail_seen(self, seen_at):
        self.connection_seen(seen_at)

    def window_tagged(self, xid, handle, app_id, seen_at):
        self.window_appeared(app_id, handle, seen_at or time.monotonic(), "window on screen")

    def connection_changed(self, is_open):
        log("connection", "open" if is_open else "closed")
        self.call("ConnectionChanged", (is_open,), "(b)")
        self.changed()

    def auth_failed(self):
        log("Windows refused the stored password; a new one is set next time")
        self.keyring.forget(self.host.get("account", "wild"))

    def guest_paths(self, files):
        mapping = paths.folder_map(self.host.get("folders", []), self.host.get("share_root", ""))
        drive = self.host.get("drive", "Z")
        return [paths.to_guest(f, mapping, drive) for f in files], mapping

    def launch(self, path, args, name, app_id, done):
        """Starts a Windows program. Windows is woken first (resumed,
        restored or started; immediate when it is running). With the
        connection open the program starts inside the session through
        wild-guest; otherwise the connection opens with it."""
        started = time.monotonic()
        self.show_starting(name, app_id)
        finish = done

        def done(message):
            if message:
                self.hide_starting()
            finish(message)

        self.queue.append((path, args, name, app_id, started, done))
        if self.waking:
            return
        self.waking = True

        def woke(reply):
            self.waking = False
            pending, self.queue = self.queue, []
            if not reply.get("ok"):
                for item in pending:
                    item[5](reply.get("message") or "Windows mode couldn't start. Try again, or restart the PC.")
                log("wake failed:", reply.get("error"))
                return
            if self.conn.open:
                for item in pending:
                    self.launch_in_session(*item, wake=reply)
                return
            self.queue = pending
            self.credentials(lambda creds: self.open_connection(creds, reply))

        self.call_json("Wake", done=woke)

    def show_starting(self, name, app_id):
        """The card that says Windows is starting (wildos-system-dialogs),
        whenever the app will wait more than a moment for Windows. The
        estimate is the last time the same path took on this PC."""
        lifecycle = self.host.get("state")
        if lifecycle == "Running" and self.conn.open:
            return
        came_from = {"Running": "running", "Paused": "paused", "Saved": "saved"}.get(lifecycle, "stopped")
        key = wake_key(came_from, "session" if self.conn.open else "new connection")
        seconds = self.wake_times.get(key) or DEFAULT_WAKE.get(key, 1)
        if seconds < 3 or self.starting is not None:
            return
        self.starting = {w.get("hwnd") for w in self.windows}
        icon = launchers.PREFIX + app_id if app_id in self.registry else ""
        self.dialogs("ShowWindowsStarting", GLib.Variant("(ssib)", (name, icon, int(round(seconds)), came_from == "stopped")))
        # Never left up by a launch that opens no window of its own.
        self.starting_timer = GLib.timeout_add_seconds(int(seconds * 3) + 60, lambda: (self.hide_starting(), False)[1])

    def hide_starting(self):
        if self.starting is None:
            return
        self.starting = None
        if self.starting_timer:
            GLib.source_remove(self.starting_timer)
            self.starting_timer = None
        self.dialogs("HideWindowsStarting", None)

    def dialogs(self, method, args):
        self.session.call("org.wildos.SystemDialogs", "/Dialogs", "org.wildos.SystemDialogs", method, args, None,
                          Gio.DBusCallFlags.NO_AUTO_START, 2000, None, None, None)

    def launch_in_session(self, path, args, name, app_id, started, done, wake):
        self.start_timing(app_id, started, {"from": wake.get("from"), "wake_seconds": wake.get("seconds"), "via": "session"})

        # A connection that has only just opened is still signing in, and the
        # session helper starts after that: a cold sign-in can take a good
        # while, so the launch waits for the session up to a time limit.
        deadline = time.monotonic() + SESSION_WAIT

        def attempt():
            def got(reply):
                if reply.get("type") == "launched":
                    self.mark(app_id, "process_started")
                    self.mark(app_id, "warm", bool(reply.get("warm")))
                    done("")
                elif reply.get("error") in ("no-session", "not-connected", "timeout") and time.monotonic() < deadline:
                    GLib.timeout_add(250, lambda: (attempt(), False)[1])
                else:
                    log("launch failed:", reply)
                    done("That Windows app didn't open. Try again.")
            self.call_json("Launch", (path, args, ""), "(sass)", got, timeout=20)
        attempt()

    def drive_folder(self):
        try:
            return paths.drive_folder(self.host.get("folders") or config.load()["folders"], config.user_data_dir())
        except OSError as e:
            log("shared folders:", e)
            return None

    def m_set_product_key(self, sender, key, defer):
        def done(reply):
            self.refresh_host()
            defer.done(reply if isinstance(reply, str) else "Windows mode's background service didn't answer.")
        self.call("SetProductKey", (key,), "(s)", done, timeout=300)
        return defer

    def m_open_desktop(self, sender, defer):
        """The whole Windows desktop, for things only it can do (Windows'
        own settings, an installer that needs it). The app connection
        closes first: the desktop takes the one session over."""
        def woke(reply):
            if not reply.get("ok"):
                defer.done(reply.get("message") or "Windows mode couldn't start. Try again, or restart the PC.")
                return

            def signed(creds):
                if not creds.get("password"):
                    defer.done("Windows mode couldn't sign in to Windows. Restart the PC, then try again.")
                    return
                self.conn.stop_now()
                args = rdp.desktop_arguments(self.host.get("guest_address"), self.host.get("rdp_port", 3389),
                                             creds["account"], creds.get("domain", ""), drive=self.drive_folder())
                try:
                    self.desktop.start(args, creds["password"])
                except OSError as e:
                    defer.done(f"The Windows desktop couldn't open. ({e.strerror})")
                    return
                defer.done("")
            self.credentials(signed)

        if self.desktop.open:
            return ""
        self.call_json("Wake", done=woke)
        return defer

    def credentials(self, done):
        account = self.host.get("account", "wild")
        pw = self.keyring.get(account)
        if pw:
            done({"account": account, "password": pw, "domain": self.host.get("computer_name", "")})
            return

        def got(reply):
            if reply.get("password"):
                self.keyring.put(reply["account"], reply["password"])
            done(reply)
        self.call_json("Credentials", done=got, timeout=120)

    def open_connection(self, creds, wake):
        pending, self.queue = self.queue, []
        self.waking = False
        if not pending:
            return
        if not creds.get("password"):
            for item in pending:
                item[5]("Windows mode couldn't sign in to Windows. Restart the PC, then try again.")
            return
        path, args, name, app_id, started, done = pending[0]
        info = {"from": wake.get("from"), "wake_seconds": wake.get("seconds"), "via": "new connection"}
        self.start_timing(app_id, started, info)
        rdp_args = rdp.arguments(self.host.get("guest_address"), self.host.get("rdp_port", 3389), creds["account"], creds.get("domain", ""),
                                 name, path, args, drive=self.drive_folder())
        try:
            self.conn.start(rdp_args, creds["password"])
        except OSError as e:
            done(f"Windows mode couldn't open {name}. ({e.strerror})")
            return
        done("")
        for item in pending[1:]:
            self.launch_in_session(*item, wake=wake)
        if not self.refreshed:
            self.refreshed = True
            GLib.timeout_add_seconds(30, lambda: (self.m_refresh(None), False)[1])

    # D-Bus methods.
    def status(self):
        h = self.host
        setup = h.get("setup") or {}
        state = {"NotInstalled": "none", "SettingUp": "setup", "Error": "failed"}.get(h.get("state"), "ready" if h else "")
        if setup.get("running"):
            state = "failed" if setup.get("failed") else "setup"
        elif setup and setup.get("stage") != "done" and state == "ready":
            # A virtual PC is there but setup never finished with it (setup
            # was stopped part way): not ready, and the wizard offers to
            # start over.
            state = "failed"
        open_ids = {self.app_for(w) for w in self.windows}
        options = dict(setup.get("options") or {})
        # In Settings' words (board 9.8).
        options["whenIdle"] = {"keep": "memory", "save": "disk"}.get(h.get("when_idle"), h.get("when_idle"))
        return {
            "state": state,
            # Board 9.5's progress while setting up (setup.py).
            "stages": setup.get("stages") or [],
            "percent": setup.get("percent", 0),
            "left": setup.get("left", ""),
            "paused": bool(setup.get("paused")),
            "problem": setup.get("problem"),
            "licence": setup.get("licence") or {},
            "options": options,
            "apps": [{"id": i, "name": a["name"], "icon": launchers.PREFIX + i, "open": i in open_ids}
                        for i, a in sorted(self.registry.items(), key=lambda kv: kv[1]["name"].lower())],
            "lifecycle": h.get("state"),
            "diskBytes": h.get("disk_bytes") or 0,
            "vm": h.get("vm"),
            "connection": self.conn.open,
            "windows": len(self.windows),
            "openFiles": bool(self.options.get("open_files")),
            "idleIn": h.get("idle_in"),
            "lastLaunch": self.last_launch,
            "lastHostAction": h.get("last"),
        }

    def m_status(self, sender):
        return json.dumps(self.status())

    def m_launch(self, sender, app_id, files, defer):
        app = self.registry.get(app_id)
        if not app:
            return "That Windows app isn't installed any more."
        self.count_launch(app_id)
        guest, _ = self.guest_paths(files)
        if any(g is None for g in guest):
            return "Windows apps can only open files in your shared folders. Move it to Documents, then try again."
        args = split_arguments(app.get("arguments", "")) + guest
        self.launch(app["target"], args, app["name"], app_id, defer.done)
        return defer

    def m_launch_program(self, sender, path, args, defer):
        app_id = launchers.app_id_for_image(path, self.registry)
        self.launch(path, list(args), launchers.exe_stem(path) or "Windows app", app_id, defer.done)
        return defer

    def m_run_installer(self, sender, host_path, defer):
        """App Bridge and wild-run hand Windows installers here: the file is
        copied into the shared Downloads folder unless Windows can already
        see it, then run in Windows mode."""
        if not os.path.isfile(host_path):
            return "That file isn't there any more."
        guest, mapping = self.guest_paths([host_path])
        if guest[0] is None:
            downloads = mapping.get("Downloads")
            if not downloads:
                return "Windows mode can't see your Downloads folder. Check the shared folders in Settings."
            try:
                copy = paths.copy_into(host_path, downloads)
            except OSError:
                return "The installer couldn't be copied to Downloads. Check there's space on this PC."
            guest = [paths.to_guest(copy, mapping, self.host.get("drive", "Z"))]
        name = os.path.basename(host_path)
        self.installers["msiexec" if name.lower().endswith(".msi") else launchers.exe_stem(name)] = False
        if name.lower().endswith(".msi"):
            self.launch(r"C:\Windows\System32\msiexec.exe", ["/i", guest[0]], name, "installer", defer.done)
        else:
            self.launch(guest[0], [], name, "installer", defer.done)
        return defer

    def m_outside(self, sender, files):
        guest, _ = self.guest_paths(files)
        return [f for f, g in zip(files, guest) if g is None]

    def m_copy(self, sender, files):
        _, mapping = self.guest_paths(files)
        docs = mapping.get("Documents")
        out = []
        for f in files:
            try:
                out.append(paths.copy_into(f, docs) if docs else "")
            except OSError:
                out.append("")
        return out

    def m_refresh(self, sender, defer=None):
        def got(reply):
            apps = reply.get("apps")
            if not isinstance(apps, list):
                msg = "Windows mode couldn't list its apps. Try again when Windows mode is running."
            else:
                self.registry = launchers.write_all(apps)
                launchers.set_defaults(self.registry, bool(self.options.get("open_files")))
                self.push_warm()
                msg = ""
                self.changed()
            if defer:
                defer.done(msg)
        self.call_json("Apps", (True,), "(b)", got, timeout=180)
        return defer

    # Setting up (boards 9.1 to 9.7). The wizard asks here; the system
    # service does the work and owns Windows mode afterwards.
    def m_start_setup(self, sender, request, defer):
        try:
            req = json.loads(request)
        except ValueError:
            return "That request couldn't be read."
        self.setup_open_files = bool((req.get("options") or {}).get("openFiles", True))

        def started(reply):
            self.refresh_host()
            defer.done(reply if isinstance(reply, str) else "Windows mode's background service didn't answer.")
        self.call("SetupStart", (json.dumps(req),), "(s)", started, timeout=120)
        return defer

    def setup_control(self, action, argument, defer):
        def done(reply):
            # Read again before answering, so whatever asks next (the wizard
            # after Start over) sees the state the action left.
            self.refresh_host_now()
            defer.done(reply if isinstance(reply, str) else "Windows mode's background service didn't answer.")
        self.call("SetupControl", (action, argument), "(ss)", done, timeout=900)
        return defer

    def m_open_download_page(self, sender):
        try:
            subprocess.Popen(["xdg-open", msdl.PAGE], start_new_session=True,
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            return "A web browser couldn't be opened."
        return ""

    def m_firmware(self, sender, defer):
        self.call("Firmware", None, None, lambda r: defer.done(r if isinstance(r, str) else "{}"), timeout=120)
        return defer

    def setup_finished(self):
        """Windows is ready: its apps get Start entries, and files open in
        them if the person chose that in setup."""
        self.options["open_files"] = getattr(self, "setup_open_files", self.options.get("open_files", True))
        try:
            config.write_json(config.user_config_file(), self.options)
        except OSError:
            pass
        GLib.timeout_add_seconds(5, lambda: (self.m_refresh(None), False)[1])

    def m_set_option(self, sender, key, value, defer):
        if key == "openFiles":
            self.options["open_files"] = value in ("true", "1", "yes")
            config.write_json(config.user_config_file(), self.options)
            launchers.set_defaults(self.registry, self.options["open_files"])
            self.changed()
            return ""
        if key == "whenIdle":
            mapped = {"memory": "keep", "disk": "save"}.get(value, value)
            self.call("SetOption", ("when_idle", mapped), "(ss)",
                      lambda r: defer.done(r if isinstance(r, str) else "That setting couldn't be changed."))
            return defer
        return "That setting isn't available."

    # Theme.
    def watch_theme(self):
        self.theme_timer = None
        f = Gio.File.new_for_path(theme.kdeglobals_path())
        try:
            self.theme_monitor = f.monitor_file(Gio.FileMonitorFlags.WATCH_MOVES, None)
            self.theme_monitor.connect("changed", lambda *a: self.theme_soon())
        except GLib.Error:
            self.theme_monitor = None
        self.push_theme()

    def theme_soon(self):
        if self.theme_timer:
            GLib.source_remove(self.theme_timer)
        self.theme_timer = GLib.timeout_add(800, self.push_theme)

    def push_theme(self):
        self.theme_timer = None
        self.call("SetTheme", (json.dumps(theme.read()),), "(s)")
        return False


def main():
    loop = GLib.MainLoop()
    session = Gio.bus_get_sync(Gio.BusType.SESSION, None)
    system = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
    # The object exists before the name does: D-Bus delivers the call that
    # started the agent as soon as the name is taken, and it must find it.
    agent = Agent(session, system)
    Gio.bus_own_name_on_connection(session, config.BUS_NAME, Gio.BusNameOwnerFlags.NONE,
                                   lambda conn, name: log(f"wildos-winmode agent: serving {name}"),
                                   lambda conn, name: loop.quit())
    GLib.unix_signal_add(GLib.PRIORITY_HIGH, 15, loop.quit)
    loop.run()
    # FreeRDP runs in a process group of its own; left behind, it would keep
    # its windows on screen and hold up systemd stopping the agent.
    agent.conn.stop_now()
    agent.desktop.stop_now()
    # Held until here, so it lives as long as the loop.
    del agent
    return 0
