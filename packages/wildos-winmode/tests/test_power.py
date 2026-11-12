"""Save to disk, Resume, Start, Restart and Shut down, and the question
asked before Windows or Wild OS shuts down with work open."""

import base64
import json
import os
import unittest

from winmode import documents, rdp

DATA = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "data")


def window(image, title, **extra):
    return dict({"hwnd": hash(title) & 0xffff, "image": image, "title": title, "visible": True, "tool": False, "owner": 0}, **extra)


class Documents(unittest.TestCase):
    def test_office_says_itself_and_titles_say_the_rest(self):
        excel = r"C:\Program Files\Microsoft Office\root\Office16\EXCEL.EXE"
        ws = [
            window(excel, "Budget 2027 - Excel"),
            window(excel, "Book1 - Excel"),
            window(r"C:\Program Files\Microsoft Office\root\Office16\WINWORD.EXE", "Quote.docx - Word"),
            # Windows 11's Notepad, as seen in the test VM.
            window(r"C:\Windows\System32\notepad.exe", "*save and restore test, unsaved - Notepad"),
            window(r"C:\Windows\System32\notepad.exe", "notes.txt - Notepad"),
            window(r"C:\Program Files\Some\viewer.exe", "Map"),
            window(r"C:\Windows\explorer.exe", "Documents"),
            window(r"C:\Windows\System32\notepad.exe", "Save as", owner=12),
        ]
        office = [{"app": "excel", "name": "Budget 2027.xlsx", "saved": False},
                  {"app": "excel", "name": "Book1", "saved": True},
                  {"app": "winword", "name": "Quote.docx", "saved": True}]
        docs = documents.documents(ws, office)
        got = {d["document"]: d["unsaved"] for d in docs}
        self.assertEqual(got, {"Budget 2027.xlsx": True, "Book1": False, "Quote.docx": False,
                               "save and restore test, unsaved": True, "notes.txt": False, "Map": None})
        # Unsaved first, then what can't be told, then what is saved.
        self.assertEqual([d["unsaved"] for d in docs], [True, True, None, False, False, False])
        self.assertEqual(docs[0]["app"], "Excel")

    def test_office_out_of_reach_is_not_taken_as_saved(self):
        ws = [window(r"C:\O\WINWORD.EXE", "Document1 - Word")]
        self.assertIsNone(documents.documents(ws, None)[0]["unsaved"])
        # A document Office didn't list (a second Word, say).
        self.assertIsNone(documents.documents(ws, [{"app": "winword", "name": "Other.docx", "saved": True}])[0]["unsaved"])

    def test_dot_markers_and_app_names(self):
        ws = [window(r"C:\VS\Code.exe", "\u25cf main.py - project - Visual Studio Code")]
        doc = documents.documents(ws, None, lambda image: "Visual Studio Code")[0]
        self.assertEqual((doc["app"], doc["document"], doc["unsaved"]), ("Visual Studio Code", "main.py - project", True))

    def test_asking_first(self):
        self.assertFalse(documents.needs_asking([]))
        self.assertFalse(documents.needs_asking([{"unsaved": False}]))
        self.assertTrue(documents.needs_asking([{"unsaved": False}, {"unsaved": None}]))
        self.assertTrue(documents.needs_asking([{"unsaved": True}]))


class SaveToDisk(unittest.TestCase):
    OLD = """<domain type='kvm'>
  <name>wildos-windows</name>
  <memoryBacking>
    <source type='memfd'/>
    <access mode='shared'/>
  </memoryBacking>
  <devices>
    <disk type='file' device='disk'><source file='/x.qcow2'/></disk>
    <filesystem type='mount' accessmode='passthrough'>
      <driver type='virtiofs' queue='1024'/>
      <binary path='/usr/libexec/virtiofsd'/>
      <source dir='/var/lib/wildos/winmode/share'/>
      <target dir='wildos'/>
    </filesystem>
    <filesystem type='mount'><source dir='/other'/><target dir='9p'/></filesystem>
    <channel type='unix'><target type='virtio' name='org.wildos.guest.0'/></channel>
  </devices>
</domain>"""

    def test_the_old_shared_folder_comes_out(self):
        from winmode import system
        self.assertTrue(system.has_virtiofs(self.OLD))
        new = system.drop_virtiofs(self.OLD)
        self.assertFalse(system.has_virtiofs(new))
        self.assertNotIn("memoryBacking", new)
        # Everything else stays, another kind of shared folder included.
        self.assertIn("target dir='9p'", new)
        self.assertIn("org.wildos.guest.0", new)
        self.assertIn("/x.qcow2", new)
        with open(os.path.join(DATA, "templates", "domain.xml.in"), encoding="utf-8") as f:
            template = f.read()
        self.assertFalse(system.has_virtiofs(template))
        self.assertEqual(system.drop_virtiofs(template), template)

    def test_since_survives_a_service_restart_only_when_it_still_holds(self):
        from winmode import system
        s = system.Service.__new__(system.Service)
        s.state = {"since": {"state": "shut off", "at": 100}}
        s.save_state = lambda: None
        s.note_since("", "shut off")
        self.assertEqual(s.state["since"], {"state": "shut off", "at": 100})
        s.note_since("", "paused")
        self.assertEqual(s.state["since"], {"state": "paused", "at": None})
        s.state["kept"] = {"at": 1}
        s.note_since("paused", "saved")
        self.assertIn("kept", s.state)
        self.assertGreater(s.state["since"]["at"], 0)
        s.note_since("saved", "running")
        self.assertNotIn("kept", s.state)

    def test_a_reconnection_brings_windows_back_without_starting_an_app(self):
        args = rdp.resume_arguments("192.168.207.10", 3389, "wild", "PC-W", drive="/tmp/drive")
        app = [a for a in args if a.startswith("/app:")][0]
        start = json.loads(base64.b64decode(app.split("cmd:--start ", 1)[1]))
        self.assertEqual(start["path"], "")
        self.assertIn("+auto-reconnect", args)


class Defer:
    def __init__(self):
        self.out = []

    def done(self, value):
        self.out.append(value)


class Power(unittest.TestCase):
    def agent(self, docs):
        from winmode import agent
        a = agent.Agent.__new__(agent.Agent)
        a.calls, a.asked, a.closed, a.answers = [], [], [], []
        a.windows, a.host, a.registry, a.waking, a.preparing = [], {}, {}, False, None
        a.conn = type("Conn", (), {"open": True, "running": True, "close": lambda c: a.closed.append(True)})()
        a.desktop = type("Desktop", (), {"close": lambda c: None})()
        a.documents_now = lambda done, timeout=20, unanswered=None: unanswered() if docs is None and unanswered else done(docs or [])
        a.ask = lambda action, d, answered: (a.asked.append(action), answered(a.answers.pop(0)))
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: (
            a.calls.append((method, args)), done and done({"ok": True}))
        a.refresh_host = lambda then=None: None
        a.reopen_windows = lambda within=90: None
        a.prepare_session = lambda: None
        a.notify = lambda text: a.calls.append(("notify", text))
        return a

    def test_shut_down_with_nothing_unsaved_goes_ahead(self):
        a = self.agent([{"app": "Notepad", "document": "a.txt", "unsaved": False}])
        d = Defer()
        a.m_power(None, "shutdown", d)
        self.assertEqual(a.asked, [])
        self.assertEqual(a.calls, [("Power", ("shutdown",))])
        self.assertEqual(d.out, [""])
        # The connection ends first, or it would reconnect by itself.
        self.assertTrue(a.closed)

    def test_restart_with_unsaved_work_asks_and_go_back_changes_nothing(self):
        a = self.agent([{"app": "Excel", "document": "Budget", "unsaved": True}])
        a.answers = ["back"]
        d = Defer()
        a.m_power(None, "restart", d)
        self.assertEqual(a.asked, ["windows-restart"])
        self.assertEqual(a.calls, [])
        self.assertEqual(d.out, [""])

    def test_save_to_disk_instead_of_shutting_down(self):
        a = self.agent([{"app": "Excel", "document": "Budget", "unsaved": True}])
        a.answers = ["keep"]
        a.m_power(None, "shutdown", Defer())
        self.assertEqual(a.calls, [("Power", ("save",))])

    def test_switched_off_is_said(self):
        a = self.agent([])
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: done({"ok": True, "forced": True})
        d = Defer()
        a.m_power(None, "shutdown", d)
        self.assertIn("switched it off", d.out[0])

    def leave(self, docs, answer, keep_ok=True):
        a = self.agent(docs)
        a.leaving = type("L", (), {"current": lambda self: "restart"})()
        a.answers = [answer]
        if not keep_ok:
            a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: (
                a.calls.append((method, args)), done and done({"ok": args != ("keep",), "error": "x"}))
        finished = []
        a.ask_before_leaving(finished.append)
        return a, finished

    def test_leaving_keeps_work_by_default(self):
        a, finished = self.leave([{"app": "Excel", "document": "Budget", "unsaved": True}], "keep")
        self.assertEqual(a.asked, ["restart"])
        self.assertEqual(a.calls, [("Power", ("keep",))])
        self.assertEqual(finished, [False])
        self.assertTrue(a.closed)

    def test_leaving_unasked_is_kept_too(self):
        a, finished = self.leave([{"app": "Excel", "document": "Budget", "unsaved": True}], "error")
        self.assertEqual(a.calls, [("Power", ("keep",))])

    def test_leaving_goes_back_or_closes_without_saving(self):
        a, finished = self.leave([{"app": "Excel", "document": "Budget", "unsaved": True}], "back")
        self.assertEqual((a.calls, finished), ([], [True]))
        a, finished = self.leave([{"app": "Excel", "document": "Budget", "unsaved": True}], "discard")
        self.assertEqual((a.calls, finished), ([("Power", ("shutdown",))], [False]))

    def test_leaving_with_nothing_open_asks_nothing(self):
        a, finished = self.leave([], "keep")
        self.assertEqual((a.asked, a.calls, finished), ([], [], [False]))

    def test_a_missing_service_never_holds_the_session(self):
        # winmoded not answering: nothing asked, the session ends.
        a, finished = self.leave(None, "keep")
        self.assertEqual((a.asked, a.calls, finished), ([], [], [False]))

    def test_a_save_that_does_not_answer_lets_the_session_end(self):
        a = self.agent([{"app": "Excel", "document": "Budget", "unsaved": True}])
        a.leaving = type("L", (), {"current": lambda self: "signout"})()
        a.answers = ["keep"]
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: done(
            {"ok": False, "error": "Timeout was reached", "unanswered": True})
        finished = []
        a.ask_before_leaving(finished.append)
        self.assertEqual(finished, [False])

    def test_the_session_manager_is_never_left_waiting(self):
        from unittest import mock
        from winmode import xsmp
        c = xsmp.Client.__new__(xsmp.Client)
        c.log, c.pending, c.conn = (lambda *a: None), True, 1
        done = []
        c.sm = type("SM", (), {"SmcInteractDone": lambda self, conn, cancel: done.append(("interact", cancel)),
                               "SmcSaveYourselfDone": lambda self, conn, ok: done.append(("saved", ok))})()
        timers = []
        c.ask = lambda finish: None  # the question never comes back
        with mock.patch.object(xsmp.GLib, "timeout_add_seconds", lambda s, fn: (timers.append((s, fn)), 7)[1]), \
                mock.patch.object(xsmp.GLib, "source_remove", lambda t: None):
            c._interact_granted(None, None)
            self.assertEqual(timers[0][0], xsmp.Client.INTERACT_LIMIT)
            timers[0][1]()
        self.assertEqual(done, [("interact", 0), ("saved", 1)])

    def test_a_save_that_fails_calls_the_shutdown_off_and_says_so(self):
        a, finished = self.leave([{"app": "Excel", "document": "Budget", "unsaved": True}], "keep", keep_ok=False)
        self.assertEqual(finished, [True])
        self.assertTrue(any(c[0] == "notify" and "didn't restart" in c[1] for c in a.calls))
        # And the apps come back.
        self.assertIn(("Power", ("resume",)), a.calls)

    def test_a_connection_ends_as_windows_goes_to_disk(self):
        a = self.agent([])
        a.end_connection_if_gone({"action": "save", "vm": "running"})
        self.assertEqual(a.closed, [True])
        a.closed.clear()
        a.end_connection_if_gone({"action": None, "vm": "paused"})
        self.assertEqual(a.closed, [])


if __name__ == "__main__":
    unittest.main()
