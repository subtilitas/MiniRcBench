#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Cycle-level check of the backlight PIO program, src/backlight.pio.

    python3 test/pio_backlight_emu.py        # exit 1 on any failure
    python3 test/pio_backlight_emu.py -v     # one line per level
    python3 test/pio_backlight_emu.py --pioasm PATH   # also hold the words to pioasm

Assembles the program with the assembler of pio_sdio_emu.py and runs it one
state machine cycle at a time, set up as st7789Init() sets it up: 255 into
the ISR through the OSR, then a level word written as st7789SetBacklight()
writes it, L - 1, and 0xFFFFFFFF for 0. For every level from 0 to 255 it
checks, over 3 periods after the first:

  - the period: 516 cycles at a level above 0, 515 at level 0;
  - the high time: 2 x L cycles, so the duty is L / 258 of the period, within
    1 % of L / 256;
  - level 0 never high, and every level above 0 high once per period.

A level written while the program runs takes effect at the next period: the
check writes each level into the running machine, not into a fresh one.
"""

import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import pio_sdio_emu as emu  # noqa: E402

SRC = emu.ROOT / "src" / "backlight.pio"
PERIOD_ON, PERIOD_OFF = 516, 515


class Machine:
    """One state machine running the backlight program, its pin and TX FIFO."""

    def __init__(self, prog):
        self.w = prog.words
        self.wrap_bottom, self.wrap_top = prog.wrap_target, prog.wrap
        self.pc, self.x, self.y, self.isr, self.osr = 0, 0, 0, 0, 0
        self.pin = 0
        self.fifo = []

    def side(self, w):
        # .side_set 1 opt: bit 12 enables, bit 11 is the value.
        if (w >> 12) & 1:
            self.pin = (w >> 11) & 1

    def execute(self, w):
        """Run one instruction word; returns the next pc, or None to advance."""
        op = w >> 13
        self.side(w)
        if op == emu.OP_JMP:
            cond, addr = (w >> 5) & 7, w & 31
            if cond == 0:
                return addr
            if cond == emu.JMP_COND["y--"]:
                take = self.y != 0
                self.y = (self.y - 1) & 0xFFFFFFFF
                return addr if take else None
            if cond == emu.JMP_COND["x!=y"]:
                return addr if self.x != self.y else None
            raise emu.Fail(f"jmp condition {cond} is not modelled")
        if op == emu.OP_PUSHPULL and (w >> 7) & 1:
            block = (w >> 5) & 1
            if self.fifo:
                self.osr = self.fifo.pop(0)
            elif block:
                raise emu.Fail("a blocking pull with an empty FIFO")
            else:
                self.osr = self.x   # pull noblock from an empty FIFO copies X
            return None
        if op == emu.OP_MOV:
            dst, src, mop = (w >> 5) & 7, w & 7, (w >> 3) & 3
            if mop:
                raise emu.Fail("mov with an operation is not modelled")
            val = {emu.MOV_SRC["x"]: self.x, emu.MOV_SRC["y"]: self.y,
                   emu.MOV_SRC["isr"]: self.isr, emu.MOV_SRC["osr"]: self.osr}[src]
            if dst == emu.MOV_DST["x"]:
                self.x = val
            elif dst == emu.MOV_DST["y"]:
                self.y = val
            elif dst == emu.MOV_DST["isr"]:
                self.isr = val
            elif dst == emu.MOV_DST["osr"]:
                self.osr = val
            elif not (dst == emu.MOV_DST["y"] and src == emu.MOV_SRC["y"]):
                raise emu.Fail(f"mov destination {dst} is not modelled")
            return None
        raise emu.Fail(f"instruction {w:#06x} is not modelled")

    def step(self):
        nxt = self.execute(self.w[self.pc])
        if nxt is not None:
            self.pc = nxt
        elif self.pc == self.wrap_top:
            self.pc = self.wrap_bottom
        else:
            self.pc += 1


def level_word(level):
    return level - 1 if level else 0xFFFFFFFF


def measure(m, periods):
    """Run to the start of a period, then @p periods periods: (lengths, highs)."""
    while m.pc != m.wrap_bottom:
        m.step()
    lengths, highs = [], []
    for _ in range(periods):
        n = high = 0
        while True:
            m.step()
            n += 1
            high += m.pin
            if m.pc == m.wrap_bottom:
                break
        lengths.append(n)
        highs.append(high)
    return lengths, highs


def main():
    args = sys.argv[1:]
    verbose = "-v" in args
    progs = emu.assemble(SRC, expect=("backlight",))
    if "--pioasm" in args:
        emu.cross_check(progs, args[args.index("--pioasm") + 1], SRC)
        print("backlight: pioasm assembles the same words")
    prog = progs["backlight"]
    m = Machine(prog)
    # st7789Init(): 255 into the ISR through the OSR.
    m.osr = 255
    m.isr = m.osr
    fails = 0
    for level in list(range(256)) + [0, 255, 128]:
        m.fifo = [level_word(level)]
        measure(m, 1)                      # the period that takes the word
        lengths, highs = measure(m, 3)
        want_len = PERIOD_ON if level else PERIOD_OFF
        ok = all(n == want_len for n in lengths) and all(h == 2 * level for h in highs)
        duty = highs[0] / lengths[0]
        ok = ok and abs(duty - level / 256) <= 0.01
        if verbose or not ok:
            print(f"level {level:3}: period {lengths} high {highs} duty {duty:.4f}"
                  f"{'' if ok else '  FAIL'}")
        fails += not ok
    print(f"backlight: {len(prog.words)} words, 259 levels, "
          f"{'all pass' if not fails else f'{fails} fail'}")
    return 1 if fails else 0


if __name__ == "__main__":
    sys.exit(main())
