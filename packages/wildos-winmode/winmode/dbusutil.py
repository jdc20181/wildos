"""A small layer over Gio's D-Bus object export: methods are plain Python
callables that return a value, or take the invocation and answer later
(for work that waits on libvirt or the guest)."""

import inspect
import threading

from gi.repository import Gio, GLib


class Deferred:
    """Returned by a method that answers later: call done(value) once, from
    any thread."""

    def __init__(self, invocation, signature):
        self.invocation = invocation
        self.signature = signature
        self.answered = False

    def done(self, value):
        def answer():
            if not self.answered:
                self.answered = True
                self.invocation.return_value(GLib.Variant("(" + self.signature + ")", (value,)))
            return False
        GLib.idle_add(answer)

    def error(self, message):
        def answer():
            if not self.answered:
                self.answered = True
                self.invocation.return_dbus_error("org.wildos.WindowsMode1.Error", message)
            return False
        GLib.idle_add(answer)


def in_thread(fn, *args, done=None):
    """Runs fn in a thread; done(result or exception) back on the main loop."""
    def work():
        try:
            result = fn(*args)
        except Exception as e:
            result = e
        if done:
            GLib.idle_add(lambda: (done(result), False)[1])
    t = threading.Thread(target=work, daemon=True)
    t.start()
    return t


class Exported:
    """Exports one interface. methods maps a method name to (callable, reply
    signature or None, needs authorisation). A callable gets (sender,
    *args); one that takes a defer keyword may return that Deferred and
    answer through it later."""

    def __init__(self, connection, path, xml, interface, methods, authorize=None):
        self.connection = connection
        self.path = path
        self.interface = interface
        self.methods = methods
        self.authorize = authorize
        node = Gio.DBusNodeInfo.new_for_xml(xml)
        self.info = node.lookup_interface(interface)
        self.id = connection.register_object(path, self.info, self._call, None, None)

    def _call(self, connection, sender, path, interface, method, params, invocation):
        entry = self.methods.get(method)
        if entry is None:
            invocation.return_dbus_error("org.freedesktop.DBus.Error.UnknownMethod", method)
            return
        fn, signature, needs_auth = entry
        args = params.unpack()
        deferring = "defer" in inspect.signature(fn).parameters

        def run():
            try:
                defer = Deferred(invocation, signature) if deferring else None
                result = fn(sender, *args, defer=defer) if deferring else fn(sender, *args)
                if signature is None:
                    invocation.return_value(None)
                elif result is not defer:
                    invocation.return_value(GLib.Variant("(" + signature + ")", (result,)))
            except Exception as e:
                invocation.return_dbus_error("org.wildos.WindowsMode1.Error", str(e))

        if needs_auth and self.authorize:
            self.authorize(sender, lambda ok: run() if ok else invocation.return_dbus_error(
                "org.freedesktop.DBus.Error.AccessDenied", "Not allowed to use Windows mode"))
        else:
            run()

    def emit(self, signal, signature=None, *values):
        self.connection.emit_signal(None, self.path, self.interface, signal,
                                    GLib.Variant(signature, values) if signature else None)
