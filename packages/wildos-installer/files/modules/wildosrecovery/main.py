#!/usr/bin/env python3
"""Adds the recovery key from the Encryption page to every encrypted
partition, as a second LUKS key slot beside the disk password.

The key comes from global storage (wildosRecoveryKey, scrambled the way
Calamares scrambles the disk password). It goes to cryptsetup in a file
under /run, which is memory and never the installed disk, and is deleted
straight after; the disk password goes in on standard input, so neither
shows in the process list.

It runs straight after the partitions are made, before anything is
copied, and a failure stops the install with the reason: people were
promised a recovery key, so the install does not carry on without one.
wildosRecoveryAdded tells the finished page the key is in place.
"""

import os
import subprocess
import tempfile

import libcalamares


def pretty_name():
    return "Adding the recovery key"


def encrypted_devices(partitions):
    """Every partition with a LUKS container, by its file system name.

    Not by luksMapperName: that is only filled in once the container is
    open, which the partition jobs do not always leave it.
    """
    devices = []
    for p in partitions or []:
        if str(p.get("fsName", "")).lower().startswith("luks") or p.get("luksUuid"):
            if p.get("device") and p["device"] not in devices:
                devices.append(p["device"])
    return devices


def run():
    gs = libcalamares.globalstorage
    gs.insert("wildosRecoveryAdded", False)
    if not gs.value("wildosRecovery") or not gs.value("wildosEncrypt"):
        return None
    key = libcalamares.utils.obscure(gs.value("wildosRecoveryKey") or "")
    passphrase = libcalamares.utils.obscure(gs.value("luksPassphrase") or "")
    if not key or not passphrase:
        return ("The recovery key could not be added",
                "The installer lost the disk password or the recovery key before it could add the key.")

    devices = encrypted_devices(gs.value("partitions"))
    if not devices:
        return ("The recovery key could not be added",
                "The installer found no encrypted partition to add it to.")

    fd, path = tempfile.mkstemp(dir="/run", prefix="wildos-key-")
    try:
        os.fchmod(fd, 0o600)
        os.write(fd, key.encode("utf-8"))
        os.close(fd)
        for device in devices:
            result = subprocess.run(["cryptsetup", "luksAddKey", "--batch-mode", "--key-file=-", device, path],
                                    input=passphrase.encode("utf-8"), capture_output=True)
            if result.returncode != 0:
                reason = result.stderr.decode(errors="replace").strip()
                libcalamares.utils.warning("Recovery key not added to {}: {}".format(device, reason))
                return ("The recovery key could not be added",
                        "cryptsetup could not add it to {}: {}".format(device, reason))
    finally:
        try:
            os.unlink(path)
        except OSError:
            pass
    gs.insert("wildosRecoveryAdded", True)
    libcalamares.utils.debug("Recovery key added to {}".format(", ".join(devices)))
    return None
