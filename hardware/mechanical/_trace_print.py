#!/usr/bin/env python3
"""Trace the board's printed legends from Waveshare's photo of its rear.

The photo is RP2350-Touch-LCD-2.8-3.jpg, as check_photo.py takes it.
LEGENDS lists a box round each legend, in mm as read from the photo.
Inside a box the photo, enlarged ZOOM times, is split into print and
board by Otsu's threshold on each pixel's darkest colour channel. Left
out: everything within KEEP_OFF of a solid of the model's rear or of a
standoff, and shapes under MIN_AREA. Each shape left is one outline
with its holes, simplified to within STRAIGHT of the traced edge.

Not traced: the maker's logo and name, the outlines printed round
parts, and the marks on the flat cables.

Usage:
    python3 hardware/mechanical/_trace_print.py PHOTO [--preview OUT.png]

Writes hardware/mechanical/_print.py, the shapes in the rear view's
frame in mm. Needs numpy, Pillow and OpenCV (pip install
opencv-python-headless).
"""

import argparse
import sys
from pathlib import Path

import cv2
import numpy as np
from PIL import Image, ImageDraw

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))
import check_photo  # noqa: E402
import touch_lcd_2_8 as spec  # noqa: E402

ZOOM = 4
KEEP_OFF = 0.10         # mm
MIN_AREA = 0.05         # mm2
STRAIGHT = 0.04         # mm
# Legend, then xr0, y0, xr1, y1 of its box in the photo's mm.
LEGENDS = (
    ("RP2350-Touch-LCD-2.8", -6.6, 26.2, 16.4, 28.6),
    ("240x320 Pixels", -6.6, 24.0, 5.7, 26.0),
    ("BOOT", -20.2, 17.4, -18.95, 21.0),
    ("RESET", -20.2, 9.4, -18.95, 13.45),
    ("BAT_PWR", -20.2, 0.8, -18.95, 6.6),
    ("IO29 to GND", -23.3, -27.7, -20.25, -15.3),
    ("GND 3V3 SDA SCL", -13.7, -31.55, -9.05, -28.7),
    ("PWR", -8.7, -32.15, -5.7, -30.9),
    ("Charge", 5.4, -32.15, 9.5, -30.7),
    ("GND 3V3 TXD RXD", 9.55, -31.8, 13.85, -29.1),
    ("I2C", -18.0, -36.85, -15.95, -35.6),
    ("UART", 15.55, -36.85, 19.0, -35.6),
    ("Volume", 23.05, 4.05, 24.35, 8.6),
    ("R", 21.85, -4.1, 23.2, -2.75),
    ("Speaker", 22.85, -10.0, 24.35, -5.25),
    ("L", 22.15, -12.35, 23.2, -11.1),
    ("RTC", 22.9, -18.0, 24.3, -15.25),
    ("RTC -", 22.4, -15.05, 23.1, -14.3),
    ("RTC +", 22.4, -18.7, 23.2, -17.85),
    ("BAT", 23.05, -25.3, 24.7, -22.7),
    ("BAT +", 22.4, -22.3, 23.3, -21.3),
    ("BAT -", 22.4, -26.9, 23.0, -25.8),
    ("X", -9.2, -16.3, -8.05, -15.3),
    ("axes, Z", -9.2, -19.6, -7.95, -16.3),
    ("axes, Y", -7.95, -18.4, -5.7, -17.1),
)


def _simple(points):
    """Whether a closed polygon does not cross or touch itself."""
    n = len(points)
    if n < 3 or len({tuple(q) for q in points}) < n:
        return False

    def side(a, b, c):
        return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0])

    for i in range(n):
        a, b = points[i], points[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            c, d = points[j], points[(j + 1) % n]
            if (side(a, b, c) * side(a, b, d) <= 0
                    and side(c, d, a) * side(c, d, b) <= 0):
                return False
    return True


def trace(photo):
    """Find the print.

    Args:
        photo: The photo as a PIL image.

    Returns:
        (shapes, mask): shapes as (outline, holes) of (xr, y) points in
        mm, and the mask of print pixels on the enlarged photo.
    """
    matrix, _worst = check_photo.fit(np.asarray(photo).astype(int))
    scale = np.linalg.norm(matrix[0]) * ZOOM

    def pixel(x, y):
        q = np.array([x - check_photo.SHIFT, y, 1.0]) @ matrix
        return ZOOM * q[0], ZOOM * q[1]

    big = np.asarray(photo.resize((photo.width * ZOOM, photo.height * ZOOM),
                                  Image.BICUBIC))
    dark = np.ascontiguousarray(big.min(2))
    white = np.zeros(dark.shape, bool)
    for _legend, xr0, y0, xr1, y1 in LEGENDS:
        a = pixel(-xr0 + check_photo.SHIFT, y0)
        b = pixel(-xr1 + check_photo.SHIFT, y1)
        c0, c1 = sorted((int(a[0]), int(b[0])))
        r0, r1 = sorted((int(a[1]), int(b[1])))
        _level, part = cv2.threshold(dark[r0:r1, c0:c1], 0, 1,
                                     cv2.THRESH_BINARY + cv2.THRESH_OTSU)
        white[r0:r1, c0:c1] |= part > 0
    allowed = Image.new("L", (big.shape[1], big.shape[0]), 255)
    draw = ImageDraw.Draw(allowed)
    k = KEEP_OFF

    def block(x0, y0, x1, y1):
        a, b = pixel(x0 - k, y0 - k), pixel(x1 + k, y1 + k)
        draw.rectangle([min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]),
                        max(a[1], b[1])], fill=0)

    for outer, _holes, d0, d1, _rgb, (o, u, v, w) in (spec.rear_parts()
                                                    + spec.cables()):
        corners = np.array([[o[i] + q[0] * u[i] + q[1] * v[i] + d * w[i]
                             for i in range(3)]
                            for q in outer for d in (d0, d1)])
        if corners[:, 2].max() <= -spec.BOARD_REAR + 1e-6:
            low, high = corners.min(0), corners.max(0)
            block(low[0], low[1], high[0], high[1])
    for x, y in spec.STANDOFFS:
        r = spec.COLLAR_D / 2 + k
        a, b = pixel(x - r, y - r), pixel(x + r, y + r)
        draw.ellipse([min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]),
                      max(a[1], b[1])], fill=0)
    mask = (white & (np.asarray(allowed) > 0)).astype(np.uint8)
    found, tree = cv2.findContours(mask, cv2.RETR_CCOMP,
                                   cv2.CHAIN_APPROX_NONE)
    inverse = np.linalg.inv(np.vstack([matrix.T, [0.0, 0.0, 1.0]]))

    def to_mm(contour):
        simple = cv2.approxPolyDP(contour, STRAIGHT * scale, True)[:, 0, :]
        q = np.column_stack([simple / ZOOM, np.ones(len(simple))])
        mm = (inverse @ q.T).T
        return [(round(float(-(x + check_photo.SHIFT)), 2),
                 round(float(y), 2)) for x, y, _one in mm]

    shapes = []
    for i, contour in enumerate(found):
        if tree[0][i][3] != -1:             # a hole; taken with its outline
            continue
        if cv2.contourArea(contour) < MIN_AREA * scale * scale:
            continue
        outline = to_mm(contour)
        if not _simple(outline):
            continue
        holes, child = [], tree[0][i][2]
        while child != -1:
            hole = to_mm(found[child])
            if (cv2.contourArea(found[child]) >= MIN_AREA * scale * scale / 4
                    and _simple(hole)):
                holes.append(hole)
            child = tree[0][child][0]
        shapes.append((outline, holes))
    shapes.sort(key=lambda s: (-max(q[1] for q in s[0]),
                               min(q[0] for q in s[0])))
    return shapes, mask


def main(argv):
    """Run from the command line.

    Args:
        argv: Command-line arguments without the program name.

    Returns:
        0.
    """
    ap = argparse.ArgumentParser()
    ap.add_argument("photo", type=Path)
    ap.add_argument("--preview", type=Path)
    args = ap.parse_args(argv)
    shapes, mask = trace(Image.open(args.photo).convert("RGB"))
    lines = ['"""The board\'s white print, traced by _trace_print.py.',
             "",
             "Each shape is (outline, holes) of (xr, y) points in the rear",
             'view\'s frame, mm. Do not edit; run _trace_print.py."""',
             "", "PRINT = ("]
    for outline, holes in shapes:
        lines.append(f"    ({outline!r},")
        lines.append(f"     {holes!r}),")
    lines.append(")")
    (HERE / "_print.py").write_text("\n".join(lines) + "\n")
    print(f"{len(shapes)} shapes, "
          f"{sum(len(o) + sum(len(h) for h in hs) for o, hs in shapes)} "
          "points")
    if args.preview:
        Image.fromarray(mask * 255).save(args.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
