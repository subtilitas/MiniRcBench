#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Cycle-level test of the SDIO PIO programs against a model of an SD card.

Provenance: written for MiniRcBench from the PIO (programmable input/output)
chapter of the RP2350 datasheet and the SD Physical Layer Simplified
Specification v9.10. No third-party PIO emulator and no SD host driver source
was consulted.

    python3 test/pio_sdio_emu.py             # every run; exit 1 on any failure
    python3 test/pio_sdio_emu.py -v          # one line per run
    python3 test/pio_sdio_emu.py --quick     # 25 MHz only
    python3 test/pio_sdio_emu.py --pio FILE  # another copy of sd_pio.pio
    python3 test/pio_sdio_emu.py --pioasm build/pioasm-install/pioasm/pioasm --asm-only

What it does
------------
1. Assembles the two programs from src/sdio/sd_pio.pio itself, with the
   assembler below, which covers the part of the PIO assembly language the
   file uses and refuses the rest. Nothing here holds a copy of the programs.
   With --pioasm it also runs the SDK's pioasm on the same file and fails
   unless every instruction word, wrap point, public label and side-set
   setting matches.
2. Loads them where pio_add_program() puts them in an empty block (sdpio_cmd
   at 19, sdpio_dat at 0) and runs both state machines one system clock at a
   time at 150 MHz, configured as sdpio_claim() configures them.
3. Drives them through a host model that makes the calls of src/sdio/sd_pio.c
   in the same order: halts, forced instructions, FIFO (first in, first out)
   writes, the clock divider of busSetClock(). The FIFO words come from the
   packers in src/sdio/sd_proto.c, compiled with the host C compiler and
   called through ctypes, and sdp_resp48_check(), sdp_r2_check(),
   sdp_rx_check() and sdp_crc_status() from the same file judge the results.
4. Connects the pins to a card model that samples CMD and DAT on rising CLK
   edges and changes its outputs a set number of system clocks after falling
   edges (Figure 6-11 draws that order without numbers), with its own CRC7
   and CRC16 (4.5).

Each scenario runs at 400 kHz, 10 MHz and 25 MHz, with card output delays of
0, 1 and 2 system clocks, against a fast and a slow card. NCR counts the
clocks between a command's end bit and the response's start bit, NAC those
between a read command's end bit and the data's start bit: 2 and 2 for the
fast card, 64 and 100 for the slow one.

  - CMD0, no response: the frame reaches the card with a valid CRC7;
  - an R1 response (CMD13): the 48 bits, CRC7 and status;
  - an R2 response (CMD9): the 136 bits and the CSD (card-specific data);
  - CMD17: one block whose start bit arrives while the response is still on
    CMD for the fast card, data and CRC16 of all four lines;
  - CMD18 over 3 blocks, chosen so that the last CRC nibble of the first two
    holds DAT0 low;
  - CMD12 in the fourth block: R1, busy on DAT0 seen and released;
  - CMD24: one block written, the card's CRC16 matches, the CRC status token
    reads 010 (accepted, Figure 5-8), busy released.

A run also fails on:

  - a system clock in which the host and the card drive the same line;
  - a command bit or a write nibble that the card samples from a line the
    host does not drive, even when the pull-up holds the right level;
  - a host output that changes within 1 system clock (6.7 ns) of a rising
    CLK edge at the card, before or after it;
  - a line that IN or JMP PIN samples changing in the system clock the input
    synchroniser takes it, or in the next one.

A control run with a card output delay of 3 system clocks at 25 MHz puts each
change of CMD on the system clock in which the command program samples it,
and must fail; that shows that the runs can fail.

What the model holds to the datasheet
-------------------------------------
- Instruction encoding and the behaviour of JMP, WAIT (with the RP2350's
  JMPPIN source), IN, OUT, PUSH, PULL, MOV and SET; side-set at issue, also
  when the instruction stalls; delay cycles after it completes, not while it
  stalls; wrap; JMP addresses relocated by the load offset.
- The fractional clock divider in 1/256 steps.
- Shift left, autopull and autopush at 32 bits, FIFOs of 4 words each.
- GPIO inputs through a 2-flip-flop synchroniser: an instruction sees a pad
  as it was 2 system clocks before. An output written by an instruction
  reaches the pad in the next system clock.
- Forced instructions (pio_sm_exec), pio_sm_restart() and
  pio_sm_clear_fifos() as sd_pio.c uses them.

What it does not cover
----------------------
It is a model, not proof of hardware timing. It has no analogue behaviour:
no rise and fall times, slew, drive strength, ringing or crosstalk, and no
trace or connector delay. A pad changes in whole system clocks, and the
sampling margins above are 1 system clock wide; the simplified specification
carries no bus timing values to hold them to. A constant card delay of a whole
bus clock shifts every bit by one clock and reads as a longer NCR or NAC, so
it passes. The DMA (direct memory access) channels are ideal: they move a FIFO
word in the system clock it appears, with no bus contention. Nothing else
runs on the processor, so the time between bus calls is the time sd_pio.c
waits and nothing more. The card model is one card with the parameters above.
It does not model initialisation, which the host suite runs against its
simulated card, nor bus width switching, the high speed and UHS (ultra high
speed) modes, or errors a real card reports, and its busy counts clocks.
Input synchroniser metastability and RP2350 errata are not modelled. The host
model follows sd_pio.c by hand: a change to the order of its calls has to be
made here too.

Python 3 standard library and a C compiler ($CC, default cc).
"""

import argparse
import collections
import ctypes
import json
import os
import pathlib
import re
import subprocess
import sys
import tempfile

ROOT = pathlib.Path(__file__).resolve().parent.parent
PIO_SRC = ROOT / "src" / "sdio" / "sd_pio.pio"

SYS_HZ = 150_000_000
CLK, CMD, D0 = 19, 20, 21          # PIN_SD_SCK, PIN_SD_CMD, PIN_SD_D0 of the board
DAT_MASK = 0xF << D0
PULLED_UP = (1 << CMD) | DAT_MASK  # sdpio_claim() pulls CMD and DAT up
SD_PINS = (CMD, D0, D0 + 1, D0 + 2, D0 + 3)
SETUP = 1                          # system clocks of setup the card needs
HOLD = 1                           # system clocks of hold the card needs


class Fail(Exception):
    """A run, or the assembly, fails."""


# =================================================================== assembler
#
# The PIO assembly language as the RP2350 datasheet describes it, for the
# instructions and directives sd_pio.pio uses. Anything else is an error,
# not a guess.

OP_JMP, OP_WAIT, OP_IN, OP_OUT, OP_PUSHPULL, OP_MOV, OP_IRQ, OP_SET = range(8)
JMP_COND = {"": 0, "!x": 1, "x--": 2, "!y": 3, "y--": 4, "x!=y": 5, "pin": 6, "!osre": 7}
WAIT_SRC = {"gpio": 0, "pin": 1, "jmppin": 3}
IN_SRC = {"pins": 0, "x": 1, "y": 2, "null": 3, "isr": 6, "osr": 7}
OUT_DST = {"pins": 0, "x": 1, "y": 2, "null": 3, "pindirs": 4, "pc": 5, "isr": 6, "exec": 7}
MOV_DST = {"pins": 0, "x": 1, "y": 2, "pindirs": 3, "exec": 4, "pc": 5, "isr": 6, "osr": 7}
MOV_SRC = {"pins": 0, "x": 1, "y": 2, "null": 3, "status": 5, "isr": 6, "osr": 7}
SET_DST = {"pins": 0, "x": 1, "y": 2, "pindirs": 4}


class Program:
    def __init__(self, name):
        self.name = name
        self.words = []
        self.labels = {}
        self.public = {}
        self.wrap_target = None
        self.wrap = None
        self.side_count = 0      # side-set bits, without the enable bit
        self.side_opt = False
        self.side_pindirs = False
        self.fixups = []         # (instruction index, label, line number)


def number(text, line):
    try:
        return int(text, 0)
    except ValueError:
        raise Fail(f"{PIO_SRC.name}:{line}: '{text}' is not a number")


def encode(prog, mnem, ops, side, delay, line):
    ss_bits = prog.side_count + (1 if prog.side_opt else 0)
    if side is None and prog.side_count and not prog.side_opt:
        raise Fail(f"line {line}: the program's side-set is not optional; this needs a side value")
    if side is not None and not prog.side_count:
        raise Fail(f"line {line}: side value in a program without .side_set")
    if delay > (1 << (5 - ss_bits)) - 1:
        raise Fail(f"line {line}: delay {delay} does not fit")
    field = delay
    if side is not None:
        if side >= 1 << prog.side_count:
            raise Fail(f"line {line}: side value {side} does not fit")
        field |= side << (5 - ss_bits)
        if prog.side_opt:
            field |= 1 << 4
    ds = field << 8
    words = ops.replace(",", " ").split()

    def count(s):
        n = number(s, line)
        if not 1 <= n <= 32:
            raise Fail(f"line {line}: bit count {n} outside 1..32")
        return n & 31

    if mnem == "nop":
        return (OP_MOV << 13) | ds | (MOV_DST["y"] << 5) | MOV_SRC["y"]
    if mnem == "jmp":
        cond = words[0] if len(words) == 2 else ""
        if cond not in JMP_COND:
            raise Fail(f"line {line}: jmp condition '{cond}'")
        prog.fixups.append((len(prog.words), words[-1], line))
        return (OP_JMP << 13) | ds | (JMP_COND[cond] << 5)
    if mnem == "wait":
        pol = number(words[0], line)
        src = words[1]
        if src not in WAIT_SRC:
            raise Fail(f"line {line}: wait source '{src}' is not modelled")
        rest = "".join(words[2:]).lstrip("+")
        index = number(rest, line) if rest else 0
        return (OP_WAIT << 13) | ds | ((pol & 1) << 7) | (WAIT_SRC[src] << 5) | (index & 31)
    if mnem == "in":
        return (OP_IN << 13) | ds | (IN_SRC[words[0]] << 5) | count(words[1])
    if mnem == "out":
        return (OP_OUT << 13) | ds | (OUT_DST[words[0]] << 5) | count(words[1])
    if mnem in ("push", "pull"):
        flag = "iffull" if mnem == "push" else "ifempty"
        block = "noblock" not in words
        return ((OP_PUSHPULL << 13) | ds | ((mnem == "pull") << 7) | ((flag in words) << 6)
                | (block << 5))
    if mnem == "mov":
        dst, src = words[0], words[1]
        op = 0
        if src.startswith(("!", "~")):
            op, src = 1, src[1:]
        elif src.startswith("::"):
            op, src = 2, src[2:]
        return (OP_MOV << 13) | ds | (MOV_DST[dst] << 5) | (op << 3) | MOV_SRC[src]
    if mnem == "set":
        v = number(words[1], line)
        if not 0 <= v <= 31:
            raise Fail(f"line {line}: set value {v} outside 0..31")
        return (OP_SET << 13) | ds | (SET_DST[words[0]] << 5) | v
    raise Fail(f"line {line}: instruction '{mnem}' is not modelled")


def assemble(path):
    """Programs of a .pio file, by name."""
    progs, prog = {}, None
    for n, raw in enumerate(path.read_text().splitlines(), 1):
        text = re.split(r";|//", raw, maxsplit=1)[0].strip()
        if not text:
            continue
        if text.startswith("."):
            d = text.split()
            if d[0] == ".program":
                prog = progs[d[1]] = Program(d[1])
            elif d[0] == ".pio_version":
                if number(d[1], n) > 1:
                    raise Fail(f"line {n}: PIO version {d[1]}")
            elif prog is None:
                raise Fail(f"line {n}: {d[0]} outside a program")
            elif d[0] == ".side_set":
                prog.side_count = number(d[1], n)
                prog.side_opt = "opt" in d[2:]
                prog.side_pindirs = "pindirs" in d[2:]
            elif d[0] == ".wrap_target":
                prog.wrap_target = len(prog.words)
            elif d[0] == ".wrap":
                prog.wrap = len(prog.words) - 1
            else:
                raise Fail(f"line {n}: directive {d[0]} is not modelled")
            continue
        m = re.match(r"^(public\s+)?([A-Za-z_]\w*):\s*(.*)$", text)
        if m:
            prog.labels[m.group(2)] = len(prog.words)
            if m.group(1):
                prog.public[m.group(2)] = len(prog.words)
            text = m.group(3).strip()
            if not text:
                continue
        delay = 0
        m = re.search(r"\[\s*(\w+)\s*\]\s*$", text)
        if m:
            delay, text = number(m.group(1), n), text[:m.start()].strip()
        side = None
        m = re.search(r"\bside(?:set)?\s+(\w+)\s*$", text)
        if m:
            side, text = number(m.group(1), n), text[:m.start()].strip()
        mnem, _, ops = text.partition(" ")
        prog.words.append(encode(prog, mnem.lower(), ops.strip().lower(), side, delay, n))
    for p in progs.values():
        for i, label, n in p.fixups:
            target = p.labels.get(label)
            if target is None:
                target = number(label, n)
            p.words[i] |= target & 31
        if p.wrap_target is None:
            p.wrap_target = 0
        if p.wrap is None:
            p.wrap = len(p.words) - 1
    if set(progs) != {"sdpio_cmd", "sdpio_dat"}:
        raise Fail(f"{path} defines {sorted(progs)}, not sdpio_cmd and sdpio_dat")
    if sum(len(p.words) for p in progs.values()) > 32:
        raise Fail("the two programs do not fit one PIO block")
    return progs


def cross_check(progs, pioasm, path):
    """Fail unless pioasm encodes the file exactly as assemble() does."""
    out = subprocess.run([pioasm, "-o", "json", str(path)], check=True,
                         capture_output=True, text=True).stdout
    ref = {p["name"]: p for p in json.loads(out)["programs"]}
    for name, p in progs.items():
        r = ref.get(name)
        if r is None:
            raise Fail(f"pioasm has no program {name}")
        words = [int(i["hex"], 16) for i in r["instructions"]]
        got = (words, r["wrapTarget"], r["wrap"], r["publicLabels"],
               (r["sideset"]["size"], r["sideset"]["optional"], r["sideset"]["pindirs"]))
        mine = (p.words, p.wrap_target, p.wrap, p.public,
                (p.side_count + p.side_opt, p.side_opt, p.side_pindirs))
        if got != mine:
            raise Fail(f"{name}: pioasm {got} differs from this assembler {mine}")


# ================================================================== PIO model

class SM:
    """One state machine and its configuration."""

    def __init__(self, prog, offset):
        self.prog, self.off = prog, offset
        self.code = [w if (w >> 13) != OP_JMP else (w & ~31) | ((w + offset) & 31)
                     for w in prog.words]   # JMP targets relocated
        self.wrap_bottom = offset + prog.wrap_target
        self.wrap_top = offset + prog.wrap
        self.ss_bits = prog.side_count + prog.side_opt
        self.side_base = self.out_base = self.set_base = self.in_base = self.jmp_pin = 0
        self.out_count = self.set_count = 1
        self.div = 256
        self.acc = 0
        self.enabled = False
        self.pc = offset
        self.x = self.y = self.isr = self.osr = 0
        self.isr_cnt, self.osr_cnt = 0, 32
        self.delay = 0
        self.txf, self.rxf = collections.deque(), collections.deque()
        self.wait_stall = False   # stalled on a WAIT at its last instruction

    def label(self, name):
        return self.off + self.prog.public[name]

    def restart(self):
        """pio_sm_restart(): shift counters, delay and stall cleared."""
        self.isr_cnt, self.osr_cnt, self.delay, self.wait_stall = 0, 32, 0, False

    def clear_fifos(self):
        self.txf.clear()
        self.rxf.clear()


class Emu:
    """The pads, one PIO block's two state machines, the card and two DMA channels."""

    def __init__(self, cmd_sm, dat_sm, card):
        self.sms = (cmd_sm, dat_sm)
        self.card = card
        card.emu = self
        self.cycle = 0
        self.out_val = 0          # PIO output latches, one bit per GPIO
        self.out_dir = 0
        self.dirty = False        # outputs changed since the pads were last resolved
        self.card_val = self.card_dir = 0
        self.pending = []         # card output changes: (cycle, mask, value or None)
        self.pad = PULLED_UP
        self.p0 = self.p1 = self.p2 = self.pad   # pads at this, the last and the cycle before
        self.last_change = {}
        self.changes = {pin: collections.deque(maxlen=3) for pin in SD_PINS}
        self.last_rise = -10
        self.conflicts = 0
        self.timing = []
        self.rx_dma = None        # [words wanted, words received]
        self.tx_dma = None        # deque of words to feed

    # -- pins written by the state machines and the host
    def write_pins(self, base, count, value, dirs=False):
        mask = ((1 << count) - 1) << base & 0xFFFFFFFF
        bits = (value << base) & mask
        if dirs:
            new = (self.out_dir & ~mask) | bits
            if new != self.out_dir:
                self.out_dir, self.dirty = new, True
        else:
            new = (self.out_val & ~mask) | bits
            if new != self.out_val:
                self.out_val, self.dirty = new, True

    def card_drive(self, at, mask, value):
        self.pending.append((at, mask, value))

    def resolve(self):
        c = self.cycle
        if self.pending:
            keep = []
            for ev in self.pending:
                if ev[0] <= c:
                    _, mask, value = ev
                    if value is None:
                        self.card_dir &= ~mask
                    else:
                        self.card_dir |= mask
                        self.card_val = (self.card_val & ~mask) | (value & mask)
                else:
                    keep.append(ev)
            self.pending = keep
        hd, cd = self.out_dir, self.card_dir
        if hd & cd:
            self.conflicts += 1
        return ((self.out_val & hd) | (self.card_val & cd & ~hd)
                | (PULLED_UP & ~hd & ~cd))

    def step(self):
        self.cycle += 1
        c = self.cycle
        clk_was = (self.pad >> CLK) & 1
        clk = (self.out_val >> CLK) & 1 if (self.out_dir >> CLK) & 1 else 0
        if clk_was and not clk:
            self.card.fall(c)       # may schedule a change for this very cycle
        pad = self.resolve()
        self.dirty = False
        changed = pad ^ self.pad
        if changed:
            sampled = self.out_dir & ((1 << CMD) | DAT_MASK)
            if changed & sampled and c - self.last_rise <= HOLD and (pad >> CLK) & 1:
                self.timing.append(f"hold: {changed & sampled:#x} changed at {c}, "
                                   f"{c - self.last_rise} after a rising edge")
            for pin in SD_PINS:
                if changed >> pin & 1:
                    self.last_change[pin] = c
                    self.changes[pin].append(c)
        self.pad = pad
        if not clk_was and clk:
            self.last_rise = c
            for pin in SD_PINS:
                if (self.out_dir >> pin & 1) and c - self.last_change.get(pin, -99) < SETUP:
                    self.timing.append(f"setup: GPIO{pin} changed at {c}, the rising edge")
            self.card.rise(c, pad)
        self.p2, self.p1, self.p0 = self.p1, self.p0, pad
        for sm in self.sms:
            if sm.enabled:
                sm.acc += 256
                if sm.acc >= sm.div:
                    sm.acc -= sm.div
                    if sm.delay:
                        sm.delay -= 1
                    else:
                        self.execute(sm, sm.code[sm.pc - sm.off])
        if self.rx_dma is not None:
            rxf, want = self.sms[1].rxf, self.rx_dma
            while rxf and len(want[1]) < want[0]:
                want[1].append(rxf.popleft())
        if self.tx_dma:
            txf = self.sms[1].txf
            while self.tx_dma and len(txf) < 4:
                txf.append(self.tx_dma.popleft())

    def quiet_for(self, limit):
        """System clocks that can pass with nothing changing, at most @p limit."""
        if self.pending or self.dirty or not (self.p2 == self.p1 == self.p0 == self.pad):
            return 0
        dat = self.sms[1]
        if self.tx_dma and len(dat.txf) < 4:
            return 0
        if self.rx_dma is not None and dat.rxf and len(self.rx_dma[1]) < self.rx_dma[0]:
            return 0
        n = limit
        for sm in self.sms:
            if not sm.enabled:
                continue
            if sm.wait_stall and not sm.delay:
                continue            # its inputs are steady: it stays stalled
            n = min(n, (sm.div - sm.acc + 255) // 256 - 1)
        return max(n, 0)

    def advance(self, n):
        self.cycle += n
        for sm in self.sms:
            if sm.enabled:
                total = sm.acc + 256 * n
                sm.acc = total % sm.div

    def run_until(self, cond, limit):
        """Run until cond() or @p limit system clocks; return cond()."""
        end = self.cycle + limit
        while self.cycle < end:
            if cond():
                return True
            q = self.quiet_for(end - self.cycle)
            if q:
                self.advance(q)
            else:
                self.step()
        return cond()

    def run(self, n):
        self.run_until(lambda: False, n)

    def sampled(self, pins):
        """The host's input: a line an instruction samples must not have
        changed in the system clock the synchroniser took it, nor in the next."""
        s = self.cycle - 2
        for pin in pins:
            if pin in self.changes and any(s <= t <= s + 1 for t in self.changes[pin]):
                self.timing.append(f"host sample: GPIO{pin} changed within a system "
                                   f"clock of the sample at {s}")

    # -- one instruction
    def execute(self, sm, w, forced=False):
        op = w >> 13
        field = (w >> 8) & 31
        delay = field & ((1 << (5 - sm.ss_bits)) - 1)
        if sm.ss_bits:
            opt = sm.prog.side_opt
            if not opt or field >> 4:
                side = (field >> (5 - sm.ss_bits)) & ((1 << sm.prog.side_count) - 1)
                self.write_pins(sm.side_base, sm.prog.side_count, side, sm.prog.side_pindirs)
        a = (w >> 5) & 7
        idx = w & 31
        sync = self.p2
        jumped = stalled = False
        if op == OP_JMP:
            if a == 0:
                take = True
            elif a == 1:
                take = sm.x == 0
            elif a == 2:
                take = sm.x != 0
                sm.x = (sm.x - 1) & 0xFFFFFFFF
            elif a == 3:
                take = sm.y == 0
            elif a == 4:
                take = sm.y != 0
                sm.y = (sm.y - 1) & 0xFFFFFFFF
            elif a == 5:
                take = sm.x != sm.y
            elif a == 6:
                take = (sync >> sm.jmp_pin) & 1 == 1
                if not forced:
                    self.sampled((sm.jmp_pin,))
            else:
                take = sm.osr_cnt < 32
            if take:
                sm.pc, jumped = idx, True
        elif op == OP_WAIT:
            pol, src = (w >> 7) & 1, (w >> 5) & 3
            if src == 0:
                pin = idx
            elif src == 1:
                pin = (sm.in_base + idx) & 31
            elif src == 3:
                pin = (sm.jmp_pin + idx) & 31
            else:
                raise Fail("WAIT IRQ is not modelled")
            stalled = (sync >> pin) & 1 != pol
        elif op == OP_IN:
            n = idx or 32
            if sm.isr_cnt + n >= 32 and len(sm.rxf) >= 4:
                stalled = True      # autopush would find the RX FIFO full
            else:
                if a == 0:
                    v = (((sync | (sync << 32)) >> sm.in_base) & 0xFFFFFFFF)
                    if not forced:
                        self.sampled([(sm.in_base + i) & 31 for i in range(n)])
                elif a == 1:
                    v = sm.x
                elif a == 2:
                    v = sm.y
                elif a == 3:
                    v = 0
                else:
                    raise Fail(f"IN source {a} is not modelled")
                v &= (1 << n) - 1
                sm.isr = ((sm.isr << n) | v) & 0xFFFFFFFF if n < 32 else v
                sm.isr_cnt = min(32, sm.isr_cnt + n)
                if sm.isr_cnt >= 32:
                    sm.rxf.append(sm.isr)
                    sm.isr, sm.isr_cnt = 0, 0
        elif op == OP_OUT:
            n = idx or 32
            if sm.osr_cnt >= 32:
                if not sm.txf:
                    stalled = True  # autopull with the TX FIFO empty
                else:
                    sm.osr, sm.osr_cnt = sm.txf.popleft(), 0
            if not stalled:
                v = sm.osr >> (32 - n)
                sm.osr = (sm.osr << n) & 0xFFFFFFFF if n < 32 else 0
                sm.osr_cnt = min(32, sm.osr_cnt + n)
                if a == 0:
                    self.write_pins(sm.out_base, sm.out_count, v)
                elif a == 1:
                    sm.x = v
                elif a == 2:
                    sm.y = v
                elif a == 3:
                    pass
                elif a == 4:
                    self.write_pins(sm.out_base, sm.out_count, v, dirs=True)
                elif a == 5:
                    sm.pc, jumped = v & 31, True
                else:
                    raise Fail(f"OUT destination {a} is not modelled")
        elif op == OP_PUSHPULL:
            pull, cond, block = (w >> 7) & 1, (w >> 6) & 1, (w >> 5) & 1
            if (w & 31) or (w >> 4) & 1:
                raise Fail("the RP2350's MOV to and from the FIFO is not modelled")
            if not pull:
                if cond and sm.isr_cnt < 32:
                    pass
                elif len(sm.rxf) >= 4:
                    stalled = bool(block)
                else:
                    sm.rxf.append(sm.isr)
                    sm.isr, sm.isr_cnt = 0, 0
            else:
                if cond and sm.osr_cnt < 32:
                    pass
                elif not sm.txf:
                    if block:
                        stalled = True
                    else:
                        sm.osr, sm.osr_cnt = sm.x, 0
                else:
                    sm.osr, sm.osr_cnt = sm.txf.popleft(), 0
        elif op == OP_MOV:
            dst, mop, src = a, (w >> 3) & 3, w & 7
            v = {0: (((sync | (sync << 32)) >> sm.in_base) & 0xFFFFFFFF), 1: sm.x, 2: sm.y,
                 3: 0, 6: sm.isr, 7: sm.osr}.get(src)
            if v is None:
                raise Fail(f"MOV source {src} is not modelled")
            if mop == 1:
                v ^= 0xFFFFFFFF
            elif mop == 2:
                v = int(f"{v:032b}"[::-1], 2)
            if dst == 1:
                sm.x = v
            elif dst == 2:
                sm.y = v
            elif dst == 6:
                sm.isr, sm.isr_cnt = v, 0
            elif dst == 7:
                sm.osr, sm.osr_cnt = v, 0
            elif dst == 5:
                sm.pc, jumped = v & 31, True
            else:
                raise Fail(f"MOV destination {dst} is not modelled")
        elif op == OP_SET:
            if a == 0:
                self.write_pins(sm.set_base, sm.set_count, idx)
            elif a == 1:
                sm.x = idx
            elif a == 2:
                sm.y = idx
            elif a == 4:
                self.write_pins(sm.set_base, sm.set_count, idx, dirs=True)
            else:
                raise Fail(f"SET destination {a} is not modelled")
        else:
            raise Fail("IRQ is not modelled")
        if forced:
            return stalled
        sm.wait_stall = stalled and op == OP_WAIT
        if stalled:
            return True
        sm.delay = delay
        if not jumped:
            sm.pc = sm.wrap_bottom if sm.pc == sm.wrap_top else (sm.pc + 1) & 31
        return False


# ================================================================= card model

def crc7(bits):
    """CRC7, generator x^7 + x^3 + 1 (4.5)."""
    r = 0
    for b in bits:
        fb = ((r >> 6) & 1) ^ b
        r = (r << 1) & 0x7F
        if fb:
            r ^= 0x09
    return r


def crc16(bits):
    """CRC16, generator x^16 + x^12 + x^5 + 1, initial value 0 (4.5)."""
    r = 0
    for b in bits:
        fb = ((r >> 15) & 1) ^ b
        r = (r << 1) & 0xFFFF
        if fb:
            r ^= 0x1021
    return r


def bits_of(value, n):
    return [(value >> (n - 1 - i)) & 1 for i in range(n)]


def nibbles_of(data):
    out = []
    for b in data:
        out += [b >> 4, b & 15]
    return out


def crc_nibbles(nibs):
    """The 16 CRC nibbles after a four-bit block: CRC16 per line, MSB first."""
    crcs = [crc16([(v >> line) & 1 for v in nibs]) for line in range(4)]
    return [sum(((crcs[line] >> (15 - k)) & 1) << line for line in range(4)) for k in range(16)]


KEEP = "keep"


class Card:
    """One SD card in four-bit SD mode.

    Samples CMD and DAT on rising CLK edges. Changes its outputs on falling
    edges, @p delay system clocks after the edge reaches its CLK pin. @p ncr
    and @p nac count clocks from a command's end bit to the start bit of the
    response and of the first data block; @p gap counts clocks between the
    blocks of a multiple-block read, and @p busy the clocks of busy after CMD12
    and after a written block. Data packets as 3.6.1 draws them: a start bit on
    all four lines, the bytes high nibble first, a CRC16 per line, an end bit.
    """

    def __init__(self, delay, ncr, nac, gap, busy):
        self.delay, self.ncr, self.nac, self.gap, self.busy = delay, ncr, nac, gap, busy
        self.emu = None
        self.cmd_q = collections.deque()   # one item per falling edge: 0, 1, None, KEEP
        self.dat_q = collections.deque()   # (mask, nibble), None or KEEP
        self.driving_cmd = self.driving_dat = False
        self.rx = None                     # command bits being received
        self.commands = []                 # (index, argument, valid, rising edge)
        self.errors = []
        self.status = 0x900                # READY_FOR_DATA, state transfer (4.10.1)
        self.csd = None
        self.multi = None                  # next block of a CMD18
        self.write = None                  # block address of a CMD24, before its data
        self.wr_nibs = None
        self.written = []                  # (block, data, CRC matched)
        self.t_resp_end = None             # falling edge after a response's end bit
        self.t_data_start = None           # falling edge that drives a start bit

    @staticmethod
    def block(lba):
        return bytes((lba * 7 + i * 13 + (i >> 5)) & 0xFF for i in range(512))

    # -- edges
    def fall(self, c):
        at = c + self.delay
        if self.cmd_q:
            v = self.cmd_q.popleft()
            if v is None and self.t_resp_end is None:
                self.t_resp_end = c
            if v is not KEEP:
                self.driving_cmd = v is not None
                self.emu.card_drive(at, 1 << CMD, None if v is None else v << CMD)
        if self.dat_q:
            v = self.dat_q.popleft()
            if v is not KEEP:
                if v is None:
                    self.driving_dat = False
                    self.emu.card_drive(at, DAT_MASK, None)
                else:
                    mask, nib = v
                    self.driving_dat = True
                    if mask != 0xF:
                        self.emu.card_drive(at, (0xF & ~mask) << D0, None)
                    self.emu.card_drive(at, mask << D0, nib << D0)
                    if v == (0xF, 0) and self.t_data_start is None:
                        self.t_data_start = c
        elif self.multi is not None:
            self.queue_block(self.multi, self.gap)
            self.multi += 1

    def rise(self, c, pad):
        # A bit the host sends counts only while the host drives its line: a
        # pull-up that happens to hold the right level is not a driven bit.
        host = self.emu.out_dir
        cmd = (pad >> CMD) & 1
        if self.rx is not None:
            if not (host >> CMD) & 1:
                self.errors.append("a command bit on an undriven CMD")
            self.rx.append(cmd)
            if len(self.rx) == 48:
                bits, self.rx = self.rx, None
                self.command(bits, c)
        elif not cmd and not self.driving_cmd and not self.cmd_q:
            self.rx = [0]
        nib = (pad >> D0) & 15
        if self.wr_nibs is not None:
            if host & DAT_MASK != DAT_MASK:
                self.errors.append("a write nibble on undriven DAT lines")
            self.wr_nibs.append(nib)
            if len(self.wr_nibs) == 1 + 1024 + 16 + 1:
                self.write_done(c)
        elif self.write is not None and not self.driving_dat and not self.dat_q:
            if nib != 15:
                if nib:
                    self.errors.append(f"start bit nibble {nib:x}")
                self.wr_nibs = [nib]

    # -- protocol
    def respond(self, index, arg=None, reg=None):
        if reg is None:
            bits = [0, 0] + bits_of(index, 6) + bits_of(arg, 32)
            bits += bits_of(crc7(bits), 7) + [1]
        else:
            bits = [0, 0] + [1] * 6 + bits_of(int.from_bytes(reg, "big"), 128)
        self.last_response = bits
        self.cmd_q = collections.deque([KEEP] * self.ncr + bits + [None])
        return len(self.cmd_q)

    def queue_block(self, lba, wait):
        nibs = nibbles_of(self.block(lba))
        self.dat_q.extend([KEEP] * wait + [(0xF, 0)] + [(0xF, v) for v in nibs]
                          + [(0xF, v) for v in crc_nibbles(nibs)] + [(0xF, 0xF), None])

    def command(self, bits, c):
        index = int("".join(map(str, bits[2:8])), 2)
        arg = int("".join(map(str, bits[8:40])), 2)
        crc = int("".join(map(str, bits[40:47])), 2)
        ok = bits[1] == 1 and bits[47] == 1 and crc7(bits[:40]) == crc
        self.commands.append((index, arg, ok, c))
        if not ok:
            self.errors.append(f"CMD{index}: bad frame")
            return
        if index == 0:
            self.multi, self.write = None, None
            return
        if index == 9:
            self.respond(9, reg=self.csd)
        elif index in (17, 18):
            self.respond(index, self.status)
            self.dat_q.clear()
            self.queue_block(arg, self.nac)
            self.multi = arg + 1 if index == 18 else None
        elif index == 12:
            # The model ends the transfer two clocks after the end bit of
            # CMD12, and signals busy on DAT0 after the response.
            n = self.respond(12, self.status | 0x200)
            rest = [self.dat_q.popleft() for _ in range(min(2, len(self.dat_q)))]
            self.multi = None
            self.dat_q = collections.deque(rest + [None] + [KEEP] * max(0, n - len(rest))
                                           + [(1, 0)] * self.busy + [None])
        elif index == 24:
            self.respond(24, self.status)
            self.write = arg
        else:
            self.respond(index, self.status)

    def write_done(self, c):
        nibs, self.wr_nibs = self.wr_nibs, None
        data_nibs = nibs[1:1025]
        ok = nibs[1025:1041] == crc_nibbles(data_nibs) and nibs[-1] == 15
        data = bytes((data_nibs[2 * i] << 4) | data_nibs[2 * i + 1] for i in range(512))
        self.written.append((self.write, data, ok))
        self.write = None
        token = [0, 1, 0] if ok else [1, 0, 1]
        # The CRC status token on DAT0 (Figure 5-8) two clocks after the end
        # bit, then busy for the programming of the block.
        self.dat_q = collections.deque([KEEP, KEEP] + [(1, b) for b in [0] + token + [1]]
                                       + [(1, 0)] * self.busy + [None])


# ================================================================== the host

class Proto:
    """src/sdio/sd_proto.c, compiled for this machine and called through ctypes."""

    def __init__(self, cc, workdir):
        lib = pathlib.Path(workdir) / "libsdproto.so"
        subprocess.run([cc, "-std=c11", "-O1", "-shared", "-fPIC", "-o", str(lib),
                        str(ROOT / "src/sdio/sd_proto.c")], check=True)
        self.lib = ctypes.CDLL(str(lib))
        header = (ROOT / "src/sdio/sd_proto.h").read_text()
        body = re.search(r"typedef enum \{(.*?)\} sdp_resp_t;", header, re.S).group(1)
        names = re.findall(r"^\s*(SDP_RESP_\w+)", body, re.M)
        self.kind = {n: i for i, n in enumerate(names)}
        self.tx_words = int(re.search(r"#define SDP_TX_WORDS\s+(\d+)", header).group(1))
        self.trail = int(re.search(r"#define SDP_TRAIL_CLOCKS\s+(\d+)", header).group(1))
        u32, u8 = ctypes.c_uint32, ctypes.c_uint8
        L = self.lib
        L.sdp_cmd_words.argtypes = [u8, u32, ctypes.c_uint, ctypes.POINTER(u32)]
        L.sdp_resp_words.argtypes = [ctypes.c_uint]
        L.sdp_resp_words.restype = ctypes.c_uint
        L.sdp_resp_unpack.argtypes = [ctypes.POINTER(u32), ctypes.c_uint, ctypes.POINTER(u8)]
        L.sdp_resp48_check.argtypes = [ctypes.POINTER(u8), ctypes.c_int, u8]
        L.sdp_resp48_arg.argtypes = [ctypes.POINTER(u8)]
        L.sdp_resp48_arg.restype = u32
        L.sdp_r2_check.argtypes = [ctypes.POINTER(u8), ctypes.POINTER(u8)]
        L.sdp_tx_pack.argtypes = [ctypes.POINTER(u8), ctypes.POINTER(u32)]
        L.sdp_rx_check.argtypes = [ctypes.POINTER(u8), ctypes.c_size_t, ctypes.POINTER(u32)]
        L.sdp_rx_check.restype = ctypes.c_bool
        L.sdp_crc_status.argtypes = [ctypes.POINTER(u32)]

    def cmd_words(self, index, arg, resp_bits):
        w = (ctypes.c_uint32 * 2)()
        self.lib.sdp_cmd_words(index, arg, resp_bits, w)
        return list(w)

    def resp_words(self, resp_bits):
        return self.lib.sdp_resp_words(resp_bits)

    def unpack(self, raw, resp_bits):
        w = (ctypes.c_uint32 * len(raw))(*raw)
        out = (ctypes.c_uint8 * 17)()
        self.lib.sdp_resp_unpack(w, resp_bits, out)
        return bytes(out)[:resp_bits // 8]

    def resp48_check(self, r, kind, index):
        return self.lib.sdp_resp48_check((ctypes.c_uint8 * 6)(*r), self.kind[kind], index)

    def resp48_arg(self, r):
        return self.lib.sdp_resp48_arg((ctypes.c_uint8 * 6)(*r))

    def r2_check(self, r):
        reg = (ctypes.c_uint8 * 16)()
        return self.lib.sdp_r2_check((ctypes.c_uint8 * 17)(*r), reg), bytes(reg)

    def tx_pack(self, data):
        out = (ctypes.c_uint32 * self.tx_words)()
        self.lib.sdp_tx_pack((ctypes.c_uint8 * 512)(*data), out)
        return list(out)

    def rx_check(self, data, crc):
        return self.lib.sdp_rx_check((ctypes.c_uint8 * len(data))(*data), len(data),
                                     (ctypes.c_uint32 * 2)(*crc))

    def crc_status(self, w):
        return self.lib.sdp_crc_status((ctypes.c_uint32 * 2)(*w))


def enc_jmp(sm, addr, side=None):
    w = addr & 31
    if side is not None:
        w |= side << (13 - sm.ss_bits)
    return w


class Host:
    """The calls of src/sdio/sd_pio.c, on the model instead of the hardware."""

    def __init__(self, emu, proto):
        self.emu, self.p = emu, proto
        self.cmd, self.dat = emu.sms
        self.running = False
        self.clk_hz = 1
        self.y = None
        # sdpio_claim(): pins, then each machine's configuration.
        c, d = self.cmd, self.dat
        c.side_base = CLK
        c.out_base = c.set_base = c.in_base = c.jmp_pin = CMD
        c.out_count = c.set_count = 1
        d.out_base = d.set_base = d.in_base = D0
        d.out_count = d.set_count = 4
        d.jmp_pin = CLK
        c.pc, d.pc = c.label("start"), d.label("rx_start")
        emu.write_pins(CMD, 1, 1)
        emu.write_pins(CLK, 1, 1, dirs=True)
        emu.write_pins(CMD, 1, 0, dirs=True)
        emu.write_pins(D0, 4, 0xF)
        emu.write_pins(D0, 4, 0, dirs=True)
        self.dat_set_y(2 * 512 + 16 - 1)
        self.set_clock(400_000)
        self.cmd_halt()

    def us(self, us):
        return us * SYS_HZ // 1_000_000

    def clocks(self, n):
        """System clocks that @p n bus clocks take."""
        return n * 2 * self.cmd.div // 256 + 2

    def clocks_us(self, n):
        return (n * 1_000_000 + self.clk_hz - 1) // self.clk_hz

    def cmd_halt(self):
        c = self.cmd
        c.enabled = False
        c.restart()
        c.clear_fifos()
        self.emu.execute(c, enc_jmp(c, c.label("start"), 0), forced=True)
        self.running = False

    def dat_halt(self):
        d = self.dat
        d.enabled = False
        self.emu.execute(d, (OP_SET << 13) | (SET_DST["pindirs"] << 5), forced=True)
        d.restart()
        d.clear_fifos()

    def dat_set_y(self, y):
        d = self.dat
        d.txf.append(y)
        self.emu.execute(d, (OP_PUSHPULL << 13) | (1 << 7) | (1 << 5), forced=True)
        self.emu.execute(d, (OP_MOV << 13) | (MOV_DST["y"] << 5) | MOV_SRC["osr"], forced=True)
        d.restart()
        self.y = y

    def set_clock(self, hz):
        if self.running:
            self.stop_clock()
        div = (SYS_HZ * 256 + 2 * hz - 1) // (2 * hz)
        div = min(max(div, 3 * 256), 0xFFFFFF)
        self.cmd.div = div
        self.clk_hz = SYS_HZ * 256 // (2 * div)
        return self.clk_hz

    def stop_clock(self):
        if self.running:
            self.emu.run(self.us(self.clocks_us(self.p.trail)))
        self.cmd_halt()

    def command(self, index, arg, resp_bits, limit_clocks=400):
        """busCommand(): the response bytes, or None for no response in time."""
        w = self.p.cmd_words(index, arg, resp_bits)
        self.cmd_halt()
        c = self.cmd
        c.txf.extend(w)
        c.enabled = True
        self.running = True
        limit = self.clocks(limit_clocks)
        if resp_bits == 0:
            at = c.label("wait_start")
            if not self.emu.run_until(lambda: c.pc in (at, at + 1), limit):
                self.cmd_halt()
                return None
            self.emu.run(self.us(self.clocks_us(self.p.trail)))
            self.cmd_halt()
            return b""
        raw = []
        for _ in range(self.p.resp_words(resp_bits)):
            if not self.emu.run_until(lambda: bool(c.rxf), limit):
                self.cmd_halt()
                return None
            raw.append(c.rxf.popleft())
        return self.p.unpack(raw, resp_bits)

    def rx_arm(self, nbytes, blocks):
        self.dat_halt()
        y = 2 * nbytes + 16 - 1
        if y != self.y:
            self.dat_set_y(y)
        d = self.dat
        self.emu.execute(d, enc_jmp(d, d.label("rx_start")), forced=True)
        self.rx_layout = (nbytes, blocks)
        self.emu.rx_dma = [blocks * (nbytes // 4 + 2), []]
        d.enabled = True

    def rx_wait(self, limit_clocks):
        want = self.emu.rx_dma
        ok = self.emu.run_until(lambda: len(want[1]) >= want[0], self.clocks(limit_clocks))
        nbytes, blocks = self.rx_layout
        per = nbytes // 4 + 2
        words = want[1]
        out = []
        for b in range(blocks):
            w = words[b * per:(b + 1) * per]
            data = b"".join(x.to_bytes(4, "big") for x in w[:-2])   # the DMA byte swap
            out.append((data, w[-2:]))
        return ok, out

    def rx_stop(self):
        self.emu.rx_dma = None
        self.dat_halt()

    def tx_block(self, words, limit_clocks=1300):
        self.dat_halt()
        d = self.dat
        self.emu.execute(d, enc_jmp(d, d.label("tx_start")), forced=True)
        self.emu.tx_dma = collections.deque(words)
        d.enabled = True
        ok = self.emu.run_until(lambda: len(d.rxf) >= 2, self.clocks(limit_clocks))
        status = [d.rxf.popleft(), d.rxf.popleft()] if ok else None
        self.dat_halt()
        self.emu.tx_dma = None
        return status

    def dat0(self):
        return (self.emu.p2 >> D0) & 1

    def busy_wait(self, limit_clocks):
        """busyWait(): DAT0 polled every 2 us. Returns (released, busy seen)."""
        seen = False
        end = self.emu.cycle + self.clocks(limit_clocks)
        while self.emu.cycle < end:
            if self.dat0():
                return True, seen
            seen = True
            self.emu.run(self.us(2))
        return bool(self.dat0()), seen


# ================================================================== scenarios

def make_csd():
    """A CSD version 2.0 register (5.3.3) with its CRC7 and end bit."""
    reg = bytearray([0x40, 0x0E, 0x00, 0x32, 0x5B, 0x59, 0x00, 0x00,
                     0x76, 0xB2, 0x7F, 0x80, 0x0A, 0x40, 0x00, 0x00])
    reg[15] = (crc7(bits_of(int.from_bytes(reg[:15], "big"), 120)) << 1) | 1
    return bytes(reg)


_MULTI_LBA = []


def multi_lba():
    """First block of the CMD18 run: blocks whose last CRC nibble has DAT0 low.

    After a block the data program wraps to rx_start and waits for DAT0 high,
    the end bit, before it looks for the next start bit. With DAT0 low in the
    last CRC nibble, a program that skipped that wait would take the end bit's
    rising edge for the next start bit, and the run would see it.
    """
    if not _MULTI_LBA:
        lba = 100
        while any(crc_nibbles(nibbles_of(Card.block(lba + i)))[-1] & 1 for i in range(2)):
            lba += 1
        _MULTI_LBA.append(lba)
    return _MULTI_LBA[0]


def run_session(progs, proto, hz, delay, profile):
    """One session of every scenario. Returns {scenario: problem or None}."""
    ncr, nac, gap, busy = profile
    cmd_sm = SM(progs["sdpio_cmd"], 32 - len(progs["sdpio_cmd"].words))
    dat_sm = SM(progs["sdpio_dat"], cmd_sm.off - len(progs["sdpio_dat"].words))
    card = Card(delay, ncr, nac, gap, busy)
    card.csd = make_csd()
    emu = Emu(cmd_sm, dat_sm, card)
    host = Host(emu, proto)
    host.set_clock(hz)
    res = {}
    lim_resp = ncr + 136 + 16 + 20

    def r1(index, arg, kind="SDP_RESP_R1"):
        r = host.command(index, arg, 48, lim_resp)
        if r is None:
            return f"no response to CMD{index}"
        e = proto.resp48_check(r, kind, index)
        if e:
            return f"CMD{index}: sdp_resp48_check() returns {e}"
        if r != bytes(int("".join(map(str, card.last_response[i:i + 8])), 2)
                      for i in range(0, 48, 8)):
            return f"CMD{index}: response {r.hex()} differs from the card's"
        return None

    # CMD0
    r = host.command(0, 0, 0)
    if r is None:
        res["CMD0"] = "the command program never reached its wait for a response"
    elif [x[:3] for x in card.commands] != [(0, 0, True)]:
        res["CMD0"] = f"the card received {card.commands}, not one valid CMD0"
    else:
        res["CMD0"] = None

    # R1
    res["R1 (CMD13)"] = r1(13, 0x1234 << 16)
    host.stop_clock()

    # R2
    r = host.command(9, 0x1234 << 16, 136, lim_resp)
    if r is None:
        res["R2 (CMD9)"] = "no response"
    else:
        e, reg = proto.r2_check(r)
        res["R2 (CMD9)"] = (f"sdp_r2_check() returns {e}" if e else
                            "the register differs from the card's CSD" if reg != card.csd
                            else None)
    host.stop_clock()

    # CMD17, the data block over the response for the fast card
    card.t_data_start = card.t_resp_end = None
    host.rx_arm(512, 1)
    p = r1(17, 5)
    ok, blocks = host.rx_wait(nac + 1100 + 100)
    host.rx_stop()
    host.stop_clock()
    if p:
        res["CMD17"] = p
    elif not ok:
        res["CMD17"] = "the block did not arrive"
    elif blocks[0][0] != card.block(5):
        res["CMD17"] = "the data differ from the card's block"
    elif not proto.rx_check(blocks[0][0], blocks[0][1]):
        res["CMD17"] = "sdp_rx_check() rejects the CRC words"
    elif nac < 48 and not (card.t_data_start or 1 << 62) < (card.t_resp_end or 0):
        res["CMD17"] = "the data start bit did not overlap the response"
    else:
        res["CMD17"] = None

    # CMD18 over 3 blocks
    first = multi_lba()
    host.rx_arm(512, 3)
    p = r1(18, first)
    ok, blocks = host.rx_wait(nac + 3 * (1100 + gap) + 100)
    host.rx_stop()
    if p:
        res["CMD18, 3 blocks"] = p
    elif not ok:
        res["CMD18, 3 blocks"] = "not all three blocks arrived"
    else:
        bad = [i for i, (data, crc) in enumerate(blocks)
               if data != card.block(first + i) or not proto.rx_check(data, crc)]
        res["CMD18, 3 blocks"] = f"blocks {bad} wrong" if bad else None

    # CMD12 while the card sends the fourth block
    p = r1(12, 0, "SDP_RESP_R1B")
    if p:
        res["CMD12, R1b"] = p
    else:
        released, seen = host.busy_wait(busy + 200)
        res["CMD12, R1b"] = ("busy was not seen on DAT0" if not seen else
                             "busy was not released" if not released else
                             "the card kept sending data" if card.multi is not None
                             or card.driving_dat else None)
    host.stop_clock()

    # CMD24 and its CRC status token
    data = bytes((i * 29 + 3) & 0xFF for i in range(512))
    p = r1(24, 9)
    if p:
        res["CMD24, CRC status"] = p
    else:
        status = host.tx_block(proto.tx_pack(data))
        if status is None:
            res["CMD24, CRC status"] = "no CRC status window captured"
        else:
            t = proto.crc_status(status)
            released, seen = host.busy_wait(busy + 200)
            if not card.written:
                res["CMD24, CRC status"] = "the card received no block"
            elif card.written[-1] != (9, data, True):
                res["CMD24, CRC status"] = "the card received another block or a bad CRC16"
            elif t != 2:
                res["CMD24, CRC status"] = f"sdp_crc_status() returns {t}, not 2"
            elif not released:
                res["CMD24, CRC status"] = "busy was not released"
            else:
                res["CMD24, CRC status"] = None
    host.stop_clock()

    problems = []
    if emu.conflicts:
        problems.append(f"{emu.conflicts} system clocks with host and card driving one line")
    if emu.timing:
        problems.append(f"{len(emu.timing)} sampling margin violations, first {emu.timing[0]}")
    if card.errors:
        problems.append("card: " + ", ".join(sorted(set(card.errors))))
    res["bus"] = "; ".join(problems) or None
    return res


def control(progs, proto):
    """A card output delay of 3 system clocks at 25 MHz must fail.

    It moves each change of CMD onto the system clock in which the command
    program samples CMD, so the run has to report it.
    """
    res = run_session(progs, proto, 25_000_000, 3, PROFILES["fast card"])
    return any(res.values())


def rate_name(hz):
    return f"{hz / 1e6:g} MHz" if hz >= 1_000_000 else f"{hz / 1e3:g} kHz"


PROFILES = {"fast card": (2, 2, 2, 64), "slow card": (64, 100, 40, 200)}


def main():
    global PIO_SRC
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    ap.add_argument("--pio", type=pathlib.Path, default=PIO_SRC, help="the .pio file to test")
    ap.add_argument("--pioasm", help="pioasm to compare the assembler with")
    ap.add_argument("--asm-only", action="store_true", help="assemble, compare, stop")
    ap.add_argument("--quick", action="store_true", help="25 MHz only")
    ap.add_argument("--cc", default=os.environ.get("CC", "cc"))
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()
    PIO_SRC = args.pio
    try:
        progs = assemble(args.pio)
        if args.pioasm:
            cross_check(progs, args.pioasm, args.pio)
    except (Fail, OSError, subprocess.CalledProcessError) as e:
        print(f"pio_sdio_emu: FAIL: {e}")
        return 1
    if args.asm_only:
        n = sum(len(p.words) for p in progs.values())
        print(f"pio_sdio_emu: {args.pio.name} assembles to {n} instructions"
              + (", the same words as pioasm" if args.pioasm else ""))
        return 0

    rates = [25_000_000] if args.quick else [400_000, 10_000_000, 25_000_000]
    failed, total = [], 0
    with tempfile.TemporaryDirectory() as tmp:
        try:
            proto = Proto(args.cc, tmp)
        except (OSError, subprocess.CalledProcessError) as e:
            print(f"pio_sdio_emu: FAIL: cannot build sd_proto.c: {e}")
            return 1
        for hz in rates:
            for delay in (0, 1, 2):
                for pname, profile in PROFILES.items():
                    run = f"{rate_name(hz)}, card delay {delay}, {pname}"
                    try:
                        res = run_session(progs, proto, hz, delay, profile)
                    except Fail as e:
                        res = {"model": str(e)}
                    total += len(res)
                    bad = {k: v for k, v in res.items() if v}
                    failed += [f"{run}: {k}: {v}" for k, v in bad.items()]
                    if args.verbose or bad:
                        print(f"  {run}: " + ("ok" if not bad else
                                              "; ".join(f"{k}: {v}" for k, v in bad.items())))
        try:
            control_failed = control(progs, proto)
        except Fail:
            control_failed = True
    if not control_failed:
        failed.append("control: a card output delay of 3 system clocks at 25 MHz passes")
    runs = len(rates) * 3 * len(PROFILES)
    bad = len(failed) - (not control_failed)
    print(f"pio_sdio_emu: {'FAIL' if failed else 'ok'}: {total - bad} of {total} checks"
          f" pass in {runs} runs ({', '.join(rate_name(hz) for hz in rates)};"
          " card output delay 0, 1 and 2 system clocks; fast and slow card); control run"
          f" {'fails, as it must' if control_failed else 'PASSES, so the runs cannot fail'}")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
