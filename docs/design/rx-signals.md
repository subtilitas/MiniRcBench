# Receiver signals: S.BUS monitor and S.BUS output

Status: slices 1a, 2 and 3 built, the rest planned. Slice 1a copied
rcbench's S.BUS decoder and its test into `lib/rcbench/` and `test/rcbench/`.
Slice 2 is the S.BUS monitor's core1 job, `src/sbus_job.*`, with its pin user
and its host tests ([slice 2 as built](#slice-2-as-built)). Slice 3 is the
S.BUS MONITOR screen, `src/ui_sbus.*` and its formatter `src/sbus_view.*`,
and RX SIGNALS, the chooser behind the MENU row, `src/ui_rx_signals.*`
([slice 3 as built](#slice-3-as-built)). All three are tested on the host
only, none on hardware ([work breakdown](#work-breakdown)). The owner
answered the note's open questions on 2026-11-14 ([decisions](#decisions)):
build the S.BUS monitor and an S.BUS output, no PPM output. The S.BUS
monitor reads a receiver's S.BUS output on GP1 through UART0. The S.BUS
output, inverted 8E2 at 100,000 baud on GP0 through UART0's TX, is not
designed: its open points are listed under [S.BUS output](#sbus-output), and
its encoder is written and tested in rcbench first. The PPM sections stay as
the record of a design that is not built, by decision 1. Both tools sit on
the pin claim of [pin-claim.md](pin-claim.md) under one MENU row, RX
SIGNALS, which opens a chooser with MONITOR and OUTPUT (decision 4); OUTPUT
is drawn disabled with `NOT BUILT` until the output exists ([MENU](#menu)).
The decoder is rcbench's, host-tested there and nowhere measured on a pin.

## Terms

S.BUS is Futaba's serial receiver bus: 16 proportional channels and two
digital ones in a 25-byte frame. PPM is pulse-position modulation: several
channels on one wire, each carried by the time from one fixed mark to the
next. UART is universal asynchronous receiver-transmitter; the RP2350's two
hardware UARTs are ARM PL011 cells. 8E2 is 8 data bits, even parity and 2
stop bits. PIO is the RP2350's programmable input/output block, SM one PIO
state machine, DMA direct memory access, FIFO a first-in, first-out buffer,
GPIO general-purpose input/output, PWM pulse-width modulation, SDIO Secure
Digital input/output (the card bus), USB Universal Serial Bus. IOVDD is the
RP2350's I/O supply, 3.3 V on this board. INOVER and OUTOVER are the input and
output override fields of a GPIO's control register, which can invert the
signal between the pad and the peripheral. ESC is electronic speed
controller, FC flight controller, UI user interface. SIG and AUX are the
claim's two roles ([pin-claim.md](pin-claim.md#the-two-pins)): the ESC pin,
GP0 by default, and the KISS pin, GP1 by default. CRSF is the Crossfire
serial receiver protocol, iBUS FlySky's, F.Port FrSky's.

Futaba, S.BUS, FrSky, F.Port, FlySky, iBUS and Crossfire are names or
trademarks of their owners, used here only to say which receivers a tool
works with.

## Sources

rcbench at `origin/main`, commit 5ef42efd (2026-11-13):

| rcbench path | What it is |
|---|---|
| `protocols/sbus/sbus.c`, `include/sbus.h` | S.BUS frame decoder, plain C11, no pin driver |
| `protocols/ppm/ppm.c`, `include/ppm.h` | PPM frame layout as run lengths, plain C11 |
| `protocols/ppm/rp2350/out_ppm.c`, `out_ppm.h`, `ppm.pio` | PPM pin driver: one SM and two DMA channels per pin |
| `test/host/test_sbus.c` | 12 cases on the decoder |
| `test/host/test_ppm.c` | 9 cases on the frame layout; includes rcbench's `outputs.h` |

rcbench's [STATUS.md](https://github.com/subtilitas/rcbench/blob/main/STATUS.md)
records the decoder
as built and tested, with no PIO receiver written, and says of its output
drivers, PPM among them: "No instrument has been on any output pin." Both
modules are host-tested only. Commits 453cf334 and 3e29dc1b, from which
`lib/rcbench/` holds the servo, KST and Hitec files, already contain both
modules; the changes since then are Doxygen comments and unit spacing, no
change in behaviour. The copies come from 5ef42efd because it was rcbench's
`origin/main` at the time of the copy.

## S.BUS frame

What this note relies on, checked against rcbench's decoder and README:

| Fact | Value | rcbench |
|---|---|---|
| Line | inverted UART, 100,000 baud, 8E2 | stated (`sbus.h`, README) |
| Level at the pin when idle | low | follows from the inversion of a UART that idles high; not stated. To confirm on a scope |
| Frame | 25 bytes: header, 22 bytes of channels, flag byte, footer | stated |
| Header | 0x0F, also an ordinary value inside channel data | stated (`SBUS_HEADER`) |
| Channels | 16 of 11 bits, packed least significant bit first across bytes 1 to 22 | stated (`unpack()`) |
| Flag byte, byte 23 | bit 0 channel 17, bit 1 channel 18, bit 2 frame lost, bit 3 failsafe | stated (`SBUS_FLAG_*`) |
| Footer, byte 24 | 0x00 by the specification; `footer_ok()` accepts every byte whose low nibble is 0x0 or 0x4, 32 values, among them the 0x04, 0x14, 0x24 and 0x34 its comment names | 0x00 stated as "what the specification says", and 0x04 to 0x34 as what "receivers in the field also send"; which receivers, and whether those are S.BUS2 frames, is not stated. To confirm |
| Checksum | none | stated |
| Byte time | 120 µs: 12 bits of 10 µs | stated |
| Frame time | 3 ms: 25 bytes of 120 µs | stated |
| Frame period | 7 ms or 14 ms | stated in `sbus.h`; which receiver sends which is not. To confirm |
| Gap between frames | at least 4 ms | stated |
| Framing rule | a gap of 2000 µs or more (`SBUS_GAP_US`) starts a frame | stated |
| Raw to µs | 172 is 1000 µs, 1811 is 2000 µs, linear, rounded to nearest; 0 is 895 µs and 2047 is 2144 µs (`sbus_to_us()`) | stated as "what the protocol settled on", with no source. Whose convention it is: to confirm |

Two consequences shape the receive path. S.BUS has no checksum, so the
parity bit of each byte is the only integrity check the line has. And the
decoder frames on time, so every byte needs an arrival time within a small
fraction of the 2000 µs threshold.

## S.BUS monitor

### What it is for

- Checking a receiver bound to a transmitter: does it send S.BUS, at which
  frame period, and do the channels follow the sticks.
- Channel order: which stick moves which channel, before an FC is set up.
- Failsafe settings: with the transmitter switched off, the receiver either
  keeps sending frames with the failsafe bit and its failsafe values, which
  the screen shows, or stops sending, which the screen shows as `NO SIGNAL`.
- Link quality on the bench: the count of frames with the frame-lost bit.

It sends nothing and measures nothing electrical.

### Electrical

| Item | Value |
|---|---|
| Pin | GP1, J5 pin 1 or J4 pin 10. GP1 is AUX in the default wiring |
| Pin rating | "Digital IO (FT)": 5.5 V while IOVDD is at 3.3 V, 3.63 V while IOVDD is at 0 V (RP2350 datasheet, Table 1674) |
| Receiver output level | unknown. S.BUS outputs are commonly 3.3 V logic; neither this project nor rcbench has measured one. A receiver whose output swings to its supply would exceed 5.5 V with a supply above 5.5 V |
| Line idle | low; the pad's pull-down holds it there with nothing plugged in |
| Board input threshold | the RP2350's at IOVDD 3.3 V; that a 3.3 V output clears it with margin is to confirm on the bench |
| Receiver supply | its own source, or the switched servo supply of [kst-programming.md](kst-programming.md#switched-servo-supply). Never the board: J5 pin 3 and J4 pin 6 are 3.3 V, J4 pin 2 is VBUS, the USB 5 V supply |

The line idles low and is high only for the start bit and the zero bits. A
3.3 V receiver on an unpowered board therefore puts at most 3.3 V on the
pin, under the 3.63 V rating. A receiver with a higher output level does not
have that margin: power the board before the receiver, and the receiver off
before the board, as for a servo ([pin-claim.md](pin-claim.md#wiring)).

In the first slice the monitor never drives the switched supply's enable,
as the servo tester does not (servo-tester.md, decision 20). A receiver on
the converter is unpowered there unless the owner chooses otherwise
(later work below).

**RP2350-E9.** A pad with the internal pull-down can latch at about 2.1 V
instead of 0 V (`src/board_pins.h`). After a receiver that drove the line
high is unplugged, GP1 may read high, which the inversion turns into a
continuous low at the UART. The PL011 loads one 0 character with the break
flag and then waits for the line to return high (SDK 2.1.1, the description
of `UART_UARTDR_BE`): one break entry, then silence. The screen decides
`NO SIGNAL` from frames and clean bytes, never from the line level, so the
latched pad reads as `NO SIGNAL` with the break count one higher.

### Wiring

```
  receiver supply, the receiver's rated input
     +  ----------------------------------------------  receiver +
     -  ----------------+-----------------------------  receiver -
                        |
  board GND  -----------+
  board GP1  ------------------------------------------  receiver S.BUS out

  GP1 = J5 pin 1 or J4 pin 10, GND = J5 pin 4 or J4 pin 1 or 5
```

The receiver's own servo and ESC outputs stay unplugged: a receiver bound
to a transmitter drives them whatever this screen does.

### Receive path

Three ways to get the bytes. During any claim the DShot pump and the KISS
receiver are torn down, so pio2 is empty and any of them fits
([resources](#resources)).

| | A. Extend `pio_uart_rx` | B. UART0 on GP1, INOVER | C. New PIO program |
|---|---|---|---|
| Inversion | INOVER on the pad | INOVER on the pad | INOVER on the pad |
| PIO | a second 9-word variant (`set x, 8` for 8 data bits and parity), 1 SM | none | about 12 words, 1 SM |
| DMA | none | none | none |
| Pins | either | GP1 only: GP0 is UART0's TX pin | either |
| Parity | in C, from the ninth bit | hardware, per byte (`UARTDR` bit 9, PE) | in C, from the ninth bit |
| Framing error | `irq 4`, which nothing reads: dropped, not counted | hardware, per byte (FE, bit 8) | pushed as a marker word and counted |
| Break | not told apart | hardware, per byte (BE, bit 10) | from a zero byte with a framing error |
| Overrun | the SM stalls on `push` and drops bits unseen | hardware (OE, bit 11) | not detected |
| FIFO | 8 entries joined: 960 µs at 120 µs a byte | 32 entries: 3,840 µs | 8 entries: 960 µs |
| Second stop bit | not checked | not checked: a PL011 receiver checks the first stop bit only (PL011 reference manual; to confirm in the RP2350 datasheet) | can check both |
| Risk | edits the program of KISS, the one receiver with a hardware report | none to existing code | a program with no hardware record and no model yet |

The owner chose B on 2026-11-14 ([decision 2](#decisions)).

Recommendation: **B, UART0 on GP1 with the pad's input inverted.** It costs
no PIO and no DMA, it reports parity, framing, break and overrun per byte,
its FIFO holds more than a whole 25-byte frame, and it touches no code that
works today. The baud divisor is exact: Pico SDK 2.1.1 runs `clk_peri` from
`clk_sys` at 150 MHz (`runtime_init_clocks.c`), and `uart_set_baudrate()`
computes an integer baud-rate divisor (IBRD) of 93 and a fractional one
(FBRD) of 48, which is 150 MHz × 4 / (64 × 93 + 48) =
100,000 baud. A is rejected because it puts KISS at risk; C because it needs
a new PIO model and gains only the second stop bit and GP0. The price of B
is the pin: the monitor reads GP1 whatever role GP1 has.

| Wiring in SETTINGS | Role on GP1 | Claim | Job binds |
|---|---|---|---|
| ESC GP0, KISS GP1 (default) | AUX | `PIN_SIG \| PIN_AUX` | GP1 |
| ESC GP1, KISS GP0 | SIG | `PIN_SIG`, once no ESC answers on GP1 | GP1 |
| ESC GP1, KISS GP1 (KISS off) | SIG | `PIN_SIG`, once no ESC answers on GP1 | GP1 |
| ESC GP0, KISS GP0 (KISS off) | none | not asked | nothing: the screen says `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` |

The bind sets the pad in this order: `gpio_set_function(1, GPIO_FUNC_UART)`,
then `gpio_set_inover(1, GPIO_OVERRIDE_INVERT)`, then the pull-down. The
order matters: `gpio_set_function()` writes the whole control register and
zeroes the overrides (SDK 2.1.1, `gpio.c`), the rule `hitec_line.c` already
follows. Then `uart_init(uart0, 100000)` and `uart_set_format(uart0, 8, 2,
UART_PARITY_EVEN)`. GP0 keeps its function: the UART's TX reaches no pad.
The bind refuses with `CORE1_JOB_CLOCK` when the baud rate `uart_init()`
returns is more than 1 % from 100,000 (`GP1 COULD NOT BE TAKEN (CLOCK)`),
and with a new `CORE1_JOB_NO_GP1` when no role is GP1. The screen checks the
wiring before it claims, so that refusal is a backstop; its text is the
screen's own, `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` The
firmware uses UART0 nowhere else: `pico_enable_stdio_uart` is 0 in
`CMakeLists.txt`.

The release calls `uart_deinit(uart0)`, sets INOVER back to normal, returns
GP1 to a plain input with the pull-down on, as the servo job leaves its
pins, and reports done at once. DShot's bind and `pioUartRxBegin()` both
call `gpio_set_function()`, which zeroes the override again; the release
clears it anyway, so a GP1 left alone with KISS off carries no inversion.

### Errors

A byte with a parity, framing or break error is counted and **not fed** to
the decoder. Feeding it would let a corrupted byte become a channel value:
parity is the line's only check. The frame it belonged to then holds 24
bytes when the gap comes; the decoder's gap rule discards it and counts a
resync. An overrun is counted and its byte is fed: the byte itself arrived
intact, and the bytes lost before it leave the frame short, which the gap
rule discards the same way.

### The job

Core1 runs `src/sbus_job.cpp` while the claim holds, in the pattern of the
servo job. Its decisions are pure functions on a plain struct, `SbusCore`,
which the host suite drives without the SDK:

```c
void sbusCoreReset(SbusCore *c, uint32_t nowUs);                 // at the bind
void sbusCoreFeed(SbusCore *c, uint16_t dr, uint32_t nowUs);     // one UARTDR word
bool sbusCoreStep(SbusCore *c, uint32_t nowUs, uint32_t nowMs);  // once per pass
```

`sbusCoreStep()` stamps `lastFrameMs` and `lastByteMs` with `millis()` for
what the pass fed, tracks how long the line has been quiet, and returns
whether a counted word arrived, which is when the job publishes.

**A bind into a live line.** The bind can land in the middle of a frame, and in
the middle of a character. On the chip `uart_init()` receives at 8N1 (8 data
bits, no parity, 1 stop bit) until `uart_set_format()` takes 8E2, and
`uart_set_format()` turns the UART off for about 15 bit times, 149 µs at
100,000 baud, while the character under way completes in the old format (SDK
2.1.1, `uart_disable_before_lcr_write()`). So the bind reads and discards the
receive FIFO after `uart_set_format()`, and the core counts and feeds nothing
until it has seen the line quiet for 2000 µs (`SBUS_GAP_US`): from the last
pass with a word, or from the bind, to a pass with none. The first word after
that starts a frame, and every count starts from it. A frame that begins less
than 2000 µs after the bind is not taken, since it may be the tail of one under
way; a line that is never quiet for 2000 µs is never counted.
Every pass of `loop1()`, the job:

1. reads `micros()` once;
2. drains the receive FIFO while `UARTFR.RXFE`, the receive-FIFO-empty
   flag, is clear, handing each
   `UARTDR` word (the byte in bits 0 to 7, the error flags in bits 8 to 11)
   to `sbusCoreFeed()` with that time;
3. publishes the status under the job's critical section when it changed.

`sbusCoreFeed()` counts the error flags, passes a clean byte to
`sbus_decode_byte()` with its time, and on a frame stores the channels and
flags and the interval since the last frame.

**Timing.** A byte's time is the time of the pass that drains it, not its
arrival. With the claim held, core1 runs `escTaskPoll()`, which returns at
once, and this job; no flash write parks it (settings saves are refused during
a claim). A pass is therefore far shorter than the 2000 µs threshold, so bytes
of one frame never look 2000 µs apart, and the 4 ms gap between frames is never
missed. The binding limit on a late pass is 2000 µs: a pass that late in the
middle of a frame stamps the bytes after it 2000 µs or more after those before
it, and the gap rule splits the frame. The FIFO's 32 bytes, 3,840 µs of line,
would allow more before an overrun. The pass time with this job is not
measured. The heartbeat (`core1UiAlive()`) stops nothing here: the job sends
nothing.

**Published state**, `SbusStatus`, read by core0 once per UI frame:

| Field | Meaning |
|---|---|
| `channel[16]` | raw 0 to 2047, from the last frame |
| `ch17`, `ch18`, `failsafe`, `frameLost` | the last frame's flags |
| `frames` | frames decoded since the bind |
| `lastFrameMs` | `millis()` at the last frame; 0 before the first |
| `intervalUs` | mean of the last 16 intervals between frames; 0 before two frames |
| `lostFrames`, `failsafeFrames` | frames that carried the frame-lost bit, the failsafe bit |
| `bytes`, `lastByteMs` | clean bytes fed, and when the last arrived |
| `parityErr`, `framingErr`, `breakErr`, `overrun` | from `UARTDR` |
| `resyncs`, `badFooter` | the decoder's own counters |

All counts start at 0 at each bind and count from the first quiet line after
it, so each visit to the screen starts clean. The release sets the status back
to 0 as well. Frame rate is 1,000,000 / `intervalUs`: 71.4 Hz at 14 ms,
142.9 Hz at 7 ms. The interval carries the drain jitter of one pass.

**Link state**, a pure function of the status and `millis()` on core0,
`SBUS_LOST_MS` 100 ms, seven frames at 14 ms:

| State | Rule | Chip |
|---|---|---|
| `NO SIGNAL` | no frame and no clean byte for 100 ms, or none since the bind | `C_AMBER` |
| `NO FRAMES` | clean bytes within 100 ms, no frame for 100 ms: a line at 100,000 baud 8E2 that is not S.BUS, or a variant whose footer the decoder refuses | `C_AMBER` |
| `FAILSAFE` | the last frame, within 100 ms, carries the failsafe bit | `C_RED` |
| `FRAME LOST` | a frame with the frame-lost bit within the last 250 ms, held so one such frame shows on a 40 Hz screen | `C_AMBER` |
| `OK` | otherwise | `C_GREEN` |

The first matching row wins.

### Value scale

The value column shows µs from rcbench's `sbus_to_us()`: linear through raw
172 at 1000 µs and raw 1811 at 2000 µs, so raw 992 is 1500 µs, raw 0 is
895 µs and raw 2047 is 2144 µs. The function clamps to 800 µs to 2200 µs;
the decoder's 11 bits never reach the clamp. This is rcbench's convention.
rcbench gives no source for it, and others differ: Betaflight's S.BUS input
is widely quoted as 5/8 of raw plus 880 µs, which puts raw 172 at 987.5 µs
and raw 1811 at 2011.9 µs (to confirm against Betaflight's source). A value
on this screen can therefore differ from an FC's by about 12 µs at the ends
and match at the centre. The bar is drawn in raw units, so it shows the
same thing whichever convention reads it. The owner chose this scale on
2026-11-14: µs by `sbus_to_us()`, the bar in raw units
([decision 3](#decisions)).

### Screen

Built in slice 3 ([slice 3 as built](#slice-3-as-built)): `src/ui_sbus.*`,
S.BUS MONITOR, opened from MONITOR on the RX SIGNALS chooser ([MENU](#menu)).
What each line says comes from `src/sbus_view.*`, pure functions of the
status and `millis()`, so the host suite checks every rule at its
boundaries. Header `S.BUS MONITOR` at scale 2, 154 px, x 8 to 161, clear of
BACK at x 170. All other text is scale 1: 5 × 7 px glyphs, 6 px a
character, a string of n characters (6n − 1) px wide.

```
y   0 +------------------------------------------+
      | S.BUS MONITOR                    [ BACK ]|  header 36 px, BACK 170..233 x 2..33
   40 | AUX GP1  100 KBAUD  8E2      (NO SIGNAL) |  strip 40..55
   62 |  1 [=======|=========:========|    ] 1500 |  16 rows, 12 px pitch
   74 |  2 [=======|======:   :        |    ] 1320 |
      |  .                                       |
  242 | 16 [===|          :           |     ]  988 |  last row 242..252
  258 | CH17 OFF  CH18 OFF       14.0 MS  71.4 HZ |
  272 | FRAMES 123456                LOST 12  FS 3|
  286 | PAR 0 FRM 0 BRK 0 OVR 0                   |
  300 | RESYNC 2 FOOTER 0                         |  ends at y 307
  320 +------------------------------------------+
```

| Element | Rectangle (x, y, w, h) | Content |
|---|---|---|
| Strip caption | 8, 44, 137, 7 | `AUX GP1  100 KBAUD  8E2`, 23 characters, 137 px, `C_DIM`; `SIG GP1  100 KBAUD  8E2` when SIG is GP1 |
| Strip chip | 161, 42, 71, 12 at its widest | `uiChip()`: the text's width plus 6 px each side, right edge x 231. Widest `FRAME LOST`, 10 characters, 59 px. The caption ends at x 144, 16 px clear. Drawn only while the job runs |
| Channel row i, i = 0 to 15 | 0, 62 + 12i, 240, 11 | number, bar, value; 1 px between rows |
| Number | right-aligned to x 18, y + 2 | `1` to `16`, `C_DIM`, at most 11 px from x 8 |
| Bar outline | 24, y + 1, 176, 9 | 1 px frame in `C_DIM` |
| Bar fill | 25, y + 2, raw × 174 / 2047 rounded down, 7 | `UI_COL_LIVE` (`C_LIME`); `C_RED` while `FAILSAFE`; `C_DIM` while `NO SIGNAL` or `NO FRAMES`; none before the first frame |
| Bar ticks | x 39, 109, 178; y + 2, 1, 7 | raw 172, 992 and 1811, the fill's end for each: 1000, 1500 and 2000 µs; `C_GRID`, over the fill |
| Value | right-aligned to x 231, y + 2 | µs by `sbus_to_us()`, 3 or 4 characters, at most 23 px, from x 209; `C_TEXT`, `C_RED` while `FAILSAFE`, `C_DIM` while the bars are; `----` in `C_DIM` before the first frame. 9 px clear of the bar |
| Line 1 | 8, 258 and right-aligned to x 231 | `CH17 OFF  CH18 OFF`, 18 characters, 107 px; `14.0 MS  71.4 HZ`, at most `100.0 MS  999.9 HZ`, 18 characters, 107 px, from x 125: 10 px between the two. `C_TEXT` while the last frame is current, `C_DIM` otherwise; `CH17 --  CH18 --` before the first frame, `-- MS  -- HZ` while there is no interval |
| Line 2 | 8, 272 and right-aligned to x 231 | `FRAMES 123456`, at most 13 characters, 77 px; `LOST 9999  FS 9999`, at most 18 characters, 107 px; `C_TEXT` |
| Line 3 | 8, 286, at most 209, 7 | `PAR 9999 FRM 9999 BRK 9999 OVR 9999`, at most 35 characters, 209 px; `C_TEXT` |
| Line 4 | 8, 300, at most 137, 7 | `RESYNC 9999 FOOTER 9999`, at most 23 characters, 137 px; `C_TEXT` |

Counts above 9999 show 9999, and `FRAMES` above 999999 shows 999999, so no
line passes its rectangle. The rate shows at most 999.9 Hz; the interval is
under 100,000 µs by the job's rule ([slice 2 as built](#slice-2-as-built)),
so it shows at most `100.0 MS`. The bars span the whole 11-bit field, so a
receiver that sends past 172 or 1811 shows it. Layout constants
(`SBUS_UI_*` in `src/ui_sbus.h`) have `static_assert`s for every gap, as
the other screens have.

| State | Chip | Bars and values | Line 1 |
|---|---|---|---|
| Before the first frame | `NO SIGNAL`, `C_AMBER` | outlines and ticks, `----` | `CH17 --  CH18 --`, `-- MS  -- HZ`, `C_DIM` |
| `OK` | `C_GREEN` | `UI_COL_LIVE`, values `C_TEXT` | `C_TEXT` |
| `FRAME LOST` | `C_AMBER` | as `OK` | `C_TEXT` |
| `FAILSAFE` | `C_RED` | `C_RED`: the receiver's failsafe values, which is what the user came to check | `C_TEXT` |
| `NO SIGNAL` after frames | `C_AMBER` | the last frame's, `C_DIM`, so the last state before the loss stays readable | `C_DIM` |
| `NO FRAMES` | `C_AMBER` | the last frame's, `C_DIM`, or empty; line 4's footer count or line 3 says why | `C_DIM` |

A chip on `C_AMBER` has its label in `C_PAPER`, on `C_GREEN` and `C_RED` in
`C_ONACCENT`: `uiChip()` takes `C_PAPER` over the two fills that are bright
in one theme and dark in the other, as `theme.h` names them. `FRAME LOST`
holds for 250 ms (`SBUS_VIEW_LOST_HOLD_MS`) from the UI frame that sees
`lostFrames` rise, which is at most one UI frame, 25 ms, after the frame
arrived.

The screen has no control but BACK, and RETRY after a failed bind. Each
region repaints only when its values change; at 16 bars that is at most 16
rectangles of 240 × 11 px a frame.

**When the pin is unavailable**, as in
[pin-claim.md](pin-claim.md#when-the-pin-is-unavailable), the body shows a
message in place of the channels, from x 8 at y 80 and y 94, and no chip:

| Cause | Lines | Colour | Width |
|---|---|---|---|
| No role on GP1 | `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` | `C_AMBER` | 95 and 173 px |
| An ESC answers on GP1, GP1 is SIG | `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT: THE RECEIVER GOES THERE.` | `C_AMBER` | 155 and 209 px |
| KISS frames on GP1, GP1 is AUX | `KISS WIRE ON AUX GP1 IS LIVE.` `UNPLUG IT: THE RECEIVER GOES THERE.` | `C_AMBER` | 173 and 209 px |
| The claim drains DShot (Stopping) | `RELEASING AUX GP1` | `C_DIM` | 101 px |
| Held, the job not bound yet | `TAKING AUX GP1` | `C_DIM` | 83 px |
| The bind refused the clock | `AUX GP1 COULD NOT BE TAKEN (CLOCK)`, and RETRY, the wide button at 6, 268, 228 × 46, scale 2 | `C_AMBER` | 203 px |
| Armed, or another claim holds | `DISARM FIRST`, `WAIT: DSHOT RESTARTING` | `C_AMBER` | backstops, unreachable through the UI |

`AUX` reads `SIG` when SIG is GP1. No role on GP1 is unreachable through
SETTINGS on this board, whose steppers swap GP0 and GP1 (menu-b.md, decision
10); the screen checks it before it claims, and the job's `CORE1_JOB_NO_GP1`
behind it shows the same lines, without RETRY.

**Leaving.** BACK on the monitor is the leave, as on SERVO TESTER: by
[menu-b.md](menu-b.md#the-leave-page) owner decision 3 the unplug page is
the leave page, `src/ui_leave.*`, which opens releasing. The job lets GP1 go
at once, so the page is released on the frame after BACK.

```
y   0 | S.BUS MONITOR                   [  BACK  ]|  BACK drawn disabled
   40 | SIG GP0  AUX GP1                          |  strip, C_DIM
   66 |#1 UNPLUG THE RECEIVER.                    |  step 1, current
   78 |#  DSHOT RESTARTS ON SIG GP0.              |
   92 | 2 RESTART DSHOT.                          |  step 2, C_DIM
  214 | [BACK TO MONITOR]                         |  6, 214, 110 x 46
  268 | [          RESTART DSHOT           ]      |  live 1000 ms after the release
```

| Text | Characters | Width at scale 1 | Room |
|---|---|---|---|
| `UNPLUG THE RECEIVER.` | 20 | 119 px | 35 characters from x 20 |
| `DSHOT RESTARTS ON SIG GP0.`, GP1 is AUX | 26 | 155 px | 35 |
| `DSHOT RESTARTS ON SIG GP1, WHERE`, GP1 is SIG | 32 | 191 px | 35 |
| `THE RECEIVER DRIVES THE LINE.`, its second line | 29 | 173 px | 35 |
| `BACK TO MONITOR` | 15 | 89 px | 110 px button |
| `RELEASING THE PIN`, the note until released | 17 | 101 px | 37 characters from x 8 |

With GP1 as SIG the strip reads `SIG GP1` and step 2 starts at y 104. BACK TO
MONITOR calls `pinClaimKeep()`: the claim returns to Held, the job binds
again and every count starts at 0. RESTART DSHOT calls `pinClaimRestart()`
and returns to MENU. BACK before a claim held, or while it is Stopping,
returns to MENU with DShot resumed at once and no page.

### Scope

The first slice reads inverted S.BUS at 100,000 baud 8E2 on GP1, nothing
else. Later work, each a change to this screen and its job:

| Signal | What it needs |
|---|---|
| Non-inverted S.BUS, as some receivers offer on a pad of their own | INOVER normal instead of inverted: one choice on the screen, same job. Smallest addition |
| S.BUS2 | the servo frames already decode, if the footers 0x04 to 0x34 are S.BUS2's (to confirm). Its telemetry slots on the same wire need a slot decoder; rcbench's STATUS names a 325 µs slot and puts answering in it out of scope, and states no more of the slot timing |
| F.Port | rate, framing and checksum not stated in rcbench; its facts confirmed and a decoder |
| CRSF | a decoder (rcbench STATUS: not started), a rate the PL011 reaches from 150 MHz, and a reply path, since CRSF is bidirectional (to confirm) |
| iBUS | a decoder (rcbench STATUS: not started) and its rate and framing (to confirm) |
| Plain PWM input | pulse width on GP1, channel B of PWM slice 0, which can count while its B input is high; GP0 is a channel A pin and needs a PIO capture. One or two channels, not a receiver's eight |
| Supply on the switched converter | a hold to switch it on, off at BACK, at a heartbeat lapse and at the claim's end, the rules of [kst-programming.md](kst-programming.md#switched-servo-supply) |

## S.BUS output

The owner's choice of 2026-11-14 ([decision 1](#decisions)), option (c) of
the question it answered: an S.BUS output on GP0, UART0's TX pin, inverted
8E2 at 100,000 baud, with the pad's output inverted by OUTOVER, the PL011
sending, no PIO and no DMA. rcbench has no S.BUS encoder: the encoder is
written and tested in rcbench first and then copied here unchanged, as the
decoder was (decision 5). The throttle channel obeys MAX THROTTLE of
SETTINGS (decision 6). Not designed. To design:

- **Frame timing.** The frame period, 7 ms or 14 ms or a choice, and what
  sends each frame on core1: the PL011's 32-entry transmit FIFO holds a
  25-byte frame, and the gap between frames comes from the pass that loads
  the next one.
- **Channel sliders.** How the 16 proportional channels and the two
  digital ones are set on a 240 px wide screen, the throttle channel and
  its ceiling from MAX THROTTLE, and the values at START.
- **Failsafe and frame-lost flags.** Whether the screen sets the failsafe
  and frame-lost bits, to test an FC's reaction to each, and what a frame
  stop on demand looks like.
- **The pin claim's ESC check.** GP0 is SIG in the default wiring, where
  the ESC sits: the output drives an ESC's signal input if one is plugged
  in. What the claim's ESC check (`escCheck`) means for an output on GP0,
  and for the wirings in which GP0 is AUX, is to be designed, after the
  PPM section's rule, which waited while an ESC answered on SIG.
- **SAFE, START, STOP and the heartbeat trip,** as the servo tester and the
  PPM design have them, and the line's level after the release.
- **Electrical.** The level an FC's S.BUS input takes, 3.3 V from GP0, and
  the idle level of the inverted line, high, against the line's low idle
  on the monitor.

## PPM output

Not built, by the owner's decision of 2026-11-14 ([decision
1](#decisions)): no PPM output. This section, and the PPM rows of the
sections after it, stay as the record of the design.

### What PPM output is for

- An FC's PPM receiver input: send known channel values to check its
  channel map, endpoints and mode switches without a transmitter, and stop
  the frames to see its signal-loss failsafe.
- A simulator dongle that turns PPM into a USB joystick.
- A transmitter's trainer port, which takes PPM from a second transmitter.
- A PPM-to-PWM decoder board.

The use is thin. A pilot who owns a receiver owns the transmitter that
drives it, and that transmitter does each of these better: its sticks are
real, its trainer port is made for the cable, and a dongle exists to plug a
transmitter in. Modern receivers and FCs favour serial buses over PPM; which
current FC firmwares still take PPM is not checked here. What this tool adds
is fixed, repeatable values and a frame stop on demand. Unless the owner has
a concrete PPM device to test, the recommendation is not to build it
(question 1 of 2026-11-13, answered by [decision 1](#decisions): no PPM
output).

### Electrical

| Item | Value |
|---|---|
| Pin | SIG, GP0 by default: J5 pin 2 or J4 pin 9 |
| Level | 3.3 V push-pull from IOVDD |
| Polarity | rcbench's: high marks of 300 µs on a low line, low when stopped. Inverted output would be OUTOVER on the pad (rcbench README: "Negative shift is a pad inversion the caller sets"); not offered in the first slice |
| FC input | commonly a 3.3 V timer input; whether a given FC reads 3.3 V marks, and which polarity it expects, is unknown |
| Trainer port, dongle | input level and polarity per product, unknown |
| Supply | none: the board sends signal and ground only, as for a servo |

The polarity names "positive" and "negative" are used the other way round
by some makers, so the screen names neither; the README states the level
of the marks.

### Frame

| Item | Value | Source |
|---|---|---|
| Channels | 8 | `PPM_MAX_CHANNELS` |
| Mark | 300 µs | `PPM_DEFAULT_MARK_US` |
| Channel range carried | 400 µs to 2500 µs, refused outside | `PPM_CHANNEL_MIN_US`, `PPM_CHANNEL_MAX_US` |
| Shortest sync gap | 3000 µs, refused below | `PPM_SYNC_MIN_US` |
| Frame | 23,809 µs: `out_ppm_bind(pin, 8, 42)` | see below |
| Sync gap | 7,509 µs with every channel at 2000 µs; 3,509 µs at 2500 µs | 23,809 − 8 × width − 300 |

rcbench's default frame is 22.5 ms, but `out_ppm_bind()` takes a rate in
whole hertz and refuses a rate whose frame cannot carry every channel at
2500 µs: `ppm_min_frame_us(8)` is 8 × 2500 + 300 + 3000 = 23,300 µs.
43 Hz is 23,255 µs and is refused; 42 Hz is 23,809 µs, the fastest rate
that binds eight channels. The frame is therefore 23.8 ms, not 22.5 ms.
Whether an FC, dongle or trainer port accepts it is unknown; the usual PPM
decoder frames on the sync gap, not on the frame length, but that is to
confirm per device. The program counts in microseconds: its divider is
`clk_sys` in MHz, 150 at 150 MHz.

### Safety

The output is a throttle to whatever listens: an FC passes it to motors
once armed, and an ESC that reads its pin as PWM, OneShot or another
protocol sees 300 µs marks it was not made for. What an ESC does with
them is not measured. The rules are the servo tester's, with the throttle
channel added:

| | PPM OUTPUT |
|---|---|
| Off state | SAFE: SIG bound by `out_ppm_bind()`, held low, no frames |
| Starting | HOLD TO START, `ARM_HOLD_MS` 1000 ms, on `ui_hold` |
| First frame | the throttle channel at 1000 µs; channels 1 to 4 otherwise at 1500 µs; channels 5 to 8 at 1000 µs |
| Throttle channel | channel 3 by default; any of 1 to 8, chosen while SAFE |
| Throttle ceiling | 1000 µs + 1000 µs × MAX THROTTLE / 2000; 1200 µs at the default 400 of 2000 (20 %) |
| Stopping | STOP, on press: `out_ppm_stop()`, the pin low at once |
| UI stalls 250 ms | `core1UiAlive()` false while RUNNING: `out_ppm_stop()`, `Tripped` latched, chip `STALLED`, a new HOLD TO START needed |
| Idle | none |

Why each rule:

- Channels 5 to 8 start at 1000 µs, not at the centre, because they are
  switch channels on most setups, and 1500 µs can fall inside an FC's ARM
  range.
- The throttle channel starts low and its slider stops at the ceiling of
  SETTINGS, the same ceiling the DShot throttle obeys. Full travel takes a
  higher MAX THROTTLE, a deliberate change.
- STOP and a trip stop the frames rather than send a low throttle, as the
  servo tester stops pulses (servo-tester.md, decision 6): a listener with
  no frames goes to its own signal-loss failsafe. How long each FC waits
  before it does is FC-specific and unknown.
- `out_ppm_stop()` halts the DMA and the SM where they are, so the last
  frame is cut short: the listener sees fewer than 8 channels and then
  silence. Whether a decoder takes a short frame as a frame of fewer
  channels is unknown and is a bench test. A stop at the frame boundary
  needs the DMA's transfer count, which `out_ppm.h` does not expose; it is
  later work, upstream in rcbench first if the bench shows the need.

### The job

`src/ppm_job.cpp`, with its decisions in pure functions on `PpmCore`, as
the servo job:

```c
enum class PpmCmdKind : uint8_t { Safe, Start, Channels, Throttle };
struct PpmCmd    { uint16_t seq; PpmCmdKind kind; uint16_t us[8];
                   uint8_t thrCh; uint16_t ceilingUs; };
struct PpmStatus { uint16_t ackSeq; bool running, tripped;
                   uint8_t bindError; uint16_t outUs[8]; };

bool ppmCoreTake(PpmCore *c, const PpmCmd *cmd, bool uiAlive);
void ppmCoreStep(PpmCore *c, bool uiAlive);  // sets c->write or c->stop
```

- **Bind.** Pull-down on SIG, then `out_ppm_bind(sig, 8, 42)`. A refusal is
  `CORE1_JOB_PIO`: the driver does not say whether the PIO or the DMA ran
  out, so the screen says `GP0 COULD NOT BE TAKEN (PIO)` for both. The
  firmware builds the driver with `OUT_PPM_MAX_PINS=1`, one slot.
- **Step.** Take a command; on a heartbeat lapse while RUNNING, stop and
  latch the trip; while RUNNING, `out_ppm_write()` the channels, which
  rewrites the buffer only when a width changed; a refused write stops the
  output and reports SAFE. `Start` is refused while the heartbeat has
  lapsed. Core1 clamps every channel to 1000 µs to 2000 µs and the
  throttle channel to `ceilingUs`, behind the screen's own clamp.
- **Release.** `out_ppm_release(sig)`, which halts, frees the SM, the
  program and both DMA channels and leaves the pin a plain input; then the
  pull-down, so the line does not float high on DShot's pull-up until
  DShot binds. Done at once.

A drag reaches core1 at 40 Hz, one `Channels` command a UI frame. The frame
picks up a change at its next start; a frame caught mid-update carries some
channels from the update before (`out_ppm.h`).

### Screen

Header `PPM OUTPUT` at scale 2, 118 px. Rows are sliders of `ui_slider`
with delta drag and no tap-to-set, so no tap jumps a channel. A slider is
drawn disabled while SAFE: START sets the first frame, not the sliders.

```
y   0 +------------------------------------------+
      | PPM OUTPUT                       [ BACK ]|  header 36 px
   40 | GP0 8 CH 42 HZ 23.8 MS            (SAFE) |  strip 40..55
   62 | THROTTLE CH3 STARTS AT 1000 US.          |  two notes
   71 | PPM ON GP0 IS NOT FOR AN ESC.            |
   80 | CH1     [==========O==========]     1500 |  8 rows, 22 px, 25 px pitch
  105 | CH2     [==========O==========]     1500 |
  130 | CH3 THR [O===!--------------- ]     1000 |  ! = ceiling at 1200 US
  155 | CH4     [==========O==========]     1500 |
  180 | CH5     [O====================]     1000 |
  205 | CH6     [O====================]     1000 |
  230 | CH7     [O====================]     1000 |
  255 | CH8     [O====================]     1000 |  last row 255..276
  282 | [   HOLD TO START    ]   [ THR CH3 ]     |  32 px tall
  320 +------------------------------------------+
```

| Element | Rectangle (x, y, w, h) | Content |
|---|---|---|
| Strip caption | 8, 44, 131, 7 | `GP0 8 CH 42 HZ 23.8 MS`, 22 characters |
| Strip chip | 179, 40, 53, 16 | `SAFE` `C_GREEN`, `RUNNING` `C_RED`, `STALLED` `C_MAGENTA`: at most 7 characters, 41 px plus 6 px each side (`uiChip()`). The caption ends at x 138, 40 px clear |
| Notes | 8, 62, 224, 7 and 8, 71, 224, 7 | SAFE: as drawn, 31 and 29 characters. RUNNING: `THROTTLE CEILING 1200 US (20 %).`, 32 characters. STALLED: `STOPPED: SCREEN STALLED` and `HOLD TO START AGAIN.` |
| Row i, i = 0 to 7 | 8, 80 + 25i, 224, 22 | label, slider, value; 22 px is `UI_TAP_MIN` |
| Label | 8, y + 8, at most 41, 7 | `CH1` to `CH8`, `CH3 THR` on the throttle channel in `C_AMBER`, at most 7 characters |
| Track | 54, y + 6, 146, 10 | 1000 µs at x 54, 1500 µs at x 126, 2000 µs at x 199: 1000 / 145 = 6.9 µs a pixel |
| Knob | `THUMB_W` 14 px wide, the track's height plus 6 px, 16 px tall (`ui_slider.cpp`) | centred on the value and clamped inside the track; `UI_SLIDER_OVERHANG` 3 px above and below it |
| Ceiling mark | x 54 + 145 × (ceiling − 1000) / 1000, y, 1, 22 | throttle row only, `C_AMBER`: x 83 at 1200 µs; the track past it `C_DIM` |
| Value | right-aligned to x 231, y + 8 | µs, 4 characters, 23 px |
| HOLD TO START | 8, 282, 150, 32 | `STOP` in `C_RED` while RUNNING, acting on press |
| THR CH | 162, 282, 70, 32 | `THR CH3`, 7 characters, 41 px; a tap steps the throttle channel 1 to 8 and back to 1; disabled while RUNNING |

A slider moves in 5 µs steps; a drag of one pixel is 6.9 µs. When the
throttle channel changes, the label tag and the ceiling mark move with it.
The unavailable-pin texts are the servo tester's, with `PPM` for `SERVO`:
`AN ESC ANSWERS ON GP0.` `UNPLUG IT: PPM MARKS MAY READ AS` `THROTTLE.`
The unplug page reads `UNPLUG PPM LEAD.` at scale 2, 16 characters, 190 px,
then `DSHOT RESTARTS ON GP0 AND READS AS` `PULSES EVERY 1000 US.` at scale 1.

## Resources

During either claim the DShot pump and the KISS receiver are down, so pio2
is empty ([pin-claim.md](pin-claim.md#pio-and-dma-budget)).

| Block or channel | ESC running | RX MONITOR claim | PPM OUTPUT claim (PPM: not built, decision 1) |
|---|---|---|---|
| pio0 | backlight, 6 words, 1 SM | backlight | backlight, or `ppm_tx` |
| pio1 | SDIO, 32 of 32 words, 2 SMs | SDIO | SDIO |
| pio2 | DShot 16 + KISS 9 = 25 words, 3 SMs | empty | `ppm_tx`, 4 words, 1 SM, or empty |
| UART0 | unused | GP1 receive | unused |
| DMA | 3 of 16: SDIO 2, panel 1 | 3 of 16 | 5 of 16: plus `ppm_tx`'s data and restart channels |
| PWM slice 0 | free | free | free |

`out_ppm_bind()` lets the SDK pick the block
(`pio_claim_free_sm_and_add_program_for_gpio_range()`), which searches pio2
first; GP0 is in reach of every block. Neither tool needs the claim for
resources: the claim exists for the pin.

## MENU

Built in slice 3 ([slice 3 as built](#slice-3-as-built)). The owner decided
on 2026-11-14 for one MENU row, RX SIGNALS, that opens a chooser with
MONITOR and OUTPUT ([decision 4](#decisions)). In menu structure B
([menu-b.md](menu-b.md), decision 32) the row comes after SERVO PROGRAMMER,
row 4 of seven. Slice 9 of menu-b.md built the row drawn disabled with
`NOT BUILT` (decision 35 there), until a screen behind it exists; slice 3
puts the chooser behind it.

**The row.** RX SIGNALS, 10 characters at scale 2, 118 px from x 16 to
133. It has no status, as no tool row has one (menu-b.md, decision 18), so
its widest status in `UI_MENU` is 0. It opens the chooser, which claims
nothing but opens tools that claim, so it is refused while the pin claim is
not Free, as PROGRAMMER's row is (`freePins`): drawn disabled under the
strip's `DSHOT RESTARTING ON SIG GP0`, a tap opens nothing.

**The chooser**, `src/ui_rx_signals.*`, follows PROGRAMMER's pattern
([menu-b.md](menu-b.md#programmer-the-chooser)): one 228 × 46 px button per
tool at an 80 px pitch, its lines under it from 4 px below at a 12 px
pitch, `C_DIM`.

```
y   0 | RX SIGNALS                      [  BACK  ]|  header 36 px
   40 | SIG GP0  AUX GP1                          |  strip, C_DIM
   62 | [             MONITOR             ]       |  6, 62, 228 x 46
  112 | S.BUS FROM A RECEIVER, ON GP1.            |  C_DIM
  142 | [              OUTPUT             ]       |  6, 142, 228 x 46: disabled
  192 | S.BUS TO A FLIGHT CONTROLLER.             |  C_DIM
  204 | NOT BUILT                                 |  C_DIM; ends at y 210
  320 +-------------------------------------------+
```

| Text | Characters | Width, scale | From x 8 to |
|---|---|---|---|
| title `RX SIGNALS`, MENU's label | 10 | 118 px, 2 | 125 |
| `SIG GP0  AUX GP1` | 16 | 95 px, 1 | 102 |
| `MONITOR` | 7 | 82 px, 2 | centred in 228 px |
| `OUTPUT` | 6 | 70 px, 2 | centred |
| `S.BUS FROM A RECEIVER, ON GP1.` | 30 | 179 px, 1 | 186 |
| `S.BUS TO A FLIGHT CONTROLLER.` | 29 | 173 px, 1 | 180 |
| `NOT BUILT` | 9 | 53 px, 1 | 60 |

A tap on MONITOR opens S.BUS MONITOR through `uiChooseTool()`, as
PROGRAMMER's entries open their tools: the monitor's entry point asks for
the claim on that frame, and its origin is MENU, so its BACK before a claim
held and its RESTART DSHOT return to MENU. OUTPUT, the S.BUS output, is not
designed (decision 5): its entry has no screen, so it is drawn disabled
(`uiButton()`'s outline, the label `C_GRID`), reported disabled in the
control table, takes no pressed look and opens nothing. Its `NOT BUILT` is
the status MENU's row carried before (menu-b.md, decision 35), as a line
under the button. BACK returns to MENU. The entries are `UI_RX_ENTRIES` in
`src/ui_registry.cpp`, with a build-time check that they fit as
PROGRAMMER's do.

### Record: two rows, 2026-11-13

The rest of this section is the design of 2026-11-13, made for MENU before
structure B, with two rows, RX MONITOR and PPM OUTPUT; it stays as that
record.

On 2026-11-13 MENU had seven rows and scrolled 36 px. The two tools sat
after the servo tools, SERVO TESTER, KST SERVO and HITEC SERVO, which take
the signal pins as they do, and before SETTINGS. SETTINGS was the sixth row,
whole at scroll 0. With the new rows before it, SETTINGS became the eighth
or ninth row and was below the fold again; question 4, now decision 4,
decided between that and placing the new rows after SETTINGS:

| | Rows | Content | Scroll range |
|---|---|---|---|
| 2026-11-13 | 7 | BEEP THE ESC, AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO, SETTINGS, SD LOG | 0 to 36 px |
| 2026-11-13, RX MONITOR only | 8 | ... HITEC SERVO, RX MONITOR, SETTINGS, SD LOG | 0 to 78 px |
| 2026-11-13, both tools | 9 | ... HITEC SERVO, RX MONITOR, PPM OUTPUT, SETTINGS, SD LOG | 0 to 120 px |

The scroll range is rows × 42 px − 258 px, by the rules of
[menu.md](menu.md#menu). MENU structure B ([menu-b.md](menu-b.md)), now
built, places the rows; question 4 held until then. The owner's answer
is decision 4.

| Row | Label | Fit | Status |
|---|---|---|---|
| RX MONITOR | 10 characters at scale 2: 118 px, x 16 to 133 | 35 px clear of `PINS BUSY`, which starts at x 169; the check asks for 8 px | `GP1`; `PINS BUSY` |
| PPM OUTPUT | 10 characters: 118 px | as above | `GP0`; `PINS BUSY` |

`S.BUS MONITOR` would be 154 px, x 16 to 169, and run onto the first pixel
of `PINS BUSY`, so the registry's fit check refuses it. `RX MONITOR` also
names the screen that the later protocols join. The screen's own title is
`S.BUS MONITOR`.

In structure B MENU's tool rows show no pin status and `PINS BUSY` is gone
([menu-b.md](menu-b.md#decisions) decisions 15 f and 18): a row refused
while the claim is not Free is drawn disabled under the strip's
`DSHOT RESTARTING ON SIG GP0`. The one row, RX SIGNALS, opens the chooser
above.

## Interlocks

Each tool is one `PinUser`, one `Core1Job` and one row in `PIN_USERS`:

| PinUser | Core1Job | `unplug` | `escCheck` | Roles |
|---|---|---|---|---|
| `Sbus` | `Sbus` | true | false, and the screen's own check when GP1 is SIG (below) | `PIN_SIG \| PIN_AUX` when GP1 is AUX, `PIN_SIG` when GP1 is SIG |
| `Ppm` | `Ppm` | true | true | `PIN_SIG` |

The `Ppm` row and the PPM rules below are not built, by decision 1. The
S.BUS output's row is to be designed ([S.BUS output](#sbus-output)).

- **Armed and Busy** apply to both, as to every claim. Opening MENU disarms;
  the 250 ms zero-throttle drain runs before either job binds.
- **The ESC check.** PPM waits while an ESC answers on SIG within 500 ms:
  its marks are pulses into the ESC. The monitor drives nothing, but the
  receiver does. In the default wiring GP1 is AUX, no ESC signal is on it,
  and the claim's check is off for the monitor (`escCheck` false): an ESC
  on SIG only loses its frames, which the drain ends with zero throttle
  first. When GP1 is SIG, the receiver goes onto the SIG net, where an ESC
  may still sit, and the receiver's S.BUS output would drive the ESC's
  signal input. In those wirings the monitor waits while an ESC answers on
  GP1 within `ESC_LINK_STALE_MS`, 500 ms, and shows `AN ESC ANSWERS ON
  SIG GP1.` `UNPLUG IT: THE RECEIVER GOES THERE.` (26 and 35 characters,
  155 px and 209 px). `PinUserInfo.escCheck` is one flag per user, so the
  screen makes the check itself before each `pinClaimBegin()`, with
  `escFieldFresh()` on the snapshot's `lastRpmMs`, the rule
  `pinClaimCheck()` uses, after that function's Armed and Busy rules, and
  asks again every frame; the claim's table stays as it is. Like the
  claim's check, it misses an ESC that does not answer in bidirectional
  DShot.
- **KissLive** refuses the monitor while a KISS frame arrived on GP1 within
  500 ms in the default wiring: a telemetry wire is where the receiver
  goes. PPM does not ask for AUX.
- **No output outside the screen.** PPM frames exist only between HOLD TO
  START and STOP, BACK or a trip. The claim ends only through BACK, and no
  other screen is reachable while it holds. The monitor drives no pin.
- **What the claim must guarantee before DShot returns.** The monitor is an
  input, but the receiver on GP1 is an output. Before `escTaskResume()`:
  the job is idle, UART0 is off, GP1 has no input inversion and is a plain
  input. In the default wiring DShot restarts on GP0 and the receiver's
  line meets at most the KISS receiver's pull-up on GP1. When GP1 is SIG,
  DShot's transmitter drives the line the receiver drives: two outputs
  against each other once a millisecond. The leave page covers both
  wirings with one rule, UNPLUG THE RECEIVER.; its detail lines name the
  case.
- **DShot restart.** As for every claim: `out_dshot_bind()` and
  `pioUartRxBegin()` set their own pulls and overrides. The tests check
  that neither tool leaves an override behind.
- **Flash.** No settings save during either claim, as for every claim.

## Porting

| rcbench file, at 5ef42efd | Here | How |
|---|---|---|
| `protocols/sbus/sbus.c`, `include/sbus.h` | `lib/rcbench/protocols/sbus/` | copied unchanged |
| `protocols/ppm/ppm.c`, `include/ppm.h` | `lib/rcbench/protocols/ppm/` | copied unchanged |
| `protocols/ppm/rp2350/out_ppm.c`, `out_ppm.h`, `ppm.pio` | `lib/rcbench/protocols/ppm/rp2350/` | copied unchanged; built by the firmware only, `OUT_PPM_MAX_PINS=1` |
| `test/host/test_sbus.c` | `test/rcbench/` | copied unchanged, 12 cases |
| `test/host/test_ppm.c` | `test/rcbench/` | copied unchanged, 9 cases |
| `shared/outputs/include/outputs.h` | `test/rcbench/outputs.h` | adapted: `OUT_FLOOR_US` 400 and `OUT_CEILING_US` 2500 only, the two values `test_ppm.c` holds the module to, as `test/rcbench/link_pages.h` does for Hitec |
| the modules' `README.md` and `CMakeLists.txt` | none | MiniRcBench lists its sources in its own `.cmake` and `.mk` files |

The PPM rows are not built, by decision 1. The S.BUS output's encoder
comes from rcbench once it is written there (decision 5).

`lib/rcbench/README.md` gains the rows and the commit. Copied files keep
their text and their SPDX line.

New code in `src/`:

| File | What |
|---|---|
| `sbus_job.h`, `.cpp` | `SbusCore`, the UART bind and release, the drain, the status |
| `sbus_view.h`, `.cpp` | the link state and the text and colour of each line, pure |
| `ui_sbus.h`, `.cpp` | the S.BUS MONITOR screen and its leave page |
| `ui_rx_signals.h`, `.cpp` | RX SIGNALS, the chooser behind the MENU row |
| `ppm_job.h`, `.cpp` | `PpmCore`, the bind, step and release on `out_ppm.h` (PPM: not built, decision 1) |
| `ui_ppm.h`, `.cpp` | the PPM OUTPUT screen and its unplug page (PPM: not built, decision 1) |
| `rx.cmake`, `ppm.cmake` | the features' firmware sources (`ppm.cmake`, PPM: not built, decision 1) |

Shared files each slice edits by a line or a few (the second job, user and row
were PPM's: not built, decision 1; the S.BUS output's are to be designed):
`core1_job.h` (two jobs, `CORE1_JOB_NO_GP1`), `core1_job.cpp` (the job table),
`pin_claim.h` (two users and rows), `ui_registry.cpp` (one line each),
`CMakeLists.txt`, `test/Makefile`, `test/test_main.cpp`, `test/fakes.*`,
`test/stubs/hardware/` and `.github/workflows/ci.yml`. The Config permutations
job of `ci.yml` compiles `src/*.cpp` with two hand-kept `-I` lists, which need
`lib/rcbench/protocols/sbus/include`, and `lib/rcbench/protocols/ppm/include`
and `lib/rcbench/protocols/ppm/rp2350` (PPM: not built, decision 1), once a file
in `src/` includes their headers, as Hitec step 1 added its own (commit
424014a). The screen slices also edit `tools/check_wiki.py`, for the pair of new
wiki pages, and `tools/make_previews.py`, for the new preview, as Hitec step 5
did (commit 5e1666d).

## Testing

### Host

`test/rcbench/test_sbus.c` and `test_ppm.c` run as C11 programs from
`make run`, as the other rcbench tests do.

New fakes and models:

- **A microsecond clock.** `test/fakes.cpp` counts microseconds, and
  `millis()` is that count divided by 1000; `fakeAdvanceUs()` moves it, so
  bytes 120 µs apart can be stamped. `sleep_us()` still moves only whole
  milliseconds.
- **A UART0 register fake.** The stub of `hardware/uart.h` reads UARTFR
  and UARTDR from a receive queue of `UARTDR` words, each due at a time:
  the RXFE flag, a 32-entry FIFO with overrun, the baud computation of SDK
  2.1.1 from a settable `clk_peri`, the 8N1 format of `uart_init()`, the
  wait of `uart_set_format()` and a character caught in its middle;
  `gpio_set_inover()` is recorded per pin.
- **An S.BUS line model**, `test/sbus_line.h`: frames from 16 values and
  flags, bytes 120 µs apart, periods of 7 ms and 14 ms, even parity
  computed; injected parity, framing, break and overrun errors, a cut
  frame, a footer variant.
- **An `out_ppm.h` fake**: records each bind, write, stop and release with
  its channels and the time.
- **A PIO model of `ppm.pio`**, `test/pio_ppm_emu.py`, in the manner of
  `pio_backlight_emu.py`: four instructions with side-set, one cycle per
  µs; each run plays for exactly its duration, the mark is 300 µs, and a
  frame sums to 23,809 µs.

Checked:

- the monitor's bind for each wiring of the table above, the refusal with
  no role on GP1, the wait while an ESC answers on GP1 when GP1 is SIG and
  its end 500 ms after the last eRPM frame, no such wait when GP1 is AUX;
  the pad order (function, then INOVER, then pull) and the release leaving
  no override and the pull-down on;
- the clock check at 150 MHz (100,000 baud exactly) and at a `clk_peri`
  that misses 1 %;
- a clean stream at 7 ms and 14 ms: every frame decodes, the interval and
  rate, the flags; drains late by up to 1,000 µs split no frame;
- a parity, framing or break error in a frame: the byte is not fed, that
  frame is lost, the next one decodes; each count by its kind; an overrun
  counted;
- the link states at their boundaries: `NO SIGNAL` at 100 ms and not at
  99 ms, `NO FRAMES` with clean bytes and no frame, `FAILSAFE`, `FRAME
  LOST` held 250 ms, `OK`; across a `millis()` wrap;
- a new visit starts its counts at 0;
- the PPM core, not built by decision 1: SAFE writes nothing; START's first
  frame (the throttle channel and channels 5 to 8 at 1000 µs, the rest at
  1500 µs); the clamp to 1000 µs to 2000 µs and to the ceiling; the trip at
  250 ms and not at 249 ms, latched, cleared only by START; STOP on the next
  pass; a refused write; the bind refusal; the release and the pull-down;
- the S.BUS MONITOR screen through MENU and the RX SIGNALS chooser by
  control id: it opens and BACK passes the leave page; the
  unavailable texts; every string within its rectangle; the preview
  `docs/sbus-preview.png`;
- the MENU row and its chooser: the row acts while the claim is Free and is
  drawn disabled while it is not; the chooser claims nothing, MONITOR
  opens the monitor, OUTPUT is drawn disabled under `NOT BUILT` and opens
  nothing;
- the PPM screen, not built by decision 1: its row opening it, HOLD TO
  START at 1000 ms and not at 999 ms, STOP on press, sliders and THR CH
  disabled while SAFE and RUNNING respectively, the throttle slider at its
  ceiling, the preview `docs/ppm-preview.png`.

`make syntax` does not typecheck `sbus_job.cpp`: the suite links all of it,
the UART calls against the UART0 fake. `make syntax` typechecks no rcbench
driver: `out_ppm.c` is built by the firmware only, as the KST and Hitec line
drivers are, and checking it on the host would need a C rule and stubs of
its own, `ppm.pio.h` among them. The coverage table of STATUS.md is
regenerated by `tools/coverage.py` in each slice that adds a file to
`src/`.

### Bench

Only hardware settles these:

- **S.BUS.** The receiver's output level and idle level on a scope at GP1,
  before the first connection; frames decoded from at least one receiver
  per make at hand, with the frame period it sends; zero parity and framing
  errors over 10 minutes; channel order against the transmitter's channel
  monitor; failsafe with the transmitter off, both hold and no-pulses
  modes; `NO SIGNAL` within 100 ms plus one UI frame of unplugging; GP1's
  level after unplugging (RP2350-E9) and what the counts show; KISS on GP1
  after the claim, to show no inversion is left.
- **PPM,** not built by decision 1. On a scope at GP0: the 300 µs mark, each
  channel within 1 µs, the 23,809 µs frame, the line low while SAFE and after
  STOP; STOP and a heartbeat trip (core0 halted in a debugger) with the cut
  last frame; an FC's receiver page reading the eight channels, its reaction
  to the 23.8 ms frame, to the cut frame and to the loss of frames; a dongle
  or trainer port if the owner has one; the line after the release and through
  the unplug page.

## Work breakdown

Each slice builds, passes the host suite and merges alone. Slice 2 depends
on slice 1a and slice 4 on slice 1b, and neither on the other; their shared
files differ by one line each. Slice 1a is built.

| Slice | Content | Files | Lines, rough |
|---|---|---|---|
| 1a. S.BUS copies, built | `sbus.c`, `sbus.h`, `test_sbus.c`; `test/rx.mk`; the `test/Makefile` lists, `lib/rcbench/README.md`, README, STATUS, CHANGELOG | 3 copied, 1 new, 5 edited | 695 copied; 80 new or changed |
| 1b. PPM copies: not built, by decision 1 | `ppm.c`, `ppm.h`, `test_ppm.c`; the `outputs.h` stub, `test/ppm.mk`; the `test/Makefile` lists, `lib/rcbench/README.md`, README, STATUS, CHANGELOG | 3 copied, 2 new, 5 edited | 441 copied; 80 new or changed |
| 2. S.BUS job, built | `sbus_job.*`, `rx.cmake`, `CORE1_JOB_NO_GP1`, the job and user rows, the microsecond clock, the UART fake, `sbus_line.h`, the job's tests, the `-I` lists of `ci.yml` | 3 new in `src/`, 2 new in `test/`, 15 edited, 7 documents | about 480 in `src/`; 1,300 in `test/` |
| 3. S.BUS screen, built | `sbus_view.*`, `ui_rx_signals.*`, `ui_sbus.*`, the registry line, the screen and MENU tests, `test_ids.cpp`, the preview, `tools/check_wiki.py`, `tools/make_previews.py`, README section, wiki pages in English and German, STATUS, CHANGELOG, this note's status | 6 new in `src/`, 2 new and 10 edited elsewhere | 650 in `src/`; 600 in `test/`; 250 of documentation |
| 4. PPM job: not built, by decision 1 | the `out_ppm` driver copies, `ppm_job.*`, `ppm.cmake`, the job and user rows, the `out_ppm.h` fake, `pio_ppm_emu.py`, the job's tests, the `-I` lists of `ci.yml` | 3 copied, 3 new in `src/`, 3 new in `test/`, 6 edited | 490 copied; 300 in `src/`; 600 in `test/` |
| 5. PPM screen: not built, by decision 1 | `ui_ppm.*`, the registry line, the screen tests, the preview, `tools/check_wiki.py`, `tools/make_previews.py`, README, wiki pages in English and German, STATUS, CHANGELOG | 2 new in `src/`, 2 new and 10 edited elsewhere | 600 in `src/`; 500 in `test/`; 200 of documentation |
| 6. Bring-up | the bench tests above, each tool on its own | none | none |

Slices 1b, 4 and 5 are not built, by the owner's decision 1 of
2026-11-14. The S.BUS output has no slices yet: its encoder in rcbench
comes first (decision 5), then its design ([S.BUS output](#sbus-output)).
Slices 2 and 3 are built; slice 3 designed the RX SIGNALS row's chooser
(decision 4) with the screen ([MENU](#menu), [screen](#screen)).

### Slice 2 as built

`src/sbus_job.h` and `.cpp`, `src/rx.cmake`, the `Sbus` rows of
`Core1Job` and `PinUser`, `CORE1_JOB_NO_GP1`; in the host suite the
microsecond clock (`fakeAdvanceUs()`), the UART0 fake, `test/sbus_line.h`
and `test/test_sbus_job.cpp`. Choices the note left open:

- **One count per bad word.** A word with BE counts as a break only, one
  with FE and not BE as a framing error only, one with PE alone as a parity
  error. OE is counted beside any of them, and its byte is fed only when no
  other flag is set. The line model flags a break as BE and FE, as a
  character low throughout.
- **The mean interval starts again after a lost link.** An interval of
  `SBUS_LOST_MS`, 100 ms, or more is no frame period: it empties the ring
  of 16, and `intervalUs` reads 0 until the next frame, rather than showing
  a rate from a 10 s gap for the next 16 frames.
- **Frame rate.** `sbusRateDeciHz()` gives the rate in tenths of a hertz,
  rounded to the nearest: 714 at 14,000 µs, 1429 at 7,000 µs.
- **The roles.** `sbusClaimRoles()` gives the roles of the wiring table of
  [receive path](#receive-path), so the screen of slice 3 asks for the role
  that is GP1; the job binds GP1 from whichever role carries it.
- **A clock refusal binds nothing.** The pad is set before `uart_init()`, as
  the note orders; when the rate is refused, the bind turns UART0 off and
  leaves GP1 as the release does.
- **A bind into a live line counts nothing** until the line has been seen
  quiet for 2000 µs, and the bind empties the FIFO after `uart_set_format()`
  ([the job](#the-job)). A frame that starts within 2000 µs of the bind is
  given up.
- **The release clears the status**, so a read after the claim finds no
  frame and every count at 0.

The UART0 fake hears the line only while UART0 is on and GP1 is on the UART
function with INOVER inverted. It reads each word in the format set: at 8N1 the
parity bit is taken as the stop bit, so a parity bit of 0 is a framing error. A
character whose start bit came before the receiver was on loads as a framing
error. `uart_set_format()` on a running UART takes the SDK's wait, 149 µs at
100,000 baud from 150 MHz, with the receiver off, and only the character under
way completes. `fakeUartInitTakes()` keeps the receiver at 8N1 for a while
before `uart_set_format()`, as a slow core would. The fake models the 32-entry
FIFO, a word lost to a full FIFO and OE on the next word that enters, and the
divisors of SDK 2.1.1's `uart_set_baudrate()`. It does not model the bits on
the wire or what a line that is not inverted reads; what a PL011 loads for a
character caught in its middle, and that the next character loads aligned, is
the fake's guess. The inversion on the pin, the receiver's level and the
PL011's flags on a real line are not measured (bench, below).

### Slice 3 as built

`src/sbus_view.*`, the formatter: the link state, the frame-lost hold, the
ink of the channels, the bar's fill and every line's text, pure.
`src/ui_sbus.*`, S.BUS MONITOR, with `UiScreenId::Sbus`, its claim, its
messages and its leave page. `src/ui_rx_signals.*`, RX SIGNALS, the chooser,
`UiScreenId::RxSignals`, and its entries `UI_RX_ENTRIES` in
`src/ui_registry.cpp`; MENU's RX SIGNALS row now opens it, and the stub of
`src/ui_menu.cpp` with `UI_MENU_NOT_BUILT` is gone. `uiChip()` in
`src/ui_widgets.cpp` draws its label in `C_PAPER` over `C_AMBER` and
`C_INK`. In the host suite, `test/test_sbus_ui.cpp`, and the MENU, id and
tester tests where they named the row. Choices the note left open:

- **The chooser's look.** PROGRAMMER's pattern: buttons 228 × 46 px at an
  80 px pitch with their lines under them. OUTPUT's `NOT BUILT` is a
  `C_DIM` line under its button, where the chooser puts an entry's state,
  not a status at the right of the button as on MENU's rows. Each entry
  has a line that says what it is for.
- **The chooser's strip,** `SIG GP0  AUX GP1`: the monitor needs GP1, and
  the strip says which role it has before the tap.
- **The chooser's code.** A file of its own, `ui_rx_signals.*`, with a table
  of two fields per entry and a build-time fit check in
  `ui_registry.cpp`, rather than PROGRAMMER's `UiProgEntry`, whose power
  lines are a servo's.
- **The row.** No status, as decision 18 of menu-b.md gives the tool rows;
  refused while the claim is not Free through `freePins`, as PROGRAMMER.
- **The strip caption** names GP1 by its role and number, `AUX GP1` or
  `SIG GP1`, by menu-b.md decision 10. `AUX GP1  100 KBAUD  8E2  INV`
  would end at x 174, past the widest chip at x 161, so `INV` is dropped,
  by menu-b.md decision 14's rule for a strip too long: items go from the
  right, never the spaces. Every S.BUS line is inverted ([S.BUS
  frame](#sbus-frame)).
- **The bar's colour.** `UI_COL_LIVE` (`C_LIME`) for a live frame, in place
  of the note's `C_INK`, by menu-b.md decision 11: the fill is a live
  reading, as the throttle fill is. Values are `C_TEXT`; the ticks
  `C_GRID`, drawn over the fill.
- **`NO FRAMES` dims the bars** as `NO SIGNAL` does: in both no frame
  arrived for 100 ms, so the values on screen are old.
- **Line 1** is dim whenever the bars are; `CH17 --  CH18 --` before the
  first frame, since `OFF` would be a reading not taken. The interval
  shows one decimal, rounded to the nearest 0.1 ms, and the rate at most
  `999.9 HZ`. Lines 2 to 4 are counts and stay `C_TEXT`.
- **The FRAME LOST hold** starts on the UI frame that sees `lostFrames`
  rise, at most 25 ms after the frame came in, and BACK TO MONITOR forgets
  it with the counts.
- **No chip until the job runs.** Before that the link has no state; the
  body shows the claim's message.
- **Messages name the role:** `AN ESC ANSWERS ON SIG GP1.`, not
  `ON GP1.`; `AUX GP1 COULD NOT BE TAKEN (CLOCK)`, not `GP1 COULD ...`;
  `RELEASING AUX GP1` and `TAKING AUX GP1` while the claim drains and
  binds, as SERVO TESTER reads `RELEASING SIG GP0`. RETRY is the wide
  button. The job's `CORE1_JOB_NO_GP1` shows the screen's own lines without
  RETRY.
- **The ESC check's order.** The screen's check runs only where
  `pinClaimCheck()` would not refuse for Armed or Busy, so a backstop
  refusal reads as it does on the other screens.
- **The leave page,** not the unplug page of the note: by owner decision 3
  of menu-b.md every unplug page is the leave page. It opens releasing, as
  SERVO TESTER's, with `UNPLUG THE RECEIVER.` and its detail lines as step
  1, `RESTART DSHOT.` as step 2, and BACK TO MONITOR in the left button, in
  place of the note's `UNPLUG RECEIVER.` headline at scale 2.
- **The chip's label on amber** is `C_PAPER`, the rule `theme.h` gives for
  the fills that are bright in one theme and dark in the other. No chip used
  amber before.

The suite runs the monitor through MENU and the chooser on the UART0 fake,
with core1's job passes every 500 µs: frames at 14 ms and 7 ms, the flags,
failsafe values, a receiver that stops, footers the decoder refuses, a
parity, framing and break error, BACK, BACK TO MONITOR and RESTART DSHOT;
GP1 as SIG with an ESC answering and its end 500 ms on; KISS on GP1; no
role on GP1; a refused clock and RETRY. Not run on hardware: the frames a
receiver sends, the screen's reading of them on the panel and the pass
times are not measured ([bench](#bench)).

33 mutations of `src/sbus_view.cpp` were run against the suite: each link
rule's boundary and order, the count check, the hold's start, stamp, end
and count, the chips' texts and fills, each ink, the bar's clamp and scale,
the empty value and digital text, the rounding and caps of every line, and
a count put in another's place. All 33 fail the suite. 48 mutations of
`src/ui_sbus.cpp`, `src/ui_rx_signals.cpp`, the registry lines and
`uiChip()`: the ESC check dropped, applied to AUX, at 400 ms and before the
Armed and Busy rules; no role on GP1 claiming; the roles fixed; each
message, its colour and RETRY; the job's NO GP1; BACK skipping the leave
page or live on it; RESTART DSHOT's wait; BACK TO MONITOR keeping nothing,
live while releasing, or keeping the hold; each ink colour; the repaint
keys and the body's clear; the chip before the job runs; the role and the
leave page's lines and strip; the claim not asked again; the chooser's
press, colours, `freePins`, strip, BACK and control table; MONITOR without
a screen, the row opening the monitor directly, OUTPUT's `NOT BUILT`; the
chip's label on amber and on every fill. 44 fail the suite, three of them
after tests were added for the Busy refusal before the ESC check, the job's
NO GP1 and the releasing phase. Three passed on code that is redundant and
was removed: the job-error clause of `live()`, since a failed bind leaves
the claim Held; the guard on an empty bar's fill, whose width is 0; and
the chooser's guard on a disabled button's press, which `uiButton()`
ignores. One passes and is equivalent through the screen: the frame-lost
memory kept at a new visit, which comes at least 1000 ms after the last,
RESTART DSHOT's wait, past the 250 ms hold.

## Decisions

Decisions of the owner, 2026-11-14, answering the open questions of
2026-11-13:

1. **Scope.** Build the S.BUS monitor and an S.BUS output, option (c) of
   the first question: inverted 8E2 at 100,000 baud on GP0, UART0's TX. No
   PPM output: slices 1b, 4 and 5 and the PPM sections are not built. The
   questions on the PPM pin, the PPM frame length and the PPM throttle
   ceiling fall away with it. This follows the note's remark that (c) is
   the better output, and goes beyond its recommendation, (b), the monitor
   only.
2. **Receive path.** UART0 on GP1, AUX by default, with the pad's input
   inverted by INOVER, as recommended.
3. **Value scale.** µs by rcbench's `sbus_to_us()`, the bar in raw units,
   as recommended.
4. **MENU placement.** One row, RX SIGNALS, opening a chooser with
   MONITOR and OUTPUT. This is not the note's recommendation of two rows;
   the [MENU](#menu) section describes it as built. Its place in menu
   structure B is [menu-b.md](menu-b.md) decision 32.

Further decisions of the owner, 2026-11-14, on the output:

5. **The encoder comes from rcbench.** The S.BUS encoder is written and
   tested in rcbench first, then copied here unchanged, like the decoder.
6. **The throttle channel obeys MAX THROTTLE** of SETTINGS, the ceiling the
   DShot throttle obeys.

## Open questions

None open. Questions 1 to 4 of 2026-11-13 are answered by decisions 1 to
4; questions 5 to 7 were on the PPM output and fell away with decision 1.
The S.BUS output's open points are design work, listed under
[S.BUS output](#sbus-output).

## To confirm

Not stated by rcbench and not measured by this project:

- the idle level at the pin, low, as derived from the inversion;
- which receivers send footers 0x04, 0x14, 0x24 and 0x34, and whether they
  are S.BUS2 frames;
- which receivers send at 7 ms and which at 14 ms;
- whose convention 172 at 1000 µs and 1811 at 2000 µs is; Betaflight's
  5/8 × raw + 880 µs, not checked against Betaflight's source;
- the output level of any S.BUS receiver;
- that the PL011 checks only the first of two stop bits, in the RP2350
  datasheet;
- core1's pass time with either job;
- for the PPM output, not built by decision 1: whether FCs, dongles and
  trainer ports accept a 23.8 ms PPM frame, 3.3 V marks, this polarity and
  a cut last frame, and what an ESC does with 300 µs PPM marks on its
  signal pin;
- the facts of S.BUS2 slots beyond the 325 µs rcbench names, F.Port, CRSF
  and iBUS, listed as later work.
