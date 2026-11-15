#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Cycle-level check of the USB bridge's line: src/bridge_line.pio and
src/pio_uart_rx.pio in two state machines on one pad.

    python3 test/pio_bridge_emu.py                 # exit 1 on any failure
    python3 test/pio_bridge_emu.py -v              # one line per run
    python3 test/pio_bridge_emu.py --pioasm PATH   # also hold the words to pioasm

Assembles both programs with the assembler of pio_sdio_emu.py and runs them
one state machine cycle at a time, configured as src/bridge_line_port.cpp
configures them: one clock divider, both enabled in the same cycle; the
transmitter's OUT, SET and side-set pin and the receiver's IN and JMP pin
the same pad; OUT and IN shifting right; the transmit FIFO joined to 8
words, the receive FIFO to 8. One bit is 8 cycles. The words are the job's:
a byte in bits 0 to 7, bit 8 set on every byte of a frame but the last
(src/bridge_wire.cpp).

Checked, for frames of 1, 2, 21 and 64 bytes:

  - each byte on the pad: a start bit 8 cycles low, the data bits least
    significant first, 8 cycles each, a stop bit at least 8 cycles high;
  - the pad driven from the first start bit to the end of the last stop
    bit, 8 cycles into it, and released then, with no cycle released in
    between, also when the next byte comes late;
  - 81 cycles from one start bit to the next while the FIFO holds the next
    byte, 80 for the last;
  - every byte received back by the receiver, in order: the echo;
  - in the first cycle the transmitter waits on `pull` with its FIFO empty,
    every echo is in the receive FIFO and the receiver waits on its start
    bit: the condition under which the job counts a missing echo as a
    collision (bridgeLineIdle()).

And against a model ESC that drives the pad only while the transmitter has
let it go:

  - a reply of 6 bytes back to back starting 0.3 ms after the release, its
    bits as long as the line's, 2 % and 3 % longer and 2 % shorter, at 8
    phases of a cycle: every byte received, and the pad never driven by
    both. A control run with bits 3 % shorter loses bytes at every phase:
    pio_uart_rx samples the stop bit 9.6 bit times after the start edge and
    waits for the next start bit only after that, so a fast sender's next
    start edge is seen late, a little later each byte;
  - the ESC pulling the pad low inside a byte the bridge sends: the echo
    differs from the byte;
  - 9 bytes with nobody reading the receive FIFO: the first 8 kept and the
    receiver stalled on `push`, which sets FDEBUG's RXSTALL bit on the chip.

What the model holds to the datasheet: the encoding and behaviour of PULL,
OUT, SET, JMP (X-- and PIN), WAIT on a pin, IN, PUSH and IRQ; side-set at
issue, also when the instruction stalls; delays after the instruction;
wrap. An instruction reads the pad as the previous cycle left it: the
2-flip-flop input synchroniser takes 2 system clocks, 1/488 of a state
machine cycle at 150 MHz. It is a model: no analogue behaviour, no rise
time, the ESC's edges sampled at whole cycles; when both drive, a low
wins, which on the chip is a contention of unknown outcome. That the pad
reads its own output while driving it is the pad's design, not measured.
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pio_sdio_emu as emu  # noqa: E402

TX_SRC = emu.ROOT / "src" / "bridge_line.pio"
RX_SRC = emu.ROOT / "src" / "pio_uart_rx.pio"
FIFO = 8                  # both FIFOs joined
BIT = 8                   # cycles a bit
KEEP = 0x100              # bit 8: the pad stays driven after the stop bit
ESC_DELAY = 0.3e-3 * 19200 * BIT   # 0.3 ms in cycles


class SM:
    """One state machine: its program at an offset, registers and FIFOs."""

    def __init__(self, prog, offset):
        self.prog, self.off = prog, offset
        self.code = [w if (w >> 13) != emu.OP_JMP else (w & ~31) | ((w + offset) & 31)
                     for w in prog.words]
        self.wrap_bottom = offset + prog.wrap_target
        self.wrap_top = offset + prog.wrap
        self.ss_bits = prog.side_count + prog.side_opt
        self.pc = offset
        self.x = self.isr = self.osr = 0
        self.isr_cnt = 0
        self.delay = 0
        self.txf, self.rxf = [], []
        self.stalled = False
        self.rx_stall = False     # FDEBUG.RXSTALL
        self.irqs = 0


class Line:
    """The pad, the transmitter and the receiver, and a model ESC."""

    def __init__(self, tx_prog, rx_prog):
        self.tx = SM(tx_prog, 0)
        self.rx = SM(rx_prog, len(tx_prog.words))
        self.cycle = 0
        self.val = 1              # the output latch, high from the bind
        self.dir = 0              # an input from the bind
        self.esc = []             # (from, to, level): the ESC drives level
        self.conflicts = 0
        self.trace = []           # (pad, driven) per cycle

    def esc_level(self, c):
        for a, b, lev in self.esc:
            if a <= c < b:
                return lev
        return None

    def pad(self, c):
        e = self.esc_level(c)
        if self.dir and e is not None:
            self.conflicts += 1
            return self.val & e
        if self.dir:
            return self.val
        return 1 if e is None else e

    def step(self):
        p = self.trace[-1][0] if self.trace else 1   # as the last cycle left it
        for sm in (self.tx, self.rx):
            if sm.delay:
                sm.delay -= 1
            else:
                self.execute(sm, sm.code[sm.pc - sm.off], p)
        self.trace.append((self.pad(self.cycle), self.dir))
        self.cycle += 1

    def execute(self, sm, w, p):
        op = w >> 13
        field = (w >> 8) & 31
        delay = field & ((1 << (5 - sm.ss_bits)) - 1)
        if sm.ss_bits and (not sm.prog.side_opt or field >> 4):
            self.dir = (field >> (5 - sm.ss_bits)) & 1     # side-set pindirs, 1 pin
        a, idx = (w >> 5) & 7, w & 31
        jumped = stalled = False
        if op == emu.OP_JMP:
            take = {0: True, 2: sm.x != 0, 6: p == 1}.get(a)
            if take is None:
                raise emu.Fail(f"jmp condition {a} is not modelled")
            if a == 2:
                sm.x = (sm.x - 1) & 0xFFFFFFFF
            if take:
                sm.pc, jumped = idx, True
        elif op == emu.OP_WAIT:
            if a & 3 != 1 or idx:
                raise emu.Fail("only wait on pin 0 is modelled")
            stalled = p != (w >> 7) & 1
        elif op == emu.OP_IN:
            if a != 0 or idx != 1:
                raise emu.Fail("only in pins, 1 is modelled")
            sm.isr = (sm.isr >> 1) | (p << 31)             # shift right
            sm.isr_cnt += 1
        elif op == emu.OP_OUT:
            if idx != 1 or a not in (0, 4):
                raise emu.Fail("only out pins, 1 and out pindirs, 1 are modelled")
            v, sm.osr = sm.osr & 1, sm.osr >> 1           # shift right
            if a == 0:
                self.val = v
            else:
                self.dir = v
        elif op == emu.OP_PUSHPULL:
            pull, block = (w >> 7) & 1, (w >> 5) & 1
            if (w >> 6) & 1 or not block:
                raise emu.Fail("only blocking push and pull are modelled")
            if pull:
                if sm.txf:
                    sm.osr = sm.txf.pop(0)
                else:
                    stalled = True
            elif len(sm.rxf) >= FIFO:
                stalled = sm.rx_stall = True
            else:
                sm.rxf.append(sm.isr)
                sm.isr, sm.isr_cnt = 0, 0
        elif op == emu.OP_SET:
            if a == 0:
                self.val = idx & 1
            elif a == 1:
                sm.x = idx
            else:
                raise emu.Fail(f"set destination {a} is not modelled")
        elif op == emu.OP_IRQ:
            sm.irqs += 1
        else:
            raise emu.Fail(f"instruction {w:#06x} is not modelled")
        sm.stalled = stalled
        if stalled:
            return
        sm.delay = delay
        if not jumped:
            sm.pc = sm.wrap_bottom if sm.pc == sm.wrap_top else sm.pc + 1

    def tx_idle(self):
        """bridgeLineIdle()'s transmit half: FIFO empty, stalled on `pull`."""
        return not self.tx.txf and self.tx.pc == self.tx.off and self.tx.stalled

    def rx_waiting(self):
        return self.rx.pc == self.rx.off


def words_of(frame):
    return [b | (KEEP if i < len(frame) - 1 else 0) for i, b in enumerate(frame)]


def decode(trace, start, end):
    """UART bytes on the pad between cycles: [(start cycle, byte, stop length)]."""
    out, c = [], start
    while c < end:
        if trace[c][0] == 0:
            low = 0
            while c + low < end and trace[c + low][0] == 0 and low < BIT:
                low += 1
            byte = 0
            for i in range(8):
                lev = {trace[c + BIT * (1 + i) + k][0] for k in range(BIT)}
                if len(lev) != 1:
                    return out + [(c, None, 0)]
                byte |= lev.pop() << i
            s = c + 9 * BIT
            stop = 0
            while s + stop < len(trace) and trace[s + stop][0] == 1:
                stop += 1
            out.append((c, byte if low == BIT else None, stop))
            c = s + min(stop, BIT)
        else:
            c += 1
    return out


def run_frame(progs, frame, late_at=None, late_by=0, ln=None):
    """Send one frame, refilling the FIFO as it empties, and run until 4 byte
    times after the transmitter is idle. @p late_at holds the byte of that
    index back until the transmitter has waited @p late_by cycles for it,
    stalled on `pull` with the pad driven.
    Returns the failures, the line and the first idle cycle."""
    fails = []
    ln = ln or Line(progs["bridge_tx"], progs["pio_uart_rx"])
    words = words_of(frame)
    pending, put, echoes = list(words), 0, []
    waited = 0
    first_idle = None
    for _ in range(40):
        ln.step()                                   # idle before the frame
    t0 = ln.cycle
    while first_idle is None or ln.cycle < first_idle + 40 * BIT:
        if ln.cycle > t0 + 100 * BIT * (len(frame) + 4):
            fails.append("the transmitter never went idle")
            break
        if pending and len(ln.tx.txf) < FIFO:
            if put != late_at:
                ln.tx.txf.append(pending.pop(0))
                put += 1
            elif not ln.tx.txf and ln.tx.stalled:
                waited += 1
                if waited > late_by:
                    ln.tx.txf.append(pending.pop(0))
                    put += 1
        ln.step()
        while first_idle is None and ln.rx.rxf:
            echoes.append((ln.cycle, ln.rx.rxf.pop(0) >> 24))
        if not pending and first_idle is None and ln.tx_idle():
            first_idle = ln.cycle
            got = len(echoes)
            if got != len(frame) or not ln.rx_waiting():
                fails.append(f"at the first idle cycle {got} of {len(frame)} echoes, "
                             f"receiver {'waiting' if ln.rx_waiting() else 'in a byte'}")
    tr = ln.trace
    bytes_ = decode(tr, t0, first_idle or len(tr))
    sent = [b for _, b, _ in bytes_]
    if sent != list(frame):
        fails.append(f"on the pad {sent}, sent {list(frame)}")
    if [b for _, b in echoes] != list(frame):
        fails.append(f"echo {[b for _, b in echoes]}")
    starts = [c for c, _, _ in bytes_]
    gaps = [b - a for a, b in zip(starts, starts[1:])]
    for i, g in enumerate(gaps):
        if i + 1 == late_at:
            # A pull that waited takes the word the cycle after it arrives.
            if g != 83 + late_by:
                fails.append(f"the late byte {i + 1} came {g} cycles on, "
                             f"not {83 + late_by}")
        elif g != 82:
            fails.append(f"bytes {i} and {i + 1} {g} cycles apart")
    if starts:
        first, last = starts[0], starts[-1]
        driven = [c for c in range(t0, len(tr)) if tr[c][1]]
        end = last + 10 * BIT                        # the last stop bit's end
        if not driven or driven[0] != first or driven[-1] != end - 1 or \
                len(driven) != end - first:
            fails.append(f"driven from {driven[:1]} to {driven[-1:]}, "
                         f"{len(driven)} cycles; the frame {first} to {end - 1}")
        if any(stop < BIT for _, _, stop in bytes_):
            fails.append("a stop bit shorter than 8 cycles")
    if ln.conflicts:
        fails.append(f"{ln.conflicts} cycles driven by both")
    return fails, ln, first_idle


def run_reply(progs, frame, reply, rate, phase):
    """A frame, then the ESC's reply @p ESC_DELAY after the release."""
    ln = Line(progs["bridge_tx"], progs["pio_uart_rx"])
    rel = [None]
    orig = ln.step

    def step():
        orig()
        if rel[0] is None and len(ln.trace) > 1 and ln.trace[-2][1] and not ln.trace[-1][1]:
            rel[0] = ln.cycle - 1                   # the first cycle let go
            t = rel[0] + ESC_DELAY + phase
            bit = BIT * rate
            for b in reply:
                ln.esc.append((t, t + bit, 0))
                for i in range(8):
                    ln.esc.append((t + bit * (1 + i), t + bit * (2 + i), (b >> i) & 1))
                t += 10 * bit
            ln.reply_end = t
    ln.step = step
    fails, ln, idle = run_frame(progs, frame, ln=ln)
    while ln.cycle < ln.reply_end + 4 * BIT:
        ln.step()
    # run_frame() takes the echoes up to its first idle cycle and leaves the
    # reply in the receive FIFO, which holds 8 bytes.
    got = [w >> 24 for w in ln.rx.rxf]
    if got != list(reply):
        fails.append(f"reply received {got}, sent {list(reply)}")
    if ln.conflicts:
        fails.append(f"{ln.conflicts} cycles driven by both")
    return fails


def run_collision(progs):
    """The ESC pulls the pad low through bits 2 to 4 of the second byte."""
    ln = Line(progs["bridge_tx"], progs["pio_uart_rx"])
    frame = [0xFF, 0xFF, 0xFF]
    ln.tx.txf.extend(words_of(frame))
    start = 1 + 82                                  # the second start bit
    ln.esc.append((start + 3 * BIT, start + 6 * BIT, 0))
    got = []
    for _ in range(4 * 82):
        ln.step()
        while ln.rx.rxf:
            got.append(ln.rx.rxf.pop(0) >> 24)
    fails = []
    if len(got) != 3 or got[0] != 0xFF or got[1] == 0xFF or got[2] != 0xFF:
        fails.append(f"echo {got}: the second byte should differ")
    if not ln.conflicts:
        fails.append("no contention counted")
    return fails


def run_overflow(progs):
    """Nine bytes, nobody reading the receive FIFO."""
    ln = Line(progs["bridge_tx"], progs["pio_uart_rx"])
    frame = list(range(0x30, 0x39))
    words = words_of(frame)
    while words or not ln.tx_idle():
        if words and len(ln.tx.txf) < FIFO:
            ln.tx.txf.append(words.pop(0))
        ln.step()
    got = [w >> 24 for w in ln.rx.rxf]
    fails = []
    if got != frame[:8] or not ln.rx.rx_stall:
        fails.append(f"FIFO {got}, RXSTALL {ln.rx.rx_stall}")
    return fails


def main():
    args = sys.argv[1:]
    verbose = "-v" in args
    tx = emu.assemble(TX_SRC, expect=("bridge_tx",))
    rx = emu.assemble(RX_SRC, expect=("pio_uart_rx",))
    if "--pioasm" in args:
        pioasm = args[args.index("--pioasm") + 1]
        emu.cross_check(tx, pioasm, TX_SRC)
        emu.cross_check(rx, pioasm, RX_SRC)
        print("bridge_tx, pio_uart_rx: pioasm assembles the same words")
    progs = {**tx, **rx}
    words = len(tx["bridge_tx"].words) + len(rx["pio_uart_rx"].words)
    if words > 32:
        print(f"{words} words do not fit one PIO block")
        return 1
    runs = fails = 0

    def report(name, f):
        nonlocal runs, fails
        runs += 1
        fails += bool(f)
        if verbose or f:
            print(f"{name}: {'pass' if not f else 'FAIL ' + '; '.join(f)}")

    init = [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0D, ord('B'), ord('L'),
            ord('H'), ord('e'), ord('l'), ord('i'), 0xF4, 0x7D]
    frames = {"1 byte": [0x5A], "2 bytes": [0xFD, 0x00], "21 bytes": init,
              "64 bytes": [(i * 37 + 11) & 0xFF for i in range(64)]}
    for name, frame in frames.items():
        report(f"frame of {name}", run_frame(progs, frame)[0])
    for at, by in ((1, 3), (5, 200), (20, 30)):
        f, _, _ = run_frame(progs, init, late_at=at, late_by=by)
        report(f"21 bytes, byte {at} {by} cycles late", f)
    reply = [0x30, 0xC1, 0x55, 0xAA, 0x00, 0xFF]
    for rate in (1.0, 1.02, 1.03, 0.98):
        for k in range(8):
            f = run_reply(progs, [0xFD, 0x00], reply, rate, k / 8)
            report(f"reply with bits {rate:.2f} long, phase {k}/8", f)
    # The control: a sender 3 % fast, back to back, loses bytes in this
    # receiver (its stop bit is sampled 9.6 bit times in), so the reply runs
    # can fail.
    lost = sum(bool(run_reply(progs, [0xFD, 0x00], reply, 0.97, k / 8)) for k in range(8))
    report("control: bits 0.97 long lose bytes at 8 of 8 phases",
           [] if lost == 8 else [f"{lost} of 8 phases lost bytes"])
    report("collision", run_collision(progs))
    report("receive FIFO full", run_overflow(progs))
    print(f"bridge line: {words} words ({len(tx['bridge_tx'].words)} + "
          f"{len(rx['pio_uart_rx'].words)}), {runs} runs, "
          f"{'all pass' if not fails else f'{fails} fail'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
