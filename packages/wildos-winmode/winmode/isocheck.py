"""Is this file Windows 11 Pro? For a Windows 11 ISO someone already has,
instead of the download from Microsoft.

A downloaded ISO is checked against the SHA-256 Microsoft publishes. A file
someone brings can't be: there is no list of every ISO Microsoft has made,
and a copy someone kept of an older build is still Microsoft's. So this
checks what the file is, from the list of editions Windows Setup itself
reads, and never says it is genuine.

Windows ISOs hold UDF, with sources/install.wim (or install.esd, from the
Media Creation Tool) in it. The WIM's header points at its XML description
of every edition, which is never compressed. Everything here reads a few
kilobytes of the file with nothing but the standard library, so the wizard
can run it as the person, on a file only they can read, and the service can
run it again on its own copy.

  python3 -I isocheck.py FILE    prints {"ok": true, ...} or {"ok": false, "text": ...}
"""

import json
import os
import struct
import sys
import xml.etree.ElementTree as ET

SECTOR = 2048
# Every Windows 11 x64 ISO Microsoft has made is over 4 GB; the Media
# Creation Tool's single-language ones are the smallest, at about 4.5.
MIN_BYTES = 3 * 1000 ** 3
# The XML of a twelve-edition image is about 30 KB.
MAX_XML = 16 * 1024 * 1024
WIN11_BUILD = 22000
AMD64, ARM64 = 9, 12

GONE = "This file isn't there any more."
CANT_OPEN = "Windows mode can't open this file."
NOT_WINDOWS = "This file isn't a copy of Windows."
INCOMPLETE = "This file is incomplete, so it may not have finished downloading."
UNREADABLE = "This file couldn't be read as Windows 11."
TOO_SMALL = "This file is too small to be a full copy of Windows 11."
OLDER = "This file is an older version of Windows. Windows mode needs Windows 11."
WINDOWS_10 = "This file is Windows 10. Windows mode needs Windows 11."
SERVER = "This file is Windows Server. Windows mode needs Windows 11."
ARM = "This file is Windows 11 for ARM PCs. Windows mode needs the version for Intel and AMD PCs."
NOT_X64 = "This file isn't the 64-bit version of Windows 11, which Windows mode needs."
NO_PRO = "This file doesn't include Windows 11 Pro."


class Unusable(Exception):
    """Why the file can't be used, in one sentence for the person."""

    def __init__(self, text):
        super().__init__(text)
        self.text = text


def _u16(b, o):
    return struct.unpack_from("<H", b, o)[0]


def _u32(b, o):
    return struct.unpack_from("<I", b, o)[0]


def _u64(b, o):
    return struct.unpack_from("<Q", b, o)[0]


class Udf:
    """Just enough of UDF (ECMA-167, as Windows ISOs write it: one physical
    partition, 2048-byte blocks) to find a file and where its bytes are."""

    def __init__(self, f, size):
        self.f, self.size = f, size
        anchor = self.sector(256)
        if _u16(anchor, 0) != 2:
            raise Unusable(NOT_WINDOWS)
        length, where = _u32(anchor, 16), _u32(anchor, 20)
        self.start = None
        fileset = None
        for i in range(min(length // SECTOR, 64)):
            d = self.sector(where + i)
            tag = _u16(d, 0)
            if tag == 5:
                self.start = _u32(d, 188)
            elif tag == 6:
                if _u32(d, 212) != SECTOR or _u32(d, 268) < 1 or d[440] != 1:
                    raise Unusable(UNREADABLE)
                fileset = _u32(d, 252)
            elif tag == 8:
                break
        if self.start is None or fileset is None:
            raise Unusable(UNREADABLE)
        fsd = self.block(fileset)
        if _u16(fsd, 0) != 256:
            raise Unusable(UNREADABLE)
        self.root = _u32(fsd, 404)

    def sector(self, n):
        if (n + 1) * SECTOR > self.size:
            raise Unusable(INCOMPLETE)
        self.f.seek(n * SECTOR)
        data = self.f.read(SECTOR)
        if len(data) != SECTOR:
            raise Unusable(INCOMPLETE)
        return data

    def block(self, lbn):
        return self.sector(self.start + lbn)

    def entry(self, lbn):
        """A file's length and its extents, as (offset in the ISO, length);
        or its bytes, for a small file kept in the entry itself."""
        fe = self.block(lbn)
        tag = _u16(fe, 0)
        if tag == 261:
            length, l_ea, l_ad, base = _u64(fe, 56), _u32(fe, 168), _u32(fe, 172), 176
        elif tag == 266:
            length, l_ea, l_ad, base = _u64(fe, 56), _u32(fe, 208), _u32(fe, 212), 216
        else:
            raise Unusable(UNREADABLE)
        kind = _u16(fe, 34) & 7
        ads = fe[base + l_ea:base + l_ea + l_ad]
        if kind == 3:
            return length, None, ads[:length]
        if kind not in (0, 1):
            raise Unusable(UNREADABLE)
        step = 8 if kind == 0 else 16
        extents, hops = [], 0
        while ads:
            n, pos = _u32(ads, 0), _u32(ads, 4)
            ads = ads[step:]
            size, flag = n & 0x3FFFFFFF, n >> 30
            if size == 0:
                break
            if flag == 3:
                # The list carries on in an Allocation Extent Descriptor.
                hops += 1
                aed = self.block(pos)
                if hops > 64 or _u16(aed, 0) != 258:
                    raise Unusable(UNREADABLE)
                ads = aed[24:24 + _u32(aed, 20)]
                continue
            if flag != 0:
                raise Unusable(UNREADABLE)
            extents.append(((self.start + pos) * SECTOR, size))
        if sum(n for _, n in extents) < length:
            raise Unusable(UNREADABLE)
        return length, extents, None

    def read(self, extents, offset, count):
        """count bytes from offset in a file made of extents."""
        out = bytearray()
        for where, size in extents:
            if offset >= size:
                offset -= size
                continue
            take = min(size - offset, count - len(out))
            if where + offset + take > self.size:
                raise Unusable(INCOMPLETE)
            self.f.seek(where + offset)
            out += self.f.read(take)
            offset = 0
            if len(out) >= count:
                break
        if len(out) != count:
            raise Unusable(INCOMPLETE)
        return bytes(out)

    def listing(self, lbn):
        length, extents, inline = self.entry(lbn)
        data = inline if extents is None else self.read(extents, 0, min(length, 4 * 1024 * 1024))
        names, o = {}, 0
        while o + 38 <= len(data):
            if _u16(data, o) != 257:
                raise Unusable(UNREADABLE)
            chars, l_fi, l_iu = data[o + 18], data[o + 19], _u16(data, o + 36)
            child = _u32(data, o + 24)
            raw = data[o + 38 + l_iu:o + 38 + l_iu + l_fi]
            if not chars & 8 and raw:
                name = raw[1:].decode("utf-16-be" if raw[0] == 16 else "latin-1", "replace")
                names[name.lower()] = (child, bool(chars & 2))
            o += (38 + l_iu + l_fi + 3) & ~3
        return names

    def find(self, *parts):
        lbn = self.root
        for i, part in enumerate(parts):
            hit = self.listing(lbn).get(part.lower())
            if not hit or hit[1] != (i < len(parts) - 1):
                return None
            lbn = hit[0]
        return self.entry(lbn)

    def check_extents(self, extents):
        if any(where + size > self.size for where, size in extents or []):
            raise Unusable(INCOMPLETE)


def wim_xml(udf, extents):
    """The XML a WIM or ESD keeps about its editions, from the resource its
    208-byte header points at."""
    header = udf.read(extents, 0, 208)
    if header[:8] != b"MSWIM\0\0\0":
        raise Unusable(UNREADABLE)
    size = _u64(header, 72) & ((1 << 56) - 1)
    offset = _u64(header, 80)
    if not 2 <= size <= MAX_XML:
        raise Unusable(UNREADABLE)
    raw = udf.read(extents, offset, size)
    try:
        return raw.decode("utf-16-le").lstrip("﻿")
    except UnicodeDecodeError:
        raise Unusable(UNREADABLE) from None


def images(xml_text):
    """Each edition in a WIM's XML: index, name, edition, build, and so on."""
    try:
        root = ET.fromstring(xml_text)
    except ET.ParseError:
        raise Unusable(UNREADABLE) from None
    out = []
    for image in root.findall("IMAGE"):
        def text(path, default=""):
            node = image.find(path)
            return (node.text or "").strip() if node is not None and node.text else default

        def number(path):
            try:
                return int(text(path, "0"))
            except ValueError:
                return 0
        out.append({
            "index": int(image.get("INDEX") or 0),
            "name": text("NAME"),
            "edition": text("WINDOWS/EDITIONID") or text("FLAGS"),
            "arch": number("WINDOWS/ARCH"),
            "major": number("WINDOWS/VERSION/MAJOR"),
            "build": number("WINDOWS/VERSION/BUILD"),
            "type": text("WINDOWS/INSTALLATIONTYPE"),
            "product": text("WINDOWS/PRODUCTTYPE"),
            "languages": [n.text for n in image.findall("WINDOWS/LANGUAGES/LANGUAGE") if n.text],
        })
    return out


def is_server(image):
    return image["product"] in ("ServerNT", "LanmanNT") or image["type"].startswith("Server")


def is_windows_11(image):
    return image["major"] == 10 and image["build"] >= WIN11_BUILD and not is_server(image)


def pro_image(found):
    """The Windows 11 Pro edition in a list from images(), or the one
    sentence that says why there isn't one. The checks go from what the
    whole file is to which editions it has, so the sentence names the first
    thing wrong."""
    if not found:
        raise Unusable(UNREADABLE)
    windows_11 = [i for i in found if is_windows_11(i)]
    if not windows_11:
        if all(is_server(i) for i in found):
            raise Unusable(SERVER)
        if any(i["major"] == 10 for i in found):
            raise Unusable(WINDOWS_10)
        if any(0 < i["major"] < 10 for i in found):
            raise Unusable(OLDER)
        raise Unusable(UNREADABLE)
    x64 = [i for i in windows_11 if i["arch"] == AMD64]
    if not x64:
        raise Unusable(ARM if any(i["arch"] == ARM64 for i in windows_11) else NOT_X64)
    # Pro N, Pro Education and Pro for Workstations have their own edition
    # IDs; the answer file's key installs plain Pro.
    pro = [i for i in x64 if i["edition"] == "Professional"]
    if not pro:
        raise Unusable(NO_PRO)
    return pro[0]


def check(path):
    """What the file at path is, when it is Windows 11 Pro for x64:
    {"ok": True, "name", "index", "build", "bytes", "file", "languages"}.
    Unusable with the sentence to show when it isn't."""
    try:
        f = open(path, "rb")
    except FileNotFoundError:
        raise Unusable(GONE) from None
    except IsADirectoryError:
        raise Unusable(NOT_WINDOWS) from None
    except OSError:
        raise Unusable(CANT_OPEN) from None
    with f:
        try:
            size = os.fstat(f.fileno()).st_size
            if size < 257 * SECTOR:
                raise Unusable(NOT_WINDOWS)
            udf = Udf(f, size)
            found = udf.find("sources", "install.wim") or udf.find("sources", "install.esd")
            if not found:
                raise Unusable(NOT_WINDOWS)
            length, extents, inline = found
            if extents is None:
                raise Unusable(UNREADABLE)
            udf.check_extents(extents)
            image = pro_image(images(wim_xml(udf, extents)))
        except OSError:
            raise Unusable(CANT_OPEN) from None
        except (struct.error, IndexError):
            raise Unusable(UNREADABLE) from None
    if size < MIN_BYTES:
        raise Unusable(TOO_SMALL)
    return {"ok": True, "name": image["name"] or "Windows 11 Pro", "index": image["index"], "build": image["build"],
            "bytes": size, "file": os.path.basename(path), "languages": image["languages"]}


def main(argv):
    if len(argv) != 1:
        print("usage: isocheck.py FILE", file=sys.stderr)
        return 2
    try:
        result = check(argv[0])
    except Unusable as e:
        result = {"ok": False, "text": e.text}
    print(json.dumps(result))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
