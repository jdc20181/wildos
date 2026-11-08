#!/usr/bin/env python3
"""Build the emoji panel's list from Unicode's emoji-test.txt and CLDR's
English annotations: every fully qualified emoji in Unicode's order, grouped
the way the panel shows them, with its name, search words and skin tone
variants.

Usage: make-emoji.py emoji-test.txt annotations/en.xml annotationsDerived/en.xml out.json
"""

import json
import sys
import xml.etree.ElementTree as ET

GROUPS = {
    "Smileys & Emotion": "Smileys and people",
    "People & Body": "Smileys and people",
    "Animals & Nature": "Animals and nature",
    "Food & Drink": "Food and drink",
    "Travel & Places": "Travel and places",
    "Activities": "Activities",
    "Objects": "Objects",
    "Symbols": "Symbols",
    "Flags": "Flags",
}
ORDER = ["Smileys and people", "Animals and nature", "Food and drink", "Travel and places",
         "Activities", "Objects", "Symbols", "Flags"]
TONES = [0x1F3FB, 0x1F3FC, 0x1F3FD, 0x1F3FE, 0x1F3FF]


def bare(text):
    return text.replace("️", "")


def annotations(paths):
    words, names = {}, {}
    for path in paths:
        for node in ET.parse(path).getroot().iter("annotation"):
            cp = bare(node.get("cp", ""))
            if node.get("type") == "tts":
                names.setdefault(cp, node.text or "")
            else:
                words.setdefault(cp, [w.strip() for w in (node.text or "").split("|") if w.strip()])
    return words, names


def main():
    test, base_xml, derived_xml, out = sys.argv[1:5]
    words, names = annotations([base_xml, derived_xml])
    groups = {name: [] for name in ORDER}
    by_base = {}
    group = None
    with open(test, encoding="utf-8") as handle:
        for line in handle:
            line = line.rstrip("\n")
            if line.startswith("# group:"):
                group = GROUPS.get(line.split(":", 1)[1].strip())
                continue
            if not line or line.startswith("#") or group is None:
                continue
            codes, rest = line.split(";", 1)
            if "fully-qualified" not in rest.split("#", 1)[0]:
                continue
            points = [int(c, 16) for c in codes.split()]
            text = "".join(chr(p) for p in points)
            tones = [p for p in points if p in TONES]
            if tones:
                # Only one tone for the whole emoji: the panel's tone choice
                # applies to every person in it alike.
                if len(set(tones)) != 1:
                    continue
                base = "".join(chr(p) for p in points if p not in TONES)
                entry = by_base.get(bare(base))
                if entry is not None:
                    entry[3][TONES.index(tones[0])] = text
                continue
            key = bare(text)
            fallback = rest.split("#", 1)[1].split(" ", 3)[-1].strip() if "#" in rest else ""
            name = names.get(key) or fallback
            keys = [w for w in words.get(key, []) if w.lower() != name.lower()]
            entry = [text, name, " ".join(keys), ["", "", "", "", ""]]
            by_base[key] = entry
            groups[group].append(entry)

    result = []
    for name in ORDER:
        emoji = []
        for text, label, keys, tones in groups[name]:
            item = [text, label[:1].upper() + label[1:], keys]
            if all(tones):
                item.append(tones)
            emoji.append(item)
        result.append({"name": name, "emoji": emoji})
    with open(out, "w", encoding="utf-8") as handle:
        json.dump({"groups": result}, handle, ensure_ascii=False, separators=(",", ":"))


if __name__ == "__main__":
    main()
