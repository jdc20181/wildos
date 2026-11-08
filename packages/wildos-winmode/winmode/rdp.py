"""The one RemoteApp connection (brief 5.5, phase 1 risk 10.4).

Windows Pro gives each account one remote session, and a second connection
takes it over, so all Windows apps share one FreeRDP 3 connection. It is
opened with the first app; every later app is started inside the session
by wild-guest. Its first program is wild-guest's "--start", which starts
the app and ends, because FreeRDP's /app option splits on commas and
cannot carry a file name that has one.
"""

import base64
import json
import os
import signal
import subprocess

from gi.repository import GLib

GUEST_EXE = r"C:\Program Files\Wild OS\wild-guest.exe"
AUTH_FAILED = ("LOGON_FAILURE", "AUTHENTICATION_FAILED", "ERRCONNECT_LOGON_FAILURE", "STATUS_LOGON_FAILURE",
               "ERRCONNECT_PASSWORD")


def start_argument(path, args, cwd=None):
    msg = {"path": path, "args": list(args)}
    if cwd:
        msg["cwd"] = cwd
    return base64.b64encode(json.dumps(msg, separators=(",", ":")).encode()).decode()


def plain(text):
    """Text that can only ever be one FreeRDP argument: one per line on
    standard input, so a line break in a file name would start a new one
    (/drive:all, say), and /app splits on commas."""
    return "".join(" " if ch < " " or ch in ",\x7f" else ch for ch in str(text))


def display_scale():
    """The scale Plasma draws X11 apps at, so Windows lays its apps out at
    the same size as the rest of the desktop. KWin scales XWayland itself
    and leaves the factor in kwinrc's [Xwayland] Scale."""
    import configparser
    cfg = configparser.ConfigParser(interpolation=None, strict=False)
    try:
        cfg.read(os.path.join(os.environ.get("XDG_CONFIG_HOME") or os.path.expanduser("~/.config"), "kwinrc"))
        return max(1.0, min(5.0, float(cfg.get("Xwayland", "Scale", fallback="1"))))
    except (ValueError, configparser.Error):
        return 1.0


def scale_arguments(scale):
    """Windows' scale for the session (any whole percentage FreeRDP takes,
    100 to 500), and the nearest of the three device scales RDP has."""
    device = 180 if scale >= 1.6 else 140 if scale >= 1.2 else 100
    return [f"/scale-desktop:{round(scale * 100)}", f"/scale-device:{device}"]


def arguments(host, port, account, domain, name, path, args, cwd=None, drive=None):
    """FreeRDP's arguments, without the password, which goes after them on
    standard input. Every argument goes on standard input (/args-from:stdin),
    so nothing secret is on a command line. The path and arguments travel
    base64 encoded, so only the name, account and domain need cleaning."""
    app = f"program:{GUEST_EXE},name:{plain(name)},cmd:--start {start_argument(path, args, cwd)}"
    out = [f"/v:{host}:{port}", f"/u:{plain(account)}"]
    if domain:
        out.append(f"/d:{plain(domain)}")
    out += [
        # The guest's certificate is self-signed and only reached on the
        # isolated network: trust it the first time, refuse it if it changes.
        "/cert:tofu",
        "/sec:nla",
        # Kerberos cannot work without a domain controller; trying it first
        # costs time and logs errors (phase 1).
        "/auth-pkg-list:!kerberos",
        f"/app:{app}",
        "+clipboard",
        "/sound:sys:pulse",
        "+auto-reconnect",
        *drive_arguments(drive),
        *scale_arguments(display_scale()),
        "/log-level:WARN",
    ]
    return out


def drive_arguments(drive):
    """The shared folders, as the redirected drive wild-guest maps to Z:."""
    return [f"/drive:wildos,{drive}"] if drive else []


def desktop_arguments(host, port, account, domain, drive=None):
    """The whole Windows desktop in a window of its own, for Settings'
    Windows desktop button. It takes the one session over, so the app
    windows go into it until it closes."""
    out = [f"/v:{host}:{port}", f"/u:{plain(account)}"]
    if domain:
        out.append(f"/d:{plain(domain)}")
    return out + [
        "/cert:tofu",
        "/sec:nla",
        "/auth-pkg-list:!kerberos",
        "/title:Windows desktop",
        "/dynamic-resolution",
        *drive_arguments(drive),
        "+clipboard",
        "/sound:sys:pulse",
        *scale_arguments(display_scale()),
        "/log-level:WARN",
    ]


class Connection:
    """One xfreerdp3 process. on_change(open) runs when it opens or ends;
    on_auth_failed runs when Windows refused the password."""

    def __init__(self, on_change, on_auth_failed, log_path):
        self.proc = None
        self.on_change = on_change
        self.on_auth_failed = on_auth_failed
        self.log_path = log_path
        self.started = None

    @property
    def open(self):
        return self.proc is not None and self.proc.poll() is None

    def start(self, args, password, env=None):
        os.makedirs(os.path.dirname(self.log_path), exist_ok=True)
        log = open(self.log_path, "wb")
        self.proc = subprocess.Popen(["xfreerdp3", "/args-from:stdin"], stdin=subprocess.PIPE, stdout=log,
                                     stderr=subprocess.STDOUT, env=env, start_new_session=True)
        log.close()
        self.proc.stdin.write(("\n".join(args + [f"/p:{password}"]) + "\n").encode())
        self.proc.stdin.close()
        self.started = GLib.get_monotonic_time()
        GLib.child_watch_add(GLib.PRIORITY_DEFAULT, self.proc.pid, self._ended, self.proc)
        self.on_change(True)

    def _ended(self, pid, status, proc):
        if proc is not self.proc:
            return
        self.proc = None
        failed = False
        try:
            with open(self.log_path, "rb") as f:
                f.seek(max(0, os.path.getsize(self.log_path) - 8192))
                tail = f.read().decode("utf-8", "replace")
            failed = any(word in tail for word in AUTH_FAILED)
        except OSError:
            pass
        self.on_change(False)
        if failed:
            self.on_auth_failed()

    def stop_now(self):
        """For the agent's own exit, when no main loop is left to finish
        close()'s work."""
        if self.open:
            proc = self.proc
            for sig, wait in ((signal.SIGTERM, 3), (signal.SIGKILL, 2)):
                try:
                    os.killpg(proc.pid, sig)
                    proc.wait(wait)
                    return
                except subprocess.TimeoutExpired:
                    continue
                except OSError:
                    return

    def close(self):
        if self.open:
            proc = self.proc
            try:
                os.killpg(proc.pid, signal.SIGTERM)
            except OSError:
                return
            # FreeRDP has been seen to catch SIGTERM and never exit, leaving
            # its windows frozen on screen; three seconds is plenty to say
            # goodbye to Windows.
            def kill():
                if proc.poll() is None:
                    try:
                        os.killpg(proc.pid, signal.SIGKILL)
                    except OSError:
                        pass
                return False
            GLib.timeout_add_seconds(3, kill)
