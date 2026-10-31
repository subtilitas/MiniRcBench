#!/usr/bin/env python3
"""Draw the bench wiring as docs/wiring.svg.

The connector map and the pin roles come from the firmware's own sources, so
the drawing follows the code:

- the JST-SH connector table in src/board_rp2350_touch_lcd_2_8.h
  (the "J4 (12P): 1=GND ..." lines);
- DSHOT_PIN_LCD_2_8 and KISS_PIN_LCD_2_8 in src/config.h, the pins a board
  with blank flash starts on;
- the ServoSupplyMode values in src/servo_supply.h, which name the enable
  pins and their connector pins, held against servoSupplyPinOf() in
  src/servo_supply.cpp, the mapping the firmware runs.

Usage:
    python3 tools/make_wiring.py           # write docs/wiring.svg
    python3 tools/make_wiring.py --check   # fail when docs/wiring.svg differs
"""

import difflib
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
BOARD_H = os.path.join(ROOT, "src", "board_rp2350_touch_lcd_2_8.h")
CONFIG_H = os.path.join(ROOT, "src", "config.h")
SUPPLY_H = os.path.join(ROOT, "src", "servo_supply.h")
SUPPLY_CPP = os.path.join(ROOT, "src", "servo_supply.cpp")
OUT = os.path.join(ROOT, "docs", "wiring.svg")

W = 1200
FONT = "DejaVu Sans Mono, Menlo, Consolas, monospace"

# Wire colours: a servo lead's own (orange signal, red plus, brown minus),
# blue for the telemetry wire, green for the supply enable.
INK = "#1a1a1a"
DIM = "#5f6368"
SIG = "#e65100"
PLUS = "#c62828"
MINUS = "#4e342e"
AUX = "#1565c0"
EN = "#2e7d32"
PANEL = "#f5f7fa"
BOX = "#ffffff"


# --------------------------------------------------------------------------
# sources
# --------------------------------------------------------------------------

def read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def connectors():
    """{"J4": {1: "GND", ...}, ...} from the board header's connector table."""
    text = read(BOARD_H)
    out = {}
    current = None
    for line in text.splitlines():
        m = re.match(r"\s*\*\s+(J\d+)\s+\((\d+)P\):\s*(.*)$", line)
        if m:
            current = m.group(1)
            out[current] = {"count": int(m.group(2)), "pins": {}}
            rest = m.group(3)
        elif current and re.match(r"\s*\*\s+\d+=", line):
            rest = line.split("*", 1)[1]
        else:
            current = None
            continue
        for num, name in re.findall(r"(\d+)=(\S+)", rest):
            out[current]["pins"][int(num)] = name
    for name, c in out.items():
        if sorted(c["pins"]) != list(range(1, c["count"] + 1)):
            raise SystemExit("%s: connector %s lists pins %s, not 1 to %d"
                             % (BOARD_H, name, sorted(c["pins"]), c["count"]))
    if not {"J3", "J4", "J5"} <= set(out):
        raise SystemExit("%s: connector table not found" % BOARD_H)
    return out


def define(path, name):
    m = re.search(r"#define\s+%s\s+(\d+)" % name, read(path))
    if not m:
        raise SystemExit("%s: no #define %s" % (path, name))
    return int(m.group(1))


def supply_pins():
    """[(gpio, connector, pin)] for each ServoSupplyMode with a pin.

    Every enum member named GpN has to name its connector pin in its comment,
    the members have to number SERVO_SUPPLY_MODE_MAX, and servoSupplyPinOf()
    has to map each one to GPIO N: a member that parses short fails here
    rather than dropping out of the drawing.
    """
    body = re.search(r"enum class ServoSupplyMode[^{]*\{(.*?)\};", read(SUPPLY_H), re.S)
    if not body:
        raise SystemExit("%s: enum class ServoSupplyMode not found" % SUPPLY_H)
    members = re.findall(r"\bGp(\d+)\s*=", body.group(1))
    found = re.findall(r"\bGp(\d+)\s*=\s*\d+,?\s*/\*\*<[^*]*?\bGP(\d+), (J\d+) pin (\d+)",
                       body.group(1))
    if not members or len(found) != len(members):
        raise SystemExit("%s: %d of the %d enable pins in ServoSupplyMode name their "
                         "connector pin as 'GPn, Jn pin n'" % (SUPPLY_H, len(found), len(members)))
    most = define(SUPPLY_H, "SERVO_SUPPLY_MODE_MAX")
    if most != len(members):
        raise SystemExit("%s: SERVO_SUPPLY_MODE_MAX is %d, ServoSupplyMode has %d enable pins"
                         % (SUPPLY_H, most, len(members)))
    cpp = read(SUPPLY_CPP)
    out = []
    for enum_gp, gp, conn, pin in found:
        if enum_gp != gp:
            raise SystemExit("%s: Gp%s names GP%s" % (SUPPLY_H, enum_gp, gp))
        if not re.search(r"case ServoSupplyMode::Gp%s:\s*return %s;" % (gp, gp), cpp):
            raise SystemExit("%s: servoSupplyPinOf() does not map Gp%s to %s" % (SUPPLY_CPP, gp, gp))
        out.append((int(gp), conn, int(pin)))
    return out


def pin_of(conns, conn, gpio):
    for num, name in conns[conn]["pins"].items():
        if name.rstrip("*") == "GP%d" % gpio:
            return num
    raise SystemExit("%s: GP%d is on no pin of %s" % (BOARD_H, gpio, conn))


def ground_of(conns, conn):
    pins = [n for n, name in sorted(conns[conn]["pins"].items()) if name == "GND"]
    if not pins:
        raise SystemExit("%s: %s has no GND pin" % (BOARD_H, conn))
    return pins


# --------------------------------------------------------------------------
# drawing
# --------------------------------------------------------------------------

class Svg:
    def __init__(self):
        self.parts = []

    def add(self, s):
        self.parts.append(s)

    def text(self, x, y, s, size=13, color=INK, anchor="start", weight="normal"):
        s = s.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
        w = ' font-weight="bold"' if weight == "bold" else ""
        self.add('<text x="%g" y="%g" font-size="%g" fill="%s" text-anchor="%s"%s>%s</text>'
                 % (x, y, size, color, anchor, w, s))

    def rect(self, x, y, w, h, fill=BOX, stroke=INK, rx=4, dash=False):
        d = ' stroke-dasharray="6 4"' if dash else ""
        self.add('<rect x="%g" y="%g" width="%g" height="%g" rx="%g" fill="%s" '
                 'stroke="%s" stroke-width="1.5"%s/>' % (x, y, w, h, rx, fill, stroke, d))

    def wire(self, pts, color, dash=False, width=2.5):
        d = ' stroke-dasharray="8 5"' if dash else ""
        self.add('<polyline points="%s" fill="none" stroke="%s" stroke-width="%g" '
                 'stroke-linejoin="round" stroke-linecap="round"%s/>'
                 % (" ".join("%g,%g" % p for p in pts), color, width, d))

    def dot(self, x, y, color):
        self.add('<circle cx="%g" cy="%g" r="4" fill="%s"/>' % (x, y, color))

    def cross(self, x, y, color=PLUS):
        self.add('<path d="M%g %gl12 12m0 -12l-12 12" stroke="%s" stroke-width="3"/>'
                 % (x - 6, y - 6, color))

    def resistor(self, x, y0, y1, color):
        """A vertical resistor between y0 and y1, the zigzag in the middle third."""
        h = y1 - y0
        z0, z1 = y0 + h / 3, y1 - h / 3
        step = (z1 - z0) / 6
        pts = [(x, y0), (x, z0)]
        for i in range(6):
            pts.append((x + (8 if i % 2 == 0 else -8), z0 + step * (i + 0.5)))
        pts += [(x, z1), (x, y1)]
        self.wire(pts, color, width=2)

    def switch(self, x0, x1, y, color):
        """An open switch between x0 and x1 on a horizontal wire at y."""
        self.dot(x0, y, color)
        self.dot(x1, y, color)
        self.add('<line x1="%g" y1="%g" x2="%g" y2="%g" stroke="%s" stroke-width="2.5"/>'
                 % (x0, y, x1 - 4, y - 16, color))

    def supply(self, x, y, label_lines):
        """A supply box 80 px wide, 60 px tall: + at the top centre, - at the bottom."""
        self.rect(x, y, 80, 60)
        self.text(x + 40, y + 26, "+", 16, PLUS, "middle", "bold")
        self.text(x + 40, y + 48, "-", 16, MINUS, "middle", "bold")
        for i, s in enumerate(label_lines):
            self.text(x + 92, y + 22 + 16 * i, s, 12, DIM)
        return (x + 40, y), (x + 40, y + 60)

    def render(self, height):
        head = ('<svg xmlns="http://www.w3.org/2000/svg" width="%d" height="%d" '
                'viewBox="0 0 %d %d" font-family="%s">\n'
                '<rect width="100%%" height="100%%" fill="#ffffff"/>\n'
                % (W, height, W, height, FONT))
        return head + "\n".join(self.parts) + "\n</svg>\n"


def stub(svg, x, y, conn, rows):
    """A connector stub: rows of (pin, label, colour); returns {pin: (x, y)} of
    its ports on the right edge, and of its left edge under key -pin."""
    h = 24 * len(rows) + 16
    svg.rect(x, y, 130, h, fill="#eceff1")
    svg.text(x, y - 6, conn, 12, DIM, weight="bold")
    ports = {}
    for i, (pin, label, color) in enumerate(rows):
        py = y + 20 + 24 * i
        svg.rect(x + 8, py - 8, 16, 16, fill=color, stroke=INK, rx=2)
        svg.text(x + 16, py + 4, str(pin), 10, "#ffffff", "middle", "bold")
        svg.text(x + 30, py + 4, label, 12)
        ports[pin] = (x + 130, py)
        ports[-pin] = (x, py)
    return ports


def part(svg, x, y, w, h, title, pins, title_lines=(), below=False):
    """A component box with labelled pins on its left edge at absolute ys; the
    title above the box, or under it with @p below."""
    svg.rect(x, y, w, h)
    svg.text(x + w / 2, y + h + 18 if below else y - 8, title, 13, INK, "middle", "bold")
    for i, s in enumerate(title_lines):
        svg.text(x + w / 2, y + h + 16 + 14 * i, s, 11, DIM, "middle")
    for py, label in pins:
        svg.text(x + 6, py + 4, label, 11, DIM)


def notes(svg, x, y, lines):
    for i, s in enumerate(lines):
        svg.text(x, y + 18 * i, s, 12, INK)
    return y + 18 * len(lines)


def panel(svg, y, h, title):
    svg.rect(20, y, W - 40, h, fill=PANEL, stroke="#b0bec5", rx=8)
    svg.text(36, y + 26, title, 15, INK, weight="bold")


ROLE_COLOUR = {"SIG": SIG, "AUX": AUX, "EN": EN, "GND": MINUS}


def draw():
    conns = connectors()
    sig = define(CONFIG_H, "DSHOT_PIN_LCD_2_8")
    aux = define(CONFIG_H, "KISS_PIN_LCD_2_8")
    supplies = supply_pins()
    en_gp, en_conn, en_pin = supplies[0]
    if pin_of(conns, en_conn, en_gp) != en_pin:
        raise SystemExit("%s says GP%d is %s pin %d; %s disagrees"
                         % (SUPPLY_H, en_gp, en_conn, en_pin, BOARD_H))
    for gp, conn, pin in supplies[1:]:
        if pin_of(conns, conn, gp) != pin:
            raise SystemExit("%s says GP%d is %s pin %d; %s disagrees"
                             % (SUPPLY_H, gp, conn, pin, BOARD_H))
    en_names = {"GP%d" % gp for gp, _, _ in supplies}

    def role(name):
        bare = name.rstrip("*")
        if bare == "GP%d" % sig:
            return "SIG", "ESC or servo signal", SIG
        if bare == "GP%d" % aux:
            return "AUX", "KISS telemetry, servo B", AUX
        if bare in en_names:
            return "EN", "servo supply enable, option", EN
        if name == "GND":
            return "", "ground", MINUS
        if name == "3V3":
            return "", "3.3 V out, not used", DIM
        if name == "VBUS":
            return "", "USB 5 V, feeds no servo or ESC", DIM
        if name in ("USB_N", "USB_P"):
            return "", "USB data, not used", DIM
        if name.endswith("*"):
            return "", "shared I2C bus: do not use", "#9e9e9e"
        return "", "", DIM

    svg = Svg()
    svg.text(W / 2, 36, "MiniRcBench wiring: Waveshare RP2350-Touch-LCD-2.8", 20,
             INK, "middle", "bold")
    svg.text(W / 2, 58, "One device on SIG at a time. Grounds are joined once, at the board.",
             13, DIM, "middle")

    # --- the board's connectors -------------------------------------------
    y0 = 80
    panel(svg, y0, 330, "Board connectors (JST-SH 1.0 mm; the board has no 2.54 mm headers)")
    cols = [("J5", 40), ("J4", 420), ("J3", 820)]
    for conn, x in cols:
        c = conns[conn]
        svg.text(x, y0 + 58, "%s, %d pins" % (conn, c["count"]), 13, INK, weight="bold")
        for i, (num, name) in enumerate(sorted(c["pins"].items())):
            py = y0 + 80 + 20 * i
            short, what, color = role(name)
            svg.rect(x, py - 8, 16, 16, fill=color, stroke=INK, rx=2)
            svg.text(x + 8, py + 4, str(num), 10, "#ffffff", "middle", "bold")
            label = "%-6s %-4s %s" % (name.rstrip("*"), short, what)
            svg.text(x + 24, py + 4, label, 12, INK if short or name == "GND" else DIM)
    sig_j5 = pin_of(conns, "J5", sig)
    aux_j5 = pin_of(conns, "J5", aux)
    gnd_j5 = ground_of(conns, "J5")[0]
    gnd_j4 = ground_of(conns, "J4")[0]
    v3_j5 = [n for n, s in conns["J5"]["pins"].items() if s == "3V3"][0]
    other = [s for s in conns["J5"]["pins"] if s not in (sig_j5, aux_j5, gnd_j5, v3_j5)]
    if other:
        raise SystemExit("%s: J5 carries pins this drawing does not place: %s"
                         % (BOARD_H, other))
    notes(svg, 40, y0 + 200, [
        "USB-C: power and the USB serial port.",
        "LiPo cell: optional, charged by the board.",
        "SIG and AUX are set under",
        "MENU > SETTINGS; these are the",
        "pins of a board with blank flash.",
    ])
    svg.text(820, y0 + 200, "SIG, AUX: GP%d, GP%d, fault tolerant," % (sig, aux), 12)
    svg.text(820, y0 + 218, "5.5 V with the board powered,", 12)
    svg.text(820, y0 + 236, "3.63 V with it off.", 12)
    svg.text(820, y0 + 260, "EN: %s, ADC pins," % ", ".join("GP%d" % g for g, _, _ in supplies), 12)
    svg.text(820, y0 + 278, "not fault tolerant: an output only.", 12)

    def j5_rows():
        rows = {}
        for num, name in conns["J5"]["pins"].items():
            short, _, color = role(name)
            label = name if short in ("", name) else "%s %s" % (name, short)
            rows[num] = (num, label, color)
        return [rows[n] for n in sorted(rows)]

    # --- A: the ESC ---------------------------------------------------------
    ya = y0 + 350
    panel(svg, ya, 262, "A  ESC: bidirectional DShot on SIG, KISS telemetry on AUX")
    p = stub(svg, 40, ya + 56, "J5", j5_rows())
    ys = {n: p[n][1] for n in conns["J5"]["pins"]}
    part(svg, 520, ya + 56, 160, 116, "ESC", [
        (ys[aux_j5], "telemetry"), (ys[sig_j5], "signal"),
        (ys[v3_j5], "BEC +5 V"), (ys[gnd_j5], "GND")])
    svg.wire([p[aux_j5], (520, ys[aux_j5])], AUX, dash=True)
    svg.text(300, ys[aux_j5] - 6, "optional, SETTINGS > KISS TELEM", 11, AUX, "middle")
    svg.wire([p[sig_j5], (520, ys[sig_j5])], SIG)
    svg.wire([(400, ys[v3_j5]), (520, ys[v3_j5])], PLUS)
    svg.cross(400, ys[v3_j5])
    svg.text(390, ys[v3_j5] + 4, "cut: no pin", 11, PLUS, "end")
    svg.text(p[v3_j5][0] + 8, ys[v3_j5] + 4, "n.c.", 11, DIM)
    svg.wire([p[gnd_j5], (520, ys[gnd_j5])], MINUS)
    # motor and battery
    mx, my = 900, ya + 96
    for k, dy in enumerate((-16, 0, 16)):
        svg.wire([(680, my + dy), (mx - 34 + abs(dy) / 4, my + dy)], INK, width=2)
    svg.add('<circle cx="%g" cy="%g" r="34" fill="%s" stroke="%s" stroke-width="1.5"/>'
            % (mx, my, BOX, INK))
    svg.text(mx, my + 6, "M", 18, INK, "middle", "bold")
    svg.text(mx + 44, my + 4, "motor, 3 phases", 12, DIM)
    by = ya + 140
    svg.wire([(680, by), (980, by)], PLUS)
    svg.wire([(680, by + 18), (980, by + 18)], MINUS)
    svg.rect(980, by - 12, 150, 44)
    svg.text(1055, by + 6, "ESC battery", 13, INK, "middle", "bold")
    svg.text(1055, by + 22, "its own pack", 11, DIM, "middle")
    notes(svg, 40, ya + 208, [
        "Cut or depin the ESC lead's middle wire, the BEC +5 V output: it goes to no pin.",
        "Power the ESC from its own battery, never from the board's 5 V. Unplug any servo before DShot runs.",
    ])

    # --- B: a servo on a hand-switched supply --------------------------------
    yb = ya + 282
    panel(svg, yb, 360, "B  Servo, supply switched by hand: servo tester, KST, Hitec D series")
    p = stub(svg, 40, yb + 56, "J5", j5_rows())
    ys = {n: p[n][1] for n in conns["J5"]["pins"]}
    sy_sig, sy_plus, sy_minus = ys[sig_j5], ys[sig_j5] + 24, ys[sig_j5] + 48
    part(svg, 900, sy_sig - 18, 120, 84, "servo A", [
        (sy_sig, "signal"), (sy_plus, "+ red"), (sy_minus, "- brown")], below=True)
    svg.wire([p[sig_j5], (900, sy_sig)], SIG)
    rail = yb + 262
    sp, sm = svg.supply(300, yb + 184, ["servo supply: 5 V for KST and Hitec;",
                                         "tester 4.8 V to 6.0 V, 8.4 V for HV"])
    svg.wire([sp, (sp[0], yb + 172), (480, yb + 172)], PLUS)
    svg.switch(480, 540, yb + 172, PLUS)
    svg.text(510, yb + 148, "switch", 11, DIM, "middle")
    svg.wire([(540, yb + 172), (860, yb + 172), (860, sy_plus), (900, sy_plus)], PLUS)
    svg.wire([p[gnd_j5], (230, ys[gnd_j5]), (230, rail), (880, rail), (880, sy_minus),
              (900, sy_minus)], MINUS)
    svg.wire([sm, (sm[0], rail)], MINUS)
    svg.dot(sm[0], rail, MINUS)
    svg.text(236, ys[gnd_j5] + 18, "one ground tap", 11, MINUS)
    svg.text(p[v3_j5][0] + 8, ys[v3_j5] + 4, "n.c.", 11, DIM)
    # servo B, the tester's second servo, reached over the top of servo A
    # whichever J5 row AUX has
    sb = yb + 70
    svg.wire([p[aux_j5], (200, ys[aux_j5]), (200, yb + 40), (1100, yb + 40), (1100, sb)],
             AUX, dash=True)
    svg.rect(1035, sb, 135, 60, dash=True)
    svg.text(1102, sb + 20, "servo B", 13, INK, "middle", "bold")
    svg.text(1102, sb + 36, "tester only;", 11, DIM, "middle")
    svg.text(1102, sb + 50, "+ and - as A", 11, DIM, "middle")
    notes(svg, 40, yb + 296, [
        "The servo's middle wire goes to its supply, never to a connector pin. Power the board before the servo supply,",
        "and switch the servo supply off first. KST answers only just after power-up: switch the supply on when the screen",
        "searches. Power a Hitec D series servo before tapping D SERIES.",
    ])

    # --- C: a servo on the switched supply ---------------------------------
    yc = yb + 380
    sig_j4 = pin_of(conns, "J4", sig)
    j4_rows = sorted([(gnd_j4, "GND", MINUS), (sig_j4, "GP%d SIG" % sig, SIG)] + [
        (pin, ("GP%d EN" if i == 0 else "or GP%d EN") % gp, EN)
        for i, (gp, _, pin) in enumerate(supplies)])
    # the converter: EN on the left, VIN on the right, VOUT on top, GND below
    cx, cy = 480, yc + 250
    rail = cy + 150
    panel(svg, yc, rail + 140 - yc, "C  Servo on the switched supply: KST, Hitec D series and classic"
          " (SETTINGS > SERVO SUPPLY GP%d)" % en_gp)
    q = stub(svg, 40, yc + 56, "J4 (part)", j4_rows)
    en_y = q[en_pin][1]
    sy_sig, sy_plus, sy_minus = yc + 190, yc + 214, yc + 238
    part(svg, 900, sy_sig - 18, 120, 84, "servo", [
        (sy_sig, "signal"), (sy_plus, "+ red"), (sy_minus, "- brown")])
    svg.wire([q[sig_j4], (820, q[sig_j4][1]), (820, sy_sig), (900, sy_sig)], SIG)
    svg.rect(cx, cy, 160, 90)
    svg.text(cx + 80, cy + 50, "DC-DC", 14, INK, "middle", "bold")
    svg.text(cx + 80, cy + 66, "5 V out", 11, DIM, "middle")
    svg.text(cx + 6, cy + 24, "EN", 11, DIM)
    svg.text(cx + 154, cy + 24, "VIN", 11, DIM, "end")
    svg.text(cx + 80, cy + 14, "VOUT", 11, DIM, "middle")
    svg.text(cx + 80, cy + 84, "GND", 11, DIM, "middle")
    # the user supply into VIN
    sp, sm = svg.supply(700, cy + 40, ["user supply"])
    svg.wire([sp, (sp[0], cy + 20), (cx + 160, cy + 20)], PLUS)
    svg.wire([sm, (sm[0], rail)], MINUS)
    svg.dot(sm[0], rail, MINUS)
    svg.wire([(cx + 80, cy + 90), (cx + 80, rail)], MINUS)
    svg.dot(cx + 80, rail, MINUS)
    # the enable, and its pull-down at the converter from EN to GND
    svg.wire([q[en_pin], (440, en_y), (440, cy + 20), (cx, cy + 20)], EN)
    svg.dot(460, cy + 20, EN)
    svg.resistor(460, cy + 20, rail, EN)
    svg.dot(460, rail, MINUS)
    svg.text(448, cy + 64, "10 k\u03a9", 12, EN, "end", "bold")
    svg.text(448, cy + 80, "EN to GND,", 11, DIM, "end")
    svg.text(448, cy + 94, "at the converter", 11, DIM, "end")
    # the converter's output to the servo, and the ground rail
    svg.wire([(cx + 80, cy), (cx + 80, cy - 30), (860, cy - 30), (860, sy_plus),
              (900, sy_plus)], PLUS)
    svg.wire([q[-gnd_j4], (30, q[gnd_j4][1]), (30, rail), (880, rail), (880, sy_minus),
              (900, sy_minus)], MINUS)
    svg.text(36, rail - 8, "one ground tap", 11, MINUS)
    svg.text(cx, rail + 20, "DC-DC: > 1 A; buck, or output disconnect;", 11, DIM)
    svg.text(cx, rail + 34, "e.g. Pololu S13V30F5, 2.8 V to 22 V in", 11, DIM)
    notes(svg, 40, rail + 62, [
        "EN on %s, with SERVO SUPPLY at that pin. SIG, EN and GND share one J4 pigtail;"
        % " or ".join("GP%d (%s pin %d)" % t for t in supplies),
        "J5 stays free. The pull-down sits at the converter: 10 k\u03a9 for the Pololu S13V30F5, whose EN has a 475 k\u03a9",
        "pull-up; 10 k\u03a9 to 100 k\u03a9 for an EN without one. EN is high only during a KST or Hitec claim. The servo tester",
        "does not switch it: a servo there needs a supply of its own. An ESC on this lead gets VOUT on its BEC wire.",
    ])

    height = rail + 140 + 20
    svg.text(W - 24, height - 8,
             "tools/make_wiring.py, from src/board_rp2350_touch_lcd_2_8.h, src/config.h, src/servo_supply.{h,cpp}",
             10, DIM, "end")
    return svg.render(height)


def main(argv):
    if argv not in ([], ["--check"]):
        print("usage: python3 tools/make_wiring.py [--check]", file=sys.stderr)
        return 2
    check = argv == ["--check"]
    out = draw()
    if check:
        try:
            have = read(OUT)
        except FileNotFoundError:
            have = ""
        if have != out:
            diff = list(difflib.unified_diff(have.splitlines(), out.splitlines(),
                                             "docs/wiring.svg", "drawn", lineterm="", n=0))
            print("\n".join(diff[:20]), file=sys.stderr)
            print("docs/wiring.svg is stale -- run: python3 tools/make_wiring.py",
                  file=sys.stderr)
            return 1
        print("docs/wiring.svg is current")
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(out)
    print("docs/wiring.svg written")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
