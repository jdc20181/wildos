r"""wildos-winmode: the command behind Windows apps' Start entries, and the
agent itself.

  wildos-winmode                          set up Windows mode (the wizard, boards 9.1 to 9.7)
  wildos-winmode start-over               stop a setup under way, keeping the downloads, and open the wizard
  wildos-winmode launch <app> [file...]   open an app (Start entries, file types)
  wildos-winmode launch-program <C:\path> [arg...]  open any Windows program (testing)
  wildos-winmode run-installer <file>      run a Windows installer in Windows mode
  wildos-winmode open <file>              open a Windows shortcut (.lnk) in Windows mode
  wildos-winmode agent                     the per-user agent (user unit)
  wildos-winmode status                    the agent's and the system service's status
  wildos-winmode refresh-apps              make Start entries for Windows' apps again
  wildos-winmode windows                   the Windows windows open now
  wildos-winmode idle pause|save           apply the idle policy now (testing)
  wildos-winmode launch-stats [--check]    launch times by state, 50th and 95th percentile;
                                           --check fails when a 95th percentile misses its target
  wildos-winmode install-guest             put this package's wild-guest into Windows
  wildos-winmode guest '<json>'            send wild-guest one request (troubleshooting)
"""

import json
import os
import shutil
import subprocess
import sys
from urllib.parse import unquote, urlparse

from gi.repository import Gio, GLib

from . import config


def call(bus_type, interface, method, args=None, signature=None, timeout=600):
    bus = Gio.bus_get_sync(bus_type, None)
    reply = bus.call_sync(config.BUS_NAME, config.OBJECT_PATH, interface, method,
                          GLib.Variant(signature, args) if signature else None, None,
                          Gio.DBusCallFlags.NONE, timeout * 1000, None)
    values = reply.unpack()
    return values[0] if values else None


def agent(method, args=None, signature=None, timeout=600):
    return call(Gio.BusType.SESSION, config.AGENT_INTERFACE, method, args, signature, timeout)


def host(method, args=None, signature=None, timeout=600):
    return call(Gio.BusType.SYSTEM, config.HOST_INTERFACE, method, args, signature, timeout)


def local_path(arg):
    if arg.startswith("file:"):
        return unquote(urlparse(arg).path)
    return os.path.abspath(arg)


def ask(question):
    """Yes or no from the person, in a plain dialog. None when no dialog
    tool is there to ask with."""
    for tool, cmd in (("kdialog", ["kdialog", "--title", "Windows mode", "--yesno", question]),
                      ("zenity", ["zenity", "--question", "--title=Windows mode", "--text", question])):
        if shutil.which(tool):
            return subprocess.run(cmd).returncode == 0
    return None


def tell(sentence):
    print(sentence, file=sys.stderr)
    if shutil.which("notify-send"):
        subprocess.run(["notify-send", "--app-name=Windows mode", "--icon=wildos-windows-app", "Windows mode", sentence])


def cmd_launch(app, files):
    files = [local_path(f) for f in files]
    outside = agent("OutsideSharedFolders", (files,), "(as)") if files else []
    if outside:
        names = ", ".join(os.path.basename(f) for f in outside)
        answer = ask(f"Windows apps can only open files in your shared folders. Copy {names} into Documents and open the copy?")
        if not answer:
            if answer is None:
                tell("Windows apps can only open files in your shared folders. Move it to Documents, then try again.")
            return 1
        copies = agent("CopyIntoDocuments", (outside,), "(as)")
        if "" in copies:
            tell("The file couldn't be copied to Documents. Check there's space on this PC.")
            return 1
        swap = dict(zip(outside, copies))
        files = [swap.get(f, f) for f in files]
    sentence = agent("Launch", (app, files), "(sas)")
    if sentence:
        tell(sentence)
        return 1
    return 0


# Brief 5.5a: click to first window, at the 95th percentile.
TARGETS = {"warm-paused": 1.0, "cold-paused": 2.0, "saved": 6.0}


def percentile(values, p):
    values = sorted(values)
    if not values:
        return None
    k = (len(values) - 1) * p / 100
    lo = int(k)
    hi = min(lo + 1, len(values) - 1)
    return round(values[lo] + (values[hi] - values[lo]) * (k - lo), 3)


def launch_stats(check, path=None):
    path = path or os.path.join(config.user_data_dir(), "launches.jsonl")
    groups = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                try:
                    rec = json.loads(line)
                except ValueError:
                    continue
                groups.setdefault(rec.get("state", "unknown"), []).append(rec)
    except OSError:
        print("No launches recorded yet.")
        return 1 if check else 0
    failed = False
    for state in sorted(groups):
        recs = groups[state]
        row = {"state": state, "launches": len(recs)}
        for mark in ("connection_ready", "process_started", "first_window"):
            vals = [r[mark] for r in recs if isinstance(r.get(mark), (int, float))]
            row[mark] = {"p50": percentile(vals, 50), "p95": percentile(vals, 95)}
        target = TARGETS.get(state)
        if target is not None:
            row["target"] = target
            row["met"] = row["first_window"]["p95"] is not None and row["first_window"]["p95"] < target
            failed = failed or not row["met"]
        print(json.dumps(row))
    return 1 if check and failed else 0


SETUP_APP = "/usr/libexec/wildos/wildos-winmode-setup"


def main(argv):
    # Settings, Welcome and App Bridge all open setup by running this with
    # nothing after it.
    if not argv or argv[0] == "setup":
        os.execv(SETUP_APP, [SETUP_APP, *argv[1:]])
    if argv[0] in ("-h", "--help"):
        print(__doc__.strip())
        return 0
    cmd, rest = argv[0], argv[1:]
    try:
        if cmd == "start-over":
            # Through the agent, which the wizard reads, so it opens on the
            # state the cancel left rather than the one before.
            sentence = agent("CancelSetup", timeout=120)
            if sentence:
                tell(sentence)
                return 1
            os.execv(SETUP_APP, [SETUP_APP])
        if cmd == "agent":
            from . import agent as agent_module
            return agent_module.main()
        if cmd == "launch" and rest:
            return cmd_launch(rest[0], rest[1:])
        if cmd == "launch-program" and rest:
            sentence = agent("LaunchProgram", (rest[0], rest[1:]), "(sas)")
            if sentence:
                tell(sentence)
            return 1 if sentence else 0
        # A shortcut Windows apps leave on the shared Desktop: Windows opens
        # it, so it starts whatever it points at.
        if cmd == "open" and len(rest) == 1:
            sentence = agent("RunInstaller", (local_path(rest[0]),), "(s)")
            if sentence:
                tell(sentence)
            return 1 if sentence else 0
        if cmd == "run-installer" and len(rest) == 1:
            sentence = agent("RunInstaller", (local_path(rest[0]),), "(s)")
            if sentence:
                tell(sentence)
            return 1 if sentence else 0
        if cmd == "status":
            print(json.dumps({"agent": json.loads(agent("Status", timeout=10)),
                              "system": json.loads(host("Status", timeout=10))}, indent=1))
            return 0
        if cmd == "refresh-apps":
            sentence = agent("RefreshApps")
            print(sentence or "done")
            return 1 if sentence else 0
        if cmd == "windows":
            print(json.dumps(json.loads(host("Windows", timeout=10)), indent=1))
            return 0
        if cmd == "launch-stats":
            return launch_stats("--check" in rest)
        if cmd == "idle" and rest and rest[0] in ("pause", "save"):
            print(host("Idle", (rest[0],), "(s)"))
            return 0
        if cmd == "guest" and len(rest) == 1:
            print(host("GuestRequest", (rest[0],), "(s)"))
            return 0
        if cmd == "install-guest":
            print(host("InstallGuestHelper"))
            return 0
    except GLib.Error as e:
        tell("Windows mode isn't answering. Restart the PC, then try again.")
        print(e.message, file=sys.stderr)
        return 1
    print(__doc__.strip(), file=sys.stderr)
    return 2
