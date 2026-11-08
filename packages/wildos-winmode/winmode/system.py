"""winmoded: the Windows mode system service (brief section 3).

A small root service that owns libvirt, the disk image and the guest's
channels. It runs the lifecycle state machine and idle policy (5.6), keeps
the guest's clock right after a pause or restore, and relays wild-guest's
window list. Sleep and shutdown handling (logind's delay inhibitor, board
9.9) are here too. The per-user agent (agent.py) reaches it on the
system bus; polkit lets the active local user in (org.wildos.winmode.use).
"""

import json
import os
import pwd
import secrets
import socket
import subprocess
import sys
import threading
import time

from gi.repository import Gio, GLib

from . import config, dbusutil, firewall, guestlink, hostinfo, lifecycle, paths, qga, setup, virsh

XML = """
<node>
  <interface name="org.wildos.WindowsMode1.Host">
    <method name="Status"><arg type="s" direction="out"/></method>
    <method name="Wake"><arg type="s" direction="out"/></method>
    <method name="Idle">
      <arg name="mode" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="Launch">
      <arg name="path" type="s" direction="in"/>
      <arg name="args" type="as" direction="in"/>
      <arg name="cwd" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="Windows"><arg type="s" direction="out"/></method>
    <method name="Window">
      <arg name="hwnd" type="t" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="Apps">
      <arg name="icons" type="b" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="SetTheme">
      <arg name="theme" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="SetOption">
      <arg name="key" type="s" direction="in"/>
      <arg name="value" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="SetWarm">
      <arg name="warm" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="ConnectionChanged">
      <arg name="open" type="b" direction="in"/>
    </method>
    <method name="Credentials"><arg type="s" direction="out"/></method>
    <method name="InstallGuestHelper"><arg type="s" direction="out"/></method>
    <method name="SetupStart">
      <arg name="request" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="SetupControl">
      <arg name="action" type="s" direction="in"/>
      <arg name="argument" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="Firmware"><arg type="s" direction="out"/></method>
    <method name="SetProductKey">
      <arg name="key" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <method name="GuestRequest">
      <arg name="message" type="s" direction="in"/>
      <arg type="s" direction="out"/>
    </method>
    <signal name="Changed"/>
    <signal name="WindowsChanged"><arg name="windows" type="s"/></signal>
  </interface>
</node>
"""

LAUNCH_HOLD = "launch"
# From the moment Windows runs until wild-guest's first window list, the
# window count is unknown, not zero: an app may well be open.
WINDOW_LIST_HOLD = "window list"
# A wild-guest that never reports must not keep Windows awake for good.
WINDOW_LIST_WAIT = 600


def log(*parts):
    print(*parts, file=sys.stderr, flush=True)


def random_password():
    # The old builder's alphabet, without look-alike characters, and a fixed
    # tail that meets Windows' complexity rule.
    alphabet = "ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz23456789"
    return "".join(secrets.choice(alphabet) for _ in range(24)) + "a7Q"


def app_windows(windows):
    """The windows that count as a Windows app being open: visible and not
    a tool window. Owned windows (dialogs) count too, since a dialog left
    open is still work in progress."""
    return [w for w in windows if isinstance(w, dict) and w.get("visible", True) and not w.get("tool")]


SAVE_TO_DISK = False
# Windows' share of memory follows what it uses (trim_memory), in KiB.
TRIM_HEADROOM_KIB = 1536 * 1024
TRIM_FLOOR_KIB = 3 * 1024 * 1024
TRIM_STEP_KIB = 512 * 1024
TRIM_AFTER = 60


class Service:
    def __init__(self, bus):
        self.bus = bus
        self.cfg = config.load()
        self.domain = self.cfg["domain"]
        saved = config.read_json(config.state_file(), {}) or {}
        self.state = saved if isinstance(saved, dict) else {}
        self.state.setdefault("when_idle", self.cfg["when_idle"])
        self.memory = virsh.defined_memory(self.domain)
        self.last_wake = 0.0
        self.life = lifecycle.Lifecycle(self.cfg["idle_seconds"])
        self.windows = {}
        self.error = None
        self.busy_action = None
        self.idle_timer = None
        self.window_list_timer = None
        self.auth_cache = {}
        # Taken from the main loop (claim) and from wake's worker thread.
        self.bind_lock = threading.Lock()
        self.link = guestlink.GuestLink(self.guest_event, self.guest_state)
        self.export = dbusutil.Exported(bus, config.OBJECT_PATH, XML, config.HOST_INTERFACE, {
            "Status": (self.m_status, "s", False),
            "Wake": (self.m_wake, "s", True),
            "Idle": (self.m_idle, "s", True),
            "Launch": (self.m_launch, "s", True),
            "Windows": (self.m_windows, "s", True),
            "Window": (self.m_window, "s", True),
            "Apps": (self.m_apps, "s", True),
            "SetTheme": (self.m_set_theme, "s", True),
            "SetOption": (self.m_set_option, "s", True),
            "SetWarm": (self.m_set_warm, "s", True),
            "ConnectionChanged": (self.m_connection, None, True),
            "Credentials": (self.m_credentials, "s", True),
            "InstallGuestHelper": (self.m_install_guest, "s", True),
            "GuestRequest": (self.m_guest_request, "s", True),
            "SetupStart": (self.m_setup_start, "s", True),
            "SetupControl": (self.m_setup_control, "s", True),
            "Firmware": (self.m_firmware, "s", True),
            "SetProductKey": (self.m_set_product_key, "s", True),
        }, authorize=self.authorize)
        self.power = hostinfo.Power(bus, self.host_changed)
        self.pressure = hostinfo.PressureWatch(self.host_changed)
        self.watch_libvirt()
        self.bind_folders()
        if virsh.state(self.domain) != "":
            ok, why = firewall.apply(self.cfg, self.state.get("file_sharing", False))
            if not ok:
                log("firewall:", why)
        self.setup = setup.Setup(self)
        self.refresh()
        self.setup_changed()
        self.setup.resume_after_restart()
        GLib.timeout_add_seconds(30, self.slow_check)
        bus.signal_subscribe("org.freedesktop.login1", "org.freedesktop.login1.Manager", "PrepareForShutdown",
                             "/org/freedesktop/login1", None, Gio.DBusSignalFlags.NONE, self.preparing_shutdown)

    def preparing_shutdown(self, conn, sender, path, iface, signal, params):
        if params.unpack()[0]:
            threading.Thread(target=self.shut_down_windows, name="shutdown", daemon=True).start()

    def shut_down_windows(self):
        """libvirt holds the PC's shutdown back while Windows runs, and only
        lets go when logind's InhibitDelayMaxSec runs out (30 s); then
        libvirt-guests shuts Windows down. Shutting it down as soon as the PC
        starts to removes that wait: libvirt lets go when Windows is off,
        about ten seconds in, under the shutdown screen. A setup in progress
        is left to libvirt-guests, so setup carries on after the restart as
        it was."""
        if self.setup.thread and self.setup.thread.is_alive():
            return
        state = virsh.state(self.domain)
        if state not in ("running", "paused"):
            return
        log("the PC is shutting down: shutting Windows down")
        try:
            if state == "paused":
                virsh.resume(self.domain)
            virsh.run("shutdown", self.domain, timeout=15)
        except virsh.VirshError as e:
            log("shutdown:", e)
        # Inside logind's 30 s, so libvirt never holds the PC to the limit.
        deadline = time.monotonic() + 22
        while time.monotonic() < deadline:
            if virsh.state(self.domain) not in ("running", "paused", "in shutdown"):
                return
            time.sleep(0.5)
        log("Windows did not shut down in time: turning it off")
        virsh.run("destroy", self.domain, check=False, timeout=15)

    def save_state(self):
        try:
            config.write_json(config.state_file(), self.state, 0o600)
        except OSError as e:
            log("state:", e)

    def changed(self):
        self.export.emit("Changed")

    def notify(self):
        """From the setup worker's thread: setup's progress changed."""
        GLib.idle_add(self.setup_changed)

    def setup_changed(self):
        # Windows is never paused or saved while it is being set up.
        running = bool((self.state.get("setup") or {}).get("running"))
        self.life.hold(time.monotonic(), "setup", running)
        self.schedule_idle()
        self.changed()
        return False

    def new_guest_password(self):
        """The Windows account's first password, for the answer file. Nobody
        keeps it: the person's agent sets a new one through the guest agent
        the first time it connects (Credentials) and keeps that one."""
        return random_password()

    # Authorisation: root always; anyone else through polkit, which lets
    # the active local user in without a password.
    def authorize(self, sender, reply):
        cached = self.auth_cache.get(sender)
        if cached and cached[1] > time.monotonic():
            reply(cached[0])
            return

        def got_uid(conn, res):
            try:
                uid = conn.call_finish(res).unpack()[0]
            except GLib.Error:
                reply(False)
                return
            if uid == 0:
                self.auth_cache[sender] = (True, time.monotonic() + 60)
                reply(True)
                return
            subject = ("system-bus-name", {"name": GLib.Variant("s", sender)})
            self.bus.call("org.freedesktop.PolicyKit1", "/org/freedesktop/PolicyKit1/Authority",
                          "org.freedesktop.PolicyKit1.Authority", "CheckAuthorization",
                          GLib.Variant("((sa{sv})sa{ss}us)", (subject, config.POLKIT_ACTION, {}, 1, "")),
                          GLib.VariantType("((bba{ss}))"), Gio.DBusCallFlags.NONE, 60000, None, got_polkit)

        def got_polkit(conn, res):
            try:
                ok = bool(conn.call_finish(res).unpack()[0][0])
            except GLib.Error as e:
                log("polkit:", e.message)
                ok = False
            self.auth_cache[sender] = (ok, time.monotonic() + 60)
            reply(ok)

        self.bus.call("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus", "GetConnectionUnixUser",
                      GLib.Variant("(s)", (sender,)), GLib.VariantType("(u)"), Gio.DBusCallFlags.NONE, 5000, None, got_uid)

    def sender_uid(self, sender):
        try:
            return self.bus.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus",
                                      "GetConnectionUnixUser", GLib.Variant("(s)", (sender,)), GLib.VariantType("(u)"),
                                      Gio.DBusCallFlags.NONE, 5000, None).unpack()[0]
        except GLib.Error:
            return None

    # One person's Windows mode. Windows Pro gives its account one remote
    # session and a new connection takes it over, and the shared drive holds
    # one person's folders, so the first person to use it owns it. Root (the
    # command line, setup) is let through.
    def claim(self, sender):
        """None if the caller may use Windows mode, else why not."""
        uid = self.sender_uid(sender)
        if uid is None:
            return "Windows mode couldn't tell who is asking."
        if uid == 0:
            return None
        owner = self.state.get("owner_uid")
        if owner is None:
            # Nothing to own until Windows mode is set up.
            if virsh.state(self.domain) == "":
                return None
            self.state["owner_uid"] = uid
            self.save_state()
            log(f"Windows mode now belongs to uid {uid}")
            self.bind_folders()
            return None
        if owner != uid:
            try:
                name = pwd.getpwuid(owner).pw_gecos.split(",")[0] or pwd.getpwuid(owner).pw_name
            except KeyError:
                name = "another account"
            return f"Windows mode on this PC is set up for {name}."
        return None

    def bind_folders(self):
        """Nothing to do: the shared folders reach Windows through the
        RemoteApp connection's drive redirection (the agent's FreeRDP shares
        them, wild-guest maps them to Z:), not through mounts made here.
        virtiofs failed on real hardware: reads broke partway and a save to
        disk would not load back."""

    def runtime_installed(self):
        """From setup's thread, once the virtual PC package is in: until then
        there was no virsh to watch."""
        GLib.idle_add(lambda: (self.events is None and self.watch_libvirt(), False)[1])

    # What libvirt and logind say.
    def watch_libvirt(self):
        """virsh event streams lifecycle events, so state changes arrive as
        they happen with nothing polling."""
        try:
            self.events = subprocess.Popen(["virsh", "-q", "-c", virsh.URI, "event", "--all", "--loop", "--event", "lifecycle"],
                                           stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
        except OSError:
            self.events = None
            return
        os.set_blocking(self.events.stdout.fileno(), False)

        def readable(fd, cond):
            try:
                data = os.read(fd, 4096)
            except BlockingIOError:
                return True
            if not data:
                GLib.timeout_add_seconds(5, lambda: (self.watch_libvirt(), False)[1])
                return False
            self.refresh()
            return True

        GLib.io_add_watch(self.events.stdout.fileno(), GLib.PRIORITY_DEFAULT, GLib.IO_IN | GLib.IO_HUP, readable)

    def host_changed(self):
        action = self.life.reconsider_paused(self.state["when_idle"], self.power.on_battery, *hostinfo.meminfo(),
                                             hostinfo.under_pressure())
        # A wake or anything else holding Windows awake comes first.
        if action == lifecycle.SAVE and SAVE_TO_DISK and not self.busy_action and not self.life.busy_reasons:
            log("host needs the memory or is on battery: saving the paused guest to disk")
            self.run_idle(lifecycle.SAVE)

    def slow_check(self):
        """Every 30 s: the Automatic policy's inputs, the guest channel, and
        the window list, read afresh in case an update was lost (the guest
        drops what it cannot send while the channel reconnects)."""
        self.host_changed()
        if virsh.state(self.domain) == "running":
            self.trim_memory()
            if not self.link.connected:
                self.connect_guest()
            else:
                self.link.request({"type": "windows"}, self.guest_event)
        return True

    def trim_memory(self):
        """Windows keeps the memory it was given even when it isn't using
        it (free page reporting only works with Linux guests), which left a
        16 GB PC with 8 GB tied up and the desktop in swap. Every 30 s the
        balloon is set to what Windows uses plus headroom; a launch gives it
        everything back first (wake), so apps never start short."""
        if self.busy_action or time.monotonic() - self.last_wake < TRIM_AFTER:
            return
        try:
            stats = virsh.memory_stats(self.domain)
        except virsh.VirshError:
            return
        actual, usable = stats.get("actual"), stats.get("usable")
        if not actual or usable is None or not self.memory:
            return
        ceiling = self.memory // 1024
        used = actual - usable
        target = max(TRIM_FLOOR_KIB, min(ceiling, used + TRIM_HEADROOM_KIB))
        # Small moves cost more than they save; growing is never held back.
        if target < actual - TRIM_STEP_KIB or target > actual:
            try:
                virsh.set_memory(self.domain, target)
            except virsh.VirshError as e:
                log("memory:", e)

    def full_memory(self):
        if self.memory:
            try:
                virsh.set_memory(self.domain, self.memory // 1024)
            except virsh.VirshError as e:
                log("memory:", e)

    # State.
    def refresh(self):
        st = virsh.state(self.domain)
        prev = self.life.vm_state
        now = time.monotonic()
        self.life.update(now, vm_state=st)
        if st == "running":
            if prev != "running":
                self.await_window_list(True)
            self.connect_guest()
        elif prev == "running" and st != "running":
            self.await_window_list(False)
            self.link.close()
            if st != "paused":
                self.set_windows({})
        self.schedule_idle()
        if st != prev:
            self.changed()

    def await_window_list(self, waiting):
        if self.window_list_timer:
            GLib.source_remove(self.window_list_timer)
            self.window_list_timer = None
        if waiting:
            def gave_up():
                self.window_list_timer = None
                log(f"wild-guest sent no window list in {WINDOW_LIST_WAIT} s; going by what is known")
                self.await_window_list(False)
                return False
            self.window_list_timer = GLib.timeout_add_seconds(WINDOW_LIST_WAIT, gave_up)
        self.life.hold(time.monotonic(), WINDOW_LIST_HOLD, waiting)
        self.schedule_idle()

    def connect_guest(self):
        if self.link.sock is not None:
            if not self.link.connected:
                self.link.request({"type": "hello"}, self.link._hello, timeout=5)
            return
        path = virsh.channel_path(self.domain, "org.wildos.guest.0")
        if path:
            self.link.connect(path)

    def guest_state(self, connected):
        log("wild-guest", "connected" if connected else "disconnected",
            (self.link.hello or {}).get("version", "") if connected else "")
        if connected:
            edition = (self.link.hello or {}).get("edition")
            if isinstance(edition, str) and edition != self.state.get("edition"):
                self.state["edition"] = edition[:40]
                self.save_state()
            theme = self.state.get("theme")
            if theme:
                self.link.send(dict(theme, type="theme"))
            warm = self.state.get("warm")
            if warm:
                self.link.send(dict(warm, type="warm"))
            self.link.request({"type": "windows"}, self.guest_event)
        self.changed()

    def guest_event(self, msg):
        t = msg.get("type")
        if t == "windows":
            session = msg.get("session")
            ws = msg.get("windows")
            if isinstance(ws, list):
                windows = dict(self.windows)
                if session is None:
                    windows = {}
                else:
                    windows[session] = ws
                self.set_windows(windows)
                if WINDOW_LIST_HOLD in self.life.busy_reasons:
                    self.await_window_list(False)
        elif t == "session" and msg.get("state") == "down":
            windows = dict(self.windows)
            windows.pop(msg.get("session"), None)
            self.set_windows(windows)

    def all_windows(self):
        return [w for ws in self.windows.values() for w in app_windows(ws)]

    def set_windows(self, windows):
        before = self.all_windows()
        self.windows = windows
        after = self.all_windows()
        self.life.update(time.monotonic(), windows=len(after))
        self.schedule_idle()
        if before != after:
            self.export.emit("WindowsChanged", "(s)", json.dumps(after))
            self.changed()

    def schedule_idle(self):
        if self.idle_timer:
            GLib.source_remove(self.idle_timer)
            self.idle_timer = None
        d = self.life.deadline()
        if d is None:
            return
        wait = max(0, d - time.monotonic())
        self.idle_timer = GLib.timeout_add(int(wait * 1000) + 50, self.idle_fired)

    def idle_fired(self):
        self.idle_timer = None
        if self.life.due(time.monotonic()) and not self.busy_action:
            action = lifecycle.idle_action(self.state["when_idle"], self.power.on_battery, *hostinfo.meminfo(),
                                           hostinfo.under_pressure())
            log(f"no Windows app open for {self.cfg['idle_seconds']} s: {action}")
            self.run_idle(action)
        return False

    def run_idle(self, action, done=None):
        # Saving to disk is off: on a test PC (libvirt 11.3, virtiofs
        # attached) the save was written but would not load back, which
        # left Windows unable to start. Paused in memory it resumes in a
        # second and loses nothing; saving comes back once restore is proven.
        if action == lifecycle.SAVE and not SAVE_TO_DISK:
            action = lifecycle.PAUSE
            if virsh.state(self.domain) == "paused":
                return
        self.busy_action = action

        def work():
            t0 = time.monotonic()
            if action == lifecycle.PAUSE:
                virsh.suspend(self.domain)
            else:
                self.do_save(open_windows=False)
            return time.monotonic() - t0

        def finished(result):
            self.busy_action = None
            if isinstance(result, Exception):
                self.error = str(result)
                log(f"{action} failed:", result)
            else:
                self.error = None
                self.state["last"] = {"action": action, "seconds": round(result, 2), "at": int(time.time())}
                self.save_state()
            self.refresh()
            if done:
                done(result)

        dbusutil.in_thread(work, done=finished)

    def do_save(self, open_windows):
        """virsh managedsave. Held until the bare-metal transport test
        decides between virtiofs and Samba:
        libvirt before 10.6 and virtiofsd before 1.11 refuse while the
        virtiofs share is attached (phase 1, risk 10.7), and then the share
        is taken out first and put back on restore. That is only safe with
        nothing open on it, which the idle path guarantees."""
        try:
            virsh.managedsave(self.domain)
            self.state["share_detached"] = False
            return
        except virsh.VirshError as e:
            if "migrat" not in str(e).lower() and "virtiofs" not in str(e).lower():
                raise
            log("managed save refused with the shared folder attached:", e)
        if open_windows:
            log("warning: taking the shared folder out with Windows apps open; files open on it will need saving again")
        xml = self.filesystem_xml()
        if not xml:
            raise virsh.VirshError("managed save refused and no shared folder to take out")
        path = os.path.join(config.STATE_DIR, "filesystem.xml")
        with open(path, "w") as f:
            f.write(xml)
        virsh.run("detach-device", self.domain, path, "--live", timeout=30)
        deadline = time.monotonic() + 30
        while time.monotonic() < deadline and "<filesystem " in virsh.dumpxml(self.domain):
            time.sleep(0.25)
        self.state["share_detached"] = True
        self.save_state()
        try:
            virsh.managedsave(self.domain)
        except virsh.VirshError:
            # Still running: Windows must not be left without its drive.
            if virsh.state(self.domain) == "running":
                self.reattach_share()
            raise

    def filesystem_xml(self):
        xml = virsh.dumpxml(self.domain)
        start = xml.find("<filesystem ")
        end = xml.find("</filesystem>", start)
        return xml[start:end + len("</filesystem>")] if start >= 0 and end > 0 else None

    def reattach_share(self):
        path = os.path.join(config.STATE_DIR, "filesystem.xml")
        virsh.run("attach-device", self.domain, path, "--live", timeout=30)
        # The virtiofs service only mounts at start.
        drive = self.cfg["drive"]
        qga.powershell(self.domain, "Restart-Service VirtioFsSvc -Force; $d = (Get-Date).AddSeconds(60); "
                       f"while (-not (Test-Path {drive}:\\) -and (Get-Date) -lt $d) {{ Start-Sleep -Milliseconds 200 }}", 90)
        self.state["share_detached"] = False
        self.save_state()

    def reopen_guest_channel(self):
        """After a restore the guest channel has a new host end, which
        wild-guest only notices when told (its service control code 128)."""
        try:
            qga.run(self.domain, r"C:\Windows\System32\sc.exe", ["control", "WildGuest", "128"], 10)
        except (virsh.VirshError, ValueError, KeyError, TimeoutError) as e:
            log("telling wild-guest to reopen its channel:", e)
        GLib.idle_add(lambda: (self.link.close(), self.connect_guest(), False)[2])

    def wait_rdp(self, seconds):
        deadline = time.monotonic() + seconds
        while time.monotonic() < deadline:
            try:
                with socket.create_connection((self.cfg["guest_address"], self.cfg["rdp_port"]), timeout=1):
                    return True
            except OSError:
                time.sleep(0.2)
        return False

    def wake(self):
        """Gets the guest running and ready for a RemoteApp connection:
        resume, restore or boot, then the clock, the guest agent and
        Remote Desktop (brief 5.6, Resume)."""
        t0 = time.monotonic()
        self.last_wake = t0
        st = virsh.state(self.domain)
        came_from = {"running": "running", "paused": "paused", "saved": "saved", "shut off": "stopped"}.get(st, st)
        if st == "":
            raise RuntimeError("Windows mode isn't set up on this PC")
        if st == "paused":
            virsh.resume(self.domain)
        elif st in ("saved", "shut off"):
            firewall.apply(self.cfg, self.state.get("file_sharing", False))
            # virtiofsd starts with the guest and sees the binds made before it.
            self.bind_folders()
            try:
                virsh.start(self.domain)
            except virsh.VirshError as e:
                if st != "saved":
                    raise
                # A save that will not load back must not keep Windows from
                # starting at all: it starts afresh, as after a power cut,
                # and only what was open in it is lost.
                log("the saved Windows would not load; starting it afresh:", e)
                virsh.run("managedsave-remove", self.domain, check=False)
                virsh.start(self.domain)
                st, came_from = "shut off", "stopped"
                self.state["restore_failed_at"] = int(time.time())
                self.save_state()
        elif st != "running":
            raise RuntimeError(f"Windows is {st}")
        # An app is about to start: all the memory first (trim_memory takes
        # back what goes unused a minute later).
        if st in ("running", "paused"):
            self.full_memory()
        resumed = time.monotonic() - t0
        if st != "running":
            if not qga.wait(self.domain, 30 if st != "shut off" else 240):
                raise RuntimeError("Windows didn't respond after starting")
            try:
                qga.set_time(self.domain)
            except (virsh.VirshError, ValueError, KeyError) as e:
                log("guest-set-time:", e)
            if st == "saved":
                self.reopen_guest_channel()
        # Also covers a save that took the drive out and then failed. A
        # fresh start has it back already, from the guest's definition.
        if self.state.get("share_detached"):
            if st == "shut off":
                self.state["share_detached"] = False
                self.save_state()
            else:
                self.reattach_share()
        if not self.wait_rdp(30 if st != "shut off" else 240):
            raise RuntimeError("Windows isn't accepting the connection")
        return {"ok": True, "from": came_from, "seconds": round(time.monotonic() - t0, 2), "resume_seconds": round(resumed, 2)}

    def qemu_usage(self):
        """QEMU's CPU time and memory, for Task Manager: a root process that
        the person's own tools may not be allowed to see in /proc."""
        try:
            with open(f"/run/libvirt/qemu/{self.domain}.pid") as f:
                pid = int(f.read().strip())
            with open(f"/proc/{pid}/stat") as f:
                fields = f.read().rsplit(")", 1)[1].split()
            ticks = os.sysconf("SC_CLK_TCK")
            cpu = (int(fields[11]) + int(fields[12])) / ticks
            rss = 0
            with open(f"/proc/{pid}/status") as f:
                for line in f:
                    if line.startswith("VmRSS:"):
                        rss = int(line.split()[1]) * 1024
            return {"pid": pid, "cpu_seconds": round(cpu, 2), "rss_bytes": rss}
        except (OSError, ValueError, IndexError):
            return None

    # D-Bus methods.
    def status(self):
        # libvirt's events keep this current; asking virsh on every call
        # would start a process each time Task Manager refreshes.
        st = self.life.vm_state or virsh.state(self.domain)
        d = self.life.deadline()
        return {
            "state": lifecycle.state_from_vm(st, self.state.get("setting_up", False), self.error if st in ("", "crashed") else None),
            "vm": st,
            "windows": self.all_windows(),
            "connection": self.life.connection,
            "busy": sorted(self.life.busy_reasons),
            "idle_in": None if d is None else max(0, round(d - time.monotonic(), 1)),
            "when_idle": self.state["when_idle"],
            "on_battery": self.power.on_battery,
            "guest": {"connected": self.link.connected, "version": (self.link.hello or {}).get("version"),
                      "sessions": (self.link.hello or {}).get("sessions")},
            "edition": self.state.get("edition"),
            "share_detached": self.state.get("share_detached", False),
            "last": self.state.get("last"),
            "action": self.busy_action,
            "error": self.error,
            "computer_name": self.cfg["computer_name"] or self.state.get("computer_name", ""),
            "account": self.cfg["account"],
            "guest_address": self.cfg["guest_address"],
            "rdp_port": self.cfg["rdp_port"],
            "drive": self.cfg["drive"],
            "memory_bytes": self.memory,
            "disk_bytes": paths.used_bytes(setup.DISK),
            "idle_seconds": self.cfg["idle_seconds"],
            "qemu": self.qemu_usage() if st in ("running", "paused") else None,
            "share_root": self.cfg["share_root"],
            "folders": self.cfg["folders"],
            "setup": self.setup.status(),
        }

    def m_status(self, sender):
        return json.dumps(self.status())

    def m_wake(self, sender, defer):
        why = self.claim(sender)
        if why:
            return json.dumps({"ok": False, "error": why, "message": why})
        now = time.monotonic()
        self.life.hold(now, LAUNCH_HOLD, True)
        self.schedule_idle()

        def finished(result):
            # The agent opens its connection next; the hold stays a little
            # longer so the guest is not paused in between.
            GLib.timeout_add_seconds(20, lambda: (self.release_launch(), False)[1])
            self.refresh()
            if isinstance(result, Exception):
                defer.done(json.dumps({"ok": False, "error": str(result)}))
            else:
                self.state["last"] = {"action": "wake", "seconds": result["seconds"], "from": result["from"], "at": int(time.time())}
                self.save_state()
                defer.done(json.dumps(result))

        def work():
            # A pause or save already under way finishes first.
            while self.busy_action:
                time.sleep(0.1)
            return self.wake()

        dbusutil.in_thread(work, done=finished)
        return defer

    def release_launch(self):
        self.life.hold(time.monotonic(), LAUNCH_HOLD, False)
        self.schedule_idle()

    def m_idle(self, sender, mode, defer):
        """Pause or save now, for testing and for the command line."""
        if mode not in (lifecycle.PAUSE, lifecycle.SAVE):
            return json.dumps({"ok": False, "error": "mode is pause or save"})
        if virsh.state(self.domain) != "running":
            return json.dumps({"ok": False, "error": "not running"})
        self.run_idle(mode, done=lambda r: defer.done(json.dumps(
            {"ok": False, "error": str(r)} if isinstance(r, Exception) else {"ok": True, "seconds": round(r, 2)})))
        return defer

    def guest_request(self, msg, defer, timeout=15):
        if not self.link.connected:
            self.connect_guest()
            return json.dumps({"type": "error", "error": "not-connected"})
        self.link.request(msg, lambda reply: defer.done(json.dumps(reply)), timeout=timeout)
        return defer

    def m_launch(self, sender, path, args, cwd, defer):
        msg = {"type": "launch", "path": path, "args": list(args)}
        if cwd:
            msg["cwd"] = cwd
        return self.guest_request(msg, defer)

    def m_windows(self, sender):
        return json.dumps(self.all_windows())

    def m_window(self, sender, hwnd, defer):
        return self.guest_request({"type": "window", "hwnd": hwnd}, defer, 5)

    def m_apps(self, sender, icons, defer):
        return self.guest_request({"type": "apps", "icons": bool(icons)}, defer, 120)

    def m_set_theme(self, sender, theme_json):
        try:
            theme = json.loads(theme_json)
            theme = {"dark": bool(theme.get("dark")), "accent": str(theme.get("accent", ""))[:7]}
        except (ValueError, AttributeError):
            return "That theme couldn't be read."
        if theme == self.state.get("theme"):
            return ""
        self.state["theme"] = theme
        self.save_state()
        if self.link.connected:
            self.link.send(dict(theme, type="theme"))
        return ""

    def m_set_warm(self, sender, warm_json):
        """Which apps wild-guest keeps warm (brief 5.5a), from the agent:
        {"apps": [{"id", "path"}], "limit": n}."""
        try:
            warm = json.loads(warm_json)
            apps = [{"id": str(a.get("id", ""))[:40], "path": str(a["path"])[:400]} for a in warm.get("apps", [])[:8]]
            warm = {"apps": apps, "limit": max(0, min(int(warm.get("limit", 0)), 6))}
        except (ValueError, AttributeError, KeyError, TypeError):
            return "That list couldn't be read."
        if warm != self.state.get("warm"):
            self.state["warm"] = warm
            self.save_state()
            if self.link.connected:
                self.link.send(dict(warm, type="warm"))
        return ""

    def m_set_option(self, sender, key, value):
        if key == "whenIdle":
            key = "when_idle"
        allowed = config.OPTION_KEYS.get(key)
        if not allowed or value not in allowed:
            return "That setting isn't available."
        self.state[key] = value
        self.save_state()
        self.changed()
        return ""

    def m_connection(self, sender, is_open):
        self.life.update(time.monotonic(), connection=bool(is_open))
        self.schedule_idle()
        self.changed()

    def m_credentials(self, sender, defer):
        """A new password for the Windows account, set through the guest
        agent and handed to the person's agent, which keeps it in their
        keyring (brief section 4). Nothing keeps a copy here."""
        why = self.claim(sender)
        if why:
            defer.error(why)
            return defer
        account = self.cfg["account"]

        def work():
            if virsh.state(self.domain) != "running":
                self.wake()
            pw = random_password()
            qga.set_password(self.domain, account, pw)
            # A PC set up by the earlier proof script keeps its copy in step, so
            # that still connects.
            legacy = os.path.join(config.STATE_DIR, "secrets", "guest-password")
            if os.path.exists(legacy):
                with open(legacy, "w") as f:
                    f.write(pw)
            return pw

        def finished(result):
            if isinstance(result, Exception):
                defer.error(f"couldn't set the Windows password: {result}")
            else:
                defer.done(json.dumps({"account": account, "password": result,
                                       "domain": self.cfg["computer_name"] or self.state.get("computer_name", "")}))

        dbusutil.in_thread(work, done=finished)
        return defer

    def m_setup_start(self, sender, request, defer):
        """Sets up Windows mode for whoever asked, who then owns it."""
        try:
            req = json.loads(request)
            if not isinstance(req, dict):
                raise ValueError
        except ValueError:
            return "That request couldn't be read."
        uid = self.sender_uid(sender)
        if uid is None:
            return "Windows mode couldn't tell who is asking."
        owner = self.state.get("owner_uid")
        if uid != 0 and owner is not None and owner != uid:
            return self.claim(sender) or "Windows mode on this PC is set up for another account."
        if uid != 0:
            self.state["owner_uid"] = uid
            self.save_state()

        def work():
            self.bind_folders()
            return self.setup.start(req, uid)

        dbusutil.in_thread(work, done=lambda r: defer.done(f"Windows mode couldn't start setting up. ({r})"
                                                         if isinstance(r, Exception) else r))
        return defer

    def m_setup_control(self, sender, action, argument, defer):
        """pause, resume, cancel, or iso with a file's path."""
        actions = {"pause": lambda: self.setup.pause() or "", "resume": lambda: self.setup.resume() or "",
                   "cancel": lambda: self.setup.cancel() or "", "iso": lambda: self.setup.choose_iso(argument)}
        if action not in actions:
            return "That isn't something setup can do."
        dbusutil.in_thread(actions[action], done=lambda r: defer.done(str(r) if isinstance(r, Exception) else r))
        return defer

    def m_set_product_key(self, sender, key, defer):
        """Settings' Add a key (board 9.8): installs a Windows product key,
        or this PC's own built-in one when the key is "firmware", and
        activates. "" when Windows is activated, or why not."""
        dbusutil.in_thread(lambda: self.setup.set_key(key),
                           done=lambda r: defer.done(str(r) if isinstance(r, Exception) else r))
        return defer

    def m_firmware(self, sender, defer):
        """Board 9.1's licence row: whether this PC's firmware has a key and
        Windows is still on its disks. The key itself never leaves here."""
        dbusutil.in_thread(setup.firmware, done=lambda r: defer.done(
            json.dumps({"error": str(r)} if isinstance(r, Exception) else r)))
        return defer

    def m_guest_request(self, sender, message, defer):
        """Any wild-guest request, answered with its reply: for tests and
        troubleshooting."""
        try:
            msg = json.loads(message)
        except ValueError:
            return json.dumps({"type": "error", "error": "not JSON"})
        if not isinstance(msg, dict) or not isinstance(msg.get("type"), str):
            return json.dumps({"type": "error", "error": "not a request"})
        return self.guest_request(msg, defer, 30)

    def m_install_guest(self, sender, defer):
        """Puts this package's wild-guest into Windows and restarts it: the
        setup payload carries it for new installs, this updates one that
        exists."""
        exe = self.cfg["guest_exe"]
        self.life.hold(time.monotonic(), "install", True)

        def work():
            with open(exe, "rb") as f:
                data = f.read()
            if virsh.state(self.domain) != "running":
                self.wake()
            target = r"C:\Program Files\Wild OS\wild-guest.exe"
            qga.powershell(self.domain, "New-Item -ItemType Directory -Force 'C:\\Program Files\\Wild OS' | Out-Null", 60)
            qga.write_file(self.domain, target + ".new", data)
            code, out, err = qga.powershell(self.domain, INSTALL_GUEST_PS, 90)
            return {"ok": code == 0, "output": (out + err).strip()[-2000:]}

        def finished(result):
            self.life.hold(time.monotonic(), "install", False)
            self.schedule_idle()
            defer.done(json.dumps({"ok": False, "error": str(result)} if isinstance(result, Exception) else result))

        dbusutil.in_thread(work, done=finished)
        return defer


# The same registration setup.ps1 does, for a guest that already exists.
INSTALL_GUEST_PS = r"""
$ErrorActionPreference = 'Stop'
$dir = 'C:\Program Files\Wild OS'
# A running program can be renamed but not replaced, and the service
# restarts itself if stopped, so the old copy is moved aside instead.
Get-ChildItem "$dir\wild-guest.*.old" -ErrorAction SilentlyContinue | Remove-Item -Force -ErrorAction SilentlyContinue
if (Test-Path "$dir\wild-guest.exe") { Rename-Item -Force "$dir\wild-guest.exe" ("wild-guest.{0}.old" -f (Get-Date -Format yyyyMMddHHmmss)) }
Rename-Item -Force "$dir\wild-guest.exe.new" 'wild-guest.exe'
if (-not (Get-Service WildGuest -ErrorAction SilentlyContinue)) {
    New-Service -Name WildGuest -DisplayName 'Wild OS guest helper' -BinaryPathName "`"$dir\wild-guest.exe`" --service" -StartupType Automatic | Out-Null
    sc.exe failure WildGuest reset= 60 actions= restart/2000/restart/5000/restart/30000 | Out-Null
}
Get-Process wild-guest -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep -Milliseconds 500
Start-Service WildGuest -ErrorAction SilentlyContinue
(Get-Service WildGuest).Status.ToString()
"""


def main():
    loop = GLib.MainLoop()
    bus = Gio.bus_get_sync(Gio.BusType.SYSTEM, None)
    # The object exists before the name does: D-Bus delivers the call that
    # started this service as soon as the name is taken, and it must find it.
    service = Service(bus)

    def lost(conn, name):
        log(f"winmoded: lost {name}")
        loop.quit()

    Gio.bus_own_name_on_connection(bus, config.BUS_NAME, Gio.BusNameOwnerFlags.NONE,
                                   lambda conn, name: log(f"winmoded: serving {name}"), lost)
    GLib.unix_signal_add(GLib.PRIORITY_HIGH, 15, loop.quit)
    loop.run()
    # Held until here, so it lives as long as the loop.
    del service
    return 0
