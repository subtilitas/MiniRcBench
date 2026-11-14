#!/usr/bin/env python3
"""Draw the model's rear parts on Waveshare's photo of the board's rear.

The photo is RP2350-Touch-LCD-2.8-3.jpg, 1600 x 1200 pixels, from
Waveshare's product page of the RP2350-Touch-LCD-2.8. It is not in this
repository. The photo is fitted to the model at the 4 standoffs: each
standoff's centre is the centre of the bright pixels within 45 pixels of
NEAR, and a least-squares affine map takes the model's standoff
positions to those centres.

The output is the photo, 3 times enlarged, with the outline box of
every solid behind the board's rear face in magenta. A part placed
wrongly shows as a box beside its part.

Usage:
    python3 hardware/mechanical/check_photo.py PHOTO OUT.png

Prints the pixels per mm and the largest distance left at a standoff.
Exit status 1 when that distance is above 2 pixels. Needs numpy and
Pillow.
"""

import sys
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import touch_lcd_2_8 as spec  # noqa: E402

# Model (x, y) of each standoff to its place in the photo, pixels. The
# photo shows the rear, so model +x is on its left.
NEAR = {(-20.5, 27.1): (1073, 193), (20.5, 27.1): (527, 193),
        (-20.5, -32.9): (1073, 995), (20.5, -32.9): (527, 995)}
SHIFT = 0.15            # mm, the photo's offset the spec corrects for
ZOOM = 3


def fit(pixels):
    """The affine map from model (x, y) to photo pixels.

    Args:
        pixels: The photo as an (h, w, 3) integer array.

    Returns:
        (map, worst): a 3 x 2 matrix for (x, y, 1), and the largest
        distance in pixels between a standoff and its mapped position.
    """
    model, found = [], []
    for (x, y), (px, py) in NEAR.items():
        window = pixels[py - 45:py + 45, px - 45:px + 45]
        bright = ((window.min(2) > 150)
                  & (window.max(2) - window.min(2) < 40))
        rows, cols = np.nonzero(bright)
        model.append((x, y, 1.0))
        found.append((px - 45 + cols.mean(), py - 45 + rows.mean()))
    model, found = np.array(model), np.array(found)
    matrix = np.linalg.lstsq(model, found, rcond=None)[0]
    worst = np.linalg.norm(model @ matrix - found, axis=1).max()
    return matrix, worst


def main(argv):
    """Run from the command line.

    Args:
        argv: The photo and the output file.

    Returns:
        0 when the fit holds, else 1.
    """
    photo = Image.open(argv[0]).convert("RGB")
    matrix, worst = fit(np.asarray(photo).astype(int))
    scale = np.linalg.norm(matrix[0])
    print(f"{scale:.2f} px/mm, {worst:.2f} px left at the worst standoff")
    big = photo.resize((photo.width * ZOOM, photo.height * ZOOM),
                       Image.BICUBIC)
    draw = ImageDraw.Draw(big)
    for outer, _holes, d0, d1, _rgb, (o, u, v, w) in (spec.rear_parts()
                                                    + spec.cables()):
        corners = np.array([[o[k] + q[0] * u[k] + q[1] * v[k] + d * w[k]
                             for k in range(3)]
                            for q in outer for d in (d0, d1)])
        if corners[:, 2].max() > -spec.BOARD_REAR + 1e-6:
            continue
        low, high = corners.min(0), corners.max(0)
        a = np.array([low[0] - SHIFT, low[1], 1.0]) @ matrix
        b = np.array([high[0] - SHIFT, high[1], 1.0]) @ matrix
        draw.rectangle([ZOOM * min(a[0], b[0]), ZOOM * min(a[1], b[1]),
                        ZOOM * max(a[0], b[0]), ZOOM * max(a[1], b[1])],
                       outline=(255, 0, 255))
    big.save(argv[1])
    return 1 if worst > 2.0 else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
