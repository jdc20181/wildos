"""The host end of wild-guest (guest/wild-guest), over the virtio-serial
channel org.wildos.guest.0 that libvirt exposes as a socket.

One JSON object per line each way. Everything from the guest is untrusted:
lines over 8 MB are dropped, and anything that is not an object with a
string "type" is ignored. Requests carry a numeric "id" that the reply
repeats; messages without one are events.

To the guest:
  {"type":"hello"}, {"type":"ping"}         answered with hello
  {"type":"time","unix_ms":n}               set the clock (fallback to the guest agent)
  {"type":"windows"}                        the session's windows
  {"type":"window","hwnd":n}                one window, unfiltered
  {"type":"launch","path":"C:\\...","args":[...],"cwd":"..."}
                                            start a program in the session
  {"type":"theme","dark":bool,"accent":"#rrggbb"}
  {"type":"apps","icons":bool}              Start menu programs, icons as .ico base64
  {"type":"icon","path":"C:\\..."}          one program's icon
  {"type":"sessions"}
From the guest:
  {"type":"hello","version":"1","account":"wild","sessions":[n],"boot":"..."}
  {"type":"windows","session":n,"windows":[{hwnd,pid,image,app,title,class,owner,tool,visible,width,height}]}
  {"type":"session","session":n,"state":"up|down"}
  {"type":"session-change","session":n,"event":"SessionLogon|..."}
  {"type":"launched","pid":n}, {"type":"error","error":"..."}, and the
  replies named above.
"""

import json
import socket

from gi.repository import GLib

MAX_LINE = 8 * 1024 * 1024
# wild-guest reopens its end of the channel when it has heard nothing for
# 45 s (a read already waiting misses what follows a reconnect), so the host
# speaks at least this often.
PING_SECONDS = 20


def parse_line(line):
    """A guest line as a message, or None for anything malformed."""
    if len(line) > MAX_LINE:
        return None
    try:
        msg = json.loads(line)
    except (ValueError, UnicodeDecodeError):
        return None
    if not isinstance(msg, dict) or not isinstance(msg.get("type"), str):
        return None
    return msg


class GuestLink:
    def __init__(self, on_event, on_state=None):
        self.on_event = on_event
        self.on_state = on_state or (lambda connected: None)
        self.sock = None
        self.path = None
        self.buf = b""
        self.watch = None
        self.next_id = 1
        self.pending = {}
        self.hello = None
        self.ping_timer = None

    @property
    def connected(self):
        return self.sock is not None and self.hello is not None

    def connect(self, path):
        if self.sock is not None and path == self.path:
            return True
        self.close()
        try:
            s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
            s.connect(path)
            s.setblocking(False)
        except OSError:
            return False
        self.sock, self.path, self.buf = s, path, b""
        self.watch = GLib.io_add_watch(s.fileno(), GLib.PRIORITY_DEFAULT, GLib.IO_IN | GLib.IO_HUP | GLib.IO_ERR, self._readable)
        # A partial line from an earlier connection must not be taken as
        # the start of this one.
        self._write(b"\n")
        self.request({"type": "hello"}, self._hello, timeout=10)
        self.ping_timer = GLib.timeout_add_seconds(PING_SECONDS, self._ping)
        return True

    def _ping(self):
        # wild-guest answers a ping with its hello, so a hello lost to a
        # reconnect is made good here too.
        if self.sock is None:
            self.ping_timer = None
            return False
        self.request({"type": "ping"}, lambda msg: None, timeout=PING_SECONDS)
        return True

    def close(self):
        if self.ping_timer:
            GLib.source_remove(self.ping_timer)
            self.ping_timer = None
        if self.watch:
            GLib.source_remove(self.watch)
            self.watch = None
        if self.sock:
            self.sock.close()
        was = self.connected
        self.sock = None
        self.hello = None
        for cb, timer in list(self.pending.values()):
            GLib.source_remove(timer)
            cb({"type": "error", "error": "disconnected"})
        self.pending.clear()
        if was:
            self.on_state(False)

    def _hello(self, msg):
        if msg.get("type") == "hello":
            self.hello = msg
            self.on_state(True)

    def _write(self, data):
        if not self.sock:
            return False
        try:
            self.sock.setblocking(True)
            self.sock.settimeout(5)
            self.sock.sendall(data)
            return True
        except OSError:
            self.close()
            return False
        finally:
            if self.sock:
                self.sock.setblocking(False)

    def send(self, msg):
        return self._write(json.dumps(msg, separators=(",", ":")).encode() + b"\n")

    def request(self, msg, callback, timeout=15):
        """Sends a request; callback(reply) runs once, with an error reply
        on timeout or disconnect."""
        rid = self.next_id
        self.next_id += 1
        msg = dict(msg, id=rid)

        def expire():
            entry = self.pending.pop(rid, None)
            if entry:
                entry[0]({"type": "error", "error": "timeout"})
            return False

        self.pending[rid] = (callback, GLib.timeout_add_seconds(timeout, expire))
        if not self.send(msg):
            entry = self.pending.pop(rid, None)
            if entry:
                GLib.source_remove(entry[1])
                callback({"type": "error", "error": "disconnected"})

    def _readable(self, fd, cond):
        try:
            data = self.sock.recv(1 << 16) if self.sock else b""
        except BlockingIOError:
            return True
        except OSError:
            data = b""
        if not data:
            self.close()
            return False
        self.buf += data
        while b"\n" in self.buf:
            line, self.buf = self.buf.split(b"\n", 1)
            if line.strip():
                self._dispatch(line)
        if len(self.buf) > MAX_LINE:
            self.buf = b""
        return True

    def _dispatch(self, line):
        msg = parse_line(line)
        if msg is None:
            return
        # Whatever asked for it, a hello means the guest is listening.
        if msg["type"] == "hello" and self.hello is None:
            self._hello(msg)
        rid = msg.get("id")
        if isinstance(rid, int) and rid in self.pending:
            cb, timer = self.pending.pop(rid)
            GLib.source_remove(timer)
            cb(msg)
            return
        self.on_event(msg)
