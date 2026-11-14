"""Waveshare RP2350-Touch-LCD-2.8, the board MiniRcBench runs on.

A facsimile: the cover glass, the display stack, the board with its
cut-outs, the 4 standoffs, the two flat cables and every part on the
board's rear that is 1.0 or more long. It is not Waveshare's model.
Waveshare publishes no 3D file of this board; the dimensions come from
four sources, all mm:

- D: Waveshare's dimension drawing of the RP2350-Touch-LCD-2.8
  (waveshare.com/wiki/RP2350-Touch-LCD-2.8): cover glass 73.06 x 50.54,
  display area 57.60 x 43.20, board 69.00 x 49.90, holes 2.5.
- E: Waveshare's STEP file of the ESP32-S3-Touch-LCD-2.8
  (ESP32-S3-Touch-LCD-2.8_3D.zip on that board's wiki page), read with
  OpenCASCADE. That board has the same glass, display, board outline,
  cut-outs, connectors, buttons, card slot and audio amplifier in the
  same places. Taken from it: every height, the display stack, the
  board's outline and cut-outs, and the sockets, buttons, USB-C
  receptacle, card slot and amplifier.
- P: Waveshare's photo of the RP2350 board's rear, 1600 x 1200
  (RP2350-Touch-LCD-2.8-3.jpg on the product page), fitted to the 4
  standoffs at 13.3 px/mm. A position read from it is good to 0.3.
  Taken from it: every part E does not have in that place, which is
  every chip part, the RP2350, the flash, the converter, the small
  semiconductors, the trimmer and the two flat-cable sockets.
- Q: Waveshare's angled photo of the rear, 1600 x 1200
  (RP2350-Touch-LCD-2.8-5.jpg). The 4 standoffs' tops, 4.03 high (E),
  give its scale: 14.25 px/mm across and 9.6 px/mm of height. Taken
  from it: the standoffs' body, 5.5 +- 0.3 across, and the trimmer's
  height, 2.5 +- 0.3.
- N: nominal package sizes, for the parts placed from P.

Against the parts E and P share, positions read from P lie 0.15 to
the right of E on average seen from the rear; every position from P is
corrected by that.

The stack, from the glass's front face back (E): glass 0.70; touch
film 0.35; display module 2.33, its front 1.27 behind the glass's
front; board 1.60, its front 4.40 behind the glass's front; standoffs
4.03 tall on the board's rear, so the whole is 10.03 thick.

Estimated, in no source: the heights of the parts placed from P (N
gives the package's), the height of the standoffs' collar (0.3), the
walls of the
sockets and of the USB-C shell (0.6 and 0.3), the display module as a
steel back 0.3 thick with a white frame 0.3 wide, the printed border as 0.05 thick and on the glass's
front, the 0.3 deep counterbore of the standoffs.

The standoffs are 5.5 across in E and in Q; the 6.2 seen in P is the
collar at their foot. Differences from the board: the flat
cables fold round the display's lower end 0.25 from it; that distance
is estimated. Chip parts are drawn without solder. Pads,
print, tracks, vias, the battery and the speaker are not modelled. No
dimension is verified with a caliper on a board.

Model frame: the origin is the centre of the display area on the
glass's front face; z = 0 is that face, +z towards the viewer; x to the
right and y up seen from the front in portrait, USB-C at -y. The parts
on the rear are listed in the rear view's frame, as the photo shows
them: xr = -x to the right, y up, height above the board's rear face.
"""

import _parts as p
from _solid import X, box, circle, ext, items, place, rect, rounded, stadium, strip

BLUE = (0.05, 0.20, 0.47)
GLASS = (0.16, 0.17, 0.19)
FILM = (0.25, 0.25, 0.27)
FLEX = (0.88, 0.50, 0.10)

# Glass and display (D, E).
GLASS_X, GLASS_Y0, GLASS_Y1, GLASS_T, GLASS_R = 50.54, -39.53, 33.53, 0.70, 2.0
WINDOW_X, WINDOW_Y = 43.80, 58.20       # clear part of the glass (E)
BORDER_T = 0.05
FILM_X, FILM_Y0, FILM_Y1, FILM_T = 48.0, -33.15, 30.95, 0.35
LCD_X, LCD_Y0, LCD_Y1, LCD_FRONT, LCD_REAR = 50.0, -37.50, 31.70, 1.27, 3.60
# Board (D for the size, E for the place and the cut-outs).
BOARD_X, BOARD_Y0, BOARD_Y1, BOARD_R = 49.90, -37.40, 31.60, 1.0
BOARD_FRONT, BOARD_REAR = 4.40, 6.00
STEP_X, STEP_Y0, STEP_Y1, STEP_R = 23.58, -29.33, 24.13, 0.67
SLOT = (-18.01, -0.73, 9.01, 2.28)      # for the two flat cables
WINDOW = (-16.30, -26.30, -8.10, -16.90, 1.0)   # over the touch chip
STANDOFFS = ((-20.5, -32.9), (20.5, -32.9), (-20.5, 27.1), (20.5, 27.1))
STANDOFF_D, STANDOFF_H, NECK_D, THREAD_D, BORE_D = 5.5, 4.03, 4.3, 2.5, 3.4
COLLAR_D, COLLAR_H = 6.2, 0.3           # ring at the standoff's foot (P, Q)
# Flat cables (E): thickness, z of the run between display and board,
# height of the run above the board's rear.
CABLE_T, CABLE_Z, CABLE_UP = 0.19, -3.775, 0.925

R, C = "r0402", "c0402"
# Chip parts on the rear (P): kind, xr, y, turn in degrees; at turn 0
# the part's length runs along x.
CHIPS = (
    (C, -15.20, 27.55, 90), (C, -10.80, 27.55, 90),
    (R, -15.85, 25.0, 90), (C, -14.30, 25.0, 90), (C, -13.50, 25.0, 90),
    (C, -12.70, 25.0, 90), (R, -11.70, 25.0, 90),
    (R, -18.65, 19.6, 90), (R, -18.65, 18.4, 90), (R, -18.65, 16.7, 90),
    (R, -7.35, 21.0, 0), (R, -7.35, 20.0, 0), (R, -7.35, 19.05, 0),
    ("c0603", -7.35, 17.55, 0),
    (R, -4.05, 21.7, 90), (R, -3.05, 21.7, 90),
    (R, -4.05, 18.57, 90), (R, -3.02, 18.57, 90), (R, -1.99, 18.57, 90),
    (R, -0.92, 18.57, 90), (R, 0.14, 18.57, 90),
    (R, -18.40, 13.4, 90), (C, -17.40, 13.4, 90), (C, -13.75, 13.4, 90),
    (R, -12.75, 13.4, 90), (R, -9.75, 13.85, 0), (C, -9.75, 12.85, 0),
    (R, -17.25, 4.7, 0),
    (R, -18.57, 2.1, 90), (R, -17.77, 2.1, 90), (R, -16.96, 2.1, 90),
    (R, -16.16, 2.1, 90), (R, -15.35, 2.1, 90), (R, -14.55, 2.1, 90),
    (R, -13.74, 2.1, 90), (R, -12.94, 2.1, 90),
    (R, -7.75, 7.25, 0), (R, -7.75, 6.05, 0),
    (R, -9.65, -1.45, 90), (R, -8.50, -1.65, 90), (R, -7.20, -1.65, 90),
    (R, -5.90, -1.65, 90), (R, -4.62, -1.65, 90),
    (R, -2.20, -1.63, 0), (R, -0.55, -1.63, 0),
    (R, 18.82, 12.15, 0), (R, 18.82, 10.7, 0), (R, 21.55, 10.7, 0),
    (R, -9.65, -11.2, 90), (R, -8.80, -14.2, 90),
    (C, -1.90, -3.7, 90), (C, -1.90, -9.9, 90),
    ("r0603", -0.30, -6.05, 0), ("r0603", -0.30, -7.6, 0),
    (R, -0.52, -2.85, 90), (R, -0.52, -4.25, 90), (R, -0.52, -9.5, 90),
    (R, -0.52, -11.2, 90),
    (C, -8.60, -11.4, 90), (C, -7.30, -11.4, 90), (R, -6.05, -11.4, 90),
    (R, -5.05, -11.4, 90), (C, -3.70, -11.4, 90),
    (R, -2.35, -12.25, 0), ("c0805", 0.95, -12.9, 0),
    (R, 3.05, -12.9, 90), (R, 4.20, -12.9, 90), (R, 5.35, -12.9, 90),
    (R, 6.50, -12.9, 90), (R, 7.70, -12.9, 90),
    (R, 1.85, -14.3, 0), (R, 1.85, -15.15, 0), (R, 1.85, -16.0, 0),
    (R, -4.15, -17.15, 90), (R, 6.55, -18.35, 0),
    (C, 4.45, -19.6, 0), (R, 2.60, -19.6, 0), (R, 6.45, -19.6, 0),
    (C, -7.45, -19.6, 90), (C, -6.50, -19.6, 90),
    (C, -7.55, -25.25, 90), (C, -6.50, -25.25, 90),
    (R, -0.30, -28.1, 90), (R, 0.70, -28.1, 90), (R, 1.12, -22.0, 90),
    (R, 6.50, -20.65, 0), (R, 6.50, -21.5, 0), (R, 6.50, -22.45, 0),
    ("c0603", 6.35, -24.75, 90),
    (C, 2.50, -27.35, 0), (R, 4.30, -27.35, 0),
    ("led0603", -7.30, -30.15, 0), (R, -7.25, -29.05, 0),
    ("led0603", 7.20, -30.15, 0), (R, 7.40, -28.9, 0),
)

# Other parts on the rear: name, shapes, xr, y, turn. The first group
# is placed and sized from E, the second placed from P and sized from N.
PARTS = (
    ("USB-C", p.usb_c(), 0.0, -30.93, 0),
    ("I2C socket, 4 pins 1.0",
     p.socket(4, 1.0, 6.35, 5.2, 3.05, 0.7, 0.8), -11.5, -34.805, 0),
    ("UART socket, 4 pins 1.0",
     p.socket(4, 1.0, 6.35, 5.2, 3.05, 0.7, 0.8), 11.5, -34.805, 0),
    ("12-pin socket, 1.0",
     p.socket(12, 1.0, 14.35, 5.1, 3.13, 0.7, 0.5), -17.03, -21.7, 90),
    ("battery socket, 2 pins 1.25",
     p.socket(2, 1.25, 7.65, 5.2, 3.4, 1.0, 0.8, 1.7, 2.2, 2.0),
     20.105, -24.225, -90),
    ("RTC battery socket, 2 pins 1.0",
     p.socket(2, 1.0, 4.35, 5.2, 3.05, 0.7, 0.8), 20.105, -16.875, -90),
    ("speaker socket, 4 pins 1.25",
     p.socket(4, 1.25, 10.15, 5.2, 3.4, 1.0, 0.8, 1.7, 2.2, 2.0),
     20.105, -7.575, -90),
    ("PWR button", p.side_button(), -22.43, 3.62, 0),
    ("RESET button", p.side_button(), -22.43, 11.44, 0),
    ("BOOT button", p.side_button(), -22.43, 19.26, 0),
    ("card slot", p.micro_sd(), -16.985, -7.235, 0),
    ("APA2068 amplifier, SOP-16",
     p.leaded(3.68, 9.67, 1.75, p.rows(8, 1.27), p.rows(8, 1.27), 5.8, 0.4),
     4.41, -6.735, 0),

    ("RP2350A, QFN-60", p.qfn(7.0, 7.0, 0.9, 15, 15, 0.4), -13.58, 19.37, 0),
    ("crystal, 2.5 x 2.0", p.can(2.5, 2.0, 0.6, p.TAN), -12.93, 27.56, 0),
    ("flash, SOIC-8 5.28 wide",
     p.leaded(5.28, 5.28, 1.95, p.rows(4, 1.27), p.rows(4, 1.27), 7.9, 0.42),
     -13.65, 8.07, 0),
    ("inductor, 2.0 x 2.0", p.block(2.0, 2.0, 1.0, p.EPOXY), -15.72, 13.3, 0),
    ("SOT-23", p.sot23(2, 1), -0.35, 21.7, 0),
    ("PCM5101A converter, TSSOP-20",
     p.leaded(4.4, 6.5, 1.1, p.rows(10, 0.65), p.rows(10, 0.65), 6.4, 0.22),
     -6.15, -6.65, 90),
    ("QMI8658 motion sensor, LGA 2.5 x 3.0", p.qfn(2.5, 3.0, 0.86, 0, 0, 0.5),
     -6.10, -15.1, 0),
    ("PCF85063 clock, 2.7 x 2.7", p.qfn(2.7, 2.7, 0.5, 0, 0, 0.5), -2.42, -14.55, 0),
    ("crystal, 2.0 x 1.2", p.can(2.0, 1.2, 0.6, p.STEEL), 0.27, -15.15, 90),
    ("SOT-23", p.sot23(1, 2), 5.05, -15.7, 0),
    ("diode, SOD-323", p.sod(1.7, 1.25, 0.95, 2.5, 0.3), -1.50, -17.3, 0),
    ("diode, SOD-323", p.sod(1.7, 1.25, 0.95, 2.5, 0.3), -1.50, -18.75, 0),
    ("diode, SOD-323", p.sod(1.7, 1.25, 0.95, 2.5, 0.3), 2.70, -18.05, 0),
    ("SOT-23-5", p.sot23(2, 3), -6.25, -22.35, 0),
    ("diode, SOD-123", p.sod(2.7, 1.6, 1.1, 3.7, 0.6), -4.85, -26.6, 90),
    ("SOT-23", p.sot23(1, 2), -1.35, -21.35, 0),
    ("SOT-23", p.sot23(1, 2), -1.35, -24.85, 0),
    ("inductor, 2.5 x 2.0", p.block(2.5, 2.0, 1.0, p.GREY), 3.57, -21.55, 0),
    ("charger, DFN-10 3 x 3", p.qfn(3.0, 3.0, 0.75, 0, 5, 0.5),
     3.20, -24.55, 0),
    ("volume trimmer", p.trimmer(4.95, 2.5, 2.4), 20.30, 6.3, 0),
    ("display cable socket",
     p.flat_cable_socket(13.2, 3.75, 2.2, 0.95, 1.25, 2.0), -0.70, 10.65, 0),
    ("touch cable socket",
     p.flat_cable_socket(7.55, 3.7, 2.25, 0.5, 1.3, 2.0), 11.93, 7.25, 0),
)

# Flat cables on the rear (E, P): xr of the two edges, y where the cable
# enters its socket, colour.
DISPLAY_CABLE = (-5.45, 4.05, 10.65, FLEX)
TOUCH_CABLE = (10.38, 13.88, 7.25, p.BLACK)
# Both cables fold round the display's lower end: distance of the fold's
# centre from that end (estimated), z of the touch cable under the glass (E).
FOLD_GAP, TAIL_Z = 0.25, -0.875
STIFFENER_Y = 7.15                      # display cable, black from here on
# The touch cable's wide part behind the board's window (E, P), and the
# parts on it (P): the CST328 touch controller, 5 x 5, and 5 small parts.
TOUCH_PAD = (7.44, -26.60, 16.69, -16.60)
TOUCH_PARTS = (
    (p.qfn(5.0, 5.0, 0.85, 12, 12, 0.4), 11.78, -22.78, 0),
    (p.r0402(), 8.75, -17.9, 90), (p.r0402(), 9.75, -17.9, 90),
    (p.c0402(), 12.15, -17.9, 90), (p.c0402(), 13.05, -17.9, 90),
    (p.block(1.0, 1.6, 0.6, p.EPOXY, 0.1), 14.25, -17.7, 0),
)


def _board_outline():
    """The board's outline with the step at the buttons' edge (E)."""
    x, r, s = BOARD_X / 2, BOARD_R, STEP_R
    return [
        (-x + r, BOARD_Y0), (x - r, BOARD_Y0, (x - r, BOARD_Y0 + r, 1)),
        (x, BOARD_Y0 + r), (x, STEP_Y0 - s, (x - s, STEP_Y0 - s, 1)),
        (x - s, STEP_Y0), (STEP_X + s, STEP_Y0, (STEP_X + s, STEP_Y0 + s, -1)),
        (STEP_X, STEP_Y0 + s),
        (STEP_X, STEP_Y1 - s, (STEP_X + s, STEP_Y1 - s, -1)),
        (STEP_X + s, STEP_Y1), (x - s, STEP_Y1, (x - s, STEP_Y1 + s, 1)),
        (x, STEP_Y1 + s), (x, BOARD_Y1 - r, (x - r, BOARD_Y1 - r, 1)),
        (x - r, BOARD_Y1), (-x + r, BOARD_Y1, (-x + r, BOARD_Y1 - r, 1)),
        (-x, BOARD_Y1 - r), (-x, BOARD_Y0 + r, (-x + r, BOARD_Y0 + r, 1)),
    ]


def front():
    """Glass, touch film, display module and board, in the model frame."""
    gx, wx, wy = GLASS_X / 2, WINDOW_X / 2, WINDOW_Y / 2
    glass = rounded(-gx, GLASS_Y0, gx, GLASS_Y1, GLASS_R)
    lx, rim = LCD_X / 2, 0.3
    holes = [circle(x, y, NECK_D / 2) for x, y in STANDOFFS]
    holes.append(stadium(*SLOT))
    holes.append(rounded(*WINDOW))
    return [
        ext(glass, -BORDER_T, 0.0, p.BLACK, holes=[rect(-wx, -wy, wx, wy)]),
        ext(glass, -GLASS_T, -BORDER_T, GLASS),
        box(-FILM_X / 2, FILM_Y0, FILM_X / 2, FILM_Y1, -GLASS_T - FILM_T,
            -GLASS_T, FILM),
        box(-lx, LCD_Y0, lx, LCD_Y1, -LCD_REAR, -LCD_REAR + rim, p.STEEL),
        ext(rect(-lx, LCD_Y0, lx, LCD_Y1), -LCD_REAR + rim, -LCD_FRONT,
            p.WHITE, holes=[rect(-lx + rim, LCD_Y0 + rim, lx - rim,
                                 LCD_Y1 - rim)]),
        box(-lx + rim, LCD_Y0 + rim, lx - rim, LCD_Y1 - rim, -LCD_REAR + rim,
            -LCD_FRONT, p.BLACK),
        ext(_board_outline(), -BOARD_REAR, -BOARD_FRONT, BLUE, holes=holes),
    ]


def standoffs():
    """The 4 threaded standoffs pressed into the board."""
    out = []
    for x, y in STANDOFFS:
        thread, top = circle(x, y, THREAD_D / 2), -BOARD_REAR - STANDOFF_H
        out.append(ext(circle(x, y, NECK_D / 2), -BOARD_REAR, -BOARD_FRONT,
                       p.STEEL, holes=[thread]))
        out.append(ext(circle(x, y, COLLAR_D / 2), -BOARD_REAR - COLLAR_H,
                       -BOARD_REAR, p.STEEL, holes=[thread]))
        out.append(ext(circle(x, y, STANDOFF_D / 2), top + 0.3,
                       -BOARD_REAR - COLLAR_H, p.STEEL, holes=[thread]))
        out.append(ext(circle(x, y, STANDOFF_D / 2), top, top + 0.3, p.STEEL,
                       holes=[circle(x, y, BORE_D / 2)]))
    return out


def cables():
    """The two flat cables from the display's lower end to their sockets."""
    rear, t = -BOARD_REAR - CABLE_UP, CABLE_T
    out = []
    fold = LCD_Y0 - FOLD_GAP
    starts = (
        # The display cable leaves the display module's lower end.
        [(LCD_Y0, -LCD_FRONT - 0.9), (fold, -LCD_FRONT - 0.9)],
        # The touch cable leaves the touch film between glass and display.
        [(FILM_Y0, TAIL_Z), (fold, TAIL_Z)],
    )
    for (xr0, xr1, y_end, rgb), start in zip((DISPLAY_CABLE, TOUCH_CABLE),
                                             starts, strict=True):
        path = [*start, (fold, CABLE_Z), (0.0, CABLE_Z), (1.6, rear),
                (y_end, rear)]
        out.append(ext(strip(path, t), -xr1, -xr0, rgb, frame=X))
    xr0, xr1, y_end, _rgb = DISPLAY_CABLE
    out.append(box(-xr1, STIFFENER_Y, -xr0, y_end, rear - t / 2 - 0.2,
                   rear - t / 2, p.BLACK))
    # The touch cable's wide part.
    x0, y0, x1, y1 = TOUCH_PAD
    pad_rear = CABLE_Z - t / 2 - t
    out.append(box(-x1, y0, -x0, y1, pad_rear, CABLE_Z - t / 2, FLEX))
    for shapes, xr, y, turn in TOUCH_PARTS:
        out += place(place(shapes, xr, y, turn=turn), z=pad_rear, flip=True)
    return out


def rear():
    """Every part on the board's rear, in the model frame."""
    out = []
    for kind, xr, y, turn in CHIPS:
        out += place(getattr(p, kind)(), xr, y, turn=turn)
    for _name, shapes, xr, y, turn in PARTS:
        out += place(shapes, xr, y, turn=turn)
    return place(out, z=-BOARD_REAR, flip=True)


def board(_colors=None):
    """The whole board.

    Returns:
        The solids, as step_gen.write takes them.
    """
    return items(front() + standoffs() + cables() + rear())


def parts(colors):
    """Every model file of the board.

    Args:
        colors: Colour name to RGB; not used, the spec has its own.

    Returns:
        A dict of file stem to solids.
    """
    return {"rp2350_touch_lcd_2_8": board(colors)}
