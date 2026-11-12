#!/usr/bin/env python3
"""Builds the theme wallpapers as Plasma wallpaper packages.

    python3 tools/wallpapers.py <branding dir> <wallpapers dir>

Each photo in branding/wallpaper/photos.json becomes
<wallpapers dir>/WildOS-<id>, laid out like the WildOS package:
contents/images/<width>x<height>.webp, named for the master's own size, a
screenshot for the pickers and a metadata.json naming the photographer and
the licence. A generated wallpaper such as Warp has no photographer, so its
metadata names only the licence. The masters in branding/wallpaper/photos
are made from the originals by make-masters.py, 5K except Warp's; Plasma
scales them down for smaller screens.
"""

import json
import subprocess
import sys
from pathlib import Path

LICENCES = {"Pexels": "Pexels License", "Unsplash": "Unsplash License", "GPL-3.0-or-later": "GPL-3.0-or-later"}


def main():
    branding = Path(sys.argv[1])
    dest = Path(sys.argv[2])
    photos = json.loads((branding / "wallpaper" / "photos.json").read_text())

    for photo in photos:
        package = dest / f"WildOS-{photo['id']}"
        images = package / "contents" / "images"
        images.mkdir(parents=True, exist_ok=True)

        master = branding / "wallpaper" / "photos" / f"{photo['id']}.webp"
        # identify, not magick identify: the dev loop caches magick runs by
        # their arguments and would answer a repeat with no output.
        size = subprocess.run(["identify", "-format", "%wx%h", str(master)],
                              check=True, capture_output=True, text=True).stdout.strip()
        (images / f"{size}.webp").write_bytes(master.read_bytes())

        subprocess.run(["magick", str(master), "-resize", "600x375^", "-gravity", "center",
                        "-extent", "600x375", "-quality", "85", str(package / "contents" / "screenshot.jpg")],
                       check=True)

        plugin = {
            "Id": f"WildOS-{photo['id']}",
            "License": LICENCES[photo["licence"]],
            "Name": photo["title"],
        }
        if "author" in photo:
            plugin["Authors"] = [{"Name": photo["author"]}]
            plugin["Website"] = photo["source"]
        metadata = {"KPlugin": dict(sorted(plugin.items()))}
        (package / "metadata.json").write_text(json.dumps(metadata, indent=4) + "\n")
        print(package)


if __name__ == "__main__":
    main()
