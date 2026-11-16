#!/usr/bin/env python3
"""The arithmetic and the mock-up patch behind docs/design/ui-theme.md.

    python3 docs/design/ui_theme.py             # the note's tables
    python3 docs/design/ui_theme.py --patch DIR [--radius]
    python3 docs/design/ui_theme.py --sheet OUT.png DIR SHOT...
    python3 docs/design/ui_theme.py --cvd OUT.png DIR SHOT...

The tables: rcbench's palette packed as rgb() in src/theme.h packs it, the
8-bit colour each RGB565 word stands for, WCAG 2 contrast ratios, and the
CIEDE2000 distance between role colours as seen with normal vision and
through the Machado, Oliveira and Fernandes (2009) simulation of protanopia,
deuteranopia and tritanopia at severity 1.0.

The last tables are the cues of the owner's decisions of 2026-11-15: cyan as
the accent with "changed" as a fill, and green as a start button's fill.

--patch writes the proposed palette and widget changes into DIR, a scratch
copy of the tree at cff09ae (`git archive cff09ae | tar -x -C DIR`), never
the working tree: `make -C DIR/test -s` then renders the mock-ups as
shot_*.ppm. The palette and the neutral fills of the patch are in src/ as
slices 7 and 8 of the build plan; "today" in the tables is cff09ae.
--sheet and --cvd tile such shots into one PNG and need Pillow.
"""

import itertools
import math
import os
import re
import sys

# rcbench shared/ui/ui_theme.c at origin/main a2d10ccf, 8 bits a channel.
RCB_DARK = dict(
    BG=(2, 5, 9), PANEL=(14, 22, 33), PANEL_HI=(26, 40, 58),
    PANEL_SUNK=(5, 9, 15), EDGE=(58, 116, 156), EDGE_HI=(112, 190, 236),
    GRID=(34, 66, 90), GRID_STRONG=(70, 126, 164), TEXT=(232, 248, 255),
    TEXT_DIM=(132, 172, 202), TEXT_FAINT=(72, 106, 134),
    TEXT_ON_LIGHT=(4, 8, 14), ACCENT=(0, 240, 255), OK=(0, 255, 148),
    WARN=(255, 186, 0), DANGER=(255, 40, 72), VOLT=(255, 190, 26),
    CURR=(255, 36, 112), POWER=(0, 208, 255), RPM=(132, 255, 56),
    LILAC=(192, 138, 255), TEAL=(34, 244, 202), BEVEL_LIT=(74, 106, 140),
    BEVEL_DARK=(2, 5, 9))
RCB_LIGHT = dict(
    BG=(234, 239, 245), PANEL=(252, 253, 255), PANEL_HI=(228, 236, 246),
    PANEL_SUNK=(216, 225, 236), EDGE=(170, 186, 204),
    EDGE_HI=(104, 126, 150), GRID=(204, 215, 228),
    GRID_STRONG=(162, 180, 200), TEXT=(10, 18, 28), TEXT_DIM=(70, 88, 108),
    TEXT_FAINT=(120, 138, 158), TEXT_ON_LIGHT=(4, 8, 14),
    ACCENT=(0, 196, 232), OK=(0, 198, 128), WARN=(246, 168, 0),
    DANGER=(232, 48, 48), VOLT=(196, 122, 0), CURR=(208, 24, 78),
    POWER=(0, 132, 186), RPM=(46, 148, 22), LILAC=(118, 80, 200),
    TEAL=(0, 150, 130), BEVEL_LIT=(255, 255, 255),
    BEVEL_DARK=(150, 166, 186))

# The order of ThemeColor in src/theme.h.
ORDER = ("BG PANEL GRID DIM TEXT INK PAPER ONACCENT GREEN LIME AMBER RED "
         "REDDARK GHOST BLUE CYAN MAGENTA").split()

# src/theme.cpp at cff09ae.
NOW_DARK = dict(zip(ORDER, (
    0x0000, 0x18E3, 0x39E7, 0x8410, 0xE71C, 0xFFFF, 0x0000, 0xFFFF, 0x2665,
    0x07E0, 0xFD20, 0xF9E7, 0x6000, 0x1082, 0x2D7F, 0x07FF, 0xF81F)))
HC8 = dict(zip(ORDER, (
    (255, 255, 255), (206, 212, 220), (56, 62, 72), (68, 74, 84), (0, 0, 0),
    (0, 0, 0), (255, 255, 255), (255, 255, 255), (0, 110, 52), (0, 122, 40),
    (174, 88, 0), (190, 0, 24), (244, 190, 190), (238, 241, 245),
    (0, 58, 168), (0, 88, 122), (150, 0, 140))))

# The proposed dark palette: the entry, its 8-bit value, where it is from.
_D = RCB_DARK
BENCH = dict(
    BG=(_D["BG"], "UI_C_BG"),
    PANEL=(_D["PANEL_HI"], "UI_C_PANEL_HI"),
    GRID=(_D["TEXT_FAINT"], "UI_C_TEXT_FAINT"),
    DIM=(_D["TEXT_DIM"], "UI_C_TEXT_DIM"),
    TEXT=(_D["TEXT"], "UI_C_TEXT"),
    INK=((255, 255, 255), "GFX_WHITE"),
    PAPER=(_D["BG"], "UI_C_BG"),
    ONACCENT=(_D["TEXT_ON_LIGHT"], "UI_C_TEXT_ON_LIGHT"),
    GREEN=(_D["OK"], "UI_C_OK"),
    LIME=(_D["RPM"], "UI_C_RPM"),
    AMBER=(_D["WARN"], "UI_C_WARN"),
    RED=(_D["DANGER"], "UI_C_DANGER"),
    REDDARK=((96, 16, 32), "own"),
    GHOST=(_D["PANEL"], "UI_C_PANEL"),
    BLUE=((72, 136, 255), "own"),
    CYAN=(_D["ACCENT"], "UI_C_ACCENT"),
    MAGENTA=((255, 0, 255), "own"))


def pack(r, g, b):
    """RGB565 as rgb() in src/theme.h and GFX_RGB() in rcbench's gfx.h."""
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def unpack(w):
    """The 8-bit colour an RGB565 word stands for, by bit replication."""
    r5, g6, b5 = (w >> 11) & 31, (w >> 5) & 63, w & 31
    return ((r5 << 3) | (r5 >> 2), (g6 << 2) | (g6 >> 4),
            (b5 << 3) | (b5 >> 2))


def lin(c):
    c /= 255.0
    return c / 12.92 if c <= 0.04045 else ((c + 0.055) / 1.055) ** 2.4


def gam(v):
    v = min(1.0, max(0.0, v))
    c = 12.92 * v if v <= 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055
    return int(round(c * 255))


def lum(rgb):
    """WCAG 2 relative luminance of an sRGB colour."""
    r, g, b = (lin(c) for c in rgb)
    return 0.2126 * r + 0.7152 * g + 0.0722 * b


def contrast(a, b):
    """WCAG 2 contrast ratio, 1 to 21."""
    la, lb = lum(a), lum(b)
    if la < lb:
        la, lb = lb, la
    return (la + 0.05) / (lb + 0.05)


# Machado, Oliveira, Fernandes 2009, severity 1.0, on linear RGB.
CVD = {
    "protan": ((0.152286, 1.052583, -0.204868),
               (0.114503, 0.786281, 0.099216),
               (-0.003882, -0.048116, 1.051998)),
    "deutan": ((0.367322, 0.860646, -0.227968),
               (0.280085, 0.672501, 0.047413),
               (-0.011820, 0.042940, 0.968881)),
    "tritan": ((1.255528, -0.076749, -0.178779),
               (-0.078411, 0.930809, 0.147602),
               (0.004733, 0.691367, 0.303900)),
}
VIEWS = ("normal", "protan", "deutan", "tritan")


def simulate(rgb, view):
    if view == "normal":
        return rgb
    v = [lin(c) for c in rgb]
    return tuple(gam(sum(m * x for m, x in zip(row, v)))
                 for row in CVD[view])


def lab(rgb):
    r, g, b = (lin(c) for c in rgb)
    x = (0.4124564 * r + 0.3575761 * g + 0.1804375 * b) / 0.95047
    y = 0.2126729 * r + 0.7151522 * g + 0.0721750 * b
    z = (0.0193339 * r + 0.1191920 * g + 0.9503041 * b) / 1.08883

    def f(t):
        if t > 216 / 24389:
            return t ** (1 / 3)
        return (24389 / 27 * t + 16) / 116
    fx, fy, fz = f(x), f(y), f(z)
    return (116 * fy - 16, 500 * (fx - fy), 200 * (fy - fz))


def de2000_lab(p, q):
    L1, a1, b1 = p
    L2, a2, b2 = q
    cb = (math.hypot(a1, b1) + math.hypot(a2, b2)) / 2
    g = 0.5 * (1 - math.sqrt(cb ** 7 / (cb ** 7 + 25 ** 7)))
    a1p, a2p = (1 + g) * a1, (1 + g) * a2
    c1p, c2p = math.hypot(a1p, b1), math.hypot(a2p, b2)

    def hue(b, a):
        if a == 0 and b == 0:
            return 0.0
        return math.degrees(math.atan2(b, a)) % 360
    h1p, h2p = hue(b1, a1p), hue(b2, a2p)
    dl, dc = L2 - L1, c2p - c1p
    dh = 0.0
    if c1p * c2p != 0:
        dh = h2p - h1p
        if dh > 180:
            dh -= 360
        elif dh < -180:
            dh += 360
    dhh = 2 * math.sqrt(c1p * c2p) * math.sin(math.radians(dh / 2))
    lbp, cbp = (L1 + L2) / 2, (c1p + c2p) / 2
    if c1p * c2p == 0:
        hbp = h1p + h2p
    elif abs(h1p - h2p) <= 180:
        hbp = (h1p + h2p) / 2
    elif h1p + h2p < 360:
        hbp = (h1p + h2p + 360) / 2
    else:
        hbp = (h1p + h2p - 360) / 2
    t = (1 - 0.17 * math.cos(math.radians(hbp - 30))
         + 0.24 * math.cos(math.radians(2 * hbp))
         + 0.32 * math.cos(math.radians(3 * hbp + 6))
         - 0.20 * math.cos(math.radians(4 * hbp - 63)))
    dth = 30 * math.exp(-((hbp - 275) / 25) ** 2)
    rc = 2 * math.sqrt(cbp ** 7 / (cbp ** 7 + 25 ** 7))
    sl = 1 + 0.015 * (lbp - 50) ** 2 / math.sqrt(20 + (lbp - 50) ** 2)
    sc, sh = 1 + 0.045 * cbp, 1 + 0.015 * cbp * t
    rt = -math.sin(math.radians(2 * dth)) * rc
    return math.sqrt((dl / sl) ** 2 + (dc / sc) ** 2 + (dhh / sh) ** 2
                     + rt * (dc / sc) * (dhh / sh))


def de2000(c1, c2):
    """CIEDE2000 between two sRGB colours."""
    return de2000_lab(lab(c1), lab(c2))


# Sharma, Wu and Dalal's test pair 1, and the two ends of the contrast scale.
assert abs(de2000_lab((50.0, 2.6772, -79.7751), (50.0, 0.0, -82.7485))
           - 2.0425) < 1e-3
assert abs(contrast((255, 255, 255), (0, 0, 0)) - 21.0) < 1e-9


# The cues the owner's decisions of 2026-11-15 rest on. A fill against what
# is around it, and the ink on it, as luminance contrast in each view.
CUES = (
    ("CYAN", "BG", "changed: the fill against the row"),
    ("CYAN", "PANEL", "changed: the fill against the selected row"),
    ("ONACCENT", "CYAN", "changed: the ink on the fill; BACK's label"),
    ("CYAN", "BG", "unchanged: a settings value, no fill"),
    ("GREEN", "BG", "a start button's fill against the screen"),
    ("ONACCENT", "GREEN", "a start button's label"),
    ("GREEN", "BG", "SAFE: the outlined badge's word and frame"),
    ("RED", "PANEL", "ARMED: the badge's fill against the status bar"),
    ("ONACCENT", "RED", "ARMED, DISARM, STOP"),
)
# Two fills or marks that must not be taken for each other.
APART = (
    ("GREEN", "RED", "HOLD TO ARM against DISARM, in one rectangle"),
    ("GREEN", "CYAN", "a start button against BACK"),
    ("GREEN", "LIME", "a start button against its hold bar"),
    ("CYAN", "TEXT", "a settings value against a device value"),
    ("CYAN", "DIM", "a settings value against its label"),
)


def table_decisions():
    _, bench, hc = palettes()
    for name, pal in (("proposed", bench), ("high contrast", hc)):
        u = shown(pal)
        print(f"\n{name}: cue | " + " | ".join(VIEWS))
        for fg, bg, use in CUES:
            print(f"{fg} on {bg}, {use} | " + " | ".join(
                f"{contrast(simulate(u[fg], v), simulate(u[bg], v)):.2f}"
                for v in VIEWS))
        print(f"\n{name}: apart, CIEDE2000 | " + " | ".join(VIEWS))
        for a, b, use in APART:
            print(f"{a} {b}, {use} | " + " | ".join(
                f"{de2000(simulate(u[a], v), simulate(u[b], v)):.1f}"
                for v in VIEWS))


def shown(pal):
    return {k: unpack(v) for k, v in pal.items()}


def palettes():
    bench = {k: pack(*v[0]) for k, v in BENCH.items()}
    hc = {k: pack(*v) for k, v in HC8.items()}
    return NOW_DARK, bench, hc


def table_rcbench():
    print("rcbench role | dark 8 bit | RGB565 | shown as | light 8 bit "
          "| RGB565")
    for k in RCB_DARK:
        d, li = RCB_DARK[k], RCB_LIGHT[k]
        print(f"UI_C_{k} | {d} | 0x{pack(*d):04X} | {unpack(pack(*d))} "
              f"| {li} | 0x{pack(*li):04X}")


def table_mapping():
    now, bench, _ = palettes()
    print("\nentry | today | proposed 8 bit | RGB565 | shown as | from")
    for k in ORDER:
        v, src = BENCH[k]
        print(f"TC_{k} | 0x{now[k]:04X} {unpack(now[k])} | {v} "
              f"| 0x{bench[k]:04X} | {unpack(bench[k])} | {src}")


# Foreground, background, and what the pair is.
PAIRS = (
    ("TEXT", "BG", "values, titles of text"),
    ("DIM", "BG", "labels, captions, the neutral chip"),
    ("GRID", "BG", "disabled labels, group captions, hints; frames"),
    ("GREEN", "BG", "SAVED, WRITE VERIFIED, SAFE and the outlined chips"),
    ("LIME", "BG", "RPM digits, readouts"),
    ("AMBER", "BG", "warnings, the current step"),
    ("RED", "BG", "failures"),
    ("CYAN", "BG", "a settings value, a section caption, >"),
    ("BLUE", "BG", "servo B's label and horn"),
    ("MAGENTA", "BG", "STALLED, the alert lines"),
    ("TEXT", "PANEL", "button labels, header title, selected row"),
    ("DIM", "PANEL", "LATCH and MENU labels, tile captions"),
    ("AMBER", "PANEL", "REVERT and DEFAULTS labels"),
    ("CYAN", "PANEL", "a settings value in the selected row, a tab"),
    ("LIME", "PANEL", "ESC STATUS OK in its tile"),
    ("RED", "PANEL", "ESC STATUS in its tile"),
    ("MAGENTA", "PANEL", "ESC STATUS alert in its tile"),
    ("ONACCENT", "GREEN", "HOLD TO ARM, WRITE, START, APPLY at rest"),
    ("ONACCENT", "CYAN", "BACK, the tester's MENU, a changed value"),
    ("ONACCENT", "LIME", "servo A's selector, PORT OPEN chip"),
    ("ONACCENT", "RED", "ARMED, DISARM, STOP, a held hold"),
    ("ONACCENT", "BLUE", "LATCH on, a toggle on, PROG MODE"),
    ("ONACCENT", "MAGENTA", "the OVERCURRENT and STALLED chips"),
    ("PAPER", "AMBER", "EJECT, the SDERR chip"),
    ("PAPER", "INK", "the AB selector"),
    ("PANEL", "BG", "not text: a fill against the screen"),
    ("GRID", "PANEL", "not text: BACK's frame in the header"),
    ("REDDARK", "BG", "not text: dead RPM digits"),
    ("GHOST", "BG", "not text: unlit segments"),
)


def table_contrast():
    now, bench, hc = palettes()
    n, b, h = shown(now), shown(bench), shown(hc)
    print("\ntext | on | today | proposed | high contrast | use")
    for fg, bg, use in PAIRS:
        print(f"{fg} | {bg} | {contrast(n[fg], n[bg]):.2f} "
              f"| {contrast(b[fg], b[bg]):.2f} "
              f"| {contrast(h[fg], h[bg]):.2f} | {use}")


ROLES = "GREEN LIME AMBER RED BLUE CYAN MAGENTA TEXT DIM GRID".split()


def distances(pal):
    u = shown(pal)
    out = {}
    for a, b in itertools.combinations(ROLES, 2):
        out[(a, b)] = [de2000(simulate(u[a], v), simulate(u[b], v))
                       for v in VIEWS]
    return out


def table_cvd():
    now, bench, hc = palettes()
    for name, pal in (("today", now), ("proposed", bench),
                      ("high contrast", hc)):
        print(f"\n{name}: pairs under 10 in any view "
              "| normal | protan | deutan | tritan")
        for (a, b), d in distances(pal).items():
            if min(d) < 10:
                print(f"{a} {b} | " + " | ".join(f"{x:.1f}" for x in d))


def table_light():
    print("\nrcbench light role as text on white | white on it")
    w = (255, 255, 255)
    for k in ("ACCENT", "OK", "WARN", "DANGER", "VOLT", "CURR", "POWER",
              "RPM", "LILAC", "TEAL"):
        c = unpack(pack(*RCB_LIGHT[k]))
        print(f"{k} | {contrast(c, w):.2f}")


def table_alternatives():
    _, bench, _ = palettes()
    u = shown(bench)
    w = (255, 255, 255)
    print("\nalternatives")
    for v in VIEWS:
        red = simulate(u["RED"], v)
        ink = simulate(u["ONACCENT"], v)
        print(f"on RED, {v}: ONACCENT {contrast(ink, red):.2f}, "
              f"white {contrast(w, red):.2f}")
    for name, c in (("LILAC", RCB_DARK["LILAC"]), ("POWER", RCB_DARK["POWER"]),
                    ("today's blue", unpack(0x2D7F)),
                    ("proposed blue", BENCH["BLUE"][0])):
        c = unpack(pack(*c))
        row = []
        for other in ("CYAN", "MAGENTA", "DIM"):
            row.append(other + " " + "/".join(
                f"{de2000(simulate(c, v), simulate(u[other], v)):.1f}"
                for v in VIEWS))
        print(f"{name} {c}: on BG {contrast(c, u['BG']):.2f}; "
              + "; ".join(row))
    for name, c in (("UI_C_PANEL", RCB_DARK["PANEL"]),
                    ("UI_C_PANEL_HI", RCB_DARK["PANEL_HI"])):
        c = unpack(pack(*c))
        print(f"{name} {c}: against BG {contrast(c, u['BG']):.2f}, "
              f"RED on it {contrast(u['RED'], c):.2f}")
    for name, c in (("UI_C_EDGE", RCB_DARK["EDGE"]),
                    ("UI_C_TEXT_FAINT", RCB_DARK["TEXT_FAINT"])):
        c = unpack(pack(*c))
        print(f"{name} {c}: on BG {contrast(c, u['BG']):.2f}")


# ---------------------------------------------------------------- mock-ups

def _sub(path, old, new, count=1):
    s = open(path).read()
    assert s.count(old) == count, (path, old, s.count(old))
    open(path, "w").write(s.replace(old, new))


def _resub(path, pattern, repl):
    s = open(path).read()
    t = re.sub(pattern, repl, s)
    assert s != t, (path, pattern)
    open(path, "w").write(t)


def patch(root, radius):
    """Write the proposal into a scratch copy of the tree at @p root."""
    assert not os.path.exists(os.path.join(root, ".git")), \
        "patch a scratch copy, not the working tree"
    src = os.path.join(root, "src")
    # The palette.
    p = os.path.join(src, "theme.cpp")
    s = open(p).read()
    a = s.index("static const uint16_t THEME_DARK[TC_COUNT] = {")
    b = s.index("};", a)
    rows = "".join("\trgb(%3d, %3d, %3d), /* TC_%s */\n" % (*BENCH[k][0], k)
                   for k in ORDER)
    open(p, "w").write(
        s[:a] + "static const uint16_t THEME_DARK[TC_COUNT] = {\n" + rows
        + s[b:])
    w = os.path.join(src, "ui_widgets.cpp")
    # The header's rule.
    _sub(w, "\tgfxRect(0, 0, GFX_W, UI_HDR_H, C_PANEL);\n",
         "\tgfxRect(0, 0, GFX_W, UI_HDR_H, C_PANEL);\n"
         "\tgfxHLine(0, UI_HDR_H - 1, GFX_W, C_GRID);\n")
    # The neutral chip: a frame and a dim label.
    _sub(w, "\tgfxRect(x, y, w, UI_CHIP_H, fill);\n",
         "\tif (fill == C_GRID) {\n"
         "\t\tgfxRect(x, y, w, UI_CHIP_H, C_GRID);\n"
         "\t\tgfxRect(x + 1, y + 1, w - 2, UI_CHIP_H - 2, C_BG);\n"
         "\t\tgfxText(x + 6, y + (UI_CHIP_H - 7) / 2, text, C_DIM, 1);\n"
         "\t\treturn;\n\t}\n"
         "\tgfxRect(x, y, w, UI_CHIP_H, fill);\n")
    # Edit buttons are neutral.
    for f in ("ui_servo.cpp", "ui_kst.cpp", "ui_am32.cpp", "ui_sbus_out.cpp",
              "ui_hitec.cpp"):
        _resub(os.path.join(src, f),
               r'(uiButton\(BTN_(?:MINUS|PLUS|REPAIR),\s*"[^"]*",)'
               r' C_GRID, C_ONACCENT', r'\1 C_PANEL, C_TEXT')
    # A slider's fill is the faint neutral.
    for f in ("ui_servo.cpp", "ui_sbus_out.cpp"):
        _resub(os.path.join(src, f), r"uiSliderDraw\((&\w+), C_DIM\)",
               r"uiSliderDraw(\1, C_GRID)")
    decisions(src)
    if radius:
        # The corner solver of STATUS.md, open item 25: inset 2, 1, 0 at r 5.
        old = ("\t\tint dx = r - 1;\n\t\twhile (dx > 0) {\n"
               "\t\t\tint a = r - 1 - dx, b = r - 1 - dy;\n"
               "\t\t\tif (a * a + b * b <= (r - 1) * (r - 1)) break;\n"
               "\t\t\tdx--;\n\t\t}\n\t\tint inset = r - 1 - dx;\n")
        new = ("\t\tint inset = 0;\n\t\tfor (; inset < r; inset++) {\n"
               "\t\t\tint a = r - 1 - inset, b = r - 1 - dy;\n"
               "\t\t\tif (a * a + b * b <= r * r - r) break;\n\t\t}\n")
        _sub(os.path.join(src, "gfx.cpp"), old, new, count=2)


def decisions(src):
    """The owner's decisions of 2026-11-15 on cyan, green and the chips."""
    w = os.path.join(src, "ui_widgets.cpp")
    ui = os.path.join(src, "ui.cpp")
    # Cyan: BACK and the tester's MENU are the home tag.
    _sub(w, 'uiButton(uiBackRect(), "BACK", C_PANEL, C_TEXT, 1,',
         'uiButton(uiBackRect(), "BACK", C_CYAN, C_ONACCENT, 1,')
    _sub(ui, 'uiButton(BTN_MENU, "MENU", C_PANEL, C_DIM, 1,',
         'uiButton(BTN_MENU, "MENU", C_CYAN, C_ONACCENT, 1,')
    # Cyan: the settings lists' values and SETTINGS' section captions.
    st = os.path.join(src, "ui_settings.cpp")
    _sub(st, "it.sec->caption, C_TEXT, 1);", "it.sec->caption, C_CYAN, 1);")
    _sub(st, "\tr.value(v, sizeof(v), &col);\n",
         "\tr.value(v, sizeof(v), &col);\n"
         "\tif (col == C_TEXT) col = C_CYAN;\n")
    # A field list of settings passes its plain values as cyan: the mock-up
    # marks SERVO SET and OUTPUT SET by their row height's name.
    ls = os.path.join(src, "ui_list.cpp")
    _sub(ls, "\tuint16_t col = uiListValueColour(f.state);\n",
         "\tuint16_t col = uiListValueColour(f.state);\n"
         "\tif (f.state == UiValue::Plain && g_mockSetting) col = C_CYAN;\n")
    _sub(ls, "void uiListDrawField(int y, int h, const UiField &f) {",
         "bool g_mockSetting = false;\n"
         "void uiListDrawField(int y, int h, const UiField &f) {")
    for f in ("ui_servo.cpp", "ui_sbus_out.cpp"):
        _sub(os.path.join(src, f), "\tuiListDrawField(y, SET_ROW_H, f);\n",
             "\textern bool g_mockSetting;\n\tg_mockSetting = true;\n"
             "\tuiListDrawField(y, SET_ROW_H, f);\n"
             "\tg_mockSetting = false;\n")
    # Changed: dark ink on a cyan fill, and the bar as built.
    _sub(ls, "\tgfxText(GFX_W - UI_MARGIN - gfxTextW(f.value, sc), "
         "y + (fillH - 7 * sc + 1) / 2, f.value, col, sc);\n",
         "\tint vw = gfxTextW(f.value, sc), "
         "vy = y + (fillH - 7 * sc + 1) / 2;\n"
         "\tif (f.state == UiValue::Changed) {\n"
         "\t\tgfxRect(GFX_W - UI_MARGIN - vw - 3, vy - 2, vw + 6, "
         "7 * sc + 4, C_CYAN);\n"
         "\t\tcol = C_ONACCENT;\n\t}\n"
         "\tgfxText(GFX_W - UI_MARGIN - vw, vy, f.value, col, sc);\n")
    # Green: a button that starts an output, at rest.
    hold = r"(\w+(?:\.holding)?) \? (?:C_RED|UI_COL_ON) : C_PANEL,(\s*)" \
           r"\1 \? C_ONACCENT : C_TEXT"
    for f, n in (("ui_am32.cpp", 1), ("ui_hitec.cpp", 1), ("ui_kst.cpp", 2),
                 ("ui_sbus_out.cpp", 1), ("ui_servo.cpp", 2)):
        path = os.path.join(src, f)
        s = open(path).read()
        if f == "ui_kst.cpp":
            # HOLD TO REBOOT starts no output and stays neutral.
            a = s.index('"HOLD TO REBOOT"')
            head, tail = s[:a], s[a:]
            head, k = re.subn(hold, r"\1 ? C_RED : C_GREEN,\2C_ONACCENT",
                              head)
            s, n = head + tail, 1
        else:
            s, k = re.subn(hold, r"\1 ? C_RED : C_GREEN,\2C_ONACCENT", s)
        assert k == n, (f, k)
        open(path, "w").write(s)
    _sub(ui, "s_armed ? C_RED : C_PANEL, s_armed ? C_ONACCENT : C_TEXT,",
         "s_armed ? C_RED : C_GREEN, C_ONACCENT,")
    # Green as a state is an outline: the chip, and the tester's badge,
    # which is a chip and square.
    _sub(w, "\tif (fill == C_GRID) {\n",
         "\tif (fill == C_GREEN) {\n"
         "\t\tgfxRect(x, y, w, UI_CHIP_H, C_GREEN);\n"
         "\t\tgfxRect(x + 1, y + 1, w - 2, UI_CHIP_H - 2, C_BG);\n"
         "\t\tgfxText(x + 6, y + (UI_CHIP_H - 7) / 2, text, C_GREEN, 1);\n"
         "\t\treturn;\n\t}\n"
         "\tif (fill == C_GRID) {\n")
    _sub(ui, "\tgfxRoundRect(4, 3, 92, 20, 4, badge);\n",
         "\tgfxRect(4, 3, 92, 20, badge);\n"
         "\tif (!s_armed) gfxRect(6, 5, 88, 16, C_BG);\n")
    _sub(ui, "\tgfxText(4 + (92 - gfxTextW(txt, 2)) / 2, 7, txt, "
         "C_ONACCENT, 2);\n",
         "\tgfxText(4 + (92 - gfxTextW(txt, 2)) / 2, 7, txt, "
         "s_armed ? C_ONACCENT : C_GREEN, 2);\n")
    _sub(ui, "\t\tgfxRoundRect(STATUS_SDERR_X, STATUS_SDERR_Y, "
         "STATUS_SDERR_W, STATUS_SDERR_H, 2, C_AMBER);\n",
         "\t\tgfxRect(STATUS_SDERR_X, STATUS_SDERR_Y, "
         "STATUS_SDERR_W, STATUS_SDERR_H, C_AMBER);\n")


def sheet(out, shot_dir, names, views=("normal",), scale=2):
    """Tile shots, one row per view, into @p out."""
    from PIL import Image
    gap = 8
    rows = []
    for view in views:
        row = []
        for n in names:
            im = Image.open(os.path.join(shot_dir, f"shot_{n}.ppm"))
            im = im.convert("RGB")
            if view != "normal":
                cache = {}
                px = im.load()
                for y in range(im.height):
                    for x in range(im.width):
                        c = px[x, y]
                        if c not in cache:
                            cache[c] = simulate(c, view)
                        px[x, y] = cache[c]
            row.append(im)
        rows.append(row)
    w = sum(i.width for i in rows[0]) + gap * (len(names) + 1)
    h = sum(r[0].height for r in rows) + gap * (len(rows) + 1)
    sh = Image.new("RGB", (w, h), (40, 40, 40))
    y = gap
    for row in rows:
        x = gap
        for im in row:
            sh.paste(im, (x, y))
            x += im.width + gap
        y += row[0].height + gap
    if scale != 1:
        sh = sh.resize((w * scale, h * scale), Image.NEAREST)
    sh = sh.quantize(colors=128, dither=Image.Dither.NONE)
    sh.save(out, optimize=True)


def main(argv):
    if argv[:1] == ["--patch"]:
        patch(argv[1], "--radius" in argv)
    elif argv[:1] == ["--sheet"]:
        sheet(argv[1], argv[2], argv[3:])
    elif argv[:1] == ["--cvd"]:
        sheet(argv[1], argv[2], argv[3:],
              views=("protan", "deutan", "tritan"), scale=1)
    else:
        table_rcbench()
        table_mapping()
        table_contrast()
        table_cvd()
        table_light()
        table_alternatives()
        table_decisions()


if __name__ == "__main__":
    main(sys.argv[1:])
