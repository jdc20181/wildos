#!/usr/bin/env python3
"""Makes the wallpaper masters from the photographers' original files.

    python3 make-masters.py <folder of originals>

Run by hand when photos.json changes, not by the package build: the
originals are 100 MB and more and stay out of the repository. Each photo is
scaled and centre-cropped to 5120 by 2880 (16:9, 5K), the largest screen
Wild OS draws for, and saved as photos/<id>.webp. Plasma scales that down
for smaller screens and crops it for other shapes, so one size covers all.

A wallpaper with a "generator" instead of a "file" is drawn by code, not
photographed, and is skipped here. Its master is cut by opening the
generator in a browser at the size wanted and saving the canvas as
photos/<id>.webp. The 8K cut beside the generator is kept for re-cuts.
Warp's master is 10% over 5K, 5632 by 3168, because its wallpaper breathes
out to 8% larger and has to stay sharp on a 5K screen; it is the 8K cut
scaled down:
    magick warp/warp-7680x4320.webp -filter Lanczos -resize 5632x3168 -quality 88 photos/warp.webp
"""

import json
import sys
from pathlib import Path

from PIL import Image, ImageOps

SIZE = (5120, 2880)
QUALITY = 88

here = Path(__file__).resolve().parent
originals = Path(sys.argv[1])
out = here / "photos"
out.mkdir(exist_ok=True)
Image.MAX_IMAGE_PIXELS = None

for photo in json.loads((here / "photos.json").read_text()):
    if "file" not in photo:
        continue
    image = ImageOps.exif_transpose(Image.open(originals / photo["file"])).convert("RGB")
    # A few per cent of enlargement is invisible; more would soften it.
    if max(SIZE[0] / image.width, SIZE[1] / image.height) > 1.05:
        sys.exit(f"{photo['file']} is {image.width}x{image.height}, too small for {SIZE[0]}x{SIZE[1]}")
    target = out / f"{photo['id']}.webp"
    ImageOps.fit(image, SIZE, Image.LANCZOS).save(target, "WEBP", quality=QUALITY, method=6)
    print(target, round(target.stat().st_size / 1e6, 1), "MB")
