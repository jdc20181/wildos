#!/usr/bin/env python3
"""Builds the theme wallpapers as Plasma wallpaper packages.

    python3 tools/wallpapers.py <branding dir> <wallpapers dir>

Each photo in branding/wallpaper/photos.json becomes
<wallpapers dir>/WildOS-<id>, laid out like the WildOS package:
contents/images/5120x2880.webp, a screenshot for the pickers and a
metadata.json naming the photographer and the licence. The 5K masters in
branding/wallpaper/photos are made from the originals by make-masters.py;
Plasma scales them down for smaller screens.
"""

import json
import subprocess
import sys
from pathlib import Path

LICENCES = {"Pexels": "Pexels License", "Unsplash": "Unsplash License"}


def main():
    branding = Path(sys.argv[1])
    dest = Path(sys.argv[2])
    photos = json.loads((branding / "wallpaper" / "photos.json").read_text())

    for photo in photos:
        package = dest / f"WildOS-{photo['id']}"
        images = package / "contents" / "images"
        images.mkdir(parents=True, exist_ok=True)

        master = branding / "wallpaper" / "photos" / f"{photo['id']}.webp"
        (images / "5120x2880.webp").write_bytes(master.read_bytes())

        subprocess.run(["magick", str(master), "-resize", "600x375^", "-gravity", "center",
                        "-extent", "600x375", "-quality", "85", str(package / "contents" / "screenshot.jpg")],
                       check=True)

        metadata = {
            "KPlugin": {
                "Authors": [{"Name": photo["author"]}],
                "Id": f"WildOS-{photo['id']}",
                "License": LICENCES[photo["licence"]],
                "Name": photo["title"],
                "Website": photo["source"],
            }
        }
        (package / "metadata.json").write_text(json.dumps(metadata, indent=4) + "\n")
        print(package)


if __name__ == "__main__":
    main()
