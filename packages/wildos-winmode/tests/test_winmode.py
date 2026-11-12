import base64
import io
import json
import os
import struct
import tempfile
import time
import unittest

from winmode import compat, firewall, guestlink, launchers, lifecycle, paths, rdp, theme, windows

HERE = os.path.dirname(os.path.abspath(__file__))
DATA = os.path.join(HERE, "..", "data")


def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


class Lifecycle(unittest.TestCase):
    def test_states_from_libvirt(self):
        self.assertEqual(lifecycle.state_from_vm(""), lifecycle.NOT_INSTALLED)
        self.assertEqual(lifecycle.state_from_vm("", setting_up=True), lifecycle.SETTING_UP)
        self.assertEqual(lifecycle.state_from_vm("running"), lifecycle.RUNNING)
        self.assertEqual(lifecycle.state_from_vm("paused"), lifecycle.PAUSED)
        self.assertEqual(lifecycle.state_from_vm("saved"), lifecycle.SAVED)
        self.assertEqual(lifecycle.state_from_vm("shut off"), lifecycle.STOPPED)
        self.assertEqual(lifecycle.state_from_vm("crashed"), lifecycle.ERROR)
        self.assertEqual(lifecycle.state_from_vm("running", error="boom"), lifecycle.ERROR)

    def test_idle_after_sixty_seconds_without_windows(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="running", windows=2)
        self.assertIsNone(life.deadline())
        life.update(10, windows=0)
        self.assertEqual(life.deadline(), 70)
        self.assertFalse(life.due(69.9))
        self.assertTrue(life.due(70))

    def test_a_window_opening_stops_the_timer(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="running", windows=0)
        life.update(30, windows=1)
        self.assertIsNone(life.deadline())
        life.update(40, windows=0)
        self.assertEqual(life.deadline(), 100)

    def test_the_connection_does_not_keep_windows_running(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="running", windows=0, connection=True)
        self.assertEqual(life.deadline(), 60)

    def test_holds_keep_it_running(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="running", windows=0)
        life.hold(5, "launch")
        self.assertIsNone(life.deadline())
        life.hold(8, "launch", False)
        self.assertEqual(life.deadline(), 68)

    def test_no_timer_unless_running(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="paused", windows=0)
        self.assertIsNone(life.deadline())

    def test_policy(self):
        gb = 1024 ** 3
        self.assertEqual(lifecycle.idle_action("keep", True, 1, 100), lifecycle.PAUSE)
        self.assertEqual(lifecycle.idle_action("save", False, 90, 100), lifecycle.SAVE)
        self.assertEqual(lifecycle.idle_action("automatic", False, 8 * gb, 16 * gb), lifecycle.PAUSE)
        self.assertEqual(lifecycle.idle_action("automatic", True, 8 * gb, 16 * gb), lifecycle.SAVE)
        self.assertEqual(lifecycle.idle_action("automatic", False, 3 * gb, 16 * gb), lifecycle.SAVE)
        self.assertEqual(lifecycle.idle_action("automatic", False, 4 * gb, 16 * gb), lifecycle.PAUSE)
        self.assertEqual(lifecycle.idle_action("automatic", False, 8 * gb, 16 * gb, mem_pressure=True), lifecycle.SAVE)

    def test_paused_guest_is_saved_when_power_goes(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="paused")
        self.assertIsNone(life.reconsider_paused("automatic", False, 8, 16))
        self.assertEqual(life.reconsider_paused("automatic", True, 8, 16), lifecycle.SAVE)
        self.assertIsNone(life.reconsider_paused("keep", True, 8, 16))

    def test_long_pause_goes_to_disk_under_automatic_only(self):
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="paused")
        long = lifecycle.SAVE_AFTER_PAUSED
        self.assertIsNone(life.reconsider_paused("automatic", False, 8, 16, paused_seconds=long - 1))
        self.assertEqual(life.reconsider_paused("automatic", False, 8, 16, paused_seconds=long), lifecycle.SAVE)
        self.assertIsNone(life.reconsider_paused("automatic", False, 8, 16, paused_seconds=None))
        self.assertIsNone(life.reconsider_paused("keep", False, 8, 16, paused_seconds=long * 10))
        life.update(1, vm_state="running")
        self.assertIsNone(life.reconsider_paused("automatic", False, 8, 16, paused_seconds=long))


class Paths(unittest.TestCase):
    def setUp(self):
        self.home = tempfile.mkdtemp()
        os.makedirs(os.path.join(self.home, ".config"))
        with open(os.path.join(self.home, ".config", "user-dirs.dirs"), "w") as f:
            f.write('XDG_DOCUMENTS_DIR="$HOME/Dokumente"\nXDG_DOWNLOAD_DIR="$HOME/Downloads"\n')
        for d in ("Dokumente/Work", "Downloads", "Elsewhere"):
            os.makedirs(os.path.join(self.home, d))

    def test_user_folders_map_to_the_drive(self):
        m = paths.folder_map(["Documents", "Downloads"], "/var/lib/wildos/winmode/share", self.home)
        self.assertEqual(m["Documents"], os.path.join(self.home, "Dokumente"))
        f = os.path.join(self.home, "Dokumente", "Work", "Budget 2027.xlsx")
        self.assertEqual(paths.to_guest(f, m), "Z:\\Documents\\Work\\Budget 2027.xlsx")
        self.assertIsNone(paths.to_guest(os.path.join(self.home, "Elsewhere", "a.docx"), m))
        self.assertIsNone(paths.to_guest(os.path.join(self.home, "Dokumente2", "a.docx"), m))

    def test_a_share_root_in_the_home_folder_holds_the_folders(self):
        root = os.path.join(self.home, "winmode-proof")
        m = paths.folder_map(["Documents"], root, self.home)
        self.assertEqual(m["Documents"], os.path.join(root, "Documents"))

    def test_copy_into_picks_a_free_name(self):
        src = os.path.join(self.home, "Elsewhere", "a.docx")
        with open(src, "w") as f:
            f.write("x")
        dest = os.path.join(self.home, "Dokumente")
        self.assertEqual(os.path.basename(paths.copy_into(src, dest)), "a.docx")
        self.assertEqual(os.path.basename(paths.copy_into(src, dest)), "a (2).docx")


class Launchers(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.mkdtemp()
        os.environ["XDG_DATA_HOME"] = os.path.join(self.dir, "data")
        os.environ["XDG_CONFIG_HOME"] = os.path.join(self.dir, "config")

    def test_office_apps_get_their_ids_and_types(self):
        a = launchers.describe({"name": "Word", "target": r"C:\Program Files\Microsoft Office\root\Office16\WINWORD.EXE"})
        self.assertEqual(a["id"], "word")
        self.assertIn("application/vnd.openxmlformats-officedocument.wordprocessingml.document", a["mime"])
        b = launchers.describe({"name": "Invoice Maker 2", "target": r"C:\Program Files\IM\im.exe"})
        self.assertEqual(b["id"], "invoice-maker-2")
        self.assertEqual(b["mime"], [])

    def test_entry(self):
        a = launchers.describe({"name": "Excel", "target": r"C:\X\EXCEL.EXE"})
        text = launchers.entry_text(a, "wildos-winmode-excel")
        self.assertIn("Exec=wildos-winmode launch excel %F\n", text)
        self.assertIn("StartupWMClass=wildos-winmode-excel\n", text)
        self.assertIn("X-WildOS-Engine=windows-mode\n", text)
        self.assertNotIn("X-WildOS-WindowsMode", text)

    def test_windows_map_to_apps_by_program(self):
        reg = {"word": {"target": r"C:\Program Files\Microsoft Office\root\Office16\WINWORD.EXE"},
               "invoice": {"target": r"C:\Program Files\IM\im.exe"}}
        self.assertEqual(launchers.app_id_for_image(r"c:\program files\microsoft office\root\office16\winword.exe", reg), "word")
        self.assertEqual(launchers.app_id_for_image(r"D:\Other\IM.EXE", reg), "invoice")
        self.assertEqual(launchers.app_id_for_image(r"C:\Program Files\Microsoft Office\root\Office16\EXCEL.EXE", reg), "excel")
        self.assertEqual(launchers.app_id_for_image(r"C:\Tools\New Thing.exe", reg), "new-thing")
        self.assertTrue(launchers.is_shell(r"C:\Windows\System32\rdpinit.exe"))
        self.assertFalse(launchers.is_shell(r"C:\X\WINWORD.EXE"))

    def test_open_windows_for_the_taskbar_panel(self):
        reg = {"word": {"name": "Word", "target": r"C:\O\WINWORD.EXE"}}
        ws = [{"image": r"C:\O\WINWORD.EXE", "title": "Quarterly report.docx - Word", "owner": 0},
              {"image": r"C:\O\WINWORD.EXE", "title": "Save As", "owner": 4242},
              {"image": r"C:\Windows\System32\rdpinit.exe", "title": "", "owner": 0},
              {"image": r"C:\Tools\notepad.exe", "title": "Notepad", "owner": 0}]
        self.assertEqual(launchers.open_windows(ws, reg), [
            {"id": "word", "name": "Word", "icon": "wildos-winmode-word", "title": "Quarterly report.docx", "installed": True},
            {"id": "notepad", "name": "Notepad", "icon": "wildos-windows-app", "title": "", "installed": False}])
        self.assertEqual(launchers.document_title("Budget 2027.xlsx - Excel", "Excel"), "Budget 2027.xlsx")
        self.assertEqual(launchers.document_title("Report - Draft - Word", "Microsoft Word"), "Report - Draft")
        self.assertEqual(launchers.document_title("Pay - Slip", "Invoices"), "Pay - Slip")

    def test_last_app(self):
        reg = {"word": {}, "excel": {}}
        self.assertEqual(launchers.last_app(reg, "excel", {"word": 9}), "excel")
        self.assertEqual(launchers.last_app(reg, "gone", {"word": 9, "excel": 2, "gone": 50}), "word")
        self.assertIsNone(launchers.last_app(reg, None, {}))

    def test_write_all_makes_and_removes_entries(self):
        reg = launchers.write_all([{"name": "Word", "target": r"C:\O\WINWORD.EXE"}, {"name": "Old", "target": r"C:\O\old.exe"}])
        apps = os.path.join(self.dir, "data", "applications")
        self.assertTrue(os.path.exists(os.path.join(apps, "wildos-winmode-old.desktop")))
        reg = launchers.write_all([{"name": "Word", "target": r"C:\O\WINWORD.EXE"}])
        self.assertEqual(sorted(reg), ["word"])
        self.assertFalse(os.path.exists(os.path.join(apps, "wildos-winmode-old.desktop")))
        self.assertTrue(os.path.exists(os.path.join(apps, "wildos-winmode-word.desktop")))

    def test_icon_from_ico(self):
        from PIL import Image
        img = Image.new("RGBA", (48, 48), (200, 30, 30, 255))
        buf = io.BytesIO()
        img.save(buf, "ICO", sizes=[(16, 16), (48, 48)])
        png = launchers.icon_png(base64.b64encode(buf.getvalue()).decode())
        self.assertEqual(Image.open(io.BytesIO(png)).size, (256, 256))
        self.assertIsNone(launchers.icon_png("not base64 at all"))

    def test_store_apps_get_entries_and_icons(self):
        # wild-guest sends a Store app's logo as a one-image .ico holding
        # the PNG (Icons.FromPng), 0 in the entry meaning 256.
        from PIL import Image

        def ico_of_png(size):
            buf = io.BytesIO()
            Image.new("RGBA", (size, size), (0, 120, 212, 255)).save(buf, "PNG")
            png = buf.getvalue()
            side = 0 if size >= 256 else size
            return base64.b64encode(struct.pack("<HHHBBBBHHII", 0, 1, 1, side, side, 0, 0, 1, 32, len(png), 22) + png).decode()

        for size in (256, 176, 44):
            self.assertEqual(Image.open(io.BytesIO(launchers.icon_png(ico_of_png(size)))).size, (256, 256))
        outlook = {"name": "Outlook (new)", "aumid": "Microsoft.OutlookForWindows_8wekyb3d8bbwe!Microsoft.OutlookforWindows",
                   "target": r"C:\Program Files\WindowsApps\Microsoft.OutlookForWindows_1.2025.1.0_x64__8wekyb3d8bbwe\olk.exe",
                   "arguments": "", "icon": ico_of_png(256)}
        shared = {"name": "Teams Classroom", "aumid": "Contoso.Teams_abcdefghjkmnp!Classroom",
                  "target": r"shell:AppsFolder\Contoso.Teams_abcdefghjkmnp!Classroom", "arguments": ""}
        reg = launchers.write_all([{"name": "Word", "target": r"C:\O\WINWORD.EXE"}, outlook, shared])
        self.assertEqual(sorted(reg), ["outlook-new", "teams-classroom", "word"])
        self.assertEqual(reg["outlook-new"]["aumid"], outlook["aumid"])
        apps = os.path.join(self.dir, "data", "applications")
        with open(os.path.join(apps, "wildos-winmode-outlook-new.desktop")) as f:
            text = f.read()
        self.assertIn("Exec=wildos-winmode launch outlook-new %F\n", text)
        self.assertIn("Icon=wildos-winmode-outlook-new\n", text)
        self.assertNotIn("MimeType", text)
        # The Store updates an app into a folder of a new version; its
        # windows still belong to the app.
        newer = r"C:\Program Files\WindowsApps\Microsoft.OutlookForWindows_1.2026.3.0_x64__8wekyb3d8bbwe\olk.exe"
        self.assertEqual(launchers.app_id_for_image(newer, reg), "outlook-new")
        # A frame-hosted app's window belongs to ApplicationFrameHost;
        # wild-guest reports the app inside it by AUMID.
        self.assertEqual(launchers.app_id_for_aumid("contoso.teams_abcdefghjkmnp!classroom", reg), "teams-classroom")
        self.assertIsNone(launchers.app_id_for_aumid("Other.App_abcdefghjkmnp!App", reg))
        self.assertIsNone(launchers.app_id_for_aumid(None, reg))

    def test_mime_defaults_merge(self):
        path = os.path.join(self.dir, "mimeapps.list")
        with open(path, "w") as f:
            f.write("[Added Associations]\ntext/plain=kate.desktop;\n\n[Default Applications]\ntext/plain=kate.desktop;\n"
                    "application/msword=libreoffice-writer.desktop;\n")
        shipped = launchers.shipped_defaults(os.path.join(DATA, "mimeapps.list"))
        self.assertEqual(shipped["application/msword"], "wildos-winmode-word.desktop")
        reg = {"word": {}}
        launchers.set_defaults(reg, True, shipped, path)
        text = read(path)
        self.assertIn("application/msword=wildos-winmode-word.desktop;", text)
        self.assertNotIn("libreoffice-writer", text)
        self.assertIn("text/plain=kate.desktop;", text)
        self.assertNotIn("excel", text)
        launchers.set_defaults(reg, False, shipped, path)
        text = read(path)
        self.assertNotIn("wildos-winmode", text)
        self.assertIn("[Added Associations]\ntext/plain=kate.desktop;", text)

    def test_mime_defaults_without_a_file(self):
        path = os.path.join(self.dir, "new", "mimeapps.list")
        launchers.set_defaults({"word": {}}, True, {"application/msword": "wildos-winmode-word.desktop"}, path)
        self.assertEqual(read(path), "[Default Applications]\napplication/msword=wildos-winmode-word.desktop;\n")


class Guest(unittest.TestCase):
    def test_lines(self):
        self.assertEqual(guestlink.parse_line(b'{"type":"hello","version":"1"}')["version"], "1")
        self.assertIsNone(guestlink.parse_line(b"[1,2]"))
        self.assertIsNone(guestlink.parse_line(b'{"no":"type"}'))
        self.assertIsNone(guestlink.parse_line(b"\xff\xfe"))
        self.assertIsNone(guestlink.parse_line(b"x" * (guestlink.MAX_LINE + 1)))

    def test_a_hello_in_any_reply_connects(self):
        # After a reconnect the first hello can be lost; the ping's reply is
        # a hello too, and must count.
        states = []
        link = guestlink.GuestLink(lambda msg: None, states.append)
        link.sock = object()
        link._dispatch(b'{"type":"hello","version":"2","id":7}')
        self.assertTrue(link.connected)
        self.assertEqual(states, [True])
        link.sock = None

    def test_rail_class(self):
        self.assertEqual(windows.rail_handle(("RAIL", "RAIL:000301C4")), 0x301C4)
        self.assertIsNone(windows.rail_handle(("wildos-winmode-word", "wildos-winmode-word")))
        self.assertIsNone(windows.rail_handle(None))


class RemoteApp(unittest.TestCase):
    def test_arguments_carry_no_secret_and_any_file_name(self):
        args = rdp.arguments("192.168.207.10", 3389, "wild", "PC-W", "Word", r"C:\O\WINWORD.EXE", [r"Z:\Documents\a, b.docx"])
        self.assertNotIn("/p:", " ".join(args))
        app = [a for a in args if a.startswith("/app:")][0]
        self.assertEqual(app.count(","), 2)
        encoded = app.split("cmd:--start ", 1)[1]
        start = json.loads(base64.b64decode(encoded))
        self.assertEqual(len(start.pop("once")), 16)
        self.assertEqual(start, {"path": r"C:\O\WINWORD.EXE", "args": [r"Z:\Documents\a, b.docx"]})
        self.assertIn("/d:PC-W", args)
        self.assertNotIn("/wm-class", " ".join(args))

    def test_a_file_name_cannot_add_arguments(self):
        # Arguments go one per line on standard input.
        name = "x\n/drive:all,/\r\n.exe"
        args = rdp.arguments("192.168.207.10", 3389, "wild\n/drive:x,/", "PC\n/v:evil", name, r"C:\a.exe", [])
        self.assertTrue(all("\n" not in a and "\r" not in a for a in args))
        self.assertFalse(any(a.startswith("/drive") for a in args))
        app = [a for a in args if a.startswith("/app:")][0]
        self.assertEqual(app.count(","), 2)


NL = chr(10)


class NativeClient(unittest.TestCase):
    """wildos-rdp, Wild OS's own RemoteApp client, and its fallback."""

    def setUp(self):
        self.dir = tempfile.mkdtemp()
        self.client = os.path.join(self.dir, "wildos-rdp")
        # Stands in for the client: keeps its arguments and what it is given
        # on standard input.
        with open(self.client, "w") as f:
            f.write('#!/bin/sh\nprintf "%s\\n" "$@" > "$0.argv"\ncat > "$0.stdin"\n')
        os.chmod(self.client, 0o755)

    def test_the_own_client_unless_missing_or_asked_not_to(self):
        self.assertEqual(rdp.client_program("auto", native=self.client), self.client)
        self.assertEqual(rdp.client_program("xfreerdp3", native=self.client), "xfreerdp3")
        self.assertEqual(rdp.client_program("auto", native=os.path.join(self.dir, "missing")), "xfreerdp3")

    def test_the_desktop_opens_in_xfreerdp3_unless_remote_is_asked_for(self):
        remote = os.path.join(self.dir, "wildos-remote")
        with open(remote, "w") as f:
            f.write("#!/bin/sh\n")
        os.chmod(remote, 0o755)
        self.assertEqual(rdp.desktop_client("xfreerdp3", remote=remote), "xfreerdp3")
        self.assertEqual(rdp.desktop_client("remote", remote=remote), remote)
        self.assertEqual(rdp.desktop_client("remote", remote=os.path.join(self.dir, "missing")), "xfreerdp3")

    def test_loading_arguments_are_one_line_each(self):
        args = rdp.loading_arguments("wildos-winmode-word", "Word\n/drive:all,/", "wildos-winmode-word", "Slow, really",
                                     r"C:\O\WINWORD.EXE")
        self.assertEqual(args[:2], ["--app-id", "wildos-winmode-word"])
        self.assertTrue(all("\n" not in a for a in args))
        # The name is the value of --title, never an argument of its own.
        self.assertEqual(args[args.index("--title") + 1], "Word /drive:all,/")
        self.assertEqual(args[args.index("--reason") + 1], "Slow, really")
        # The program its first window is matched by.
        self.assertEqual(args[-2:], ["--program", r"C:\O\WINWORD.EXE"])

    def wait(self, conn):
        conn.proc.wait(5)

    def test_preparing_a_session_signs_in_only(self):
        args = rdp.prepare_arguments("192.168.207.10", 3389, "wild", "PC", drive="/srv/share")
        # It must never take the session back from the connection an app
        # opens with.
        self.assertNotIn("+auto-reconnect", args)
        self.assertIn("/drive:wildos,/srv/share", args)
        self.assertFalse(any("secret" in a for a in args))
        ended = []
        proc = rdp.prepare_session(args, "secret", os.path.join(self.dir, "log", "p.log"), ended.append, native=self.client)
        proc.wait(5)
        argv = read(self.client + ".argv").split(NL)
        self.assertEqual(argv[:2], ["--prepare-session", "/args-from:stdin"])
        self.assertNotIn("secret", read(self.client + ".argv"))
        self.assertEqual(read(self.client + ".stdin").splitlines()[-1], "/p:secret")

    def test_spawned_early_then_given_the_connection(self):
        changes = []
        conn = rdp.Connection(changes.append, lambda: None, os.path.join(self.dir, "log", "c.log"))
        conn.native_client = self.client
        self.assertTrue(conn.spawn("wildos-winmode-word", "Word", "wildos-winmode-word"))
        self.assertFalse(conn.open)
        self.assertTrue(conn.running)
        self.assertFalse(conn.spawn("x", "y", "z"))
        conn.start(["/v:192.168.207.10:3389", "/u:wild"], "secret")
        self.assertTrue(changes == [True])
        self.wait(conn)
        argv = read(self.client + ".argv").split("\n")
        self.assertIn("--app-id", argv)
        self.assertEqual(argv[-2], "/args-from:stdin")
        self.assertNotIn("secret", read(self.client + ".argv"))
        lines = read(self.client + ".stdin").splitlines()
        self.assertEqual(lines, ["/v:192.168.207.10:3389", "/u:wild", "/p:secret"])

    def test_the_connection_before_keeps_its_log(self):
        path = os.path.join(self.dir, "log", "c.log")
        conn = rdp.Connection(lambda o: None, lambda: None, path)
        conn.native_client = self.client
        conn.spawn("a", "A", "")
        conn.abandon()
        self.wait(conn)
        with open(path, "a") as f:
            f.write("why the first one ended\n")
        conn.spawn("b", "B", "")
        conn.abandon()
        self.wait(conn)
        self.assertIn("why the first one ended", read(path + ".1"))
        self.assertNotIn("why the first one ended", read(path))

    def test_abandoned_before_connecting(self):
        conn = rdp.Connection(lambda o: None, lambda: None, os.path.join(self.dir, "log", "c.log"))
        conn.native_client = self.client
        conn.spawn("wildos-winmode-word", "Word", "")
        conn.abandon()
        self.wait(conn)
        self.assertEqual(read(self.client + ".stdin"), "")
        self.assertFalse(conn.open)

    def test_xfreerdp_is_never_started_early(self):
        conn = rdp.Connection(lambda o: None, lambda: None, os.path.join(self.dir, "c.log"), program="xfreerdp3")
        self.assertFalse(conn.native)
        self.assertFalse(conn.spawn("a", "b", "c"))
        self.assertIsNone(conn.proc)

    def test_slow_reasons_say_only_what_is_known(self):
        from winmode import agent
        self.assertIn("starting up from off", agent.slow_reason("Stopped", "Word"))
        self.assertIn("Word will open", agent.slow_reason("Saved", "Word"))
        self.assertEqual(agent.slow_reason("Running", "Word"), "")
        for text in (agent.slow_reason("Stopped", "Word"), agent.slow_reason("Saved", "Word")):
            # No technical words on screen, and no long dashes (the regex
            # escapes for U+2014 and U+2013).
            self.assertNotRegex(text, "VM|RDP|\\u2014|\\u2013")


class Reliability(unittest.TestCase):
    """Opening and closing that must never hang."""

    def test_ending_a_stuck_program_names_only_that_process(self):
        from winmode import system
        script = system.end_program_script(4242, r"C:\Windows\System32\notepad.exe")
        self.assertIn("Get-Process -Id 4242", script)
        self.assertIn("SessionId -ne 0", script)
        self.assertIn(r"-ieq 'C:\Windows\System32\notepad.exe'", script)
        # A quote in the path cannot end the string early, curly ones too.
        script = system.end_program_script(4242, "C:\\a'b\u2019c.exe")
        self.assertIn("'C:\\a''b\u2019\u2019c.exe'", script)
        for pid, image in ((0, "x.exe"), (4, "x.exe"), ("x", "x.exe"), (100, ""), (100, "a\nb.exe")):
            self.assertIsNone(system.end_program_script(pid, image))

    def test_refreshing_the_menus_never_holds_the_agent(self):
        # kbuildsycoca6 can take seconds; the agent's main loop must go on
        # answering meanwhile.
        from unittest import mock
        from winmode import launchers
        slow = lambda *a, **k: time.sleep(2)
        with mock.patch.object(launchers.subprocess, "run", slow):
            t = time.monotonic()
            launchers.refresh_menus()
            self.assertLess(time.monotonic() - t, 0.5)

    def test_a_program_that_ends_before_its_window_ends_the_wait(self):
        from unittest import mock
        from winmode import agent
        a = self.bare_agent()
        a.watched, a.ended_launches, a.windows = [], [], []
        updates = []
        a.remote = type("Remote", (), {"update": lambda self, token, stage="": updates.append((token, stage))})()
        answers = []
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: done(answers.pop(0))
        polls = []
        with mock.patch.object(agent.GLib, "timeout_add", lambda ms, fn: polls.append(fn)):
            a.watch_launch({"token": 7, "closed": False}, r"C:\Games\Voxel.exe", [], "MagicaVoxel", "voxel")
        poll = polls[0]
        # Not yet started: not running is no news.
        answers.append({"type": "running", "running": False})
        poll()
        answers.append({"type": "running", "running": True})
        poll()
        for _ in range(agent.LAUNCH_GONE_POLLS):
            answers.append({"type": "running", "running": False})
            poll()
        self.assertEqual(updates, [(7, "ended")])
        self.assertEqual(a.ended_launches[0]["name"], "MagicaVoxel")

    def test_try_again_opens_the_same_program_again(self):
        a = self.bare_agent()
        cancelled, launched = [], []
        a.remote = type("Remote", (), {"cancel": lambda self, token: cancelled.append(token)})()
        a.launch = lambda path, args, name, app_id, done: launched.append((path, args, name, app_id))
        # The client's own loading window (token 0 here, 1 to the client).
        a.ended_launches = [{"window": {"token": 0}, "path": "C:/Games/Voxel.exe", "args": ["-x"], "name": "MagicaVoxel",
                             "app_id": "voxel"}]
        a.remote_retry(1)
        self.assertEqual(cancelled, [1])
        self.assertEqual(launched, [("C:/Games/Voxel.exe", ["-x"], "MagicaVoxel", "voxel")])
        self.assertEqual(a.ended_launches, [])

    def test_a_new_window_anywhere_stops_the_watch(self):
        from unittest import mock
        from winmode import agent
        a = self.bare_agent()
        a.watched, a.ended_launches, a.windows = [], [], [{"hwnd": 1}]
        a.remote = type("Remote", (), {"update": lambda self, token, stage="": self.fail("no update")})()
        polls = []
        with mock.patch.object(agent.GLib, "timeout_add", lambda ms, fn: polls.append(fn)):
            a.watch_launch({"token": 7, "closed": False}, r"C:\Windows\notepad.exe", [], "Notepad", "notepad")
        # A launcher handed over to a program of another name, whose window
        # came: no longer watched.
        a.windows.append({"hwnd": 2})
        self.assertFalse(polls[0]())
        self.assertEqual(a.watched, [])

    def bare_agent(self):
        from winmode import agent
        a = agent.Agent.__new__(agent.Agent)
        a.queue, a.waking, a.launches, a.windows, a.loading = [], False, {}, [], {}
        a.removing, a.preparing = False, None
        a.calls = []
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: a.calls.append((method, done))
        a.app_window_stage = lambda app_id, stage: None
        return a

    def test_a_launch_waiting_for_a_session_that_ended_reconnects(self):
        # The last app closed just as this one opened: Windows signed the
        # session out and the connection ended. The launch must open a new
        # connection, not wait 90 s for a session nothing will bring.
        a = self.bare_agent()
        a.conn = type("Conn", (), {"running": False, "open": False})()
        done = []
        a.launch_in_session(r"C:\Windows\notepad.exe", [], "Notepad", "notepad", 0.0, done.append, {"from": "running"})
        self.assertEqual([c[0] for c in a.calls], ["Wake"])
        self.assertEqual(len(a.queue), 1)
        self.assertEqual(a.queue[0][2], "Notepad")
        self.assertEqual(done, [])

    def test_a_launch_in_a_live_session_goes_to_wild_guest(self):
        a = self.bare_agent()
        a.conn = type("Conn", (), {"running": True, "open": True})()
        a.launch_in_session(r"C:\Windows\notepad.exe", [], "Notepad", "notepad", 0.0, lambda m: None, {"from": "running"})
        self.assertEqual([c[0] for c in a.calls], ["Launch"])


class Defer:
    def __init__(self):
        self.out = []

    def done(self, value):
        self.out.append(value)


class Client:
    def __init__(self, running=False):
        self.running, self.started, self.stopped, self.closed = running, [], 0, 0

    def start(self, args, password):
        self.started.append(args)
        self.running = True

    def stop_now(self):
        self.stopped += 1
        self.running = False

    def close(self):
        self.closed += 1
        self.running = False


class Desktop(unittest.TestCase):
    """Settings' Windows desktop, and removing Windows mode (issue 12)."""

    def agent(self, desktop_running=False):
        from winmode import agent
        a = agent.Agent.__new__(agent.Agent)
        a.removing, a.preparing, a.queue = False, None, []
        a.host = {"guest_address": "192.168.207.10", "rdp_port": 3389, "account": "wild"}
        a.conn, a.desktop = Client(), Client(desktop_running)
        a.call_json = lambda method, args=None, signature=None, done=None, timeout=600: done({"ok": True})
        a.credentials = lambda done: done({"account": "wild", "password": "pw", "domain": ""})
        a.drive_folder = lambda: None
        a.changed = lambda: None
        a.hide_starting = lambda: None
        return a

    def test_the_desktop_opens(self):
        a, d = self.agent(), Defer()
        a.m_open_desktop(None, d)
        self.assertEqual(d.out, [""])
        self.assertEqual(len(a.desktop.started), 1)

    def test_a_desktop_left_running_after_its_window_closed_opens_again(self):
        # FreeRDP was seen to stay running after its window closed, and the
        # button then did nothing until the PC restarted.
        a, d = self.agent(desktop_running=True), Defer()
        a.m_open_desktop(None, d)
        self.assertEqual(d.out, [""])
        self.assertEqual(a.desktop.stopped, 1)
        self.assertEqual(len(a.desktop.started), 1)

    def test_restarting_windows_mode_ends_the_desktop_too(self):
        a = self.agent(desktop_running=True)
        a.refresh_host = lambda then=None: None
        a.reopen_windows = lambda within=90: None
        a.prepare_session = lambda: None
        a.power(lambda message: None, "restart")
        self.assertEqual(a.desktop.closed, 1)
        self.assertEqual(a.conn.closed, 1)

    def test_removing_forgets_the_apps_password_and_certificate(self):
        from unittest import mock
        from winmode import agent
        a, d = self.agent(), Defer()
        forgotten, calls = [], []
        a.keyring = type("K", (), {"forget": lambda self, account: forgotten.append(account)})()
        a.call = lambda method, args=None, signature=None, done=None, timeout=600: (calls.append(method), done(""))
        a.refresh_host = lambda then=None: then and then()
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp, "XDG_CONFIG_HOME": tmp}), \
                mock.patch.object(agent.launchers, "remove_all") as remove_all, \
                mock.patch.object(agent.dbusutil, "in_thread", lambda fn, *args, done=None: fn(*args)):
            cert = rdp.certificate_path("192.168.207.10", 3389)
            os.makedirs(os.path.dirname(cert))
            open(cert, "w").close()
            a.m_remove(None, d)
            self.assertFalse(os.path.exists(cert))
        self.assertEqual(calls, ["Remove"])
        self.assertEqual(d.out, [""])
        remove_all.assert_called_once()
        self.assertEqual(forgotten, ["wild"])
        self.assertEqual(a.desktop.closed, 1)
        self.assertFalse(a.removing)

    def test_a_refused_removal_changes_nothing_here(self):
        from unittest import mock
        from winmode import agent
        a, d = self.agent(), Defer()
        a.call = lambda method, args=None, signature=None, done=None, timeout=600: done("It needs an administrator's password.")
        a.refresh_host = lambda then=None: None
        with mock.patch.object(agent.launchers, "remove_all") as remove_all:
            a.m_remove(None, d)
        remove_all.assert_not_called()
        self.assertEqual(d.out, ["It needs an administrator's password."])
        self.assertFalse(a.removing)

    def test_nothing_opens_while_windows_is_being_removed(self):
        from winmode import agent
        a = self.agent()
        a.removing = True
        out = []
        a.launch(r"C:\Windows\notepad.exe", [], "Notepad", "notepad", out.append)
        self.assertEqual(out, [agent.REMOVING])
        self.assertEqual(a.m_open_desktop(None, Defer()), agent.REMOVING)

    def test_a_new_windows_certificate_is_trusted_once_per_setup(self):
        from unittest import mock
        a = self.agent()
        with tempfile.TemporaryDirectory() as tmp, mock.patch.dict(os.environ, {"XDG_DATA_HOME": tmp, "XDG_CONFIG_HOME": tmp}):
            cert = rdp.certificate_path("192.168.207.10", 3389)
            os.makedirs(os.path.dirname(cert))
            a.host["setup"] = {"finished": 1000}
            open(cert, "w").close()
            a.trust_current_windows()
            self.assertFalse(os.path.exists(cert))
            # The certificate the next connection trusts stays trusted.
            open(cert, "w").close()
            a.trust_current_windows()
            self.assertTrue(os.path.exists(cert))
            # Set up again: the old one goes.
            a.host["setup"] = {"finished": 2000}
            a.trust_current_windows()
            self.assertFalse(os.path.exists(cert))

    def test_the_service_deletes_windows_but_keeps_this_pcs_choices(self):
        from unittest import mock
        from winmode import config, setup, system, virsh
        with tempfile.TemporaryDirectory() as tmp:
            disk = os.path.join(tmp, "windows.qcow2")
            other = os.path.join(os.path.dirname(tmp), os.path.basename(tmp) + "-elsewhere.qcow2")
            for f in (disk, other, os.path.join(tmp, "domain.xml")):
                open(f, "w").close()
            for folder in ("cache", "secrets"):
                os.makedirs(os.path.join(tmp, folder))
                open(os.path.join(tmp, folder, "x"), "w").close()
            svc = system.Service.__new__(system.Service)
            svc.domain, svc.cfg = "wildos-windows", {"when_idle": "automatic"}
            svc.busy_action, svc.saving, svc.upkeep_running = None, False, False
            svc.state = {"owner_uid": 1000, "tune": "1", "kept": {"at": 1}, "file_sharing": True, "when_idle": "save"}
            svc.save_state = lambda: None
            cancelled = []
            svc.setup = type("S", (), {"cancel": lambda self: cancelled.append(True)})()
            svc.usb = type("U", (), {"remembered": {"a": 1}, "save": lambda self: None})()
            try:
                with mock.patch.object(config, "STATE_DIR", tmp), mock.patch.object(setup, "CACHE", os.path.join(tmp, "cache")), \
                        mock.patch.object(setup, "SECRETS", os.path.join(tmp, "secrets")), \
                        mock.patch.object(virsh, "disk_files", lambda d: [disk, other]), \
                        mock.patch.object(virsh, "state", lambda d: ""):
                    self.assertEqual(svc.remove_windows(), "")
                # Only files in Windows mode's own folder are ever deleted.
                self.assertTrue(os.path.exists(other))
            finally:
                os.remove(other)
            self.assertEqual(cancelled, [True])
            self.assertFalse(os.path.exists(disk))
            self.assertFalse(os.path.exists(os.path.join(tmp, "domain.xml")))
            self.assertFalse(os.path.exists(os.path.join(tmp, "cache")))
            self.assertFalse(os.path.exists(os.path.join(tmp, "secrets")))
            self.assertEqual(svc.state, {"file_sharing": True, "when_idle": "automatic"})
            self.assertEqual(svc.usb.remembered, {})

    def test_removing_asks_for_an_administrator_every_time(self):
        policy = read(os.path.join(DATA, "org.wildos.winmode.policy"))
        block = policy.split('id="org.wildos.winmode.remove"', 1)[1].split("</action>", 1)[0]
        self.assertIn("<allow_active>auth_admin</allow_active>", block)

    def test_windows_keeps_its_own_account_for_windows_mode(self):
        tune = read(os.path.join(DATA, "..", "guest", "tune.ps1"))
        self.assertIn("'NoConnectedUser' 1", tune)
        self.assertIn("'DevicePasswordLessBuildVersion' 0", tune)


class PcShutdown(unittest.TestCase):
    def svc(self, windows=(), restart=False):
        from types import SimpleNamespace
        from winmode import system
        calls = []
        s = SimpleNamespace(setup=SimpleNamespace(thread=None), busy_action=None, saving=False, domain="w", state={},
                            all_windows=lambda: list(windows), save_ok=lambda: True, restart_wanted=lambda: restart,
                            do_save=lambda: calls.append("save"), save_state=lambda: None,
                            stop_windows=lambda wait: calls.append("shutdown"))
        return system, s, calls

    def run_with(self, state, **kw):
        from unittest import mock
        system, s, calls = self.svc(**kw)
        with mock.patch.object(system.virsh, "state", lambda d: state):
            system.Service.shut_down_windows(s)
        return calls, s

    def test_nothing_open_is_saved_so_the_next_start_is_quick(self):
        for state in ("running", "paused"):
            calls, s = self.run_with(state)
            self.assertEqual(calls, ["save"])
            self.assertNotIn("kept", s.state)

    def test_apps_open_are_kept(self):
        calls, s = self.run_with("running", windows=[{"hwnd": 1}])
        self.assertEqual(calls, ["save"])
        self.assertIn("kept", s.state)

    def test_a_new_memory_limit_still_shuts_down(self):
        self.assertEqual(self.run_with("paused", restart=True)[0], ["shutdown"])

    def test_needs_restart(self):
        from types import SimpleNamespace
        from unittest import mock
        from winmode import system
        s = SimpleNamespace(domain="w")
        for out, want in (("True 0\r\n", "an update"), ("False 8", "8 days"), ("False 2", None), ("", None)):
            with mock.patch.object(system.qga, "powershell", lambda d, script, t, out=out: (0, out, "")):
                got = system.Service.windows_needs_restart(s)
            if want is None:
                self.assertIsNone(got)
            else:
                self.assertIn(want, got)


class GuestUpdate(unittest.TestCase):
    def test_outdated_until_this_packages_copy_is_recorded(self):
        import hashlib
        from types import SimpleNamespace
        from winmode import system
        with tempfile.NamedTemporaryFile(delete=False) as f:
            f.write(b"wild-guest")
        try:
            s = SimpleNamespace(cfg={"guest_exe": f.name}, state={})
            s.guest_exe_digest = lambda: system.Service.guest_exe_digest(s)
            self.assertTrue(system.Service.guest_outdated(s))
            s.state["guest_exe"] = hashlib.sha256(b"wild-guest").hexdigest()[:16]
            self.assertFalse(system.Service.guest_outdated(s))
            s.cfg["guest_exe"] = f.name + ".missing"
            self.assertFalse(system.Service.guest_outdated(s))
        finally:
            os.unlink(f.name)

    def test_updated_when_idle_before_pausing(self):
        from types import SimpleNamespace
        from unittest import mock
        from winmode import system
        calls = []
        life = lifecycle.Lifecycle(60)
        life.update(0, vm_state="running", windows=0)
        s = SimpleNamespace(life=life, busy_action=None, upkeep_running=False, guest_update_tried=-3600.0, domain="w",
                            cfg={"idle_seconds": 60}, state={"when_idle": "automatic"}, idle_timer=1,
                            restart_wanted=lambda: False, guest_outdated=lambda: True,
                            update_guest=lambda: calls.append("update"), run_idle=lambda a: calls.append(a),
                            power=SimpleNamespace(on_battery=False))
        with mock.patch.object(system.time, "monotonic", lambda: 100.0), \
                mock.patch.object(system.virsh, "state", lambda d: "running"):
            system.Service.idle_fired(s)
            self.assertEqual(calls, ["update"])
            # Tried a minute ago and failed: the idle policy goes ahead.
            s.guest_update_tried = 40.0
            with mock.patch.object(system.hostinfo, "meminfo", lambda: (8, 16)), \
                    mock.patch.object(system.hostinfo, "under_pressure", lambda: False):
                system.Service.idle_fired(s)
        self.assertEqual(calls, ["update", "pause"])


class Upkeep(unittest.TestCase):
    def run_upkeep_with(self, state):
        from types import SimpleNamespace
        from unittest import mock
        from winmode import system
        ran = []
        svc = SimpleNamespace(state=state, upkeep_running=False,
                              run_upkeep=lambda name, script, version: ran.append((name, version)))
        with tempfile.TemporaryDirectory() as d:
            os.makedirs(os.path.join(d, "guest"))
            for name, version in (("opengl", "3"), ("tune", "2")):
                with open(os.path.join(d, "guest", name + ".ps1"), "w", encoding="utf-8") as f:
                    f.write(f"param()\n$version = '{version}'\n")
            with mock.patch.object(system.config, "DATA", d):
                system.Service.guest_upkeep(svc)
        return ran

    def test_a_windows_set_up_before_tune_is_left_alone(self):
        self.assertEqual(self.run_upkeep_with({"opengl": "3"}), [])

    def test_a_windows_set_up_with_tune_gets_newer_versions(self):
        self.assertEqual(self.run_upkeep_with({"opengl": "3", "tune": "1"}), [("tune", "2")])
        self.assertEqual(self.run_upkeep_with({"opengl": "3", "tune": "2"}), [])


class Theme(unittest.TestCase):
    def test_dark_and_accent(self):
        d = tempfile.mkdtemp()
        p = os.path.join(d, "kdeglobals")
        with open(p, "w") as f:
            f.write("[General]\nAccentColor=61,174,233\n\n[Colors:Window]\nBackgroundNormal=32,35,38\n")
        self.assertEqual(theme.read(p), {"dark": True, "accent": "#3daee9"})
        with open(p, "w") as f:
            f.write("[Colors:Window]\nBackgroundNormal=239,240,241\n[Colors:Selection]\nBackgroundNormal=1,2,3\n")
        self.assertEqual(theme.read(p), {"dark": False, "accent": "#010203"})
        self.assertEqual(theme.read(os.path.join(d, "missing")), {"dark": False, "accent": ""})


class Firewall(unittest.TestCase):
    def test_file_sharing_opening_is_only_for_the_guest(self):
        plain = firewall.rules("virbr-wildos", "192.168.207.1", "192.168.207.10")
        self.assertNotIn("445", plain)
        smb = firewall.rules("virbr-wildos", "192.168.207.1", "192.168.207.10", True)
        self.assertIn('iifname "virbr-wildos" ip saddr 192.168.207.10 ip daddr 192.168.207.1 tcp dport 445 accept', smb)
        self.assertLess(smb.index("dport 445"), smb.index('iifname "virbr-wildos" counter drop'))


def pe_with_version(product, company):
    """A minimal PE file with just a version resource."""
    import struct
    strings = "".join(f"{k}\0{v}\0" for k, v in (("CompanyName", company), ("ProductName", product))).encode("utf-16-le")
    block = b"\0" * 8 + "VS_VERSION_INFO\0".encode("utf-16-le") + strings
    res = bytearray(0x200)
    def directory(off, entries):
        struct.pack_into("<IIHHHH", res, off, 0, 0, 0, 0, 0, len(entries))
        for i, (name, target) in enumerate(entries):
            struct.pack_into("<II", res, off + 16 + i * 8, name, target)
    directory(0, [(16, 0x80000000 | 0x20)])
    directory(0x20, [(1, 0x80000000 | 0x40)])
    directory(0x40, [(0x409, 0x60)])
    struct.pack_into("<IIII", res, 0x60, 0x1000 + 0x80, len(block), 0, 0)
    res[0x80:0x80 + len(block)] = block
    hdr = bytearray(0x200)
    hdr[0:2] = b"MZ"
    struct.pack_into("<I", hdr, 0x3C, 0x40)
    hdr[0x40:0x44] = b"PE\0\0"
    struct.pack_into("<HH", hdr, 0x44, 0x8664, 1)
    struct.pack_into("<H", hdr, 0x54, 240)
    opt = 0x58
    struct.pack_into("<H", hdr, opt, 0x20B)
    struct.pack_into("<II", hdr, opt + 112 + 16, 0x1000, len(res))
    sec = opt + 240
    hdr[sec:sec + 5] = b".rsrc"
    struct.pack_into("<IIII", hdr, sec + 8, len(res), 0x1000, len(res), 0x200)
    return bytes(hdr) + bytes(res)


class Router(unittest.TestCase):
    def setUp(self):
        self.entries = compat.load_db(os.path.join(DATA, "compat"))
        self.dir = tempfile.mkdtemp()

    def program(self, name, product, company):
        p = os.path.join(self.dir, name)
        with open(p, "wb") as f:
            f.write(pe_with_version(product, company))
        return p

    def test_version_information(self):
        info = compat.identify(self.program("x.exe", "Invoice Maker", "Acme Ltd"), check_signature=False)
        self.assertEqual(info["kind"], "exe")
        self.assertEqual(info["product"], "Invoice Maker")
        self.assertEqual(info["company"], "Acme Ltd")

    def test_calling_apps_and_steam_are_native(self):
        for name in ("Microsoft Teams", "Zoom", "Slack", "Discord", "WhatsApp", "Steam"):
            e = [x for x in self.entries if x["name"] == name]
            self.assertEqual(len(e), 1, name)
            self.assertEqual(e[0]["engine"], "native", name)
        for name in ("Microsoft Teams", "Zoom", "Slack", "Discord", "WhatsApp"):
            self.assertTrue([x for x in self.entries if x["name"] == name][0].get("calls"), name)

    def test_anti_cheat_is_unsupported_with_a_reason(self):
        info = compat.identify(self.program("RiotClientInstaller.exe", "Riot Client", "Riot Games, Inc."), check_signature=False)
        e = compat.lookup(info, self.entries)
        self.assertEqual(e["engine"], "unsupported")
        d = compat.decide(info, e, {"memory": 16 * 1024 ** 3, "installed": True, "state": "ready"})
        self.assertEqual(d["action"], "unsupported")
        self.assertTrue(d["reason"])

    def test_unknown_programs_run_in_windows_mode(self):
        info = compat.identify(self.program("InvoiceMaker-Setup.exe", "Invoice Maker", "Acme Ltd"), check_signature=False)
        self.assertIsNone(compat.lookup(info, self.entries))
        host = {"memory": 16 * 1024 ** 3, "installed": True, "state": "ready"}
        self.assertEqual(compat.decide(info, None, host)["action"], "windows-mode")
        self.assertEqual(compat.decide(info, None, dict(host, edition="home"))["action"], "home")
        self.assertEqual(compat.decide(info, None, dict(host, memory=4 * 1024 ** 3))["action"], "cannot")
        self.assertEqual(compat.decide(info, None, dict(host, state="none"))["action"], "setup")

    def test_not_a_program(self):
        p = os.path.join(self.dir, "notes.exe")
        with open(p, "wb") as f:
            f.write(b"hello")
        info = compat.identify(p, check_signature=False)
        self.assertEqual(compat.decide(info, None, {})["action"], "not-windows")

    def test_most_specific_match_wins(self):
        entries = [{"id": "a", "engine": "native", "match": {"words": ["acme"]}},
                   {"id": "b", "engine": "unsupported", "match": {"product": ["Invoice Maker"]}},
                   {"id": "c", "engine": "windows-mode", "match": {"sha256": ["ABC"]}}]
        info = {"file": "x.exe", "product": "Invoice Maker", "company": "Acme", "sha256": "abc"}
        self.assertEqual(compat.lookup(info, entries)["id"], "c")
        self.assertEqual(compat.lookup(dict(info, sha256="zzz"), entries)["id"], "b")
        self.assertEqual(compat.lookup(dict(info, sha256="zzz", product="Other"), entries)["id"], "a")


if __name__ == "__main__":
    unittest.main()


class Setup(unittest.TestCase):
    @staticmethod
    def msdm(key):
        body = bytearray(b"MSDM" + b"\0" * 32)
        body += (1).to_bytes(4, "little") + bytes(4) + (1).to_bytes(4, "little") + bytes(4) + (29).to_bytes(4, "little")
        body += key.encode()
        body[4:8] = len(body).to_bytes(4, "little")
        body[9] = (256 - sum(body) % 256) % 256
        return bytes(body)

    def test_firmware_key(self):
        from winmode import setup
        key = "VK7JG-NPHTM-C97JM-9MPGT-3V66T"
        self.assertEqual(setup.parse_msdm(self.msdm(key)), key)
        broken = bytearray(self.msdm(key))
        broken[60] ^= 1
        self.assertIsNone(setup.parse_msdm(bytes(broken)))
        self.assertIsNone(setup.parse_msdm(b"SLIC" + bytes(81)))

    def test_key_shape(self):
        from winmode import setup
        self.assertTrue(setup.key_shape_ok("VK7JG-NPHTM-C97JM-9MPGT-3V66T"))
        self.assertFalse(setup.key_shape_ok("VK7JG-NPHTM-C97JM-9MPGT-3V66"))
        self.assertFalse(setup.key_shape_ok("AAAAA-BBBBB-CCCCC-DDDDD-EEEEE"))
        self.assertFalse(setup.key_shape_ok("NNBCD-FGHJK-MPQRT-VWXY2-34678"))

    def test_office_plan(self):
        from winmode import setup
        self.assertIn('Product ID="O365BusinessRetail"', setup.office_config("business", "en-GB"))
        self.assertIn('Product ID="O365HomePremRetail"', setup.office_config("anything", "en-GB"))
        self.assertNotIn('ExcludeApp ID="Excel"', setup.office_config("personal", "en-GB"))

    def test_office_by_product_key(self):
        from winmode import setup
        self.assertIn('Product ID="HomeBusiness2024Retail"', setup.office_config("key", "en-GB"))

    def started(self, request):
        """The record a first setup leaves from this request, with nothing
        installed: no virtual PC, no download, no worker."""
        from winmode import setup

        class Svc:
            cfg = {"domain": "wildos-windows"}

            def __init__(self):
                self.state = {}

            def save_state(self):
                pass

            def notify(self):
                pass

        tmp = tempfile.mkdtemp()
        saved = (setup.virsh.state, setup.SECRETS, setup.MARKER, setup.forget_windows_iso)
        setup.virsh.state = lambda domain: ""
        setup.SECRETS, setup.MARKER = os.path.join(tmp, "secrets"), os.path.join(tmp, "setting-up")
        setup.forget_windows_iso = lambda own=False: None
        try:
            s = setup.Setup(Svc())
            s.launch = lambda: None
            return s.start(request, 1000), s
        finally:
            setup.virsh.state, setup.SECRETS, setup.MARKER, setup.forget_windows_iso = saved

    def test_licence_terms_must_be_accepted(self):
        base = {"licence": {"source": "firmware"}, "options": {"office": True}}
        why, s = self.started(dict(base))
        self.assertEqual(why, "Accept Microsoft's licence terms first.")
        self.assertEqual(s.rec, {})
        # Office chosen: its terms too.
        why, _ = self.started(dict(base, terms={"windows": True}))
        self.assertEqual(why, "Accept Microsoft's licence terms first.")
        why, _ = self.started(dict(base, options={"office": False}, terms={"windows": True}))
        self.assertEqual(why, "")

    def test_accepted_terms_are_kept_with_the_setup(self):
        why, s = self.started({"licence": {"source": "firmware"}, "options": {"office": True, "officePlan": "key"},
                               "terms": {"windows": True, "office": True}, "background": True})
        self.assertEqual(why, "")
        self.assertEqual({k: v for k, v in s.rec["terms"].items() if k != "at"}, {"windows": True, "office": True})
        self.assertGreater(s.rec["terms"]["at"], 0)
        self.assertEqual(s.rec["options"]["officePlan"], "key")
        st = s.status()
        self.assertTrue(st["background"])
        self.assertEqual(st["terms"]["windows"], True)

    @unittest.skipUnless(hasattr(os, "setpriority") and os.path.exists("/proc/self/task"), "Linux only")
    def test_setup_runs_on_spare_capacity(self):
        import ctypes
        import threading
        from winmode import setup
        seen = {}

        def work():
            setup.spare_capacity()
            tid = threading.get_native_id()
            seen["nice"] = os.getpriority(os.PRIO_PROCESS, tid)
            seen["io"] = ctypes.CDLL(None, use_errno=True).syscall(252, 1, tid) >> 13
        t = threading.Thread(target=work)
        t.start()
        t.join()
        self.assertEqual(seen["nice"], 19)
        self.assertEqual(seen["io"], 3)
        # Only that thread: the service's own stays as it was.
        self.assertNotEqual(os.getpriority(os.PRIO_PROCESS, threading.get_native_id()), 19)

    def test_office_follows_host_language(self):
        from winmode import setup
        uk = setup.office_config("personal", "en-GB")
        self.assertIn('<Language ID="en-gb"/>', uk)
        self.assertIn('<Product ID="ProofingTools">', uk)
        self.assertIn('<Language ID="MatchOS"/>', uk)
        # A language the Office Deployment Tool doesn't know would stop the
        # install, so it falls back to Windows' own.
        odd = setup.office_config("personal", "cy-GB")
        self.assertNotIn("cy-gb", odd)
        self.assertNotIn("ProofingTools", odd)
        self.assertIn('<Language ID="MatchOS"/>', odd)

    def test_templates_fill_every_field(self):
        from winmode import setup
        here = os.path.join(os.path.dirname(__file__), "..", "data", "templates")
        orig = setup.TEMPLATES
        setup.TEMPLATES = here
        try:
            with self.assertRaises(RuntimeError):
                setup.render("network.xml.in", {"NAME": "n"})
            text = setup.render("network.xml.in", {"NAME": "n", "BRIDGE": "b", "GW": "1.2.3.1", "GUEST_IP": "1.2.3.4",
                                                   "GUEST_MAC": "52:54:00:00:00:01"})
            self.assertIn("<name>n</name>", text)
        finally:
            setup.TEMPLATES = orig

    def test_progress(self):
        from winmode import setup

        class Svc:
            cfg = {}
            state = {"setup": {"running": True, "stage": "install", "options": {"office": False},
                               "licence": {"source": "none"},
                               "stages": {"media": {"state": "done"}, "install": {"state": "active", "percent": 50}}}}

        s = setup.Setup(Svc())
        st = s.status()
        self.assertEqual([x["id"] for x in st["stages"]], ["media", "install", "activate", "tune"])
        # media 25 of 85 done, install half of 45: 47.5 of 85
        self.assertEqual(st["percent"], 56)
        self.assertEqual(st["stages"][2]["title"], "Activation")
        self.assertTrue(st["left"].startswith("About "))
