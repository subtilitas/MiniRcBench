# S.BUS output

Status: slices 1 and 2 built, host tests only, not tested on hardware
([slice 1 as built](#slice-1-as-built), [slice 2 as built](#slice-2-as-built)):
rcbench's encoder is copied from b88593d3, and the core1 job
`src/sbus_out_job.*` with its pin user `PinUser::SbusOut` sends frames on the
host suite's UART0 fake. Nothing opens the job: by the owner's decision of
2026-11-15 ([decision 1](#decisions)) S.BUS OUTPUT is built only together
with a bootloader probe on GP0, slice 3, and no screen offers START before
it. The probe's prerequisites, slices 1 to 3 of
[bluejay-blheli-s.md](bluejay-blheli-s.md#work-breakdown), are built
([work breakdown](#work-breakdown)). Slices 3 to 5 are not built. The owner
chose an S.BUS output on 2026-11-14 ([rx-signals.md](rx-signals.md#decisions),
decisions 1, 4, 5 and 6): inverted 8E2 at 100,000 baud on GP0, UART0's TX
pin; the encoder from rcbench, copied unchanged; the throttle channel under
MAX THROTTLE of SETTINGS; the entry OUTPUT on the RX SIGNALS chooser, drawn
`NOT BUILT` today. rcbench's encoder, `sbus_encode()` and `sbus_from_us()`,
is on rcbench's `main` since pull request 847 and is host-tested there and
here. Nothing of this design has driven a pin on hardware. The
firmware facts below are read from the sources under [sources](#sources),
with file and line; the ones no readable source settles are marked "not
verified" and listed again under [not verified](#not-verified). No question
is open: the owner's decision is under [decisions](#decisions), and every
other point of the list in [rx-signals.md](rx-signals.md#sbus-output) is
decided here with its reason ([choices](#choices)).

## Terms

S.BUS is Futaba's serial receiver bus: 16 proportional channels of 11 bits
and two digital ones in a 25-byte frame. UART is universal asynchronous
receiver-transmitter; the RP2350's two UARTs are ARM PL011 cells. 8E2 is
8 data bits, even parity and 2 stop bits. FIFO is a first-in, first-out
buffer, TX transmit, RX receive. GPIO is general-purpose input/output, PIO
the RP2350's programmable input/output block, DMA direct memory access.
OUTOVER is the output override field of a GPIO's control register, which
can invert the signal between the peripheral and the pad. ESC is electronic
speed controller, FC flight controller, UI user interface, EEPROM an ESC's
settings store. SIG and AUX are the pin claim's two roles
([pin-claim.md](pin-claim.md#the-two-pins)): the ESC pin, GP0 by default,
and the KISS pin, GP1 by default. Raw is an 11-bit channel value, 0 to
2047. µs is microseconds by rcbench's line, raw 172 at 1000 µs and raw 1811
at 2000 µs ([rx-signals.md](rx-signals.md#value-scale)).

DShot is the digital ESC protocol the bench sends on SIG; bidirectional
DShot inverts the line and the ESC answers each frame with its eRPM
(electrical revolutions per minute). PWM is pulse-width modulation; OneShot
and Multishot are PWM variants with shorter pulses (OneShot125 125 µs to
250 µs, OneShot42 41.7 µs to 83.3 µs, Multishot 5 µs to 25 µs, per the
header of `BLHeli_S.asm` line 35).

Futaba, S.BUS, AM32, Bluejay, BLHeli_S and Betaflight are names of their
owners' products or firmware, used here only to say which devices and which
references a statement is about.

## Sources

Read for this note; line numbers are at the commit named.

| Source | Commit | Files | Licence, access |
|---|---|---|---|
| rcbench (`subtilitas/rcbench`) | `main` at bd0dffe2 (2026-11-15), whose S.BUS files are those of b88593d3 | `protocols/sbus/include/sbus.h`, `protocols/sbus/sbus.c`, `protocols/sbus/README.md`, `test/host/test_sbus.c` (19 cases) | MIT, same holder |
| AM32 (`am32-firmware/AM32`), version 2.22 | 2a695a8 | `Src/signal.c`, `Src/dshot.c`, `Src/main.c`, `Mcu/f051/Src/IO.c`, `Inc/targets.h` | GPL-3.0; read, nothing copied |
| Bluejay (`bird-sanctuary/bluejay`), revision 0.21 | 0368d11 | `src/Bluejay.asm`, `src/Modules/Isrs.asm` | GPL-3.0; read, nothing copied |
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 | a5bcf99 | `BLHeli_S SiLabs/BLHeli_S.asm`, `BLHeli_S SiLabs/A.inc` | GPL-3.0; read, nothing copied |
| Betaflight (`betaflight/betaflight`), `master` | 6241cfb (2026-11-15) | `src/main/rx/sbus.c`, `src/main/rx/sbus_channels.c`, `src/main/rx/rx.c`, `src/main/flight/failsafe.c`, `src/main/pg/rx.c` | GPL-3.0; read as the FC side's reference |
| Pico SDK | 2.1.1 | `hardware_gpio/gpio.c`, `hardware_uart/include/hardware/uart.h`, `rp2350/hardware_regs/include/hardware/regs/io_bank0.h` | BSD-3-Clause |
| This repository | `main` at 25b5a36 | `src/sbus_job.*`, `src/pin_claim.*`, `src/core1_job.*`, `src/servo_job.*`, `src/ui_servo.*`, `src/ui_rx_signals.*`, `src/ui_registry.cpp`, `src/esc_task.cpp`, `src/config.h`, `src/ui_slider.h`, `src/ui_list.h` | |

AM32, Bluejay and BLHeli_S are at the commits
[esc-commands.md](esc-commands.md#sources) read. ArduPilot and INAV, the
other common FC firmwares, are not read for this note.

## What it is for

- An FC's S.BUS receiver input without a receiver or a transmitter: send
  known values on 16 channels and check the FC's channel map, endpoints and
  mode switches in its configurator.
- The FC's reaction to the failsafe bit and the frame-lost bit, each set on
  demand, and to frames that stop.
- Fixed, repeatable values, which a transmitter's sticks do not give.

The output is a throttle to whatever listens: an FC passes the throttle channel
to its motors once it is armed, and an arm switch is one of the channels. That
is why rx-signals.md decision 6 holds the throttle channel under MAX THROTTLE,
and why the rules of [safety](#safety) are the servo tester's with the throttle
channel added.

## The line

What the output sends, from rcbench's encoder (`sbus.h` at b88593d3):

| Item | Value | Source |
|---|---|---|
| Line | UART, 100,000 baud, 8E2, the pad's output inverted: idle low | rx-signals.md decision 1; [rx-signals.md](rx-signals.md#sbus-frame) |
| Frame | 25 bytes: header 0x0F, 22 bytes of 16 channels, flag byte, footer 0x00 | `sbus.h` lines 44 to 48, 163 to 169 |
| Flag byte | bit 0 CH17, bit 1 CH18, bit 2 frame lost, bit 3 failsafe; bits 4 to 7 clear | `sbus.h` lines 74 to 77, 169 |
| Encoder refusal | a channel above 2047 refuses the frame; the output buffer is left unwritten, and the caller sends a stop frame or nothing | `sbus.h` lines 171 to 176; `sbus.c` lines 78 to 105 |
| µs to raw | `sbus_from_us()`, the inverse of `sbus_to_us()`; a ceiling is applied in raw counts | `sbus.h` lines 131 to 160 |
| Byte time | 120 µs: 12 bits of 10 µs | `sbus.h` lines 21 to 23 |
| Frame time | 3,000 µs: 25 bytes sent back to back from the TX FIFO | |
| Period | 14,000 µs, start to start, so the gap is 11,000 µs | this note, [frame period](#frame-period) |

### Frame period

rcbench says frames arrive every 7 ms or 14 ms (`sbus.h` line 21) and
names no receiver; which receiver sends which is not verified. What the
decoders read for this note accept:

| Decoder | Frames on | Period it checks | Source |
|---|---|---|---|
| Betaflight | a gap of more than 600 µs between bytes, then the header 0x0F | none: "Total packet time is not tracked" | `rx/sbus.c` lines 73, 79, 127 to 142 |
| rcbench, and the S.BUS monitor here | a gap of 2000 µs or more | none | `sbus.h` line 71; [rx-signals.md](rx-signals.md#the-job) |

Both accept 7 ms and 14 ms alike. The output sends one frame every
14,000 µs (`SBUS_OUT_PERIOD_US`). Reason: of the two periods rcbench names,
14 ms leaves the longer gap, 11,000 µs against 4,000 µs, so it also meets a
decoder that frames on a longer gap than the two read here; and the frame
rate is not what an FC test varies. 7 ms is one constant away if a device
needs it. Betaflight's 200,000 baud "fast" variant (`rx/sbus.c` line 63)
is not offered.

## Electrical

| Item | Value |
|---|---|
| Pin | GP0, UART0's TX: J5 pin 2 or J4 pin 9. GP0 is SIG in the default wiring and AUX in the swapped one |
| Level | 3.3 V push-pull from IOVDD, the RP2350's I/O supply |
| Idle | low: the UART idles high and OUTOVER inverts it |
| While SAFE and after STOP | low: the UART bound and idle, no frames |
| After the release | low: a plain input with the pad's pull-down |
| FC input | an S.BUS pad, an inverter in front of a UART or a UART that inverts in hardware; Betaflight opens it inverted by default (`rx/sbus.c` lines 205 to 207). Whether a given FC reads a 3.3 V high: not verified |
| Supply | none: the board sends signal and ground only. The FC takes power from its own USB port or its own supply |

The FC goes on GP0 and ground, nothing else:

```
  board GP0  --------------------------------------  FC S.BUS input
  board GND  --------------------------------------  FC ground

  GP0 = J5 pin 2 or J4 pin 9, GND = J5 pin 4 or J4 pin 1 or 5
```

With the FC powered from its battery, its ESCs and motors are live:
propellers off. Power the board before the FC, and the FC off before the
board, as for a receiver ([rx-signals.md](rx-signals.md#electrical)); what
an FC's S.BUS input puts on GP0 while the board is unpowered is not
verified, and GP0 is rated 3.63 V while IOVDD is at 0 V.

## UART0 on GP0

UART0's TX reaches GP0 on function 2, `uart0_tx` (SDK 2.1.1, `io_bank0.h`
line 141), `GPIO_FUNC_UART`. The firmware uses UART0 nowhere else but the
S.BUS monitor's job (`pico_enable_stdio_uart` is 0, `CMakeLists.txt` line
330), and the monitor and the output never hold the claim together. The
PL011 sends from a 32-entry TX FIFO; a frame of 25 bytes fits whole, so
core1 loads a frame in one pass and the UART sends its bytes back to back.
The baud divisor is the monitor's: 100,000 baud exactly from `clk_peri` at
150 MHz ([rx-signals.md](rx-signals.md#receive-path)).

**The bind**, in this order:

1. `gpio_pull_down(0)`. The DShot teardown leaves GP0 an input with the
   pull-up on (`esc_task.cpp` line 363); the pull-down holds the line at
   its idle level from here on.
2. `uart_init(uart0, 100000)`; the bind refuses with `CORE1_JOB_CLOCK` when
   the rate it returns is more than 1 % off, by the monitor's `sbusBaudOk()`
   (`sbus_job.h` lines 180 to 183).
3. `uart_set_format(uart0, 8, 2, UART_PARITY_EVEN)`, then the receiver off:
   RXE cleared in UARTCR. No pad carries UART0's RX during this claim, and
   what an unrouted UART input reads is not verified; with RXE off it is
   moot.
4. GP0 to the UART with its output inverted, in one write of the control
   register: FUNCSEL `GPIO_FUNC_UART` and OUTOVER `GPIO_OVERRIDE_INVERT`
   together. `gpio_set_function()` writes the whole control register and
   zeroes OUTOVER (`gpio.c` lines 38 to 53), so the SDK's two calls,
   function then `gpio_set_outover()`, would let the pad carry the UART's
   idle high between them. The job does the three steps of
   `gpio_set_function()` itself: input enable on and output disable off in
   the pad register, the control register with both fields, then pad
   isolation cleared. The pad never carries the UART's high idle.

GP1 keeps its function: in the default wiring it is AUX and not asked for.

**The release**, once the last frame is out:

1. No new frame is loaded from the pass the stop arrives on.
2. The job returns false (stays Stopping) while UARTFR.BUSY is set: the
   frame in the FIFO completes, at most 3,000 µs after it was loaded. No
   frame is cut short, so a decoder that frames on the header alone never
   reads the tail of one frame as the start of another.
3. `gpio_set_dir(0, GPIO_IN)` and `gpio_put(0, 0)`: SIO's output enable
   for GP0 is off before the pad leaves the UART, so SIO drives nothing
   when it takes the pad. Then `gpio_set_function(0, GPIO_FUNC_SIO)`, which
   zeroes OUTOVER, then the pull-down: the line stays low.
4. `uart_deinit(uart0)`, then the status cleared, as the monitor's release
   does (`sbus_job.cpp` lines 179 to 187).

The pad leaves the UART before the UART stops, so the line never shows the
UART's own level.

## Pin claim

A new pin user and a new job, `PinUser::SbusOut` and `Core1Job::SbusOut`:
the monitor's rows (`pin_claim.h` lines 87 to 96) describe a receiver that
drives the line, this one a board that drives it, and the ESC check
differs.

| PinUser | Core1Job | `unplug` | `escCheck` | `deferJob` | Roles |
|---|---|---|---|---|---|
| `SbusOut` | `SbusOut` | true | true | false | `PIN_SIG` when GP0 is SIG, `PIN_SIG \| PIN_AUX` when GP0 is AUX |

`sbusOutClaimRoles(sig, aux)` gives the roles, as `sbusClaimRoles()` does
for GP1 (`sbus_job.cpp` lines 98 to 102). SETTINGS offers only GP0 and GP1
and swaps SIG and AUX when one is stepped onto the other
([pin-claim.md](pin-claim.md#the-two-pins)), so one role is always GP0.
A new `CORE1_JOB_NO_GP0` is the job's backstop for a wiring set some other
way, with the screen's own lines `S.BUS NEEDS GP0.`
`SET A PIN TO GP0 IN SETTINGS.`, as the monitor has `CORE1_JOB_NO_GP1`.

### The ESC check

The claim refuses while an ESC answered on SIG within `ESC_LINK_STALE_MS`,
500 ms (`pinClaimCheck()`, `pin_claim.h` lines 183 to 196), and the screen
asks again every frame. `escCheck` is true for every wiring:

- **GP0 is SIG**, the default: the ESC's signal wire is where the FC lead
  goes. Frames must not reach an ESC; the check makes the pilot unplug it.
- **GP0 is AUX**: the ESC sits on GP1, and GP0 is the KISS wire, which may
  be the same ESC's telemetry output. The claim asks for AUX, so the KISS
  check refuses while KISS frames arrive within 500 ms; the ESC check asks
  for the ESC on GP1 to come off too, so no ESC stays on the bench while
  S.BUS runs. One rule, the claim's own, with no screen-side copy as the
  monitor needs.

The check sees only an ESC that answers bidirectional DShot. It misses an
ESC that is unpowered, and an ESC that does not answer: BLHeli_S has no
bidirectional DShot, so the check never sees one. Once ESC SETTINGS serves
BLHeli_S ([bluejay-blheli-s.md](bluejay-blheli-s.md)), a BLHeli_S ESC on
GP0 is a usual bench state. The next section says what each firmware does
with S.BUS on its signal pin, why that gap matters for BLHeli_S, and the
[bootloader probe](#the-bootloader-probe) that closes part of it.

### What an ESC does with S.BUS on its signal pin

Nothing here has run on an ESC; it is read from source. The line as an ESC
sees it: high for each start bit and zero bit, low for one bits, stop bits
and between frames. The shortest run is one bit, 10 µs; the longest high
run inside a frame is a 0x00 byte, start, 8 data and parity bits, 100 µs.
A pulse, a high run with the low run after it, lasts at least 20 µs. A
model of the line (frames from rcbench's packing, inverted, at 14 ms) put
the high runs of 2,000 frames with random channels from 172 to 1811 at
53 % 10 µs, 24 % 20 µs, 11 % 30 µs and 12 % 40 µs to 100 µs; the START
frame of [safety](#safety) gives 52 %, 13 %, 10 % and 24 %. The model is not
a measurement.

**AM32 2.22: takes no throttle from it.**

- Detection (`signal.c` lines 201 to 266): a DMA capture of 32 edges at
  about 6 MHz (`IO.c` line 16: `CPU_FREQUENCY_MHZ / 6` as the prescaler),
  then the shortest interval decides. DShot needs it at 8 ticks or fewer,
  about 1.5 µs (lines 203 to 227); S.BUS's shortest is 10 µs. Servo PWM
  needs it above 200 ticks, about 37.5 µs at 48 MHz (line 231); in the
  model every window of 31 intervals holds one of 30 µs or less. So neither
  is detected, `inputSet` stays 0, and arming needs `inputSet`
  (`main.c` lines 1528 to 1533).
- Even taken as servo PWM, a pulse counts only from 800 µs to 2200 µs at
  1 MHz (`signal.c` lines 51, 233); S.BUS high runs end at 100 µs.
- An AM32 that had locked to the bench's DShot before the claim checks
  each 32-edge capture against the DShot frame time it measured, within
  1/16 (`signal.c` lines 171, 172; `dshot.c` lines 74 to 77): 26.7 µs at
  DShot600, while 32 S.BUS edges span at least 310 µs. No frame passes;
  without frames it resets itself, 0.5 s after its last frame when armed
  and 2 s after it when not (`main.c` lines 2137 to 2162), and starts in
  detection again.
- `USE_CRSF_INPUT` builds read a serial input instead (`main.c` lines 243,
  2006); no target in `Inc/targets.h` defines it.

**Bluejay 0.21: takes no throttle from it.** Bluejay reads DShot only. A
frame counts with exactly 16 pulses (`Isrs.asm` line 131) whose frame time,
in Timer2's 0.5 µs counts (`Bluejay.asm` line 41), lies from the threshold
to twice it (`Isrs.asm` lines 114 to 128): 40 µs to 80 µs at DShot300,
20 µs to 40 µs at DShot600 (`Bluejay.asm` lines 566 to 581). 16 S.BUS
pulses take at least 320 µs. After 50 failed frames the throttle is set to
stop (`Isrs.asm` lines 155 to 170), the signal times out and Bluejay
returns to detection, which tests DShot300 and DShot600 and loops
(`Bluejay.asm` lines 485 to 592, 676 to 681). Its bootloader entry needs
the line high for about 150 ms (lines 496 to 506); the inverted line idles
low.

**BLHeli_S 16.7: may take it as Multishot, calibrate, arm and start its
motor.** BLHeli_S detects normal PWM,
OneShot125, OneShot42, DShot150 to 600 and Multishot in that order, 100 ms
each, and takes a mode when fewer than 10 pulses net were out of range
(`BLHeli_S.asm` lines 3651 to 3779; the count rises for each pulse out of
range and falls for each in range, lines 1198 to 1219). Each mode scales
the high time measured on Timer0 at the system clock (`A.inc` lines 143 to
157) and keeps it when it lies from 922 to 2303 in the scaled units, the
"900-2235us" of lines 1171 to 1196. Worked out for the 24 MHz builds
(`MCU_48MHZ` 0), not verified on an ESC:

| Mode | Scaling, lines | High time in range | S.BUS high runs in range, model |
|---|---|---|---|
| Normal PWM | × 43 / 1024, 1124 to 1169 | 915 µs to 2286 µs | none |
| OneShot125 | × 86 / 256, 1106 to 1122 | 114 µs to 286 µs | none |
| OneShot42 | × 258 / 256, 1088 to 1104 | 38 µs to 95 µs | 9 % |
| Multishot | × 2.094 + 768, 1053 to 1086 | 3.1 µs to 30.6 µs | 88 % |

The DShot tests fail as Bluejay's do: they check the same 16 pulses and
frame time (lines 621 to 700). Multishot is the last test, and S.BUS passes
it in the model: most high runs are 10 µs to 30 µs. Builds with
`MCU_48MHZ` 1 detect at 24 MHz (line 3619) and double the count
(`A.inc` lines 158 to 171), so their window is the same; builds with
`MCU_48MHZ` 2 are not worked out.

What follows, by the same reading and a model of the line (the reviewer's
of pull request 82, checked against the lines named), not a measurement:

- **The held reading.** The ESC's reading, `New_Rcp`, changes only on a
  high run inside the window. The flag byte and the footer, both 0x00 with
  the flags clear, are runs of 100 µs, out of the window; the first 50 out
  of range are ignored (lines 1198 to 1212), and the runs in range count
  that down again. So between frames, 11 of every 14 ms, the ESC holds the
  reading of the frame's last run in the window, which byte 22 (channel 16,
  bits 3 to 10) decides, or the flag byte when CH17 is on. In the full
  range of the calibration (minimum 1024 units, lines 1222 to 1230; gain
  125, `find_throttle_gains`) a 10 µs run reads 60 of 255 (24 %), 20 µs
  182 (71 %), and 30 µs clamps at 255; mid-stick is 127.
- **Arming needs a reading of stop** (`program_by_tx_checked`, lines 3905
  to 3915). With the default calibration, minimum throttle 1148 µs (line
  324), a high run reads as stop only below about 8.1 µs, 195 counts at
  24 MHz; S.BUS has none, so with the default calibration the ESC does not
  arm.
- **The calibration comes first and is tried again on every pass.**
  `Initial_Arm` is set at power-up (line 3568) and cleared only once the
  ESC has armed (line 3927). While it is set, every pass through
  `arming_start` (line 3796) checks it (lines 3811 to 3816); with TX
  programming on, the default (line 323), two readings above mid-stick
  100 ms apart start the calibration (lines 3819 to 3842), and ten below
  mid-stick in a row end it, store the new range in the ESC's flash (lines
  3852 to 3897), at least 35 units, 140 µs, wide (`find_throttle_gain`),
  and return to detection (line 3903). A pass with no reading of stop goes
  back to `arming_start` (line 3915).
- **The START frame**, channel 16 at raw 172: the held run is 30 µs, above
  mid-stick. In the model the ESC enters the calibration within about 1 s
  and stays in its low step, writing nothing, for as long as channel 16
  and CH17 stay there.
- **Channel 16 at 904 of its 1640 values from 172 to 1811, raw 992
  (1500 µs) among them, or CH17 on**: the held run is 10 µs, below
  mid-stick. In the model, with each reading at a random phase of the
  frame, the ESC completed the calibration, wrote a range of about 140 µs
  to its flash, armed on the held run as stop and met its motor-start
  condition (`wait_for_power_on`, lines 3930 to 4008: a reading above stop,
  and the pulses still there 100 ms later) in each of 200 runs, after
  about 40 s at the median: 4.5 s to 199 s, 91 s at the 90th percentile,
  with channel 16 at 1500 µs; 4.5 s to 198 s with CH17 on. After four
  stalls it re-detects, arms and starts again (lines
  4574 to 4579). Whether the motor turns on readings above stop for about
  3 ms of every 14 ms the source does not settle.

So the frames must never reach a BLHeli_S ESC, and the claim's ESC check
cannot see one: it never answers DShot. The servo tester does not share
this: its START puts 1500 µs on the pin, which a BLHeli_S reads below
mid-stick and above stop, so it neither calibrates nor arms until the user
drags to the ends; S.BUS reaches a motor start with no input from the
user. The owner's answer is the [bootloader probe](#the-bootloader-probe),
with the warnings of the [screen](#screen) kept for what the probe misses.

### The bootloader probe

By the owner's decision 1 of 2026-11-15, S.BUS OUTPUT is built only
together with this probe. Before the first START of a visit, the connect
cycle of [bluejay-blheli-s.md](bluejay-blheli-s.md#the-connect-cycle)
decision 2 runs on GP0: 1000 ms quiet on the pull-up, then 3 greetings,
about 125 ms. A visit is each bind of the claim: the entry, and BACK TO
OUTPUT on the leave page, which asked for the lead to be unplugged.

- **What it finds.** By that note, a powered Bluejay or BLHeli_S that was
  on DShot drops to its no-signal state when the pump stops and enters its
  EFM8 bootloader on the high line inside the quiet second; a powered
  BLHeli_S still looking for a signal checks for 15 ms of high line at each
  pass through detection. An AM32 enters its bootloader only in the 46 ms
  after each reset: its power-up, or its firmware's own reset 0.5 s or 2 s
  after its last frame (above); an AM32 already running answers DShot,
  which the claim's ESC check sees. A bootloader that answers a greeting
  is an ESC on GP0.
- **START refused.** While an EFM8 or AM32 bootloader answers, START is
  refused and the rows' area asks for the ESC's battery and lead to come
  off ([unavailable pin](#unavailable-pin)). The cycle repeats until one
  cycle passes with no answer, so for an EFM8 START stays refused until the
  ESC is unplugged: an answering bootloader may read S.BUS frames as
  commands (not verified what each would do). Then the UART binds and the
  screen is SAFE. Whether the repeated greetings count toward the AM32
  bootloader's limit of invalid commands is not verified; if they do, an
  AM32 can leave its bootloader for its firmware and a later cycle passes
  with it still on GP0, which is harmless, since AM32 takes no throttle
  from S.BUS (above).
- **An ESC left in its bootloader**, by
  [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions) decision 8 and its
  slice 2: a powered EFM8 that answered waits in its bootloader, and DShot
  must not reach it. So an answer of either kind sets MENU's run cue,
  `POWER-CYCLE THE ESC TO RUN IT` (29 characters, 173 px), as an answer on
  ESC SETTINGS does; and an EFM8 answer in the claim calls
  `pinClaimWantLeavePage()` and gives the leave page decision 8's first
  step ([leave page](#leave-page)), so SIG returns to DShot only after the
  page asks for the ESC's battery to come off.
- **What it misses.** An ESC that is unpowered during the probe, and an
  ESC plugged in after it. The warnings stay on every view before START.
- **Cost.** About 1.1 s per visit before START is offered. For that time an
  FC on GP0 sees the line on the pull-up, high, which an inverted S.BUS
  input reads as a break, and the greetings. By reasoning, not verified,
  it reads no frame from either: each 0x00 of the 19,200 baud greeting
  reads at 100,000 baud as 0xF0 with a parity error. What a given FC does
  with a second of high line is not verified.
- **Where it comes from.** The connect cycle, the greeting and the reading
  of an EFM8 or AM32 answer are built by slices 1 to 3 of
  [bluejay-blheli-s.md](bluejay-blheli-s.md#work-breakdown). The probe is a
  phase of the S.BUS OUTPUT claim before the UART bind; how its job shares
  that line driver is the probe slice's design ([work
  breakdown](#work-breakdown)).

## Safety

The rules are the servo tester's ([servo-tester.md](servo-tester.md),
"Safety model"), with the throttle channel and a frame stop added.

| | S.BUS OUTPUT |
|---|---|
| Before SAFE | the [bootloader probe](#the-bootloader-probe) on each bind of the claim: about 1.1 s; START refused while a bootloader answers |
| Off state | SAFE: GP0 bound to the UART, idle low, no frames |
| Starting | HOLD TO START, `ARM_HOLD_MS` 1000 ms, on `ui_hold`; refused by core1 while the heartbeat has lapsed |
| First frame | the START frame, set by core1 whatever the screen sent: the throttle channel at raw 172, the other three of channels 1 to 4 at raw 992, channels 5 to 16 at raw 172, CH17 and CH18 off, both flags clear |
| Throttle channel | channel 3 by default, Betaflight's default map `AETR1234` (`pg/rx.c` lines 126 to 130); one of 1 to 4, chosen while SAFE |
| Throttle ceiling | raw 172 + 1639 × MAX THROTTLE / 2000, rounded down; raw 499, 1200 µs, at the default 400 of 2000 (20 %) |
| Other channels | raw 172 to 1811, 1000 µs to 2000 µs |
| Stopping | STOP, on press: no frame after the one on the wire, which ends within 3,000 µs |
| UI stalls 250 ms | `core1UiAlive()` false while RUNNING: frames stop, `Tripped` latched, chip `STALLED`, a new HOLD TO START needed |
| Touch lost | the frames stop on the touch-loss rule pull request #49 brings to the tester and the servo tester, once it is merged ([STATUS.md](../../STATUS.md) open item 23); until then the idle stop below is the backstop, since a controller that stops answering reads as no touch |
| Idle | more than `IDLE_DISARM_MS`, 30 s, with no touch while the throttle channel is above raw 172, or any of channels 5 to 16, CH17 and CH18 differs from the START frame: frames stop, as the tester disarms; the note says why |
| BACK | the leave: frames stop, the last one completes, GP0 is let go low ([UART0 on GP0](#uart0-on-gp0)) |

Why each rule:

- **START is core1's frame.** As the servo tester's START puts every pin
  at CENTRE whatever was commanded (`servo_job.cpp` lines 56 to 69), the
  first frame is fixed, so no value set before START reaches the FC.
- **Channels 5 to 16 start at 1000 µs**, not at the centre: they are switch
  channels on most setups, and 1500 µs can lie inside an FC's arm range.
  Raw 172 reads 987.5 µs in Betaflight (5/8 × raw + 880, `sbus_channels.c`
  line 94), at the low end of every switch range.
- **The throttle channel starts at its stop and stops at the ceiling** of
  SETTINGS, the ceiling the DShot throttle obeys (rx-signals.md decision 6). The
  ceiling is in raw counts, as rcbench's `sbus.h` asks (line 149), from the same
  fraction the tester uses: MAX THROTTLE out of 2000 (`config.h` lines 431 to
  437). Betaflight reads raw 499 as 1191.9 µs. The ceiling bounds the throttle
  channel, not an armed FC's motor outputs: the FC adds its idle and its
  stabilisation to them.

  | MAX THROTTLE | Raw ceiling | µs, rcbench | µs, Betaflight |
  |---|---|---|---|
  | 100 (5 %) | 253 | 1049 | 1038.1 |
  | 400 (20 %), default | 499 | 1200 | 1191.9 |
  | 1000 (50 %) | 991 | 1500 | 1499.4 |
  | 2000 (100 %) | 1811 | 2000 | 2011.9 |

- **Channels stay from raw 172 to 1811**, the endpoints a transmitter sends
  ([rx-signals.md](rx-signals.md#value-scale)). Betaflight flags a value
  under `rx_min_usec`, 885 µs by default, on channels 1 to 4 as a lost
  signal (`pg/rx.c` line 96, `common_defaults_post.h` lines 97, 98: 885 µs
  and 2115 µs), and the endpoints keep the
  output inside what a receiver would send. With every value in range,
  `sbus_encode()` never refuses; if it does, core1 sends nothing more,
  goes SAFE and reports the refusal, the second of the two answers
  rcbench's contract allows (`sbus.h` lines 174 to 176).
- **STOP stops the frames**, rather than sending frames with the failsafe
  bit: the servo tester stops its pulses (servo-tester.md, decision 6), and
  an FC without frames goes to its own failsafe, the state a pilot tests
  for. The failsafe bit is its own control on SET.
- **The idle stop** carries the tester's `IDLE_DISARM_MS` over, with its
  bound, more than 30 s (`ui.cpp` line 1405): an FC armed through a switch
  channel spins its motors at idle with the throttle at its stop, and
  faster above it. So the frames stop when the throttle is above its stop
  or a switch channel, 5 to 16, CH17 or CH18, is off its START value. With
  the frame at its START values the frames go on, so the FC's receiver page
  can be watched for as long as needed.

### What the FC does when frames stop

Betaflight, read and not run: a frame with the failsafe bit or the
frame-lost bit counts as not received (`rx.c` line 607, `sbus_channels.c`
lines 76 to 85). With no received frame for 150 ms the receiver is lost
(`rx.c` lines 137, 570, 753). The failsafe's first stage holds for
`failsafe_delay`, 1.5 s by default, and the second runs its procedure,
`DROP` by default, which disarms (`failsafe.c` lines 86 to 95, 110,
229 to 235, 255 to 259, 534, 535); the period runs from the last valid
frame. So STOP, a heartbeat trip, the idle stop, the failsafe bit and a
frame-lost bit held on each lead a Betaflight FC into failsafe within
about 150 ms and to a disarm about 1.5 s after the last valid frame. Other
FC firmwares are not read.

## The job

`src/sbus_out_job.{h,cpp}`, in the pattern of the servo job: the
decisions are pure functions on a plain struct, which the host suite runs
as they ship; the job around them copies the shared blocks, writes the
UART and publishes.

```c
enum class SbusOutCmdKind : uint8_t { Safe, Start, Frame };
struct SbusOutCmd {
    uint16_t seq; SbusOutCmdKind kind;
    uint16_t channel[16];            // raw
    bool ch17, ch18, failsafe, frameLost;
    uint8_t thrCh;                   // 0 to 3
    uint16_t ceilRaw;                // from MAX THROTTLE
};
struct SbusOutStatus {
    uint16_t ackSeq; bool running, tripped, refused;
    uint32_t frames;                 // sent since START
    uint32_t lastFrameMs;
    sbus_frame_t sent;               // the last frame loaded
};
void sbusOutCoreReset(SbusOutCore *c, uint16_t ackSeq, bool tripped);
bool sbusOutCoreTake(SbusOutCore *c, const SbusOutCmd *cmd,
                     uint32_t nowUs, bool uiAlive);
bool sbusOutCoreStep(SbusOutCore *c, uint32_t nowUs, bool uiAlive,
                     uint8_t out[SBUS_FRAME_BYTES]);   // true: load out
uint16_t sbusOutCeilRaw(uint16_t maxThrottle);
uint8_t  sbusOutClaimRoles(uint8_t sigGpio, uint8_t auxGpio);
```

- **Take.** A command whose seq was taken changes nothing. `Safe` ends the
  frames. `Start` is refused while the heartbeat has lapsed; otherwise it
  sets the START frame, clears the trip and makes the next step send. A
  `Frame` while RUNNING replaces the channels and flags; core1 clamps every
  channel to raw 172 to 1811 and the throttle channel to `ceilRaw`, behind
  the screen's own clamp. A `Frame` while SAFE is taken and moves nothing.
- **Step.** A lapsed heartbeat while RUNNING ends the frames and latches
  the trip. While RUNNING a frame is due at the deadline: the first on the
  pass that took `Start`, each next one 14,000 µs after the one before. A
  pass more than one period late sends at once and sets the next deadline
  from that pass, so a stall never makes a burst. A due frame is encoded
  with `sbus_encode()` and returned for loading; the job loads it only when
  UARTFR.TXFE says the TX FIFO is empty, which it is 11,000 µs after the
  last one, and otherwise tries again on the next pass.
- **Ceiling.** `sbusOutCeilRaw()` is 172 + 1639 × MAX THROTTLE / 2000,
  rounded down, from `settings()->maxThrottle`. SETTINGS opens only while
  the claim is Free (`pin_claim.cpp` lines 109, 110), so it holds for the
  visit.

**Timing.** With the claim held, core1 runs `escTaskPoll()`, which returns
at once, and this job; no flash write parks it, since settings saves are
refused during a claim. A frame starts on the first pass at or after its
deadline, so each frame start is late by at most one pass, and the period
holds on average because the deadlines advance by 14,000 µs and not from
the pass. Loading writes 25 words to UARTDR. Inside a frame the bytes are
the PL011's, back to back, well inside Betaflight's 600 µs between bytes.
The pass time with this job is not measured. A `Frame` command reaches
core1 at most once per UI frame, 40 Hz; a frame carries the channels core1
held when it was encoded.

**Published status**, read by core0 once per UI frame: the frame last
loaded, so the screen shows what the line carries; RUNNING, the trip, an
encoder refusal, the frame count and `millis()` of the last frame. The
release clears it.

## Screen

S.BUS OUTPUT, `src/ui_sbus_out.{h,cpp}`, opened from OUTPUT on RX SIGNALS.
What each line says comes from a pure formatter, `src/sbus_out_view.*`, as
the monitor's from `src/sbus_view.*`. Header `S.BUS OUTPUT` at scale 2,
12 characters, 142 px, x 8 to 149, 20 px clear of BACK at x 170. Other
text is scale 1: a string of n characters is (6n − 1) px wide, (12n − 2) px
at scale 2. The panel is 240 × 320 px, portrait.

The controls follow the servo tester's: a value is dragged, never jumped;
START is a hold; STOP acts on press; a second view, SET, holds the settings
that are not dragged, with its editor bar.

### Main view

Eight channels a page, two pages. Each row is a slider of `ui_slider`
(`src/ui_slider.h`) with delta drag and tap-to-set off, so a press commands
nothing and no tap jumps a channel: the servo tester's SPEED control, the
rule its header states for a control that commands something.

```
y   0 +------------------------------------------+
      | S.BUS OUTPUT                     [ BACK ]|  header 36 px
   40 | SIG GP0  100 KBAUD  8E2           (SAFE) |  strip 40..55
   62 | GP0 GOES TO A FLIGHT CONTROLLER.         |  note 1, 62..68
   70 | NEVER TO AN ESC. PROPS OFF.              |  note 2, 70..76
   80 | CH1     [==========O==========]     1500 |  8 rows, 22 px, 25 px pitch
  105 | CH2     [==========O==========]     1500 |
  130 | CH3 THR [O===!--------------- ]     1000 |  ! = ceiling at 1200 US
  155 | CH4     [==========O==========]     1500 |
  180 | CH5     [O====================]     1000 |
  205 | CH6     [O====================]     1000 |
  230 | CH7     [O====================]     1000 |
  255 | CH8     [O====================]     1000 |  last row 255..276
  281 | ====                                     |  hold bar 281..283
  285 | [ HOLD TO START ] [CH 9-16] [  SET  ]    |  285..316
  320 +------------------------------------------+
```

| Element | Rectangle (x, y, w, h) | Content |
|---|---|---|
| Strip caption | 8, 44, 137, 7 | `SIG GP0  100 KBAUD  8E2`, 23 characters, 137 px, `C_DIM`; `AUX GP0  100 KBAUD  8E2` when GP0 is AUX. With `  14 MS` it would be 30 characters and end at x 186, past the chip, so by menu-b.md decision 14 the period goes, the item at the right; it is fixed, and the README states it |
| Strip chip | 179, 42, 53, 12 at its widest | `uiChip()`: `SAFE` on `UI_COL_SAFE`, `RUNNING` on `UI_COL_ON`, `STALLED` on `UI_COL_ALERT`; at most 7 characters, 41 px, plus 6 px each side, right edge x 231. The caption ends at x 144, 34 px clear. Drawn only while the job runs |
| Note 1 | 8, 62, at most 221, 7 | SAFE: `GP0 GOES TO A FLIGHT CONTROLLER.`, 32 characters, 191 px, `UI_COL_WARN`. RUNNING: `THROTTLE CH3: 1000 TO 1200 US (20%).`, at most 37 characters, 221 px at 100 %, `C_TEXT`. After a stop: the reason, `UI_COL_ALERT` for the heartbeat, `UI_COL_WARN` for the idle rule and a refused frame |
| Note 2 | 8, 70, at most 215, 7 | SAFE: `NEVER TO AN ESC. PROPS OFF.`, 27 characters, 161 px, `UI_COL_WARN`. RUNNING: `CH17 OFF  CH18 OFF  FS OFF  LOST OFF`, at most 36 characters, 215 px, `C_TEXT`, `UI_COL_WARN` while a flag is on. After a stop: `HOLD TO START AGAIN.`, 20 characters, 119 px, `C_DIM` |
| Row i, i = 0 to 7 | 8, 80 + 25i, 224, 22 | label, slider, value; 22 px is `UI_TAP_MIN` |
| Label | 8, y + 8, at most 41, 7 | `CH1` to `CH16`; `CH3 THR` on the throttle channel, 7 characters, 41 px, `UI_COL_WARN` |
| Track | 54, y + 6, 146, 10 | 1000 µs at x 54, 1500 µs at x 126, 2000 µs at x 199: 6.9 µs a pixel |
| Knob | 14 × 16 px (`ui_slider.cpp` `THUMB_W`, `UI_SLIDER_OVERHANG`) | on the command; 3 px above and below the track, y + 3 to y + 18, inside the row |
| Ceiling mark | x 54 + 145 × (ceiling − 1000) / 1000, y, 1, 22 | throttle row only, `UI_COL_WARN`: x 83 at 1200 µs; the track past it `C_DIM`, and the knob stops there |
| Value | right-aligned to x 231, y + 8 | `sbus_to_us()` of the raw value in the frame last sent, 4 characters, 23 px, from x 209; `UI_COL_LIVE` while RUNNING, a live reading, `C_DIM` otherwise |
| Hold bar | 6, 281, 110, 3 | HOLD TO START's progress, `UI_HOLD_BAR_H` |
| HOLD TO START | 6, 285, 110, 32 | 13 characters, 77 px; `STOP` at scale 2, 46 px, on `UI_COL_ON` while RUNNING, acting on press |
| Page | 120, 285, 55, 32 | `CH 9-16`, 7 characters, 41 px; `CH 1-8` on the second page, 6 characters, 35 px; acts in every state |
| SET | 179, 285, 55, 32 | `SET`, 3 characters, 17 px; opens SET |

Note 2 ends at y 76, 3 px above row 0. The last row ends at y 276, 4 px
above the hold bar; the bottom row ends at y 316, inside the panel. The
layout constants (`SBOUT_UI_*`) get `static_assert`s for every gap, as
the other screens have.

A slider moves in 5 µs steps; a drag of one pixel is 6.9 µs. Core0 turns
the step into raw with `sbus_from_us()`, clamps it to raw 172 to 1811 and
the throttle channel to the ceiling, and sends a `Frame` command when a
value changed. The sliders are drawn disabled while SAFE and show the START
frame's values: START sets the first frame, not the sliders, and resets
them to it. After STOP they keep their last values, disabled, until the
next START.

| State | Chip | Notes | Rows |
|---|---|---|---|
| Claim not held | none | the claim's message ([unavailable pin](#unavailable-pin)) in place of notes and rows | hidden |
| SAFE | `SAFE` | the two warnings | disabled, START values |
| Probe | none | `CHECKING GP0 FOR AN ESC.` ([unavailable pin](#unavailable-pin)) in place of notes and rows | hidden |
| RUNNING | `RUNNING` | the ceiling and the flags | live, values `UI_COL_LIVE` |
| Stopped by the heartbeat | `STALLED` | `STOPPED: SCREEN STALLED` (23 characters, 137 px, as SERVO TESTER's) and `HOLD TO START AGAIN.` | disabled, the last values |
| Stopped by the idle rule | `SAFE` | `STOPPED: NO TOUCH FOR 30 S.` (27 characters, 161 px) and `HOLD TO START AGAIN.` | disabled, the last values |
| Encoder refused | `SAFE` | `STOPPED: FRAME REFUSED.` (23 characters, 137 px) and `HOLD TO START AGAIN.` | disabled |

### SET

SERVO TESTER's SET pattern: field rows of `ui_list` (`uiListDrawField()`,
40 px rows at a 42 px pitch, the group at y + 11, the label at y + 21, the
value at scale 2 right-aligned to x 231), the list from y 62 to 215,
scrolling, the editor bar at y 218, the footer at y 268. A row is chosen
with a tap and changed with the editor's `-` and `+`. A change applies at
once: there is no HOLD TO APPLY, since every row is either locked while the
frames run or meant to change while they run.

| Group | Label | Values | Editable |
|---|---|---|---|
| `THROTTLE` | `CHANNEL` | `CH1` to `CH4`, 3 characters at scale 2, 34 px | while SAFE; ReadOnly while RUNNING, with `STOP TO CHANGE THE CHANNEL.` (27 characters, 161 px) |
| `DIGITAL` | `CH17` | `OFF`, `ON` | while RUNNING; ReadOnly while SAFE, with `START TO SET THE FLAGS.` (23 characters, 137 px) |
| `DIGITAL` | `CH18` | `OFF`, `ON` | as CH17 |
| `FLAGS` | `FAILSAFE BIT` | `OFF`, `ON` | as CH17 |
| `FLAGS` | `FRAME LOST BIT` | `OFF`, `ON` | as CH17 |

The labels are at most 14 characters, 83 px; the values at most 34 px at
scale 2, under `UI_FIELD_VALUE_W2`. CH17, CH18 and the two flags are part
of the frame, so START sets them OFF with the rest and they change only
while frames run: the value on SET is always what the line carries. They
are not on the main view: two taps, row then `+`, are harder to make by
accident than one, and CH17 or CH18 can be an FC's arm switch. The footer
holds `STOP` on press while RUNNING, in SET's right button, as SERVO
TESTER's SET does. While SAFE the footer carries the second warning,
`NEVER TO AN ESC. PROPS OFF.` (27 characters, 161 px, `UI_COL_WARN`), at
x 8, y 287, ending at x 168, so SET too warns before START; the right
button, from x 162, is not drawn then. Header BACK returns to the main
view (menu-b.md, decision 5).

### Unavailable pin

The rows' area shows the claim's message from x 8 at y 80, 94 and 108, as
the monitor's ([rx-signals.md](rx-signals.md#screen)), and no chip:

| Cause | Lines | Colour | Width |
|---|---|---|---|
| An ESC answers on SIG | `AN ESC ANSWERS ON SIG GP0.` `UNPLUG IT: NO ESC STAYS ON` `THE BENCH WHILE S.BUS RUNS.` | `UI_COL_WARN` | 155, 155 and 161 px |
| KISS frames on AUX, GP0 is AUX | `KISS WIRE ON AUX GP0 IS LIVE.` `UNPLUG IT. PLUG THE FLIGHT` `CONTROLLER IN ONCE SAFE SHOWS.` | `UI_COL_WARN` | 173, 155 and 179 px |
| The claim drains DShot | `RELEASING SIG GP0` | `C_DIM` | 101 px |
| The bootloader probe runs | `CHECKING GP0 FOR AN ESC.` | `C_DIM` | 143 px |
| A bootloader answered the probe | `AN ESC BOOTLOADER ANSWERS ON GP0.` `UNPLUG THE ESC BATTERY AND LEAD.` `START WAITS UNTIL NONE ANSWERS.` | `UI_COL_WARN` | 197, 191 and 185 px |
| Held, the job not bound yet | `TAKING SIG GP0` | `C_DIM` | 83 px |
| The bind refused the clock | `SIG GP0 COULD NOT BE TAKEN (CLOCK)`, and RETRY, the wide button at 6, 268, 228 × 46 | `UI_COL_WARN` | 203 px |
| No role on GP0 | `S.BUS NEEDS GP0.` `SET A PIN TO GP0 IN SETTINGS.` | `UI_COL_WARN` | 95 and 173 px |
| Armed, or another claim holds | `DISARM FIRST`, `WAIT: DSHOT RESTARTING` | `UI_COL_WARN` | backstops, unreachable through the UI |

`SIG GP0` reads `AUX GP0` when GP0 is AUX; the ESC line names the GPIO of
SIG, `GP1` in that wiring. In the default wiring the FC lead goes where the
ESC was, so `UNPLUG IT` comes first and the ESC line names no FC.

### Leave page

BACK on the main view is the leave, as on SERVO TESTER and S.BUS MONITOR:
`ui_leave` opens releasing (menu-b.md, owner decision 3). The job lets GP0
go within 3,000 µs, so the page is released within a UI frame or two.

```
y   0 | S.BUS OUTPUT                    [  BACK  ]|  BACK drawn disabled
   40 | SIG GP0  AUX GP1                          |  strip, C_DIM
   66 |#1 UNPLUG THE FLIGHT CONTROLLER.           |  step 1, current
   78 |#  DSHOT RESTARTS ON SIG GP0.              |
   92 | 2 RESTART DSHOT.                          |  step 2, C_DIM
  214 | [BACK TO OUTPUT]                          |  6, 214, 110 x 46
  268 | [          RESTART DSHOT           ]      |  live 1000 ms after the release
```

| Text | Characters | Width at scale 1 | Room |
|---|---|---|---|
| `1 UNPLUG THE FLIGHT CONTROLLER.` | 31 | 185 px | 37 characters |
| `  DSHOT RESTARTS ON SIG GP0.` | 28 | 167 px | 37 |
| `2 RESTART DSHOT.` | 16 | 95 px | 37 |
| `BACK TO OUTPUT` | 14 | 83 px | 110 px button |
| `RELEASING THE PIN`, until released | 17 | 101 px | 37 |

With GP0 as AUX the strip reads `SIG GP1  AUX GP0` and the detail line
`  DSHOT RESTARTS ON SIG GP1.`, the ESC's pin, the same width. BACK TO
OUTPUT calls `pinClaimKeep()`: the claim returns to Held, the job binds
again, SAFE. RESTART DSHOT calls `pinClaimRestart()` and returns to MENU.
BACK before a claim held, or while it is Stopping, returns to MENU with
DShot resumed at once and no page. DShot on an FC's S.BUS input is a
signal into an input; the FC reads no frame from it, so unplugging is for
the ESC that goes back on SIG, not for the FC.

**After an EFM8 answered the probe** in the claim, the page is decision 8
of [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions), as ESC SETTINGS
shows it after an EFM8 answer (its slice 2): BACK opens it before LEAVE,
with STAY (23 px in the 110 px button at 6, 214), which returns to the
screen and the probe, and LEAVE (scale 2, 58 px, in the wide button at 6,
268), which lets GP0 go; the steps:

| Text | Characters | Width at scale 1 |
|---|---|---|
| `1 UNPLUG THE ESC BATTERY, THEN LEAVE.` | 37 | 221 px, to x 228 |
| `  ITS BOOTLOADER WAITS FOR COMMANDS` | 35 | 209 px |
| `  AND MAY READ DSHOT AS ONE.` | 28 | 167 px |
| `2 UNPLUG THE FLIGHT CONTROLLER.` | 31 | 185 px |
| `  DSHOT RESTARTS ON SIG GP0.` | 28 | 167 px |
| `3 RESTART DSHOT.` | 16 | 95 px |
| `  THEN PLUG THE ESC BACK IN.` | 28 | 167 px |

Step 1's lines are ESC SETTINGS' own texts. The steps run from y 66 at a
12 px pitch, 14 px between steps, and end at y 148, above the notes at
y 158. After LEAVE the page is the one above, BACK TO OUTPUT and RESTART
DSHOT; BACK TO OUTPUT runs the probe again. The page holds in either
wiring: with GP0 as AUX DShot restarts on GP1, but the ESC still waits in
its bootloader until its supply goes off.

### The RX SIGNALS entry

OUTPUT gets its screen, `&UI_SBUS_OUT`, in `UI_RX_ENTRIES`
(`ui_registry.cpp` lines 121 to 124), and its second line replaces
`NOT BUILT`:

| Text | Characters | Width, scale |
|---|---|---|
| `OUTPUT` | 6 | 70 px, 2 |
| `S.BUS TO A FLIGHT CONTROLLER.` | 29 | 173 px, 1 |
| `ON GP0, NEVER TO AN ESC.` | 24 | 143 px, 1 |

Both lines are within `UI_PROG_LINE_CHARS`, 37, and end at y 210, the
second at y 204 as `NOT BUILT` does today, so `rxEntriesFit()` holds. A
tap opens S.BUS OUTPUT through `uiChooseTool()`, whose entry point asks for
the claim on that frame, as MONITOR's does; its origin is MENU.

### Warnings

The probe misses an ESC that is unpowered during it or plugged in after
it, so the warnings stay wherever START can be reached:

| Where | Text |
|---|---|
| Main view, SAFE | `GP0 GOES TO A FLIGHT CONTROLLER.` `NEVER TO AN ESC. PROPS OFF.` |
| SET, SAFE | `NEVER TO AN ESC. PROPS OFF.` in the footer |
| RX SIGNALS, under OUTPUT | `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.` |

README's section and the wiki pages RX-Signals and RX-Signale carry the
same warning in prose, written by the screen slice: GP0 goes to a flight
controller only, never to an ESC; propellers off; a BLHeli_S ESC is found
only while it is powered and in its bootloader at the probe, and by its
source it can calibrate, arm and start its motor on S.BUS.

## Resources

| Block or channel | ESC running | S.BUS OUTPUT claim |
|---|---|---|
| pio0 | backlight | backlight |
| pio1 | SDIO | SDIO |
| pio2 | DShot and KISS | empty |
| UART0 | unused | TX on GP0, RX off |
| DMA | 3 of 16 | 3 of 16 |

The claim exists for the pin, not for resources.

## Porting

| rcbench file, at b88593d3 | Here | How |
|---|---|---|
| `protocols/sbus/sbus.c`, `include/sbus.h` | `lib/rcbench/protocols/sbus/` | replace the copy of 5ef42efd, unchanged |
| `test/host/test_sbus.c` | `test/rcbench/test_sbus.c` | replace, unchanged: 19 cases, 12 of them the decoder's |

Between 5ef42efd and b88593d3 the decoder's code is the same; the changes
are the encoder, `sbus_from_us()`, `SBUS_FOOTER` and comments. The monitor's
suite runs unchanged on the new copy. `lib/rcbench/README.md` gets the new
commit for the S.BUS files.

New code in `src/`:

| File | What |
|---|---|
| `sbus_out_job.h`, `.cpp` | `SbusOutCore`, the ceiling, the roles, the UART bind and release, the frame schedule, the status |
| `sbus_out_view.h`, `.cpp` | the notes, chip, row values and colours, pure |
| `ui_sbus_out.h`, `.cpp` | S.BUS OUTPUT, its SET view and its leave page |

Shared files each slice edits by a line or a few: `core1_job.h`
(`Core1Job::SbusOut`, `CORE1_JOB_NO_GP0`), `core1_job.cpp` (the job table),
`pin_claim.h` (the user and its row), `ui_registry.cpp` (OUTPUT's screen
and line), `ui_rx_signals.h` (its header comment), `src/rx.cmake`,
`test/rx.mk`, `test/test_main.cpp`, `test/fakes.*`, `test/stubs/hardware/`.
The `-I` lists of the Config permutations job in `ci.yml` already hold the
S.BUS include directory.

## Tests

### Host

New fakes: the UART0 fake gains the TX side, each UARTDR write recorded
with its time, the 32-entry FIFO draining at 120 µs a byte, TXFE and BUSY
from it; the GPIO fake records every write of GP0's control register, so a
test sees FUNCSEL and OUTOVER as one value, and UARTCR's RXE.

Checked:

- the copied encoder: rcbench's 19 cases, from `make run`;
- `sbusOutCeilRaw()` at 100, 400, 1000 and 2000: 253, 499, 991 and 1811;
- the core: SAFE sends nothing; START's first frame byte for byte, whatever
  the last `Frame` said; every channel clamped to raw 172 to 1811 and the
  throttle channel to the ceiling, for each of channels 1 to 4; frames at
  14,000 µs with passes every 500 µs, each start late by less than one
  pass; a pass 30 ms late sends one frame, not two; a FIFO not yet empty
  delays the load; the trip at 250 ms and not at 249 ms, latched, cleared
  only by START, START refused while lapsed; STOP on the next pass; a frame
  the encoder refuses stops the frames and sets `refused`;
- the line: the bytes the TX fake sent, fed with their times to rcbench's
  decoder, give back the frames the core built, at 14 ms, with no gap
  inside a frame and none under 2000 µs between frames;
- the job through the claim: the pull-down first, the clock refusal and RETRY,
  RXE off, GP0's control register never on the UART function without OUTOVER
  inverted, GP1 untouched; the release only after BUSY clears, SIO's output
  enable for GP0 off before FUNCSEL leaves the UART, GP0 on SIO before
  `uart_deinit()`, OUTOVER normal, the pull-down on, the status cleared; the
  probe before the first bind of each visit, START refused while a model EFM8 or
  AM32 bootloader answers and offered after one cycle with no answer, no probe
  between STOP and START; an answer of
  either kind setting MENU's run cue; an EFM8 answer calling
  `pinClaimWantLeavePage()`, BACK then opening decision 8's page before
  LEAVE, STAY returning to the probe, and DShot back on SIG only after
  LEAVE and RESTART DSHOT; each wiring, GP0 as SIG and as AUX, and the NO GP0
  backstop; the ESC check in both wirings and its end 500 ms after the last eRPM
  frame; KISS on AUX GP0; a new visit starting SAFE with no trip;
- the screen through MENU and RX SIGNALS by control id: OUTPUT live and
  opening the screen; HOLD TO START at 1000 ms and not at 999 ms; STOP on
  press, on the main view and on SET; the sliders disabled while SAFE, a
  press moving nothing, a drag moving its channel and the throttle row
  stopping at the ceiling mark; the page button; SET's rows locked as the
  table says; the idle stop at 30,001 ms with the throttle above raw 172,
  not at 30,000 ms, the same with channel 9 or CH17 off its START value,
  and never with the START frame; every note in each state; SET's footer
  warning while SAFE;
  every message of the unavailable pin; the leave page, BACK TO OUTPUT and
  RESTART DSHOT; every string within its rectangle; the control tables;
  the preview `docs/sbus-out-preview.png`.

Every behaviour change gets a mutation check, as the monitor's slices did
([rx-signals.md](rx-signals.md#slice-3-as-built)).

### Bench

Only hardware settles these:

- On a scope at GP0: the idle level low while SAFE, after STOP and after
  the release; no high edge at the bind; 100,000 baud, 8E2, the inversion;
  25 bytes in 3,000 µs and a 14,000 µs period; the last frame complete at
  STOP and at a heartbeat trip (core0 halted in a debugger).
- With the S.BUS monitor of a second board: the channels and flags read
  back.
- A Betaflight FC, propellers off: its receiver page reads the 16 channels
  and CH17 and CH18 as sent; throttle at the ceiling; the failsafe bit, the
  frame-lost bit, STOP and the idle stop each bring `RXLOSS` and then the
  failsafe's disarm; frames after START clear it.
- An AM32 and a Bluejay ESC on GP0, motor disconnected, powered up after
  START, so the claim's check never saw them: neither arms nor beeps a
  detection, as read from source.
- The probe: a Bluejay and a BLHeli_S ESC powered on GP0 answer it and
  START stays refused until they are unplugged; an AM32 powered up during
  the quiet second answers too.
- BLHeli_S is not tested with a motor on it. With an expendable ESC, no
  motor, powered up after the probe, channel 16 at 1500 µs and
  CH17 on: whether it detects Multishot (its arm-sequence beeps), and its
  stored minimum and maximum throttle read with ESC SETTINGS before and
  after. The test may rewrite the ESC's settings.

## Work breakdown

Each slice builds, passes the host suite and merges alone.

| Slice | Content | Files | Lines, rough |
|---|---|---|---|
| 0. Prerequisites, met | slices 1 to 3 of [bluejay-blheli-s.md](bluejay-blheli-s.md#work-breakdown), built: the escwire copies (slice 1), the detection of an EFM8 answer with the run cue and the per-claim leave page (slice 2), the connect cycle and its job (slice 3) | there | there |
| 1. Encoder copy, built | `sbus.c`, `sbus.h`, `test_sbus.c` from rcbench b88593d3; `lib/rcbench/README.md`, STATUS, CHANGELOG | 3 replaced, 3 edited | 300 changed in copies, 30 of documentation |
| 2. Job, built | `sbus_out_job.*`, the user and job rows, `CORE1_JOB_NO_GP0`, the TX side of the UART0 fake, the control-register record, `test/test_sbus_out_job.cpp`, STATUS, CHANGELOG | 2 new in `src/`, 1 new in `test/`, 8 edited | 350 in `src/`, 700 in `test/` |
| 3. Probe | the connect cycle on GP0 as a phase of the `SbusOut` claim before the UART bind, the refusal while a bootloader answers, the repeat until a cycle with no answer, MENU's run cue at an answer, `pinClaimWantLeavePage()` and decision 8's leave page after an EFM8 answer, its host tests on the bootloader models of slice 0 | 3 edited in `src/`, 1 new in `test/` | 200 in `src/`, 350 in `test/` |
| 4. Screen | `sbus_out_view.*`, `ui_sbus_out.*`, OUTPUT's entry, `test/test_sbus_out_ui.cpp`, `test_ids.cpp`, the preview, `tools/make_previews.py`, `tools/check_wiki.py`, README and wiki pages RX-Signals and RX-Signale with the [warnings](#warnings), STATUS, CHANGELOG, this note's status | 4 new in `src/`, 1 new in `test/`, 10 edited | 750 in `src/`, 700 in `test/`, 200 of documentation |
| 5. Bring-up | the bench tests above | none | none |

By decision 1, slices 1 to 5 need slice 0, the probe's prerequisites,
which are built.
Slice 2 depends on slice 1, slice 3 on slices 0 and 2, slice 4 on slice 3,
slice 5 on all of them. No build offers START without the probe: slice 2
has no screen, and slice 4 comes after slice 3.

### Slice 1 as built

`lib/rcbench/protocols/sbus/sbus.c`, `include/sbus.h` and
`test/rcbench/test_sbus.c` are rcbench's at b88593d3, unchanged: each
compares equal with `git show b88593d3:<path>` in rcbench. `test_sbus.c`
runs 19 cases, 12 of them the decoder's. The S.BUS monitor's suites,
`runSbusJobTests` and `runSbusUiTests`, pass unchanged on the new copy, as
the note expected: the decoder's code is the same. `lib/rcbench/README.md`
names b88593d3 for the S.BUS files and keeps 5ef42efd for the sensor bus.

The copy adds no code of this repository, so its mutation check is
rcbench's own test: 10 mutations of `sbus_encode()`, `pack()` and
`sbus_from_us()` in a scratch copy of `sbus.c` (the shift of a channel's
window, its third byte, the refusal at 2047, the buffer clear, the header,
CH17, the failsafe bit, the footer, the rounding and the clamp at 2047)
each fail `test_sbus.c`. None survives.

### Slice 2 as built

`src/sbus_out_job.h` and `.cpp`; the `SbusOut` rows of `PinUser` and
`Core1Job` and the job table, `CORE1_JOB_NO_GP0`, each on lines of their
own; `src/rx.cmake` and `test/rx.mk`. In the host suite: the transmitter
and UARTCR of the UART0 fake, the record of every control-register write
and the pad register in the GPIO fake (`fakes.*`, the stubs
`hardware/structs/io_bank0.h` and `pads_bank0.h`), and
`test/test_sbus_out_job.cpp`, `runSbusOutJobTests`. CI's firmware job
measures the job's deepest stack chain, 224 bytes against 1,792. Choices
the note left open, or made more exact:

- **Frames never closer than 7 ms.** The note re-anchors the schedule after
  a pass more than one period late. A pass late by less than a period
  keeps the 14,000 µs grid, which brings the next frame closer: at 13 ms
  late the next is due 1 ms later, while the frame is still on the wire,
  and would follow it with no gap, which a decoder that frames on a
  2000 µs gap reads as one frame. `SBUS_OUT_MIN_US`,
  7,000 µs, the other period rcbench names, keeps every gap at 4,000 µs or
  more, and also spaces the first frame of a START soon after a STOP. A
  pass a period or more late, not only more than one, re-anchors, so
  exactly one period late does not send two frames in a row.
- **The FIFO wait is in the core.** `sbusOutCoreStep()` takes UARTFR.TXFE
  as `txEmpty`, so a due frame that waits for the FIFO is a host case of
  the pure core.
- **The throttle channel and the ceiling are Start's.** A `Frame` carries
  them as the note's struct has it, and the core ignores them: the channel
  the FC throttles on is the one START fixed. A Start with an index above 3
  is refused; a ceiling outside raw 172 to 1811 is clamped into it.
- **A backstop for the release.** The release lets GP0 go
  `SBUS_OUT_RELEASE_MAX_MS`, 10 ms, after its first pass even while BUSY
  stays set, so a UART that never finishes cannot hold the claim in
  Leaving. The last frame needs at most 3,000 µs.
- **A clock refusal never routes the UART to the pad.** The bind has set
  only the pull-down by then; it turns UART0 off and leaves GP0 as the
  release does, a low SIO input with the pull-down.
- **Each bind starts SAFE with no trip**, the note's "visit", and takes no
  command sent before it: the bind takes the command block's seq as
  acknowledged.
- **RXE is cleared by a read-modify-write of UARTCR** after
  `uart_set_format()`, which on the chip restores UARTCR after its wait.
- **The status is published on every pass**, as the servo job's is.
- **Nothing reaches it.** No source in `src/` but the job, the job table and
  the claim's row names `PinUser::SbusOut`, `Core1Job::SbusOut`,
  `sbusOutJobCommand()`, `sbusOutClaimRoles()` or `SBUS_OUT_JOB_OPS`; a
  host test reads the sources and fails otherwise, so no screen can claim
  or command the output before slice 3's probe.

The TX fake sends each UARTDR write from a 32-entry FIFO at the format and
rate set, back to back, 12 bits of 10 µs at 100,000 baud 8E2; TXFE, TXFF
and BUSY follow it, and a character still on its way is cut when the UART
stops or GP0 leaves the UART function with OUTOVER inverted. It does not
model the bits' levels: a character counts as on the pad while GP0's
control register held FUNCSEL UART and OUTOVER inverted from its start bit
to its last stop bit. The pad register holds IE, OD and ISO; the pulls are
kept apart, as before. What the PL011 drives while disabled and the level
on the pin are not modelled.

Mutation check: 92 mutations of `src/sbus_out_job.cpp`, each built into the
host suite and run. 90 fail it: the clamps, the START frame and its
channels, the seq rule, each kind's rule in Take, the trip, the 7,000 µs
bound and the grid, the TXFE wait, the refusal, the ceiling and the roles,
the NO GP0 refusal, the pull-down and its order, the baud check and the
clock refusal's release of GP0, 8E2 and RXE, OUTOVER and the single
control-register write, IE, OD and ISO and their order, the stale-command
rule at the bind, the status fields, BUSY and the backstop, each step of
the release and its order, the critical section's initialisation, and the
kept STOP. Six of them failed only after the tests were made stricter:
a `Frame` while SAFE writing the held frame, the bind's own NO GP0 check
called with SIG GP1 and AUX GP1, GP0 left an SIO output high before a
refused bind and before a release, the pad's OD set before the bind, and
a transmitter that never sends, which holds the next frame on TXFE. Two
survive:

- `SbusOutStatus::refused` published as false. No command reaches an
  encoder refusal through the job, since the core clamps every channel to
  11 bits; the core's own flag is tested with a channel set past the
  clamp.
- An extra clear of the pad's isolation before the control-register write,
  the later one kept. GP0 is then an SIO input with the pull-down, which
  nothing drives; judged equivalent, not verified on the chip. Moving the
  clear before the write, with none after, fails the suite.

## Choices

The points [rx-signals.md](rx-signals.md#sbus-output) listed, each decided
here with its reason above:

1. **Frame period 14 ms**, fixed: both decoders read accept 7 ms and 14 ms;
   14 ms leaves the longer gap ([frame period](#frame-period)).
2. **UART0 TX on GP0, OUTOVER inverted in the same register write as the
   function**, receiver off, release after BUSY clears, pad before UART
   ([UART0 on GP0](#uart0-on-gp0)).
3. **A new pin user, `SbusOut`, with the claim's ESC check in every
   wiring** ([pin claim](#pin-claim)).
4. **16 sliders on two pages, delta drag, tap-to-set off; throttle channel
   1 to 4, default 3; ceiling in raw counts from MAX THROTTLE; CH17, CH18
   and both flags on SET, changed only while frames run** ([screen](#screen)).
5. **START frame set by core1**: throttle at raw 172, the other sticks at
   992, channels 5 to 16 at 172 ([safety](#safety)).
6. **Frames stop** on STOP, BACK, a heartbeat lapse, the idle rule and,
   once merged, touch loss; no failsafe frames on stop ([safety](#safety)).
7. **Channels from raw 172 to 1811** only.
8. **The leave page** after every visit, `1 UNPLUG THE FLIGHT CONTROLLER.`
9. **The probe on every bind of the claim**: the entry and BACK TO OUTPUT;
   none between STOP and START ([bootloader probe](#the-bootloader-probe)).
10. **The idle rule also covers the switch channels**: a frame off its
    START values on channels 5 to 16, CH17 or CH18 stops after 30 s without
    touch, since an armed FC idles its motors ([safety](#safety)).
11. **The warnings on every view before START**, SET's footer included
    ([warnings](#warnings)).

## Decisions

Decision of the owner, 2026-11-15, answering this note's question of the
same day:

1. **A BLHeli_S ESC on GP0.** S.BUS OUTPUT is built only together with a
   bootloader probe: before the first START of a visit, the connect cycle
   of [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions) decision 2,
   1000 ms quiet on the pull-up, then 3 greetings, about 125 ms, runs on
   GP0; START is refused while an EFM8 or AM32 bootloader answers, and
   stays refused until the ESC is unplugged, since an answering bootloader
   may read S.BUS frames as commands. The probe misses an unpowered ESC and
   one plugged in after the probe; the warnings stay. The question had offered
   the warnings alone, recommended, or a confirmation tap per visit; the
   decision is neither and puts the slices behind the probe and its
   prerequisites ([the bootloader probe](#the-bootloader-probe), [work
   breakdown](#work-breakdown)).

## Open questions

None open.

## Not verified

- Which receivers send at 7 ms and which at 14 ms; which decoders other
  than Betaflight's and rcbench's accept 14 ms.
- That an FC's S.BUS input reads a 3.3 V high from GP0, and what it puts
  on GP0 while the board is unpowered.
- What an unrouted UART0 RX input reads on the RP2350.
- Core1's pass time with this job.
- Everything said of AM32, Bluejay and BLHeli_S above: read from source,
  not run on an ESC; the BLHeli_S figures for 24 MHz builds and
  `MCU_48MHZ` 1 builds, not for `MCU_48MHZ` 2.
- The BLHeli_S calibration path's timing, from a model of the line, not
  measured; whether its motor turns on 3 ms of power in 14 ms.
- What an answering bootloader does with S.BUS frames; what an FC does
  with the probe's second of high line and with the greetings.
- Whether repeated greetings count toward the AM32 bootloader's limit of
  invalid commands.
- Betaflight's behaviour, read and not run; ArduPilot and INAV not read.
- Pull request #49's touch-loss rule, not merged.
