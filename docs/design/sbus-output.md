# S.BUS output

Status: slices 1 to 4 built, host tests only, not tested on hardware
([slice 1 as built](#slice-1-as-built), [slice 2 as built](#slice-2-as-built),
[slice 3 as built](#slice-3-as-built), [slice 4 as built](#slice-4-as-built)):
rcbench's encoder is copied from b88593d3; the core1 job
`src/sbus_out_job.*` with its pin user `PinUser::SbusOut` probes GP0 for an
ESC's bootloader at every bind and, once a probe cycle passes with no
answer, sends frames on the host suite's UART0 fake; and S.BUS OUTPUT,
`src/ui_sbus_out.*` with its formatter `src/sbus_out_view.*`, opens from
OUTPUT on RX SIGNALS and offers START only while the probe's gate,
`sbusOutProbePassed()`, holds. By the owner's decision of 2026-11-15
([decision 1](#decisions)) S.BUS OUTPUT is built only together with the
bootloader probe. The probe's prerequisites, slices 1 to 3 of
[bluejay-blheli-s.md](bluejay-blheli-s.md#work-breakdown), are built
([work breakdown](#work-breakdown)). Slice 5, the bring-up, is not done. The
owner chose an S.BUS output on 2026-11-14
([rx-signals.md](rx-signals.md#decisions), decisions 1, 4, 5 and 6):
inverted 8E2 at 100,000 baud on GP0, UART0's TX pin; the encoder from
rcbench, copied unchanged; the throttle channel under MAX THROTTLE of
SETTINGS; the entry OUTPUT on the RX SIGNALS chooser. rcbench's encoder,
`sbus_encode()` and `sbus_from_us()`, is on rcbench's `main` since pull
request 847 and is host-tested there and here. Nothing of this design has
driven a pin on hardware. The firmware facts below are read from the sources
under [sources](#sources), with file and line; the ones no readable source
settles are marked "not verified" and listed again under
[not verified](#not-verified). No question is open: the owner's decision is
under [decisions](#decisions), and every other point of the list in
[rx-signals.md](rx-signals.md#sbus-output) is decided here with its reason
([choices](#choices)).

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
| During the probe | high: the pad's pull-up, with ESC SETTINGS' greetings in bursts ([the bootloader probe](#the-bootloader-probe)) |
| After the release | low: a plain input with the pad's pull-down; high on the pull-up after a release during the probe or a refused bind ([slice 3 as built](#slice-3-as-built)) |
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
S.BUS monitor's job (`pico_enable_stdio_uart` is 0 in `CMakeLists.txt`),
and the monitor and the output never hold the claim together. The
PL011 sends from a 32-entry TX FIFO; a frame of 25 bytes fits whole, so
core1 loads a frame in one pass and the UART sends its bytes back to back.
The baud divisor is the monitor's: 100,000 baud exactly from `clk_peri` at
150 MHz ([rx-signals.md](rx-signals.md#receive-path)).

**The bind**, in this order:

1. `gpio_set_dir(0, GPIO_IN)` and `gpio_put(0, 0)`, so SIO drives nothing
   whatever an earlier owner left. As SIG, the pump's teardown leaves GP0
   an SIO input with the pull-up on (`escTaskPoll()`'s teardown in
   `esc_task.cpp`); as AUX with KISS on, the KISS receiver's end leaves it
   on the NULL function with the pulls off (`pioUartRxEnd()` in
   `pio_uart_rx.cpp`).
2. `uart_init(uart0, 100000)`; the bind refuses with `CORE1_JOB_CLOCK` when
   the rate it returns is more than 1 % off, by the monitor's `sbusBaudOk()`
   (`sbus_job.h`). A refused bind turns UART0 off again and puts the
   pull-up on GP0: as SIG the level the line already rests at, the pump's
   teardown having left the pull-up; as AUX with KISS on, where GP0 had its
   pulls off, the pull-up is new and holds the line high.
3. `uart_set_format(uart0, 8, 2, UART_PARITY_EVEN)`, then the receiver off:
   RXE cleared in UARTCR. No pad carries UART0's RX during this claim, and
   what an unrouted UART input reads is not verified; with RXE off it is
   moot. No pad carries its TX either until step 5.
4. The [bootloader probe](#the-bootloader-probe) on the escwire line, GP0
   on the pull-up, until a probe cycle passes with no answer
   ([slice 3 as built](#slice-3-as-built)). The line's close leaves GP0 an
   SIO input, low, on the pull-up; then `gpio_pull_down(0)`, which holds
   the line at its idle level from here on.
5. GP0 to the UART with its output inverted, in one write of the control
   register: FUNCSEL `GPIO_FUNC_UART` and OUTOVER `GPIO_OVERRIDE_INVERT`
   together. `gpio_set_function()` writes the whole control register and
   zeroes OUTOVER (`gpio.c` lines 38 to 53), so the SDK's two calls,
   function then `gpio_set_outover()`, would let the pad carry the UART's
   idle high between them. The job does the three steps of
   `gpio_set_function()` itself: input enable on and output disable off in
   the pad register, the control register with both fields, then pad
   isolation cleared. The pad never carries the UART's high idle.

GP1 keeps its function: in the default wiring it is AUX and not asked for.

**The release** during the probe closes the escwire line, which cuts a
greeting on the wire, leaves GP0 an SIO input on the pull-up, and then
turns UART0 off: the pad never was the UART's. **The release** after step
5, once the last frame is out:

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
   does (`sbusJobRelease()` in `sbus_job.cpp`).

The pad leaves the UART before the UART stops, so the line never shows the
UART's own level.

## Pin claim

A new pin user and a new job, `PinUser::SbusOut` and `Core1Job::SbusOut`:
the monitor's rows (`PinUser::Sbus` in `pin_claim.h`) describe a receiver that
drives the line, this one a board that drives it, and the ESC check
differs.

| PinUser | Core1Job | `unplug` | `escCheck` | `deferJob` | Roles |
|---|---|---|---|---|---|
| `SbusOut` | `SbusOut` | true | true | false | `PIN_SIG` when GP0 is SIG, `PIN_SIG \| PIN_AUX` when GP0 is AUX |

`sbusOutClaimRoles(sig, aux)` gives the roles, as `sbusClaimRoles()` does
for GP1 (`sbus_job.cpp`). SETTINGS offers only GP0 and GP1
and swaps SIG and AUX when one is stepped onto the other
([pin-claim.md](pin-claim.md#the-two-pins)), so one role is always GP0.
A new `CORE1_JOB_NO_GP0` is the job's backstop for a wiring set some other
way, with the screen's own lines `S.BUS NEEDS GP0.`
`SET A PIN TO GP0 IN SETTINGS.`, as the monitor has `CORE1_JOB_NO_GP1`.

### The ESC check

The claim refuses while an ESC answered on SIG within `ESC_LINK_STALE_MS`,
500 ms (`pinClaimCheck()` in `pin_claim.h`), and the screen
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
  phase of the S.BUS OUTPUT claim before the UART bind: the job's first
  phase, on ESC SETTINGS' escwire line driver
  ([slice 3 as built](#slice-3-as-built)).

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
| BACK | the leave: frames stop, the last one completes, GP0 is let go low ([UART0 on GP0](#uart0-on-gp0)); during the probe, GP0 is let go high on the pull-up |

Why each rule:

- **START is core1's frame.** As the servo tester's START puts every pin
  at CENTRE whatever was commanded (`servoCoreTake()` in `servo_job.cpp`), the
  first frame is fixed, so no value set before START reaches the FC.
- **Channels 5 to 16 start at 1000 µs**, not at the centre: they are switch
  channels on most setups, and 1500 µs can lie inside an FC's arm range.
  Raw 172 reads 987.5 µs in Betaflight (5/8 × raw + 880, `sbus_channels.c`
  line 94), at the low end of every switch range.
- **The throttle channel starts at its stop and stops at the ceiling** of
  SETTINGS, the ceiling the DShot throttle obeys (rx-signals.md decision 6). The
  ceiling is in raw counts, as rcbench's `sbus.h` asks (line 149), from the same
  fraction the tester uses: MAX THROTTLE out of 2000 (`MAX_THROTTLE_CEILING`
  in `config.h`). Betaflight reads raw 499 as 1191.9 µs. The ceiling bounds
  the throttle channel, not an armed FC's motor outputs: the FC adds its idle
  and its stabilisation to them.

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
  bound, more than 30 s (the idle auto-disarm in `ui.cpp`): an FC armed
  through a switch channel spins its motors at idle with the throttle at its stop, and
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
    uint8_t thrCh;                   // 0 to 3, taken by Start
    uint16_t ceilRaw;                // from MAX THROTTLE, taken by Start
};
struct SbusOutStatus {
    uint16_t ackSeq; bool running, tripped, refused;
    uint32_t frames;                 // sent since START
    uint32_t lastFrameMs;
    sbus_frame_t sent;               // the last frame loaded
};
void sbusOutCoreReset(SbusOutCore *c, uint16_t ackSeq);
void sbusOutStartFrame(sbus_frame_t *f, uint8_t thrCh);
bool sbusOutCoreTake(SbusOutCore *c, const SbusOutCmd *cmd,
                     uint32_t nowUs, bool uiAlive, uint16_t ceilMax);
bool sbusOutCoreStep(SbusOutCore *c, uint32_t nowUs, bool uiAlive,
                     bool txEmpty, uint8_t *out);      // true: load out
uint16_t sbusOutCeilRaw(uint16_t maxThrottle);
uint8_t  sbusOutClaimRoles(uint8_t sigGpio, uint8_t auxGpio);
```

- **Take.** A command whose seq was taken changes nothing. `Safe` ends the
  frames. `Start` is refused while the heartbeat has lapsed and for a
  throttle index above 3; otherwise it takes the throttle channel and the
  ceiling, sets the START frame, clears the trip, the refusal and the frame
  count, and makes the next step send. The ceiling is the command's, clamped
  to raw 172 to `ceilMax`, the job's `sbusOutCeilRaw()` of
  `settings()->maxThrottle`, so a wrong ceiling from the screen never passes
  MAX THROTTLE's. A `Frame` while RUNNING replaces the channels and flags;
  core1 clamps every channel to raw 172 to 1811 and the throttle channel to
  the ceiling Start took, behind the screen's own clamp. A `Frame`'s
  throttle channel and ceiling are ignored. A `Frame` while SAFE is taken
  and moves nothing.
- **Step.** A lapsed heartbeat while RUNNING ends the frames and latches
  the trip. While RUNNING a frame is due at the deadline: the first on the
  pass that took `Start`, each next one 14,000 µs after the one before, and
  none less than 7,000 µs after the last one loaded (`SBUS_OUT_MIN_US`). A
  pass a period or more late sends at once and sets the next deadline a
  period from that pass, so a stall never makes a burst. A due frame waits
  while `txEmpty`, UARTFR.TXFE, says the TX FIFO holds a character, which
  it does not 11,000 µs after the last frame; then it is encoded with
  `sbus_encode()` and returned for loading.
- **Ceiling.** `sbusOutCeilRaw()` is 172 + 1639 × MAX THROTTLE / 2000,
  rounded down, from `settings()->maxThrottle`, which core1 reads. MENU
  draws SETTINGS disabled while the claim is not Free (`ui_menu.cpp`,
  `claimBusy()`), so it holds for the visit.

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
release clears it. The probe adds its phase, whether a bootloader answered
in this bind, and counts of the answers that no release clears
([slice 3 as built](#slice-3-as-built)).

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
| Note 2 | 8, 70, at most 215, 7 | SAFE: `NEVER TO AN ESC. PROPS OFF.`, 27 characters, 161 px, `UI_COL_WARN`. RUNNING: `CH17 OFF  CH18 OFF  FS OFF  LOST OFF`, at most 36 characters, 215 px, `C_TEXT`, `UI_COL_WARN` while a flag is on. After a stop: `NEVER TO AN ESC. PROPS OFF.`, `UI_COL_WARN`, as while SAFE, ending at x 168 |
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
| Probe | none | `CHECKING SIG GP0 FOR AN ESC.` ([unavailable pin](#unavailable-pin)) in place of notes and rows | hidden |
| RUNNING | `RUNNING` | the ceiling and the flags | live, values `UI_COL_LIVE` |
| Stopped by the heartbeat | `STALLED` | `STOPPED: SCREEN STALLED` (23 characters, 137 px, as SERVO TESTER's) and `NEVER TO AN ESC. PROPS OFF.` | disabled, the last values |
| Stopped by the idle rule | `SAFE` | `STOPPED: NO TOUCH FOR 30 S.` (27 characters, 161 px) and `NEVER TO AN ESC. PROPS OFF.` | disabled, the last values |
| Encoder refused | `SAFE` | `STOPPED: FRAME REFUSED.` (23 characters, 137 px) and `NEVER TO AN ESC. PROPS OFF.` | disabled |

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
| The bootloader probe runs | `CHECKING SIG GP0 FOR AN ESC.` | `C_DIM` | 167 px |
| A bootloader answered the probe | `AN ESC BOOTLOADER ANSWERS ON SIG GP0.` `UNPLUG THE ESC BATTERY AND LEAD.` `START WAITS UNTIL NONE ANSWERS.` | `UI_COL_WARN` | 221, 191 and 185 px |
| Held, the job not bound yet | `TAKING SIG GP0` | `C_DIM` | 83 px |
| The bind refused the clock | `SIG GP0 COULD NOT BE TAKEN (CLOCK)`, and RETRY, the wide button at 6, 268, 228 × 46 | `UI_COL_WARN` | 203 px |
| The probe's line refused at the bind | `SIG GP0 COULD NOT BE TAKEN (PIO)`, `(DMA)` or `(CLOCK)` by `escwireJobBindError()`, `(REFUSED)` for `CORE1_JOB_PIN`, and RETRY | `UI_COL_WARN` | at most 215 px |
| No role on GP0 | `S.BUS NEEDS GP0.` `SET A PIN TO GP0 IN SETTINGS.` | `UI_COL_WARN` | 95 and 173 px |
| Armed, or another claim holds | `DISARM FIRST`, `WAIT: DSHOT RESTARTING` | `UI_COL_WARN` | backstops, unreachable through the UI |

`SIG GP0` reads `AUX GP0` when GP0 is AUX; the ESC line names the GPIO of
SIG, `GP1` in that wiring. In the default wiring the FC lead goes where the
ESC was, so `UNPLUG IT` comes first and the ESC line names no FC. The line
to plug the FC in is only on the KISS line, because that line is always
about GP0, where the FC goes; the ESC line names SIG, which is GP1 in the
AUX wiring, and the FC does not go on GP1.

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
(`ui_registry.cpp`), and its second line replaces
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
| Main view, after a stop | the stop's reason, then `NEVER TO AN ESC. PROPS OFF.`: HOLD TO START is live and no probe runs before it |
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
| pio2 | DShot and KISS | empty after the probe |
| PIO, during the probe | | the escwire line: one state machine and 25 words in a block with room, as for ESC SETTINGS' job; which block is not verified |
| UART0 | unused | enabled from the bind, TX on GP0 after the probe, RX off |
| DMA | 3 of 16 | 5 of 16 during the probe, the escwire line's 2; 3 of 16 after it |

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
- the job through the claim: SIO's output off first, the clock refusal and
  RETRY, RXE off, the pull-down at the switch from the probe, GP0's control
  register never on the UART function without OUTOVER inverted, GP1
  untouched; the release only after BUSY clears, SIO's output
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
  frame; KISS on AUX GP0; a new visit starting in the probe with no trip;
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
| 3. Probe, built | the connect cycle on GP0 as a phase of the `SbusOut` claim before the UART bind, the refusal while a bootloader answers, the repeat until a cycle with no answer, MENU's run cue at an answer, `pinClaimWantLeavePage()` and the mark for decision 8's leave page after an EFM8 answer, the gate `sbusOutProbePassed()`, its host tests on the bootloader models of slice 0 | `sbus_out_job.*`, `escwire_job.*`, `ui_menu.cpp`, a comment in `usb_bridge.h`, `rx.cmake`; in `test/` the job's test file, the escwire line's fake and the GPIO stub; CI's stack step | 400 in `src/`, 700 in `test/` |
| 4. Screen, built | `sbus_out_view.*`, `ui_sbus_out.*`, OUTPUT's entry, `test/test_sbus_out_ui.cpp`, `test_ids.cpp`, the preview, `tools/make_previews.py`, `tools/check_wiki.py`, README and wiki pages RX-Signals and RX-Signale with the [warnings](#warnings), STATUS, CHANGELOG, this note's status | 4 new in `src/`, 1 new in `test/`, 10 edited | 750 in `src/`, 700 in `test/`, 200 of documentation |
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
and the pad register with its pulls in the GPIO fake (`fakes.*`, the stubs
`hardware/structs/io_bank0.h` and `pads_bank0.h`), and
`test/test_sbus_out_job.cpp`, `runSbusOutJobTests`. CI's firmware job
measures the job's deepest stack chain, 192 bytes against 1,792. Choices
the note left open, or made more exact:

- **Frames never closer than 7 ms.** A pass late by less than a period
  keeps the 14,000 µs grid, which brings the next frame closer: at 13 ms
  late the next is due 1 ms later, while the frame is still on the wire,
  and would follow it with no gap, which a decoder that frames on a
  2000 µs gap reads as one frame. `SBUS_OUT_MIN_US`,
  7,000 µs, the other period rcbench names, keeps every gap at 4,000 µs or
  more, and also spaces the first frame of a START soon after a STOP. A
  pass exactly one period late re-anchors as a later one does, so it does
  not send two frames in a row.
- **The FIFO wait is in the core.** `sbusOutCoreStep()` takes UARTFR.TXFE
  as `txEmpty`, so a due frame that waits for the FIFO is a host case of
  the pure core.
- **The throttle channel and the ceiling are Start's.** A `Frame` carries
  them as the note's struct has it, and the core ignores them: the channel
  the FC throttles on is the one START fixed. A Start with an index above 3
  is refused.
- **Core1 holds MAX THROTTLE's ceiling.** The job passes
  `sbusOutCeilRaw(settings()->maxThrottle)` to `sbusOutCoreTake()` as
  `ceilMax`, and Start clamps the command's ceiling to raw 172 to it, so a
  screen that sends a wrong ceiling cannot pass MAX THROTTLE. A lower
  ceiling from the screen is kept. Settings do not change during a claim.
- **The bind starts as the release does:** SIO's output enable for GP0 off
  and its output low, whatever an earlier owner left. The pull-down comes
  at the switch from the probe ([slice 3 as built](#slice-3-as-built)).
- **A backstop for the release.** The release lets GP0 go
  `SBUS_OUT_RELEASE_MAX_MS`, 10 ms, after its first pass even while BUSY
  stays set, so a UART that never finishes cannot hold the claim in
  Leaving. The last frame needs at most 3,000 µs.
- **A clock refusal never routes the UART to the pad.** The bind has set
  only SIO's output by then; it turns UART0 off and puts the pull-up on GP0
  ([slice 3 as built](#slice-3-as-built), transition 6).
- **Each bind starts with no trip**, the note's "visit", and takes no
  command sent before it: the bind takes the command block's seq as
  acknowledged. It starts in the probe; the switch to the UART starts SAFE
  and takes the seq again ([slice 3 as built](#slice-3-as-built)).
- **RXE is cleared by a read-modify-write of UARTCR** after
  `uart_set_format()`, which on the chip restores UARTCR after its wait.
- **The status is published on every pass**, as the servo job's is.
- **Nothing reaches it.** A host test reads every source under `src/`,
  subdirectories included, with comments blanked, and fails when a line
  names `PinUser::SbusOut`, `Core1Job::SbusOut`, `sbusOutJobCommand()`,
  `sbusOutClaimRoles()` or `SBUS_OUT_JOB_OPS` and is not one of nine: the
  job's declarations and definitions of those names, its line and its
  `static_assert` in the job table, and the claim's row. An inline function
  in `pin_claim.h` that claimed the output fails it, so no screen can claim
  or command the output before slice 3's probe.

The TX fake sends each UARTDR write from a 32-entry FIFO at the format and
rate set, back to back, 12 bits of 10 µs at 100,000 baud 8E2; TXFE, TXFF
and BUSY follow it, and a character still on its way is cut when the UART
stops or GP0 leaves the UART function with OUTOVER inverted. It does not
model the bits' levels: a character counts as on the pad while GP0's
control register held FUNCSEL UART and OUTOVER inverted from its start bit
to its last stop bit. The pad register starts at the RP2350's reset value,
`PADS_BANK0_GPIO0_RESET` of SDK 2.1.1, 0x116: ISO and the pull-down set, IE
and OD clear. It holds IE, OD, ISO and the pulls: `gpio_pull_down()` and its
kin write PUE and PDE, and a pin's pull is read from them, so a
whole-register write that drops the pull-down is seen. What the PL011 drives
while disabled and the level on the pin are not modelled.

Mutation check, on slice 2's code: 98 mutations of `src/sbus_out_job.cpp`,
each built into the host suite and run. 94 fail it: the clamps, the START
frame and its channels, the seq rule, each kind's rule in Take, MAX THROTTLE's
ceiling on core1 and its clamp at 1811, the trip, the 7,000 µs bound and the
grid, the TXFE wait, the refusal, the ceiling and the roles, the NO GP0
refusal, SIO's output off and low at the bind, the pull-down and its order,
the pull-down dropped by a pad write that names PDE, the baud check and the
clock refusal's release of GP0, 8E2 and RXE, OUTOVER and the single
control-register write, IE, OD and ISO and their order, the stale-command rule
at the bind, the status fields, BUSY and the backstop, the release's function,
pull-down and order, the critical section's initialisation, and the kept STOP.
Four survive:

- `SbusOutStatus::refused` published as false. The published field is not
  tested: no command reaches an encoder refusal through the job, since the
  core clamps every channel to 11 bits. The core's own flag is tested with
  a channel set past the clamp.
- The release's `gpio_set_dir(0, GPIO_IN)` removed, and its
  `gpio_put(0, 0)` removed: the bind has set both, and nothing writes SIO
  for GP0 while the UART holds the pad. Equivalent on the host and, by
  that reasoning, on the chip.
- An extra clear of the pad's isolation before the control-register write,
  the later one kept. GP0 is then an SIO input with the pull-down that SIO
  does not drive, as the bind has turned its output off; judged
  equivalent, not verified on the chip. Moving the clear before the write,
  with none after, fails the suite.

### Slice 3 as built

Files: `src/sbus_out_job.h` and `.cpp`, the probe as the job's first phase,
the bind and the switch to the UART in their new order, and core0's gate,
state, EFM8 mark and run cue; `src/escwire_job.*`, whose
`escwireJobBindError()` names the bind failure of a refused open of the
line for the escwire job and the probe alike; `src/ui_menu.cpp`, whose
run cue ORs `sbusOutRunCue()`; `src/rx.cmake`, which builds the probe's
classification and connect cycle with their call graphs. In the host
suite: `test/fake_escwire_port.*` writes the line driver's pad changes to
the GPIO fake, serves rcbench's AM32 model (`fakeEwResetAm32()`) and, on
request, answers every first contact (`fakeEwAnswerEach()`); the stub
`hardware/gpio.h` names the PIO functions; `test/test_sbus_out_job.cpp`
runs the slice 2 tests through the probe and seven new tests of it. The
probe's tests share the job's file and harness rather than a new file: the
probe is a phase of the job. CI's "S.BUS output job stack on core1" reads
the probe's call graphs and adds the deepest line-driver function, as the
ESC settings step does: 376 bytes against 1,792, 192 in slice 2. The chain
is `sbusOutJobStep()`, 112 bytes, and `publish()`, 152, with 112 for the
driver's `ew_line_start()` through the pointers of `ew_driver_t`.

How the probe shares the line driver, and the rest the note left open, the
developer's:

- **One job, two phases.** The probe is `SbusOutPhase::Probe` of the S.BUS
  output's job, not a job of its own: one bind, one release, one row in the
  claim, and core1 holds GP0 from the bind to the release. The job opens ESC
  SETTINGS' line on GP0 with `escwirePortOpen()`, rcbench's `ew_line`, one PIO
  state machine of 25 words and 2 DMA channels while it is open, and runs ESC
  SETTINGS' connect cycle (`greet_cycle.h`) on core1's `millis()`. The driver
  returns at once (`ew_wire.h`), so a pass of the probe waits for nothing. The
  AM32 transport, `am32_bl`, with which ESC SETTINGS greets on core0, is not
  used: it busy-waits and masks interrupts for each byte, and it runs on core0,
  where no job runs.
- **The greeting** is rcbench's first contact (`ew_a_contact()`), 17 bytes,
  eight 0x00, 0x0D, `BLHeli`, 0xF4 0x7D, at 52 µs a bit; ESC SETTINGS sends the
  same with twelve 0x00. Both are valid greetings for the EFM8 bootloader
  ([bluejay-blheli-s.md](bluejay-blheli-s.md#the-greeting-already-fits)), and
  rcbench's session sends this one to AM32's. One greeting is 8.84 ms on the
  wire; it waits `AM32_GREET_TMO_MS`, 30 ms, for a first byte, as ESC SETTINGS'
  does, and `EW_A_GAP_US`, 2 ms, for each next, up to 40 bytes, as many as the
  AM32 handshake reads. A cycle with no answer takes 1,000 ms and 3 × 38.8 ms,
  1,117 ms; with passes every 500 µs the host suite's takes 1,118 ms.
- **What counts as an answer.** `sbusOutProbeAnswer()` reads
  `blAnswerClassify()`: an EFM8 answer, or a reply holding a piece of one
  (`BlAnswer::fragment`), is an EFM8 bootloader; an AM32 answer an AM32
  bootloader. Only these set the cue and the counts, and only an EFM8 one the
  mark.
- **Any byte holds START.** A greeting whose capture holds any byte, a
  bootloader's answer or not, makes its cycle prove no silence, and so does a
  greeting that did not go out, refused by the driver, or whose capture is cut
  or not whole. So the UART takes GP0 only after a cycle of 3 greetings each
  sent, each read whole, and each answered by no byte at all. Bytes that are no
  bootloader's answer set no cue and no mark; only the cycle repeats. This
  differs from ESC SETTINGS, which reads such bytes as no answer and so may
  give DShot back after them: ESC SETTINGS greets an ESC that should answer,
  where the probe greets a line that should carry an FC's S.BUS input, which
  sends nothing. A byte there is something that transmits on GP0, a
  bootloader's garbled answer among the candidates, and holding START for it
  costs an FC nothing. rcbench's EFM8 model is such a case: it answers a later
  contact with 0xC2, and the probe holds START for it.
- **The line between greetings.** The driver parks the pad on its pull-down
  after a capture it cut or could not take whole (`ew_line_poll()`,
  `ew_wire.h`). The job puts the line back to `EW_LINE_UP` after every
  capture, so every quiet phase and every wait between greetings is on the
  pull-up, as decision 1 asks.
- **The gate.** Core1 takes no command during the probe. The bind takes the
  command block's seq as acknowledged, as slice 2's did, so a STOP held from an
  earlier visit does not hold back later commands, and the switch does it again,
  so no START sent before the switch is ever taken. Each bind starts in the
  probe, so BACK TO OUTPUT (`pinClaimKeep()`) probes again; STOP and START
  within a visit do not. For slice 4: `sbusOutProbePassed()`, the gate, true
  from the switch to the release, and `sbusOutProbePoll()`, once a UI frame, for
  the rows' area: Off, Checking, Answered or Passed.
- **The run cue and the EFM8 mark need no screen.** The status counts answers
  and EFM8 answers and stamps the last one (`answerSeq`, `efm8Seq`, `answerMs`),
  and no release clears them. The counts and core0's marks are 32 bits: at one
  answer a greeting, wrapping takes over 5 years, where 16 bits would read the
  cue as gone after exactly 65,536 answers. `sbusOutRunCue()`, ORed into MENU's,
  stands from an answer of either kind until an eRPM frame stamped after the
  last answer, as ESC SETTINGS' does. `sbusOutProbeEfm8()`, the mark for
  decision 8's first step on the leave page, stands from an EFM8 answer or a
  piece of one until `sbusOutProbeRestart()`, which RESTART DSHOT calls; it
  clears the cue too, since that page took the ESC's battery off. After an AM32
  answer alone RESTART DSHOT leaves the cue: the page asks for no power-off, and
  the cue waits for an eRPM frame.
- **`pinClaimWantLeavePage()`** is called by `sbusOutProbePoll()` while the EFM8
  mark stands, as the owner's decision asks. The claim's row already gives
  `PinUser::SbusOut` the leave page from the claim's start, so today the call
  changes nothing; it keeps the page if the row ever drops it.
- **The heartbeat does not stop the probe.** It sends only the greeting, as ESC
  SETTINGS' cycle does, and START needs the heartbeat in any case.
- **The unreachability test** also fails a line under `src/` that reaches a pin
  user or a core1 job by a number or walks the tables that hold them: a cast to
  `PinUser` or `Core1Job`, a pointer or a reference to one, `const` or not,
  C-style, functional, braced, `static_cast` or `reinterpret_cast`; and any use
  of `PIN_USERS` or the job table, an index or a range-for that picks a row by
  its fields. The tables' declarations, `pinUserInfo()`, `pinUsersComplete()`,
  `core1JobInit()` and the job table's lookups are allowed by line. It does not
  catch an enum value made without a cast, through `memcpy()`, a union, a
  template or stored bytes, nor a cast behind a macro or a typedef of another
  name; the test says so. The probe adds one allowed line, the call of
  `pinClaimWantLeavePage(PinUser::SbusOut)`.
- **The model of a bootloader.** rcbench's simulated ESC answers the first
  contact once and a later one with 0xC2, its reply to a frame whose CRC fails.
  By the EFM8 bootloader's source a later contact's leading 0x00 read as a
  restart and the rest as a new greeting (below), so the probe's tests ask the
  model to answer every contact, the EFM8 and the AM32 alike, and test the
  model as it is too. Not tested on an ESC.
- **The greeting, read from the bootloaders' sources.** Read for this slice,
  not tested on hardware:
  - AM32's bootloader takes the 17-byte greeting: `bootloader/main.c` at
    578ff29, lines 604 to 611, checks `rxBuffer[16]` for 0x7D and bytes 8 and
    9 for 0x0D and `B` before it checks the 21-byte greeting at
    `rxBuffer[20]`, and the same 17-byte check is at 3a9e057, lines 293 to
    300. A recognised greeting sends the device information and returns
    before any line that counts toward `invalid_command`:
    `send_BAD_ACK()` and `send_BAD_CRC_ACK()` (lines 555 to 565), the reply
    to an unknown command (about line 816), a full buffer (about line 1036)
    and a receive that ends with no message (about line 1045). The probe's
    repeated greetings do not walk an AM32 out of its bootloader.
  - Betaflight's 4-way interface sends this 17-byte string in builds without
    `USE_SERIAL_4WAY_SK_BOOTLOADER`, the 21-byte one with it
    (`src/main/io/serial_4way_avrootloader.c` at 134ce0f, lines 206 to 211).
  - The EFM8 bootloader of Bluejay (`src/BLHeliBootLoad.inc` at 0368d11) and
    of BLHeli_S (`BLHeli_S SiLabs/BLHeliBootLoad.inc` at a5bcf99) syncs on
    each byte's start bit (`getc`, lines 356 to 362 and 329 to 333). In its
    command loop a second greeting's first four 0x00 are the command 0x00 0x00
    and its CRC 0x0000, which restarts the bootloader (`main` to `rst`, lines
    149 to 199 and 139 to 181). The restart waits 100 µs (`init`, lines 66 to
    68 in Bluejay's), and the scan needs a falling edge before the `B`: the
    four 0x00 and the 0x0D left give about 2 ms of line, so the scan finds
    the greeting again and the bootloader answers it.


GP0 at each transition. The SDK calls are read in SDK 2.1.1, the driver's
in `ew_line.c`; the pad's behaviour is the RP2350 datasheet's as slice 2
records it. None is measured.

1. **Before the bind.** As SIG, an SIO input on the pull-up, the pump's
   teardown; as AUX with KISS on, the NULL function with the pulls off.
2. **The bind.** SIO's output enable off and its output low. `uart_init()`
   resets UART0 and enables it with TX and RX (`uart.c` lines 42 to 92) and
   writes no pad register; then 8E2 and RXE off. GP0's FUNCSEL is not the
   UART's, so the UART's high idle reaches no pin. Then `ew_line_open()`:
   the state machine's pin latch high and its direction an input before
   the pad is the machine's, the pull-down, 2 mA and slow slew, the PIO's
   function through `gpio_set_function()`, the input buffer off
   (`ew_line.c` lines 168 to 242). In the same call `EW_LINE_UP`: the
   pull-up, the input buffer on, the pad released (lines 415 to 433). The
   line has the pull-down only between those writes, microseconds, as in ESC
   SETTINGS' bind; not measured.
3. **The probe.** In the quiet phase an input on the pull-up, the PIO's
   function. A greeting drives the pad at both levels for its 17 bytes and
   releases it within one state-machine cycle of the last stop bit to the
   pull-up for the answer (`ew_wire.h`). After a capture the driver cut or
   could not take whole it parks the pad on its pull-down; the job puts
   `EW_LINE_UP` back on the same pass, after every capture. The line rests
   high, the idle level of the greeting's serial line, in every quiet phase.
4. **The switch.** `ew_line_close()`: the pull-down, `gpio_init()`, which
   sets SIO's output enable off and its output low before it selects SIO
   (`gpio.c` lines 278 to 282), drive and slew as before the open, the
   machine stopped, its direction released and its resources freed
   (`ew_line.c` lines 244 to 275); `escwirePortClose()` then puts the
   pull-up on. Then the job's pull-down: the line falls from the pull-up's
   high to the pull-down's low, the inverted UART's idle level. Then slice
   2's `padToUart()`: IE on and OD off, one control-register write with
   FUNCSEL UART and OUTOVER inverted, ISO cleared. The UART is idle since
   the bind, so the pad shows its inverted idle, low. SIO drives nothing at
   any point and the pad never carries the UART's high idle. The close's
   pull-down stands while it stops the machine, aborts and unclaims its two
   DMA channels and removes the program, and the port's pull-up until the
   job's pull-down: microseconds, not measured.
5. **A release during the probe.** `ew_line_close()` cuts a greeting on the
   wire, the pad first, and the port puts the pull-up on: GP0 an SIO input
   on the pull-up, the level of the probe; then `uart_deinit()`. The pad
   never was the UART's. A greeting cut short is a scan that fails for a
   bootloader, which then answers nothing; not verified on an ESC.
6. **A refused clock.** `uart_init()`'s rate more than 1 % off refuses the
   bind before the line opens: `uart_deinit()`, then the pull-up, SIO
   already off, GP0's function as the earlier owner left it. A refused open
   of the line, for its PIO, its DMA channels or its clock, does the same
   after `uart_init()`; the refused open claims nothing and leaves the pin
   untouched (`ew_line.h`), and the bind failure is the escwire job's
   (`CORE1_JOB_PIO`, `CORE1_JOB_DMA`, `CORE1_JOB_CLOCK`). The line has not
   been low since the pump's teardown, and the pull-up keeps it there, as ESC
   SETTINGS' refused bind does, so a bootloader that may wait on it sees no
   edge; in the AUX wiring with KISS on, the pulls were off and the pull-up
   is new.
7. **A release after the switch**: slice 2's, unchanged, which leaves GP0
   an SIO input, low, on the pull-down. Each release leaves the line at the
   level its phase held: high during the probe, low after the switch.

Tests (`runSbusOutJobTests`, on rcbench's EFM8 and AM32 models behind the
escwire line, the UART0 fake and the GPIO fake): which replies are an ESC on
GP0; the bind, the line and the switch write by write, from SIO driving high,
OD and ISO set by hand; no ESC: nothing sent for 999 ms, the first greeting at
1000 ms, three contacts, SAFE at 1,118 ms after one cycle, a STOP held before
the bind taken as acknowledged, a START sent in the quiet phase and one during
the third greeting never taken, START then frames, STOP and START with no probe
between, the gate shut once the job is off; an EFM8 bootloader: Answered,
signature 0xE8B2, the cue, the EFM8 mark, the leave page, three cycles and nine
answers with START refused and GP0 never on the UART, unplugged and SAFE after
the next whole cycle, the mark and the cue kept after the claim and gone at
RESTART DSHOT; an AM32 bootloader: Answered, the cue and no mark, the cue kept
by RESTART DSHOT and by eRPM frames stamped at the answer, before it and before
the probe, MENU's strip line `POWER-CYCLE THE ESC TO RUN IT`, gone after an
eRPM frame after the answer; a piece of an EFM8 answer taken as one; bytes that
are no answer holding START through three cycles with no cue and no mark; the
EFM8 model's 0xC2 to later contacts holding it too; a greeting refused costing
one cycle; a capture cut and a capture not whole at each of the three greetings
costing one cycle, with GP0 on the pull-up and the line at `EW_LINE_UP` on
every pass, through the next quiet phase; the answer counts 32 bits wide; a
release in the quiet phase and in the middle of a greeting, each in one pass,
GP0 an SIO input on the pull-up and never the UART's; BACK TO OUTPUT probing
again; a refused clock and a refused open of the line for PIO, DMA and CLOCK,
nothing written to GP0's control register, the pull-up, RETRY; a START every
100 ms of a second visit's probe, none taken; and the unreachability test's
matcher on seventeen casts, pointer casts, indices and walks over the tables,
and seven lines it must pass.

Mutation check: each mutation applied to a copy of the tree, the host suite
built and run. 102 on the branch before main was merged: 86 of
`src/sbus_out_job.cpp`, 2 of its header, 1 of `src/ui_menu.cpp`, 2 of
`src/escwire_job.cpp` and 11 of the unreachability test. 99 fail the suite:
the rule's EFM8, piece and AM32 cases and noise taken as an answer; SIO's
output off and low at the bind; the clock refusal's `uart_deinit()`, its
pull-up, a pull-down in its place and the refusal ignored; the refused open's
`uart_deinit()`, its pull-up, its bind failure, the refusal ignored and the
line opened before the UART; the pull-up after the open, the greeting's bytes,
the quiet phase's start, the phase, the bind's acknowledged seq; each field of
the probe's reset; the switch without the close, without the pull-down, the
pull-down after the UART write, the close after it, the phase and the
switch's acknowledged seq; the pull-up after a capture dropped; a byte, a cut
capture and a capture not whole each no longer holding the cycle, and none
holding it; each step of counting an answer, the EFM8 branch taken for AM32;
the cycle's count, its end at every greeting, the UART without a clean cycle
and a cycle never clean again; the poll's wait, the greeting's end, the
judging, greetings in the quiet phase, the capture's size, the wait for an
answer, the rate; the refused greeting's unclean cycle and its count;
commands taken in the probe and the status not published; the release
mid-probe as the UART's, without the close, without `uart_deinit()`, without
the phase or the publish, and the phase left after the UART's release; the
gate, each state of the poll, the EFM8 mark, each condition of the run cue
and of RESTART DSHOT's clear; either answer count 16 bits wide; the pad's IE
and OD write and its ISO clear at the switch; MENU's cue; two of the escwire
job's bind failures; and each alternative of the matcher, and the matcher
dropped. One of them, `pinClaimWantLeavePage()`'s call removed, fails only
the unreachability test's check that each allowed line is still there: by
behaviour it is equivalent, as the claim's row gives the leave page from the
start. After main was merged, whose `bridge_4way.cpp` has a file-static
`s_ops` of its own, the matcher's 13 mutations and two of the probe's ran
again on the merged tree: 14 fail. Three survive in all, each equivalent:

- `EW_A_GAP_US` ten times longer as the wait between an answer's bytes.
  rcbench's model ends an answer by its byte count and does not model the
  wait after its last byte, so the host cannot tell. On the chip a longer
  wait would lengthen each answered greeting, not change what holds a cycle.
- The release during the probe without its `clearState()`. The probe takes
  no command and sends no frame, so the frames' state is still the bind's
  reset when it ends; the call keeps the status cleared by the release
  whatever the probe does.
- Each allowed line found at least once in place of exactly once. A second,
  identical copy of an allowed line does what the first does: a declaration,
  the claim's row, which `pinUsersComplete()` holds to one, or the request
  for the leave page.

The script is not kept.

Not verified: everything on a pin and an ESC. No greeting of the probe has
reached a Bluejay, BLHeli_S or AM32 bootloader from this board; whether
rcbench's 17-byte contact gets the same answers as ESC SETTINGS' 21-byte
greeting, what a bootloader does with a greeting cut short, and the line's
level through the pull changes at the bind and the switch are read from the
sources, not measured.

### Slice 4 as built

Files: `src/ui_sbus_out.h` and `.cpp`, S.BUS OUTPUT with its SET view and
its leave pages; `src/sbus_out_view.h` and `.cpp`, the pure formatter;
OUTPUT's entry in `UI_RX_ENTRIES` (`src/ui_registry.cpp`) with
`&UI_SBUS_OUT`; `UiScreenId::SbusOut`; `sbusOutJobHeld()` in
`src/sbus_out_job.*`; `src/rx.cmake` and `test/rx.mk`. In the host suite:
`test/test_sbus_out_ui.cpp`, `runSbusOutUiTests`; S.BUS OUTPUT's tables in
`test/test_ids.cpp`; the source test of `test/test_sbus_out_job.cpp`
rewritten (below); the preview `docs/sbus-out-preview.png`; and in
`test/fakes.cpp` a count of the UART0 fake's queued TX characters that
starts at the first that may still wait, so a 30 s run of frames no longer
rescans every character sent on each UARTFR read. `tools/check_wiki.py`
gains a fifth check: RX-Signals and RX-Signale each quote the four warning
texts of [warnings](#warnings). Choices the note left open, or made more
exact, the developer's:

- **The gate.** `startAllowed()` is `!running && sbusOutProbePassed()`, and
  HOLD TO START's hold is live only on it; the hold's firing calls
  `startFrames()`, the one place that sends a Start. The gate holds only
  while this claim's job is bound in its UART phase, so it implies the claim
  runs with no bind error; a check of either was equivalent under mutation
  and is not made. The hold times only a press that landed on the live
  button (`ui_hold.h`), so a finger kept on START through the probe starts
  nothing when the probe passes, and the press that stops the frames never
  starts them again.
- **The probe's lines name GP0 by its role**, `CHECKING SIG GP0 FOR AN ESC.`
  (28 characters, 167 px) and `AN ESC BOOTLOADER ANSWERS ON SIG GP0.`
  (37 characters, 221 px), `AUX GP0` when GP0 is AUX: menu-b.md decision 10
  names a pin by its role and number, and `test/check_docs.py` refuses
  `CHECKING GP0` in the manual. The warnings and OUTPUT's lines keep the
  note's `GP0`, the pin whatever its role, as the monitor's line names
  `GP1`. No remaining time is shown while the probe runs: the note gives
  none.
- **When to plug the FC in.** The owner's answer of 2026-11-15 on the
  monitor's wait text ([rx-signals.md](rx-signals.md#slice-3-as-built),
  "When to plug the receiver in") is the KISS line's
  `CONTROLLER IN ONCE SAFE SHOWS.`: the `SAFE` chip shows only once the
  probe has passed and the UART holds GP0. The ESC line names no FC, by the
  note's reason under [unavailable pin](#unavailable-pin): in the default
  wiring the FC lead goes where the ESC was.
- **The bottom row stays** while a message fills the rows' area, with HOLD
  TO START drawn disabled, so the gate shows; it is not drawn under RETRY,
  whose wide button covers it, and a tap there reaches nothing under it.
- **The stopped states.** After the heartbeat's stop, the idle stop or a
  refused frame, note 1 gives the reason in its colour until the next
  START, and note 2 keeps `NEVER TO AN ESC. PROPS OFF.` in `UI_COL_WARN`:
  HOLD TO START is live then and no probe runs before it, so an ESC plugged
  in since the probe is the case the warning is for. Note 2 draws that
  warning alone, and HOLD TO START is drawn live. The screen's
  own idle stop is `SbusOutStop::Idle`, cleared by START. SET's footer
  carries the same warning in every state but RUNNING.
- **The sliders** take presses on the row's 22 px over the track's x span,
  x 54 to 199, and are drawn on the note's 10 px track with the 14 × 16 px
  knob of `ui_slider`. A slider's value is held to its step, and to the
  ceiling on the throttle row, once let go, so the next drag starts at the
  knob. The track past the ceiling is a 2 px `C_DIM` line along its middle:
  a dim band the track's height read as a fill to the ceiling. The mark is
  broken where the knob covers it.
- **The value column** is the frame last loaded, the START frame's values
  before the first frame of a visit or a START.
- **The throttle channel on SET** while SAFE also puts the sliders at the
  new channel's START frame, so the throttle row's knob and mark are what
  START sends.
- **BACK after an EFM8 answer** sends STOP before it opens decision 8's page
  before LEAVE, so no frame goes out while the page waits; STAY returns to
  the screen, SAFE. BACK opens that page again while the EFM8 mark stands,
  until RESTART DSHOT on that page calls `sbusOutProbeRestart()`. RESTART
  DSHOT on the plain page does not: on the chip core1 can count an EFM8
  answer between the screen's read at BACK and the release, and that late
  mark keeps MENU's cue, `POWER-CYCLE THE ESC TO RUN IT`.
- **SET's title** is `OUTPUT SET`, 10 characters, 118 px, after `SERVO SET`;
  the note names none. Its note band reads `STOP TO CHANGE THE CHANNEL.`
  while RUNNING and `START TO SET THE FLAGS.` while SAFE, in
  `UI_COL_WARN` as SERVO SET's.
- **The idle clock** is the last UI frame with a finger down, START's hold
  among them; the rule fires on the first frame more than 30,000 ms after it.
- **The host suite reads the commands.** `sbusOutJobHeld()` returns the
  command block, so a test sees what the screen sent, taken or not: no
  command during the probe, the Start's throttle channel and ceiling, the
  screen's own clamp of the throttle channel before core1's.
- **The source test** that held the output unreachable now holds the one
  path: a line under `src/` that names `PinUser::SbusOut`,
  `Core1Job::SbusOut`, `sbusOutJobCommand`, `SBUS_OUT_JOB_OPS` or
  `SbusOutCmdKind::Start`, or reaches a pin user or a job by a number, must
  be one of the listed lines, as often as listed: the job's, the table's,
  the claim's row, the probe's request for the leave page, and S.BUS
  OUTPUT's claim, release, keep, restart and retry lines, its one command
  call and its one Start. Then in `ui_sbus_out.cpp` the Start is sent only
  in `startFrames()`, whose name appears twice, at its definition and in
  the hold's call on the line gated by `startAllowed()`, whose one return is
  a chain of `&&` with no `|`, no `?` and none of the alternative tokens
  `or`, `and`, `not`, `bitor`, `bitand` and `compl` that ends in
  `sbusOutProbePassed()`. The numeric-cast checks stay and also catch a
  cast to `SbusOutCmdKind`, so `send((SbusOutCmdKind)1)`, a pointer to
  `startFrames`, a ternary and each alternative token in the gate each fail
  it.
  `sbusOutClaimRoles()` left the list: the screen calls it, and it is pure.
  The test is a lint of the sources, not a proof: a value made without a
  cast or a name, through `memcpy()`, a union or a macro, is not caught;
  nor are the alternative tokens `xor`, `and_eq`, `or_eq`, `xor_eq` and
  `not_eq` in the gate, although `|=` is refused as a `|`.
  The UI tests hold the behaviour: no command while the probe runs, START
  only through the hold.

Mutation check: each mutation applied to a copy of the tree, the changed
file built into the host suite and the suite run. 139 mutations on the
branch: 99 of `src/ui_sbus_out.cpp`, four of them a second path to START
for the source test (a second Start line, a second call of
`startFrames()`, the gate ORed away, a second claim line), 35 of
`src/sbus_out_view.cpp`, 2 of `src/ui_registry.cpp`, 1 of
`sbusOutJobHeld()` and 2 of the UART0 fake's queue count; and one of
`tools/check_wiki.py`'s new check, a warning dropped from RX-Signale. 135
fail: the gate, its probe and its frames, the hold's gate, START's reset
and command, the Start's throttle channel and ceiling, STOP on press on
both views, the sliders while SAFE, their snap and their page, the
ceiling on the slider and in the frame, the flags in the frame, the idle
rule's call, its touch clock, its STOP and its note, the Frame sent once a
change, each message and its colour, the role in the probe's lines, the
claim with no role, RETRY, the tap under RETRY, the EFM8 page, its STOP,
STAY, LEAVE, RESTART DSHOT's `sbusOutProbeRestart()`, BACK TO OUTPUT's new
visit, the leave page's lines and note, SET's locks, ends, flags, notes,
footer and STOP, the drawing of the chip, labels, knobs, mark, line,
values, page label and hold bar, the body's clear between views, each rule
of the formatter, OUTPUT's screen and second line, the job's command
block, a fake that took every queued character as started, and the four
second paths. A first run on an earlier commit left 17 alive: for 6 the
screen checked a thing twice (the claim's state, its bind error and
`s_claimed` in the gate, a STOP press's lift, and the claim's state beside
the probe's Off for `TAKING`) and no longer does; for 7 the suite gained a
test; the fake's mutation was made stronger; three are below. A later run
left the main view's early return after STOP alive, which is gone too.
After review, 16 more, all failing the suite: the three bypasses of the
source test (a cast to `SbusOutCmdKind`, a pointer to `startFrames`, a
ternary in the gate), the warning dropped or dimmed after a stop and the
reason dropped, RESTART DSHOT clearing a late EFM8 mark and never clearing
one, the idle clock with no touch or with SET's touches left out, `(PIN)`
read as `(CLOCK)`, and five of the source test's own checks (the cast to a
command kind, `?`, `|`, a third `startFrames`, a second return); the `|`
check survived its first run, until its samples failed on the `|` alone.
Five of the first 139 survive, each equivalent or out of the host suite's
reach:

- BACK's return to MENU from `ClaimState::Restoring` dropped: Restoring
  falls between `pinClaimPoll()` and `pinClaimEnd()` when core1 suspends the
  pump, which the host suite's frame order never makes; S.BUS MONITOR has
  the same line.
- BACK's `if (!s_claimed) return false;` dropped: with no claim,
  `pinClaimEnd()` leaves the claim Free and the next line returns, and the
  EFM8 mark, which would open decision 8's page, never stands without this
  screen's claim: every way out of a claimed visit with the mark is RESTART
  DSHOT, which clears it.
- LEAVE's return to MENU when the claim is Free: the page before LEAVE opens
  only after an EFM8 answer, so only with the job bound and the claim
  Running, which `pinClaimEnd()` takes to Leaving.
- SET's STOP on `down` in place of `pressed`: `uiPressing()` counts only a
  press that landed on STOP, which acts on its first frame anyway; while
  SAFE there is no STOP.
- The fake's count without its `startUs > now` test: from the first
  character that may still wait, every later one starts later, unless a
  stuck transmitter put one at `UINT64_MAX`, which no test un-sticks.

The script is not kept.

Not verified: everything on a pin, an FC and an ESC, as for slices 2 and 3;
whether an FC's configurator shows the 16 channels, CH17 and CH18 as sent.

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
- That an ESC answers the probe's 17-byte greeting as its bootloader's
  source says ([slice 3 as built](#slice-3-as-built)); what a bootloader
  does with a greeting cut short by a release.
- The level on GP0 through the pull changes at the probe's open and at the
  switch to the UART; which PIO block the probe's line takes.
- Whether repeated greetings count toward the AM32 bootloader's limit of
  invalid commands on an ESC; by the source at 578ff29 they do not
  ([slice 3 as built](#slice-3-as-built)).
- Betaflight's behaviour, read and not run; ArduPilot and INAV not read.
- Pull request #49's touch-loss rule, not merged.
