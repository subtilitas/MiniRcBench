# Where MiniRcBench stands

The running record: what exists, how far each part is verified, and what is
open. Two parts of this file are held to the tree by a checker. Every relative
link here and in [CHANGELOG.md](CHANGELOG.md) is checked by
`test/check_docs.py`. The coverage table under "Tests and CI" (continuous
integration) is written and checked by `tools/coverage.py`. Nothing else is
checked: the architecture figures, pin numbers, suite and job lists and
verification states are written by hand, against the code and the issue tracker
as of 2026-10-18. Read them as a dated record, not as an inventory.

MiniRcBench continues the DshotDisplay firmware, which remains a separate
project at
[github.com/subtilitas/DshotDisplay](https://github.com/subtilitas/DshotDisplay).
The commits named below are in this repository's history; the issue and report
numbers are in the DshotDisplay tracker.

[README.md](README.md) is the technical document, the
[wiki](https://github.com/subtilitas/MiniRcBench/wiki) is the manual,
[CHANGELOG.md](CHANGELOG.md) lists changes by release, and
[docs/design/](docs/design/README.md) holds the design notes.

## Architecture

MiniRcBench is a bench tester for one ESC (electronic speed controller) and
for servos. The servo tester (slice 2 of
[pin-claim.md](docs/design/pin-claim.md),
[servo-tester.md](docs/design/servo-tester.md)) drives one servo on the ESC
pin, or two on the ESC and KISS pins, with PWM (pulse-width modulation)
pulses. KST and Chaservo servo programming, on the same pins, is planned
(slice 3), with an optional bench-switched servo supply on GP0. Until then
the KST button under CFG is drawn disabled, and the screen behind it
is a stub. Before slice 3 comes a menu restructure
([menu.md](docs/design/menu.md), slices A to C). Slice A is built: one
scrolling list, `src/ui_list.*`, under the AM32 and SERVO SET lists, and one
hold, `src/ui_hold.*`, under HOLD TO WRITE, HOLD TO START, HOLD TO APPLY and
HOLD TO SAVE. Slices B and C are planned: a MENU list replaces the settings
screen, SETUP becomes one sectioned SETTINGS list with pole count and
throttle ceiling at its top, and the host tests address controls by id.
The pin claim both features share is built (slice 1): the AM32 editor and the
servo tester run on it. For the ESC, the board is the throttle source. It
sends bidirectional DShot (a digital ESC protocol in which the ESC
answers each frame on the same wire) and decodes the eRPM (electrical
revolutions per minute) and EDT (Extended DShot Telemetry) frames that come
back; RPM (revolutions per minute) is eRPM divided by half the motor's pole
count. An optional third wire carries KISS telemetry, a serial protocol from
the KISS ESC line with finer voltage and current.

The two cores of the RP2350 split the work by deadline:

| | core0 | core1 |
| --- | --- | --- |
| Work | USB (Universal Serial Bus) device task, touch, UI (user interface) rendering, SD (Secure Digital) card logging, settings, serial telemetry dump, the pin claim's state machine | DShot frame pump, eRPM and EDT decode, KISS request and receive; while a pin claim holds, the claiming screen's job instead: the servo job for SERVO, none for AM32 |
| Rate | one UI frame every 25 ms (40 Hz); USB task, log encode and card flush on every loop pass | one DShot frame every 1000 µs (1 kHz); KISS receive FIFO (first-in, first-out buffer) drained on every loop pass |
| May block | yes: display DMA (direct memory access) bursts of several ms, card writes of tens of ms | never |

The cores share one struct, `EscTelemetry` in `src/esc_task.h`, guarded by a
critical section. Core0 sends a heartbeat every UI frame. When none arrives for
250 ms (`UI_HEARTBEAT_TIMEOUT_MS`), core1 sends zero throttle on its own.
Writing flash parks core1, because core1 runs from XIP (execute-in-place)
flash, so a settings save is refused while armed and while a pin claim holds.

A screen that needs the signal pins claims them (`src/pin_claim.h`). The claim
asks core1 to suspend the DShot pump. Once core1 sees the request, it sends
zero frames for at least 250 ms (`ARM_ZERO_THROTTLE_MS`) after the last
throttle frame, then stops the pump, releases the pin, which turns its pulls
off, puts the pull-up back on it and ends the KISS receiver. A rebuild after a
SETUP pin change drains the same way. `loop1()` runs `escTaskPoll()` and then
`core1JobPoll()`, which binds, steps and releases the claiming screen's job
(`src/core1_job.h`): the servo job (`src/servo_job.h`) for SERVO; the AM32
transport runs on core0, and the KST job is not built.
Leaving rebuilds the pump from the wiring it had, disarmed. For SERVO and KST
the claim restarts DShot only after a page that tells the user to unplug the
servo.

Rates and defaults, from `src/config.h` and `src/main.cpp`:

- DShot bitrate: 150, 300, 600 or 1200 kbit/s. Default 600 kbit/s
  (`DSHOT_SPEED_KBAUD`), changeable on CFG → SETUP.
- EDT: voltage in 0.25 V steps, current in 1 A steps, temperature in 1 °C
  steps, plus stress and status. A field blanks to `--` 1000 ms after its last
  frame (`EDT_STALE_MS`). The enable command goes out as 10 consecutive frames:
  as the first frames after every start of the DShot pump, again once the ESC
  has answered eRPM for 300 ms while disarmed and stopped, then every 1000 ms
  while eRPM arrives and EDT does not. Every reply is decoded with EDT on.
- KISS: a request every 20th DShot frame, 50 Hz. The reply is 10 bytes at
  115200 baud with a CRC-8 (cyclic redundancy check, polynomial 0x07). A reply
  incomplete after 10 ms counts as a timeout. A KISS value gives way to EDT
  500 ms after the last good frame (`KISS_STALE_MS`).
- Blackbox log: 500 Hz frames, an I frame every 32 frames, 12 fields. About
  14.5 bytes a frame, 7.3 kB/s, per `config.h`. 8192-byte ring buffer, 512-byte
  card writes, 16 MiB pre-allocated per file, directory entry synced every
  2000 ms.
- Display: 240 × 320 pixel RGB565 framebuffer (16 bits per pixel: 5 red,
  6 green, 5 blue; 153,600 bytes), panel SPI
  (Serial Peripheral Interface) clock 62.5 MHz.
- Serial dump over USB CDC (communications device class) at 10 Hz
  (`SERIAL_TELEMETRY` = 1).
- Safety: arming needs a 1000 ms hold with the throttle at zero. The throttle
  ceiling starts at 400 of 2000 (20 %). Idle disarm after 30 s. Entering the
  settings screens disarms. DShot gives up the signal pin only after at least
  250 ms of zero frames since the last throttle frame.

PIO (programmable input/output) blocks: three, each with 4 state machines and
32 instruction slots.

| Block | Use |
| --- | --- |
| pio0 | free |
| pio1 | SDIO (Secure Digital input/output) card driver in `src/sdio/`: 2 state machines, 32 of 32 instructions, plus 2 DMA channels and no interrupt (`src/sd_hw_config.c`) |
| pio2 | DShot transmitter and receiver on adjacent state machines, 16 instructions; KISS receiver when KISS is on, 1 state machine, 9 instructions |

DShot and KISS claim from whichever block has room, and the Pico SDK
(software development kit) searches pio2 first. DShot binds first after every
release, so it always finds a block with no other user. A `static_assert` in
`src/esc_task.cpp` checks at build time that DShot and KISS together fit one
block: 3 state machines and 25 of 32 instructions. A bind that cannot get two
adjacent state machines is refused, and the status bar then shows `DSERR`.

PWM slices: slice 0 drives the backlight (GP16). Slice 6 carries the servo
pulses during a SERVO claim, GP29 on channel B and GP28 on channel A, one
frame rate for both, its counter at 1 MHz. The servo tester uses no PIO and
no DMA.

Board: the image is built for the Waveshare RP2350-Touch-LCD-2.8 only.

Build: native Pico SDK (software development kit) 2.1.1 with CMake, ARM cores
only. The one dependency besides the SDK is pinned by commit:
`no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` v3.6.2. Its FatFs, glue, card layer and SPI
driver are built, the SPI driver because the card layer calls into it; its
SDIO driver is not. The DShot driver in `src/dshot/` is part of the
tree, from rcbench `protocols/dshot` at commit 453cf334 (MIT, same copyright
holder). The servo sweep curve and the GPIO-to-PWM fold are rcbench files at
the same commit, copied unchanged into `lib/rcbench/`
([lib/rcbench/README.md](lib/rcbench/README.md)). The SDIO driver in
`src/sdio/` is part of the tree, MIT, written from
the SD Association's Physical Layer Simplified Specification v9.10 and the
RP2350 datasheet ([design note](docs/design/sdio-driver.md)). A release carries one UF2
(USB Flashing Format) image, `MiniRcBench-<tag>-touch-lcd-2.8.uf2`.

## Board

The Waveshare RP2350-Touch-LCD-2.8: an RP2350A, a 240 × 320 ST7789T3 panel, a
CST328 touch controller and a QMI8658 IMU (inertial measurement unit). GPIO
means general-purpose input/output.

| | RP2350-Touch-LCD-2.8 |
| --- | --- |
| Panel bus | SPI1: GP10 clock, GP11 data out |
| Touch | CST328 on I2C1 (Inter-Integrated Circuit), GP6 and GP7 |
| SD card | PIO SDIO on pio1, 4 bits, 10 MHz, GP19 to GP24 |
| GPIOs free for the two wires | 2: GP28, GP29 |
| ESC signal default | GP29, J4 pin 12 |
| KISS default | GP28, J4 pin 11, off |
| Battery sense | GP27, ADC (analog-to-digital converter) input 1, 3:1 divider |
| Power latch | GP26, asserted first thing in `setup()` |
| Other devices | PCF85063 RTC (real-time clock), I2S (Inter-IC Sound) audio codec, neither used |

The ESC and KISS pins are changeable on CFG → SETUP within the free set, and
the two wires never share a pin.

## What exists and how far it is verified

States used below. "Hardware-tested (owner report)": the owner reports it
working on hardware. "Hardware use recorded": a commit records use on a board;
the commit is named. "Reported by a user": a third party reports it. "Host
only": covered by the host suite, no hardware record. "Not recorded": no
record either way.

| Part | Host tests | Hardware |
| --- | --- | --- |
| DShot driver in `src/dshot/` (from rcbench 453cf334): frame, reply decode, three-state poll, RP2350 pin driver | frame and checksum, throttle map, line and group code, sampler against clock error, extended telemetry schedule against an AM32 model, three-state poll and the counters it feeds; the pin driver is compiled only | not tested on hardware in MiniRcBench. rcbench ran it at bidirectional DShot600 on one AM32 2.21 ESC (Skystars 35A); other rates and ESC firmwares are untested |
| Bidirectional DShot throttle, eRPM and RPM readout | core1 frame decision, throttle and arming interlocks, RPM smoothing | hardware-tested (owner report) with `pico-bidir-dshot`, the driver before `src/dshot/`; the owner has found no bug since DshotDisplay v2.3, and issue #1 is a separate, unreproduced report |
| EDT telemetry, automatic enable, expiry | merge, expiry, enable rule, decode of each frame type; enable at pump start and decode with EDT on against a simulated ESC, with and without EDT | hardware-tested (owner report) with `pico-bidir-dshot`; the enable at pump start and the decode of every reply with EDT on are not tested on hardware |
| KISS telemetry: request, PIO receiver, decode, merge, KISS/EDT tags | CRC, decode, resynchronisation, request payload, merge; the PIO receiver is typechecked only | hardware-tested (owner report) with `pico-bidir-dshot`; see below |
| DShot bitrate label in the status bar, `DSERR` | label at each bitrate, colour, `DSERR` | not recorded |
| SDIO card driver in `src/sdio/`: protocol, PIO and DMA bus, library callbacks | CRC7 and CRC16 against the specification's examples, command framing, response parsing, CSD (card-specific data register), four-bit packing, CRC status token; initialisation, reads and writes against a simulated card (standard capacity SDSC v1 and v2, high capacity SDHC, no card, voltage mismatch, timeouts, removal); the two PIO programs run in a cycle-level model against a card model (`test/pio_sdio_emu.py`: CMD0, R1, R2, single and multiple block reads, CMD12, a write and its CRC status token, at 400 kHz, 10 MHz and 25 MHz), a model and not proof of hardware timing; the bus code in `sd_pio.c` is compiled only, and the model's host side follows it by hand | not tested on hardware; open item 7 lists the bench tests |
| Blackbox logging to SD, CFG → SD LOG screen | encoder, ring buffer, round trip through `blackbox_decode`, screen states | hardware use recorded with the previous SDIO driver on the 2.8" board; see below |
| USB card reader (read-only mass storage) | handover state machine, screen | not recorded |
| AM32 settings editor over the signal wire (AM32 is an open-source ESC firmware) | init string, CRC-16/ARC, 39-field codec, timing advance encodings, write interlock; the screen on the pin claim, the latched GPIO, the drain before the handover; the list on `ui_list` and the swipe of 16 px per coarse step | hardware use recorded: ab79c67, bd90e17, both before the screen moved onto the pin claim; the claim and its drain, and the swipe of 16 px per coarse step: not tested on hardware |
| Pin claim (`src/pin_claim.*`) and core1 job dispatch (`src/core1_job.*`), the zero-throttle drain in `src/esc_task.*` | each refusal rule at its 500 ms boundary and their order; the handover decisions of `escTaskPoll()` (`escHandoverStep()`, `escDrainSlot()`, `escPumpFrameSent()`, `escPumpBind()`) one 1 ms slot at a time: 250 zero frames, the teardown 251 ms after the last throttle frame, the bind disarmed; the order of the two cores' steps against fake jobs, BACK and RETRY, Restoring, the unplug-page rule, the restore wiring, the start guard, the heartbeat rule across a millis() rollover; the PIO and pad calls around those decisions are compiled only | not tested on hardware; open item 9 |
| Settings screen layout: BEEP and SD LOG in one row, AM32, SERVO, KST and SETUP in the destination row, KST disabled | one gap in each row; SERVO live and opening its screen; disabled drawing, no pressed look and no action on KST | not recorded |
| Servo tester: the pure model (`src/servo_model.*`), the PWM driver (`src/servo_pwm.*`), the core1 job (`src/servo_job.*`), the slider and the screens (`src/ui_slider.*`, `src/ui_servo.cpp`); rcbench's sweep and PWM fold in `lib/rcbench/` | profiles, limits at their boundaries, steppers to their stops, command, angle and pulse mappings, SPEED, the dial's hit area; the PWM driver's bind, write, release and sharing rules against a register fake; SAFE at level 0, CENTRE at START, STOP on the next pass, the slew, sweeps, REVERSE B, the trip at 250 ms across a millis() wrap, kept across a rebind, the profile rebind; no pulse cut short at a release or a rebind, against a model of the counter and the latched compare; released pins pulled down; the screens through the UI: HOLD TO START at 1000 ms, the drag rules, the SET warning and HOLD TO APPLY, STOP on SET, the AUX refusals through the claim, the unplug page; rcbench's own sweep and fold tests; the SDK calls are run against the fake only | not tested on hardware; open item 10 |
| Settings in flash (CFG → SETUP) | block validation, CRC, board id check, pin defaults, the block layout DshotDisplay 2.x writes | not recorded; 46a64a7 states no hardware coverage |
| Touch handling, high-contrast theme, throttle gauge | tap on release, repeats, screen residue, gauge reach | hardware use recorded: the reports #22 to #24 named in the v2.3 commits; the fixes on hardware: not recorded |
| List and hold widgets (`src/ui_list.*`, `src/ui_hold.*`; slice A of [menu.md](docs/design/menu.md)) | the list: the scroll range and its clamp, the press rule at the viewport's edges, the row rectangle, the axis lock at 6 px, a tap on release and its slide-off, the scroll from the press point, 16 px per coarse step, the factor, the stop at a rail and the anchor after it, the clipped drawing; the hold: 1000 ms and not 999 ms, the reset on a lift and a slide off, a press that slid on, one firing per press, the latch, the progress bar's extent; through the screens: AM32's swipe on every field of the table, KV at 16 px a step and its rail, the AM32 and SERVO SET scroll ranges and edge rows, checks 4c91db9 passes too, DWELL's rail on SET, HOLD TO WRITE and HOLD TO SAVE at 999 ms and 1000 ms | not tested on hardware |
| Serial telemetry dump | none | not recorded |
| `tools/sdtest`, SD bring-up firmware | none, not built in CI | hardware use recorded in f16a70f and e9c9d9a, the 2.8" board with the previous SDIO driver among them; nothing is recorded for the current build |

KISS in detail. The owner reports KISS telemetry working on hardware, the
display's own telemetry request included. Issue #1 separately reports correct
KISS values with a receiver driving the ESC and the display only listening.
Whether each report covers the RP2350-Touch-LCD-2.8 is not recorded.

SD logging in detail. Commit 036adfe records the 2.8" board mounting a 64 GB
card over SDIO at 25 MHz and writing a file, with the library's SDIO driver
that `src/sdio/` replaces. Nothing is recorded for the current driver. Commit
5cec1ec fixes a logging fault reported from hardware. A run
at the shipped SDIO clock of 10 MHz is not recorded separately. BUF PEAK and
WORST FLUSH under sustained logging are not recorded.

Not in this history: the DshotDisplay repository's branches
`feat/usb-bridge` (a USB to one-wire bridge, 6693084) and
`copilot/create-lcars-display` (a themed UI, 5d17541) are not merged.

## Open items

1. **[Issue #1](https://github.com/subtilitas/DshotDisplay/issues/1),
   "Initialisation fails on some BL_Heli32 ESC".** Opened
   2026-09-11 by DerGeheimdienst. Open, no comments, not reproduced by the
   owner. This is an external report, not a confirmed defect. Conditions as
   reported:
   - ESCs: RAZOR32 and EXUAV 70A, both running BLHeli_32 32.6.
   - Signal wire on the display: the ESC does not initialise, and its start-up
     sounds lack the zero-throttle beep.
   - The reporter tried every DShot rate and shorter signal cables, with the
     same result.
   - Racerstar, T-Motor and Flycolor ESCs work. Their firmware versions are not
     given.
   - Signal wire on a receiver, KISS wire and GND (ground) on the display:
     the ESC arms from the transmitter and the display shows correct
     telemetry.

   Facts from the code that bear on it: the firmware drives only bidirectional
   DShot and has no plain DShot mode. While disarmed it sends DShot value 0
   every 1000 µs, with the telemetry bit set on every 20th frame when KISS is
   on. The first 10 frames after the DShot pump starts are the EDT enable,
   DShot command 13; after those, the enable goes out only while the ESC
   answers eRPM.
2. **Ring buffer size.** `SD_LOG_BUFFER_BYTES` is 8192, unmeasured. At
   7.3 kB/s it holds about 1.1 s. A card stall longer than that drops frames,
   which DROPPED FRAMES counts. BUF PEAK and WORST FLUSH on the SD LOG screen
   are the numbers to size it from.
3. **KISS line voltage.** The KISS specification puts the line at 3.6 V, the
   RP2350's absolute maximum GPIO voltage. No measurement is recorded. The
   firmware enables the internal pull-up on the KISS pin; a 1 kΩ series
   resistor is suggested in `config.h`.
4. **Settings flash and the USB card reader on hardware.** Not recorded.
5. **Design questions left open** in [kiss-telemetry.md](docs/design/kiss-telemetry.md)
   and [blackbox-logging.md](docs/design/blackbox-logging.md): which eRPM source
   the main readout uses above the crossover (about 5,500 RPM on a 14-pole
   motor), whether consumption is shown from ESC power-on or from arming,
   whether a slow frame type is worth adding.
6. **`tools/sdtest` is built by nothing in CI**, so a change that breaks it
   shows only in a local build.
7. **SDIO driver on hardware.** The driver in `src/sdio/` is not tested on
   hardware. Bench tests needed:
   - initialisation on an SDSC card (2 GB or less) and on SDHC and SDXC
     (extended capacity) cards;
   - sustained logging at 7.3 kB/s, with BUF PEAK and WORST FLUSH read off the
     SD LOG screen;
   - USB DRIVE: a host reads files, which takes multi-block reads;
   - the card pulled out while logging, while idle and while a host reads,
     then put back;
   - 10 MHz in the firmware, and 10 MHz and 25 MHz in `tools/sdtest`.
8. **The wiki of this repository has no page yet.** GitHub creates a
   repository's wiki only once its first page exists, so that page is made by
   hand in the Wiki tab. Until then the clone in `docs.yml` fails and no
   manual is published.
9. **Pin claim on hardware.** The PIO and pad calls of the drain, the
   teardown and the rebuild run only in the firmware; the host suite runs the
   decisions they act on. Bench tests needed, with an oscilloscope on the ESC
   signal pin: AM32 opened within 250 ms of the last throttle frame (zero
   frames for at least 250 ms after it, then no edges); the line's level
   after the teardown, which stops, releases and puts the pull-up back, so it
   should idle high; a SETUP pin change during the same window; and an AM32
   session left with BACK (DShot back, the EDT enable as its first
   frames).

10. **Servo tester on hardware.** Nothing of it has run on a chip. Bench
    tests needed (work breakdown item 5 of
    [servo-tester.md](docs/design/servo-tester.md)), with an oscilloscope on
    SIG and AUX:
    - SAFE after the claim: both lines low, no edges;
    - pulse width and frame period at 50 Hz and at 560 Hz (HELI TAIL 760),
      against the 1 MHz counter: 1500 µs reads 1500 µs, the wrap 19,999 and
      1,784 counts;
    - one servo, then two, the second channel started without a step on the
      first;
    - STOP latency: the last pulse ends within one frame, 20 ms at 50 Hz;
    - the heartbeat trip: core0 halted in a debugger, the pulses stop
      250 ms after the last UI frame;
    - the last pulse at BACK while RUNNING and at a profile change right
      after STOP: complete, not cut short; the pins leave PWM about 41 ms
      after BACK, and the rebind waits until about 41 ms after STOP, at
      50 Hz;
    - the line after the release (an input with the pull-down on) and
      through the unplug page, then DShot back on RESTART DSHOT;
    - STOP on SET while RUNNING: level 0 within one frame, as on SERVO;
    - a real servo on each profile, and whether it reads 3.3 V pulses.
11. **BACK from SD LOG returns to the tester.** The SD LOG button on the
    settings screen clears the settings screen's flag (`src/ui.cpp`, in
    `handleConfigTouch()`), so BACK from SD LOG lands on the tester, while
    the wiki page The-Screens says it returns to Settings. The test in
    `test/test_ui.cpp` that leaves SD LOG taps BACK twice and passes either
    way. Slice C of [menu.md](docs/design/menu.md) sends BACK to MENU and adds
    a test that pins it (its decision 11).

## Known limitations

- **Two free pins.** The board frees two GPIOs, GP28 and GP29. The ESC takes
  one and KISS the other, so there is no third choice and the SETUP steppers
  stay put. KISS is off by default and one tap on CFG → SETUP turns it on. A
  build-time pin override outside the free set, or both wires on one pin,
  fails the build.
- **Bidirectional DShot only.** An ESC that does not accept the inverted
  bidirectional signal is not driven.
- **Saved settings outrank `config.h`.** On a board with saved settings, a
  changed default in `config.h` has no effect after reflashing. SETUP marks the
  whole block, not each row: while no change is pending, its note line reads
  `SAVED SETTINGS IN USE` or `COMPILED DEFAULTS IN USE`.
- **Power loss during a log.** A log is readable up to its last directory
  sync, so pulling power loses up to 2000 ms (`SD_LOG_SYNC_MS`).
- **Logs past the pre-allocation.** 16 MiB lasts about 38 minutes at
  7.3 kB/s. Past that, or when `f_expand()` finds no contiguous space, FatFs
  allocates clusters as the file grows. The stall that causes is not measured,
  and a failed pre-allocation is not reported.
- **USB DRIVE.** Read-only. Refused while armed or recording, with the reason
  shown for 4000 ms. Arming is refused while a host holds the card.
- **Consumption.** KISS mAh (milliampere-hours) counts from ESC power-on, not
  from arming. EDT carries no consumption.
- **Log field names.** `eRPM[0]` and `escTemperature[0]` follow Betaflight's
  naming but are not in `blackbox_decode`'s list of known fields; `eRPMkiss[0]`,
  `vbatEdt`, `amperageEdt`, `escConsumption` and `escStress` are new names.
  Blackbox Explorer draws only the fields it knows. The header claims
  `Firmware type:Cleanflight`, as Betaflight's own logs do.
- **Landscape.** `LCD_ROTATION` 1 fails the build.
- **Servo tester.** No position feedback: the horn is drawn where the pulse
  puts it, not where the servo is. Two servos share one profile and one frame
  rate, because SIG and AUX are the two channels of one PWM slice. The profile
  is not stored and starts at STANDARD PWM 50 Hz on every entry.

## Tests and CI

**Host suite.** `make -C test run` compiles the firmware sources with a C++17
compiler against the stubs in `test/stubs/`, and the DShot core in
`src/dshot/` and the SDIO protocol layers in `src/sdio/` as C11, and runs one
binary, `dshot_tests`. `test/test_main.cpp` runs sixteen suites:

| Suite | File | Covers |
| --- | --- | --- |
| `runAm32Tests` | `test_am32.cpp` | AM32 init string, CRC-16/ARC, settings address per MCU (microcontroller) family, field codec, timing advance |
| `runUiTests` | `test_ui.cpp` (includes `ui_am32.cpp`) | screens, throttle modes, safety interlocks, core1 frame decision, EDT enable, EDT enable at pump start against a simulated ESC, touch, SETUP, SD LOG, USB DRIVE, the settings rows with SERVO live and KST disabled, screenshots |
| `runKissTests` | `test_kiss.cpp` | KISS CRC-8, decode, resynchronisation, request payload, KISS/EDT merge, expiry, EDT enable rule |
| `runGfxTests` | `test_gfx.cpp` | dirty bands, glyph rendering, clip rectangle, lines at any angle and thickness, arcs of dots |
| `runBlackboxTests` | `test_blackbox.cpp` | variable-byte and zig-zag encodings, header, frames, time prediction |
| `runLogRingTests` | `test_log_ring.cpp` | ring buffer, all-or-nothing writes, wraparound, high-water mark |
| `runSettingsTests` | `test_settings.cpp` | settings block, validation, board id check, pin defaults, the block layout DshotDisplay 2.x writes |
| `runUsbMscTests` | `test_usb_msc.cpp` | card handover state machine, RPM smoothing |
| `runDshotFrameTests` | `test_dshot_frame.cpp` | DShot frame layout, checksum, inverted checksum, EDT enable word, throttle map (from rcbench) |
| `runDshotTelemTests` | `test_dshot_telem.cpp` | line and group code, eRPM and EDT decode, checksum against single-bit errors, sampler (from rcbench); three-state poll, counters, every normalised eRPM frame decoded with EDT on |
| `runDshotEdtTests` | `test_dshot_edt.cpp` | extended telemetry schedule against an AM32 model (from rcbench) |
| `runSdioTests` | `test_sdio.cpp` | SDIO driver: CRC7 and CRC16 vectors, framing, responses, CSD, four-bit packing, CRC status token; initialisation, reads and writes against a simulated card |
| `runClaimTests` | `test_claim.cpp` (includes `core1_job.cpp`) | pin claim refusals at their 500 ms boundaries and their order, the handover decisions of `escTaskPoll()` one 1 ms slot at a time, the zero-throttle drain through the claim, Restoring, the order of the two cores' steps against fake jobs, BACK in every state, RETRY, the unplug-page rule, restore wiring, the job start guard, the heartbeat rule across a millis() rollover, no settings save during a claim, the AM32 screen on the claim |
| `runServoTests` | `test_servo.cpp` (includes `ui_servo.cpp`) | the servo model, the PWM driver against a register fake, the job core and the job through the claim, the slider, and the SERVO, SET and unplug screens through the UI, with screenshots |
| `runKstTests` | `test_kst.cpp` | the KST programming stub screen: not built, claims nothing, leaves at once |
| `runWidgetTests` | `test_widgets.cpp` | the list (`ui_list`) and the hold (`ui_hold`) driven with touch snapshots, each rule at its boundary |

`test/servo.mk` and `test/kst.mk` list each feature's sources and test files
for `test/Makefile`, as `src/servo.cmake` and `src/kst.cmake` do for
`CMakeLists.txt`. A feature's C sources and C test programs are built as C11:
`make run` also runs rcbench's `test_servo_sweep.c` (14 cases) and the
adapted `test_out_pwm_map.c` (5 cases) from `test/rcbench/`.

Other targets in `test/Makefile`: `syntax` typechecks the device-only sources
with `-Werror`; `pioemu` runs `test/pio_sdio_emu.py`, the SDIO PIO programs
assembled from `src/sdio/sd_pio.pio` and run one system clock at a time
against a card model, 18 runs over three clock rates, three card output
delays and two card latencies, and `make` runs it too; `bbcheck` encodes 2000 frames and decodes them with
Betaflight's `blackbox_decode`; `previews` and `wiki` rebuild screenshots;
`coverage` builds an instrumented suite. Scripts: `test/check_docs.py`,
`test/check_uf2.sh`, `test/check_uf2_selftest.sh`, `test/check_blackbox.py`,
`test/pio_sdio_emu.py`, `tools/check_wiki.py`, `tools/coverage.py`,
`tools/notices.py`.

**CI** (`.github/workflows/ci.yml`; on push to `main` or `master`, on pull
requests, by hand):

| Job | What it does |
| --- | --- |
| Firmware (`touch-lcd-2.8`) | builds with arm-none-eabi-gcc 13.3 and `STRICT=ON`; flash under 1 MiB and RAM (random-access memory) under 400,000 bytes; checks from the DWARF compile units that no file of the SD library's SDIO, ZuluSCSI-firmware or SdFat directories is in the image; writes `THIRD_PARTY_NOTICES.txt` with `tools/notices.py` and checks it against the image; compares the PIO model's assembler with the SDK's `pioasm`; checks the two pin overrides reach CMake and that `-DDSHOT_PIN`, `-DKISS_PIN`, `-DDSHOT_PIN_LCD_2`, and `-DKISS_PIN_LCD_2` are refused; validates the UF2 |
| Host tests | `make`, which runs the SDIO PIO model too; the suite again with both pin macros overridden; wiki screenshots and `check_wiki.py` |
| Blackbox round-trip | builds `blackbox_decode` from source, runs `make bbcheck` |
| Config permutations | typechecks all of `src/` with `-Werror` for 32 combinations, with the headers in `lib/rcbench/` on the include path: AM32 transmit mode × AM32 low-jump × rotation 0 or 2 × KISS on or off × SD logging on or off; checks the landscape guard fires |
| Workflow lint | actionlint 1.7.7 and a duplicate-key check on every workflow in `.github/workflows/` |
| Docs | `test/check_docs.py`, then Doxygen with zero warnings allowed |
| Coverage | `make -C test coverage`, then `python3 tools/coverage.py --check`; fails on drift from the table below, below 93 % in total, or below 85 % in any file |

**Release** (`.github/workflows/release.yml`; on `v*` tags, by hand): resolves
the tag, checks that it names the version in `src/version.h`, builds the image
after the host tests and `check_docs.py` pass, writes and checks
`THIRD_PARTY_NOTICES.txt` for it, publishes it as
`MiniRcBench-<tag>-touch-lcd-2.8.uf2` with `THIRD_PARTY_NOTICES.txt` and
`SHA256SUMS.txt`, which covers both, and confirms that those three assets
landed and no other, that they match their checksums, and that the notices
still pass their check. The notices name every component whose code is in the
image, with its pinned version and its licence text copied from the licence
files that ship with it; a missing or empty licence section fails the run
before anything is published.
Each run is titled `Release <tag>`, which Docs reads to find the tag. The
version is 0.1.0 and no MiniRcBench release exists yet; v0.1.0 is the first
tag.

**Docs** (`.github/workflows/docs.yml`): mirrors `wiki/` to the GitHub wiki
after every successful Release run, from a tag push or started by hand, and
can be started by hand itself. The wiki is
published per release on purpose, so it describes firmware that can be
downloaded.

Not covered by any job: hardware behaviour, `tools/sdtest`, the DShot and
KISS PIO programs beyond compilation, and the SDIO ones beyond the cycle-level
model.

<!-- coverage-table:start -->
| File | Lines | Covered | Coverage |
| --- | ---: | ---: | ---: |
| `src/am32_eeprom.cpp` | 69 | 67 | 97.1% |
| `src/am32_eeprom.h` | 11 | 11 | 100.0% |
| `src/blackbox_encode.cpp` | 113 | 113 | 100.0% |
| `src/core1_job.cpp` | 70 | 70 | 100.0% |
| `src/dshot/dshot_edt.c` | 33 | 33 | 100.0% |
| `src/dshot/dshot_frame.c` | 25 | 25 | 100.0% |
| `src/dshot/dshot_telem.c` | 143 | 143 | 100.0% |
| `src/esc_merge.cpp` | 44 | 44 | 100.0% |
| `src/esc_task.h` | 157 | 157 | 100.0% |
| `src/gfx.cpp` | 223 | 209 | 93.7% |
| `src/kiss_telem.cpp` | 37 | 35 | 94.6% |
| `src/log_ring.cpp` | 43 | 39 | 90.7% |
| `src/pin_claim.cpp` | 114 | 114 | 100.0% |
| `src/pin_claim.h` | 11 | 11 | 100.0% |
| `src/plat.h` | 5 | 5 | 100.0% |
| `src/rpm_filter.h` | 17 | 17 | 100.0% |
| `src/sd_log.h` | 4 | 4 | 100.0% |
| `src/sdio/sd_host.c` | 180 | 180 | 100.0% |
| `src/sdio/sd_proto.c` | 135 | 135 | 100.0% |
| `src/servo_job.cpp` | 199 | 199 | 100.0% |
| `src/servo_model.cpp` | 187 | 187 | 100.0% |
| `src/servo_pwm.cpp` | 55 | 55 | 100.0% |
| `src/settings.cpp` | 119 | 118 | 99.2% |
| `src/theme.cpp` | 11 | 11 | 100.0% |
| `src/ui.cpp` | 624 | 615 | 98.6% |
| `src/ui_am32.cpp` | 276 | 245 | 88.8% |
| `src/ui_hold.cpp` | 26 | 26 | 100.0% |
| `src/ui_input.cpp` | 25 | 25 | 100.0% |
| `src/ui_kst.cpp` | 4 | 4 | 100.0% |
| `src/ui_list.cpp` | 93 | 93 | 100.0% |
| `src/ui_servo.cpp` | 476 | 476 | 100.0% |
| `src/ui_setup.cpp` | 174 | 167 | 96.0% |
| `src/ui_slider.cpp` | 109 | 109 | 100.0% |
| `src/ui_widgets.cpp` | 43 | 43 | 100.0% |
| `src/usb_msc.h` | 37 | 35 | 94.6% |
| **Total** | **3892** | **3820** | **98.2%** |

Generated by `tools/coverage.py`. CI runs it with `--check` and fails on drift.
<!-- coverage-table:end -->
