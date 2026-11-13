# Receiver signals: S.BUS monitor and PPM output

Status: slice 1a built, the rest planned. Slice 1a copied rcbench's S.BUS
decoder and its test into `lib/rcbench/` and `test/rcbench/`, run in the
host suite only ([work breakdown](#work-breakdown)). The design: two tools
on the pin claim of [pin-claim.md](pin-claim.md), RX MONITOR, which reads a
receiver's S.BUS output on GP1, and PPM OUTPUT, which sends eight channels
of PPM on GP0. The protocol cores are rcbench's, host-tested there and
nowhere measured on a pin. The PPM output's use is thin
([what PPM output is for](#what-ppm-output-is-for)); whether to build it is
the owner's first question ([open questions](#open-questions)). The rows'
place on MENU is the owner's to confirm.

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
void sbusCoreReset(SbusCore *c);
void sbusCoreFeed(SbusCore *c, uint16_t dr, uint32_t nowUs);  // one UARTDR word
void sbusCoreStep(SbusCore *c, uint32_t nowUs);               // once per pass
```

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
once, and this job; no flash write parks it (settings saves are refused
during a claim). A pass is therefore far shorter than the 2000 µs threshold,
so bytes of one frame never look 2000 µs apart, and the 4 ms gap between
frames is never missed. The binding limit on a late pass is 2000 µs: a
pass that late in the middle of a frame stamps the bytes after it 2000 µs
or more after those before it, and the gap rule splits the frame. The FIFO's
32 bytes, 3,840 µs of line, would allow more before an overrun. The pass
time with this job is not
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

All counts start at 0 at each bind, so each visit to the screen starts
clean. Frame rate is 1,000,000 / `intervalUs`: 71.4 Hz at 14 ms, 142.9 Hz at
7 ms. The interval carries the drain jitter of one pass.

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
same thing whichever convention reads it.

### Screen

Header `S.BUS MONITOR` at scale 2, 154 px, x 8 to 161, clear of BACK at
x 170. All other text is scale 1: 5 × 7 px glyphs, 6 px a character, a
string of n characters (6n − 1) px wide.

```
y   0 +------------------------------------------+
      | S.BUS MONITOR                    [ BACK ]|  header 36 px, BACK 170..233 x 2..33
   40 | GP1 100 KBAUD 8E2 INV        (NO SIGNAL) |  strip 40..55
   62 |  1 [=======|=========:========|    ] 1500 |  16 rows, 12 px pitch
   74 |  2 [=======|======:   :        |    ] 1320 |
      |  .                                       |
  242 | 16 [===|          :           |     ]  988 |  last row 242..252
  258 | CH17 OFF  CH18 OFF       14.0 MS  71.4 HZ |
  272 | FRAMES 123456                LOST 12  FS 3|
  286 | PAR 0 FRM 0 BRK 0 OVR 0                   |
  300 | RESYNC 2 FOOTER 0                         |
  320 +------------------------------------------+
```

| Element | Rectangle (x, y, w, h) | Content |
|---|---|---|
| Strip caption | 8, 44, 125, 7 | `GP1 100 KBAUD 8E2 INV`, 21 characters, 125 px |
| Strip chip | 161, 40, 71, 16 | `uiChip()`: the text's width plus 6 px each side, right edge x 231. Widest `FRAME LOST`, 10 characters, 59 px. The caption ends at x 132, 28 px clear |
| Channel row i, i = 0 to 15 | 8, 62 + 12i, 224, 11 | number, bar, value |
| Number | right-aligned to x 18, y + 2 | `1` to `16`, 2 characters, 11 px |
| Bar outline | 24, y + 1, 176, 9 | 1 px frame in `C_DIM` |
| Bar fill | 25, y + 2, raw × 174 / 2047, 7 | `C_INK`; `C_RED` while `FAILSAFE`; `C_DIM` while `NO SIGNAL` |
| Bar ticks | x 39, 109, 178; y + 2, 1, 7 | raw 172, 992 and 1811: 1000, 1500 and 2000 µs |
| Value | right-aligned to x 231, y + 2 | µs, 3 or 4 characters, at most 23 px; `----` before the first frame |
| Line 1 | 8, 258, 107, 7 and right-aligned to x 231 | `CH17 OFF  CH18 OFF`, 18 characters; `14.0 MS  71.4 HZ`, 16 characters, 95 px, from x 137 |
| Line 2 | 8, 272 and right-aligned to x 231 | `FRAMES 123456`, at most 13 characters; `LOST 9999  FS 9999`, at most 18 characters, 107 px |
| Line 3 | 8, 286, at most 209, 7 | `PAR 9999 FRM 9999 BRK 9999 OVR 9999`, at most 35 characters |
| Line 4 | 8, 300, at most 137, 7 | `RESYNC 9999 FOOTER 9999`, at most 23 characters |

Counts above 9999 show 9999, and `FRAMES` above 999999 shows 999999, so no
line passes its rectangle. The bars span the whole 11-bit field, so a
receiver that sends past 172 or 1811 shows it. Layout constants get
`static_assert`s for every gap, as the other screens have.

- **Before the first frame**: empty bars, `----` values, chip `NO SIGNAL`,
  and `-- MS  -- HZ`.
- **`NO SIGNAL` after frames**: the last frame's bars and values in `C_DIM`,
  so the last state before the loss stays readable.
- **`FAILSAFE`**: bars and values in `C_RED`. They are the receiver's
  failsafe values, which is what the user came to check.
- **`NO FRAMES`**: bars as before; line 3's counts say why.

The screen has no control but BACK. Each region repaints only when its
values change; at 16 bars that is at most 16 rectangles of 176 × 9 px a
frame.

**When the pin is unavailable**, as in
[pin-claim.md](pin-claim.md#when-the-pin-is-unavailable): `KISS WIRE ON GP1
IS LIVE.` `UNPLUG IT: THE RECEIVER GOES THERE.`; `S.BUS NEEDS GP1.` `SET A
PIN TO GP1 IN SETTINGS.`; `AN ESC ANSWERS ON GP1.` `UNPLUG IT: THE RECEIVER
GOES THERE.` when GP1 is SIG ([interlocks](#interlocks)); `GP1 COULD NOT BE
TAKEN (CLOCK)` with RETRY.

**Unplug page.** BACK shows `UNPLUG RECEIVER.` at scale 2, as the other
unplug pages draw their title: 16 characters, 190 px, inside the 224 px
between the margins. Then, at scale 1, `DSHOT RESTARTS ON
GP0.` in the default wiring, or `DSHOT RESTARTS ON GP1, WHERE THE` `RECEIVER
DRIVES THE LINE.` when GP1 is SIG, and RESTART DSHOT
([interlocks](#interlocks)).

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

## PPM output

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
([open questions](#open-questions), question 1).

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

| Block or channel | ESC running | RX MONITOR claim | PPM OUTPUT claim |
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

Today MENU has seven rows and scrolls 36 px. The two tools sit after the
servo tools, SERVO TESTER, KST SERVO and HITEC SERVO, which take the signal
pins as they do, and before SETTINGS:

| | Rows | Content | Scroll range |
|---|---|---|---|
| Today | 7 | BEEP THE ESC, AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO, SETTINGS, SD LOG | 0 to 36 px |
| Today, RX MONITOR only | 8 | ... HITEC SERVO, RX MONITOR, SETTINGS, SD LOG | 0 to 78 px |
| Today, both tools | 9 | ... HITEC SERVO, RX MONITOR, PPM OUTPUT, SETTINGS, SD LOG | 0 to 120 px |

The scroll range is rows × 42 px − 258 px, by the rules of
[menu.md](menu.md#menu). MENU structure B (`docs/design/menu-b.md`, not
built) places the rows; question 4 holds until then. The placement is the
owner's to confirm.

| Row | Label | Fit | Status |
|---|---|---|---|
| RX MONITOR | 10 characters at scale 2: 118 px, x 16 to 133 | 35 px clear of `PINS BUSY`, which starts at x 169; the check asks for 8 px | `GP1`; `PINS BUSY` |
| PPM OUTPUT | 10 characters: 118 px | as above | `GP0`; `PINS BUSY` |

`S.BUS MONITOR` would be 154 px, x 16 to 169, and run onto the first pixel
of `PINS BUSY`, so the registry's fit check refuses it. `RX MONITOR` also
names the screen that the later protocols join. The screen's own title is
`S.BUS MONITOR`.

## Interlocks

Each tool is one `PinUser`, one `Core1Job` and one row in `PIN_USERS`:

| PinUser | Core1Job | `unplug` | `escCheck` | Roles |
|---|---|---|---|---|
| `Sbus` | `Sbus` | true | false, and the screen's own check when GP1 is SIG (below) | `PIN_SIG \| PIN_AUX` when GP1 is AUX, `PIN_SIG` when GP1 is SIG |
| `Ppm` | `Ppm` | true | true | `PIN_SIG` |

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
  GP1.` `UNPLUG IT: THE RECEIVER GOES THERE.` (22 and 35 characters, 131 px
  and 209 px). `PinUserInfo.escCheck` is one flag per user, so the screen
  makes the check itself before each `pinClaimBegin()`, with
  `escFieldFresh()` on the snapshot's `lastRpmMs`, the rule
  `pinClaimCheck()` uses, and asks again every frame; the claim's table
  stays as it is. Like the claim's check, it misses an ESC that does not
  answer in bidirectional DShot.
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
  against each other once a millisecond. The unplug page covers both
  wirings with one rule; the page's text names the case.
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

`lib/rcbench/README.md` gains the rows and the commit. Copied files keep
their text and their SPDX line.

New code in `src/`:

| File | What |
|---|---|
| `sbus_job.h`, `.cpp` | `SbusCore`, the UART bind and release, the drain, the status |
| `sbus_view.h`, `.cpp` | the link state and the text and colour of each line, pure |
| `ui_sbus.h`, `.cpp` | the S.BUS MONITOR screen and its unplug page |
| `ppm_job.h`, `.cpp` | `PpmCore`, the bind, step and release on `out_ppm.h` |
| `ui_ppm.h`, `.cpp` | the PPM OUTPUT screen and its unplug page |
| `rx.cmake`, `ppm.cmake` | the features' firmware sources |

Shared files each slice edits by a line or a few: `core1_job.h` (two jobs,
`CORE1_JOB_NO_GP1`), `core1_job.cpp` (the job table), `pin_claim.h` (two
users and rows), `ui_registry.cpp` (one line each), `CMakeLists.txt`,
`test/Makefile`, `test/test_main.cpp`, `test/fakes.*`,
`test/stubs/hardware/` and `.github/workflows/ci.yml`. The Config
permutations job of `ci.yml` compiles `src/*.cpp` with two hand-kept `-I`
lists, which need `lib/rcbench/protocols/sbus/include`, and
`lib/rcbench/protocols/ppm/include` and `lib/rcbench/protocols/ppm/rp2350`,
once a file in `src/` includes their headers, as Hitec step 1 added its own
(commit 424014a). The screen slices also edit `tools/check_wiki.py`, for the
pair of new wiki pages, and `tools/make_previews.py`, for the new preview,
as Hitec step 5 did (commit 5e1666d).

## Testing

### Host

`test/rcbench/test_sbus.c` and `test_ppm.c` run as C11 programs from
`make run`, as the other rcbench tests do.

New fakes and models:

- **A microsecond clock.** `test/fakes.cpp` counts milliseconds today, and
  `time_us_32()` is `g_ms × 1000`. The fake gains a microsecond counter
  under `millis()`, with `fakeAdvanceUs()`, so bytes 120 µs apart can be
  stamped.
- **A UART0 register fake.** The stub of `hardware/uart.h` today is never
  readable. It gains a receive queue of `UARTDR` words, each due at a time,
  the RXFE flag, a 32-entry FIFO with overrun, and the baud computation of
  SDK 2.1.1 from a settable `clk_peri`; `gpio_set_inover()` is recorded per
  pin.
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
- the PPM core: SAFE writes nothing; START's first frame (the throttle
  channel and channels 5 to 8 at 1000 µs, the rest at 1500 µs); the clamp
  to 1000 µs to 2000 µs and to the ceiling; the trip at 250 ms and not at
  249 ms, latched, cleared only by START; STOP on the next pass; a refused
  write; the bind refusal; the release and the pull-down;
- the screens through MENU by control id: both rows open their screens and
  BACK passes the unplug page; `PINS BUSY`; the unavailable texts;
  HOLD TO START at 1000 ms and not at 999 ms; STOP on press; sliders and
  THR CH disabled while SAFE and RUNNING respectively; the throttle slider
  at its ceiling; every string within its rectangle; MENU's scroll range
  with the new row count; previews `docs/sbus-preview.png` and
  `docs/ppm-preview.png`.

`make syntax` typechecks `sbus_job.cpp`'s device part, as it does the other
C++ device sources. It typechecks no rcbench driver today: `out_ppm.c` is
built by the firmware only, as the KST and Hitec line drivers are, and
checking it on the host would need a C rule and stubs of its own,
`ppm.pio.h` among them. The coverage table of STATUS.md is
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
- **PPM.** On a scope at GP0: the 300 µs mark, each channel within 1 µs,
  the 23,809 µs frame, the line low while SAFE and after STOP; STOP and a
  heartbeat trip (core0 halted in a debugger) with the cut last frame; an
  FC's receiver page reading the eight channels, its reaction to the
  23.8 ms frame, to the cut frame and to the loss of frames; a dongle or
  trainer port if the owner has one; the line after the release and
  through the unplug page.

## Work breakdown

Each slice builds, passes the host suite and merges alone. Slice 2 depends
on slice 1a and slice 4 on slice 1b, and neither on the other; their shared
files differ by one line each. Slice 1a is built.

| Slice | Content | Files | Lines, rough |
|---|---|---|---|
| 1a. S.BUS copies, built | `sbus.c`, `sbus.h`, `test_sbus.c`; `test/rx.mk`; the `test/Makefile` lists, `lib/rcbench/README.md`, README, STATUS, CHANGELOG | 3 copied, 1 new, 5 edited | 695 copied; 80 new or changed |
| 1b. PPM copies, waiting on question 1 | `ppm.c`, `ppm.h`, `test_ppm.c`; the `outputs.h` stub, `test/ppm.mk`; the `test/Makefile` lists, `lib/rcbench/README.md`, README, STATUS, CHANGELOG | 3 copied, 2 new, 5 edited | 441 copied; 80 new or changed |
| 2. S.BUS job | `sbus_job.*`, `rx.cmake`, `CORE1_JOB_NO_GP1`, the job and user rows, the microsecond clock, the UART fake, `sbus_line.h`, the job's tests, the `-I` lists of `ci.yml` | 4 new in `src/`, about 13 edited, 3 new in `test/` | 350 in `src/`; 700 in `test/` |
| 3. S.BUS screen | `sbus_view.*`, `ui_sbus.*`, the registry line, the screen and MENU tests, `test_ids.cpp`, the preview, `tools/check_wiki.py`, `tools/make_previews.py`, README section, wiki pages in English and German, STATUS, CHANGELOG, this note's status | 4 new in `src/`, 2 new and 10 edited elsewhere | 650 in `src/`; 600 in `test/`; 250 of documentation |
| 4. PPM job | the `out_ppm` driver copies, `ppm_job.*`, `ppm.cmake`, the job and user rows, the `out_ppm.h` fake, `pio_ppm_emu.py`, the job's tests, the `-I` lists of `ci.yml` | 3 copied, 3 new in `src/`, 3 new in `test/`, 6 edited | 490 copied; 300 in `src/`; 600 in `test/` |
| 5. PPM screen | `ui_ppm.*`, the registry line, the screen tests, the preview, `tools/check_wiki.py`, `tools/make_previews.py`, README, wiki pages in English and German, STATUS, CHANGELOG | 2 new in `src/`, 2 new and 10 edited elsewhere | 600 in `src/`; 500 in `test/`; 200 of documentation |
| 6. Bring-up | the bench tests above, each tool on its own | none | none |

Slices 1b, 4 and 5 wait for the owner's answer to question 1.

## Open questions

1. **Build PPM output at all.** (a) Build slices 4 and 5 as designed.
   (b) Build the S.BUS monitor only. (c) Build an S.BUS output instead:
   UART0's TX is GP0, so inverted 8E2 at 100,000 baud on GP0 is OUTOVER and
   the PL011, no PIO and no DMA, and it feeds the serial input most current
   FCs take. rcbench has no S.BUS encoder, so (c) is new code, best written
   upstream first. Recommendation: (b), and (a) only for a PPM device the
   owner has in hand; (c) is the better output if one is wanted.
2. **S.BUS receive path.** (a) UART0 on GP1 with INOVER, as recommended:
   no PIO, hardware error flags, GP1 only. (b) A new PIO program on AUX,
   any pin, about 12 words, with a PIO model to write. Recommendation: (a);
   the one wiring it cannot serve, both roles on GP0, says so on the screen.
3. **S.BUS value scale.** (a) µs by rcbench's `sbus_to_us()`. (b) Raw 0 to
   2047. (c) Percent, −100 % at 172 and +100 % at 1811. Recommendation: (a),
   with the bar in raw units; (b) as a later toggle if a user compares with
   an FC that reads raw.
4. **MENU placement.** (a) Two rows after the servo tools, before SD LOG,
   as drawn. (b) One row, `RX AND PPM`, opening a list of the two. (c) After
   SD LOG, before SETTINGS. Recommendation: (a); with (b) of question 1 it
   is one row, and today's MENU has eight rows and scrolls 78 px. The place
   after the planned restructure is settled in its own note.
5. **PPM pin.** (a) SIG, GP0, under the claim's ESC check. (b) AUX, GP1,
   where no ESC signal goes, but where KISS lives and the ESC check does not
   look. Recommendation: (a).
6. **PPM frame length.** (a) 42 Hz, 23.8 ms, rcbench's driver unchanged.
   (b) 22.5 ms by a changed copy of `out_ppm.c`, which then differs from
   rcbench. (c) A bind by frame length in rcbench first, then a copy.
   Recommendation: (a), and (c) if the bench finds a device that refuses
   23.8 ms.
7. **PPM throttle ceiling.** (a) The throttle channel obeys MAX THROTTLE of
   SETTINGS. (b) Full travel, the throttle channel only starting low.
   Recommendation: (a): one throttle rule on the device.

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
- whether FCs, dongles and trainer ports accept a 23.8 ms PPM frame,
  3.3 V marks, this polarity and a cut last frame;
- what an ESC does with 300 µs PPM marks on its signal pin;
- the facts of S.BUS2 slots beyond the 325 µs rcbench names, F.Port, CRSF
  and iBUS, listed as later work.
