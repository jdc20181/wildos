"""The QEMU guest agent in Windows (channel org.qemu.guest_agent.0).

It runs as SYSTEM in session 0: right for the clock, passwords, installs
and checks. Anything that needs the person's own session goes through
wild-guest instead (guestlink.py).
"""

import base64
import time

from . import virsh


def ping(domain, timeout=3):
    try:
        virsh.agent_command(domain, {"execute": "guest-ping"}, timeout=timeout)
        return True
    except (virsh.VirshError, ValueError):
        return False


def wait(domain, seconds):
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if ping(domain, 2):
            return True
        time.sleep(0.25)
    return False


def set_time(domain):
    """A paused or restored guest's clock is behind by however long it was
    stopped (brief 5.6)."""
    virsh.agent_command(domain, {"execute": "guest-set-time", "arguments": {"time": time.time_ns()}}, timeout=10)


def set_password(domain, account, password):
    virsh.agent_command(domain, {"execute": "guest-set-user-password", "arguments": {
        "username": account, "password": base64.b64encode(password.encode()).decode(), "crypted": False}}, timeout=20)


def run(domain, path, args, timeout=60):
    """Runs a program, waits for it, returns (exit code, stdout, stderr)."""
    r = virsh.agent_command(domain, {"execute": "guest-exec", "arguments": {
        "path": path, "arg": list(args), "capture-output": True}}, timeout=30)
    pid = r["return"]["pid"]
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        s = virsh.agent_command(domain, {"execute": "guest-exec-status", "arguments": {"pid": pid}}, timeout=10)["return"]
        if s.get("exited"):
            out = base64.b64decode(s.get("out-data", "")).decode("utf-8", "replace")
            err = base64.b64decode(s.get("err-data", "")).decode("utf-8", "replace")
            return s.get("exitcode", 1), out, err
        time.sleep(0.25)
    raise TimeoutError(f"{path} still running after {timeout} s")


def powershell(domain, script, timeout=60):
    script = "$ProgressPreference = 'SilentlyContinue'; " + script
    enc = base64.b64encode(script.encode("utf-16-le")).decode()
    return run(domain, r"C:\Windows\System32\WindowsPowerShell\v1.0\powershell.exe",
               ["-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-EncodedCommand", enc], timeout)


def write_file(domain, guest_path, data, chunk=48 * 1024):
    h = virsh.agent_command(domain, {"execute": "guest-file-open", "arguments": {"path": guest_path, "mode": "wb"}})["return"]
    try:
        for i in range(0, len(data), chunk):
            virsh.agent_command(domain, {"execute": "guest-file-write", "arguments": {
                "handle": h, "buf-b64": base64.b64encode(data[i:i + chunk]).decode()}}, timeout=30)
    finally:
        virsh.agent_command(domain, {"execute": "guest-file-close", "arguments": {"handle": h}})
