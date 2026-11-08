"""What the host can spare: mains or battery, free memory and memory
pressure, which the Automatic idle policy weighs (brief 5.6)."""

import os

from gi.repository import Gio, GLib


def meminfo(path="/proc/meminfo"):
    values = {}
    try:
        with open(path) as f:
            for line in f:
                key, _, rest = line.partition(":")
                parts = rest.split()
                if parts and parts[0].isdigit():
                    values[key] = int(parts[0]) * 1024
    except OSError:
        pass
    return values.get("MemAvailable", 0), values.get("MemTotal", 0)


def pressure(path="/proc/pressure/memory"):
    """The share of the last ten seconds some task was stalled on memory."""
    try:
        with open(path) as f:
            for line in f:
                if line.startswith("some "):
                    for field in line.split()[1:]:
                        if field.startswith("avg10="):
                            return float(field[6:])
    except (OSError, ValueError):
        pass
    return 0.0


def under_pressure(threshold=10.0):
    return pressure() >= threshold


class Power:
    """UPower's OnBattery, watched. A PC without UPower (a desktop, WSL)
    counts as on mains."""

    def __init__(self, bus, on_change):
        self.on_battery = False
        self.proxy = None
        try:
            self.proxy = Gio.DBusProxy.new_sync(bus, Gio.DBusProxyFlags.NONE, None, "org.freedesktop.UPower",
                                                "/org/freedesktop/UPower", "org.freedesktop.UPower", None)
            v = self.proxy.get_cached_property("OnBattery")
            self.on_battery = bool(v.unpack()) if v is not None else False

            def changed(proxy, changed_props, invalidated):
                v = changed_props.lookup_value("OnBattery", None)
                if v is not None:
                    self.on_battery = bool(v.unpack())
                    on_change()

            self.proxy.connect("g-properties-changed", changed)
        except GLib.Error:
            self.proxy = None


class PressureWatch:
    """A PSI trigger on memory: the kernel wakes us when tasks stall on
    memory for half a second within two, rather than us polling."""

    def __init__(self, on_change, path="/proc/pressure/memory"):
        self.fd = None
        try:
            self.fd = os.open(path, os.O_RDWR | os.O_NONBLOCK)
            os.write(self.fd, b"some 500000 2000000\0")
        except OSError:
            if self.fd is not None:
                os.close(self.fd)
            self.fd = None
            return

        def fired(fd, cond):
            on_change()
            return True

        GLib.io_add_watch(self.fd, GLib.PRIORITY_LOW, GLib.IO_PRI, fired)
