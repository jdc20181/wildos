"""Setting up Windows mode: everything from nothing to Windows ready, in the
system service (brief 5.1 to 5.4, boards 9.5 and 9.7).

The steps were first proven on a real PC on 7 November 2026: the
licence, Windows from Microsoft, the drivers, the answer file and setup
payload, the isolated network, the virtual PC, the install with setup.ps1,
activation and Microsoft 365. They run in a worker thread and report
progress in the stages board 9.5 shows:

  media     download Windows and the parts it needs, checked
  install   Windows Setup, until Windows answers its agent
  activate  slmgr /ato with the licence chosen
  tune      setup.ps1's own steps inside Windows, until it reports ready
  office    Microsoft 365, if chosen

Every step is resumable. Progress lives in the service's state file; after
a restart (of the service, or of the PC: wildos-winmode-setup-resume.service
wakes the service at boot while a setup is under way) the worker carries on
from the step it was on. Downloads continue where they stopped.
"""

import hashlib
import json
import os
import re
import shutil
import socket
import subprocess
import threading
import time
import urllib.error
import urllib.request

from . import config, firewall, msdl, qga, virsh

CACHE = os.path.join(config.STATE_DIR, "cache")
SECRETS = os.path.join(config.STATE_DIR, "secrets")
DISK = os.path.join(config.STATE_DIR, "windows.qcow2")
# Present while a setup is under way, for the unit that resumes it at boot.
MARKER = os.path.join(config.STATE_DIR, "setting-up")
TEMPLATES = os.path.join(config.DATA, "templates")

STAGES = ["media", "install", "activate", "tune", "office"]
# How much of setup each stage is, for the one progress bar, and about how
# long each takes on a typical PC, for "About N minutes left".
WEIGHTS = {"media": 25, "install": 45, "activate": 5, "tune": 10, "office": 15}
MINUTES = {"media": 15, "install": 25, "activate": 1, "tune": 10, "office": 15}

# A Windows 11 disk holds about this much once Setup has copied it.
INSTALLED_BYTES = 16 * 1000 ** 3
INSTALL_LIMIT = 2 * 3600
# Windows Setup has written this much within a few minutes of starting; a
# disk still under it after BOOT_WAIT seconds means it never started.
BOOT_WRITTEN = 200 * 1000 ** 2
BOOT_WAIT = 300
BOOT_TRIES = 3
# setup.ps1's steps, in order (guest/setup.ps1), for the "tune" stage.
GUEST_STEPS = ["guest-tools", "shared-folder", "wild-guest", "remote-desktop", "account", "tuning", "updates",
               "known-folders", "office-policies", "startup-trim", "theme", "ready-task"]

TOOL_AGENT = "wildos-winmode (+https://wildos.io)"
VIRTIO_BASE = "https://fedorapeople.org/groups/virt/virtio-win/direct-downloads"
# Microsoft's published generic installation key for Pro: installs Pro but
# never activates. Every setup installs with it, so Windows is always Pro,
# the edition with the Remote Desktop host Windows mode needs. The person's
# own key, from the firmware or typed in, goes in at the activate stage,
# where Windows turns down one for Home, instead of an hour of setup ending
# on an edition that can't work.
GENERIC_PRO_KEY = "VK7JG-NPHTM-C97JM-9MPGT-3V66T"
ACTIVATION_KEY = "activation-key"
KEY_ALPHABET = "BCDFGHJKMNPQRTVWXY2346789"
GUEST_MAC = "52:54:00:57:4f:01"
OFFICE_PRODUCTS = {"personal": "O365HomePremRetail", "family": "O365HomePremRetail",
                   "business": "O365BusinessRetail", "work": "O365ProPlusRetail"}
WORD = r"C:\Program Files\Microsoft Office\root\Office16\WINWORD.EXE"


def log(*parts):
    print("setup:", *parts, flush=True)


def is_iso(path):
    """A real disc image: big enough, with ISO 9660's signature where it
    belongs (a web page saved in its place has neither)."""
    try:
        if os.path.getsize(path) < 50 * 1000 ** 2:
            return False
        with open(path, "rb") as f:
            f.seek(0x8001)
            return f.read(5) == b"CD001"
    except OSError:
        return False


def runtime_installed():
    p = subprocess.run(["dpkg-query", "-W", "-f=${db:Status-Status}", "wildos-winmode-runtime"],
                       capture_output=True, text=True)
    return p.stdout.strip() == "installed"


class Stopped(Exception):
    """The worker was asked to stop (cancel, or the service is going)."""


class Problem(Exception):
    """A step failed in a way the person can act on. kind is what board 9.7
    shows: download, space, virtualisation or failed."""

    def __init__(self, kind, text):
        super().__init__(text)
        self.kind = kind
        self.text = text


def size_text(n):
    return f"{n / 1000 ** 3:.1f} GB" if n >= 1000 ** 3 else f"{max(1, round(n / 1000 ** 2))} MB"


# The licence (brief 2.1).
def key_shape_ok(key):
    return bool(re.fullmatch(rf"([{KEY_ALPHABET}N]{{5}}-){{4}}[{KEY_ALPHABET}N]{{5}}", key or "")) and key.count("N") <= 1


def parse_msdm(table):
    """The product key in an ACPI MSDM table: the 36 byte header, then
    version 1, reserved, data type 1, reserved, data length 29, and the key.
    None when the table isn't a valid one."""
    if len(table) < 85 or table[:4] != b"MSDM":
        return None
    length = int.from_bytes(table[4:8], "little")
    if length < 85 or length > len(table) or sum(table[:length]) % 256 != 0:
        return None
    field = lambda off: int.from_bytes(table[off:off + 4], "little")  # noqa: E731
    if field(36) != 1 or field(44) != 1 or field(52) != 29:
        return None
    key = table[56:85].decode("ascii", "replace")
    return key if key_shape_ok(key) else None


def firmware():
    """What the PC's firmware and disks say about its Windows licence, for
    board 9.1: whether there is a key, and whether Windows is still
    installed (its edition, from its registry, when hivexget can read it).
    The key itself stays here."""
    out = {"msdm": False, "windows": False, "edition": "", "description": ""}
    try:
        with open("/sys/firmware/acpi/tables/MSDM", "rb") as f:
            out["msdm"] = parse_msdm(f.read()) is not None
    except OSError:
        pass
    if os.path.exists("/boot/efi/EFI/Microsoft/Boot/bootmgfw.efi"):
        out["windows"] = True
    if shutil.which("hivexget"):
        try:
            disks = subprocess.run(["lsblk", "-nrpo", "NAME,FSTYPE"], capture_output=True, text=True, timeout=10).stdout
        except (OSError, subprocess.SubprocessError):
            disks = ""
        for line in disks.splitlines():
            parts = line.split()
            if len(parts) != 2 or parts[1] != "ntfs" or out["edition"]:
                continue
            _read_windows(parts[0], out)
    return out


def _read_windows(dev, out):
    # The kernel's own ntfs3 driver, read-only, never through FUSE, in a
    # private folder that goes away again.
    where = subprocess.run(["findmnt", "-nro", "TARGET", "-S", dev], capture_output=True, text=True).stdout.strip()
    tmp = None
    if not where:
        tmp = where = f"/run/wildos-winmode-probe-{os.getpid()}"
        os.makedirs(tmp, exist_ok=True)
        if subprocess.run(["mount", "-t", "ntfs3", "-o", "ro,nodev,nosuid,noexec", dev, tmp], capture_output=True).returncode != 0:
            os.rmdir(tmp)
            return
    try:
        hive = os.path.join(where, "Windows/System32/config/SOFTWARE")
        if os.path.isfile(hive):
            q = lambda key, value: subprocess.run(["hivexget", hive, key, value], capture_output=True, text=True,  # noqa: E731
                                                  timeout=30).stdout.strip()
            edition = q(r"\Microsoft\Windows NT\CurrentVersion", "EditionID")
            if edition:
                out.update(windows=True, edition=edition,
                           description=q(r"\Microsoft\Windows NT\CurrentVersion\SoftwareProtectionPlatform",
                                         "OA3xOriginalProductKeyDescription"))
    finally:
        if tmp:
            subprocess.run(["umount", tmp], capture_output=True)
            os.rmdir(tmp)


# This PC's language, keyboard and time zone in Windows' terms (brief 5.4).
INPUT_LOCALES = {
    "gb": "0809:00000809", "us": "0409:00000409", "ie": "1809:00001809", "de": "0407:00000407",
    "fr": "040c:0000040c", "es": "0c0a:0000040a", "it": "0410:00000410", "nl": "0413:00020409",
    "pt": "0816:00000816", "br": "0416:00000416", "se": "041d:0000041d", "no": "0414:00000414",
    "dk": "0406:00000406", "fi": "040b:0000040b", "pl": "0415:00000415", "ch": "0807:00000807",
}
TIME_ZONES = {
    "GMT Standard Time": ["Europe/London", "Europe/Dublin", "Europe/Lisbon"],
    "Romance Standard Time": ["Europe/Paris", "Europe/Brussels", "Europe/Madrid"],
    "W. Europe Standard Time": ["Europe/Berlin", "Europe/Amsterdam", "Europe/Rome", "Europe/Stockholm",
                                "Europe/Zurich", "Europe/Oslo", "Europe/Vienna"],
    "Central European Standard Time": ["Europe/Warsaw", "Europe/Prague", "Europe/Budapest"],
    "GTB Standard Time": ["Europe/Athens", "Europe/Bucharest"],
    "FLE Standard Time": ["Europe/Helsinki", "Europe/Kiev", "Europe/Kyiv", "Europe/Riga", "Europe/Tallinn",
                          "Europe/Vilnius"],
    "Eastern Standard Time": ["America/New_York", "America/Toronto", "America/Detroit"],
    "Central Standard Time": ["America/Chicago"],
    "Mountain Standard Time": ["America/Denver"],
    "Pacific Standard Time": ["America/Los_Angeles", "America/Vancouver"],
    "AUS Eastern Standard Time": ["Australia/Sydney", "Australia/Melbourne"],
    "New Zealand Standard Time": ["Pacific/Auckland"],
    "Tokyo Standard Time": ["Asia/Tokyo"],
    "India Standard Time": ["Asia/Kolkata"],
    "Singapore Standard Time": ["Asia/Singapore"],
    "Arabian Standard Time": ["Asia/Dubai"],
    "South Africa Standard Time": ["Africa/Johannesburg"],
}


def read_kv(path):
    out = {}
    try:
        with open(path, encoding="utf-8") as f:
            for line in f:
                k, sep, v = line.strip().partition("=")
                if sep:
                    out[k.strip()] = v.strip().strip('"')
    except OSError:
        pass
    return out


def host_locale():
    lang = read_kv("/etc/default/locale").get("LANG") or read_kv("/etc/locale.conf").get("LANG") or ""
    tag = lang.split(".")[0].replace("_", "-")
    if not re.fullmatch(r"[a-z]{2,3}-[A-Z]{2}", tag):
        tag = "en-GB"
    xkb = read_kv("/etc/default/keyboard").get("XKBLAYOUT", "").split(",")[0]
    keyboard = INPUT_LOCALES.get(xkb) or ("0409:00000409" if tag == "en-US" else "0809:00000809")
    try:
        tz = subprocess.run(["timedatectl", "show", "-p", "Timezone", "--value"], capture_output=True, text=True,
                            timeout=10).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        tz = ""
    windows_tz = next((w for w, names in TIME_ZONES.items() if tz in names), "UTC")
    return tag, keyboard, windows_tz


def xml_escape(s):
    return (str(s).replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
            .replace('"', "&quot;").replace("'", "&apos;"))


def render(name, values):
    with open(os.path.join(TEMPLATES, name), encoding="utf-8") as f:
        text = f.read()
    for k, v in values.items():
        text = text.replace(f"@{k}@", str(v))
    left = sorted(set(re.findall(r"@[A-Z_]+@", text)))
    if left:
        raise RuntimeError(f"template {name} has unfilled fields: {' '.join(left)}")
    return text


# The languages Microsoft 365 installs in, as the Office Deployment Tool
# names them. An ID it doesn't know stops the whole install, so anything else
# falls back to Windows' own display language.
OFFICE_LANGUAGES = {
    "ar-sa", "bg-bg", "cs-cz", "da-dk", "de-de", "el-gr", "en-gb", "en-us", "es-es", "es-mx", "et-ee",
    "fi-fi", "fr-ca", "fr-fr", "he-il", "hr-hr", "hu-hu", "id-id", "it-it", "ja-jp", "ko-kr", "lt-lt",
    "lv-lv", "nb-no", "nl-nl", "pl-pl", "pt-br", "pt-pt", "ro-ro", "ru-ru", "sk-sk", "sl-si", "sr-latn-rs",
    "sv-se", "th-th", "tr-tr", "uk-ua", "vi-vn", "zh-cn", "zh-tw",
}


def office_config(plan, lang):
    """Microsoft 365 in the host's language, with its spelling and grammar
    tools, rather than the US English MatchOS alone gave on a UK PC."""
    product = OFFICE_PRODUCTS.get(plan, "O365HomePremRetail")
    excluded = ["Access", "Bing", "Groove", "Lync", "OneDrive", "OutlookForWindows", "Publisher", "Teams"]
    office_lang = lang.lower() if lang.lower() in OFFICE_LANGUAGES else ""
    languages = (f'      <Language ID="{office_lang}"/>\n' if office_lang else "") + '      <Language ID="MatchOS"/>\n'
    proofing = ('    <Product ID="ProofingTools">\n'
                f'      <Language ID="{office_lang}"/>\n'
                "    </Product>\n") if office_lang else ""
    return ("<Configuration>\n"
            '  <Add OfficeClientEdition="64" Channel="Current">\n'
            f'    <Product ID="{product}">\n'
            + languages
            + "".join(f'      <ExcludeApp ID="{a}"/>\n' for a in excluded)
            + "    </Product>\n"
            + proofing
            + "  </Add>\n"
            '  <Updates Enabled="FALSE"/>\n'
            '  <Display Level="None" AcceptEULA="TRUE"/>\n'
            '  <Property Name="AUTOACTIVATE" Value="0"/>\n'
            '  <Property Name="FORCEAPPSHUTDOWN" Value="TRUE"/>\n'
            "</Configuration>\n")


def stage_title(stage, licence):
    source = licence.get("source")
    return {
        "media": "Download Windows 11 from Microsoft",
        "install": "Install Windows",
        "activate": ("Activate with your product key" if source == "key"
                     else "Activation" if source == "none" else "Activate with this PC's licence"),
        "tune": "Tune for speed and privacy",
        "office": "Install Microsoft 365",
    }[stage]


def stage_detail(stage, licence):
    if stage == "media":
        return "About 6 GB, checked against Microsoft's signature"
    if stage == "install":
        return "Automatic, no questions to answer"
    if stage == "activate":
        if licence.get("source") == "none":
            return "Skipped, as you chose. Add a key any time in Settings."
        return {"home": "Windows 11 Home", "pro": "Windows 11 Pro"}.get(licence.get("edition"), "Windows 11")
    if stage == "tune":
        return "Removes adverts, tracking and apps you won't use"
    return "Word, Excel, PowerPoint, Outlook, OneNote"


class Setup:
    """One setup at a time, in a worker thread. service is the system
    service: its state dict (saved with save_state), cfg, and changed()."""

    def __init__(self, service):
        self.svc = service
        self.cfg = service.cfg
        self.lock = threading.Lock()
        self.thread = None
        self.stop = threading.Event()
        self.resume_event = threading.Event()
        self.speed = 0.0

    # The record in the state file.
    @property
    def rec(self):
        return self.svc.state.get("setup") or {}

    def save(self, **changes):
        with self.lock:
            rec = dict(self.rec)
            rec.update(changes)
            self.svc.state["setup"] = rec
            self.svc.state["setting_up"] = bool(rec.get("running"))
            self.svc.save_state()
        self.svc.notify()

    def set_stage(self, stage, state, percent=None, detail=None):
        with self.lock:
            rec = dict(self.rec)
            stages = dict(rec.get("stages") or {})
            s = dict(stages.get(stage) or {})
            s["state"] = state
            if percent is not None:
                s["percent"] = max(0, min(100, int(percent)))
            if detail is not None:
                s["detail"] = detail
            stages[stage] = s
            rec["stages"] = stages
            if state == "active":
                rec["stage"] = stage
            self.svc.state["setup"] = rec
            self.svc.save_state()
        self.svc.notify()

    def done(self, step):
        return step in (self.rec.get("done") or [])

    def mark(self, step):
        self.save(done=sorted(set(self.rec.get("done") or []) | {step}))

    # For D-Bus.
    def status(self):
        rec = self.rec
        if not rec:
            return None
        licence = rec.get("licence") or {}
        ids = [s for s in STAGES if s != "office" or (rec.get("options") or {}).get("office")]
        stages, total, done_weight, minutes_left = [], 0, 0.0, 0.0
        for sid in ids:
            s = (rec.get("stages") or {}).get(sid) or {}
            state = s.get("state", "pending")
            pct = 100 if state == "done" else s.get("percent", 0)
            stages.append({"id": sid, "title": stage_title(sid, licence),
                           "detail": s.get("detail") or stage_detail(sid, licence), "state": state, "percent": pct})
            total += WEIGHTS[sid]
            done_weight += WEIGHTS[sid] * pct / 100.0
            minutes_left += MINUTES[sid] * (1 - pct / 100.0)
        if self.speed > 0 and rec.get("stage") == "media":
            remaining = (rec.get("download") or {}).get("remaining", 0)
            minutes_left += remaining / self.speed / 60 - MINUTES["media"] * (1 - ((rec.get("stages") or {}).get("media") or {}).get("percent", 0) / 100.0)
        minutes = max(1, round(minutes_left))
        return {
            "running": bool(rec.get("running")),
            "stage": rec.get("stage"),
            "stages": stages,
            "percent": round(100 * done_weight / total) if total else 0,
            "left": "About a minute left" if minutes <= 1 else f"About {minutes} minutes left",
            "paused": bool(rec.get("paused")),
            "problem": rec.get("problem"),
            "licence": {"source": licence.get("source"), "edition": licence.get("edition"),
                        "activated": licence.get("activated")},
            "options": rec.get("options") or {},
            "failed": rec.get("failed"),
        }

    # Control.
    def start(self, request, owner_uid):
        """Starts setting up from the wizard's request: {licence: {source:
        firmware|key|none, key}, options: {...}}. "" or why not."""
        if self.thread and self.thread.is_alive():
            return "Windows mode is already being set up."
        if self.rec.get("running"):
            # Trying again after a problem: carry on from the step it was on,
            # keeping everything done so far.
            self.save(failed=None, problem=None, paused=False)
            self.launch()
            return ""
        if virsh.state(self.cfg["domain"]) != "":
            return "Windows mode is set up on this PC already."
        lic = request.get("licence") or {}
        source = lic.get("source")
        if source not in ("firmware", "key", "none"):
            return "That licence choice isn't one Windows mode knows."
        key = (lic.get("key") or "").strip().upper()
        if source == "key" and not key_shape_ok(key):
            return "That doesn't look like a Windows product key. Check it and try again."
        o = request.get("options") or {}
        options = {
            "office": bool(o.get("office")),
            "officePlan": o.get("officePlan") if o.get("officePlan") in OFFICE_PRODUCTS else "personal",
            "openFiles": bool(o.get("openFiles", True)),
            "whenIdle": o.get("whenIdle") if o.get("whenIdle") in ("automatic", "memory", "disk") else "automatic",
            "memoryGb": max(4, min(int(o.get("memoryGb") or 4), 32)),
        }
        os.makedirs(SECRETS, mode=0o700, exist_ok=True)
        os.chmod(SECRETS, 0o700)
        if source == "key":
            with open(os.path.join(SECRETS, ACTIVATION_KEY), "w") as f:
                f.write(key + "\n")
            os.chmod(os.path.join(SECRETS, ACTIVATION_KEY), 0o600)
        self.svc.state["setup"] = {}
        self.save(running=True, started=int(time.time()), owner=owner_uid, options=options,
                  licence={"source": source, "edition": lic.get("edition") or ""},
                  stages={}, done=[], problem=None, failed=None, paused=False)
        self.svc.state["when_idle"] = {"memory": "keep", "disk": "save"}.get(options["whenIdle"], "automatic")
        self.svc.save_state()
        self.launch()
        return ""

    def launch(self):
        with open(MARKER, "w") as f:
            f.write(str(int(time.time())))
        self.stop.clear()
        self.thread = threading.Thread(target=self.run, name="setup", daemon=True)
        self.thread.start()

    def resume_after_restart(self):
        if self.rec.get("running") and not (self.thread and self.thread.is_alive()):
            log("carrying on with the setup that was under way")
            self.launch()

    def pause(self):
        self.save(paused=True)

    def resume(self):
        self.save(paused=False, problem=None)
        self.resume_event.set()
        if self.rec.get("running") and not (self.thread and self.thread.is_alive()):
            self.launch()

    def cancel(self):
        """Stops setup and removes what it made, apart from the downloads,
        so trying again doesn't fetch them twice."""
        self.stop.set()
        self.resume_event.set()
        if self.thread and self.thread.is_alive():
            self.thread.join(30)
        domain = self.cfg["domain"]
        if virsh.state(domain) != "":
            virsh.run("destroy", domain, check=False)
            virsh.run("undefine", domain, "--nvram", "--managed-save", check=False)
        for path in (DISK, os.path.join(CACHE, "payload.iso"), os.path.join(SECRETS, "key"),
                     os.path.join(SECRETS, ACTIVATION_KEY), MARKER):
            try:
                os.remove(path)
            except OSError:
                pass
        self.svc.state.pop("setup", None)
        self.svc.state["setting_up"] = False
        self.svc.save_state()
        self.svc.notify()

    def choose_iso(self, path):
        """A Windows 11 ISO downloaded in a browser, when Microsoft refused
        the automated download (board 9.7): kept if its SHA-256 is one
        Microsoft lists."""
        try:
            st = os.stat(path)
        except OSError:
            return "That file isn't there any more."
        if not os.path.isfile(path) or st.st_size < 3 * 1000 ** 3:
            return "That isn't a Windows 11 ISO."
        os.makedirs(CACHE, exist_ok=True)
        target = os.path.join(CACHE, "windows.iso")
        try:
            sha = hashlib.sha256()
            with open(path, "rb") as src, open(target + ".part", "wb") as dst:
                for block in iter(lambda: src.read(4 * 1024 * 1024), b""):
                    sha.update(block)
                    dst.write(block)
            digest = sha.hexdigest()
            match = next((name for name, s in msdl.checksums() if s == digest), None)
        except (OSError, urllib.error.URLError, msdl.Changed) as e:
            return f"That file couldn't be checked. ({e})"
        if not match:
            os.remove(target + ".part")
            return "That file isn't a Windows 11 ISO from Microsoft, so it wasn't used."
        os.replace(target + ".part", target)
        with open(os.path.join(CACHE, "windows-iso.json"), "w") as f:
            json.dump({"url": "", "file": os.path.basename(path), "sha256": digest, "language": match, "fetched": 0}, f)
        self.resume()
        return ""

    # The worker.
    def run(self):
        # A PC that goes to sleep while idle mid-setup pauses the virtual PC
        # at whatever it was doing, and a missed moment (the key that starts
        # Windows Setup) never comes back. Closing the lid still sleeps it.
        try:
            inhibitor = subprocess.Popen(["systemd-inhibit", "--what=idle:sleep", "--mode=block", "--who=Windows mode",
                                          "--why=Setting up Windows", "sleep", "infinity"],
                                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        except OSError:
            inhibitor = None
        try:
            self.work()
        finally:
            if inhibitor:
                inhibitor.terminate()

    def work(self):
        try:
            self.save(problem=None, failed=None)
            self.step_media()
            self.step_install()
            self.step_activate()
            self.step_tune()
            if (self.rec.get("options") or {}).get("office"):
                self.step_office()
            self.finish()
        except Stopped:
            log("stopped")
        except Problem as p:
            log("problem:", p.kind, p.text)
            if p.kind == "download":
                self.save(problem={"kind": "download", "text": p.text,
                                   "received": (self.rec.get("download") or {}).get("received", 0)}, paused=True)
            elif p.kind == "space":
                # Board 9.7: free some space and carry on; nothing failed.
                self.save(problem={"kind": "space", "text": p.text}, paused=True)
            else:
                stage = self.rec.get("stage") or "media"
                self.set_stage(stage, "failed")
                self.save(problem={"kind": p.kind, "text": p.text}, failed=p.text)
        except Exception as e:  # noqa: BLE001 (any failure is shown, never swallowed)
            log("failed:", repr(e))
            stage = self.rec.get("stage") or "media"
            self.set_stage(stage, "failed")
            text = "Windows mode couldn't finish setting up. Try again, and if it happens twice, restart the PC first."
            self.save(problem={"kind": "failed", "text": text}, failed=f"{text} ({e})")

    def check(self):
        if self.stop.is_set():
            raise Stopped()
        while self.rec.get("paused") and not self.stop.is_set():
            self.resume_event.wait(5)
            self.resume_event.clear()
        if self.stop.is_set():
            raise Stopped()

    # media
    def step_media(self):
        self.set_stage("media", "active")
        os.makedirs(CACHE, exist_ok=True)
        self.check_space()
        self.runtime()
        self.check()
        self.prepare_licence()
        self.windows_iso()
        self.check()
        self.set_stage("media", "active", 92, "Getting the drivers Windows needs")
        self.virtio_iso()
        self.set_stage("media", "done", 100, "")

    def check_space(self):
        memory = (self.rec.get("options") or {}).get("memoryGb", 4)
        need = (30 + memory) * 1000 ** 3
        if not os.path.exists(os.path.join(CACHE, "windows.iso")):
            need += 9 * 1000 ** 3
        if os.path.exists(DISK):
            need -= os.stat(DISK).st_blocks * 512
        free = shutil.disk_usage(config.STATE_DIR).free
        if free < need:
            raise Problem("space", f"Windows mode needs {size_text(need)} free and this PC has {size_text(free)}.")

    def runtime(self):
        """The virtual PC (QEMU, libvirt, FreeRDP) is its own package, which
        the Wild OS image leaves out so that people who never set up Windows
        mode never download it."""
        if runtime_installed():
            return
        self.set_stage("media", "active", 1, "Getting the parts Windows mode runs on")
        env = dict(os.environ, DEBIAN_FRONTEND="noninteractive", LC_ALL="C.UTF-8")
        # Waits out an update that is already installing rather than failing.
        apt = ["apt-get", "-o", "DPkg::Lock::Timeout=900", "-o", "Dpkg::Options::=--force-confold"]
        subprocess.run(apt + ["update", "-qq"], env=env, capture_output=True, timeout=900)
        p = subprocess.run(apt + ["install", "-y", "-qq", "--no-install-recommends", "wildos-winmode-runtime"], env=env,
                           capture_output=True, text=True, timeout=3600)
        if p.returncode != 0:
            log("apt-get:", (p.stderr or p.stdout).strip()[-500:])
            raise Problem("download", "Windows mode couldn't download the parts it runs on. Check you're "
                                      "online, then carry on.")
        self.svc.runtime_installed()

    def prepare_licence(self):
        if self.done("licence"):
            return
        lic = dict(self.rec.get("licence") or {})
        msdm_copy = os.path.join(SECRETS, "msdm.bin")
        key = None
        if lic.get("source") == "firmware":
            try:
                with open("/sys/firmware/acpi/tables/MSDM", "rb") as f:
                    table = f.read()
            except OSError:
                table = b""
            key = parse_msdm(table)
            if not key:
                raise Problem("failed", "This PC's licence couldn't be read. Go back and add a product key instead.")
            # QEMU reads the table from a root-only copy (brief 5.3).
            with open(msdm_copy, "wb") as f:
                f.write(table)
            group = next((g for g in ("libvirt-qemu", "kvm") if _gid(g) is not None), None)
            if group:
                os.chown(msdm_copy, 0, _gid(group))
                os.chown(SECRETS, 0, _gid(group))
                os.chmod(SECRETS, 0o710)
            os.chmod(msdm_copy, 0o640)
            _apparmor_allow(msdm_copy)
        elif lic.get("source") == "key":
            with open(os.path.join(SECRETS, ACTIVATION_KEY)) as f:
                key = f.read().strip()
        else:
            key = GENERIC_PRO_KEY
        with open(os.path.join(SECRETS, "key"), "w") as f:
            f.write(GENERIC_PRO_KEY + "\n")
        os.chmod(os.path.join(SECRETS, "key"), 0o600)
        lic["last5"] = key[-5:]
        self.save(licence=lic)
        self.mark("licence")

    def windows_iso(self):
        iso = os.path.join(CACHE, "windows.iso")
        meta_path = os.path.join(CACHE, "windows-iso.json")
        if self.done("windows-iso") and os.path.exists(iso):
            return
        lang, _, _ = host_locale()
        for attempt in range(2):
            meta = _read_json(meta_path)
            if not os.path.exists(iso):
                # Microsoft's links last 24 hours; a fresh one is asked for
                # when the stored one is older than 20.
                if not meta or time.time() - meta.get("fetched", 0) > 72000:
                    self.set_stage("media", "active", None, "Asking Microsoft for Windows 11")
                    try:
                        fresh = msdl.fetch(lang)
                    except msdl.Refused as e:
                        raise Problem("download", f"Microsoft didn't allow the download from here. ({e})") from e
                    except (msdl.Changed, OSError) as e:
                        raise Problem("download", f"Microsoft's download page didn't answer as expected. ({e})") from e
                    if meta and meta.get("file") != fresh["file"]:
                        _remove(iso + ".part")
                    meta = fresh
                    with open(meta_path, "w") as f:
                        json.dump(meta, f)
                try:
                    self.download(meta["url"], iso, "media", 0, 88, "Windows 11")
                except urllib.error.HTTPError as e:
                    if e.code in (403, 404, 410) and attempt == 0:
                        meta["fetched"] = 0
                        with open(meta_path, "w") as f:
                            json.dump(meta, f)
                        continue
                    raise Problem("download", f"The download stopped. ({e.code})") from e
            self.set_stage("media", "active", 89, "Checking it against Microsoft's signature")
            digest = _sha256(iso, self.check)
            if digest == (meta or {}).get("sha256"):
                self.mark("windows-iso")
                return
            log("SHA-256 doesn't match Microsoft's; deleting it")
            _remove(iso)
            if meta:
                meta["fetched"] = 0
                with open(meta_path, "w") as f:
                    json.dump(meta, f)
        raise Problem("download", "The Windows download didn't match Microsoft's signature twice, so it was deleted.")

    def download(self, url, target, stage, start, end, label, agent=None):
        """HTTPS download with resume (Range), pause and progress, into
        target.part and then target."""
        part = target + ".part"
        tries = 0
        while True:
            self.check()
            have = os.path.getsize(part) if os.path.exists(part) else 0
            req = urllib.request.Request(url, headers={"User-Agent": agent or msdl.AGENT})
            if have:
                req.add_header("Range", f"bytes={have}-")
            try:
                with urllib.request.urlopen(req, timeout=60) as r:
                    if have and r.status != 206:
                        have = 0
                    total = have + int(r.headers.get("Content-Length") or 0)
                    with open(part, "ab" if have else "wb") as f:
                        last_t, last_n, got = time.monotonic(), have, have
                        while True:
                            if self.stop.is_set() or self.rec.get("paused"):
                                break
                            block = r.read(1024 * 1024)
                            if not block:
                                break
                            f.write(block)
                            got += len(block)
                            now = time.monotonic()
                            if now - last_t >= 2:
                                self.speed = (got - last_n) / (now - last_t)
                                last_t, last_n = now, got
                                pct = start + (end - start) * got / total if total else start
                                self.save(download={"received": got, "total": total, "remaining": max(0, total - got)})
                                self.set_stage(stage, "active", pct,
                                               f"{label}: {size_text(got)} of {size_text(total)}" if total else label)
                    if self.stop.is_set() or self.rec.get("paused"):
                        self.check()
                        continue
                    if total and os.path.getsize(part) < total:
                        raise OSError("the connection ended early")
                os.replace(part, target)
                self.speed = 0.0
                return
            except urllib.error.HTTPError as e:
                if e.code == 416:
                    _remove(part)
                    continue
                raise
            except (urllib.error.URLError, OSError, socket.timeout) as e:
                tries += 1
                log("download:", e)
                if tries >= 6:
                    raise Problem("download", "The internet connection dropped. Setup carries on when it's back.") from e
                time.sleep(min(60, 5 * tries))

    def virtio_iso(self):
        iso = os.path.join(CACHE, "virtio-win.iso")
        if is_iso(iso):
            return
        _remove(iso)
        # The stable link redirects to the versioned file over plain HTTP; it
        # is followed by hand so every byte comes over HTTPS.
        url = f"{VIRTIO_BASE}/stable-virtio/virtio-win.iso"
        for _ in range(3):
            loc = _redirect(url)
            if not loc:
                break
            url = loc.replace("http://", "https://", 1)
        if not url.startswith("https://fedorapeople.org/groups/virt/virtio-win/"):
            raise Problem("download", "The driver download moved somewhere unexpected.")
        # fedorapeople.org shows browsers a "not a bot" page in place of the
        # file, so this download says what it is rather than posing as one.
        self.download(url, iso, "media", 92, 96, "Drivers", agent=TOOL_AGENT)
        if not is_iso(iso):
            _remove(iso)
            raise Problem("download", "The drivers download came back as something other than the drivers. Try again in a while.")

    # install
    def step_install(self):
        if self.done("install"):
            return
        self.set_stage("install", "active", 0)
        domain = self.cfg["domain"]
        if virsh.state(domain) == "":
            self.payload()
            self.network()
            self.define()
        self.check()
        started = self.rec.get("install_started")
        if virsh.state(domain) in ("shut off", "saved"):
            firewall.apply(self.cfg, False)
            self.svc.bind_folders()
            virsh.start(domain)
            if not started:
                self.press_to_boot(domain)
        if not started:
            started = int(time.time())
            self.save(install_started=started, boot_tries=1, booted_at=started)
        while True:
            self.check()
            if time.time() - started > INSTALL_LIMIT:
                raise Problem("failed", "Windows didn't finish installing. Try again, and if it happens twice, restart the PC first.")
            st = virsh.state(domain)
            if st == "shut off":
                raise Problem("failed", "Windows switched itself off while installing. Try again.")
            if st == "paused":
                virsh.resume(domain)
            if qga.ping(domain, 3):
                break
            try:
                used = os.stat(DISK).st_blocks * 512
            except OSError:
                used = 0
            # Windows Setup writes to the disk within a minute or two of
            # starting. A disk still empty after five means the virtual PC
            # never started it: the key for "Press any key to boot from CD
            # or DVD" was missed (the PC slept or was slow to get there).
            # Start the virtual PC again and press it again.
            tries = self.rec.get("boot_tries", 1)
            if used < BOOT_WRITTEN and time.time() - self.rec.get("booted_at", started) > BOOT_WAIT:
                if tries >= BOOT_TRIES:
                    raise Problem("failed", "Windows Setup didn't start. Try again, and if it happens twice, restart the PC first.")
                log("Windows Setup hasn't started; starting the virtual PC again")
                virsh.run("reset", domain)
                self.press_to_boot(domain)
                self.save(boot_tries=tries + 1, booted_at=int(time.time()))
                continue
            # The disk shows the copying; after it, Windows spends ten minutes
            # or so on its first start writing little, so the bar keeps
            # creeping on the clock rather than sitting still.
            if used < INSTALLED_BYTES:
                pct = used * 70 // INSTALLED_BYTES
            else:
                if not self.rec.get("copied_at"):
                    self.save(copied_at=int(time.time()))
                pct = 70 + min(25, (time.time() - self.rec["copied_at"]) / 60 * 2.5)
            self.set_stage("install", "active", int(pct))
            time.sleep(15)
        self.set_stage("install", "done", 100)
        self.mark("install")

    def press_to_boot(self, domain):
        """OVMF asks "Press any key to boot from CD or DVD" for a few seconds
        after it starts; Enter for half a minute covers a slow start. Only a
        boot meant for the installer gets it, so Windows' own restarts go to
        the disk."""
        for _ in range(30):
            virsh.run("send-key", domain, "KEY_ENTER", check=False)
            time.sleep(1)

    def payload(self):
        lang, keyboard, tz = host_locale()
        pw = self.svc.new_guest_password()
        with open(os.path.join(SECRETS, "key")) as f:
            key = f.read().strip()
        host = socket.gethostname().split(".")[0].upper()
        computer = (re.sub(r"[^A-Z0-9-]", "", host)[:13] or "WILDOS") + "-W"
        self.svc.state["computer_name"] = computer
        self.svc.save_state()
        work = os.path.join(CACHE, "payload")
        shutil.rmtree(work, ignore_errors=True)
        os.makedirs(os.path.join(work, "wildos"), mode=0o700)
        with open(os.path.join(work, "autounattend.xml"), "w", encoding="utf-8") as f:
            f.write(render("autounattend.xml.in", {
                "LANG": lang, "INPUT_LOCALE": keyboard, "TIMEZONE": xml_escape(tz), "COMPUTER_NAME": computer,
                "ACCOUNT": self.cfg["account"], "PASSWORD": xml_escape(pw), "PRODUCT_KEY": key, "FULL_NAME": "Wild OS"}))
        guest = os.path.join(config.DATA, "guest")
        shutil.copy(os.path.join(guest, "setup.ps1"), os.path.join(work, "wildos", "setup.ps1"))
        shutil.copy(self.cfg["guest_exe"], os.path.join(work, "wildos", "wild-guest.exe"))
        plan = (self.rec.get("options") or {}).get("officePlan", "personal")
        with open(os.path.join(work, "wildos", "office.xml"), "w", encoding="utf-8") as f:
            f.write(office_config(plan, lang))
        with open(os.path.join(work, "wildos", "settings.json"), "w", encoding="utf-8") as f:
            json.dump({"host": self.cfg["host_address"], "guest": self.cfg["guest_address"],
                       "account": self.cfg["account"], "folders": self.cfg["folders"], "drive": self.cfg["drive"],
                       "transport": "rdp"}, f)
        out = os.path.join(CACHE, "payload.iso")
        # Windows Setup looks for autounattend.xml at the root of every drive.
        p = subprocess.run(["xorriso", "-as", "mkisofs", "-quiet", "-J", "-joliet-long", "-r", "-V", "WILDOS_SETUP",
                            "-o", out + ".new", work], capture_output=True, text=True)
        # The answer file holds the guest password; only the ISO keeps it.
        shutil.rmtree(work, ignore_errors=True)
        if p.returncode != 0:
            raise RuntimeError(f"xorriso: {p.stderr.strip()[-300:]}")
        os.replace(out + ".new", out)
        os.chmod(out, 0o600)

    def network(self):
        name = self.cfg["network"]
        if virsh.run("net-info", name, check=False, timeout=10) == "":
            path = os.path.join(config.STATE_DIR, "network.xml")
            with open(path, "w") as f:
                f.write(render("network.xml.in", {"NAME": name, "BRIDGE": self.cfg["bridge"], "GW": self.cfg["host_address"],
                                                  "GUEST_IP": self.cfg["guest_address"], "GUEST_MAC": GUEST_MAC}))
            virsh.run("net-define", path)
        virsh.run("net-autostart", name, check=False)
        if "yes" not in re.sub(r"\s+", " ", virsh.run("net-info", name, check=False)).split("Active: ")[-1][:4]:
            virsh.run("net-start", name)
        ok, why = firewall.apply(self.cfg, False)
        if not ok:
            raise RuntimeError(f"firewall: {why}")

    def define(self):
        if not os.path.exists(DISK):
            # Metadata allocated up front saves the guest a lookup on every
            # first write (brief 5.5a); the data stays sparse.
            subprocess.run(["qemu-img", "create", "-q", "-f", "qcow2", "-o", "preallocation=metadata", DISK, "64G"],
                           check=True, capture_output=True)
        # Every core: Windows is paused whenever no Windows app is open, so
        # its share costs the desktop nothing then, and app starts inside it
        # are bound by processor (2 of 4 cores made Word slow to open).
        cpus = max(2, min(8, os.cpu_count() or 4))
        with open("/proc/cpuinfo") as f:
            cpu_off = "svm" if re.search(r"\bsvm\b", f.read()) else "vmx"
        vendor = _read("/sys/class/dmi/id/sys_vendor") or "Wild OS"
        product = _read("/sys/class/dmi/id/product_name") or "Windows mode"
        msdm = os.path.join(SECRETS, "msdm.bin")
        acpi = (f"<qemu:commandline><qemu:arg value='-acpitable'/><qemu:arg value='file={msdm}'/></qemu:commandline>"
                if os.path.exists(msdm) else "")
        memory_mb = 1024 * (self.rec.get("options") or {}).get("memoryGb", 4)
        os.makedirs(self.cfg["share_root"], exist_ok=True)
        xml = render("domain.xml.in", {
            "NAME": self.cfg["domain"], "MEMORY_MB": memory_mb, "CPUS": cpus, "CPU_OFF": cpu_off, "DISK": DISK,
            "WINDOWS_ISO": os.path.join(CACHE, "windows.iso"), "VIRTIO_ISO": os.path.join(CACHE, "virtio-win.iso"),
            "PAYLOAD_ISO": os.path.join(CACHE, "payload.iso"), "NET": self.cfg["network"], "GUEST_MAC": GUEST_MAC,
            "SHARE": self.cfg["share_root"], "VIRTIOFSD_BINARY": "", "SMBIOS_MANUFACTURER": xml_escape(vendor),
            "SMBIOS_PRODUCT": xml_escape(product), "QEMU_COMMANDLINE": acpi})
        path = os.path.join(config.STATE_DIR, "domain.xml")
        with open(path, "w") as f:
            f.write(xml)
        os.chmod(path, 0o600)
        virsh.run("define", path)
        self.svc.memory = virsh.defined_memory(self.cfg["domain"])

    # activate
    def step_activate(self):
        if self.done("activate"):
            return
        lic = dict(self.rec.get("licence") or {})
        if lic.get("source") == "none":
            self.set_stage("activate", "done", 100)
            self.mark("activate")
            return
        self.set_stage("activate", "active", 10)
        domain = self.cfg["domain"]
        key = self.activation_key(lic)
        if not key:
            raise Problem("failed", "This PC's licence couldn't be read. Go back and add a product key instead.")
        taken = None
        for _ in range(3):
            self.check()
            try:
                _, out, _ = qga.powershell(domain, (
                    f"try {{ Get-CimInstance SoftwareLicensingService | Invoke-CimMethod -MethodName InstallProductKey "
                    f"-Arguments @{{ProductKey='{key}'}} -ErrorAction Stop | Out-Null; 'taken' }} catch {{ 'refused' }}"), 120)
                last = out.strip().splitlines()[-1] if out.strip() else ""
                taken = True if last == "taken" else False if last == "refused" else None
            except (virsh.VirshError, KeyError, TimeoutError, IndexError) as e:
                log("product key:", e)
            if taken is not None:
                break
            time.sleep(10)
        if taken is False:
            # Not a Windows 11 Pro key, most often one for Home. Windows
            # stays Pro, unactivated, and works the same.
            lic.update(activated=False, refused=True)
            self.save(licence=lic)
            self.forget_activation_key()
            self.set_stage("activate", "done", 100,
                           ("This PC's licence is for another edition of Windows, usually Home, which Windows mode can't use. "
                            if lic.get("source") == "firmware" else
                            "Windows didn't accept that key. It may be for Home, which Windows mode can't use. ")
                           + "Windows works without activating; add a Windows 11 Pro key any time in Settings.")
            self.mark("activate")
            return
        code = None
        # The network may come up a moment after the agent does.
        for _ in range(6):
            self.check()
            try:
                qga.powershell(domain, r"cscript //nologo $env:SystemRoot\System32\slmgr.vbs /ato 2>&1 | Out-String", 180)
                _, out, _ = qga.powershell(domain, (
                    "(Get-CimInstance SoftwareLicensingProduct -Filter \"ApplicationID='55c92734-d682-4d71-983e-d6ec3f16059f'"
                    " and PartialProductKey is not null\" | Select-Object -First 1).LicenseStatus"), 60)
                code = int(out.strip().splitlines()[-1]) if out.strip() else None
            except (virsh.VirshError, ValueError, KeyError, TimeoutError, IndexError) as e:
                log("activation:", e)
            if code == 1:
                break
            time.sleep(30)
        lic["activated"] = code == 1
        self.save(licence=lic)
        if taken:
            self.forget_activation_key()
        # Not activating is not a failure: Windows works, and a key can be
        # added in Settings (board 9.8).
        self.set_stage("activate", "done", 100,
                       None if code == 1 else "Not activated yet. Windows works; add a key any time in Settings.")
        self.mark("activate")

    def activation_key(self, lic):
        if lic.get("source") == "firmware":
            try:
                with open(os.path.join(SECRETS, "msdm.bin"), "rb") as f:
                    return parse_msdm(f.read())
            except OSError:
                return None
        try:
            with open(os.path.join(SECRETS, ACTIVATION_KEY)) as f:
                return f.read().strip() or None
        except OSError:
            return None

    def forget_activation_key(self):
        try:
            os.remove(os.path.join(SECRETS, ACTIVATION_KEY))
        except OSError:
            pass

    def set_key(self, key):
        """For Settings, any time after setup. The key goes into Windows over
        the guest agent and is never written to disk here."""
        domain = self.cfg["domain"]
        if virsh.state(domain) != "running":
            return "Open a Windows app first, so Windows is running, then try again."
        source = "key"
        if key == "firmware":
            try:
                with open("/sys/firmware/acpi/tables/MSDM", "rb") as f:
                    key = parse_msdm(f.read())
            except OSError:
                key = None
            if not key:
                return "This PC has no Windows licence built in."
            source = "firmware"
        key = key.strip().upper()
        if not key_shape_ok(key):
            return "That doesn't look like a Windows product key. Check it and try again."
        try:
            _, out, _ = qga.powershell(domain, (
                f"$s = Get-CimInstance SoftwareLicensingService; try {{ $s | Invoke-CimMethod -MethodName InstallProductKey "
                f"-Arguments @{{ProductKey='{key}'}} -ErrorAction Stop | Out-Null }} catch {{ 'refused'; exit }}; "
                "$s | Invoke-CimMethod -MethodName RefreshLicenseStatus | Out-Null; "
                r"cscript //nologo $env:SystemRoot\System32\slmgr.vbs /ato | Out-Null; "
                "(Get-CimInstance SoftwareLicensingProduct -Filter \"ApplicationID='55c92734-d682-4d71-983e-d6ec3f16059f'"
                " and PartialProductKey is not null\" | Select-Object -First 1).LicenseStatus"), 240)
            if out.strip().endswith("refused"):
                return "Windows didn't accept that key. Check it, and that it's for Windows 11 Pro: Windows mode can't use a Home key."
            code = int(out.strip().splitlines()[-1]) if out.strip() else None
        except (virsh.VirshError, ValueError, KeyError, TimeoutError, IndexError) as e:
            log("product key:", e)
            return "Windows didn't take that key. Check it, and that this PC is online, then try again."
        lic = dict(self.rec.get("licence") or {})
        lic.update(source=source, last5=key[-5:], activated=code == 1)
        self.save(licence=lic)
        return "" if code == 1 else "The key is in, but Windows couldn't activate yet. It tries again by itself when it's online."

    # tune
    def step_tune(self):
        if self.done("tune"):
            return
        self.set_stage("tune", "active", 0)
        domain = self.cfg["domain"]
        started = self.rec.get("install_started") or int(time.time())
        while True:
            self.check()
            if time.time() - started > INSTALL_LIMIT + 3600:
                raise Problem("failed", "Windows didn't finish getting ready. Try again.")
            if virsh.state(domain) == "shut off":
                virsh.start(domain)
            ready = self._guest_json(r"C:\ProgramData\WildOS\ready.json")
            if ready:
                lic = dict(self.rec.get("licence") or {})
                edition = str(ready.get("edition", ""))
                lic["edition"] = "home" if edition.startswith("Core") else "pro" if edition else lic.get("edition", "")
                self.save(licence=lic)
                if lic["edition"] == "home":
                    # Windows 11 Home has no Remote Desktop host (brief 2.2).
                    raise Problem("home", "This licence is for Windows 11 Home, which can't show Windows apps on Wild OS.")
                break
            stage = self._guest_json(r"C:\ProgramData\WildOS\stage.json") or {}
            if stage.get("state") == "failed":
                raise Problem("failed", "Windows couldn't finish getting ready. Try again.")
            name = stage.get("name")
            if name in GUEST_STEPS:
                self.set_stage("tune", "active", 100 * GUEST_STEPS.index(name) // len(GUEST_STEPS))
            time.sleep(10)
        self.set_stage("tune", "done", 100)
        self.mark("tune")

    def _guest_json(self, path):
        try:
            code, out, _ = qga.powershell(self.cfg["domain"],
                                          f"if (Test-Path '{path}') {{ Get-Content -Raw '{path}' }} else {{ exit 3 }}", 20)
            return json.loads(out) if code == 0 and out.strip() else None
        except (virsh.VirshError, ValueError, KeyError, TimeoutError):
            return None

    # office
    def step_office(self):
        if self.done("office"):
            return
        self.set_stage("office", "active", 5)
        domain = self.cfg["domain"]
        check = f"Test-Path '{WORD}'"
        _, have, _ = qga.powershell(domain, check, 30)
        if have.strip() != "True":
            script = r"""$ErrorActionPreference = 'Stop'
$d = 'C:\ProgramData\WildOS\odt'
New-Item -ItemType Directory -Force $d | Out-Null
$cd = Get-PSDrive -PSProvider FileSystem | Where-Object { Test-Path "$($_.Root)wildos\office.xml" } | Select-Object -First 1
Copy-Item "$($cd.Root)wildos\office.xml" "$d\office.xml" -Force
Invoke-WebRequest -UseBasicParsing 'https://officecdn.microsoft.com/pr/wsus/setup.exe' -OutFile "$d\setup.exe"
$sig = Get-AuthenticodeSignature "$d\setup.exe"
if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notlike '*O=Microsoft Corporation*') { throw "setup.exe signature: $($sig.Status)" }
$p = Start-Process "$d\setup.exe" -ArgumentList '/configure', "$d\office.xml" -WorkingDirectory $d -PassThru
"pid=$($p.Id)"
"""
            code, out, err = qga.powershell(domain, script, 600)
            if code != 0:
                raise Problem("failed", "Microsoft 365 couldn't be downloaded. Add it later from Settings.")
            t0 = time.time()
            while True:
                self.check()
                _, have, _ = qga.powershell(domain, check + "; (Get-Process setup -ErrorAction SilentlyContinue).Count", 30)
                lines = have.split()
                if lines and lines[0] == "True" and (len(lines) < 2 or lines[1] == "0"):
                    break
                if time.time() - t0 > 5400:
                    raise Problem("failed", "Microsoft 365 didn't finish installing. Add it later from Settings.")
                if len(lines) > 1 and lines[1] == "0" and lines[0] != "True":
                    raise Problem("failed", "Microsoft 365 didn't install. Add it later from Settings.")
                self.set_stage("office", "active", min(95, 5 + (time.time() - t0) * 90 / (MINUTES["office"] * 60)))
                time.sleep(15)
        self.set_stage("office", "done", 100)
        self.mark("office")

    def finish(self):
        domain = self.cfg["domain"]
        # The installer, the drivers disc and the payload (which holds the
        # first password) are only for setup (brief section 4).
        for dev in ("sda", "sdb", "sdc"):
            virsh.run("change-media", domain, dev, "--eject", "--live", "--config", check=False)
        for name in ("windows.iso", "windows.iso.part", "payload.iso"):
            _remove(os.path.join(CACHE, name))
        _remove(MARKER)
        self.save(running=False, stage="done", problem=None, finished=int(time.time()))
        log("Windows mode is ready")


def _gid(group):
    import grp
    try:
        return grp.getgrnam(group).gr_gid
    except KeyError:
        return None


def _apparmor_allow(path):
    """libvirt's AppArmor profile for QEMU only lists the files it knows
    about; the MSDM copy goes on QEMU's raw command line."""
    for target in ("/etc/apparmor.d/abstractions/libvirt-qemu.d/wildos-winmode",
                   "/etc/apparmor.d/local/abstractions/libvirt-qemu"):
        if os.path.isdir(os.path.dirname(target)):
            try:
                text = open(target).read() if os.path.exists(target) else ""
                if path not in text:
                    with open(target, "a") as f:
                        f.write(f"  {path} r,\n")
            except OSError as e:
                log("apparmor:", e)
            return


def _redirect(url):
    req = urllib.request.Request(url, method="HEAD", headers={"User-Agent": msdl.AGENT})

    class NoRedirect(urllib.request.HTTPRedirectHandler):
        def redirect_request(self, *args, **kwargs):
            return None

    try:
        urllib.request.build_opener(NoRedirect).open(req, timeout=30)
    except urllib.error.HTTPError as e:
        if e.code in (301, 302, 303, 307, 308):
            return e.headers.get("Location")
    except urllib.error.URLError:
        pass
    return None


def _sha256(path, check=None):
    h = hashlib.sha256()
    with open(path, "rb") as f:
        for i, block in enumerate(iter(lambda: f.read(8 * 1024 * 1024), b"")):
            h.update(block)
            if check and i % 32 == 0:
                check()
    return h.hexdigest()


def _read(path):
    try:
        with open(path) as f:
            return f.read().strip()
    except OSError:
        return ""


def _read_json(path):
    try:
        with open(path) as f:
            return json.load(f)
    except (OSError, ValueError):
        return None


def _remove(path):
    try:
        os.remove(path)
    except OSError:
        pass
