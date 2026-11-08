import base64
import io
import json
import os
import tempfile
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
        self.assertEqual(json.loads(base64.b64decode(encoded)), {"path": r"C:\O\WINWORD.EXE", "args": [r"Z:\Documents\a, b.docx"]})
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
        self.assertIn('Product ID="O365BusinessRetail"', setup.office_config("business"))
        self.assertIn('Product ID="O365HomePremRetail"', setup.office_config("anything"))
        self.assertNotIn('ExcludeApp ID="Excel"', setup.office_config("personal"))

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
