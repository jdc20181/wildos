import json
import os
import struct
import tempfile
import unittest

from winmode import isocheck

SECTOR = isocheck.SECTOR
START = 300  # where the fake ISO's partition starts, in sectors


def edition(index, name, edition_id, build=26100, arch=9, product="WinNT", kind="Client"):
    return (f'<IMAGE INDEX="{index}"><WINDOWS><ARCH>{arch}</ARCH><PRODUCTTYPE>{product}</PRODUCTTYPE>'
            f"<INSTALLATIONTYPE>{kind}</INSTALLATIONTYPE><EDITIONID>{edition_id}</EDITIONID>"
            f"<LANGUAGES><LANGUAGE>en-GB</LANGUAGE></LANGUAGES>"
            f"<VERSION><MAJOR>10</MAJOR><MINOR>0</MINOR><BUILD>{build}</BUILD></VERSION></WINDOWS>"
            f"<NAME>{name}</NAME></IMAGE>")


CONSUMER = "".join([edition(1, "Windows 11 Home", "Core"), edition(2, "Windows 11 Home N", "CoreN"),
                    edition(6, "Windows 11 Pro", "Professional"), edition(7, "Windows 11 Pro N", "ProfessionalN")])


def wim(images_xml, tail=4096):
    """A WIM's header and XML, as Windows Setup reads them, and some bytes
    standing in for the editions themselves."""
    xml = ("﻿<WIM><TOTALBYTES>1</TOTALBYTES>" + images_xml + "</WIM>").encode("utf-16-le")
    header = bytearray(208)
    header[:8] = b"MSWIM\0\0\0"
    struct.pack_into("<IIII", header, 8, 208, 0x10d00, 0, 0)
    struct.pack_into("<QQQ", header, 72, len(xml) | (1 << 57), 208 + tail, len(xml))
    return bytes(header) + b"\x55" * tail + xml


def tagged(ident, size=SECTOR):
    b = bytearray(size)
    struct.pack_into("<H", b, 0, ident)
    return b


def fid(name, lbn, directory=False, parent=False):
    raw = b"" if parent else b"\x08" + name.encode()
    b = tagged(257, (38 + len(raw) + 3) & ~3)
    b[18] = (2 if directory else 0) | (8 if parent else 0)
    b[19] = len(raw)
    struct.pack_into("<II", b, 20, SECTOR, lbn)
    b[38:38 + len(raw)] = raw
    return bytes(b)


def file_entry(length, extents):
    """extents: (block, bytes), as short_ads."""
    fe = tagged(261)
    struct.pack_into("<Q", fe, 56, length)
    struct.pack_into("<I", fe, 172, 8 * len(extents))
    for i, (block, size) in enumerate(extents):
        struct.pack_into("<II", fe, 176 + 8 * i, size, block)
    return fe


def udf_image(path, wim_name="install.wim", wim_bytes=b"", cut=None):
    """A minimal UDF disc the way Windows ISOs are laid out: anchor at 256,
    one partition, the file set, then sources/<wim_name> in two extents."""
    blocks = {}
    root_dir = fid("", 1, parent=True) + fid("sources", 3, directory=True) + fid("setup.exe", 9)
    sources_dir = fid("", 1, parent=True) + fid("boot.wim", 9) + fid(wim_name, 5)
    fsd = tagged(256)
    struct.pack_into("<II", fsd, 400, SECTOR, 1)
    blocks[0] = fsd
    blocks[1] = file_entry(len(root_dir), [(2, len(root_dir))])
    blocks[2] = root_dir
    blocks[3] = file_entry(len(sources_dir), [(4, len(sources_dir))])
    blocks[4] = sources_dir
    # The header's block on its own and the rest further on, so reading the
    # XML has to follow the file from one extent to the next.
    first, rest = wim_bytes[:SECTOR], wim_bytes[SECTOR:]
    blocks[5] = file_entry(len(wim_bytes), [(6, len(first)), (10, len(rest))])
    blocks[6] = first
    blocks[10] = rest
    with open(path, "wb") as f:
        def put(sector, data):
            f.seek(sector * SECTOR)
            f.write(data)
        anchor = tagged(2)
        struct.pack_into("<II", anchor, 16, 3 * SECTOR, 32)
        put(256, anchor)
        pd = tagged(5)
        struct.pack_into("<I", pd, 188, START)
        put(32, pd)
        lvd = tagged(6)
        struct.pack_into("<I", lvd, 212, SECTOR)
        struct.pack_into("<II", lvd, 248, SECTOR, 0)
        struct.pack_into("<I", lvd, 268, 1)
        lvd[440] = 1
        put(33, lvd)
        put(34, tagged(8))
        for block, data in blocks.items():
            put(START + block, data)
        end = f.tell()
        f.truncate(cut if cut is not None else end)


class IsoCheck(unittest.TestCase):
    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = os.path.join(self.dir.name, "Win11_English_x64.iso")
        # The fake discs are a few kilobytes; the size floor has its own test.
        self.floor = isocheck.MIN_BYTES
        isocheck.MIN_BYTES = 0

    def tearDown(self):
        isocheck.MIN_BYTES = self.floor
        self.dir.cleanup()

    def verdict(self, **kwargs):
        udf_image(self.path, **kwargs)
        try:
            return isocheck.check(self.path)
        except isocheck.Unusable as e:
            return e.text

    def test_a_consumer_iso_gives_its_pro_edition(self):
        result = self.verdict(wim_bytes=wim(CONSUMER))
        self.assertEqual(result["index"], 6)
        self.assertEqual(result["name"], "Windows 11 Pro")
        self.assertEqual(result["build"], 26100)
        self.assertEqual(result["file"], "Win11_English_x64.iso")
        self.assertEqual(result["languages"], ["en-GB"])

    def test_the_media_creation_tools_esd_works_too(self):
        self.assertTrue(self.verdict(wim_name="install.esd", wim_bytes=wim(CONSUMER))["ok"])

    def test_names_on_the_disc_are_not_case_sensitive(self):
        self.assertTrue(self.verdict(wim_name="INSTALL.WIM", wim_bytes=wim(CONSUMER))["ok"])

    def test_windows_10(self):
        older = edition(1, "Windows 10 Home", "Core", 19045) + edition(2, "Windows 10 Pro", "Professional", 19045)
        self.assertEqual(self.verdict(wim_bytes=wim(older)), isocheck.WINDOWS_10)
        self.assertEqual(isocheck.WINDOWS_10, "This file is Windows 10. Windows mode needs Windows 11.")

    def test_no_pro(self):
        enterprise = edition(1, "Windows 11 Enterprise", "Enterprise") + edition(2, "Windows 11 Pro N", "ProfessionalN")
        self.assertEqual(self.verdict(wim_bytes=wim(enterprise)), isocheck.NO_PRO)
        self.assertEqual(isocheck.NO_PRO, "This file doesn't include Windows 11 Pro.")

    def test_arm(self):
        self.assertEqual(self.verdict(wim_bytes=wim(edition(1, "Windows 11 Pro", "Professional", arch=12))), isocheck.ARM)

    def test_server(self):
        server = edition(1, "Windows Server 2025 Standard", "ServerStandard", product="ServerNT", kind="Server")
        self.assertEqual(self.verdict(wim_bytes=wim(server)), isocheck.SERVER)

    def test_older_windows(self):
        seven = edition(1, "Windows 7 Professional", "Professional", 7601).replace("<MAJOR>10</MAJOR>", "<MAJOR>6</MAJOR>")
        self.assertEqual(self.verdict(wim_bytes=wim(seven)), isocheck.OLDER)

    def test_a_disc_without_windows_on_it(self):
        self.assertEqual(self.verdict(wim_name="readme.txt", wim_bytes=wim(CONSUMER)), isocheck.NOT_WINDOWS)
        with open(self.path, "wb") as f:
            f.write(b"not a disc " * 60000)
        with self.assertRaises(isocheck.Unusable) as e:
            isocheck.check(self.path)
        self.assertEqual(e.exception.text, isocheck.NOT_WINDOWS)

    def test_a_download_that_stopped_part_way(self):
        data = wim(CONSUMER, tail=20000)
        # Ends inside the editions, before the XML at the end of the WIM.
        self.assertEqual(self.verdict(wim_bytes=data, cut=(START + 10) * SECTOR + 4000), isocheck.INCOMPLETE)

    def test_damaged_metadata(self):
        broken = wim(CONSUMER).replace("</WIM>".encode("utf-16-le"), "</WIX>".encode("utf-16-le"))
        self.assertEqual(self.verdict(wim_bytes=broken), isocheck.UNREADABLE)
        self.assertEqual(self.verdict(wim_bytes=b"NOTAWIM!" + wim(CONSUMER)[8:]), isocheck.UNREADABLE)

    def test_too_small_to_be_real(self):
        isocheck.MIN_BYTES = self.floor
        self.assertEqual(self.verdict(wim_bytes=wim(CONSUMER)), isocheck.TOO_SMALL)

    def test_missing_file(self):
        with self.assertRaises(isocheck.Unusable) as e:
            isocheck.check(os.path.join(self.dir.name, "gone.iso"))
        self.assertEqual(e.exception.text, isocheck.GONE)

    def test_every_sentence_is_one_plain_sentence(self):
        for name in ("GONE", "CANT_OPEN", "NOT_WINDOWS", "INCOMPLETE", "UNREADABLE", "TOO_SMALL", "OLDER",
                     "WINDOWS_10", "SERVER", "ARM", "NOT_X64", "NO_PRO"):
            text = getattr(isocheck, name)
            self.assertTrue(text.endswith("."), name)
            self.assertNotIn(chr(0x2014), text)
            self.assertNotIn(chr(0x2013), text)
            for word in ("ISO", "WIM", "UDF", "x64", "edition"):
                self.assertNotIn(word, text, name)


class ProImage(unittest.TestCase):
    def test_picks_plain_pro_by_edition_not_position(self):
        found = isocheck.images("<WIM>" + edition(3, "Windows 11 Pro N", "ProfessionalN")
                                + edition(9, "Windows 11 Pro", "Professional") + "</WIM>")
        self.assertEqual(isocheck.pro_image(found)["index"], 9)

    def test_edition_from_flags_when_there_is_no_edition_id(self):
        xml = edition(1, "Windows 11 Pro", "Professional").replace("<EDITIONID>Professional</EDITIONID>", "")
        xml = xml.replace("</NAME>", "</NAME><FLAGS>Professional</FLAGS>")
        self.assertEqual(isocheck.pro_image(isocheck.images("<WIM>" + xml + "</WIM>"))["index"], 1)

    def test_nothing_in_it(self):
        with self.assertRaises(isocheck.Unusable) as e:
            isocheck.pro_image(isocheck.images("<WIM></WIM>"))
        self.assertEqual(e.exception.text, isocheck.UNREADABLE)

    def test_main_prints_json(self):
        import io
        import json
        from contextlib import redirect_stdout
        out = io.StringIO()
        with redirect_stdout(out):
            self.assertEqual(isocheck.main(["/nowhere/at/all.iso"]), 0)
        self.assertEqual(json.loads(out.getvalue()), {"ok": False, "text": isocheck.GONE})


class OwnIsoInSetup(unittest.TestCase):
    """The service's side: the check before setup starts, the copy, and
    what is cleared up afterwards."""

    def setUp(self):
        from winmode import setup
        self.setup = setup
        self.dir = tempfile.TemporaryDirectory()
        self.cache = os.path.join(self.dir.name, "cache")
        os.makedirs(self.cache)
        self.orig = setup.CACHE
        setup.CACHE = self.cache
        self.floor = isocheck.MIN_BYTES

    def tearDown(self):
        self.setup.CACHE = self.orig
        isocheck.MIN_BYTES = self.floor
        self.dir.cleanup()

    def service(self, rec):
        class Svc:
            cfg = {"domain": "test"}
            state = {"setup": rec}

            def save_state(self):
                pass

            def notify(self):
                pass
        return self.setup.Setup(Svc())

    def test_the_check_before_setup_explains_a_refusal(self):
        path = os.path.join(self.dir.name, "Win10.iso")
        udf_image(path, wim_bytes=wim(edition(1, "Windows 10 Pro", "Professional", 19045)))
        own, why = self.setup.check_own_iso(path, None)
        self.assertIsNone(own)
        self.assertEqual(why, isocheck.WINDOWS_10)
        self.assertEqual(self.setup.check_own_iso("relative.iso", None)[1], "That file couldn't be found.")

    def test_the_check_before_setup_accepts_windows_11_pro(self):
        # Sparse, so it takes no room: the check reads only its first
        # kilobytes and its size.
        path = os.path.join(self.dir.name, "Win11.iso")
        udf_image(path, wim_bytes=wim(CONSUMER), cut=self.floor + SECTOR)
        own, why = self.setup.check_own_iso(path, None)
        self.assertEqual(why, "")
        self.assertEqual(own["path"], path)
        self.assertEqual(own["bytes"], self.floor + SECTOR)
        self.assertEqual(own["name"], "Windows 11 Pro")

    def test_the_copy_is_checked_and_used_like_a_download(self):
        isocheck.MIN_BYTES = 0
        path = os.path.join(self.dir.name, "Win11.iso")
        udf_image(path, wim_bytes=wim(CONSUMER))
        size = os.path.getsize(path)
        s = self.service({"running": True, "iso": {"path": path, "bytes": size, "file": "Win11.iso"}})
        iso = os.path.join(self.cache, "windows.iso")
        meta = os.path.join(self.cache, "windows-iso.json")
        s.windows_iso()
        with open(path, "rb") as a, open(iso, "rb") as b:
            self.assertEqual(a.read(), b.read())
        with open(meta) as f:
            self.assertEqual(json.load(f)["source"], "own")
        self.assertIn("windows-iso", s.rec["done"])
        self.assertFalse(os.path.exists(iso + ".part"))

    def test_a_file_that_changed_after_the_check_is_refused(self):
        isocheck.MIN_BYTES = 0
        path = os.path.join(self.dir.name, "Win11.iso")
        udf_image(path, wim_bytes=wim(edition(1, "Windows 11 Enterprise", "Enterprise")))
        s = self.service({"running": True, "iso": {"path": path, "bytes": os.path.getsize(path), "file": "Win11.iso"}})
        with self.assertRaises(self.setup.Problem) as e:
            s.windows_iso()
        self.assertEqual(e.exception.text, isocheck.NO_PRO)
        self.assertEqual(os.listdir(self.cache), [])

    def test_a_file_that_went_missing(self):
        s = self.service({"running": True, "iso": {"path": os.path.join(self.dir.name, "gone.iso"), "bytes": 5,
                                                   "file": "gone.iso"}})
        with self.assertRaises(self.setup.Problem) as e:
            s.windows_iso()
        self.assertIn("couldn't be read", e.exception.text)
        self.assertEqual(os.listdir(self.cache), [])

    def test_only_an_own_copy_is_cleared_for_a_download(self):
        iso = os.path.join(self.cache, "windows.iso")
        meta = os.path.join(self.cache, "windows-iso.json")
        for source in ("own", "download"):
            open(iso, "w").close()
            with open(meta, "w") as f:
                json.dump({"source": source} if source == "own" else {"sha256": "ab"}, f)
            self.setup.forget_windows_iso(own=False)
            self.assertEqual(os.path.exists(iso), source == "download")
        self.setup.forget_windows_iso(own=True)
        self.assertEqual(os.listdir(self.cache), [])

    def test_progress_says_where_windows_comes_from(self):
        s = self.service({"running": True, "stage": "media", "options": {}, "licence": {"source": "none"},
                          "iso": {"path": "/x.iso"}, "stages": {"media": {"state": "done"}}})
        st = s.status()
        self.assertTrue(st["ownIso"])
        self.assertEqual(st["stages"][0]["title"], "Copy Windows 11 from your file")
        self.assertEqual(st["stages"][0]["detail"], "Checked that it's Windows 11 Pro")
        self.assertNotIn("Microsoft's signature", st["stages"][0]["detail"])
        s.svc.state["setup"].pop("iso")
        self.assertEqual(s.status()["stages"][0]["title"], "Download Windows 11 from Microsoft")


if __name__ == "__main__":
    unittest.main()
