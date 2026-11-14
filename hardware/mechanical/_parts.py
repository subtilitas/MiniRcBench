"""Electronic parts as shapes for _solid.py.

Every function returns shapes in the part's own frame: the origin is
the centre of the part's footprint on the board, z = 0 is the board's
surface and +z points away from it. All sizes are mm. Package sizes are
the packages' nominal ones, not measured on a board.
"""

from _solid import Y, box, circle, ext, place, rect, rounded, stadium

TIN = (0.80, 0.80, 0.78)
STEEL = (0.72, 0.72, 0.72)
GOLD = (0.83, 0.65, 0.22)
BLACK = (0.07, 0.07, 0.07)
EPOXY = (0.13, 0.13, 0.14)
TAN = (0.70, 0.58, 0.42)
CREAM = (0.92, 0.89, 0.80)
WHITE = (0.95, 0.95, 0.93)
GREY = (0.45, 0.45, 0.45)
LENS = (0.95, 0.90, 0.60)


def rows(n, pitch):
    """n positions at ``pitch``, centred on 0."""
    return [(i - (n - 1) / 2) * pitch for i in range(n)]


def chip(length, width, height, rgb):
    """A chip part with a tinned end cap at each end of its length (x)."""
    cap, x, y = 0.22 * length, length / 2, width / 2
    return [box(-x, -y, -x + cap, y, 0.0, height, TIN),
            box(-x + cap, -y, x - cap, y, 0.0, height, rgb),
            box(x - cap, -y, x, y, 0.0, height, TIN)]


def r0402():
    return chip(1.0, 0.5, 0.35, EPOXY)


def c0402():
    return chip(1.0, 0.5, 0.5, TAN)


def r0603():
    return chip(1.6, 0.8, 0.45, EPOXY)


def c0603():
    return chip(1.6, 0.8, 0.8, TAN)


def c0805():
    return chip(2.0, 1.25, 0.85, TAN)


def led0603():
    return chip(1.6, 0.8, 0.6, LENS)


def lead(inner, tip, top, width, thick=0.15):
    """A gull-wing lead on the +x side at y = 0.

    Args:
        inner: x where the lead leaves the body.
        tip: x of the foot's end.
        top: Height of the lead's top where it leaves the body.
        width: The lead's width along y.
        thick: The lead's thickness.
    """
    knee = inner + 0.3 * (tip - inner)
    profile = [(inner, top - thick), (knee, top - thick), (knee, 0.0),
               (tip, 0.0), (tip, thick), (knee + thick, thick),
               (knee + thick, top), (inner, top)]
    return ext(profile, -width / 2, width / 2, TIN, frame=Y)


def leaded(body_x, body_y, height, left, right, span, lead_width):
    """A moulded package with gull-wing leads on its -x and +x sides.

    Args:
        body_x, body_y: The body's size.
        height: The body's top above the board; the body stands 0.1 off.
        left, right: The leads' y positions on the -x and the +x side.
        span: Tip to tip across x.
        lead_width: Each lead's width.
    """
    out = [box(-body_x / 2, -body_y / 2, body_x / 2, body_y / 2, 0.1,
               height, EPOXY)]
    one = [lead(body_x / 2, span / 2, 0.55 * height, lead_width)]
    for y in right:
        out += place(one, y=y)
    for y in left:
        out += place(one, y=y, turn=180.0)
    return out


def sot23(left, right):
    """SOT-23 family: 1.6 x 2.9 body, leads 0.95 apart, 2.8 span."""
    return leaded(1.6, 2.9, 1.1, rows(left, 1.9 if left == 2 else 0.95),
                  rows(right, 1.9 if right == 2 else 0.95), 2.8, 0.4)


def sod(body, width, height, span, lead_width):
    """A diode with one lead at each end of its length (x)."""
    return leaded(body, width, height, [0.0], [0.0], span, lead_width)


def qfn(size_x, size_y, height, n_x, n_y, pitch, rgb=EPOXY):
    """A leadless package; each pad shows as a 0.12 solder fillet.

    Args:
        size_x, size_y, height: The body.
        n_x: Pads on each of the two sides along x (at -y and +y).
        n_y: Pads on each of the two sides along y (at -x and +x).
        pitch: Pad pitch.
        rgb: Body colour.
    """
    x, y, pad, out = size_x / 2, size_y / 2, 0.5 * pitch, []
    out.append(box(-x, -y, x, y, 0.0, height, rgb))
    for p in rows(n_y, pitch):
        out.append(box(x, p - pad / 2, x + 0.12, p + pad / 2, 0.0, 0.25, TIN))
        out.append(box(-x - 0.12, p - pad / 2, -x, p + pad / 2, 0.0, 0.25,
                       TIN))
    for p in rows(n_x, pitch):
        out.append(box(p - pad / 2, y, p + pad / 2, y + 0.12, 0.0, 0.25, TIN))
        out.append(box(p - pad / 2, -y - 0.12, p + pad / 2, -y, 0.0, 0.25,
                       TIN))
    return out


def can(length, width, height, rgb):
    """A crystal: ceramic base 0.2 high and a lid 0.15 smaller all round."""
    x, y = length / 2, width / 2
    return [box(-x, -y, x, y, 0.0, 0.2, TAN),
            ext(rounded(-x + 0.15, -y + 0.15, x - 0.15, y - 0.15, 0.15),
                0.2, height, rgb)]


def block(length, width, height, rgb, radius=0.3):
    """A moulded block with rounded corners, an inductor."""
    return [ext(rounded(-length / 2, -width / 2, length / 2, width / 2,
                        radius), 0.0, height, rgb)]


def socket(pins, pitch, width, depth, height, lead_len, rear, wing=0.0,
           wing_height=0.0, wing_depth=0.0, rgb=CREAM):
    """A side-entry wire-to-board socket; the plug enters from -y.

    The origin is the centre of the socket's box, leads included.

    Args:
        pins: Number of pins.
        pitch: Pin pitch.
        width: Overall width along x, wings included.
        depth: Overall depth along y, leads included.
        height: Height of the housing.
        lead_len: Length of the solder leads behind the housing.
        rear: Depth of the housing's closed rear part.
        wing: Width of the lower wing at each side, 0 for none.
        wing_height, wing_depth: The wings' height and depth; a wing
            ends ``rear`` before the housing's rear.
        rgb: Housing colour.
    """
    front, back = -depth / 2, depth / 2 - lead_len
    x = width / 2 - wing
    wall, floor, roof = 0.6, 0.5, 0.6
    out = [
        ext(rect(-x, 0.0, x, height), -(back - rear), -front, rgb,
            holes=[rect(-x + wall, floor, x - wall, height - roof)], frame=Y),
        box(-x, back - rear, x, back, 0.0, height - 0.44, rgb),
    ]
    if wing:
        for side in (-1, 1):
            a, b = sorted((side * x, side * width / 2))
            out.append(box(a, back - rear - wing_depth, b, back - rear, 0.0,
                           wing_height, rgb))
    mid = floor + (height - roof - floor) / 2
    for p in rows(pins, pitch):
        out.append(box(p - 0.15, front + 0.9, p + 0.15, back - rear,
                       mid - 0.15, mid + 0.15, TIN))
        out.append(box(p - 0.15, back, p + 0.15, depth / 2, 0.0, 0.25, TIN))
    return out


def usb_c():
    """A USB Type-C receptacle, 16 pins, the plug enters from -y.

    The origin is the centre of the shell's rear end on the board. The
    shell is 8.94 wide and 3.16 high, 0.09 above the board, 7.35 long.
    """
    w, z0, z1, length, cap = 4.47, 0.09, 3.25, 7.35, 0.97
    out = [
        ext(rounded(-w, z0, w, z1, 1.28), cap, length, STEEL,
            holes=[rounded(-w + 0.3, z0 + 0.3, w - 0.3, z1 - 0.3, 0.98)],
            frame=Y),
        ext(rounded(-w, z0, w, z1, 1.28), 0.0, cap, STEEL, frame=Y),
        box(-3.35, -length + 0.42, 3.35, -cap, 1.36, 1.98, BLACK),
    ]
    for side in (-1, 1):
        a, b = sorted((side * w, side * (w + 0.32)))
        out.append(box(a, -1.42, b, 0.16, 0.0, 1.6, STEEL))
        out.append(box(a, -5.17, b, -4.37, 0.0, 1.6, STEEL))
    for p in rows(12, 0.5):
        out.append(box(p - 0.14, 0.0, p + 0.14, 0.93, 0.0, 0.2, TIN))
    return out


def side_button():
    """A side-push button; the plunger points to -x.

    The origin is the centre of the body's footprint: 2.3 x 4.55, 1.8
    high. The plunger is 1.1 long, 1.6 wide and 1.0 high.
    """
    return [
        box(-1.15, -2.275, 1.15, 2.275, 0.0, 1.8, STEEL),
        box(-2.25, -0.8, -1.15, 0.8, 0.4, 1.4, GREY),
        box(1.15, 1.4, 1.83, 2.275, 0.0, 0.3, TIN),
        box(1.15, -2.275, 1.83, -1.4, 0.0, 0.3, TIN),
    ]


def trimmer(size, height, rotor):
    """A single-turn trimmer: square body, round rotor with a slot mark.

    The two end terminals are at -y, the wiper terminal at +y.
    """
    s = size / 2
    out = [
        box(-s, -s, s, s, 0.0, height - 0.25, CREAM),
        ext(circle(0.0, 0.0, rotor / 2), height - 0.25, height, WHITE),
        box(-1.75, -s - 0.5, -0.85, -s, 0.0, 0.3, TIN),
        box(0.85, -s - 0.5, 1.75, -s, 0.0, 0.3, TIN),
        box(-0.45, s, 0.45, s + 0.45, 0.0, 0.3, TIN),
    ]
    out += place([box(-0.4 * rotor, -0.2, 0.4 * rotor, 0.2, height,
                      height + 0.1, GREY)], turn=45.0)
    return out


def flat_cable_socket(width, body, latch, ear, ear_len, height):
    """A flat-cable socket: the cable enters from -y, the latch is at +y.

    The origin is the centre of the housing's front edge on the board.

    Args:
        width: Overall width along x.
        body: Depth of the housing.
        latch: Depth of the latch behind the housing.
        ear: Width of the housing's ear at each side of the latch.
        ear_len: Length of the ears.
        height: Height of the housing; the latch is 0.5 lower.
    """
    x = width / 2
    return [
        box(-x, 0.0, x, body, 0.0, height, WHITE),
        box(-x, body, -x + ear, body + ear_len, 0.0, height, WHITE),
        box(x - ear, body, x, body + ear_len, 0.0, height, WHITE),
        box(-x + ear, body, x - ear, body + latch, 0.0, height - 0.5, BLACK),
    ]


def micro_sd():
    """A push-push microSD card slot; the card enters from -x.

    The origin is the centre of the slot's footprint: 11.25 x 11.35,
    1.32 high. The shield's top has a finger notch at the card's end
    and 8 openings over the contacts, 1.1 apart.
    """
    x, y, top, sheet = 5.625, 5.675, 1.32, 0.15
    inner = y - sheet
    lid = [(-x, -inner), (x, -inner), (x, inner), (-x, inner),
           (-x, 4.235), (-4.215, 4.235), (-4.215, -3.565), (-x, -3.565)]
    contacts = [4.835 - 1.1 * i for i in range(8)]
    out = [
        ext(lid, top - sheet, top, STEEL,
            holes=[stadium(1.535, c - 0.3, 2.835, c + 0.3)
                   for c in contacts]),
        box(-x, -y, x, -inner, 0.0, top, STEEL),
        box(-x, inner, x, y, 0.0, top, STEEL),
        box(x - sheet, -inner, x, inner, 0.0, top - sheet, STEEL),
        box(-3.825, -inner, x - sheet, inner, 0.0, 0.34, BLACK),
    ]
    for c in contacts:
        out.append(box(1.4, c - 0.25, 3.0, c + 0.25, 0.34, 0.6, GOLD))
    return out
