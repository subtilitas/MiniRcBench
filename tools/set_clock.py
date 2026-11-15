#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Set the board's clock to this computer's local time over the USB serial port.

    python3 tools/set_clock.py                  the one board found, set
    python3 tools/set_clock.py --port COM5      that port, set
    python3 tools/set_clock.py --query          read the board's clock only
    python3 tools/set_clock.py --list           the boards found, then exit

The board is MiniRcBench on its USB cable. Its serial port takes one line
command, the clock's (src/clock_line.h):

    CLOCK 2026-11-14 15:42:07   answered CLOCK SET 2026-11-14 15:42:07
    CLOCK?                      answered CLOCK 2026-11-14 15:42:09 or
                                CLOCK NOT SET

and refuses a bad line with CLOCK REFUSED and the reason. The time is local
time, as the computer shows it: no time zone and no daylight saving. At a
daylight-saving change the clock is set again by hand.

The board has no cell in its RTC (real-time clock) socket J2, so the clock is
lost each time the board loses power: run this again after each power-up.
Files on the card written before the set carry FAT (File Allocation Table)
date 0, and a log's summary reads `not set`.

The port: without --port, the one serial port whose USB vendor and product
IDs are the board's, 2E8A:4006 (src/usb_descriptors.c). With none or several
found, the tool names what it found and asks for --port.

The set waits for the computer's next whole second and sends that second, so
the board starts its count within the time the line takes to arrive, plus
the part's own half second (docs/design/run-summary.md, "Setting it"); not
measured. The answer is read from the lines the board sends, skipping its
telemetry lines, for up to --timeout seconds.

Exit status: 0 when the board answered as asked, 1 for a refusal, no answer
or no port, 2 for a usage error.

Python 3 and the pyserial package (python3 -m pip install pyserial), on
Windows, Linux and macOS. The host tests in test/test_set_clock.py run
without pyserial.
"""

import argparse
import datetime
import math
import re
import sys
import time

# The board's USB IDs: Raspberry Pi's vendor ID, and the product ID
# src/usb_descriptors.c derives from its interfaces, CDC and MSC.
USB_VID = 0x2E8A
USB_PID = 0x4006

BAUD = 115200          # ignored by the USB CDC port; pyserial wants one
TIMEOUT_S = 3.0        # for the answer, over the 10 Hz telemetry lines
YEAR_MIN, YEAR_MAX = 2000, 2099

QUERY_RE = re.compile(r"^CLOCK (NOT SET|\d{4}-\d\d-\d\d \d\d:\d\d:\d\d)$")
REFUSED_RE = re.compile(r"^CLOCK REFUSED: .+$")


class ToolError(Exception):
    """A failure the tool reports and exits 1 on."""


def board_ports(ports):
    """The device names of the ports in @p ports with the board's USB IDs.

    @p ports holds objects with .device, .vid and .pid, as pyserial's
    serial.tools.list_ports.comports() gives them; vid and pid are None for a
    port that is not on USB.
    """
    return sorted(p.device for p in ports if p.vid == USB_VID and p.pid == USB_PID)


def choose_port(found):
    """The one board port of @p found, or ToolError naming what was found."""
    if len(found) == 1:
        return found[0]
    if not found:
        raise ToolError("no MiniRcBench found on USB (ID %04X:%04X); connect it, "
                        "or name the port with --port" % (USB_VID, USB_PID))
    raise ToolError("%d MiniRcBench boards found: %s; name one with --port"
                    % (len(found), ", ".join(found)))


def set_line(when):
    """The set's line for the local time @p when, a datetime, line feed included."""
    if not YEAR_MIN <= when.year <= YEAR_MAX:
        raise ToolError("the computer's year %d is outside the clock's %d to %d"
                        % (when.year, YEAR_MIN, YEAR_MAX))
    return "CLOCK %s\n" % when.strftime("%Y-%m-%d %H:%M:%S")


def next_second(now=time.time, sleep=time.sleep):
    """Wait for the computer's next whole second; return it as a local datetime.

    Taken from the epoch seconds, so a daylight-saving change on the way is
    the computer's own local time of that second.
    """
    t = now()
    target = math.floor(t) + 1
    sleep(target - t)
    return datetime.datetime.fromtimestamp(target)


def is_set_answer(line):
    """Whether @p line answers a set: CLOCK SET and a time, or a refusal."""
    return line.startswith("CLOCK SET ") or bool(REFUSED_RE.match(line))


def is_query_answer(line):
    """Whether @p line answers a query: the time, NOT SET, or a refusal."""
    return bool(QUERY_RE.match(line)) or bool(REFUSED_RE.match(line))


def read_answer(readline, accept, timeout, now=time.monotonic):
    """The first line from @p readline that @p accept takes, within @p timeout s.

    @p readline returns bytes up to and including a line feed, or what came
    before its own timeout, b"" for nothing. Lines are ASCII; a CR before the
    line feed is dropped. Every other line, the telemetry among them, is
    skipped. ToolError when no line is taken in time.
    """
    end = now() + timeout
    while now() < end:
        raw = readline()
        if not raw:
            continue
        line = raw.decode("ascii", "replace").rstrip("\r\n")
        if accept(line):
            return line
    raise ToolError("no answer from the board within %.1f s; is the firmware "
                    "one that sets the clock?" % timeout)


def set_clock(port, now=time.time, sleep=time.sleep, timeout=TIMEOUT_S):
    """Set the clock on the open @p port. Returns (exit status, message)."""
    when = next_second(now, sleep)
    line = set_line(when)
    port.reset_input_buffer()
    port.write(line.encode("ascii"))
    port.flush()
    answer = read_answer(port.readline, is_set_answer, timeout)
    want = "CLOCK SET " + line[len("CLOCK "):].rstrip("\n")
    if answer == want:
        return 0, ("%s\nlocal time of this computer, no time zone; the board loses "
                   "it at power-off, so set it again after each power-up" % answer)
    if answer.startswith("CLOCK SET "):
        return 1, "the board set another time than sent: %s, sent %s" % (answer, line.strip())
    return 1, answer


def query_clock(port, timeout=TIMEOUT_S):
    """Read the clock on the open @p port. Returns (exit status, message)."""
    port.reset_input_buffer()
    port.write(b"CLOCK?\n")
    port.flush()
    answer = read_answer(port.readline, is_query_answer, timeout)
    return (1 if REFUSED_RE.match(answer) else 0), answer


def parse_args(argv):
    ap = argparse.ArgumentParser(
        prog="set_clock.py",
        description="Set MiniRcBench's clock to this computer's local time over "
                    "the USB serial port. The board has no backup cell: set it "
                    "again after each power-up.")
    ap.add_argument("--port", help="the serial port, e.g. COM5, /dev/ttyACM0 or "
                    "/dev/cu.usbmodem1101; default: the one board found on USB")
    ap.add_argument("--query", action="store_true",
                    help="read the board's clock, set nothing")
    ap.add_argument("--list", action="store_true",
                    help="list the boards found on USB and exit")
    ap.add_argument("--timeout", type=float, default=TIMEOUT_S,
                    help="seconds to wait for the answer (default %(default)s)")
    args = ap.parse_args(argv)
    if args.timeout <= 0:
        ap.error("--timeout must be above 0")
    return args


def main(argv, serial_mod=None, list_ports=None):
    args = parse_args(argv)
    if serial_mod is None:
        try:
            import serial
            from serial.tools import list_ports as lp
        except ImportError:
            print("set_clock.py needs the pyserial package: python3 -m pip install pyserial",
                  file=sys.stderr)
            return 1
        serial_mod, list_ports = serial, lp.comports
    try:
        found = board_ports(list_ports())
        if args.list:
            print("\n".join(found) if found else "no MiniRcBench found on USB")
            return 0
        name = args.port or choose_port(found)
        try:
            port = serial_mod.Serial(name, BAUD, timeout=0.2, write_timeout=1.0)
        except (OSError, serial_mod.SerialException) as e:
            raise ToolError("cannot open %s: %s" % (name, e))
        try:
            port.dtr = True     # the board sends only while DTR is set
            if args.query:
                status, msg = query_clock(port, args.timeout)
            else:
                status, msg = set_clock(port, timeout=args.timeout)
        finally:
            port.close()
    except ToolError as e:
        print("set_clock.py: %s" % e, file=sys.stderr)
        return 1
    print("%s: %s" % (name, msg))
    return status


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
