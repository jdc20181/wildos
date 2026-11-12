"""libvirt through virsh on qemu:///system.

One process per call keeps the daemon free of libvirt's own event loop and
puts every step in the journal; nothing here is on a hot path. The calls
that can take seconds (managed save, restore) run in a thread so the D-Bus
service keeps answering.
"""

import json
import re
import subprocess
import xml.etree.ElementTree as ET

URI = "qemu:///system"


class VirshError(RuntimeError):
    pass


def run(*args, timeout=60, check=True):
    try:
        p = subprocess.run(["virsh", "-q", "-c", URI, *args], capture_output=True, text=True, timeout=timeout)
    except FileNotFoundError as e:
        raise VirshError("virsh is not installed") from e
    except subprocess.TimeoutExpired as e:
        raise VirshError(f"virsh {args[0]} took longer than {timeout} s") from e
    if check and p.returncode != 0:
        raise VirshError((p.stderr or p.stdout).strip() or f"virsh {args[0]} failed")
    return p.stdout.strip()


def state(domain):
    """running, paused, shut off, saved (shut off with a managed save
    image), crashed, pmsuspended, or "" when the domain does not exist."""
    try:
        s = run("domstate", domain, timeout=10)
    except VirshError:
        return ""
    if s == "shut off":
        try:
            info = run("dominfo", domain, timeout=10)
        except VirshError:
            return s
        m = re.search(r"^Managed save:\s*(\S+)", info, re.M)
        if m and m.group(1) == "yes":
            return "saved"
    return s


def start(domain):
    run("start", domain, timeout=180)


def suspend(domain):
    run("suspend", domain, timeout=30)


def resume(domain):
    run("resume", domain, timeout=30)


def managedsave(domain):
    return run("managedsave", domain, timeout=300)


def dumpxml(domain, inactive=False):
    return run("dumpxml", domain, *(["--inactive"] if inactive else []), timeout=10)


def disk_files(domain):
    """The files behind the definition's hard disks."""
    try:
        root = ET.fromstring(dumpxml(domain, inactive=True))
    except ET.ParseError:
        return []
    out = []
    for disk in root.findall("devices/disk"):
        source = disk.find("source")
        if disk.get("type") == "file" and disk.get("device") == "disk" and source is not None and source.get("file"):
            out.append(source.get("file"))
    return out


def channel_path(domain, name):
    """The host socket libvirt made for a virtio-serial channel, while the
    domain runs."""
    try:
        root = ET.fromstring(dumpxml(domain))
    except (VirshError, ET.ParseError):
        return None
    for ch in root.iter("channel"):
        target = ch.find("target")
        source = ch.find("source")
        if target is not None and target.get("name") == name and source is not None:
            return source.get("path")
    return None


def agent_command(domain, command, timeout=10):
    out = run("qemu-agent-command", domain, json.dumps(command), "--timeout", str(timeout), timeout=timeout + 5)
    return json.loads(out) if out else {}


def defined_memory(domain, inactive=True):
    """The memory limit in bytes: the definition's, which the next start
    takes, or with inactive=False the one the running Windows started with."""
    try:
        root = ET.fromstring(dumpxml(domain, inactive=inactive))
    except (VirshError, ET.ParseError):
        return 0
    mem = root.find("memory")
    if mem is None or not (mem.text or "").isdigit():
        return 0
    unit = mem.get("unit", "KiB")
    scale = {"KiB": 1024, "MiB": 1024 ** 2, "GiB": 1024 ** 3, "b": 1, "bytes": 1}.get(unit, 1024)
    return int(mem.text) * scale


def memory_stats(domain):
    """The balloon driver's view, in KiB: actual (what Windows has now),
    usable (what it could use without paging) and so on."""
    stats = {}
    for line in run("dommemstat", domain, timeout=10).splitlines():
        parts = line.split()
        if len(parts) == 2 and parts[1].isdigit():
            stats[parts[0]] = int(parts[1])
    return stats


def set_memory(domain, kib):
    """Inflates or deflates the balloon, live: memory Windows gives up goes
    back to Wild OS."""
    run("setmem", domain, str(int(kib)), "--live", timeout=30)
