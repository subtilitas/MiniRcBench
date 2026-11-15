#!/usr/bin/env python3
"""Host tests of tools/set_clock.py, without pyserial and without a board.

Run by `make run` in test/. A fake port answers as src/clock_line.cpp does,
between telemetry lines; a fake clock stands in for the computer's.
"""

import contextlib
import datetime
import io
import os
import sys
import time
import unittest

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, os.path.join(os.path.dirname(HERE), "tools"))
import set_clock  # noqa: E402

TELEMETRY = (b"arm=0 thr=   0 rpm=     0 erpm=     0  0.00V   0.0A   0C "
             b"stress=  0 st=0x00 ok=0 bad=0 none=0 rate=0 err=0%\r\n")


class PortInfo:
    def __init__(self, device, vid, pid):
        self.device, self.vid, self.pid = device, vid, pid


class FakePort:
    """A serial port: what was written, and the lines the board sends back."""

    def __init__(self, answer=None, before=(TELEMETRY, TELEMETRY), echo_set=True,
                 partial=False, fail=None):
        self.partial = partial    # the board holds half a line another program sent
        self.fail = fail          # raised by the next write of a CLOCK line
        self.written = b""
        self.lines = []
        self.answer = answer
        self.before = list(before)
        self.echo_set = echo_set
        self.flushed = self.reset = 0
        self.dtr = False
        self.closed = False

    def reset_input_buffer(self):
        self.reset += 1
        self.lines = []

    def write(self, b):
        if self.fail is not None and b.startswith(b"CLOCK"):
            raise self.fail
        self.written += b
        if b == b"\n":
            # The board ends the half line: `CLOCKX`, a bad line.
            if self.partial:
                self.lines.append(b"CLOCK REFUSED: BAD LINE\r\n")
                self.partial = False
            return len(b)
        if self.partial:
            # The line runs on after the half: the board refuses the whole.
            self.lines.append(b"CLOCK REFUSED: BAD LINE\r\n")
            return len(b)
        self.lines += self.before
        if self.answer is not None:
            self.lines.append(self.answer)
        elif self.echo_set and b.startswith(b"CLOCK "):
            self.lines.append(b"CLOCK SET " + b[6:].rstrip(b"\n") + b"\r\n")
        return len(b)

    def flush(self):
        self.flushed += 1

    def readline(self):
        return self.lines.pop(0) if self.lines else b""

    def close(self):
        self.closed = True


class FakeSerialModule:
    class SerialException(OSError):
        pass

    def __init__(self, port=None, fail=False):
        self.port = port or FakePort()
        self.fail = fail
        self.opened = []

    def Serial(self, name, baud, timeout=None, write_timeout=None):
        self.opened.append((name, baud, timeout, write_timeout))
        if self.fail:
            raise self.SerialException("could not open port %s" % name)
        return self.port


class Clock:
    """A monotonic clock that moves on each time it is read."""

    def __init__(self, step=0.1):
        self.t, self.step = 0.0, step

    def __call__(self):
        self.t += self.step
        return self.t


def run_main(argv, mod, ports):
    out, err = io.StringIO(), io.StringIO()
    with contextlib.redirect_stdout(out), contextlib.redirect_stderr(err):
        status = set_clock.main(argv, serial_mod=mod, list_ports=lambda: ports)
    return status, out.getvalue(), err.getvalue()


BOARD = PortInfo("/dev/ttyACM0", 0x2E8A, 0x4006)
PICO = PortInfo("/dev/ttyACM1", 0x2E8A, 0x000A)
OTHER = PortInfo("/dev/ttyUSB0", 0x0403, 0x6001)
NOT_USB = PortInfo("/dev/ttyS0", None, None)


class TestPorts(unittest.TestCase):
    def test_board_ports_by_ids(self):
        self.assertEqual(set_clock.board_ports([OTHER, BOARD, NOT_USB, PICO]), ["/dev/ttyACM0"])
        self.assertEqual(set_clock.board_ports([]), [])
        b2 = PortInfo("COM7", 0x2E8A, 0x4006)
        self.assertEqual(set_clock.board_ports([BOARD, b2]), ["/dev/ttyACM0", "COM7"])
        self.assertEqual(set_clock.board_ports([b2, BOARD]), ["/dev/ttyACM0", "COM7"])
        self.assertEqual(set_clock.board_ports([PortInfo("x", 0x2E8A, None)]), [])
        self.assertEqual(set_clock.board_ports([PortInfo("x", None, 0x4006)]), [])

    def test_choose(self):
        self.assertEqual(set_clock.choose_port(["COM5"]), "COM5")
        with self.assertRaisesRegex(set_clock.ToolError, "no MiniRcBench found.*2E8A:4006.*--port"):
            set_clock.choose_port([])
        with self.assertRaisesRegex(set_clock.ToolError, "2 MiniRcBench boards found: A, B; name one with --port"):
            set_clock.choose_port(["A", "B"])


class TestLine(unittest.TestCase):
    def test_set_line(self):
        self.assertEqual(set_clock.set_line(datetime.datetime(2026, 11, 14, 15, 42, 7)),
                         "CLOCK 2026-11-14 15:42:07\n")
        self.assertEqual(set_clock.set_line(datetime.datetime(2000, 1, 1, 0, 0, 0)),
                         "CLOCK 2000-01-01 00:00:00\n")
        self.assertEqual(set_clock.set_line(datetime.datetime(2099, 12, 31, 23, 59, 59)),
                         "CLOCK 2099-12-31 23:59:59\n")
        self.assertEqual(len(set_clock.set_line(datetime.datetime(2026, 1, 2, 3, 4, 5))), 26)

    def test_year_out_of_range(self):
        for y in (1999, 2100):
            with self.assertRaisesRegex(set_clock.ToolError, "year %d is outside the clock's 2000 to 2099" % y):
                set_clock.set_line(datetime.datetime(y, 6, 1, 12, 0, 0))

    def test_next_second(self):
        slept = []
        t0 = time.mktime((2026, 11, 14, 15, 42, 6, 0, 0, -1)) + 0.25
        got = set_clock.next_second(now=lambda: t0, sleep=slept.append)
        self.assertEqual(got, datetime.datetime(2026, 11, 14, 15, 42, 7))
        self.assertAlmostEqual(slept[0], 0.75, places=6)
        # On a whole second: the next one, a whole second on.
        t1 = time.mktime((2026, 11, 14, 15, 42, 6, 0, 0, -1))
        slept = []
        got = set_clock.next_second(now=lambda: t1, sleep=slept.append)
        self.assertEqual(got, datetime.datetime(2026, 11, 14, 15, 42, 7))
        self.assertAlmostEqual(slept[0], 1.0, places=6)
        self.assertEqual(got.microsecond, 0)

    def test_answers(self):
        self.assertTrue(set_clock.is_set_answer("CLOCK SET 2026-11-14 15:42:07"))
        self.assertTrue(set_clock.is_set_answer("CLOCK REFUSED: BAD DATE"))
        self.assertFalse(set_clock.is_set_answer("CLOCK NOW 2026-11-14 15:42:07"))
        self.assertFalse(set_clock.is_set_answer("CLOCK NOT SET"))
        self.assertFalse(set_clock.is_set_answer("CLOCK REFUSED: "))
        self.assertFalse(set_clock.is_set_answer("arm=0 CLOCK SET"))
        self.assertTrue(set_clock.is_query_answer("CLOCK NOW 2026-11-14 15:42:07"))
        self.assertFalse(set_clock.is_query_answer("CLOCK 2026-11-14 15:42:07"))
        self.assertTrue(set_clock.is_query_answer("CLOCK NOT SET"))
        self.assertTrue(set_clock.is_query_answer("CLOCK REFUSED: RTC NOT ANSWERING"))
        self.assertFalse(set_clock.is_query_answer("CLOCK SET 2026-11-14 15:42:07"))
        self.assertFalse(set_clock.is_query_answer("CLOCK NOW 2026-11-14 15:42"))
        self.assertFalse(set_clock.is_query_answer("CLOCK NOW 2026-11-14 15:42:07 "))
        self.assertFalse(set_clock.is_query_answer("CLOCK NOW NOT SET"))
        self.assertFalse(set_clock.is_query_answer("XCLOCK NOT SET"))

    def test_read_answer_skips_telemetry(self):
        lines = [TELEMETRY, b"", b"CLOCK 2026-11-14 15:42:07\r\n", TELEMETRY,
                 b"CLOCK SET 2026-11-14 15:42:07\r\n"]
        got = set_clock.read_answer(lambda: lines.pop(0), set_clock.is_set_answer, 3.0, now=Clock())
        self.assertEqual(got, "CLOCK SET 2026-11-14 15:42:07")
        self.assertEqual(lines, [])

    def test_read_answer_lf_only_and_bad_bytes(self):
        lines = [b"\xff\xfe\n", b"CLOCK NOT SET\n"]
        got = set_clock.read_answer(lambda: lines.pop(0), set_clock.is_query_answer, 3.0, now=Clock())
        self.assertEqual(got, "CLOCK NOT SET")

    def test_read_answer_times_out(self):
        clock = Clock(step=0.5)
        with self.assertRaisesRegex(set_clock.ToolError, "no answer from the board within 3.0 s"):
            set_clock.read_answer(lambda: TELEMETRY, set_clock.is_set_answer, 3.0, now=clock)
        self.assertLess(clock.t, 4.5)


class TestSet(unittest.TestCase):
    T0 = time.mktime((2026, 11, 14, 15, 42, 6, 0, 0, -1)) + 0.5

    def test_set(self):
        port = FakePort()
        status, msg = set_clock.set_clock(port, now=lambda: self.T0, sleep=lambda s: None)
        self.assertEqual(status, 0)
        self.assertEqual(port.written, b"\nCLOCK 2026-11-14 15:42:07\n")
        self.assertEqual(port.reset, 2)
        self.assertGreaterEqual(port.flushed, 1)
        self.assertTrue(msg.startswith("CLOCK SET 2026-11-14 15:42:07\n"))
        self.assertIn("set it again after each power-up", msg)
        self.assertIn("no time zone", msg)

    def test_refused(self):
        for answer in (b"CLOCK REFUSED: RTC NOT ANSWERING\r\n", b"CLOCK REFUSED: BAD DATE\r\n",
                       b"CLOCK REFUSED: RTC READ-BACK DIFFERS\r\n"):
            port = FakePort(answer=answer)
            status, msg = set_clock.set_clock(port, now=lambda: self.T0, sleep=lambda s: None)
            self.assertEqual((status, msg), (1, answer.decode().strip()))

    def test_other_time_set(self):
        port = FakePort(answer=b"CLOCK SET 2026-11-14 15:42:08\r\n")
        status, msg = set_clock.set_clock(port, now=lambda: self.T0, sleep=lambda s: None)
        self.assertEqual(status, 1)
        self.assertIn("another time than sent", msg)

    def test_query(self):
        nap = lambda s: None
        port = FakePort(answer=b"CLOCK NOT SET\r\n")
        self.assertEqual(set_clock.query_clock(port, sleep=nap), (0, "CLOCK NOT SET"))
        self.assertEqual(port.written, b"\nCLOCK?\n")
        port = FakePort(answer=b"CLOCK NOW 2026-11-14 15:42:09\r\n")
        self.assertEqual(set_clock.query_clock(port, sleep=nap), (0, "CLOCK NOW 2026-11-14 15:42:09"))
        port = FakePort(answer=b"CLOCK REFUSED: BAD LINE\r\n")
        self.assertEqual(set_clock.query_clock(port, sleep=nap), (1, "CLOCK REFUSED: BAD LINE"))

    def test_partial_line_ended_first(self):
        # Another program left `CLOCKX` on the board's line: the tool's line
        # feed ends it, and the board's refusal of it is dropped, not taken
        # as the answer to the set.
        port = FakePort(partial=True)
        slept = []
        status, msg = set_clock.set_clock(port, now=lambda: self.T0, sleep=slept.append)
        self.assertEqual(status, 0, msg)
        self.assertTrue(port.written.startswith(b"\nCLOCK 2026-11-14 15:42:07\n"))
        self.assertEqual(slept[0], set_clock.SETTLE_S)
        port = FakePort(partial=True, before=(), answer=None)
        port.answer = b"CLOCK NOT SET\r\n"
        port.partial = True
        self.assertEqual(set_clock.query_clock(port, sleep=lambda s: None), (0, "CLOCK NOT SET"))

    def test_partial_line_without_the_line_feed(self):
        # The same port, the line feed left out: the refusal is the answer.
        port = FakePort(partial=True)
        port.write(b"CLOCK 2026-11-14 15:42:07\n")
        self.assertEqual(port.readline(), b"CLOCK REFUSED: BAD LINE\r\n")


class TestMain(unittest.TestCase):
    def test_help(self):
        out = io.StringIO()
        with contextlib.redirect_stdout(out), self.assertRaises(SystemExit) as e:
            set_clock.main(["--help"])
        self.assertEqual(e.exception.code, 0)
        for word in ("--port", "--query", "--list", "--timeout", "after each power-up"):
            self.assertIn(word, out.getvalue())

    def test_usage_errors(self):
        for argv in (["--timeout", "0"], ["--bogus"], ["--timeout", "x"]):
            with contextlib.redirect_stderr(io.StringIO()), self.assertRaises(SystemExit) as e:
                set_clock.main(argv, serial_mod=FakeSerialModule(), list_ports=lambda: [])
            self.assertEqual(e.exception.code, 2)

    def test_list(self):
        status, out, _ = run_main(["--list"], FakeSerialModule(), [OTHER, BOARD])
        self.assertEqual((status, out), (0, "/dev/ttyACM0\n"))
        status, out, _ = run_main(["--list"], FakeSerialModule(), [OTHER])
        self.assertEqual((status, out), (0, "no MiniRcBench found on USB\n"))

    def test_query_found_port(self):
        mod = FakeSerialModule(FakePort(answer=b"CLOCK NOT SET\r\n"))
        status, out, err = run_main(["--query"], mod, [OTHER, BOARD])
        self.assertEqual(status, 0, err)
        self.assertEqual(out, "/dev/ttyACM0: CLOCK NOT SET\n")
        self.assertEqual(mod.opened[0][0], "/dev/ttyACM0")
        self.assertTrue(mod.port.dtr)
        self.assertTrue(mod.port.closed)

    def test_named_port(self):
        mod = FakeSerialModule(FakePort(answer=b"CLOCK NOT SET\r\n"))
        status, out, _ = run_main(["--port", "COM5", "--query"], mod, [])
        self.assertEqual((status, out), (0, "COM5: CLOCK NOT SET\n"))

    def test_no_board(self):
        status, out, err = run_main([], FakeSerialModule(), [OTHER])
        self.assertEqual((status, out), (1, ""))
        self.assertIn("no MiniRcBench found", err)

    def test_two_boards(self):
        status, _, err = run_main([], FakeSerialModule(), [BOARD, PortInfo("COM7", 0x2E8A, 0x4006)])
        self.assertEqual(status, 1)
        self.assertIn("2 MiniRcBench boards found", err)

    def test_set_found_port(self):
        mod = FakeSerialModule()
        mod.port.lines = [b"CLOCK SET 2001-01-01 00:00:00\r\n"]   # stale: dropped
        status, out, err = run_main([], mod, [BOARD])
        self.assertEqual(status, 0, err)
        sent = mod.port.written.decode()
        self.assertRegex(sent, r"^\nCLOCK \d{4}-\d\d-\d\d \d\d:\d\d:\d\d\n$")
        sent = sent[1:]
        self.assertTrue(out.startswith("/dev/ttyACM0: CLOCK SET " + sent[6:]))
        self.assertTrue(mod.port.dtr and mod.port.closed)

    def test_lost_port(self):
        mod = FakeSerialModule()
        mod.port.fail = mod.SerialException("write timeout")
        status, out, err = run_main(["--port", "COM5", "--query"], mod, [])
        self.assertEqual((status, out), (1, ""))
        self.assertIn("set_clock.py: lost COM5 during the exchange: write timeout", err)
        self.assertTrue(mod.port.closed)
        mod = FakeSerialModule()
        mod.port.fail = OSError(5, "Input/output error")
        status, _, err = run_main(["--port", "COM5"], mod, [])
        self.assertEqual(status, 1)
        self.assertIn("lost COM5 during the exchange", err)

    def test_cannot_open(self):
        status, _, err = run_main(["--port", "COM9"], FakeSerialModule(fail=True), [])
        self.assertEqual(status, 1)
        self.assertIn("cannot open COM9", err)

    def test_no_answer(self):
        port = FakePort(echo_set=False, before=())
        status, _, err = run_main(["--port", "COM5", "--query", "--timeout", "0.3"],
                                  FakeSerialModule(port), [])
        self.assertEqual(status, 1)
        self.assertIn("no answer from the board within 0.3 s", err)
        self.assertTrue(port.closed)


if __name__ == "__main__":
    unittest.main(verbosity=1)
