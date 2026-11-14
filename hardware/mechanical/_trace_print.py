#!/usr/bin/env python3
"""Trace the print and bare tin on the board's rear from Waveshare's photo.

The photo is RP2350-Touch-LCD-2.8-3.jpg, as check_photo.py takes it. It
is enlarged ZOOM times and traced in two passes.

Legends: LEGENDS lists a box round each legend, in mm as read from the
photo. Inside a box the photo is split into print and board by Otsu's
threshold on each pixel's darkest colour channel; only a solid of the
model's rear with KEEP_OFF round it is left out.

The rest of the board: a pixel counts when its darkest channel is above
BRIGHT and its channels lie within GREY of each other. Left out there:
everything within NEAR_LOW of a solid of the model's rear up to TALL
high and within NEAR_TALL of a higher one, whose sides the photo shows;
within EDGE of the board's edge, a cut-out or a standoff; and the two
flat cables.

Shapes under MIN_AREA are dropped. Each shape left is one outline with
its holes, simplified to within STRAIGHT of the traced edge. A shape
whose pixels average above PRINT in their darkest channel is print, a
darker one bare tin: a pad or solder beside a part.

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
BRIGHT, GREY, PRINT = 175, 50, 215
NEAR_LOW, NEAR_TALL, TALL, EDGE = 0.12, 0.60, 1.2, 0.60     # mm
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

    def within(a, b, c):
        """Whether c, on the line through a and b, lies on a to b."""
        return (min(a[0], b[0]) <= c[0] <= max(a[0], b[0])
                and min(a[1], b[1]) <= c[1] <= max(a[1], b[1]))

    for i in range(n):
        a, b = points[i], points[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            c, d = points[j], points[(j + 1) % n]
            s1, s2 = side(a, b, c), side(a, b, d)
            s3, s4 = side(c, d, a), side(c, d, b)
            if s1 * s2 < 0 and s3 * s4 < 0:
                return False
            if ((s1 == 0 and within(a, b, c)) or (s2 == 0 and within(a, b, d))
                    or (s3 == 0 and within(c, d, a))
                    or (s4 == 0 and within(c, d, b))):
                return False
    return True


def trace(photo):
    """Find the print.

    Args:
        photo: The photo as a PIL image.

    Returns:
        (shapes, mask): shapes as (outline, holes, tin) with the
        outlines as (xr, y) points in mm and tin True for bare tin, and
        the mask of traced pixels on the enlarged photo.
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
    size = (big.shape[1], big.shape[0])
    # 255 where a legend may be, and where anything else may be.
    in_box, rest = Image.new("L", size, 255), Image.new("L", size, 0)
    half = spec.BOARD_X / 2
    ImageDraw.Draw(rest).polygon(
        [pixel(x, y) for x, y in (
            (-half + EDGE, spec.BOARD_Y0 + EDGE),
            (spec.STEP_X - EDGE, spec.BOARD_Y0 + EDGE),
            (spec.STEP_X - EDGE, spec.BOARD_Y1 - EDGE),
            (-half + EDGE, spec.BOARD_Y1 - EDGE))], fill=255)

    def block(image, x0, y0, x1, y1, k):
        a, b = pixel(x0 - k, y0 - k), pixel(x1 + k, y1 + k)
        ImageDraw.Draw(image).rectangle(
            [min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]),
             max(a[1], b[1])], fill=0)

    for outer, _holes, d0, d1, _rgb, (o, u, v, w) in spec.rear_parts():
        corners = np.array([[o[i] + q[0] * u[i] + q[1] * v[i] + d * w[i]
                             for i in range(3)]
                            for q in outer for d in (d0, d1)])
        low, high = corners.min(0), corners.max(0)
        tall = -spec.BOARD_REAR - low[2] > TALL
        block(in_box, low[0], low[1], high[0], high[1], KEEP_OFF)
        block(rest, low[0], low[1], high[0], high[1],
              NEAR_TALL if tall else NEAR_LOW)
    for x, y in spec.STANDOFFS:
        for image, k in ((in_box, KEEP_OFF), (rest, EDGE)):
            r = spec.COLLAR_D / 2 + k
            a, b = pixel(x - r, y - r), pixel(x + r, y + r)
            ImageDraw.Draw(image).ellipse(
                [min(a[0], b[0]), min(a[1], b[1]), max(a[0], b[0]),
                 max(a[1], b[1])], fill=0)
    block(rest, *spec.SLOT, EDGE)
    block(rest, *spec.WINDOW[:4], EDGE)
    for xr0, xr1, y_end, _rgb in (spec.DISPLAY_CABLE, spec.TOUCH_CABLE):
        block(rest, -xr1, spec.SLOT[1], -xr0, y_end, EDGE)
    for _legend, xr0, y0, xr1, y1 in LEGENDS:
        block(rest, -xr1 + check_photo.SHIFT, y0, -xr0 + check_photo.SHIFT,
              y1, 0.0)
    bright = (dark > BRIGHT) & (big.max(2).astype(int) - dark < GREY)
    white = (white & (np.asarray(in_box) > 0)) | (
        bright & (np.asarray(rest) > 0))
    # Opening removes bridges and spurs one pixel wide, which make an
    # outline touch itself.
    mask = cv2.morphologyEx(white.astype(np.uint8), cv2.MORPH_OPEN,
                            np.ones((3, 3), np.uint8))
    found, tree = cv2.findContours(mask, cv2.RETR_CCOMP,
                                   cv2.CHAIN_APPROX_NONE)
    inverse = np.linalg.inv(np.vstack([matrix.T, [0.0, 0.0, 1.0]]))

    def to_mm(contour):
        # Simplify less when the simplified outline crosses itself.
        for straight in (STRAIGHT, STRAIGHT / 2, STRAIGHT / 4):
            simple = cv2.approxPolyDP(contour, straight * scale,
                                      True)[:, 0, :]
            q = np.column_stack([simple / ZOOM, np.ones(len(simple))])
            mm = (inverse @ q.T).T
            out = [(round(float(-(x + check_photo.SHIFT)), 2),
                    round(float(y), 2)) for x, y, _one in mm]
            if _simple(out):
                return out
        return None

    shapes, dropped = [], 0
    for i, contour in enumerate(found):
        if tree[0][i][3] != -1:             # a hole; taken with its outline
            continue
        if cv2.contourArea(contour) < MIN_AREA * scale * scale:
            continue
        outline = to_mm(contour)
        if outline is None:
            dropped += 1
            continue
        holes, child = [], tree[0][i][2]
        while child != -1:
            if cv2.contourArea(found[child]) >= MIN_AREA * scale * scale / 4:
                hole = to_mm(found[child])
                if hole is not None:
                    holes.append(hole)
            child = tree[0][child][0]
        inside = np.zeros(mask.shape, np.uint8)
        cv2.drawContours(inside, found, i, 1, cv2.FILLED)
        tin = bool(dark[(inside > 0) & (mask > 0)].mean() < PRINT)
        shapes.append((outline, holes, tin))
    shapes.sort(key=lambda s: (-max(q[1] for q in s[0]),
                               min(q[0] for q in s[0])))
    if dropped:
        print(f"{dropped} shapes dropped: outline crosses itself")
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
    lines = ['"""Print and bare tin on the rear, traced by _trace_print.py.',
             "",
             "Each shape is (outline, holes, tin): (xr, y) points in the",
             "rear view's frame, mm, and whether it is bare tin, not print.",
             'Do not edit; run _trace_print.py."""',
             "", "PRINT = ("]
    for outline, holes, tin in shapes:
        lines.append(f"    ({outline!r},")
        lines.append(f"     {holes!r}, {tin}),")
    lines.append(")")
    (HERE / "_print.py").write_text("\n".join(lines) + "\n")
    print(f"{len(shapes)} shapes, "
          f"{sum(s[2] for s in shapes)} of them tin, "
          f"{sum(len(o) + sum(len(h) for h in hs) for o, hs, _t in shapes)}"
          " points")
    if args.preview:
        Image.fromarray(mask * 255).save(args.preview)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
