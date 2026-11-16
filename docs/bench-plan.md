# Hardware bench plan

One ordered list of every hardware test that MiniRcBench's design notes and
[STATUS.md](../STATUS.md#open-items) name, for bringing the firmware up on
the Waveshare RP2350-Touch-LCD-2.8. Nothing in this plan has run: every test
below is open. The plan changes no design decision. A test passes only when
its result is written into STATUS.md and the note, as each test says.

## Purpose and use

The design notes in [docs/design/](design/README.md) each end with a bench
list or a bring-up step: "Bench", "Bench tests", "Bring-up" or the last row
of a work breakdown. STATUS.md's open items and the "Hardware" column of its
[verification table](../STATUS.md#what-exists-and-how-far-it-is-verified)
say what has not run on hardware. This plan gathers all of them, in sessions
that build on each other: a session starts only when the sessions it names
as prerequisites have passed.

Work through it like this:

1. Read [Safety](#safety) and the session's setup before each session.
2. Run the tests of a session in the order given. A test that fails stops
   the tests that name it as a prerequisite; record the failure and go on
   with the rest of the session.
3. Copy the [results template](#results-template) row of each test and fill
   it in on the day. Name each scope capture or photo after the test ID,
   for example `CS-B12-1.png`.
4. Carry each result into the place the test names under "Result to":
   the row of STATUS.md's verification table, the STATUS open item, and the
   note's section. A note's state changes only with a recorded result.

Acronyms: ESC is electronic speed controller, DShot the digital ESC protocol on
SIG, EDT Extended DShot Telemetry, eRPM electrical revolutions per minute, RPM
revolutions per minute, KISS the serial ESC telemetry protocol on AUX, FC flight
controller, S.BUS Futaba's serial receiver bus, MSP the MultiWii Serial
Protocol, BEC battery eliminator circuit, SD Secure Digital, SDIO Secure Digital
input/output, SDSC, SDHC and SDXC the standard, high and extended capacity SD
cards, DAT0 and CMD an SD card's data line 0 and command line, FAT File
Allocation Table, CSV comma-separated values, USB Universal Serial Bus, VBUS the
USB supply voltage, COM a serial port on Windows, UF2 USB Flashing Format,
BOOTSEL the RP2350's USB boot mode, SWD Serial Wire Debug, GPIO general-purpose
input/output, GP*n* GPIO number *n*, PIO programmable input/output, PWM
pulse-width modulation, I2C Inter-Integrated Circuit, SDA and SCL its data and
clock lines, UART universal asynchronous receiver-transmitter, IMU inertial
measurement unit, RTC real-time clock, SDK software development kit, EN a
converter's enable input, VIN and VOUT its input and output voltage, VCC a
module's supply pin, VS the INA219's supply pin, DC direct current, MS/s
megasamples per second, GND ground, LiPo lithium-polymer, HV high-voltage
(servo), EFM8 the Silicon Labs microcontroller of BLHeli_S and Bluejay ESCs, UI
user interface.

### Test IDs

Each test keeps the ID its note gives it, with a prefix for the note:
`CS-B12` is B12 of current-sense.md. Where a note lists its tests without
numbers, this plan numbers them in the note's order and says so
(`SO-1` is the first bullet of sbus-output.md's bench list). Tests that only
STATUS.md names carry `OI-` and the open item's number (`OI-23` is open
item 23), or `VT-` for a row of the verification table with no open item.
Open item 15 names five checks: `OI-15a`, `OI-15d` and `OI-15e` here; its
KISS frame on GP1 is `KS-2` and its servo channels on a scope `SV-3`, so the
letters b and c are not used.

| Prefix | Source | Section |
|---|---|---|
| `AM-` | [angle-meter.md](design/angle-meter.md#bench) | Bench, B1 to B9 |
| `BB-` | [blackbox-logging.md](design/blackbox-logging.md#bring-up) | Bring-up and its open question, numbered here |
| `BJ-` | [bluejay-blheli-s.md](design/bluejay-blheli-s.md#bench) | Bench, B1 to B13 |
| `BK-` | [STATUS.md](../STATUS.md#open-items) open item 13, for [backups.md](design/backups.md#work-breakdown) | numbered here |
| `CS-` | [current-sense.md](design/current-sense.md#bench-tests) | Bench tests, B1 to B13 |
| `DL-` | [dshot-lifetime.md](design/dshot-lifetime.md#d2-as-built) | slice D2 as built, numbered here |
| `DG-` | [diagnostics.md](design/diagnostics.md#not-tested-on-hardware) | Not tested on hardware, numbered here |
| `EC-` | [esc-commands.md](design/esc-commands.md#bench) | Bench, B1 to B9 |
| `HT-` | [hitec-programming.md](design/hitec-programming.md#work-breakdown) | Work breakdown step 6, numbered here 6a to 6f |
| `KS-` | [kiss-telemetry.md](design/kiss-telemetry.md#hardware-record) | Hardware record and open questions, numbered here |
| `KST-` | [kst-programming.md](design/kst-programming.md#work-breakdown) | Work breakdown step 7, numbered here 7a to 7d |
| `OI-` | [STATUS.md](../STATUS.md#open-items) | the open item's number |
| `PC-` | STATUS.md open item 8, for [pin-claim.md](design/pin-claim.md#hardware) | numbered here |
| `RS-` | [run-summary.md](design/run-summary.md#tests) | "Bench, after the slices", 1 to 5 |
| `RX-` | [rx-signals.md](design/rx-signals.md#bench) | Bench, the S.BUS bullet, split here into 1 to 8 |
| `SD-` | [sdio-driver.md](design/sdio-driver.md#bench-tests-needed) | Bench tests needed, numbered here |
| `SO-` | [sbus-output.md](design/sbus-output.md#bench) | Bench, numbered here 1 to 6 |
| `SV-` | STATUS.md open item 9, for [servo-tester.md](design/servo-tester.md#work-breakdown) step 5 | numbered here |
| `UB-` | [usb-bridge.md](design/usb-bridge.md#bench) | Bench, B1 to B11 |
| `VT-` | [STATUS.md](../STATUS.md#what-exists-and-how-far-it-is-verified) | a row of the verification table, numbered here |
| `WC-` | [sd-write-cache.md](design/sd-write-cache.md#work-breakdown) step 5 and STATUS.md open item 12 | numbered here |

Not in this plan: rx-signals.md's PPM (pulse-position modulation) bench
tests, since PPM is not built (that note's decision 1), and the restore of
bluejay-blheli-s.md's slice 6, which is not built.

### Firmware for the bench

- **Main image.** `MiniRcBench-touch-lcd-2.8.uf2` built from `main` at the
  commit the session runs. Record the commit in every result row. The
  v0.1.0-rc1 pre-release image (tag at a93e1c1) predates the tools of
  Sessions 3 to 9 and serves Sessions 1 and 2 only; it writes a settings
  block of version 5, which `OI-19` uses.
- **`tools/sdtest`.** The SD bring-up firmware, target `sdtest` of the same
  build, for `SD-5`.
- **A bring-up build for `CS-B6`**, which polls the INA219's bus register
  alone every 1 ms (current-sense.md, "Read-back from the bus voltage").
  It is not in the tree; `CS-B6` records the scope figure without it.
- **The USB bridge's traffic log**, the build option `USB_BRIDGE_LOG` in
  `src/config.h`, off by default, for the `UB-` tests that need it.

## Equipment

Specifications marked "derived" are worked out here from the figures the
notes give; no note states them.

| Instrument | Specification | Used by |
|---|---|---|
| Oscilloscope | 4 channels: SIG, AUX, EN and VOUT at once, or the two ends of the INA219's 0.1 Ω shunt for a current step. Bandwidth derived: the fastest edge measured is the I2C rise time against 300 ns (`CS-B1`); a scope rise time of a fifth of that, 60 ns, is about 0.35 / 60 ns = 6 MHz, so any scope of 20 MHz or more serves. Sample rate derived: at least 33 MS/s, ten samples over a 300 ns edge (`CS-B1`). Single-shot capture and a record of at least 1 s at 1 MS/s (a heartbeat trip at 250 ms, a 1000 ms stall). Ground clips on board GND only: a clip on a shunt end or on VOUT shorts the converter through the PC's earth. The shunt is read with two probes, as the difference of two channels, or with a differential probe | most sessions |
| Logic analyser | 3 channels or more, with UART, I2C and inverted-UART decoders. Rate derived: at 24 MS/s one sample is 42 ns, a fortieth of a DShot600 bit, 1.67 µs; 3 % of a 19200 baud bit, 52 µs, is 1.6 µs (`UB-B11`). Its ground on board GND only | Sessions 2, 3, 9 |
| Bench supply 1 | 0 V to 5 V DC with a current limit. In place of the converter's LiPo cell for the lowest and highest VIN, 3.0 V and 4.2 V (`CS-B6`); current derived: 5 V × 3 A / 3.0 V = 5 A into the converter at 3 A out, and 5.2 A at 3.1 A out, before its losses (`CS-B7`, `CS-B9a`); limit set below the cell's fuse. At 5 V into the INA219 for `CS-B4` and `CS-B5`. At most 5.0 V into the board through a USB-C power breakout for `DG-3`, never into J4 pin 2 (VBUS) | Sessions 1, 4, 7 |
| Bench supply 2, or the ESC's own pack | for the ESC; current limit set for a motor without propeller. No note sets the limit's value: record the value used. At most 3.3 V with a current limit as the separate source for the EN threshold (`CS-B6`) | Sessions 2 to 4, 8 to 10 |
| Multimeter | DC volts and ohms; the reference for the bus voltage at 5 V (`CS-B5`) and the BOARD voltage (`OI-22b`) | all |
| Reference ammeter | in series with the load, 0 A to 3.5 A (`CS-B4`), and a µA range for the current into SIG (`BJ-B13`). Its accuracy is not set in the note: record its type, range and burden | `CS-B4`, `CS-B8`, `CS-B9a`, `BJ-B13` |
| Resistive loads | the current is set by the load in series with the INA219's 0.1 Ω shunt, the leads and the ammeter's burden: at 5.0 V with the shunt alone in series, 1.67 Ω gives 2.82 A, 1.43 Ω 3.27 A and the note's 1.5 Ω 3.13 A. Set each current with the reference ammeter, trimming the load, or with an electronic load in constant-current mode: 0.1 A, 1 A, 2 A, 3 A and 3.5 A (`CS-B4`), and above the 3.0 A cut for `CS-B9a`, about 16 W at 3.1 A | `CS-B4`, `CS-B5`, `CS-B7`, `CS-B9a` |
| Small parts | 100 Ω to hold SDA low (`CS-B3`); 68 kΩ for the Hitec classic line (`HT-6a`); the pull-down for EN at the converter (Setup C); a 10 kΩ pull-down for a servo-only lead, only if `KST-7d` loses reply edges (kst-programming.md decision 3) | Sessions 4, 5 |
| Thermometer | a contact probe or an infrared thermometer, for the converter and the shunt after a 1000 ms stall; `AM-B7` uses the IMU's own temperature | `CS-B7`, `CS-B8` |
| SWD debug probe | to halt core0 for the heartbeat tests and for a debugger reset (`DG-4`). Whether the board brings SWD out is not recorded in this repository | `SV-5`, `CS-B11`, `SO-1`, `DG-4`, `DG-7` |
| PC | Windows, Linux and macOS for `UB-B1` and `RS-3`; Chrome for Web Serial; ESC-Configurator (v0.32.1 is the version usb-bridge.md read), the AM32 configurator (`am32-firmware/am32-configurator`), BLHeliSuite on Windows, Python 3 with `tools/set_clock.py`, picotool, and logwiju for `.BFL` logs | Sessions 1, 2, 9 |
| ESCs | an AM32 ESC (AM32 2.22 for `UB-B7`; rcbench ran 2.21), a Bluejay 0.21 ESC (EFM8BB2, signature E8B2), a BLHeli_S 16.7 ESC, an EFM8BB51 Bluejay ESC if at hand, a BLHeli_32 ESC for `EC-B4` (32.6 for `OI-1`), and spare ESCs of each EFM8 firmware for the tests that may leave an ESC without its tags | Sessions 2, 3, 8 to 10 |
| Motor | a brushless motor of known rated kV, for `EC-B8`, clamped to the bench; an optical tachometer if at hand | Sessions 2, 3 |
| Flight controller | Betaflight, with an S.BUS input; its own supply; propellers off | `SO-3` |
| Second MiniRcBench board | with the S.BUS monitor, to read the output back | `SO-2` |
| Receivers and transmitter | an S.BUS receiver of each make at hand and its transmitter with a channel monitor | Session 6 |
| INA219 board | the CJMCU-219 of `ina219_cjmcu219.step`, 0.1 Ω shunt, address 0x40, on a JST-SH 1.0 mm pigtail to J3. Not a Qwiic cable: J3's order is SCL, SDA, 3V3, GND from pin 1 | Sessions 4, 6, 7 |
| Boost converter | one LiPo cell, 3.0 V to 4.2 V, in; 5.0 V out, at least 3 A, synchronous with output disconnect, EN high at 3.3 V (current-sense.md decision 10). The owner's `boost_5v_en.step` board names no chip; its ratings come only from `CS-B6` and `CS-B7`. A fuse at the cell, which is charged off the bench | Sessions 4, 5, 7 |
| Servos | two servos for the tester; an MG90S (`CS-B10`); the owner's servos for `CS-B8`; the owner's KST servo, HV, which is to enter programming mode at 5 V (kst-programming.md decision 16); the owner's Hitec classic and D series servos; Hitec's programmer for `HT-6e` and `HT-6f` | Sessions 4, 5, 7 |
| SD cards | an SDSC card of 2 GB or less, an SDHC and an SDXC card (`SD-1`); a fast and a slow card (`RS-5`); a card formatted FAT32 and one exFAT (`BK-1`, `OI-11`); a fragmented card, with no free contiguous 16 MiB run, where a log grows cluster by cluster (blackbox-logging.md); a full card (`CARD FULL`); a card with 1000 old logs in its root (`OI-11`). Scratch cards only for `BB-1`, `SD-4`, `WC-2`, `WC-3` | Sessions 1, 2, 5, 10 |
| Angle references | wedges whose angles come from caliper readings, `atan(h / L)`, at 0°, 5°, 10°, 20° and 30°, and the caliper | `AM-B6` |
| Board power | the board's USB-C cable and a LiPo cell for its battery connector | all |

## Setups and wiring

Pins by role and number, as [docs/wiring.svg](wiring.svg) draws them. SIG
is GP0, J4 pin 9 or J5 pin 2; AUX is GP1, J4 pin 10 or J5 pin 1; GND is
J4 pin 1 or 5, or J5 pin 4; EN is GP28, J4 pin 11, or GP29, J4 pin 12; J3
pin 1 is SCL (GP7), pin 2 SDA (GP6), pin 3 3V3, pin 4 GND. These are the
pins of a board with blank flash; SETTINGS → SIG PIN swaps SIG and AUX.
J5 pin 3 and J4 pin 6 are 3.3 V out and are never connected, J4 pin 2 is
USB VBUS and feeds no servo or ESC, and J4 pins 7 and 8 are the shared I2C
bus: do not use.

| Setup | Wiring | Drawing |
|---|---|---|
| **A**, the board alone | USB-C to the PC; a LiPo cell on the battery connector where the test says | |
| **E**, an ESC on SIG | ESC signal to SIG, ESC ground to GND; the ESC lead's middle wire, BEC +5 V, cut or depinned; the ESC on its own pack or bench supply 2; telemetry to AUX where the test says (SETTINGS → KISS TELEM); motor without propeller, clamped | part A |
| **S**, a servo on its own supply | servo A signal to SIG, servo B to AUX for two servos, servo minus to the supply's minus with one ground tap to GND; the servo's middle wire to its supply, never to a connector pin; SERVO SUPPLY at `OWN` | part B |
| **C**, a servo on the switched supply | as S, with the boost converter's VOUT to the servo's plus through the INA219's screw terminal (VIN+ from VOUT, VIN- to the servo's plus), EN to GP28 (J4 pin 11) or GP29 (J4 pin 12) with SERVO SUPPLY at that pin, only after `CS-B6` has measured EN at or below 0.5 V with the board unpowered; the pull-down from EN to GND at the converter: 10 kΩ to 100 kΩ for an EN with no pull-up, and with a pull-up to VIN at most 0.135 times that pull-up at 4.2 V, so EN stays at or below 0.5 V with the board off (wiring.svg; kst-programming.md, "Hardware"); the INA219 on J3 by a pigtail, CURRENT SENSE at `INA219`; SIG, EN and GND on one J4 pigtail, J5 free. An ESC on this lead gets VOUT on its BEC wire: ESCs go on Setup E's lead only | part C |
| **R**, a receiver on AUX | receiver S.BUS output to GP1 (AUX), receiver ground to GND; the receiver on its own supply, never the board's | |
| **F**, an FC on SIG | GP0 to the FC's S.BUS input, GND to the FC's ground, nothing else | |
| **U**, a PC through the board | Setup E, and the board's USB-C to the PC running the desktop tool | |

## Safety

These hold in every session; each test adds its own.

- Propellers off, every motor clamped. Wear eye protection and stay out of
  the plane of rotation. Keep a way to cut the ESC's power that does not
  depend on the touchscreen (README.md, "Safety").
- One device on SIG at a time. Unplug any servo before ESC TESTER or ESC
  COMMANDS opens: DShot runs only there (dshot-lifetime.md, slice D2);
  elsewhere SIG is driven low.
- The ESC on its own pack, never on the board's 5 V; its BEC wire cut.
- Never connect J5 pin 3 or J4 pin 6 (3.3 V out) or J4 pin 2 (VBUS) to a
  device.
- SIG and AUX take 5.5 V while the board is powered and 3.63 V while it is
  off; GP28 and GP29 are not fault tolerant, carry only EN and drive it
  to 0 V or 3.3 V: no other source on EN while it is on J4. Power the
  board before a servo supply, a receiver, an FC or an ESC with a telemetry
  wire, and switch them off before the board.
- No level above 3.6 V on SIG from a KST servo (kst-programming.md
  decision 4); measure the Hitec classic line off the board first
  (`HT-6a`).
- **S.BUS output never to an ESC.** S.BUS OUTPUT on GP0 goes to an FC only.
  The only exceptions are `SO-4`, `SO-5` and `SO-6`, which test the
  bootloader probe against ESCs with no motor on them, the BLHeli_S one an
  expendable ESC. By sbus-output.md, a BLHeli_S ESC can detect S.BUS as
  Multishot, calibrate, arm and start its motor.
- A bench supply's current limit set before the load is connected, and
  recorded.
- Scope and analyser ground clips on board GND only, never on a shunt end
  or on VOUT.
- Destructive tests (Session 10, `UB-B8`, `UB-B10`) on spare parts only.

## Order

| Session | Content | Prerequisites |
|---|---|---|
| [1](#session-1-the-board-alone-and-the-sd-card) | Power-up, display and touch, splash, BOARD voltage, settings, diagnostics, the clock; then the SD card | none |
| [2](#session-2-pins-the-pin-claim-and-dshot) | The DShot lifetime, DShot on SIG, KISS on AUX, the pin claim, the touch-loss stop, logs with an ESC | 1 |
| [3](#session-3-esc-tools-on-dshot) | ESC COMMANDS, ESC SETTINGS on AM32, Bluejay and BLHeli_S, read and write | 2 |
| [4](#session-4-servo-tester-current-sensor-and-converter) | Servo tester, INA219, the boost converter, and the converter's protections: the hard cut, the stuck bus and the heartbeat | 1, 2 (`PC-`) |
| [5](#session-5-servo-programmers-and-backups) | KST and Hitec bring-up, the backups | 3 (for `BK-1`), 4 complete |
| [6](#session-6-receiver-sbus-output-on-a-scope-angle-meter) | S.BUS monitor, S.BUS output on a scope and a second board, the angle meter | 1, 2 (`KS-2`, for `RX-8`), 4 (`CS-B2`) |
| [7](#session-7-risky-supply-cuts-and-the-servo-log) | Risky: the stall cut on the owner's servos, the move timing, then the servo log `CS-B12` | 4 complete, 1 (`SD-`) |
| [8](#session-8-risky-the-sbus-bootloader-probe-then-an-fc) | Risky: the S.BUS bootloader probe against real ESCs, then frames to an FC | 3 (`BJ-B1`, `BJ-B2`), 6 (`SO-1`, `SO-2`) |
| [9](#session-9-risky-the-usb-bridge-with-desktop-tools) | Risky: the USB bridge with the desktop tools | 3 complete |
| [10](#session-10-risky-destructive-tests-on-spare-parts) | Risky: cut writes and power cuts on spare ESCs and scratch cards | 3, 9 |

The order follows STATUS.md's "Next steps": the pins of open item 15 and the
pin claim first, the SDIO driver with them, then the servo tester, KST and
Hitec. Sessions 6 to 10 cover what that list does not name.

## Session 1: the board alone and the SD card

Setup A. No ESC, servo or receiver on any pin. The board on USB to the PC,
and a LiPo cell on its battery connector where a test says.

### 1A. Power, screen, touch, diagnostics and the clock

#### OI-19. First boot over an older settings block

[STATUS.md](../STATUS.md#open-items) open items 15, 16 and 19.

- **Verifies:** a stored block of version 2, 3, 4 or 5 is carried over into
  version 6 at the first boot, field by field, and SETTINGS reads `UPDATED`.
  **Why:** the carry-over runs only in `settingsLoad()` on flash; the host
  suite runs it on a model.
- **Setup:** A. The v0.1.0-rc1 image, which writes a version 5 block; a
  DshotDisplay 2.x image, which writes version 2, as well if one is at hand.
  Run it first, before the main image saves any setting.
- **Steps:** flash the older image; change MAX THROTTLE, the DShot rate and
  the theme, and save; flash the main image; open SETTINGS.
- **Pass:** each changed value reads as set in the older firmware, and
  SETTINGS' MENU row reads `UPDATED` until the block is saved over (README.md,
  MENU table).
- **Record:** the older firmware and its block version; each value before
  and after; a photo of SETTINGS.
- **Result to:** STATUS verification table, row "Settings in flash"; open
  items 15, 16 and 19.
- **Safety:** nothing on the pins.

#### OI-22a. The splash's touch line

[STATUS.md](../STATUS.md#open-items) open item 22.

- **Verifies:** the CST328 touch controller answers its probe, so the
  splash does not show `TOUCH CONTROLLER NOT FOUND`. **Why:** the splash line
  and `touchAnswered()` run on a model only.
- **Setup:** A.
- **Steps:** power up ten times from off, by USB and by the cell; watch the
  splash each time.
- **Pass:** the line does not appear while the controller is fitted. With
  the controller missing or slow to start the line is to appear; that case
  is not reproduced here unless the flex cable can come off without harm.
- **Record:** the count of power-ups and of splash lines seen.
- **Result to:** verification table, row "The splash's `TOUCH CONTROLLER NOT
  FOUND`"; open item 22.
- **Safety:** none.

#### OI-22b. The BOARD voltage, its colour, and USB power alone

[STATUS.md](../STATUS.md#open-items) open items 21 and 22.

- **Verifies:** the BOARD reading against a multimeter on the cell, the
  colour bands, and what the divider on GP27 reads with USB power and no
  cell. **Why:** with USB alone the reading is not measured, and a red
  reading refuses every KST, Hitec and ESC write (open item 21).
- **Setup:** A; the multimeter on the cell's terminals.
- **Steps:** read BOARD with a cell, at the cell's charge on the day; then
  remove the cell and read BOARD on USB alone; if a cell near 3.3 V or
  3.5 V is at hand, read it there.
- **Pass:** the colour follows the reading: amber below 3.50 V, red from
  2.50 V up to below 3.30 V, back only 50 mV above each threshold; below
  2.50 V the text colour and no refusal (STATUS open item 21, README.md). The
  reading's error against the multimeter: criterion not set in the note;
  record it.
- **Record:** BOARD and the multimeter for each case; the colour; the USB
  alone reading.
- **Result to:** verification table, row "The splash's ... and the BOARD
  voltage's colour"; open items 21 and 22. Whether a write is refused on
  USB alone is recorded in `BJ-B6` and `KST-7d`.
- **Safety:** none.

#### OI-15e. The backlight

[STATUS.md](../STATUS.md#open-items) open item 15.

- **Verifies:** the backlight from pio0 at 20 kHz at BACKLIGHT 16 and 255.
  **Why:** the backlight moved from PWM slice 0 to a PIO state machine so
  the slice is free for the servos (pin-claim.md, "PWM slices").
- **Setup:** A; the scope on GP16 if a test point is reachable on the
  board, which is not recorded.
- **Steps:** set BACKLIGHT to 16, then 255.
- **Pass:** the panel dims at 16 and is full at 255; on the scope, 20 kHz.
- **Record:** the frequency and duty cycle at both values, or "not
  reachable".
- **Result to:** open item 15.
- **Safety:** none.

#### OI-4. Settings in flash

[STATUS.md](../STATUS.md#open-items) open item 4.

- **Verifies:** a saved setting survives a power cycle. **Why:** "Settings
  in flash" is not recorded on hardware (46a64a7 states no coverage).
- **Setup:** A.
- **Steps:** change MAX THROTTLE and SIG PIN, save; power off and on; read
  SETTINGS. Then HOLD DEFAULTS for 1 s, save, power-cycle.
- **Pass:** the saved values after the first power cycle, the compiled
  defaults after the second.
- **Record:** the values each time.
- **Result to:** verification table, row "Settings in flash"; open item 4.
- **Safety:** none. Set SIG PIN back to GP0 before Session 2.

#### OI-18. The screens on the panel

[STATUS.md](../STATUS.md#open-items) open item 18;
[menu-b.md](design/menu-b.md#slice-2-as-built), slice 2 as built.

- **Verifies:** menu structure B on the panel: MENU's rows and their
  scroll, SETTINGS' list, the list and hold widgets, the colour roles in
  both themes. **Why:** slices 2 to 9 run in the host suite only; the
  colours have not been looked at on the panel.
- **Setup:** A.
- **Steps:** open each MENU row and each SETTINGS section, normal and
  high-contrast theme; drag MENU to SETTINGS; hold HOLD DEFAULTS and slide
  off; compare with the previews in `docs/`.
- **Pass:** criterion not set in the note. In the high-contrast palette,
  open item 18 names the pairs that differ in brightness only: the KISS and
  EDT tags, the DShot rate with and without EDT, `SWIPE = THROTTLE` armed
  and disarmed, SETTINGS' captions against its labels.
- **Record:** a photo of each screen in each theme; which of those pairs
  can be told apart.
- **Result to:** verification table, the menu structure B rows, "List and
  hold widgets", "MENU layout", "SETTINGS layout" and "Touch handling,
  high-contrast theme, throttle gauge"; open item 18 and menu-b.md's
  slice 2 points.
- **Safety:** none.

#### OI-21. Two fingers on the glass

[STATUS.md](../STATUS.md#open-items) open item 21.

- **Verifies:** what the CST328 reports with two fingers on the glass.
  **Why:** whether the tester disarms on a second contact is open until it
  is measured; the driver reads finger 1 only.
- **Setup:** A.
- **Steps:** no tool in the tree prints the controller's report; the method
  is not set in STATUS.md. Record what the screen does with a second finger
  on the tester, disarmed, and on a slider.
- **Pass:** criterion not set in the note: a measurement for the owner's
  decision.
- **Record:** the behaviour seen, and the report if a debug print is used.
- **Result to:** open item 21.
- **Safety:** disarmed, no ESC on SIG.

#### VT-6. The serial telemetry dump

[STATUS.md](../STATUS.md#what-exists-and-how-far-it-is-verified), row
"Serial telemetry dump".

- **Verifies:** the dump on the USB serial port. **Why:** not recorded on
  hardware.
- **Setup:** A; a terminal on the board's serial port.
- **Steps:** open the port; watch for a minute.
- **Pass:** criterion not set; record what arrives.
- **Record:** a minute of the output.
- **Result to:** verification table, row "Serial telemetry dump".
- **Safety:** none.

#### DG-1 to DG-8. The restart reason

[diagnostics.md](design/diagnostics.md#not-tested-on-hardware), "Not
tested on hardware", numbered here in its order; STATUS open item 17.

- **Verifies:** SETTINGS → DIAGNOSTICS → LAST RESTART after each kind of
  restart. **Why:** nothing of `src/diag.*` has run on a chip; the reset
  registers' behaviour in three cases is not known.
- **Setup:** A; the SWD probe for DG-4 and DG-7; picotool for DG-6. For
  DG-3: the cell off and the PC's cable off; bench supply 1 at no more than
  5.0 V, its current limit set, into the board's USB-C through a USB-C power
  breakout, then lowered slowly until the chip resets. Never into J4 pin 2
  (VBUS). DG-7 takes a glitch or a debugger reset, so the SWD probe's
  reset serves; only DG-8 needs a glitch reset, and with no safe method to
  make one it is recorded as not run.
- **Steps and pass**, one restart each, reading LAST RESTART after it:

  | ID | Restart | Expected (the note) |
  |---|---|---|
  | DG-1 | power applied from off | `POWER-ON` |
  | DG-2 | the RUN pin pulled low (whether the board's RESET button drives RUN is not recorded) | `RUN PIN` |
  | DG-3 | a supply dip below the brown-out level (the level is not in the note) | `BROWN-OUT` |
  | DG-4 | a reset from the debugger | `DEBUGGER`; an Arm SYSRESETREQ may not reach the power manager, and what it shows is not known |
  | DG-5 | a UF2 copy in BOOTSEL mode | `UPDATE` |
  | DG-6 | `picotool reboot` | `REBOOT` |
  | DG-7 | a power-on, then a glitch or a debugger reset | not set: record whether CHIP_RESET holds one flag or two |
  | DG-8 | a UF2 copy, then a glitch reset | `GLITCH` if the glitch reset clears REASON, `UPDATE` or `REBOOT` if it does not |

- **Record:** LAST RESTART and UPTIME after each; how DG-3 and DG-7's
  glitch were made.
- **Result to:** verification table, row "Restart reason and uptime"; open
  item 17; diagnostics.md, "Not tested on hardware". The owner's fault-log
  questions wait on this session (diagnostics.md decision 1).
- **Safety:** DG-3: the bench supply's current limit set before it feeds
  the board.

#### RS-1. The RTC answers

[run-summary.md](design/run-summary.md#tests), "Bench, after the slices",
item 1; STATUS open item 26.

- **Verifies:** the PCF85063 at 0x51 answers, and its oscillator-stop flag
  (OS) is 1 after the first power-up. **Why:** the clock driver's read at
  start-up has not run on the part.
- **Setup:** A.
- **Steps:** power up from off; open SETTINGS → BOARD.
- **Pass:** `CLOCK` reads `NOT SET` with `SET AT EACH POWER-UP` under it, as
  a part with OS at 1 reads; `python3 tools/set_clock.py --query` answers
  `CLOCK NOT SET`.
- **Record:** the screen and the tool's answer.
- **Result to:** verification table, row "Clock driver and FAT dates"; open
  item 26; run-summary.md, slice 6.
- **Safety:** none.

#### RS-2. Setting the clock, and its drift

[run-summary.md](design/run-summary.md#tests), item 2.

- **Verifies:** the set over USB, the loss at power-off (no cell on J2),
  and the drift over 24 h. **Why:** `clockSet()` and the serial line
  through TinyUSB have not run.
- **Setup:** A; the PC with `tools/set_clock.py` on Windows, Linux or
  macOS (record which).
- **Steps:** `python3 tools/set_clock.py`; read `CLOCK` on SETTINGS → BOARD;
  power off and on; read it; set it again and leave the board on USB for
  24 h; `python3 tools/set_clock.py --query` against the PC's clock.
- **Pass:** `CLOCK SET` answered and the time shown to the minute; `NOT SET`
  after the power cycle. The drift over 24 h: criterion not set in the note;
  record it.
- **Record:** the tool's answers; the drift in seconds per 24 h.
- **Result to:** verification table, row "Setting the clock"; open item 26.
- **Safety:** none.

### 1B. The SD card

Prerequisite: 1A. Setup A with a card in the slot. Scratch cards for
`SD-4` and `BB-1`.

#### SD-1. Initialisation on each card type

[sdio-driver.md](design/sdio-driver.md#bench-tests-needed), first bullet;
STATUS open item 6.

- **Verifies:** `src/sdio/` brings up an SDSC card (2 GB or less), an SDHC
  and an SDXC card. **Why:** the driver has not run on hardware; only the
  previous driver mounted a 64 GB card (036adfe).
- **Setup:** A; each card in turn.
- **Steps:** insert; open SD LOG; START a log for 10 s; STOP.
- **Pass:** SD LOG reads `READY` and the log is written, for each card.
- **Record:** each card's make, size, type and file system; the state SD
  LOG shows.
- **Result to:** verification table, row "SDIO card driver"; open item 6.
- **Safety:** none.

#### SD-5. The SDIO clock

[sdio-driver.md](design/sdio-driver.md#bench-tests-needed), fifth bullet.

- **Verifies:** the card mounts in `tools/sdtest`, which tries 25 MHz,
  10 MHz, 4 MHz and 1 MHz in that order and stops at the first rate that
  mounts, and in the firmware at 10 MHz. **Why:** the firmware ships at
  10 MHz (`SD_LOG_SDIO_HZ`); a run at 10 MHz is not recorded. A card that
  mounts in `sdtest` and not in the firmware puts the fault in the
  integration (blackbox-logging.md, "Bring-up").
- **Setup:** A; the `sdtest` image, then the main image.
- **Steps:** run `sdtest` on each card of SD-1 and read its output on the
  USB serial port; then the main image with each card.
- **Pass:** `sdtest` mounts each card at some rate, recorded; 10 MHz is
  covered by the firmware's run, where each card mounts. A card that
  mounts only below 10 MHz in `sdtest` is recorded as such.
- **Record:** `sdtest`'s output per card.
- **Result to:** verification table, rows "SDIO card driver" and
  "`tools/sdtest`"; open item 6.
- **Safety:** none.

#### SD-2. Sustained logging

[sdio-driver.md](design/sdio-driver.md#bench-tests-needed), second bullet;
[blackbox-logging.md](design/blackbox-logging.md#bring-up); STATUS open
items 2 and 6.

- **Verifies:** logging at 7.3 kB/s, with BUF PEAK and WORST FLUSH, the
  figures to size `SD_LOG_BUFFER_BYTES` (8192 bytes, about 1.1 s at
  7.3 kB/s) from. **Why:** the ring's size is not measured.
- **Setup:** A; each card of SD-1, and the slow and the fragmented card.
- **Steps:** START on SD LOG; run 10 min; read BUF PEAK and WORST FLUSH
  during the run; STOP; read the log's `.TXT`. Record the rate, the log's
  size over its time; if it is below 7.3 kB/s with no ESC, repeat the test
  under Setup E in Session 2.
- **Pass:** criterion not set in the notes beyond DROPPED FRAMES: a stall
  longer than the ring holds drops frames, which DROPPED FRAMES counts.
  Record the figures.
- **Record:** BUF PEAK, WORST FLUSH, DROPPED FRAMES and the rate per card.
- **Result to:** open items 2 and 6; verification table, rows "Blackbox
  logging" and "Blackbox logs in `LOGS/`".
- **Safety:** none.

#### SD-3. USB DRIVE: a host reads the card

[sdio-driver.md](design/sdio-driver.md#bench-tests-needed), third bullet;
STATUS open item 4.

- **Verifies:** the read-only card reader with multi-block reads. **Why:**
  the USB card reader is not recorded on hardware.
- **Setup:** A, a card with logs from SD-2.
- **Steps:** USB to the PC; with no log recording and the tester
  disarmed, MENU → SD LOG → USB DRIVE; the drive appears on the PC; copy
  every log to the PC; compare sizes; open a `.BFL` in logwiju. `OI-20b` and
  `VT-2` start from this step.
- **Pass:** the files copy whole and the logs open; arming is refused while
  the host holds the card (README.md, "Safety").
- **Record:** the files and their sizes; the copy rate.
- **Result to:** verification table, row "USB card reader"; open items 4
  and 6.
- **Safety:** no ESC on SIG.

#### OI-20b. The USB cable pulled with the board on its cell

[STATUS.md](../STATUS.md#open-items) open item 20.

- **Verifies:** a pulled cable reaches the firmware as a bus suspend, so the
  card stays with the host until EJECT on SD LOG. **Why:** this is read
  from TinyUSB 0.18.0's source and not seen on the board (open item 20, and
  the verification table's row "USB card reader").
- **Setup:** A with the cell; the card on USB DRIVE, as in SD-3's steps.
- **Steps:** pull the cable; read SD LOG; tap EJECT.
- **Pass:** SD LOG reads `USB DRIVE` until EJECT and `READY` after it, as
  open item 20 says.
- **Record:** the state before and after EJECT.
- **Result to:** open item 20.
- **Safety:** none.

#### SD-4. The card pulled out

[sdio-driver.md](design/sdio-driver.md#bench-tests-needed), fourth bullet.

- **Verifies:** the driver's recovery from a card pulled while logging,
  while idle and while a host reads, and put back. **Why:** not run on
  hardware.
- **Setup:** A, a scratch card.
- **Steps:** each of the three cases; then put the card back; START a new
  log.
- **Pass:** criterion not set in the note; the new log is written after the
  card is back.
- **Record:** SD LOG's state at the pull and after; the card checked on the
  PC.
- **Result to:** open item 6.
- **Safety:** none.

#### OI-20a. The log numbering scan

[STATUS.md](../STATUS.md#open-items) open item 20.

- **Verifies:** the first log on a mount takes the highest `LOGnnnnn.BFL` in
  `LOGS/` plus 1, and how long the directory scan takes. **Why:** the scan's
  time on a card is not measured.
- **Setup:** A; a card with many logs in `LOGS/`, copied there on the PC.
- **Steps:** mount; START; read the new log's name; time START to `RECORDING`.
- **Pass:** the name is the highest plus 1. The scan time: criterion not
  set; record it.
- **Record:** the number of logs; the time.
- **Result to:** open item 20.
- **Safety:** none.

#### RS-3. File dates

[run-summary.md](design/run-summary.md#tests), item 3.

- **Verifies:** a log's `.BFL` and `.TXT` carry the set clock's date, and a
  file written before the set carries FAT date 0. **Why:** `get_fattime()`
  has not run on a card.
- **Setup:** A; the PC with `tools/set_clock.py`.
- **Steps:** power up; log 10 s; set the clock; log 10 s; read both logs'
  dates on Windows, Linux and macOS.
- **Pass:** the first log's files carry FAT date 0, the second's the set
  time.
- **Record:** the dates each system shows.
- **Result to:** verification table, row "Clock driver and FAT dates"; open
  item 26.
- **Safety:** none.

#### RS-5. The time a stop takes

[run-summary.md](design/run-summary.md#tests), item 5.

- **Verifies:** the time from STOP to the `.TXT` written, on a slow card.
  **Why:** the summary's write at the stop is not measured.
- **Setup:** A; the slow card and the fast one.
- **Steps:** log 60 s; STOP; time STOP until SD LOG shows the summary.
- **Pass:** criterion not set in the note; record the time.
- **Record:** the time per card.
- **Result to:** run-summary.md, slice 6; open item 26.
- **Safety:** none.

#### BB-1. A log cut by a power loss

[blackbox-logging.md](design/blackbox-logging.md#open-questions), "Open
questions", the power-loss question, numbered here.

- **Verifies:** what a log cut by a power loss looks like on the card.
  **Why:** the note works it out from the code: cut after its first sync
  (2000 ms) a log is 16 MiB long and holds what reached the card, lagging
  by up to 8192 bytes; cut in its first 2 s it reads 0 bytes. "Still open:
  a check on hardware".
- **Setup:** A on the cell alone; a freshly formatted scratch card, so the
  16 MiB run is free.
- **Steps:** START; cut the board's power after 1 s; power up; START; cut
  after 30 s; read both files on the PC and open them in each viewer.
- **Pass:** the sizes the note works out: 0 bytes, and 16 MiB. Whether each
  viewer stops cleanly at the end of the written data: criterion not set.
- **Record:** the sizes; each viewer's result; a file system check's
  result for the run left allocated.
- **Result to:** blackbox-logging.md, "Open questions".
- **Safety:** a scratch card only.

## Session 2: pins, the pin claim and DShot

Prerequisite: Session 1. Setup E with the AM32 ESC first; MAX THROTTLE at
its default, 20 %, unless a test says. The scope or the logic analyser on
SIG (GP0) and AUX (GP1).

#### DL-1 to DL-6. The DShot lifetime

[dshot-lifetime.md](design/dshot-lifetime.md#d2-as-built), slice D2,
numbered here. None of it has run on hardware.

| Test | Steps | Pass | Record |
|---|---|---|---|
| DL-1 | the analyser on SIG and AUX from before power-up; power the board; wait 10 s on MENU; open SERVO, RX SIGNALS, SD LOG and SETTINGS | no DShot frame; SIG low from shortly after reset, through the splash and every screen; AUX low on its pull-down | the time from power-up to SIG driven low; any edge |
| DL-2 | the AM32 ESC on SIG; tap ESC TESTER; hold HOLD TO ARM from 0.5 s on, then again from 1.6 s on | the 10 EDT enable frames first, then zero throttle; `DSHOT STARTING: WAIT 1.5 S` for 1.5 s; no arm from the first press, an arm 1 s into the second | the capture; the time from the tap to the first frame |
| DL-3 | armed, LATCH on at 10 %; tap MENU | zero-throttle frames from the tap until more than 250 ms after the last throttle frame, then SIG driven low, no frame | the capture; what the ESC does (signal-loss behaviour, not recorded) |
| DL-4 | from ESC CONFIG, ESC COMMANDS, then BACK; then USB BRIDGE and its leave page | ESC COMMANDS binds with the enable first; BACK parks at once (no drain, no throttle was sent); USB BRIDGE puts SIG on its pull-up with no frame; the leave page's ESC CONFIG drives SIG low again, no frame | the captures |
| DL-5 | a Bluejay ESC, powered, on the parked SIG for 60 s; then ESC TESTER | the ESC does not enter its bootloader on the low line (from source, not measured); it answers eRPM once the tester opens | whether the ESC answers, and after how long |
| DL-6 | a: armed, LATCH on at 10 %; tap MENU, then the ESC TESTER row within 250 ms; hold HOLD TO ARM at once. b: on ESC TESTER, tap MAX, change SIG PIN to GP1 and back to GP0 in SETUP, then BACK within 1.5 s of the last change; hold HOLD TO ARM at once, then again 1.5 s after the last change | a: zero-throttle frames from the MENU tap on, no gap and no new EDT enable on the reopen; an arm 1 s into the hold. b: the frames stop on GP0 and start on GP1 with the EDT enable first, then again on GP0; `DSHOT STARTING: WAIT 1.5 S` on the track after BACK; no arm from the first press, an arm 1 s into the second | the captures of SIG and AUX; the time from the reopen tap to the arm |

#### OI-15a. An ESC on SIG at each DShot rate

[STATUS.md](../STATUS.md#open-items) open item 15; verification table
rows "DShot driver in `src/dshot/`" (`VT-1`) and "DShot bitrate label"
(`VT-4`).

- **Verifies:** `src/dshot/` on GP0 at DShot150, 300, 600 and 1200, and the
  status bar's `DSHOTnnn` label. **Why:** the driver has not run on hardware
  in MiniRcBench; rcbench ran it at DShot600 on one AM32 2.21 ESC only.
- **Setup:** E, the AM32 ESC, then each other ESC firmware at hand.
- **Steps:** set SETTINGS → DSHOT to each rate; read the tester's LINK and
  the top row; arm and run at 10 % for 10 s; disarm.
- **Pass:** LINK above 0/S at each rate (open item 15); the label reads the
  rate set, dim, then in the text colour once an EDT enable has been sent
  (README.md). `DSERR` is not provoked here: criterion not set.
- **Record:** LINK and the eRPM at 10 % per rate and per ESC; a capture of
  one frame per rate.
- **Result to:** verification table, rows "DShot driver" and "DShot bitrate
  label"; open item 15.
- **Safety:** propeller off, motor clamped.

#### VT-3. The EDT enable at the pump's start

[STATUS.md](../STATUS.md#what-exists-and-how-far-it-is-verified), row "EDT
telemetry, automatic enable, expiry"; open item 1's facts.

- **Verifies:** the first 10 frames after the DShot pump starts are DShot
  command 13, the EDT enable, and each EDT reply decodes. **Why:** the
  enable at pump start and the decode of every reply with EDT on are not
  tested on hardware.
- **Setup:** E, the AM32 ESC with EDT; the logic analyser on SIG.
- **Steps:** capture from the tap on MENU's ESC TESTER row; then run at
  10 %.
- **Pass:** 10 frames of command 13 first; after them, the enable only while
  the ESC answers eRPM (open item 1); the EDT tags on the tiles.
- **Record:** the capture; the tiles' tags and values.
- **Result to:** verification table, row "EDT telemetry".
- **Safety:** propeller off.

#### VT-2. HOLD TO ARM counts only a press that lands while arming is allowed

[STATUS.md](../STATUS.md#what-exists-and-how-far-it-is-verified), row
"Bidirectional DShot throttle"; README.md, "Safety".

- **Verifies:** arming needs a 1 s hold with the throttle at zero for at
  least 250 ms; the press that disarms, or one made while a USB host holds
  the card, does not arm however long it stays. **Why:** the HOLD TO ARM
  press rule is not tested on hardware.
- **Setup:** E; then the card on USB DRIVE, as in SD-3's steps.
- **Steps:** arm; tap DISARM and keep the finger down for 3 s; with the host
  holding the card, hold HOLD TO ARM for 3 s; eject; hold it for 1 s. Then
  MENU, the ESC TESTER row, and HOLD TO ARM pressed at once and held 4 s.
- **Pass:** no arm in the first two cases; an arm in the third; in the
  fourth no arm (the press landed while the arm gate held,
  `DSHOT STARTING: WAIT 1.5 S` in the track), and a new press after 1.5 s
  arms 1 s later.
- **Record:** each outcome.
- **Result to:** verification table, row "Bidirectional DShot throttle".
- **Safety:** propeller off.

#### KS-1. The KISS line voltage

[kiss-telemetry.md](design/kiss-telemetry.md#hardware-record), "Hardware
record"; STATUS open item 3.

- **Verifies:** the level an ESC drives on its telemetry wire. **Why:** the
  KISS specification puts the line at 3.6 V; GP1 takes 5.5 V while the board
  is powered and 3.63 V while it is off; the level is not measured.
- **Setup:** E with telemetry to AUX, KISS on; the scope on AUX.
- **Steps:** capture a frame's high level and the idle level with the
  board on. Only if both measure at or below 3.63 V, capture them again
  with the board off and the ESC on.
- **Pass:** the high level at most 5.5 V with the board on and at most
  3.63 V with it off (the pin's rating). Above 3.63 V with the board off,
  the rule "power the board before the ESC" is the protection.
- **Record:** both levels per ESC.
- **Result to:** open item 3; kiss-telemetry.md, "Hardware record".
- **Safety:** the board powered before the ESC; the board-off capture
  only if both board-on levels measure at or below 3.63 V.

#### KS-2. A KISS frame on GP1 on the telemetry bit alone

[kiss-telemetry.md](design/kiss-telemetry.md#open-questions), first open
question; STATUS open item 15 ("a KISS frame on GP1").

- **Verifies:** a KISS frame on AUX, requested by the telemetry bit in every
  20th frame, without `DSHOT_CMD_SIGNAL_LINE_TELEMETRY_ENABLE`, which the
  firmware does not send. **Why:** whether the bit is enough on AM32 is
  unconfirmed; KISS on GP1 of this board is not recorded.
- **Setup:** E with telemetry to AUX, KISS on.
- **Steps:** run at 10 %; read the tiles' KISS tags; capture AUX.
- **Pass:** the tiles show `KISS` tags with values; a frame on AUX after a
  request.
- **Record:** the capture; the tags per ESC firmware.
- **Result to:** open item 15; kiss-telemetry.md, "Open questions".
- **Safety:** propeller off.

#### PC-1 to PC-4. The pin claim's drain and teardown

[STATUS.md](../STATUS.md#open-items) open item 8, numbered here in its
order; [pin-claim.md](design/pin-claim.md#hardware).

- **Verifies:** the PIO and pad calls of the drain, the teardown and the
  rebuild, which only the firmware runs. **Why:** every tool after this
  session takes SIG through the claim.
- **Setup:** E, the AM32 ESC; the logic analyser on SIG.
- **Steps and pass:**

  | ID | Step | Pass (open item 8) |
  |---|---|---|
  | PC-1 | run at 10 %, disarm, open ESC SETTINGS (MENU → ESC CONFIG) within 250 ms of the last throttle frame | zero frames for at least 250 ms after the last throttle frame, then no edges |
  | PC-2 | the line after the teardown | idle high: the teardown stops, releases and puts the pull-up back |
  | PC-3 | a SETTINGS pin change in the same window | criterion not set apart from PC-1's; record whether the drain holds |
  | PC-4 | leave the AM32 session with BACK | DShot back, the EDT enable as its first frames |

- **Record:** the captures; the time from the last throttle frame to the
  claim, since above 250 ms no drain is needed and PC-1 does not exercise
  it (pin-claim.md, "Testing"); what the ESC does after the screen is left,
  which is not recorded anywhere (no RUN command is sent).
- **Result to:** verification table, row "Pin claim"; open item 8.
- **Safety:** propeller off.

#### OI-23. The touch-loss stop

[STATUS.md](../STATUS.md#open-items) open item 23.

- **Verifies:** the CST328 answers reads with no finger on the glass for
  minutes. **Why:** the firmware disarms the tester, stops SERVO TESTER and
  stops S.BUS OUTPUT's frames when the controller does not answer for
  4 reads in a row and 100 ms (`uiTouchLost()`); if the controller dozes,
  it disarms every latched motor shortly after the finger lifts.
- **Setup:** E; the main image.
- **Steps:** power up and leave the tester untouched for 5 min; then,
  propeller off, arm with LATCH on at a low throttle and lift the finger.
- **Pass:** `TOUCH NOT ANSWERING` does not appear in the 5 min; the motor
  runs until DISARM or the 30 s idle disarm.
- **Record:** the time watched; when the motor stopped and why.
- **Result to:** open item 23.
- **Safety:** propeller off, motor clamped; a hand on the ESC's power cut.

#### RS-4. The run summary against the log

[run-summary.md](design/run-summary.md#tests), item 4.

- **Verifies:** the summary in `LOGS/LOGnnnnn.TXT` and on SD LOG against the
  log. **Why:** slices 1 to 3 run in the host suite only.
- **Setup:** E; one run with EDT only, one with KISS on AUX; a card.
- **Steps:** arm (the log starts with the arm); run 30 % for 20 s; disarm;
  read SD LOG; open the `.BFL` in logwiju.
- **Pass:** criterion not set in the note: compare the maxima and minimum
  with logwiju, and the current integral with the KISS counter's rise.
- **Record:** each figure from both sides.
- **Result to:** verification table, rows "Run summary" (slices 1 to 3);
  open item 26.
- **Safety:** MAX THROTTLE raised to 30 % for this test; propeller off.

#### OI-1. Issue #1, optional

[STATUS.md](../STATUS.md#open-items) open item 1.

- **Verifies:** whether a BLHeli_32 32.6 ESC initialises on the board.
  **Why:** an external report, not reproduced: no zero-throttle beep, no
  initialisation on RAZOR32 and EXUAV 70A ESCs.
- **Setup:** E, a BLHeli_32 32.6 ESC, only if one is at hand.
- **Steps:** power the ESC; listen; then the owner's request to the
  reporter: arm with the throttle at zero, then plug the ESC in while
  armed, the throttle still at zero.
- **Pass:** criterion not set: record the start-up sounds and LINK.
- **Record:** the ESC, its firmware, the sounds, LINK at each rate.
- **Result to:** open item 1; the issue.
- **Safety:** propeller off.

## Session 3: ESC tools on DShot

Prerequisite: Session 2, `PC-1` to `PC-4` in particular. Setup E, each ESC
firmware at hand in turn; a card in the slot; the logic analyser on SIG.
For `BJ-`, the ESC's pack stays reachable: decision 8 of bluejay-blheli-s.md
asks for it to come off before the leave page's last button after an EFM8
answer.

### 3A. ESC COMMANDS and the kV estimate

[esc-commands.md](design/esc-commands.md#bench), "Bench"; STATUS open item
28. "Only an ESC settles these, one per firmware at hand." MENU's first row,
ESC COMMANDS.

#### EC-B1. Direction survives a power cycle

- **Verifies:** REVERSED, saved with command 12, holds after the ESC's power
  cycle. **Why:** a change that reverts at the next power cycle is what the
  save exists to prevent (decision 2).
- **Setup:** E, 3D off; tape on the bell.
- **Steps:** REVERSED with its 1 s hold; 5 % on the tester; power-cycle the
  ESC; 5 % again; NORMAL back.
- **Pass:** the bell turns reversed before and after the power cycle, and
  forward after NORMAL.
- **Record:** per firmware: the direction each time.
- **Result to:** verification table, row "ESC COMMANDS"; esc-commands.md
  slice 5.
- **Safety:** propeller off; MAX THROTTLE at least 5 %.

#### EC-B2. AM32's save tone, and the settings as sent

- **Verifies:** the save's tone against BEEP 1 and BEEP 2; ESC SETTINGS
  reading the MOTOR group's `DIRECTION` and `3D MODE` as
  sent. **Why:** the page's commands are read from source only.
- **Setup:** E, the AM32 ESC.
- **Steps:** BEEP 1, BEEP 2, then a direction change; open ESC SETTINGS.
- **Pass:** `DIRECTION` and `3D MODE` read as sent. The tone: criterion not
  set; record it.
- **Record:** the tones heard; the two fields.
- **Result to:** as EC-B1.
- **Safety:** propeller off.

#### EC-B3. 3D ON and the tester's note

- **Verifies:** the note `3D MODE ESC: 0-50% REVERSED AT 2X` and the motor in
  3D. **Why:** decision 9's text and decision 4's rule, not on hardware
  (STATUS open item 24).
- **Setup:** E.
- **Steps:** 3D ON; read the tester; 10 %; above 50 %, from low; 3D OFF;
  power-cycle the ESC; restart the board.
- **Pass:** the note on the tester after 3D ON; at 10 % the motor runs as at
  about 20 % and reversed; above 50 % in the set direction; the note gone
  after the board's restart (decision 4).
- **Record:** the note each time; the motor's direction and speed.
- **Result to:** as EC-B1; open item 24.
- **Safety:** propeller off; MAX THROTTLE raised to 60 % for this test, a
  value this plan sets, not the note.

#### EC-B4. BEEP 1 to 5 on each firmware

- **Verifies:** each beacon; whether BLHeli_32 takes a command within its
  beep lengths; on Bluejay, how long the answers stop (it turns interrupts
  off for the tone and 200 ms after it, `DShot.asm` 90 to 94) and whether
  the readiness falls to `NoAnswer` and `Settling`.
- **Setup:** E, each firmware.
- **Steps:** BEEP 1 to 5 in turn, then two in quick succession.
- **Pass:** criterion not set in the note; record each.
- **Record:** per firmware: each beacon heard; the gap in eRPM on Bluejay.
- **Result to:** as EC-B1.
- **Safety:** propeller off.

#### EC-B5. The sequence on the wire

- **Verifies:** a direction change as frames. **Why:** the slot's sequence
  is host-tested only.
- **Setup:** E; the logic analyser on SIG with a DShot decoder.
- **Steps:** capture a direction change.
- **Pass:** 10 frames of 8, 300 ms of 0, 10 of 12; the EDT enable never
  broken by a sequence.
- **Record:** the capture.
- **Result to:** as EC-B1; verification table, row "ESC command slot".
- **Safety:** propeller off.

#### EC-B6. An ESC plugged in with the page open

- **Verifies:** the first command waits `ESC_CMD_SETTLE_MS`. **Why:** the
  settle rule protects an ESC that is still starting.
- **Setup:** E, the AM32 ESC unpowered, ESC COMMANDS open.
- **Steps:** power the ESC; request BEEP 1 at once; time it on the analyser.
- **Pass:** the first command waits 1500 ms and AM32 acts on it.
- **Record:** the time from the first eRPM answer to the command.
- **Result to:** as EC-B1.
- **Safety:** propeller off.

#### EC-B7. BLHeli_S 16.7 on inverted DShot

- **Verifies:** whether a BLHeli_S 16.7 ESC answers or starts at all on
  inverted DShot. **Why:** BLHeli_S sends no eRPM; the board sends only
  bidirectional DShot.
- **Setup:** E, a BLHeli_S 16.7 ESC, if one is at hand.
- **Steps:** power; read LINK; arm at 5 %.
- **Pass:** criterion not set; record.
- **Record:** start-up sounds; LINK; whether the motor turns.
- **Result to:** as EC-B1; bluejay-blheli-s.md cites this as B7 of
  esc-commands.md.
- **Safety:** propeller off.

#### EC-B8. kV and RPM/V

- **Verifies:** `KV EST` and `RPM/V` against the motor's rating and the
  factor table. **Why:** no figure has been compared with a rated kV or a
  tachometer (STATUS verification table, "RPM/V and the kV estimate").
- **Setup:** E; a motor of known rated kV; EDT, then KISS voltage on AUX;
  AM32 with SINE STARTUP on and off; an optical tachometer if at hand.
- **Steps:** 30 %, 50 %, 75 % and 100 %; at 75 % hold 1 s for `KV EST`.
- **Pass:** criterion not set in the note; record the figures.
- **Record:** per step: `RPM/V`, `KV EST`, the rated kV, the tachometer.
- **Result to:** verification table, row "RPM/V and the kV estimate".
- **Safety:** propeller off, motor clamped; MAX THROTTLE raised to 100 %
  (2000) for this test only, and set back after.

### 3B. ESC SETTINGS on AM32

#### VT-5. The AM32 editor on the pin claim, with its backup

[STATUS.md](../STATUS.md#what-exists-and-how-far-it-is-verified), row
"AM32 settings editor"; [backups.md](design/backups.md#work-breakdown)
step 3.

- **Verifies:** a read, an edit with the swipe of 16 px per coarse step, the
  backup on the card before the first WRITE, and the write. **Why:** the
  editor has hardware use from before the claim (ab79c67, bd90e17); the
  claim, its drain, the swipe and the backup are not tested on hardware.
- **Setup:** E, the AM32 ESC; a card.
- **Steps:** MENU → ESC CONFIG → ESC SETTINGS; swipe a value 16 px; WRITE...
  and its 1 s hold; read again; BACK.
- **Pass:** one coarse step per 16 px; a backup file in `BACKUP/` before the
  write; the value read back as written.
- **Record:** the value before and after; the backup's name.
- **Result to:** verification table, rows "AM32 settings editor" and
  "Device backups".
- **Safety:** propeller off.

#### OI-24c. An ESC set to EDT ARM

[STATUS.md](../STATUS.md#open-items) open item 24, its last paragraph.

- **Verifies:** an AM32 ESC with PROTOCOL at EDT ARM on the tester. **Why:**
  STATUS records that such an ESC does not run on the tester, which sends
  zero frames after its EDT enable; sending the enable in place of zero
  frames while armed at zero throttle is proposed, not built, and needs a
  check on hardware.
- **Setup:** E, the AM32 ESC; PROTOCOL set to EDT ARM on ESC SETTINGS, the
  backup of VT-5 on the card.
- **Steps:** arm; 5 % on the tester; disarm; PROTOCOL back as it was.
- **Pass:** criterion not set; record whether the motor runs.
- **Record:** LINK, the tiles and the motor.
- **Result to:** open item 24.
- **Safety:** propeller off.

#### OI-24b. An AM32 layout the table does not know, optional

[STATUS.md](../STATUS.md#open-items) open item 24.

- **Verifies:** a layout other than 2 to 4 opens read-only, the bytes in hex,
  `NOT VERIFIED.`, with a backup file. Only with such an ESC at hand.
- **Pass:** as open item 24 states; no way to the write.
- **Record:** the layout number; a photo.
- **Result to:** open item 24.
- **Safety:** propeller off.

#### WC-1. The card's busy, polled once per loop pass

[sd-write-cache.md](design/sd-write-cache.md#work-breakdown) step 5; STATUS
open item 12.

- **Verifies:** while the AM32 claim holds the card's write cache, core0
  looks at the card's busy once per loop pass. **Why:** the glue and
  `src/card_cache.cpp` are built by the firmware only.
- **Setup:** E, a card; the scope on the card's DAT0 and CMD lines (GP21
  and GP20) at the microSD socket, where the probe points are not
  recorded.
- **Steps:** a write on ESC SETTINGS, capturing the backup's flush.
- **Pass:** criterion not set beyond "once per loop pass". The scope does
  not show a look at DAT0, and the note sets no method; record how long
  DAT0 shows busy and the commands before and after it.
- **Record:** the capture.
- **Result to:** verification table, row "SD write cache"; open item 12.
- **Safety:** propeller off.

#### WC-4. A card swapped between two writes

[sd-write-cache.md](design/sd-write-cache.md#card-swap), "Card swap";
STATUS open item 12.

- **Verifies:** the serial number read before each write catches a card
  swapped between two writes of a session. **Why:** a block from the queue
  would otherwise land on the new card and corrupt its volume.
- **Setup:** E; two cards with different volume serial numbers.
- **Steps:** a write; swap the card; a second write.
- **Pass:** the swap is caught: the card mounted again and the backup saved
  to it before the second write (sd-write-cache.md, Tests); the new card's
  volume checks clean on the PC.
- **Record:** the screens; the check's result.
- **Result to:** as WC-1.
- **Safety:** propeller off.

### 3C. ESC SETTINGS on Bluejay and BLHeli_S

[bluejay-blheli-s.md](design/bluejay-blheli-s.md#bench), "Bench"; STATUS
open item 30. "Only an ESC settles these." `BJ-B8`, `BJ-B9`, `BJ-B10` and
`BJ-B12` are in Session 10.

#### BJ-B1. Bluejay 0.21 answers the greeting

- **Verifies:** the connect cycle's entry: quiet, then greetings. **Why:** no
  Bluejay or BLHeli_S ESC has answered the greeting on this board.
- **Setup:** E, a Bluejay 0.21 ESC; the analyser on SIG.
- **Steps:** run DShot; open ESC SETTINGS.
- **Pass:** it answers in the first quiet second, `471d E8 B2 06 01 30`.
- **Record:** the time from the pump's stop to the answer.
- **Result to:** verification table, row "Bluejay and BLHeli_S, slice 2";
  bluejay-blheli-s.md slice 5.
- **Safety:** propeller off; the ESC's pack off before the leave page's
  last button (decision 8).

#### BJ-B2. The same with BLHeli_S 16.7

- **Verifies and why:** as BJ-B1; BLHeli_S may not have taken the inverted
  frames before.
- **Pass:** criterion not set apart from BJ-B1's answer; record.
- **Record, result to, safety:** as BJ-B1.

#### BJ-B3. Power-cycled during a greet phase

- **Verifies:** the time to the answer when the ESC powers up during a greet
  phase; an AM32 ESC under the same cycle: how many power-ups it takes,
  whether it answers without a second power cycle, and after how long.
- **Setup:** E, each ESC.
- **Pass:** criterion not set; record.
- **Record:** the times and counts.
- **Result to:** as BJ-B1.
- **Safety:** as BJ-B1.

#### BJ-B4. escwire's first contact after the AM32 greeting's answer

- **Verifies:** escwire's own first contact after an answer to the AM32
  greeting. **Why:** the test on the simulated ESC leaves the `00 00`
  restart open; this settles it (bluejay-blheli-s.md, Tests).
- **Pass:** criterion not set; record whether the contact succeeds.
- **Setup, record, result to, safety:** as BJ-B1.

#### BJ-B5. The EFM8's open-drain answer on a scope

- **Verifies:** the answer's rising edges on the RP2350's pull-up and the
  EFM8's weak pull-up (`BLHeliBootLoad.inc` 76, 79).
- **Setup:** E; the scope on SIG.
- **Pass:** criterion not set beyond the bit time, 52 µs a bit; record the
  rise time.
- **Record:** the capture.
- **Result to, safety:** as BJ-B1.

#### BJ-B6. Read, edit, write and read-back

- **Verifies:** the whole write on Bluejay 0.21 and BLHeli_S 16.7, and on an
  EFM8BB51 Bluejay if one is at hand. **Why:** no write has reached an ESC.
- **Setup:** E; a card for the backup (`BJnnn.TXT`, `BHnnn.TXT`).
- **Steps:** read; set DIRECTION reversed; WRITE... and its hold; tape on
  the bell, 5 % on the tester; LEAVE. Repeat once on USB power alone with
  no cell, to record whether the red BOARD voltage refuses the write
  (`OI-22b`).
- **Pass:** the read-back verified; the bell reversed; after LEAVE the
  melody and, on Bluejay, eRPM again without a power cycle.
- **Record:** the backup's name; the result line; the melody; eRPM; the USB
  alone outcome.
- **Result to:** verification table, rows "Bluejay and BLHeli_S, slice 3"
  and "slice 4"; open items 21 and 22 for the USB alone outcome.
- **Safety:** propeller off; DIRECTION back after the test.

#### BJ-B7. The time of a write

- **Verifies:** a write's time, and the stretch from erase to program, the
  window in which a power cut loses the tags.
- **Setup:** as BJ-B6; the analyser on SIG.
- **Pass:** criterion not set; record.
- **Record:** both times per firmware.
- **Result to:** verification table, row "Bluejay and BLHeli_S, slice 4".
- **Safety:** as BJ-B6.

#### BJ-B11. LEAVE and the ESC's start-up check

- **Verifies:** after LEAVE, the time to the pump's first frame against the
  ESC's start-up and its 15 ms or 150 ms check. **Why:** LEAVE has no answer,
  and a line idle high for 150 ms after the melody takes Bluejay back to its
  bootloader.
- **Setup:** as BJ-B6.
- **Pass:** criterion not set; record whether the ESC starts its firmware.
- **Record:** the time; the ESC's state.
- **Result to:** as BJ-B7.
- **Safety:** as BJ-B6.

#### BJ-B13. SIG with the ESC's battery unplugged

- **Verifies:** the current into SIG and the ESC's 3.3 V rail while the
  quiet phase runs with the ESC unpowered; whether a power cycle restarts
  the microcontroller. **Why:** the current into SIG is not measured
  either way.
- **Setup:** E, the ESC's pack unplugged; the ammeter in series with SIG;
  the multimeter on the ESC's 3.3 V rail.
- **Pass:** criterion not set; record.
- **Record:** the current, the rail's voltage, the restart.
- **Result to:** bluejay-blheli-s.md, "Not verified".
- **Safety:** propeller off.

## Session 4: servo tester, current sensor and converter

Prerequisites: Session 1, and `PC-1` to `PC-4` of Session 2 (the servo
tester takes SIG and AUX through the claim). No ESC on any pin.

### 4A. The servo tester on its own supply

[STATUS.md](../STATUS.md#open-items) open item 9, numbered here in its
order; [servo-tester.md](design/servo-tester.md#work-breakdown) step 5.
Setup S; SERVO SUPPLY at `OWN`; the scope on SIG (GP0) and AUX (GP1).

- **Verifies:** the PWM driver and the core1 job on the chip. **Why:**
  nothing of the servo tester has run on a chip; the host fake models the
  counter, not the SDK's calls.
- **Result to:** verification table, row "Servo tester"; open item 9; open
  item 15 for SV-3.
- **Safety:** the servo's middle wire to its own supply; power the board
  first; the servo supply at 4.8 V to 6.0 V, 8.4 V for an HV servo
  (pin-claim.md, "Wiring").

| ID | Step | Pass (open item 9) | Record |
|---|---|---|---|
| SV-1 | open SERVO TESTER, no START | SAFE after the claim: both lines low, no edges | the capture |
| SV-2 | START at 50 Hz, CENTRE 1500 µs; then the 560 Hz profile, HELI TAIL 760, with no servo on the pins or with a 760 µs tail servo only | 1500 µs reads 1500 µs on the scope; the frame from the 1 MHz counter's wrap, 19,999 at 50 Hz and 1,784 at 560 Hz, so 20,000 µs and 1,785 µs | pulse width and period at each rate |
| SV-3 | one servo, then SET → SERVOS TWO while the first runs | the second channel starts without a step on the first | the capture of both channels |
| SV-4 | STOP while running at 50 Hz | the last pulse ends within one frame, 20 ms at 50 Hz | the time from the press to the last edge |
| SV-5 | halt core0 in the debugger while running | the pulses stop 250 ms after the last UI frame | the time |
| SV-6 | BACK while RUNNING; a profile change right after STOP | the last pulse complete, not cut short; the pins leave PWM about 41 ms after BACK; the rebind waits until about 41 ms after STOP, at 50 Hz | both captures |
| SV-7 | the line after the release, through the leave page; unplug the servos (the leave page's step 1), then SERVO, the last button: ESC TESTER's DShot reads to a servo as a pulse of about 970 µs at 1 kHz (README.md) | an input with the pull-down on: low; driven low after SERVO, no DShot frame | the capture |
| SV-8 | STOP on SET while RUNNING | level 0 within one frame, as on SERVO | the time |
| SV-9 | a real servo on each profile, each profile only on a servo rated for it: a rate above 60 Hz overheats an analogue servo (README.md) | criterion not set: record whether it reads 3.3 V pulses and moves | per servo and profile |

#### OI-24a. A profile taken past its travel

[STATUS.md](../STATUS.md#open-items) open item 24.

- **Verifies:** HOLD TO APPLY before a TYPE, MIN or MAX step that takes the
  profile past its travel. **Why:** "Not checked on a servo".
- **Setup:** S.
- **Steps:** on SET, step MAX past the travel; then SERVOS ONE to TWO on
  such a profile.
- **Pass:** each waits for HOLD TO APPLY (1 s), as open item 24 states.
  How far the servo moves: criterion not set; record.
- **Record:** the servo's travel against the end stop.
- **Result to:** open item 24.
- **Safety:** a servo whose end stop may take a short stall; the supply's
  current limit set.

### 4B. The INA219 on J3

[current-sense.md](design/current-sense.md#bench-tests), "Bench tests";
STATUS open item 16. Setup A with the INA219 on J3; CURRENT SENSE at
`INA219`. "Only hardware settles these. Each names what it records."

#### CS-B1. Pull-ups and rise time

- **Verifies:** the board's pull-ups on SDA and SCL, and the rise time at
  400 kHz with the J3 lead and the module. **Why:** the design takes 0.3 m as
  the longest J3 lead until this measures it; if the edges miss 300 ns, the
  bus runs at 100 kHz, where standard mode allows 1000 ns.
- **Setup:** A; the ohmmeter, board off; the scope on SDA and SCL at J3.
- **Steps:** measure the pull-ups; capture SDA and SCL with the lead and the
  module; lengthen the lead.
- **Pass:** a rise time within 300 ns at 400 kHz; the longest lead that
  keeps it.
- **Record:** the pull-up values; the rise time per lead length.
- **Result to:** verification table, row "INA219 current sensor"; open
  item 16; current-sense.md, "The bus on J3".
- **Safety:** J3 by a pigtail in J3's order, not a Qwiic cable.

#### CS-B2. The INA219 beside the touch controller

- **Verifies:** the part found at 0x40; one frame's I2C traffic; touch with
  and without the module. **Why:** no INA219 has been on the board's bus.
- **Setup:** A; the scope or the analyser on SDA and SCL.
- **Steps:** power up with the module; open the servo tester's readout;
  capture one frame; tap through MENU with and without the module.
- **Pass:** the part found; touch works both ways. The frame's time:
  criterion not set; the host suite's figures are 96 clocks for the sensor
  and 84 for touch.
- **Record:** the frame's bus time.
- **Result to:** as CS-B1.
- **Safety:** none.

#### CS-B4. Current against a reference meter

- **Verifies:** offset and gain error at /8. **Why:** the datasheet facts
  the design marks "to confirm" are not checked against SBOS448G.
- **Setup:** bench supply 1 at 5 V into the INA219's VIN+, the resistive
  load and the reference ammeter from VIN- to GND; no converter. SERVO
  SUPPLY at `OWN`, SERVO TESTER open and SAFE: its readout row shows the
  current and the bus voltage, and with no switched supply the guard's
  cuts switch nothing.
- **Steps:** 0 A, 0.1 A, 1 A, 2 A and 3 A, each set with the ammeter or
  the electronic load (see Equipment); then 3.5 A, beyond the range.
- **Pass:** criterion not set in the note; record the offset and gain
  error, and the code at 3.5 A.
- **Record:** the readout and the reference at each step.
- **Result to:** as CS-B1.
- **Safety:** the load's power rating (17 W at 3.5 A and 5 V); the supply's
  current limit at 4 A, a value this plan sets.

#### CS-B5. Bus voltage against a multimeter

- **Verifies:** the bus voltage at 5 V, idle and at 2 A.
- **Setup:** as CS-B4; the multimeter at VIN-.
- **Pass:** criterion not set; record both readings.
- **Record:** the readout and the multimeter.
- **Result to, safety:** as CS-B4.

### 4C. The converter

[current-sense.md](design/current-sense.md#q1-the-converter-module), Q1:
"Whichever module is used is checked on the bench before it is relied on".
Setup C; the converter's input from bench supply 1 or its own cell. EN
stays off J4 until `CS-B6` has measured it with the board unpowered.

#### CS-B6. Disconnect, threshold, start-up and decay

- **Verifies:** VOUT with EN low at the lowest and the highest VIN (the
  disconnect); the EN threshold rising and falling at 3.0 V and 4.2 V VIN; EN
  from reset, in BOOTSEL and with the board off; the start-up time from EN
  high to 4.5 V; the decay after the switch-off with a servo and with none.
  **Why:** the module names no chip; its disconnect, threshold and
  start-up are known only from this test. `SUPPLY_START_MAX_MS`, 500 ms, is
  the note's value until B6 measures the start-up.
- **Setup:** C; the scope on EN and VOUT; bench supply 1 at 3.0 V, then
  4.2 V. GP28 and GP29 drive only 0 V or 3.3 V: a source on EN while EN is
  on J4 fights the pin.
- **Steps:**
  1. EN's wire off J4, VIN at 4.2 V: EN on its pull-down alone, which is
     what an unpowered board's pad sees; VOUT at 3.0 V and 4.2 V VIN (the
     disconnect).
  2. EN's wire still off J4: the threshold, rising and falling, at 3.0 V and
     4.2 V VIN, from bench supply 2 limited to 3.3 V and a current limit.
  3. Only if step 1 passes: EN to GP28 (J4 pin 11), SERVO SUPPLY at `GP28`.
     The board unpowered, held in reset, in BOOTSEL, and on the main
     screen: EN and VOUT in each.
  4. START and STOP on the servo tester: the start-up time from EN high to
     4.5 V; the decay after the switch-off, with a servo and with none.
- **Pass:** EN at or below 0.5 V in step 1 and with the board unpowered in
  step 3, and below the converter's falling threshold (kst-programming.md,
  "Hardware"); with EN low, VOUT at 0 V at both VIN and in each state of
  step 3 (kst-programming.md step 7). The start-up time and the decay:
  criterion not set; record them.
- **Record:** VOUT and EN in each state; both thresholds; the start-up time
  on the scope; the decay with and without a servo.
- **Result to:** verification table, row "The servo tester on the switched
  supply"; open items 16 and 19; current-sense.md Q1.
- **Safety:** a fuse at the cell; no servo for the disconnect steps.

#### OI-15d. EN at each pin

[STATUS.md](../STATUS.md#open-items) open item 15.

- **Verifies:** EN high and low at GP28 and at GP29, with the converter's
  output following, and EN low in every board state at GP29 as CS-B6 step 3
  found it at GP28. **Why:** no converter has been on GP28 or GP29. KST-7a
  and HT-6b use this record and CS-B6's.
- **Setup:** C, EN on GP28 (J4 pin 11), then GP29 (J4 pin 12), SERVO SUPPLY
  set to match.
- **Steps:** at GP29, CS-B6's step 3; at each pin, START and STOP on the
  servo tester.
- **Pass:** VOUT follows EN at each pin; at GP29 EN low and VOUT at 0 V in
  each board state.
- **Record:** the capture per pin and state.
- **Result to:** open item 15.
- **Safety:** GP28 and GP29 are not fault tolerant: EN only, nothing that
  drives the pin.

#### CS-B7. The converter under load

- **Verifies:** VOUT at 1 A, 2 A and 3 A, its current limit, its temperature
  after 1000 ms of stall. **Why:** decision 10 needs 5.0 V and at least 3 A;
  the module's ratings are unknown.
- **Setup:** C with no servo and CURRENT SENSE at `NONE`, so none of the
  guard's cuts acts: not the stall cut (above 1.0 A for 400 ms), the hard
  cut (3.0 A) or the collapse cut (under 4.0 V for 1000 ms). The resistive
  loads from VOUT to GND. The servo tester's START switches EN on. The scope
  on VOUT; the thermometer.
- **Pass:** criterion not set in the note beyond the requirement of 5 V at
  at least 3 A, with no tolerance on VOUT. For reference, the firmware cuts
  the supply when VOUT reads under 4.0 V for 1000 ms (`GUARD_LOW_MV`) with
  a sensor declared.
- **Steps:** set bench supply 1's limit just above the input current that
  3 A out needs, derived: 5.0 V × 3 A / 4.2 V = 3.6 A at 4.2 V VIN, 5.0 A at
  3.0 V VIN, before the converter's losses, which are not known. Load 1 A,
  2 A and 3 A in turn and read VOUT. Then step the load past 3 A in small
  steps, each at most 1000 ms, with STOP after each step and the converter
  cooled between steps; raise the supply's limit only while VOUT holds.
  Stop at the first droop of VOUT or when the converter is too hot to
  touch. At the end, set CURRENT SENSE back to `INA219` and save before
  OI-16: every later test in this plan relies on the cuts.
- **Record:** VOUT per load; the current at which it limits; the
  temperature.
- **Result to:** as CS-B6.
- **Safety:** no cut acts in this test; only bench supply 1's limit bounds
  the converter and the loads. The limit as in Steps; loads rated at least
  15 W at 3 A (1.67 Ω), more for a step past 3 A; each step at most 1000 ms;
  CURRENT SENSE back at `INA219` before OI-16.

#### OI-16. The servo tester on the switched supply

[STATUS.md](../STATUS.md#open-items) open item 16, which lists "the servo
tester's switch-on, its pulses at 4.5 V and its switch-off at STOP, a trip
and BACK" among B6 to B13;
[current-sense.md](design/current-sense.md#slice-3-as-built), slice 3.

- **Verifies:** EN low while SAFE, high at START, pulses only after VOUT
  reads 4.5 V, EN low at STOP and at BACK. **Why:** slice 3 runs in the host
  suite only.
- **Setup:** C with a servo; the scope on EN, VOUT and SIG.
- **Steps:** START; STOP; START; BACK.
- **Pass:** no pulse on SIG before VOUT reaches 4.5 V; EN low in the same
  pass as level 0 on the pins at STOP; after the switch-off, the reading at
  or below 1.0 V; no "not off" warning, which takes a reading above 2.5 V
  for 2000 ms (current-sense.md, "Read-back from the bus voltage").
- **Record:** the captures; the start-up time the screen shows.
- **Result to:** verification table, row "The servo tester on the switched
  supply"; open item 16.
- **Safety:** as Setup C.

#### CS-B3. Touch and the bus under faults

- **Verifies:** touch with J3 empty, with the module's VCC lead open, and
  with SDA held low through 100 Ω to GND during a run (the supply goes off,
  the bus recovers); what `i2c_write_timeout_us()` returns while SDA is
  held; IN- leakage with VS at 0 V and 5 V on IN+. **Why:** the stuck-bus
  cut and its error code are to be confirmed on hardware.
- **Setup:** C with a servo running; the 100 Ω resistor on a clip to SDA at
  J3.
- **Pass:** with SDA held, the supply goes off and the bus recovers after
  release. The return code and the leakage: criterion not set; record.
- **Record:** the screen; the code; the leakage current.
- **Result to:** verification table, row "The servo tester on the switched
  supply" (B3); open item 16.
- **Safety:** 100 Ω, never a short, to GND.

#### CS-B9a. The hard cut

[current-sense.md](design/current-sense.md#bench-tests), B9, its 1.5 Ω step,
numbered here 9a; the stall step is `CS-B9b` in Session 7. Run here so that
Session 5 relies on a proven hard cut, not on bench supply 1's current limit
alone.

- **Verifies:** the hard cut on one reading at or above 3.0 A
  (`GUARD_HARD_MA`), and the time from the current step to EN low.
- **Setup:** first CS-B4's setup: bench supply 1 at 5.0 V into VIN+, with
  the load, the shunt and the leads used below. The cut fires on one
  reading, within about 45 ms of the step (a reading every 25 ms, each over
  a 17.55 ms conversion), too soon to trim the load or read a
  meter, so the current is set and read here. The note's 1.5 Ω gives about
  3.13 A with the shunt in series; leads and the ammeter lower it. Set it at
  least 0.1 A over 3.0 A, a margin this plan sets; above 3.2 A, the end of
  the /8 range, the reading is clipped, which cuts as well. Then Setup C
  with the servo tester running: the load on VIN-, the servo's plus behind
  the shunt, so the INA219 sees its current, never on VOUT ahead of the
  shunt. VIN at 4.2 V. Bench supply 1's limit above the converter's input
  current at 3.1 A out, derived: 5.0 V × 3.1 A / 4.2 V = 3.7 A at 4.2 V VIN,
  and 5.2 A at 3.0 V VIN, before the converter's losses, which are not
  known; set it at about 4.5 A at 4.2 V VIN, a margin this plan sets for
  80 % efficiency. A lower limit lets VOUT sag and the current fall under
  3.0 A, which reads as a false fail. The scope on EN, on VOUT and across
  the shunt with two probes.
- **Steps:** switch the load onto VIN- while the tester runs; read the step
  on the scope across the shunt.
- **Pass:** EN low on the hard cut, with the current set beforehand at or
  above 3.1 A and the step on the scope consistent with it (the voltage
  across 0.1 Ω about 310 mV; two single-ended probes at about 5 V resolve
  it coarsely, a differential probe better). The time to EN low:
  criterion not set; record it.
- **Record:** the current set on bench supply 1; the step on the scope;
  VOUT during the step; the time; bench supply 1's limit and VIN.
- **Result to:** verification table, row "The cuts on the current sensor's
  readings"; open item 16.
- **Safety:** the load's rating, about 16 W; bench supply 1's limit as set
  above, with the cell's fuse.

#### CS-B11. The heartbeat with the supply on

[current-sense.md](design/current-sense.md#bench-tests), B11. Run here so
that Session 5 relies on it.

- **Verifies:** core0 halted in a debugger while the supply is on.
- **Setup:** C with a servo, the tester running; the SWD probe; the scope on
  EN.
- **Pass:** the supply off within 250 ms.
- **Record:** the time from the halt to EN low.
- **Result to:** verification table, row "The servo tester on the switched
  supply" (B11); open item 16.
- **Safety:** as Setup C.

## Session 5: servo programmers and backups

Prerequisites: all of Session 4 passed with the converter used here, the hard
cut (`CS-B9a`), the stuck bus (`CS-B3`) and the heartbeat
(`CS-B11`) among it; Session 3 for the ESC backups of `BK-1`. Setup C with the
servo under test; a card in the slot. Each write is relied on only after its
family's write with read-back below has passed.

### 5A. KST bring-up

[kst-programming.md](design/kst-programming.md#work-breakdown), "Work
breakdown" step 7, numbered here 7a to 7d; STATUS open item 10. "Bring-up,
in rcbench's order and before any write is relied on."

#### KST-7a. The converter, with the user's supply on

- **Verifies:** the converter with the KST servo as its load: the start-up
  time from EN high to VOUT in range, and VOUT at the end of the 2000 ms off
  time. EN and VOUT in each board state, at GP28 and GP29, and VOUT with EN
  low at the lowest and the highest VIN are `CS-B6`'s and `OI-15d`'s
  records, which serve here. **Why:** the KST search's entry delay rests on
  a measured start-up time.
- **Setup:** C with the KST servo; the scope on EN and VOUT.
- **Steps:** a switched search with the servo; LEAVE, which switches the
  supply off for 2000 ms.
- **Pass:** criterion not set for the start-up time: record it. VOUT at the
  end of the off time: "the level at which a KST servo resets is not known,
  so the record is the voltage reached, not a pass or fail".
- **Record:** the start-up time with the servo; VOUT at the end of the off
  time.
- **Result to:** verification table, row "KST switched servo supply"; open
  item 10.
- **Safety:** as Setup C.

#### KST-7b. The reply level at the connector

- **Verifies:** the level the servo drives on SIG in its reply. **Why:** the
  owner states the servo replies at 3.3 V (decision 4).
- **Setup:** C; the scope on SIG at the connector.
- **Pass:** no level above 3.6 V on SIG (decision 4's rule); above it, the
  servo needs a level shifter in its lead.
- **Record:** the reply's high level.
- **Result to:** open item 10; kst-programming.md decision 4.
- **Safety:** measure with the board powered.

#### KST-7c. The power-on window

- **Verifies:** without a switch, the shortest delay from power-on to a sync
  that still enters; with the switch, by stepping
  `KST_SUPPLY_ENTRY_DELAY_MS` (a rebuild per step); that the owner's HV
  servo enters programming mode at 5 V (decision 16).
- **Setup:** S for the window by hand, then C.
- **Pass:** the HV servo enters at 5 V. The window: criterion not set; by
  decision 1, a window shorter than about 120 ms makes the wrapper start
  attempts closer together, a design change for the owner.
- **Record:** the window in ms; the delays that enter and that miss.
- **Result to:** open item 10; kst-programming.md decision 1.
- **Safety:** HV servo at 5 V from the converter only.

#### KST-7d. Frames, reads and one write

- **Verifies:** frames and replies on a scope, read reliability, one write
  with read-back, then a power cycle and a read. **Why:** no frame of the
  line driver has been on a pin.
- **Setup:** C; the scope on SIG. Repeat the write once on USB power alone
  with no cell, to record whether a red BOARD voltage refuses it (`OI-22b`).
- **Steps:** read 20 times; one value changed and written with its hold;
  BACK to the leave page; HOLD TO REBOOT, which power-cycles the servo;
  read; then LEAVE.
- **Pass:** the read-back verified, and the value read after the power
  cycle. Read reliability: criterion not set; record the count of good
  reads.
- **Record:** the captures; the counts; the value before, after and after
  the power cycle; the backup's name; the USB alone outcome.
- **Result to:** verification table, row "KST programming"; open items 10,
  21 and 22.
- **Safety:** the servo free to move; a reply edge lost at the window start
  is met by a 10 kΩ pull-down in a servo-only lead, never on the board pin
  (decision 3).

#### OI-11. The KST card-phase cap on a real card

[STATUS.md](../STATUS.md#open-items) open item 11;
[kst-programming.md](design/kst-programming.md#work-breakdown).

- **Verifies:** whether a card reaches the cap of 15 s
  (`KST_CARD_PHASE_MAX_MS`). **Why:** at the SD specification's SDXC times
  the first write that keeps an old record takes 16.5 s on exFAT; a phase
  past the cap switches the supply off and loses the backup in memory; the
  phases with the write cache are not counted.
- **Setup:** C; the slow exFAT card; then the card with 1000 old logs in its
  root.
- **Steps:** a first write that keeps an old record, on each card.
- **Pass:** criterion not set: record whether the write completes or the
  supply goes off.
- **Record:** the outcome and the time on the write page.
- **Result to:** open item 11; kst-programming.md, "The cap".
- **Safety:** a spare servo setting that can be restored.

### 5B. Hitec bring-up

[hitec-programming.md](design/hitec-programming.md#work-breakdown), "Work
breakdown" step 6, numbered here 6a to 6f; STATUS open item 14. "Bring-up,
before a write is relied on."

#### HT-6a. The classic line's level

- **Verifies:** the classic line's high level with the servo's signal lead
  on a 68 kΩ resistor to ground and not on the board, while Hitec's
  programmer or the converter makes the servo drive the line; the pad's low
  level against the servo's 1 kΩ. "A check, not a gate": CLASSIC is enabled
  without it (decision 13).
- **Setup:** the servo on the converter; its signal lead to 68 kΩ to GND,
  off the board; the scope there. Then on SIG for the low level.
- **Pass:** criterion not set in the note. SIG is rated 5.5 V while the
  board is powered; a level above it means the lead is not put on the board.
- **Record:** the high level; the pad's low level.
- **Result to:** verification table, row "Hitec programming"; open item 14;
  decision 13's unknowns.
- **Safety:** the lead off the board until the high level is known.

#### HT-6b. The converter's checks

- **Verifies:** the checks of `KST-7a`, which Hitec shares: `CS-B6`'s and
  `OI-15d`'s records, with the Hitec servo as the load for the start-up
  time and VOUT at the end of the 2,000 ms off time.
- **Pass, record:** as `KST-7a`.
- **Result to:** open item 14.

#### HT-6c. The classic entry

- **Verifies:** on the owner's classic servos, the `+` after the switch-on,
  the second `+`, the answer to `67`, after the 2,000 ms off time; the
  converter's output decay to 0 V on a scope against that off time.
- **Setup:** C, HITEC CLASSIC; the scope on SIG and VOUT.
- **Pass:** each step of the entry answered. The decay: criterion not set;
  record whether VOUT reaches 0 V within the 2,000 ms.
- **Record:** the captures.
- **Result to:** as HT-6a.
- **Safety:** as Setup C.

#### HT-6d. The D series entry with a switch

- **Verifies:** the model read after 1,000 ms; a read by PRECHARGE and its
  answer delay.
- **Setup:** C, HITEC D SERIES; the scope on SIG.
- **Pass:** the model read; the answer delay within 5.93 ms to 6.52 ms.
- **Record:** the delay; the model.
- **Result to:** as HT-6a.
- **Safety:** as Setup C.

#### HT-6e. The full read against Hitec's programmer

- **Verifies:** the full read of each family against what Hitec's
  programmer shows.
- **Pass:** each value as the programmer shows it.
- **Record:** both lists.
- **Result to:** as HT-6a.

#### HT-6f. One write with read-back per family, then a restore

- **Verifies:** one value written, the read-back verified, the servo
  power-cycled and read again, and the value checked with Hitec's
  programmer; then a restore of the backup that changes at least three
  settings in one plan, so the 2 ms spacing of classic writes and the
  balance after each setting run once, read back, power-cycled and read
  again.
- **Pass:** each read-back verified; the values after each power cycle as
  written; the programmer agrees.
- **Record:** the values at each step; the backup's name.
- **Result to:** as HT-6a; a write is relied on per family only after this.
- **Safety:** a servo whose settings can be restored from the backup.

### 5C. The card under the claims

#### WC-2. A card swapped during a flush

[sd-write-cache.md](design/sd-write-cache.md#work-breakdown) step 5.

- **Verifies:** a swap during the flush of a KST write's backup. **Why:**
  with no card-detect line, a block from the queue would land on the new
  card, built from the old card's FAT.
- **Setup:** C with the KST servo; two scratch cards with different serial
  numbers.
- **Steps:** WRITE... and its hold; swap the card while the page shows the
  flush.
- **Pass:** a different serial number drops the queue and sets the cache
  `Failed`, and no write frame goes out ("Card swap", "Errors"); the new
  card's volume checks clean on the PC.
- **Record:** the screens; the check's result.
- **Result to:** verification table, row "SD write cache"; open item 12.
- **Safety:** scratch cards.

#### BK-1 to BK-3. Device backups

[STATUS.md](../STATUS.md#open-items) open item 13, numbered here;
[backups.md](design/backups.md#the-ring), "The ring".

- **Verifies:** the ring of `BACKUP/` on real cards. **Why:** built and run
  in the host suite only.
- **Setup:** each device of Sessions 3 and 5; a FAT32 and an exFAT card.
  The ESCs on Setup E's lead, never on Setup C's, which puts VOUT on an
  ESC's BEC wire.

| ID | Step | Pass | Record |
|---|---|---|---|
| BK-1 | a backup of each device (KST, Hitec, AM32, Bluejay, BLHeli_S) on each card | a file per device in `BACKUP/` with its prefix | the names |
| BK-2 | the ring turned past 50 on a card (backups copied on from the PC to fill it) | 001 after 050; the latest backup never written over; `BACKUP/LAST` holds the latest number | the names and `LAST` |
| BK-3 | a backup read back on a computer, then loaded with LOAD | criterion not set beyond the file's checksum line, which refuses a damaged file | the file |

- **Result to:** verification table, row "Device backups"; open item 13.
- **Safety:** as each device's session.

## Session 6: receiver, S.BUS output on a scope, angle meter

Prerequisites: Session 1; `KS-2` of Session 2 for `RX-8`; `CS-B2` of
Session 4 for `AM-B9`. No ESC on any pin, except in `RX-8`, which uses
Setup E with its safety rules.

### 6A. The S.BUS monitor

[rx-signals.md](design/rx-signals.md#bench), "Bench", the S.BUS bullet,
split here into RX-1 to RX-8 in its order; STATUS open item 19. Setup R;
MENU → RX SIGNALS → MONITOR. "Only hardware settles these."

- **Why:** the UART inversion on the pin, a receiver's level and frames on
  GP1 and the screen's reading of them are not tested on hardware.
- **Result to:** verification table, rows "S.BUS monitor's core1 job" and
  "S.BUS MONITOR and RX SIGNALS"; open item 19; rx-signals.md slice 6.
- **Safety:** the receiver on its own supply, never J5 pin 3, J4 pin 6 or
  J4 pin 2; the board powered before the receiver, the receiver off before
  the board.

| ID | Step | Pass | Record |
|---|---|---|---|
| RX-1 | the receiver's output level and idle level on the scope at GP1, before the first connection | the high level within GP1's rating, 5.5 V with the board powered; that a 3.3 V output clears the RP2350's threshold "to confirm" | both levels per receiver |
| RX-2 | frames from at least one receiver per make at hand | frames decoded on the screen | the frame period each sends |
| RX-3 | 10 min of frames | zero parity and framing errors over 10 min | the counts |
| RX-4 | each stick and switch against the transmitter's channel monitor | the channel order matches | the order |
| RX-5 | the transmitter off, in the receiver's hold mode and its no-pulses mode | criterion not set: record what each shows (`FAILSAFE` is the last frame's failsafe bit, `NO SIGNAL` no frame for 100 ms) | the screen per mode |
| RX-6 | unplug the receiver | `NO SIGNAL` within 100 ms plus one UI frame | the time |
| RX-7 | GP1's level after unplugging (erratum RP2350-E9) | criterion not set | the level; what the counts show |
| RX-8 | after `KS-2` has passed: leave the monitor, unplug the receiver; then Setup E with its safety rules (propeller off, the board powered before the ESC) and KISS on GP1 by the telemetry wire | KISS decodes again, no inversion left | the tiles' tags |

### 6B. The S.BUS output without an ESC or an FC

[sbus-output.md](design/sbus-output.md#bench), "Bench", bullets 1 and 2,
numbered here SO-1 and SO-2; STATUS open item 31. MENU → RX SIGNALS →
OUTPUT. Nothing on GP0 but the scope or the second board. The bootloader
probe runs at each bind with nothing to answer it.

#### SO-1. The line on a scope

- **Verifies:** the idle level low while SAFE, after STOP and after the
  release; no high edge at the bind; 100,000 baud, 8E2, the inversion;
  25 bytes in 3,000 µs and a 14,000 µs period; the last frame complete at
  STOP and at a heartbeat trip (core0 halted in a debugger). **Why:** the
  inversion and the level on GP0, the bind's and the release's edges and
  the frames are not tested on hardware.
- **Setup:** the scope on GP0 (J5 pin 2), nothing else on GP0; the SWD
  probe.
- **Pass:** each item as listed. At the bind: the probe's greetings, the
  line resting high on the pull-up between them, then one fall at the
  switch to the UART, and no high edge after it (sbus-output.md, slice 3 as
  built, steps 3 and 4).
- **Record:** the captures; core1's pass time with the job if the
  debugger reads it.
- **Result to:** verification table, rows "S.BUS output, slices 1 and 2"
  and "S.BUS OUTPUT, slice 4"; open item 31.
- **Safety:** nothing on GP0 but the probe.

#### SO-2. Read back by a second board's monitor

- **Verifies:** the channels and flags as sent.
- **Setup:** GP0 of this board to GP1 of the second board, the grounds
  joined; the second board keeps SIG at GP0, so its AUX is GP1, and runs
  S.BUS MONITOR.
- **Pass:** criterion not set in the note beyond "read back": each channel,
  CH17, CH18 and both flags as set on SET.
- **Record:** a photo of both screens per setting.
- **Result to:** as SO-1.
- **Safety:** both boards powered before the link; no ESC.

### 6C. The angle meter

[angle-meter.md](design/angle-meter.md#bench), "Bench", B1 to B9; STATUS
open item 27. Setup A; MENU → TOOLS → ANGLE METER. "Only the board settles
these." Raw counts, WHO_AM_I and REVISION_ID are not on the screen: the
screen shows `NO IMU` or `NOT A QMI8658: ID 0X..`; the raw values need a
debugger or a debug print, which the note does not set.

- **Result to:** verification table, rows "QMI8658 IMU driver", "Angle
  meter's model" and "Angle meter's screen"; open item 27; angle-meter.md
  slice 6.
- **Safety:** none; the board on the cell or a short USB lead.

| ID | Test | Pass (the note) | Record |
|---|---|---|---|
| AM-B1 | address and identity | 0x6B answers; WHO_AM_I 0x05 (no `NO IMU`, no `NOT A QMI8658`) | REVISION_ID, which tells the part |
| AM-B2 | byte order and burst, flat on a table, then a known tilt | `a_z` reads ±1 g within 0.1 g; a known tilt reads on the expected axis; the time stamp changes once a sample | when the first sample arrives after CTRL7 (the host model assumes 32 ms) |
| AM-B3 | each edge up in turn | criterion not set: which chip axis reads +1 g | the axis map, for `BOARD_IMU_AXES` |
| AM-B4 | 60 s still on a table | criterion not set | the standard deviation of `theta` per frame; how often `SETTLED` drops |
| AM-B5 | the flip, five times | criterion not set; the limits are proposals until B5: offsets within ±0.2 g, gains 0.88 to 1.12 | the spread of each offset |
| AM-B6 | wedges at 0°, 5°, 10°, 20° and 30°, each way, with and without the flip; zero, move and return ten times | criterion not set; a gain error over 0.2° brings decision 3's edge poses | the error at each angle; the repeatability |
| AM-B7 | from a cold start, 30 min still | criterion not set | the drift of `theta` against the IMU temperature |
| AM-B8 | the board's mass with and without its LiPo; a band across the glass; the standoffs | criterion not set | the mass; whether the band reads as a touch; whether the standoffs, not a plug, carry the board |
| AM-B9 | touch and the INA219 while the meter reads | touch and the INA219 work | the INA219's readout during the meter |

## Session 7 (risky): supply cuts and the servo log

Prerequisites: all of Session 4 passed with the converter and the INA219
used here; `SD-1` and `SD-2` of Session 1 on the card used for `CS-B12`.
Setup C; the scope on EN, SIG and the two ends of the 0.1 Ω shunt, each
end with its own probe and the ground clips on board GND only, or a
differential or current probe. The hard cut and the heartbeat (`CS-B9a`,
`CS-B11`) passed in Session 4. Run the tests in the order
below: the stall first, the log last.

[current-sense.md](design/current-sense.md#bench-tests), "Bench tests";
STATUS open item 16.

- **Safety:** the converter's cell fused; bench supply 1's current limit
  set; a servo stalled no longer than the test needs; the shunt and the
  load hot after a stall (record, do not touch); the board powered first.

#### CS-B8. The owner's servos and the limit run

- **Verifies:** idle, moving and holding current at 5.0 V, and stall at an
  end stop; the shunt's temperature after a 1000 ms stall; whether the
  limit run, above 1.0 A for 400 ms (decision 13), trips at MIN and MAX
  against a stop. **Why:** the note warns that the limit run trips on many
  servos against a stop; per-servo values come only from here.
- **Steps:** each servo: idle, a sweep, a hold, then MIN and MAX against a
  stop.
- **Pass:** criterion not set: a measurement against the design's
  expectation. Record whether it trips.
- **Record:** the currents per servo; the shunt's temperature; trips at MIN
  and MAX.
- **Result to:** verification table, row "The cuts on the current sensor's
  readings"; open item 16; current-sense.md, "Protection".

#### CS-B9b. The stall cut

[current-sense.md](design/current-sense.md#bench-tests), B9, its stall step,
numbered here 9b; the hard-cut step is `CS-B9a` in Session 4.

- **Verifies:** the stall cut with a servo held at its stop, and the time
  from the stall to EN low.
- **Steps:** a servo held at its stop while the tester runs.
- **Pass:** the stall cut at 400 ms above 1.0 A. The time to EN low:
  criterion not set; record it.
- **Record:** the capture; the current across the shunt; the time.
- **Result to:** as CS-B8.

#### CS-B10. The move timing against a scope

- **Verifies:** a move timed by `servo_move` against SIG and the current on
  an MG90S, beside rcbench's record of the same servo.
- **Pass:** criterion not set in the note; the host test holds the time
  within one read and two cycles of the model.
- **Record:** the screen's time, the scope's, rcbench's record.
- **Result to:** verification table, row "The move timing"; open item 16.

#### CS-B13. The voltage at the servo at a stall

- **Verifies:** the voltage at the servo's plus at a stall, behind the shunt
  and the leads.
- **Pass:** criterion not set; record.
- **Record:** the voltage at the servo and at VOUT.
- **Result to:** as CS-B8.

#### CS-B12. A 60 s log with the supply on

- **Verifies:** the servo log, `LOGS/SRVnnnnn.CSV`, on a real card with the
  supply on: no heartbeat trip, the rows counted, and the cuts' timing with
  the log on. **Why:** a real card's read latency and busy time, the
  receiver's `busRxReady()` on the board and the cuts' timing with the log
  on are not measured.
- **Prerequisites:** CS-B9a, CS-B9b and CS-B11 passed.
- **Steps:** HOLD TO START with a card mounted; run 60 s, sweeping; STOP;
  repeat CS-B9b with the log on; read the files on the PC.
- **Pass:** no heartbeat trip in the 60 s; the rows counted. The row count
  has no figure in the note; for reference, one reading every 25 ms gives
  about 2,400 rows in 60 s (derived). The cut's time with the log on:
  criterion not set; record it against CS-B9b's.
- **Record:** the row count; the file's size; the cut's time with the log.
- **Result to:** verification table, row "The servo log"; open item 16;
  current-sense.md, slice 6 as built, "Not on hardware".

## Session 8 (risky): the S.BUS bootloader probe, then an FC

Prerequisites: `BJ-B1` and `BJ-B2` (Session 3), so each ESC is known to
answer ESC SETTINGS' greeting on this board; `SO-1` and `SO-2`
(Session 6). The probe tests come before any frame reaches an FC.

[sbus-output.md](design/sbus-output.md#bench), "Bench", bullets 3 to 6,
numbered here SO-3 to SO-6; STATUS open item 31. Run in the order SO-5,
SO-4, SO-6, SO-3.

- **Safety:** S.BUS output never to an ESC outside SO-4 to SO-6. In those, no
  motor on the ESC; the ESC's pack off before the leave page's last button after
  an EFM8 answer (decision 8's leave page). SO-6 on an expendable ESC: by
  source, BLHeli_S can calibrate a throttle range into its flash, arm and meet
  its motor-start condition on S.BUS.
- **Result to:** verification table, rows "S.BUS output's bootloader probe"
  and "S.BUS OUTPUT, slice 4"; open item 31; sbus-output.md slice 5.

#### SO-5. The probe against powered ESCs

- **Verifies:** a Bluejay and a BLHeli_S ESC powered on GP0 answer the probe
  and START stays refused until they are unplugged; an AM32 powered up
  during the quiet second answers too. **Why:** no ESC has answered the
  probe from this board.
- **Setup:** Setup E without a motor, each ESC powered before OUTPUT opens.
- **Pass:** as listed; MENU's run cue after an answer; after an EFM8 answer
  the leave page of decision 8.
- **Record:** the screen per ESC; the time to the refusal.

#### SO-4. ESCs powered after START

- **Verifies:** an AM32 and a Bluejay ESC on GP0, motor disconnected,
  powered up after START, so the claim's check never saw them.
- **Setup:** Setup E without a motor; the ESC unpowered at START.
- **Pass:** neither arms nor beeps a detection, as read from source.
- **Record:** the sounds; 2 min watched per ESC, a time this plan sets.

#### SO-6. BLHeli_S and Multishot, on an expendable ESC

- **Verifies:** with no motor, powered up after the probe, channel 16 at
  1500 µs and CH17 on: whether it detects Multishot (its arm-sequence
  beeps), and its stored minimum and maximum throttle read with ESC
  SETTINGS before and after. "The test may rewrite the ESC's settings."
- **Setup:** Setup E without a motor, the expendable BLHeli_S ESC,
  unpowered until the probe has passed.
- **Steps:** START; power the ESC; watch for at least 200 s: in the note's
  model of the line, detection takes 4.5 s to 199 s, about 40 s at the
  median. Channel 16 at 1500 µs and CH17 on are off the START frame, so the
  idle stop ends the frames after 30 s with no touch (sbus-output.md,
  "Safety"): touch an inert spot of the screen at least every 30 s, and
  record any idle stop. STOP; read the throttle values on ESC SETTINGS.
- **Pass:** criterion not set; record.
- **Record:** the beeps and the time to them; any idle stop; both
  throttle values before and after.

#### SO-3. A Betaflight FC

- **Verifies:** the FC's receiver page reads the 16 channels and CH17 and
  CH18 as sent; throttle at the ceiling; the failsafe bit, the frame-lost
  bit, STOP and the idle stop each bring `RXLOSS` and then the failsafe's
  disarm; frames after START clear it.
- **Setup:** F; the FC on its own supply, propellers off; a PC on the FC's
  USB for its receiver page.
- **Pass:** as listed. The ceiling at the default MAX THROTTLE is raw 499,
  which Betaflight reads as 1191.9 µs (sbus-output.md, "Safety"). By the
  note's reading of Betaflight, not run: `RXLOSS` within about 150 ms and
  the disarm about 1.5 s after the last valid frame; record both.
- **Record:** the receiver page per setting; both times per stop cause.

## Session 9 (risky): the USB bridge with desktop tools

Prerequisites: all of Session 3 (ESC SETTINGS reads each ESC used here);
`SD-3` (the card reader enumerates). Setup U; MENU → ESC CONFIG → USB
BRIDGE; the logic analyser on SIG.

[usb-bridge.md](design/usb-bridge.md#bench), "Bench", B1 to B11; STATUS
open item 29. "Only an ESC and a PC settle these."

- **Safety:** propeller off; flash only a firmware version already on the
  ESC; `UB-B8` and `UB-B10` on a spare ESC, last.
- **Result to:** verification table, rows "USB bridge, slice 1" to
  "slice 3"; open item 29; usb-bridge.md slice 4.

| ID | Test | Pass (the note) | Record |
|---|---|---|---|
| UB-B1 | enumeration on Windows, Linux and macOS; PORT OPEN with each tool | one serial port and the card reader | per system and tool |
| UB-B6 | the analyser on SIG during a read | no gap within a frame; the turnaround and the echo's timing: criterion not set | the captures; the turnaround (AM32 answers in about 0.3 ms to 0.35 ms by rcbench's specification) |
| UB-B2 | AM32 configurator, Direct ticked: read, write one setting, flash the same firmware version again | each of the three completes | the tool's messages |
| UB-B3 | AM32 configurator, default: MSP, 4-way, the same three | each completes | as UB-B2 |
| UB-B4 | ESC-Configurator in Chrome: connect, read and write a Bluejay or BLHeli_S ESC and an AM32 ESC; flash | each completes | as UB-B2 |
| UB-B5 | BLHeliSuite with each interface that reaches a COM port | criterion not set | which interfaces work |
| UB-B7 | AM32 2.22 reaching its bootloader without a power cycle, after a power-on and after DShot stops | criterion not set | both cases |
| UB-B9 | BACK after a session | criterion not set | whether the ESC starts its firmware once DShot resumes, after how long; the tester's eRPM |
| UB-B11 | the receiver's rate limit: `pio_uart_rx` loses bytes from a sender 3 % fast in the model | criterion not set | the bootloader's bit time on the analyser; a read of 256 bytes through the bridge at room temperature and warm |
| UB-B8 | a spare ESC: the USB cable pulled during a page write | criterion not set | the ESC's state after a power cycle; a second flash through the bridge |
| UB-B10 | the USB cable pulled during a page write, the board on its cell and on USB power alone | criterion not set | the screen, SIG's level on the analyser, the ESC's state |

## Session 10 (risky): destructive tests on spare parts

Prerequisites: Sessions 3 and 9 on the same ESC types. Spare ESCs and
scratch cards only.

- **Safety:** no motor, or a motor without propeller; parts that may be
  left without their settings or tags.

#### BJ-B8. The battery pulled during a write

[bluejay-blheli-s.md](design/bluejay-blheli-s.md#bench), B8.

- **Pass:** the next session shows lost tags; the ESC runs on defaults; a
  reflash on a computer brings the tags back (the note's expectation, read
  from source).
- **Record:** the screens; the reflash tool.
- **Result to:** verification table, row "Bluejay and BLHeli_S, slice 4";
  open item 30.

#### BJ-B9. The signal wire pulled during a write

- **Pass:** `TORN` or `LOST`; with the wire back, READ AGAIN and WRITE
  restore the page, tags included.
- **Record:** the result line; the page after.
- **Result to:** as BJ-B8.

#### BJ-B10. The board's USB cable pulled during a write

- **Pass:** the ESC stays in its bootloader. What the next session reads:
  criterion not set; record.
- **Record:** the board on its cell and on USB alone; the next read.
- **Result to:** as BJ-B8.

#### BJ-B12. DShot into a waiting bootloader

- **Verifies:** a spare ESC in its bootloader, DShot frames for 60 s.
- **Pass:** criterion not set; record.
- **Record:** the page afterwards; the firmware after a power cycle.
- **Result to:** as BJ-B8; bluejay-blheli-s.md, "Not verified".

#### EC-B9. A link loss during a sequence

[esc-commands.md](design/esc-commands.md#bench), B9.

- **Verifies:** the ESC's supply cut during the first block and during the
  save block of a direction change.
- **Pass:** the outcome line follows the slot's rule
  ([slice 1 as built](design/esc-commands.md#slice-1-as-built)): not sent
  for a stop before frame 6 of the first block, save lost after it.
- **Record:** the page's outcome lines; the ESC's direction after.
- **Result to:** verification table, row "ESC COMMANDS".

#### WC-3. Power pulled during a flush

[sd-write-cache.md](design/sd-write-cache.md#work-breakdown) step 5;
STATUS open item 12.

- **Verifies:** the board's power cut while the write cache flushes a
  backup; the volume checked on a computer.
- **Pass:** criterion not set; record the check's result.
- **Record:** the check's output; which files survive.
- **Result to:** verification table, row "SD write cache"; open item 12.

## Tests whose note sets no pass criterion

These tests record a figure; their notes give no number to pass against.
Where a note gives one for part of a test, the rest is listed here. Each
result is recorded as measured, and a pass or fail is the owner's call.

| Criterion | Tests |
|---|---|
| None: record only | `OI-21`, `VT-6`, `DG-7`, `SD-4`, `RS-4`, `RS-5`, `OI-1`, `EC-B4`, `EC-B7`, `EC-B8`, `WC-1`, `WC-3`, `BJ-B3`, `BJ-B4`, `BJ-B7`, `BJ-B11`, `BJ-B12`, `BJ-B13`, `SV-9`, `CS-B4`, `CS-B5`, `CS-B7`, `CS-B8`, `CS-B10`, `CS-B13`, `OI-11`, `HT-6a`, `KST-7a`, `HT-6b`, `OI-24c`, `BK-3`, `RX-5`, `RX-7`, `AM-B3` to `AM-B8`, `SO-6`, `OI-18`, `UB-B5`, `UB-B7` to `UB-B11` |
| Part of the test | `OI-22b` (the reading's error), `RS-2` (the drift), `SD-2` (beyond the drop count), `OI-20a` (the scan's time), `BB-1` (the viewers), `OI-15a` (`DSERR`), `PC-3`, `EC-B2` (the tone), `BJ-B2`, `BJ-B5` (the rise time), `BJ-B10` (the next read), `OI-24a` (the servo's travel), `CS-B2` (the frame's time), `CS-B3` (the code, the leakage), `CS-B6` (the start-up time, the decay), `CS-B9a` and `CS-B9b` (the time to EN low), `CS-B12` (the row count, the cut's time), `KST-7c` (the window), `KST-7d` (read reliability), `HT-6c` (the decay), `SO-2`, `SO-3` (the two times), `UB-B6` (the turnaround, the echo) |

## Results template

Copy one row per test run. "Result" is `pass`, `fail` or `recorded` (for a
test with no criterion). "Recorded in" names the STATUS row or open item
and the note's section the result went into.

| Test | Date | Firmware commit | Parts (make, model, firmware) | Setup and pins | Result | Figures with units | Captures and photos | Recorded in |
|---|---|---|---|---|---|---|---|---|
| CS-B12 | | | | C, EN GP28 (J4 pin 11), SIG GP0 (J5 pin 2) | | | `CS-B12-1.png` | STATUS row "The servo log", open item 16, current-sense.md slice 6 |
| | | | | | | | | |

## Tests per source

| Source | Tests in this plan |
|---|---|
| angle-meter.md | 9: `AM-B1` to `AM-B9` |
| blackbox-logging.md | 1: `BB-1`; its unmeasured BUF PEAK and WORST FLUSH are `SD-2` |
| bluejay-blheli-s.md | 13: `BJ-B1` to `BJ-B13` |
| current-sense.md | 13: `CS-B1` to `CS-B13`, B9 in two steps, `CS-B9a` and `CS-B9b` |
| dshot-lifetime.md, slice D2 | 6: `DL-1` to `DL-6` |
| diagnostics.md | 8: `DG-1` to `DG-8` |
| esc-commands.md | 9: `EC-B1` to `EC-B9` |
| hitec-programming.md | 6: `HT-6a` to `HT-6f` |
| kiss-telemetry.md | 2: `KS-1` (also open item 3), `KS-2` |
| kst-programming.md | 4: `KST-7a` to `KST-7d` |
| rx-signals.md | 8: `RX-1` to `RX-8`, from its one S.BUS bullet; the PPM bullet is not built |
| run-summary.md | 5: `RS-1` to `RS-5` |
| sbus-output.md | 6: `SO-1` to `SO-6` |
| sdio-driver.md | 5: `SD-1` to `SD-5` (also open item 6) |
| sd-write-cache.md | 4: `WC-1` to `WC-3` from step 5, `WC-4` from open item 12 |
| usb-bridge.md | 11: `UB-B1` to `UB-B11` |
| pin-claim.md, through open item 8 | 4: `PC-1` to `PC-4` |
| servo-tester.md, through open item 9 | 9: `SV-1` to `SV-9` |
| backups.md, through open item 13 | 3: `BK-1` to `BK-3` |
| STATUS.md open items alone | 18: `OI-1`, `OI-4`, `OI-11`, `OI-15a`, `OI-15d`, `OI-15e`, `OI-16`, `OI-18`, `OI-19`, `OI-20a`, `OI-20b`, `OI-21`, `OI-22a`, `OI-22b`, `OI-23`, `OI-24a`, `OI-24b`, `OI-24c` |
| STATUS.md verification table alone | 4: `VT-2`, `VT-3`, `VT-5`, `VT-6`; `VT-1` and `VT-4` are part of `OI-15a` |

147 tests in all. menu.md and menu-b.md have no bench list; the panel check
of open item 18 is `OI-18`.
