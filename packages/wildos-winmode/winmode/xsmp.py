"""Taking part in Plasma's sign-out, shut down and restart the way an app
with unsaved work does: as a client of ksmserver's X session management
(XSMP, through libSM). When the session ends, ksmserver asks every client
to save itself and lets one at a time interact with the person; a client
can then call the whole thing off. Plasma 6 does this before KWin closes
the remaining windows (plasma-shutdown: closeSession, then
closeWaylandWindows), so Windows apps are still open while the question
is asked, and ksmserver's 15 second limit for clients is stopped while
one interacts.

ksmserver puts its address and the ICE authority file in the session's
environment (SESSION_MANAGER, ICEAUTHORITY), which a D-Bus started agent
may have started without; both are read from systemd's user environment
when missing.

LeaveWatcher says which of the three is happening: the session bus calls
to org.kde.Shutdown, which every way of leaving in Plasma goes through
(Start's power menu, Win+X, the lock screen, Settings and the restart
prompt after updates), seen as a monitor.
"""

import ctypes
import ctypes.util
import os
import time

from gi.repository import Gio, GLib

SM_INTERACT_STYLE_NONE = 0
SM_DIALOG_NORMAL = 1
SM_RESTART_NEVER = 3

_SaveYourself = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_int, ctypes.c_int)
_Simple = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_void_p)
_IceIOError = ctypes.CFUNCTYPE(None, ctypes.c_void_p)
_IceError = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                             ctypes.c_void_p)
_SmcError = ctypes.CFUNCTYPE(None, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong, ctypes.c_int, ctypes.c_int,
                             ctypes.c_void_p)


class _Callbacks(ctypes.Structure):
    _fields_ = [("save_yourself", _SaveYourself), ("save_yourself_data", ctypes.c_void_p),
                ("die", _Simple), ("die_data", ctypes.c_void_p),
                ("save_complete", _Simple), ("save_complete_data", ctypes.c_void_p),
                ("shutdown_cancelled", _Simple), ("shutdown_cancelled_data", ctypes.c_void_p)]


class _PropValue(ctypes.Structure):
    _fields_ = [("length", ctypes.c_int), ("value", ctypes.c_void_p)]


class _Prop(ctypes.Structure):
    _fields_ = [("name", ctypes.c_char_p), ("type", ctypes.c_char_p), ("num_vals", ctypes.c_int),
                ("vals", ctypes.POINTER(_PropValue))]


def _libraries():
    sm = ctypes.CDLL(ctypes.util.find_library("SM") or "libSM.so.6")
    ice = ctypes.CDLL(ctypes.util.find_library("ICE") or "libICE.so.6")
    sm.SmcOpenConnection.restype = ctypes.c_void_p
    sm.SmcOpenConnection.argtypes = [ctypes.c_char_p, ctypes.c_void_p, ctypes.c_int, ctypes.c_int, ctypes.c_ulong,
                                     ctypes.POINTER(_Callbacks), ctypes.c_char_p, ctypes.POINTER(ctypes.c_void_p), ctypes.c_int,
                                     ctypes.c_char_p]
    sm.SmcGetIceConnection.restype = ctypes.c_void_p
    sm.SmcGetIceConnection.argtypes = [ctypes.c_void_p]
    sm.SmcInteractRequest.restype = ctypes.c_int
    sm.SmcInteractRequest.argtypes = [ctypes.c_void_p, ctypes.c_int, _Simple, ctypes.c_void_p]
    sm.SmcInteractDone.argtypes = [ctypes.c_void_p, ctypes.c_int]
    sm.SmcSaveYourselfDone.argtypes = [ctypes.c_void_p, ctypes.c_int]
    sm.SmcSetProperties.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.POINTER(ctypes.POINTER(_Prop))]
    sm.SmcCloseConnection.argtypes = [ctypes.c_void_p, ctypes.c_int, ctypes.c_void_p]
    sm.SmcSetErrorHandler.argtypes = [_SmcError]
    sm.SmcSetErrorHandler.restype = ctypes.c_void_p
    ice.IceConnectionNumber.restype = ctypes.c_int
    ice.IceConnectionNumber.argtypes = [ctypes.c_void_p]
    ice.IceProcessMessages.restype = ctypes.c_int
    ice.IceProcessMessages.argtypes = [ctypes.c_void_p, ctypes.c_void_p, ctypes.c_void_p]
    ice.IceSetIOErrorHandler.argtypes = [_IceIOError]
    ice.IceSetIOErrorHandler.restype = ctypes.c_void_p
    ice.IceSetErrorHandler.argtypes = [_IceError]
    ice.IceSetErrorHandler.restype = ctypes.c_void_p
    return sm, ice


def session_environment(bus, names):
    """Values from systemd's user environment, where ksmserver puts its own."""
    try:
        value = bus.call_sync("org.freedesktop.systemd1", "/org/freedesktop/systemd1", "org.freedesktop.DBus.Properties", "Get",
                              GLib.Variant("(ss)", ("org.freedesktop.systemd1.Manager", "Environment")), GLib.VariantType("(v)"),
                              Gio.DBusCallFlags.NONE, 3000, None).unpack()[0]
    except GLib.Error:
        return {}
    out = {}
    for item in value:
        key, _, val = item.partition("=")
        if key in names:
            out[key] = val
    return out


class Client:
    """One XSMP connection to ksmserver. wanted() says whether the person
    should be asked now (Windows apps are open); ask(finish) asks them and
    calls finish(cancel) once, cancel True to call the session's end off.
    Connects when it can and again after ksmserver goes away."""

    RETRY_SECONDS = 30
    # ksmserver waits for a client that is asking the person with no limit
    # of its own, so this is the limit: whatever happens to the question,
    # the dialog or Windows mode's services, the session's end goes ahead
    # after this long.
    INTERACT_LIMIT = 150

    def __init__(self, bus, wanted, ask, log):
        self.bus = bus
        self.wanted = wanted
        self.ask = ask
        self.log = log
        self.conn = None
        self.watch = None
        self.pending = False
        self.address = None
        try:
            self.sm, self.ice = _libraries()
        except OSError as e:
            log("session end checks are off (libSM):", e)
            self.sm = None
            return
        # libICE and libSM end the process on a lost connection unless told
        # otherwise; a lost ksmserver only means connecting again.
        self._io_error = _IceIOError(lambda ice: None)
        self._ice_error = _IceError(lambda *a: None)
        self._smc_error = _SmcError(lambda *a: None)
        self.ice.IceSetIOErrorHandler(self._io_error)
        self.ice.IceSetErrorHandler(self._ice_error)
        self.sm.SmcSetErrorHandler(self._smc_error)
        self._callbacks = _Callbacks(_SaveYourself(self._save_yourself), None, _Simple(self._die), None,
                                     _Simple(lambda conn, data: None), None, _Simple(self._cancelled), None)
        self._interact = _Simple(self._interact_granted)
        self.connect()
        GLib.timeout_add_seconds(self.RETRY_SECONDS, self._retry)

    def _retry(self):
        if self.conn is None:
            self.connect()
        return True

    def connect(self):
        env = session_environment(self.bus, ("SESSION_MANAGER", "ICEAUTHORITY"))
        address = os.environ.get("SESSION_MANAGER") or env.get("SESSION_MANAGER")
        if not address:
            return False
        # libICE reads the authority file's name from the environment.
        if env.get("ICEAUTHORITY"):
            os.environ["ICEAUTHORITY"] = env["ICEAUTHORITY"]
        error = ctypes.create_string_buffer(256)
        client_id = ctypes.c_void_p()
        conn = self.sm.SmcOpenConnection(address.encode(), None, 1, 0, 15, ctypes.byref(self._callbacks), None,
                                         ctypes.byref(client_id), len(error), error)
        if not conn:
            if address != self.address:
                self.log("couldn't reach the session manager:", error.value.decode(errors="replace"))
            self.address = address
            return False
        if client_id.value:
            ctypes.CDLL(None).free(client_id)
        self.conn, self.address = conn, address
        self._set_properties()
        fd = self.ice.IceConnectionNumber(self.sm.SmcGetIceConnection(conn))
        self.watch = GLib.io_add_watch(fd, GLib.PRIORITY_DEFAULT, GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, self._readable)
        self.log("taking part in the session's end, to keep Windows apps' work")
        return True

    def _set_properties(self):
        """What XSMP asks every client to say about itself, and that it is
        never to be started again by the session: the agent starts by
        itself when needed."""
        keep = []

        def value(text):
            b = ctypes.create_string_buffer(text.encode())
            keep.append(b)
            return _PropValue(len(text.encode()), ctypes.cast(b, ctypes.c_void_p))

        def prop(name, kind, values):
            arr = (_PropValue * len(values))(*values)
            keep.append(arr)
            p = _Prop(name.encode(), kind.encode(), len(values), arr)
            keep.append(p)
            return ctypes.pointer(p)

        hint = ctypes.create_string_buffer(bytes([SM_RESTART_NEVER]), 1)
        keep.append(hint)
        props = [
            prop("Program", "ARRAY8", [value("wildos-winmode")]),
            prop("UserID", "ARRAY8", [value(str(os.getuid()))]),
            prop("RestartCommand", "LISTofARRAY8", [value("wildos-winmode"), value("agent")]),
            prop("CloneCommand", "LISTofARRAY8", [value("wildos-winmode"), value("agent")]),
            prop("RestartStyleHint", "CARD8", [_PropValue(1, ctypes.cast(hint, ctypes.c_void_p))]),
        ]
        array = (ctypes.POINTER(_Prop) * len(props))(*props)
        self.sm.SmcSetProperties(self.conn, len(props), array)

    def _readable(self, fd, cond):
        if self.conn is None:
            return False
        status = self.ice.IceProcessMessages(self.sm.SmcGetIceConnection(self.conn), None, None)
        if status != 0 or cond & (GLib.IO_HUP | GLib.IO_ERR):
            self._lost(status)
            return False
        return True

    def _lost(self, status=0):
        self.log("the session manager went away", f"(ICE status {status})" if status else "")
        self.watch = None
        self.conn = None
        self.pending = False

    def _save_yourself(self, conn, data, save_type, shutdown, interact_style, fast):
        self.log("the session is ending" if shutdown else "the session is being saved", f"(interact style {interact_style})")
        try:
            if shutdown and interact_style != SM_INTERACT_STYLE_NONE and self._safe(self.wanted):
                if self.sm.SmcInteractRequest(conn, SM_DIALOG_NORMAL, self._interact, None):
                    self.pending = True
                    return
        except Exception as e:
            self.log("session end check:", e)
        self.sm.SmcSaveYourselfDone(conn, 1)

    def _interact_granted(self, conn, data):
        answered = []
        timer = []

        def finish(cancel):
            if timer:
                GLib.source_remove(timer.pop())
            if answered or not self.pending or self.conn is None:
                return
            answered.append(cancel)
            self.pending = False
            self.sm.SmcInteractDone(self.conn, 1 if cancel else 0)
            self.sm.SmcSaveYourselfDone(self.conn, 1)

        def too_long():
            timer.clear()
            self.log(f"no answer about Windows apps in {self.INTERACT_LIMIT} s: letting the session end")
            finish(False)
            return False

        timer.append(GLib.timeout_add_seconds(self.INTERACT_LIMIT, too_long))
        try:
            self.ask(finish)
        except Exception as e:
            # Never leave the session hanging on a question nobody asked.
            self.log("asking before the session ends:", e)
            finish(False)

    def _cancelled(self, conn, data):
        # Another client called the session's end off: a question still on
        # screen answers into nothing (finish sees pending is gone).
        self.pending = False

    def _die(self, conn, data):
        if self.watch:
            GLib.source_remove(self.watch)
            self.watch = None
        self.sm.SmcCloseConnection(conn, 0, None)
        self.conn = None

    def _safe(self, fn):
        try:
            return bool(fn())
        except Exception as e:
            self.log("session end check:", e)
            return False


class LeaveWatcher:
    """Which way the session is ending, from the calls Plasma makes to
    org.kde.Shutdown, seen on a monitor connection of its own (the session
    bus lets its own user monitor it)."""

    ACTIONS = {"logoutAndShutdown": "shutdown", "logoutAndReboot": "restart", "logout": "signout"}
    # A choice older than this belongs to a sign-out that was called off.
    FRESH = 300

    def __init__(self, log):
        self.action = None
        self.at = 0.0
        self.conn = None
        try:
            address = Gio.dbus_address_get_for_bus_sync(Gio.BusType.SESSION, None)
            self.conn = Gio.DBusConnection.new_for_address_sync(
                address, Gio.DBusConnectionFlags.AUTHENTICATION_CLIENT | Gio.DBusConnectionFlags.MESSAGE_BUS_CONNECTION, None, None)
            self.conn.add_filter(self._filter)
            rules = [f"type='method_call',interface='org.kde.Shutdown',member='{m}'" for m in self.ACTIONS]
            self.conn.call_sync("org.freedesktop.DBus", "/org/freedesktop/DBus", "org.freedesktop.DBus.Monitoring", "BecomeMonitor",
                                GLib.Variant("(asu)", (rules, 0)), None, Gio.DBusCallFlags.NONE, 3000, None)
        except GLib.Error as e:
            log("can't tell shutting down from restarting:", e.message)
            self.conn = None

    def _filter(self, conn, message, incoming):
        # GDBus's own thread: only plain assignments here. The calls seen
        # as a monitor are dropped once read; anything else, the outgoing
        # BecomeMonitor and its reply included, goes on as usual.
        if not incoming or message.get_message_type() != Gio.DBusMessageType.METHOD_CALL:
            return message
        if message.get_interface() == "org.kde.Shutdown":
            action = self.ACTIONS.get(message.get_member())
            if action:
                self.action, self.at = action, time.monotonic()
        return None

    def current(self):
        """shutdown, restart or signout; shutdown when nothing was seen."""
        if self.action and time.monotonic() - self.at < self.FRESH:
            return self.action
        return "shutdown"
