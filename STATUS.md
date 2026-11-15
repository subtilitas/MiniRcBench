# Where MiniRcBench stands

The running record: what exists, how far each part is verified, and what is
open. Two parts of this file are held to the tree by a checker. Every relative
link here and in [CHANGELOG.md](CHANGELOG.md) is checked by
`test/check_docs.py`. The coverage table under "Tests and CI" (continuous
integration) is written and checked by `tools/coverage.py`. Nothing else is
checked: the architecture figures, pin numbers, suite and job lists and
verification states are written by hand, against the code and the issue tracker
as of 2026-11-15. The KST and Hitec card-phase tables in
[kst-programming.md](docs/design/kst-programming.md) and
[hitec-programming.md](docs/design/hitec-programming.md) are written and
checked by `make -C test kstcount`. Read the rest as a dated record, not as an
inventory.

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
pulses. KST and Chaservo servo programming, on the same pins, is slice 3 of
[pin-claim.md](docs/design/pin-claim.md), with an optional bench-switched
servo supply whose enable is on GP28 or GP29, which the servo tester switches
too (slice 3 of [current-sense.md](docs/design/current-sense.md)), and which
the current sensor's cuts switch off (slice 4); the servo tester times each
step of MIN, CENTRE or MAX from the sensor's current (slice 5). Steps 1 to
5 of [kst-programming.md](docs/design/kst-programming.md) are built:
rcbench's KST core, line driver and file store are copied into `lib/rcbench/`,
with rcbench's tests of them, 298 cases; the search (`src/kst_seek.h`) and the
core1 job (`src/kst_job.h`) run the session on the line, tested against
rcbench's simulated servo; `src/kst_files.h` puts the SAVE AS files and the
record of a write in `KST/` through FatFs, and the session's backup into the
ring of `BACKUP/` that the AM32 editor shares (`src/backup_ring.h`,
[backups.md](docs/design/backups.md)), tested against an in-memory volume;
`src/ui_kst.cpp` is KST SERVO, opened from MENU → SERVO PROGRAMMER →
KST / CHASERVO and run through both in the host suite.
The switched supply (step 5, `src/servo_supply.h`) powers the servo from a
converter whose enable is on GP28 or GP29, with `SERVO SUPPLY` at `GP28` or
`GP29`; with `NONE` the servo is powered up and down by hand.
Hitec programming, on the same pins, is slice 4 of
[pin-claim.md](docs/design/pin-claim.md): steps 1 to 5 of 6 of
[hitec-programming.md](docs/design/hitec-programming.md) are built; the
bring-up is open (open item 14).
Slice 3 builds on the menu restructure
([menu.md](docs/design/menu.md), slices A to C). Slice A is built: one
scrolling list, `src/ui_list.*`, under the AM32 and SERVO SET lists, and one
hold, `src/ui_hold.*`, under HOLD TO WRITE, HOLD TO START, HOLD TO APPLY,
HOLD TO SAVE and HOLD DEFAULTS. Slice B is built: every screen reports its
controls by id (`src/ui_screen.h`), and the host tests tap them by id.
Slice C is built: MENU, a list from the registry in `src/ui_registry.cpp`,
and SETTINGS, one sectioned list with pole count and throttle ceiling at its
top (`src/ui_menu.*`, `src/ui_settings.*`, `src/ui_sdlog.*`). Every header is
36 px tall and every tool's BACK returns to MENU, but SETTINGS opened from the
tester's readouts, whose BACK returns to the tester (slice 7 of menu
structure B).
The S.BUS (Futaba's serial bus for receiver channels) monitor of
[rx-signals.md](docs/design/rx-signals.md) is built in its slices 2 and 3,
host tests only, not on hardware. Its core1 job (`src/sbus_job.h`): during
its claim GP1 is the input of UART0 (universal asynchronous
receiver-transmitter 0) with the pad's input inverted, and rcbench's decoder
turns the bytes into frames, flags, a mean frame interval and error counts.
Its screen, S.BUS MONITOR (`src/ui_sbus.h`, with the pure formatter
`src/sbus_view.h`), opens from MENU → RX SIGNALS → MONITOR: the link chip,
16 channel bars in raw units with the value in µs, CH17 and CH18, the frame
rate and the error counts, and the leave page. RX SIGNALS
(`src/ui_rx_signals.h`) is the chooser behind the MENU row; its OUTPUT, the
S.BUS output, is drawn disabled, `NOT BUILT` (open item 31). The output's
core1 job (`src/sbus_out_job.h`, slices 1 and 2 of
[sbus-output.md](docs/design/sbus-output.md#slice-2-as-built)) is built,
host tests only: UART0's TX sends rcbench's encoded frames on GP0, the pad's
output inverted, one every 14 ms. Nothing opens it: no screen claims
`PinUser::SbusOut` until the bootloader probe of slice 3 is built.

Slices 2 to 4 of
[bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-4-as-built)
are built, host tests only, not on hardware. `src/bl_answer.h` classifies the
greeting's answer: an AM32 family, an EFM8 bootloader of Bluejay or BLHeli_S
by `471` with 0xE8 four bytes on, or neither. MENU's second row is
`ESC CONFIG` (`src/ui_esc_config.h`), a chooser holding `ESC SETTINGS`, the
AM32 editor retitled, and `USB BRIDGE`, the USB bridge's screen; the
editor's views are titled `ESC SETTINGS`, `ESC HEX` and `ESC WRITE`. The
connect page greets in bursts of 3 between seconds of quiet line
(`src/greet_cycle.h`). An EFM8 answer sets MENU's run cue and gives the
claim a leave page (`pinClaimWantLeavePage()`); the AM32 transport lets go
of SIG and the claim's core1 job, rcbench's escwire session
(`src/escwire_job.h`), makes its own first contact and reads the page. A
Bluejay 208 or BLHeli_S 33 page opens as an editable list, an older layout,
lost tags or a wrong marker as a read-only list that saves its backup as it
opens, with strip notes and ESC HEX (`src/efm8_view.h`); a page rcbench does
not take is named on the connect page. ESC WRITE's hold saves the page as
first read to the card as `BJnnn.TXT` or `BHnnn.TXT`
(`src/efm8_backup.h`), then the job writes the draft with rcbench's write:
re-read, erase, program and read-back, on the settings page only. A
half-written page is guarded: no edit, no LEAVE, and BACK's second tap
aborts the session and opens the leave page. BACK from the list sends LEAVE,
`00 01`, and DShot returns; the cue stays until an eRPM frame, as LEAVE has no
answer. Every other BACK after an EFM8 answer, or after a reply holding a
piece of one, opens the leave page of that note's decision 8, whose RESTART
DSHOT alone restarts DShot and clears the cue.

Slices 1 to 3 of
[usb-bridge.md](docs/design/usb-bridge.md#slice-3-as-built) are built, host
tests only, not on hardware. `PinUser::Bridge`
claims SIG with no ESC check and no leave page of its own, and runs
`Core1Job::Bridge` (`src/bridge_wire.h`): DshotDisplay's frame slot and
1024-byte ring between the cores, on a PIO line of two state machines in
one block (`src/bridge_line_port.h`), the 7-word transmitter of
`src/bridge_line.pio`, which sets the pad's direction by side-set and keeps
it driven between the bytes of a frame, and `pio_uart_rx`'s receiver on the
same pad, whose echo the job matches against the bytes sent. A claim
without the leave page restores DShot only once its job is idle; the
bridge's release finishes the frame under way, at most 250 ms.
USB BRIDGE (`src/ui_bridge.h`), ESC CONFIG's second entry, takes the claim
unless USB DRIVE holds the card; while the claim runs, `src/usb_bridge.h`,
called every pass of core0's loop after `usbDevTask()`, takes the serial
port from the console (`usbDevConsole()`). The PC's first byte chooses
the mode: `$` MSP, answered by `src/bridge_msp.h` as a flight controller
named `MRCB` with one motor; 0x2F the 4-way interface,
`src/bridge_4way.h`, which checks each frame whole and turns it into
escwire's bootloader frames for the one ESC on SIG; any other passthrough,
bytes both ways. Both read byte 0x12 of AM32 settings pages for the
tester's 3D note. BACK is drawn disabled while a write-class frame waits
for its answer; every other BACK of the running bridge opens the leave
page, as the servo tools' BACK does, and MENU shows the run cue from the
open until RESTART DSHOT or an eRPM frame. The console has the port
again once the claim is free, and the clock's `CLOCK` line with it.
`USB_BRIDGE_LOG`, 0 by default, builds DshotDisplay's traffic log.

Menu structure B ([menu-b.md](docs/design/menu-b.md)), the owner's choice of
2026-11-13, is built in its 10 slices, host tests only, not on hardware.
Slice 1: the leave page
widget (`src/ui_leave.*`), the BACK guard and two strip lines
(`src/ui_widgets.*`), the field row (`src/ui_list.*`), the colour role names and
the layout constants. Slice 2:
the uses that decision 11's tables name in their roles' colours (`UI_COL_*`);
the rows of AM32 CONFIG, KST SERVO, HITEC SERVO and SERVO SET drawn through the
field row, a changed value `C_CYAN` with a 3 px bar; square 12 px chips;
`PROG MODE` on blue. Slice 3, names and SETTINGS: SIG PIN and AUX PIN with their
J5 pin, a step of either swapping the two; SERVO SUPPLY a core section of
SETTINGS, its first value `OWN`; MENU's strip `SIG GP0  AUX GP1  DSHOT600` and,
while the claim is not Free, `DSHOT RESTARTING ON SIG GP0` over rows drawn
disabled; `DSHOT600` on the tester; `NOTHING SAVED: DEFAULTS IN USE`; four KST
labels. Slice 4, the layout table (the note's decision 14): the footers of
SETTINGS and SERVO SET, the `-` and `+` of SERVO SET and AM32 CONFIG, AM32's
result OK as the wide button, SD LOG's button edge, HOLD DEFAULTS in place of
HOLD RESET, the red HOLD TO SAVE, the write pages' `CHANGES` and Hitec's 4 rows
and strip. Slice 5, the write rule and the guard (decisions 6, 7 and 13's AM32
titles): `WRITE...` and `RESTORE...` open the write and restore pages of AM32
CONFIG, KST SERVO and HITEC SERVO on a tap, and only the page's 1 s hold
writes; AM32 CONFIG has the page `AM32 WRITE`, its editor bar names the old
value, and its dump is titled `AM32 HEX`; BACK on AM32 CONFIG's list with edits
not written and on SERVO SET with a change waiting arms `UiBackGuard`; BACK is
drawn disabled while a write runs or waits for the card, and the leave queue is
gone. Slice 7, the tester: a tap on `14 POLES` or `MAX 20%`, underlined
while disarmed and while the claim is Free, opens SETTINGS through
`uiOpenTool()` in `src/ui.cpp`, which runs MENU's disarm, and BACK there returns
to the tester; the latch reads LATCH. Slice 6, the leave page (decisions 5
and 21, owner decisions 3, 25 and 30): KST SERVO, HITEC SERVO and SERVO
TESTER leave through `ui_leave`, its numbered steps, STAY, LEAVE, then NEXT
SERVO or BACK TO SERVO and RESTART DSHOT in LEAVE's rectangle, live 1000 ms
after the pin is let go; BACK on the page drawn disabled in every phase.
Slice 8: SERVO TESTER titled `SERVO TESTER`, its MENU label, on the main
view and the leave page. Slice 9: MENU's seven rows by device, BEEP THE
ESC, AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG,
SETTINGS, scrolling 36 px, the tool rows without a pin status, RX SIGNALS
drawn disabled as `NOT BUILT` until slice 3 of rx-signals.md put its
chooser behind it; PROGRAMMER (`src/ui_servo_prog.*`), the
chooser of KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC from
`UI_PROGRAMMERS`, which claims nothing and whose tap opens the tool through
`uiOpenTool()` with the entry's own entry point, the claim of SIG on that
frame; HITEC SERVO without its family view, a D series servo on its own
supply waiting in Held for READ (the owner's decisions 34 and 35 of
2026-11-14). Slice 10: the documentation no slice owned, menu.md's tree and
MENU as built, revision lines in the design notes, README's Layout, Tests
and user manual, and rule 9 of `test/check_docs.py`, which fails when
README.md or a wiki page names a screen text, view or image structure B
removed. The owner answered the note's eight questions on 2026-11-14 (open
item 18). The pin
claim both features share is built (slice 1): the AM32 editor and the servo
tester run on it. For the ESC, the board is the throttle source. It sends
bidirectional DShot (a digital ESC protocol in which the ESC answers each
frame on the same wire) and decodes the eRPM (electrical revolutions per
minute) and EDT (Extended DShot Telemetry) frames that come back; RPM
(revolutions per minute) is eRPM divided by half the motor's pole count. An
optional third wire carries KISS telemetry, a serial protocol from the KISS
ESC line with finer voltage and current.

The two cores of the RP2350 split the work by deadline:

| | core0 | core1 |
| --- | --- | --- |
| Work | USB (Universal Serial Bus) device task, the USB bridge's pass, touch, the INA219 current sensor read after touch, the QMI8658 IMU (inertial measurement unit) read after the sensor while the angle meter is open, UI (user interface) rendering, SD (Secure Digital) card logging, settings, serial telemetry dump, the pin claim's state machine | DShot frame pump with its command slot, eRPM and EDT decode, KISS request and receive; while a pin claim holds, the claiming screen's job instead: the servo job for SERVO, the KST job for KST, the Hitec job for HITEC, the S.BUS job for the S.BUS monitor, the bridge's job for USB BRIDGE, none for AM32 |
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
SETTINGS pin change drains the same way. `loop1()` runs `escTaskPoll()` and then
`core1JobPoll()`, which binds, steps and releases the claiming screen's job
(`src/core1_job.h`): the servo job (`src/servo_job.h`) for SERVO, the KST
job (`src/kst_job.h`) for KST, the Hitec job (`src/hitec_job.h`) for
HITEC and the S.BUS job (`src/sbus_job.h`) for the S.BUS monitor, and the
USB bridge's job (`src/bridge_wire.h`) for USB BRIDGE, and the S.BUS
output's job (`src/sbus_out_job.h`), which nothing opens yet; the AM32
transport runs on core0.
Leaving rebuilds the pump from the wiring it had, disarmed. For SERVO, KST,
HITEC and the S.BUS monitor, and for AM32 once an EFM8 bootloader answered,
the claim restarts DShot only from the leave page, whose RESTART DSHOT acts
1000 ms after the pin is let go, once the page has told the user to unplug
the servo, the receiver or the ESC's battery. A claim without the page
restarts DShot once its job is idle: AM32, which has none, within BACK.

Rates and defaults, from `src/config.h` and `src/main.cpp`:

- DShot bitrate: 150, 300, 600 or 1200 kbit/s. Default 600 kbit/s
  (`DSHOT_SPEED_KBAUD`), changeable on MENU → SETTINGS.
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
  ceiling starts at 400 of 2000 (20 %). Idle disarm after 30 s. Opening
  MENU disarms. DShot gives up the signal pin only after at least
  250 ms of zero frames since the last throttle frame.

PIO (programmable input/output) blocks: three, each with 4 state machines and
32 instruction slots.

| Block | Use |
| --- | --- |
| pio0 | backlight on GP16, 20 kHz (`src/backlight.pio`): 1 state machine, 6 of 32 instructions. While a Hitec D series line is open with PRECHARGE receive, its sampler takes 1 more state machine, up to 12 instructions (7 at 150 MHz), built at run time |
| pio1 | SDIO (Secure Digital input/output) card driver in `src/sdio/`: 2 state machines, 32 of 32 instructions, plus 2 DMA channels and no interrupt (`src/sd_hw_config.c`) |
| pio2 | DShot transmitter and receiver on adjacent state machines, 16 instructions; KISS receiver when KISS is on, 1 state machine, 9 instructions. While a KST or Hitec claim holds, DShot and KISS are released and the line program takes 1 state machine: the KST line, 24 instructions, or the Hitec line, 30 instructions |

DShot and KISS claim from whichever block has room, and the Pico SDK
(software development kit) searches pio2 first. DShot binds first after every
release, so it always finds a block with no other user. A `static_assert` in
`src/esc_task.cpp` checks at build time that DShot and KISS together fit one
block: 3 state machines and 25 of 32 instructions. A bind that cannot get two
adjacent state machines is refused, and the status bar then shows `DSERR`.

PWM slices: slice 0 carries the servo
pulses during a SERVO claim, GP0 on channel A and GP1 on channel B, one
frame rate for both, its counter at 1 MHz. The backlight on GP16, also
slice 0 channel A, runs from pio0, so the slice is free. The servo tester
uses no PIO and no DMA.

Board: the image is built for the Waveshare RP2350-Touch-LCD-2.8 only.

Build: native Pico SDK (software development kit) 2.1.1 with CMake, ARM cores
only. The one dependency besides the SDK is pinned by commit:
`no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` v3.6.2. Its FatFs, glue, card layer and SPI
driver are built, the SPI driver because the card layer calls into it; its
SDIO driver is not. The DShot driver in `src/dshot/` is part of the
tree, from rcbench `protocols/dshot` at commit 453cf334 (MIT, same copyright
holder). The servo sweep curve, the GPIO-to-PWM fold, the KST programming
core, its line driver and the KST file store are rcbench files at the same
commit, and the Hitec programming core, its line driver, the Hitec servo file
text and its CRC are rcbench files at commit 3e29dc1b, and the S.BUS
(Futaba's serial bus for receiver channels) decoder and encoder are rcbench
files at commit b88593d3, and the current sensor's bus record,
`sense_bus.c`, is an rcbench file at commit 5ef42efd, and the
escwire core, its line driver and the servo file store are rcbench files at
commit 2b4f934a, and the control-surface arithmetic `servo_rudder`, the IMU
(inertial measurement unit) model `imu` and the WitMotion decoder are
rcbench files at commit e98345aa, all copied unchanged into `lib/rcbench/`
([lib/rcbench/README.md](lib/rcbench/README.md)). The firmware builds the S.BUS
decoder for the S.BUS job, the S.BUS encoder for the S.BUS output's job,
which nothing opens yet, and the escwire core and its line driver for the
ESC settings job; the host suite alone builds `servo_rudder`, `imu` and
`witmotion`, which nothing in `src/` calls yet. The SDIO driver in `src/sdio/`
is part of the tree, MIT, written from the SD Association's Physical Layer
Simplified Specification v9.10 and the RP2350 datasheet
([design note](docs/design/sdio-driver.md)). A release carries one UF2
(USB Flashing Format) image, `MiniRcBench-<tag>-touch-lcd-2.8.uf2`.

## Board

The Waveshare RP2350-Touch-LCD-2.8: an RP2350A, a 240 × 320 ST7789T3 panel, a
CST328 touch controller and a QMI8658 IMU (inertial measurement unit). GPIO
means general-purpose input/output.

| | RP2350-Touch-LCD-2.8 |
| --- | --- |
| Panel bus | SPI1: GP10 clock, GP11 data out |
| Touch | CST328 on I2C1 (Inter-Integrated Circuit), GP6 and GP7 |
| Current sensor | optional INA219 at 0x40 on J3, on the same I2C1 bus; SETTINGS → CURRENT SENSE, `NONE` by default |
| IMU | QMI8658 at 0x6B on the same I2C1 bus; `src/qmi8658.*`, read while the angle meter is open (MENU → TOOLS → ANGLE METER) |
| SD card | PIO SDIO on pio1, 4 bits, 10 MHz, GP19 to GP24 |
| GPIOs free for the two wires | 2: GP0, GP1 (`BOARD_FREE_GPIO_MASK` `0x00000003`), the pins of UART0 (universal asynchronous receiver-transmitter 0), which only the S.BUS jobs use: the monitor's receiving on GP1, the output's sending on GP0, which nothing opens yet |
| ESC signal default | GP0, J4 pin 9 and J5 pin 2 |
| KISS default | GP1, J4 pin 10 and J5 pin 1, off |
| Switched servo supply enable | GP28, J4 pin 11, or GP29, J4 pin 12, under SETTINGS → SERVO SUPPLY; none by default |
| Signal pin level | GP0 and GP1 are "Digital IO (FT)", fault tolerant: 5.5 V while IOVDD, the I/O supply, is at 3.3 V, 3.63 V while it is at 0 V (RP2350 datasheet, Table 1674). GP28 and GP29 are ADC (analog-to-digital converter) pins and not fault tolerant; they carry only the enable, a 3.3 V output |
| Battery sense | GP27, ADC input 1, 3:1 divider |
| Power latch | GP26, asserted first thing in `setup()` |
| Other devices | PCF85063 RTC (real-time clock) at 0x51 on the same I2C1 bus, read once at start-up ([run-summary.md](docs/design/run-summary.md#slice-4-as-built)) and set over the USB serial port ([slice 5](docs/design/run-summary.md#slice-5-as-built)); no cell on its socket J2, so it is set again after each power-up; I2S (Inter-IC Sound) audio codec, not used |

The ESC and KISS pins are changeable on MENU → SETTINGS within the free set,
and the two wires never share a pin.

**Mechanical models of two modules.**
[ina219_cjmcu219.step](hardware/mechanical/models/ina219_cjmcu219.step), the
INA219 current sensor board, 24.2 × 21.9 mm, 225 solids, and
[boost_5v_en.step](hardware/mechanical/models/boost_5v_en.step), the 5V boost
converter board, 13.7 × 21.7 mm, 69 solids, are drawn part by part with the
print of both sides. Their specs are rcbench's, copied unchanged to
`lib/rcbench/hardware/`, and measured from owner photos on 5 mm squared paper:
positions within about 0.2 mm, no dimension verified with a caliper, board
thickness and the inductor's height estimated. The boost board does not name
its enable pad; the one pad besides the 4 corner pads is drawn as it. Its
chip is not known.
`check_models.py` reads both back, outside CI: valid solids, none sharing
volume.

**Mechanical model.**
[hardware/mechanical/models/rp2350_touch_lcd_2_8.step](hardware/mechanical/models/rp2350_touch_lcd_2_8.step)
is a STEP (Standard for the Exchange of Product model data) facsimile of the
board, 50.54 × 73.06 × 10.03 mm, 1039 solids: glass, display stack, board with
its cut-outs, 4 standoffs, the two flat cables, every rear part 1.0 mm or
more long, and the print and bare tin of the rear traced from a photo. Waveshare publishes no 3D file of this board. The stack, the
outline and the connectors are measured in Waveshare's STEP file of the
ESP32-S3-Touch-LCD-2.8, which shares them; the other parts are placed from
Waveshare's rear photo, good to 0.3 mm. No dimension is verified with a
caliper on a board. Sources, limits and commands are in
[hardware/mechanical/README.md](hardware/mechanical/README.md).
`hardware/mechanical/gen_models.py` writes the file from
`touch_lcd_2_8.py`, and its `--check` runs in the Docs job. Read back with
OpenCASCADE (cadquery-ocp 8.0.1.1.0) by `check_models.py`, outside CI: 1039 valid solids, none
sharing volume.

## What exists and how far it is verified

States used below. "Hardware-tested (owner report)": the owner reports it
working on hardware. "Hardware use recorded": a commit records use on a board;
the commit is named. "Reported by a user": a third party reports it. "Host
only": covered by the host suite, no hardware record. "Not recorded": no
record either way.

| Part | Host tests | Hardware |
| --- | --- | --- |
| DShot driver in `src/dshot/` (from rcbench 453cf334): frame, reply decode, three-state poll, RP2350 pin driver | frame and checksum, throttle map, line and group code, sampler against clock error, extended telemetry schedule against an AM32 model, three-state poll and the counters it feeds; the pin driver is compiled only | not tested on hardware in MiniRcBench. rcbench ran it at bidirectional DShot600 on one AM32 2.21 ESC (Skystars 35A); other rates and ESC firmwares are untested |
| Bidirectional DShot throttle, eRPM and RPM readout | core1 frame decision, throttle and arming interlocks, HOLD TO ARM timing only a press that landed while arming was allowed, RPM smoothing | hardware-tested (owner report) with `pico-bidir-dshot`, the driver before `src/dshot/`; the owner has found no bug since DshotDisplay v2.3, and issue #1 is a separate, unreproduced report; the HOLD TO ARM press rule is not tested on hardware |
| The splash's `TOUCH CONTROLLER NOT FOUND` (`touchAnswered()` in `src/cst328.cpp`, `uiSplashTick()`) and the BOARD voltage's colour (`uiBoardCell()`) | against a touch fake that stops answering: the splash line at boot without an answer, kept while none comes, cleared by one; a finger on the glass from the splash on does not arm, and its lift completes a tap on MENU; the real driver against the CST328 model: a probe with another ID records no answer, a finger read that completes records one, `touchInit()` starts the record again; each band edge, the centivolt rounding at 3.50 V and 3.30 V, the 50 mV hysteresis, the text colour below 2.50 V and at 0 V and red again from 2.55 V, drawn through the ADC fake; IDLE's countdown in amber in place of an amber voltage; the KST, Hitec and AM32 write and the KST and Hitec restore refused while red, at the hold and after a flush that ended red, with no file and no frame, and written while amber, below 2.50 V and in the text colour (`testBoardLow()`, `testAm32BoardLow()`) | not tested on hardware; open item 22 |
| EDT telemetry, automatic enable, expiry | merge, expiry, enable rule, decode of each frame type; enable at pump start and decode with EDT on against a simulated ESC, with and without EDT | hardware-tested (owner report) with `pico-bidir-dshot`; the enable at pump start and the decode of every reply with EDT on are not tested on hardware |
| ESC command slot on core1 (`src/esc_task.h`, slice 1 of [esc-commands.md](docs/design/esc-commands.md#slice-1-as-built)): beacons 1 to 5, 12, and 7 to 10 with 12 after them | the frames of a sequence and their telemetry bit, the reply check of frames 1 to 6 of each block, the guards (armed, no answer, eRPM, the 1500 ms settle, the 2000 ms wait), the EDT enable's order, the gaps, the `millis()` wrap, a request dropped by a teardown, a cut sequence keeping its outcome, against AM32, Bluejay and BLHeli_S models read from source | not tested on hardware: no command of the slot has gone to an ESC from this board |
| ESC COMMANDS (`src/ui_esc_cmd.*`, slice 2 of [esc-commands.md](docs/design/esc-commands.md#slice-2-as-built)), MENU's first row, and the tester's 3D note (`src/esc_3d.*`) | through MENU against the fake slot: the row refused while a claim holds, the disarm, the layout and each text's width, every control at each readiness, BEEP 1 to 5, the holds at 999 and 1000 ms with the save, each outcome's lines, the 3D note set by 3D ON at Sent and SaveLost and kept by 3D OFF, BACK disabled while a request waits, BACK and the BEEP taps acting only on a press that landed live, the cancel on leaving | not tested on hardware: no command of the page has gone to an ESC |
| RPM/V and the kV estimate on the armed tester (`src/kv_est.h`, slice 3 of [esc-commands.md](docs/design/esc-commands.md#slice-3-as-built)) | both formulas at the rounding and the 99999 clamp, 64-bit at the decoder's largest eRPM; the 0.5 V floor, the pole count, RPM above 0, the 75 % minimum and 2000 maximum, the 1000 ms settle at 999 and 1000 ms, within a band of ±20 under touch jitter and restarted at 21, and across the `millis()` wrap, the filtered eRPM within 0.5 % of the reply and held once passed and released at 3.0 %, a noisy series showing a steady figure, a reply of 0 and a step past 3 % reading `--`, the 3D note; on the tester: the texts, their colours and widths, a figure only 1000 ms after the throttle stood, KISS over EDT, stale voltage and link, nothing kept after the disarm | not measured on a motor: no figure has been compared with a rated kV or a tachometer |
| KISS telemetry: request, PIO receiver, decode, merge, KISS/EDT tags | CRC, decode, resynchronisation, request payload, merge; the PIO receiver is typechecked only | hardware-tested (owner report) with `pico-bidir-dshot`; see below |
| DShot bitrate label in the status bar, `DSHOT600`, `DSERR` | label at each bitrate, colour, `DSERR` | not recorded |
| SDIO card driver in `src/sdio/`: protocol, PIO and DMA bus, library callbacks | CRC7 and CRC16 against the specification's examples, command framing, response parsing, CSD (card-specific data register), four-bit packing, CRC status token; initialisation, reads and writes against a simulated card (standard capacity SDSC v1 and v2, high capacity SDHC, no card, voltage mismatch, timeouts, removal); the two PIO programs run in a cycle-level model against a card model (`test/pio_sdio_emu.py`: CMD0, R1, R2, single and multiple block reads, CMD12, a write and its CRC status token, at 400 kHz, 10 MHz and 25 MHz), a model and not proof of hardware timing; the bus code in `sd_pio.c` is compiled only, and the model's host side follows it by hand | not tested on hardware; open item 6 lists the bench tests |
| Blackbox logging to SD, MENU → SD LOG screen | encoder, ring buffer, round trip through `blackbox_decode`, screen states; how a log ended (`sdLogWriteEnd()`: a short write with `FR_OK` is a full card, any error a failed write), each end's two note lines and their colour, a USB DRIVE refusal ahead of them, START clearing them, a mount clearing a card fault and keeping a tool's stop (`sdLogMountForget()`), a mount hint under a tool's stop, a mount resetting FILE and the counters beside it, AM32 CONFIG's, KST SERVO's and HITEC SERVO's claims recording the stop, the FILE row's `(LAST)`, `SDERR` on an amber chip while armed, a new log's number (`sdLogNameNumber()`, `sdLogNoteName()`, `sdLogNextNumber()`: the name rule in any letter case, four and six digits, other extensions and long names ignored, `LOG00000.BFL`, `LOG99999.BFL` refusing the start with its note), against the fake logger; the directory, the scan, the kept number and the open (`src/sd_log_next.cpp`) over FatFs R0.15 on a RAM disk, FAT32 and exFAT, with a card swapped or emptied after a fault and a write-protected card (`make -C test lognum`): the clearing, the recording and the forgetting on each error in `sd_log.cpp` run over the same FatFs in `test/sd_log_fatfs.cpp` as far as its run summary tests reach them: a full card and a failed write during the log, a failed sync at the stop, a failed start | hardware use recorded with the previous SDIO driver on the 2.8" board; see below |
| USB card reader (read-only mass storage) | handover state machine, screen; `src/usb_msc.cpp` itself in `test/test_usb_msc_cb.cpp`: `tud_umount_cb()` while serving reclaims the card as an eject does, nothing while idle | not recorded. On the TinyUSB 0.18.0 port a pulled cable reaches the firmware as a suspend, which is not handled (open item 20) |
| AM32 settings editor over the signal wire (AM32 is an open-source ESC firmware) | init string, CRC-16/ARC, 39-field codec, timing advance encodings, write interlock; PROTOCOL zero-based as AM32's `enum inputType`, TEMP LIMIT and CURRENT LIMIT `OFF` where AM32 applies no limit, `3D MODE` at byte 0x12; LOW VOLT CUTOFF, PWM TYPE and BRAKE ON STOP as three values with the third gated by firmware and an unedited byte written back as read; KV, MAX RAMP, MIN DUTY, ABS VOLT CUT and ACTIVE BRAKE on AM32's scales; the strip's notes for 3D MODE, a protection off and a PROTOCOL without DShot, the next step under a write that is not verified, the connect page's lines for a reply it cannot parse and for `NO VALID SETTINGS`, MENU's `POWER-CYCLE THE ESC TO RUN IT` until an eRPM frame after the handshake (`testAm32Guidance()`); the tester's `3D MODE ESC: THROTTLE SCALE DIFFERS` set by a list read and by a write of 3D MODE on that the ESC accepted, its read-back failing or differing included, cleared by a list read and a verified write with it off, kept by an edit not written, a refused write, a failed read, a write of it off not verified, a read that does not open the list and the read-only view of a layout not known, drawn armed and disarmed and beside `NO TELEMETRY`, `SWIPE ACTIVE` in its place while the pad is engaged and the note back on release (`testAm32TesterNote()`); the firmware gates (ABSOLUTE from 2.21, the 200 A limit from 2.21, PWM FREQ above 48 kHz from 2.19), the PROTOCOL note for 2 to 9 (EDT ARM included: a zero frame relocks it), layouts 1 and 5 to 8 opened in the read-only view: the bytes in hex, no field, its strip in each backup state, the backup with no field line saved as it opens, BACK its one control, a hold and taps where the list's controls stand writing nothing, a write forced into the write state refused on the layout (`testAm32ReadOnly()`), bytes 0x2A to 0x2D listed from 1.80, 0x2E from 1.94, 0x2F from 2.16 with a note below 1.80 (AlkaMotors commits 9909ea7, f917c01, aa590e8), ADVANCE in 0..3 below 2.19, every row tapped on 14 firmware versions and three layouts, a stored number outside the firmware's range read as the byte, every field over all 256 bytes on eight firmware versions against a literal table of ranges, arbitrary blocks written back through the screen; the facts behind them read from AM32's source (am32-firmware/AM32 tags v2.05, v2.16, v2.18 to v2.22 and `main` 077b6a4; the configurator at 06955fe; AlkaMotors/AM32-MultiRotor-ESC-firmware tags v1.77 to v2.01), not measured; the screen on the pin claim, the latched GPIO, the drain before the handover; the list on `ui_list` and the swipe of 16 px per coarse step; the backup before the first write (`src/am32_backup.*`): its text and checksum, `BACKUP/AM001.TXT` holding the settings as read at entry, a card that refuses it stopping the write, no second backup in a session | hardware use recorded: ab79c67, bd90e17, both before the screen moved onto the pin claim; the claim and its drain, the swipe of 16 px per coarse step and the backup: not tested on hardware |
| Device backups: the ring of `BACKUP/` (`src/backup_ring.*`), the card calls and the directory scope (`src/card_dir.*`); [backups.md](docs/design/backups.md) | the ring's rules, saves of two devices, a rename under another device's number, the list of one device, gaps, no marker, directories and names outside the ring, refusals, no card and a USB-held card (`runBackupTests`); the card phases over FatFs R0.15 itself (`make -C test kstcount`) | not tested on hardware |
| Blackbox logs in `LOGS/` (`SD_LOG_DIR_PATH`) | the next log's file over FatFs R0.15 (`make -C test lognum`); `sd_log.cpp` itself over the same FatFs on a RAM disk (`test/sd_log_fatfs.cpp`): start, frames, stop, the failures during the log and at the stop; the SDIO driver below it is not in that program | not tested on hardware |
| Run summary core (`src/run_summary.*`), slice 1 of [run-summary.md](docs/design/run-summary.md#slice-1-as-built) | `runRunSummaryTests`: the extremes, sources, charge, armed time, the `.TXT` text byte for byte and SD LOG's row texts and fits; 95 mutations, 91 failing the suite and 4 equivalent, listed in the note | not tested on hardware |
| Run summary in the logger and its `.TXT` (`src/sd_log.cpp`, `src/sd_log_summary.*`), slice 2 of [run-summary.md](docs/design/run-summary.md#slice-2-as-built) | `test/sd_log_fatfs.cpp` in `make -C test lognum`, over FatFs R0.15 on FAT32 and exFAT: the summary fed by every frame and ended by STOP, DISARM and each tool, the `.TXT` beside its `.BFL` as `runSumText()` builds it, a stray `.TXT` replaced, the next log's number unmoved, the `.TXT` not saved on a full card, in a full directory, by a failed write and a failed close with the logger `READY` and the `.BFL` byte for byte, the log failing during the run, at the stop's sync and in the stop's drain with more than a chunk in the ring, the summary ended by STOP at the stop's start; the summary forgotten by a mount and by USB DRIVE's release; 56 mutations, 49 failing it, 6 equivalent and 1 not killed, listed in the note | not tested on hardware; the cost per frame on core0 is estimated from the compiled code, not measured |
| Run summary on SD LOG (`src/ui_sdlog.cpp`), slice 3 of [run-summary.md](docs/design/run-summary.md#slice-3-as-built) | `testLogRun()`, `testTesterNoRun()` and `testLogEndNotes()` in `test_ui.cpp`, against the fake logger: rows 2 to 8 after STOP with each row's value, tag, colour and column, `--` for unknown values, EDT and MIXED tags dim, MAX TEMP red at 90 °C and not at 89 °C, both `NOT SAVED` texts red, `USED FROM CURRENT`, MAX RPM from KISS, the widest values, the counters, CARD and MOUNT hidden, MOTOR POLES at the stop; no summary before the first log, while logging, after a mount and after USB DRIVE; a failed start keeping the run; CARD FULL showing it not saved; each part of the redraw key; the tester's frame the same with and without a summary; 41 mutations, 40 failing the suite and 1 equivalent, listed in the note | not tested on hardware |
| Clock driver and FAT dates (`src/clock.*`, `get_fattime()`, the `.TXT`'s clock lines), slice 4 of [run-summary.md](docs/design/run-summary.md#slice-4-as-built) | `runClockTests` against a register-level PCF85063A model on the I2C fake: BCD, the calendar over 2000 to 2099, the decode of each cause of a clock not set, the boot read of a set clock in one 129-clock transaction with nothing written, the kept time across a minute, a year's end, a leap day and 2^32 µs, the software reset and CLKOUT off only with OS set, nothing written with STOP, EXT_TEST, 12-hour mode or a bad register, no part, a timeout, an aborted or short read, a held SDA and a stuck bus, `get_fattime()` 0 while not set and packed while set; the clock lines in `runRunSummaryTests` and through the logger in `test/sd_log_fatfs.cpp`, read once at the start and once at the stop; 95 mutations, 94 failing them and 1 equivalent, listed in the note | not tested on hardware: the part at 0x51, OS after a power-up, the reset, the read's timing, any file's date on a card |
| Setting the clock (`clockSet()` in `src/clock.cpp`, the `CLOCK` line in `src/clock_line.*` read by `usbDevTask()`, SETTINGS → BOARD → CLOCK, `tools/set_clock.py`), slice 5 of [run-summary.md](docs/design/run-summary.md#slice-5-as-built) | `runClockTests` on the PCF85063A model: the set's four writes byte for byte, 308 clocks with the read-back, the part counting from STOP's release, the kept time and the count of sets, ten bad times refused with nothing sent, each of the six transfers failing with the kept time unchanged, a timeout, a stuck bus, a short write, four read-backs that differ, each stopping the part; the line's parser on good, bad, short, long and ignored lines, the reader one line a pass over CR, LF and CR LF, each answer, each answer echoed back and ignored, the reader off and on, a set during a log giving `(clock set during the log)`; `testSettingsClock()` in `test_menu.cpp`: the row's texts and colours, repainted at a set and when the minute turns, not within it and not while scrolled out; `test/test_set_clock.py`: the port by USB IDs, the line, the wait for the next second, the answers past telemetry, the timeout, a half line another program left, a port lost during the exchange, `--help`, `--list`, `--port`, `--query` and the exit statuses; 156 mutations, 150 failing them and 6 equivalent, listed in the note | not tested on hardware: the set on the part, the serial line through TinyUSB (`usb_dev.cpp` is typechecked only), the tool against a board on Windows, Linux or macOS |
| SD write cache for the backup claims (`src/sdio/sd_cache.*`, `src/card_cache.*`, the glue in `src/sdio/sd_card_glue.c`, `sdh_write_start()` and `sdh_write_poll()`); [sd-write-cache.md](docs/design/sd-write-cache.md) | the cache against a fake card (`runSdCacheTests`), the asynchronous block write against the simulated card (`runSdioTests`); the KST write waiting for the flush, a failed flush, the claim turning the cache on and off, a running log stopped, AM32's write refused after a failed flush, against a fake of `card_cache.h`; the glue and `card_cache.cpp` are built by the firmware only | not tested on hardware |
| Pin claim (`src/pin_claim.*`) and core1 job dispatch (`src/core1_job.*`), the zero-throttle drain in `src/esc_task.*` | each refusal rule at its 500 ms boundary and their order; the handover decisions of `escTaskPoll()` (`escHandoverStep()`, `escDrainSlot()`, `escPumpFrameSent()`, `escPumpBind()`) one 1 ms slot at a time: 250 zero frames, the teardown 251 ms after the last throttle frame, the bind disarmed; the order of the two cores' steps against fake jobs, BACK and RETRY, Restoring, the unplug-page rule, the restore wiring, the start guard, the heartbeat rule across a millis() rollover; the PIO and pad calls around those decisions are compiled only | not tested on hardware; open item 8 |
| MENU layout (`src/ui_menu.cpp`, `src/ui_registry.cpp`): ESC COMMANDS, ESC CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG and SETTINGS as 40 px rows, scrolled 36 px to reach SETTINGS | every registry entry opens and BACK returns to MENU; opening MENU disarms; ESC COMMANDS the first row, with no status; the rows drawn disabled and `DSHOT RESTARTING ON SIG GP0` while the claim restores, and a tap on a row opens nothing; over test registries, six rows do not scroll and seven scroll 36 px; SD LOG, the sixth row, is whole and takes a tap at 0 px, and SETTINGS, the seventh, is cut to 6 px until MENU is dragged 36 px; MENU opens at 0 px again when a tool returns; the UNSAVED status on MENU after leaving SETTINGS, its row revealed by a drag; the label and status fit check at build time | not recorded |
| Servo tester: the pure model (`src/servo_model.*`), the PWM driver (`src/servo_pwm.*`), the core1 job (`src/servo_job.*`), the slider and the screens (`src/ui_slider.*`, `src/ui_servo.cpp`); rcbench's sweep and PWM fold in `lib/rcbench/` | profiles, limits at their boundaries, steppers to their stops, command, angle and pulse mappings, SPEED, the dial's hit area; the PWM driver's bind, write, release and sharing rules against a register fake; SAFE at level 0, CENTRE at START, STOP on the next pass, the slew, sweeps, REVERSE B, the trip at 250 ms across a millis() wrap, kept across a rebind, the profile rebind; no pulse cut short at a release or a rebind, against a model of the counter and the latched compare; released pins pulled down; the screens through the UI: HOLD TO START at 1000 ms, the drag rules, the SET warning and HOLD TO APPLY, a TYPE, MIN or MAX change further past the travel and SERVOS ONE to TWO past it held for HOLD TO APPLY, a step back or toward the centre and TWO to ONE applied at once (`servoTravelNeedsHold()`), a pending TWO dropped to one servo when the claim takes SIG alone, a wait that held only for TWO ended and a heli, 60 Hz or travel wait beside it kept, the end-stop note and its place among SET's notes, STOP on SET, the AUX refusals through the claim, the leave page; rcbench's own sweep and fold tests; the SDK calls are run against the fake only | not tested on hardware; open item 9 |
| KST programming: rcbench's session, line driver and file store (`lib/rcbench/`), the search (`src/kst_seek.*`), the core1 job (`src/kst_job.*`) and its line (`src/kst_line_port.*`), the files on the card (`src/kst_files.*`), the screen (`src/ui_kst.cpp`, `src/kst_view.*`) | rcbench's seven tests, 298 cases, the line's PIO program among them in a model (`test_kst_pio.c`); the search and the job against rcbench's simulated servo, the files against an in-memory FAT (File Allocation Table) volume, the backup in `BACKUP/` and SAVE AS in `KST/` up to 50 files among them, the card phases' block counts over FatFs R0.15 itself (`make -C test kstcount`), the screen through MENU against both (`runKstTests`); the line driver and `kst_line_port.cpp` are built by the firmware and typechecked only | not recorded: no frame of the line driver has been on a pin (`kst_line.h`) |
| SETTINGS layout (`src/ui_settings.cpp`): sections ESC TESTER, WIRING, SERVO SUPPLY, DISPLAY, BOARD and DIAGNOSTICS in one scrolling list | the scroll range, 0 px at opening and 620 px at most, 632 px while the ESC pin's warning shows; each row by id; no scroll from a press on a control; a cut control takes a press only while 40 px show; HOLD TO SAVE in place at every scroll position and saving at the end of the list; the pole count reaches the eRPM divisor, the ceiling steps by 5 %, and HOLD DEFAULTS restores both; HOLD DEFAULTS loads nothing on a tap, at 975 ms or after a slide off, loads at 1000 ms once per press, and draws its bar, its `C_RED` fill and its two-line label (`testResetHold()`); the footer at 6/150 and 162/72, y 268, 46 px, and HOLD TO SAVE's `C_RED` fill | not recorded |
| Restart reason and uptime (`src/diag.*`), SETTINGS → DIAGNOSTICS: LAST RESTART and UPTIME; [diagnostics.md](docs/design/diagnostics.md) | the decoding of every CHIP_RESET flag alone and in pairs, the flags of resets that leave the power manager alone before those that reset it (POR and GLITCH read GLITCH), the watchdog before them: enable caused, REASON with a flash update boot (UPDATE) and with any other boot type or an error (REBOOT); every text within 10 characters; the capture through fakes of `powman_hw->chip_reset`, `watchdog_hw->reason`, `watchdog_enable_caused_reboot()` and `rom_get_last_boot_type()`; the uptime's text up to 1193046:28:15; the rows' place, text and each reason's colour, no control on either, UPTIME repainted once a second while it shows, not within the second and not while scrolled out; the SDK bit values held by `static_assert` in the firmware build | not tested on hardware; open item 17. No watchdog is armed, so `WATCHDOG` cannot appear |
| Settings in flash (MENU → SETTINGS) | block validation, CRC, board id check, pin defaults on GP0 and GP1 and not GP28 or GP29, the version 6 block layout with CURRENT SENSE at byte 13 and the angle meter's fields from byte 14, the servo supply clamp at 2, the current sensor clamp at 1, the angle fields' clamps at their boundaries; blocks of versions 2, 3, 4 and 5 built byte by byte from the layouts in git and carried over (`settingsCarriedOver()`): each field, the newer ones at their defaults, version 3's GP0 supply as none, validation of out-of-range values, a damaged CRC, payload byte or magic and another magic with its CRC right discarded with no notice, a size field of another layout taking the reset path, flash untouched until a save, a save with nothing changed writing a version 6 block that the next boot loads with no notice; `settingsSaveAngle()` writing the angle fields over the saved settings, a SETTINGS change not saved left unsaved, nothing written for an equal block, refused during a claim; the splash's two lines, `UPDATED` on MENU and the note line for it; a reset by an update (`settingsResetByUpdate()`) for versions 1 and 7 and for a version 2, 3 or 4 block of another layout's size, not for blank flash, a bad CRC, a wrong size of this version or a payload over `SETTINGS_OTHER_MAX`, its CRC at the next 4-byte boundary, ended by a save that lands and not by one the flash refuses; the splash's two lines, `UPDATED` on MENU and the note line; the ESC pin's warning in SETTINGS against the boot pin, on its own line at y 230 beside UNSAVED, SAVED, SAVE FAILED and DEFAULTS LOADED, the list ending at y 227 and scrolling 632 px while it shows | not recorded; 46a64a7 states no hardware coverage |
| KST switched servo supply with its enable on GP28 or GP29 (`src/servo_supply.*`), the SERVO SUPPLY row, the switched search, REBOOT, the close rule, the lapse rule and the card phase (`src/kst_seek.*`, `src/kst_job.*`, `src/ui_kst.cpp`, `sdh_wait_hook` in `src/sdio/sd_host.h`) | the enable pin untouched with `NONE` from boot; with `GP28` or `GP29` an undriven input (SIO, output off, pulls off, input off) at boot and at every SETTINGS change, a change from GP28 to GP29 switching only GP29; low at bind, high and low between bind and release, an undriven input after release; the mode to pin map; the row stepped NONE, GP28, GP29, applied at once, saved and reset; against a model of the converter and a servo with a power-on window (`test/kst_supply_sim.h`): found on the first power-up, 14 power-ups and the timeout, off 2000 ms before every switch-on, the entry delay at -40 ms and +300 ms, a stop, a lapse and the timeout in a wait, REBOOT's 2000 ms off and 1000 ms on, refused while busy and without a switch, a lapse in the mode, during an operation and in REBOOT's off wait, the close 2000 ms after the switch-off, time off before a leave counted, the enable pin untouched with `NONE`; the screens: the switched search, the leave page and HOLD TO REBOOT, the leave page's 2 s countdown, the stalled screen and RETRY; the card phase: `CardBegin` and `CardEnd` in each job state, stamps holding the supply on for 900 ms and off 250 ms after the last, the 15,000 ms cap at 14,900 and 15,100 ms, closed by a command and by LEAVE, the first write's 300 ms card calls inside a phase with and without stamps, the hook null with `NONE`, the busy wait calling the hook | not tested on hardware |
| Hitec programming: rcbench's Hitec session, register model and line driver (`lib/rcbench/`), the core1 job (`src/hitec_job.*`) and its line (`src/hitec_line_port.*`), the files on the card (`src/hitec_files.*`), the HITEC SERVO screen (`src/ui_hitec.*`, `src/hitec_view.*`) | rcbench's 115 cases; the job against a simulated servo (`runHitecTests`); the deepest stack chain of the job on the Cortex-M33 checked in CI against 1,792 bytes; the files against an in-memory volume and the card phases' block counts over FatFs R0.15 (`make -C test kstcount`); the screen through MENU and PROGRAMMER by control id, the claim on SIG on the entry's tap with the job started at once for CLASSIC and the switched supply and deferred to READ for a D series servo on its own supply (`testDeferredJob()`, `testEntry()`) | not tested on hardware; CLASSIC needs SERVO SUPPLY at GP28 or GP29; the classic line voltage not measured |
| S.BUS monitor's core1 job (`src/sbus_job.*`, slice 2 of [rx-signals.md](docs/design/rx-signals.md)), its pin user `PinUser::Sbus` and `CORE1_JOB_NO_GP1`; rcbench's decoder (`lib/rcbench/protocols/sbus/`) | the core on a line model of a receiver: nothing counted before 2000 µs of quiet line after the bind, clean streams at 7 ms and 14 ms, the mean of the last 16 intervals and its restart after 100 ms, the flag byte and its counts, parity, framing and break bytes counted and not fed, overrun counted and fed, footers, a cut frame, the 2000 µs gap, the micros() wrap; the job through the claim on a UART0 fake: GP1 on the UART function with its input inverted and the pull-down, GP0 untouched, 100,000 baud from 150 MHz, 8 data bits, even parity and 2 stop bits, drains every 1000 µs, a pass 2000 µs late splitting a frame, the FIFO's overrun, the heartbeat ignored, each wiring, the NO GP1 and CLOCK refusals and RETRY, KISS live, an ESC on SIG, the claim held, the release in one pass leaving GP1 a plain input and the status cleared, a bind 1 ms into a live frame with the 8N1 window counting nothing, counts at 0 on a new visit | not tested on hardware: the inversion on the pin, a receiver's level and frames, the UART's error flags on a real line |
| S.BUS MONITOR and RX SIGNALS, slice 3 of [rx-signals.md](docs/design/rx-signals.md#slice-3-as-built) (`src/ui_sbus.*`, `src/sbus_view.*`, `src/ui_rx_signals.*`, the RX SIGNALS row in `src/ui_registry.cpp`, `uiChip()`'s label on amber in `src/ui_widgets.cpp`) | the view's link state, hold, ink, bar and texts at their boundaries; the chooser through MENU, OUTPUT disabled; the monitor on the UART0 fake through the claim: frames at 14 ms and 7 ms, every bar and value, the chip in each state, failsafe, a receiver gone, a refused footer, error counts, the leave page and BACK TO MONITOR, RESTART DSHOT; GP1 as SIG with the screen's ESC check; KISS on GP1; no GP1; the clock and RETRY; the row refused while the claim is not Free; 33 mutations of the view, all failing the suite, and 48 of the screen, the chooser, the registry and the chip, 44 failing, one equivalent, three on code later removed as redundant, listed in the note | not tested on hardware: a receiver's frames on GP1, the screen's reading of them on the panel, the pass time with the job |
| S.BUS output, slices 1 and 2 of [sbus-output.md](docs/design/sbus-output.md#slice-2-as-built): rcbench's encoder at b88593d3 (`lib/rcbench/protocols/sbus/`), the core1 job (`src/sbus_out_job.*`), `PinUser::SbusOut`, `Core1Job::SbusOut` and `CORE1_JOB_NO_GP0`; nothing opens it | `test_sbus.c`, 19 cases; `runSbusOutJobTests` on the UART0 fake's transmitter and the GPIO fake's control and pad registers: the ceiling at 100, 400, 1000 and 2000 and START's ceiling held to MAX THROTTLE's on core1 whatever the screen sends, the roles of each wiring, the START frame byte for byte for each throttle channel whatever was sent before, every channel clamped to raw 172 to 1811 and the throttle to its ceiling, frames at 14,000 µs with passes every 333 µs, a pass 30 ms, one period and 13 ms late, never two under 7,000 µs apart, the FIFO's wait, the micros() wrap, the trip at 250 ms and not 249 ms, START refused while lapsed, STOP on the next pass and a STOP never replaced, a refused frame; the bind: SIO's output off and low first, from SIO driving high or the KISS receiver's NULL function, the pull-down before `uart_init()`, 8E2, RXE off, one control-register write with FUNCSEL UART and OUTOVER inverted after the pad's IE and before its ISO clear, GP1 untouched; the line read back by rcbench's decoder, back to back inside a frame and 11,000 µs between; the release after the last stop bit, SIO's output enable off and low before the pad leaves the UART, the pad on SIO before `uart_deinit()`, the pull-down, the 10 ms backstop; each wiring, NO GP0, CLOCK and RETRY, the ESC check in both wirings at 499 and 500 ms, KISS on AUX GP0, a new visit SAFE; no code line under `src/`, read recursively with comments blanked, but the job's declarations, its table line and the claim's row names it; 98 mutations of the job, 94 failing the suite, four surviving and listed in the note, and 10 of the copied encoder, all failing `test_sbus.c` | not tested on hardware: the inversion and the level on GP0, the bind's and the release's edges, the frames on a scope or an FC, core1's pass time with the job |
| Bluejay and BLHeli_S, slice 2 of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-2-as-built): the greeting's answer classified (`src/bl_answer.*`, `am32BlAnswer()`), an EFM8 answer named on ESC SETTINGS' connect page, the run cue it sets, the leave page per claim (`pinClaimWantLeavePage()`, `pinClaimLeavePage()` in `src/pin_claim.*`) and ESC SETTINGS' leave page of the note's decision 8 (`src/ui_am32.*`), the ESC CONFIG chooser (`src/ui_esc_config.*`, `UI_ESC_CFG_ENTRIES` in `src/ui_registry.cpp`), the titles ESC SETTINGS, ESC HEX and ESC WRITE | the classification of AM32 answers of three families and of EFM8 answers with each signature, with an echo, and of answers not understood; the leave page per claim through `pinClaimEnd()` and RESTART DSHOT; the EFM8 answer through the screen: its line, nothing read, the cue, the leave page's steps, STAY, LEAVE, RESTART DSHOT at 1000 ms, the cue cleared; the leave page from every BACK that would leave after an EFM8 answer, the connect page, the list's guard and the read-only view, STAY back to each, BACK on ESC HEX to the list; an AM32 claim with no EFM8 answer giving DShot back within BACK; the chooser through MENU, USB BRIDGE disabled, the claim on ESC SETTINGS' tap; 39 mutations, 38 failing the suite and one equivalent, listed in the note | not tested on hardware: no Bluejay or BLHeli_S ESC has answered the greeting on this board, the classification of a real answer and the leave page's steps are not verified on an ESC |
| Bluejay and BLHeli_S, slice 3 of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-3-as-built): the connect cycle (`src/greet_cycle.*`), the loose EFM8 rule (`src/bl_answer.*`), the ESC settings job on core1 (`src/escwire_job.*`, `src/escwire_port.*`, `Core1Job::EscWire`), the page's rows and texts (`src/efm8_view.*`), the read-only list, ESC HEX, the strip notes, the tester's 3D note, the refusals on the connect page and LEAVE's `00 01` in `src/ui_am32.*` | the cycle's phases and counts, alone and through the screen; the job against rcbench's simulated ESC: bind, entry, read, leave, refusals, heartbeat and release; the rows, values and texts and their fits; Bluejay 208, BLHeli_S 33 and the read-only lists through the screen, BACK sending `00 01` before DShot returns with the run cue kept until eRPM, a piece of an EFM8 answer bringing the leave page and noise not, a refused bind putting the pull-up on SIG, the refusals, a stopped session greeted again, the leave page from each BACK that needs it; 70 mutations, 65 failing the suite, 4 equivalent and one on a check the code does not have, listed in the note; the job's deepest stack chain, 408 bytes of 1,792 with the line driver's 112 bytes behind `ew_driver_t`'s pointers added, from the firmware build; `loop1()` and `core1JobPoll()` are not in it, and SDK calls count 0 bytes | not tested on hardware: no frame of the escwire job has reached an ESC; the entry, the read, LEAVE's effect, the gap between the AM32 transport and the job's bind and the connect cycle's entry times are not verified on an ESC |
| Bluejay and BLHeli_S, slice 4 of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-4-as-built): the job's Write (`EwJobOp::Write`, `escwireJobWrite()` in `src/escwire_job.*`), the backup as `BJnnn.TXT` or `BHnnn.TXT` (`src/efm8_backup.*`, `src/backup_ring.*`), the editable list, ESC WRITE, its results, READ AGAIN, the guarded BACK on a half-written page and the read-only lists' backups (`src/efm8_view.*`, `src/ui_am32.*`) | the job's write against rcbench's simulated ESC: rcbench's order of frames, Bluejay on E8B2 and E8B5 and BLHeli_S, the guard refusing tags, marker, revision, shown fields, melody, values and lengths, every read-only page refused with nothing sent, a page changed before the erase, the ESC gone before and after the erase, programs that do not take, a corrupt byte, erases the ESC does not hear, a driver that fails, heartbeat lapses before and after the erase, the release in a write; after every write the simulated ESC's flash outside the settings page as before and every erase and program on its address; the editing, the backup text read back by rcbench's parser, the results' lines and their widths; the whole write through the screen, a card that refuses the backup, the BOARD voltage, READ AGAIN, the half-written page restored with its tags, BACK aborting with no LEAVE and no DShot until RESTART DSHOT, sessions ending half written keeping the old page on `OLD PAGE`, the simulated ESC's firmware and bootloader patterned so that an erase there shows; 78 mutations, 69 failing the suite, 6 equivalent and 3 backstops the suite cannot see, listed in the note; the job's deepest stack chain, 408 bytes of 1,792 as before, the write's chain shallower than the text interface's, from the firmware build | not tested on hardware: no write has reached an ESC; the erase and program times, the window in which a power cut loses the tags, and what the firmware does on LEAVE after a write are not measured |
| USB bridge, slice 3 of [usb-bridge.md](docs/design/usb-bridge.md#slice-3-as-built): the mode from the PC's first byte, MSP answered as a flight controller named `MRCB` (`src/bridge_msp.*`), the 4-way interface for the one ESC on SIG on escwire's bootloader frames (`src/bridge_4way.*`), its writes held to passthrough's BACK and note rules, the 3D note from AM32 settings pages in both modes, `4-WAY INTERFACE` and the motor refusal on USB BRIDGE | `runBridgeProtoTests`, 215 checks in 13 sections against a model AM32 bootloader that drops data joined to SET BUFFER's header and a model BLHeli_S: ESC-Configurator's and the AM32 configurator's sequences with MSP readers that fail past the end, the recovery frames, every 4-way command, the signature at its bounds, 4 ms of idle line before SET BUFFER's data, ERASE and PROGRAM answered 1 ms inside and outside their waits, BACK and the unfinished write, a port close during a write, malformed and partial frames, passthrough's 3D tap, passthrough after a 4-way or MSP session in one open; the modes in `runUsbBridgeTests`; `testFourWay` in `runBridgeUiTests`, which has 150 checks in 7 sections; 137 mutations, 133 failing, one showing an abort call redundant, three surviving, listed in the note | not tested on hardware: no desktop tool has spoken MSP or the 4-way interface to the board, no bootloader frame of it has reached an ESC; the tools' acceptance of the board's answers is not verified |
| USB bridge, slice 2 of [usb-bridge.md](docs/design/usb-bridge.md#slice-2-as-built): passthrough on core0 (`src/usb_bridge.*`), the console switch (`usbDevConsole()` in `src/usb_dev.*`), the traffic log as a build option off by default (`src/bridge_log.*`), USB BRIDGE (`src/ui_bridge.*`) opened from ESC CONFIG, BACK disabled while a write waits, the leave page at every BACK of the running bridge, MENU's run cue from the claim's begin and the open, the clock's serial line off while the bridge holds the port | `runUsbBridgeTests`, 170 checks on a CDC fake with the real `usb_dev.cpp`, the line fake and the claim: DshotDisplay's bridge tests ported, models of the Python tool's 25 ms reads and the AM32 configurator's Direct rule, the gather, the modes, the write rules, BACK with the PC connected, the console from the open to Free, the run cue from the claim's begin and the open kept through a RUN, RUN only with its CRC, a `CLOCK` line going to the wire and setting nothing, no frame handed to the wire outside the claim; `runBridgeUiTests`, 139 checks through the UI; `test_bridge_log`, 25 checks with the log built on; 70 mutations, 66 failing, four surviving, listed in the note | not tested on hardware: no byte from a PC has reached an ESC through the board, no desktop tool has been run against it; whether each tool sets DTR, and core0's longest pass during the bridge, are not measured |
| USB bridge, slice 1 of [usb-bridge.md](docs/design/usb-bridge.md#slice-1-as-built): `PinUser::Bridge` and `Core1Job::Bridge` (`src/pin_claim.*`, `src/core1_job.*`), the job, frame slot and ring (`src/bridge_wire.*`), the PIO line (`src/bridge_line.pio`, `src/bridge_line_port.*`), a claim without the leave page restoring DShot only once its job is idle | `test/pio_bridge_emu.py`, the transmitter and `pio_uart_rx` in a model of two state machines on one pad, 42 runs: each byte's bits, the pad driven through a frame and let go after it, the echo in the FIFO before the transmitter idles, an ESC's reply at 8 phases and 2 % and 3 % off the rate, a collision, a full FIFO, and a control run that fails; `runBridgeTests`, 162 checks on a line fake: the divider, the slot, the frame whole with passes 1 ms and 5 ms apart, the echo kept, dropped, differing and lost, overruns, the ring's wrap, the release, the claim from Stopping to Free with DShot never bound while the line is open, Free with only `pinClaimState()` called after BACK, the leave page given and kept, RETRY; 42 mutations, 38 failing, one showing a redundant line, removed, three equivalent on the host, listed in the note | not tested on hardware: no byte has been on SIG from this line; `bridge_line_port.cpp` is compiled only; the pad reading itself while driven and the receiver's rate tolerance on a real ESC are not measured |
| INA219 current sensor on J3, the shared I2C1 bus (`src/sense.*`, slice 1 of [current-sense.md](docs/design/current-sense.md)): the codec, the probe, the reading on core0 after the touch read, the read-back, the recovery, the `SenseSample` published for core1, which the servo job reads (slice 3); rcbench's `sense_bus.c` (5ef42efd) in `lib/rcbench/shared/sense/`; the touch driver's transfers noted; SETTINGS → CURRENT SENSE; the readout on SERVO TESTER, KST SERVO and HITEC SERVO | against an I2C fake with an INA219 and a CST328 model (`test/fake_i2c.*`, `runSenseTests`): 19EFh written at the probe, the shunt code to µA on 0.1 Ω at ±1, ±31999, ±32000 and beyond, the bus code to mV, the clip, a negative current; 399Fh and 19EFh accepted as an INA219, 4127h, 7127h and any other value as another part read every second, nothing but the register pointer written to it over 3000 ms, an INA219 put in its place found, FFFFh as none, a refused set-up; absent and probed again 1000 ms on, not 999 ms; three NACKs to offline, two timeouts to stuck, a held SDA, a recovery at once and every 100 ms, a part freed by its clocks, touch transfers noted, a touch abort with SDA held as a line low and the recovery run, a short count through the wire; the read-back once a second, a power-on reset found within two, the reset flag; 220 clocks for the probe, 96 for a reading, 84 for touch; the worst bus time of a frame in each failure mode and with one timeout after each count of transfers that answer, with timeouts moving the clock: the sensor at most 1480 µs, the probe's last data read timing out (1335 µs with the 19EFh write timing out, 1290 µs in a frame with the read-back, 1170 µs in a reading), touch at most 2073 µs, its data read timing out; a read's abort with SDA low for the block's own STOP a NACK, not a line held low; the recovery's START and STOP; the sample's 250 ms age, not at 249 ms, across a `millis()` wrap; the readout's texts and widths; touch read every frame with the sensor present, absent and failing; the SETTINGS row by id, saved, reset, nothing sent to 0x40 with NONE; the readout on the three screens | not tested on hardware: no INA219 has been on the board's bus; open item 16 |
| Angle meter's rcbench files, slice 1 of [angle-meter.md](docs/design/angle-meter.md#slice-1-as-built): `servo_rudder`, the IMU model `imu` with `imu_feed_angles()` and the WitMotion decoder (`lib/rcbench/`, rcbench e98345aa), called by the angle model (slice 3) and built by the firmware with it | rcbench's tests copied unchanged: `test_servo_rudder.c` (4 cases), `test_imu_angles.c` (7) and `test_witmotion.c` (14); 12 mutations of the copies, each failing a test or the build; the `imu.h`-only cases of rcbench's `test_imu.c` (9 of 9) and `test_imu_filter.c` (10 of 11), adapted in slice 3, and 8 mutations of `imu.c`, each failing one of them | host only; nothing of it has run with the board's QMI8658 |
| The wiring sheet's current sensor and boost converter, slice 2 of [current-sense.md](docs/design/current-sense.md#slice-2-as-built): `^` on J3's I2C pins in the connector table of `src/board_rp2350_touch_lcd_2_8.h`, J3's roles and part C in `tools/make_wiring.py`, `docs/wiring.svg` | `tools/make_wiring.py --check`: the drawing against the sources; `sensor()` refusing the marker on no connector and on two, GP0 in place of J3's GND, an unmarked bus pin on J4, a marker off the bus, another SCL pin, and the address at 0x1A, 0x78 and 0x07 | not on hardware: no INA219 and no boost converter has been wired to the board; open item 16 |
| The servo tester on the switched supply, slice 3 of [current-sense.md](docs/design/current-sense.md#slice-3-as-built) and decision 20 of [servo-tester.md](docs/design/servo-tester.md): `servoPowerStep()` and the job's enable pin in `src/servo_job.*`, `touchAnsweredMs()` in `src/touch.h` and `src/cst328.cpp`, the strip's phase line, the notes and SET's start-up time in `src/ui_servo.*`, part C's title and notes in `tools/make_wiring.py` | `runServoTests`: the rules alone at each boundary (499 and 500 ms, 4499 and 4500 mV, 1000 and 1001 mV, 2500 and 2501 mV, 1999 and 2000 ms, a stale reading, a wrap); the job through the job table on the fake pads: the enable pin low at the bind and never an output while high, high at START, the pulses at 500 ms or at the first reading of 4.5 V after the switch-on, low at STOP, STOP while starting, the heartbeat trip at 250 ms, a touch controller silent for 250 ms, a stuck bus, a supply that did not come up, BACK's first pass, an undriven input after the quiet wait and after DShot's restart, undriven after a refused bind, low across a rebind; START refused with the sensor declared and absent, touch silent or the bus stuck; NotOff and Off read back; GP28 and GP29 untouched with OWN; the screens with a fake INA219 following the pin; the order of the writes, EN low before level 0 and the enable pin an input before SIG leaves PWM; BACK while the supply comes up; a reading 18 ms after the switch command not counted, 19 ms counted; STOP redrawn at a cut while the supply comes up; `runSenseTests`: the touch stamp; 83 mutants, 79 failing the suite, 4 equivalent, listed in the note | not tested on hardware: the enable pin from reset and in BOOTSEL, a converter's start-up time and decay (B6), the stuck bus on the board (B3), the trip with the supply on (B11); open items 15 and 16 |
| The cuts on the current sensor's readings, slice 4 of [current-sense.md](docs/design/current-sense.md#slice-4-as-built): `src/supply_guard.*`, the guard in `servoPowerStep()` (`src/servo_job.*`), in `src/kst_job.*` and `src/hitec_job.*`, the chips and notes in `src/ui_servo.*`, the cut texts in `src/ui_kst.cpp`, `src/ui_hitec.cpp` and `src/ui_widgets.h` | `runSupplyGuardTests`: each rule at its boundary (2999.999 and 3000 mA of either sign, a clipped reading, 1000 and 1000.1 mA, 399 and 400 ms, 3999 and 4000 mV, 999 and 1000 ms, the start-up reading at 4499 and 4500 mV and 18 and 19 ms, 249 and 250 ms without a reading, a gap of 249 and 250 ms), a stale reading and a reading seen twice never counted, the clock's wrap; `runServoTests`: the cuts in `servoPowerStep()` with the switch and with OWN, and through the job table on the fake INA219, the enable pin low and level 0 in the pass that reads the cut and in that order, a failed read and two in a row without a cut, the part unplugged, the chips, notes and caption; `runKstTests` and `runHitecTests`: the hard cut during a read, a write, a search's entry, REBOOT's on wait, a leave's write and the release, the collapse at 1000 ms, no stall rule there, no switch-on until RETRY, the close 2000 ms after the cut, the Hitec transaction cut, the screens' texts; 137 mutants, 130 failing the suite, 3 equivalent and 4 not killed, listed in the note | not tested on hardware: the time from a step of current to EN low (B9), the stall cut at MIN and MAX on the owner's servos (B8); open item 16 |
| The move timing, slice 5 of [current-sense.md](docs/design/current-sense.md#slice-5-as-built): `src/move_timer.*`, fed by the servo job (`src/servo_job.*`) after every rule has decided the pulses; `ServoCmd::step` from MIN, CENTRE and MAX and SET's line in `src/ui_servo.*`; rcbench's `servo_move.{c,h}` and `servo_sim.{c,h}` (5ef42efd) in `lib/rcbench/shared/servo/` | `runMoveTimerTests`: the rest at 3560 and 3561 ms after a change and 500 ms after an arrival, 9 and 10 readings, a reading 500 and 501 ms old, -21 and -20 mA, the threshold from the noise, the window at 3060 and 3061 ms, late and unseen, the arrival and the settled rule, the destination's level, CENTRE's idle level and the levels' slots, the hold at 9 and 10 readings and closing 500 ms after the arrival, a step at a lower SPEED, a change, STOP, the sensor offline, 249 and 250 ms without a reading, CURRENT SENSE at NONE, readings counted at their time taken, clipped readings, a peak and a hold below 0, a bind, the line's texts and width; `runServoTests`: the job through the job table with rcbench's servo model behind the fake INA219 on its conversion clock, each reading the mean over the shunt conversion, a move timed 16 ms to 61 ms after the model's 417 ms, its peak and hold, unseen at the first reading 3061 ms or more after the command, the stall cut during a timed move at its 17th reading as without it, the release, a profile's rebind; SET's line through the screen, kept while SAFE, gone at START, under a two-line note; rcbench's `test_servo_move.c`, 18 cases; 101 mutants, 99 failing the suite, 2 equivalent, listed in the note | not tested on hardware: the time against a scope on SIG and the current, bench test B10; open item 16 |
| QMI8658 IMU driver on the shared I2C1 bus (`src/qmi8658.*`, slice 2 of [angle-meter.md](docs/design/angle-meter.md#slice-2-as-built)), ticked from `uiTick()` after the current sensor and sending nothing until the angle meter opens it; `senseNote()` for every client of the bus; the bus recovered with CURRENT SENSE at `NONE` | against a register model of the part on the I2C fake (`test/fake_i2c.*`, `runQmi8658Tests`): the burst decoded little-endian into signed counts and a 24-bit time stamp; no part: `NO IMU`, probed again at 1000 ms and not 999 ms; WHO_AM_I 3Ch: another part, read again every 1000 ms, nothing written; WHO_AM_I, REVISION_ID, then B0h to RESET; the first opening, at millis() 0 before any reset, probing at once; dQY_L at 15 ms and not 14 ms, 80h configuring in the same frame, CTRL1 40h, CTRL2 08h, CTRL5 07h, CTRL7 01h each one byte and read back, dQY_L read before CTRL7; dQY_L 00h or no answer: WHO_AM_I at 1750 ms and not 1749 ms; a read-back mismatch `NO IMU`, the second reset at 2000 ms after the first and not 1999 ms, also across BACK and a new opening, and a probe at once 30 days after the last reset; nothing read in the first 100 ms after CTRL7, the first burst after it a baseline neither counted nor taken, the first sample taken latched 128 ms after CTRL7, not before 99 ms; 25 samples in 32 frames of 25 ms, 7 repeated time stamps not counted, a burst whose time stamp changed dropped, the 24-bit wrap; three failed frames offline, a good frame starting the count again, a short count, two timeouts sticking the shared bus and the IMU sending nothing until a touch read frees it, a held SDA as a line low; BACK's CTRL7 00h, also after a CTRL7 write that timed out after reaching the part, nothing after it, nothing for a part not configured or a stuck bus; touch read every frame and the INA219 every frame beside it, 186 clocks for the IMU's frame; the worst frame in each failure mode and with one timeout after each count of transfers, at most 1730 µs; `uiTick()` ticking it; the recovery at `NONE` at once and every 100 ms, nothing sent to 0x40; 96 mutations, 94 failing the suite; of the two left, `<=` for `<` in the reset gap is equivalent, and the sample count's skip of 0 comes after 2^32 samples | not tested on hardware: the part, its address, byte order, reset and time stamp are not verified; bench tests B1, B2 and B9 of the note |
| Angle meter's model (`src/angle_model.*`, slice 3 of [angle-meter.md](docs/design/angle-meter.md#slice-3-as-built)), pure, built by the firmware with rcbench's `servo_rudder.c`, `imu.c` and `witmotion.c`; the screen, slice 4, calls it | on synthetic gravity vectors fed as the driver's samples (`runAngleModelTests`): the axis map, also turned and with no normal; `theta`'s change equal to the rotation about a line tilted 0° to 70° within 0.001° for each ALONG AXIS, and through the samples within 0.005° / cos(tilt) for each ALONG AXIS and SIGN; TOO STEEP at 70° and 70.1° and not at 69.9°, amber at 45° and not at 44.9°; frames of four, seq 0 and a repeat not taken, a 300 ms gap and not 299 ms starting the frame again, stale at 300 ms and not 299 ms; SETTLED at 1000 ms and not 999 ms; ZERO 1000 ms after the lift and not 999 ms, taken by a frame at 9999 ms and timed out at 10000 ms, by the tick too; MAX from settled frames only, each side, swapped by SIGN, cleared by ZERO and RESET MAX; the wrap from 170° to −170°, −180° kept by SIGN; the travel for LENGTH 10 and 500 mm and none for 9, 501 and OFF; the temperature warning at 2.0 °C and not 1.898 °C either way; the calibration's limits; the errors of the note's calibration table within 0.006°, and removed by a calibration; the flip's arithmetic against values worked by hand, offsets recovered exactly on a surface level and sloped 5°, the normal on chip Z, −Z and X, each pose's limit at 0.8 g; its steps: 2000 ms and not 1999 ms, 0.5° and 300 ms breaking a stretch, step 2 waiting for a move, `Failed` past the limits, the frames uncalibrated; NaN and 0 g; the texts in tenths, never `-0.0`; a calibration that applies as the current one keeping the zero; the screen-down flag from the mapped, calibrated normal; 161 mutations, 158 failing the suite, the 3 left equivalent | not tested on hardware: the angle, the axes (B3), the settle (B4), the flip (B5), the accuracy (B6) and the drift (B7) of the note |
| Angle meter's screen and settings, slices 4 and 5 of [angle-meter.md](docs/design/angle-meter.md#slice-4-as-built): ANGLE METER (`src/ui_angle.*`) with SETUP and FLIP CAL, TOOLS (`src/ui_tools.*`) in MENU's sixth row by decision 1, the settings block version 6 with the carry-over of version 5 and `settingsSaveAngle()` (`src/settings.*`) by decision 4; README, the wiki pages Angle-Meter and Winkelmesser | through the real UI, the driver and the QMI8658 model on the I2C fake (`runAngleUiTests`): TOOLS' row, chooser, strip, entry and BACK, no claim, nothing to 0x6B before the meter opens; the driver opened, CTRL7 00h on BACK and nothing after it, a new visit unzeroed; the angle in tenths and `---` while starting, stale or too steep, the tilt on the strip at 44.9°, 45.1°, 69.9° and 70.1°, SETTLED and MOVING; NO IMU with no part and after the part is lost, ZERO drawn disabled then, another part's ID; ZERO not at 900 ms and taken after 1000 ms, its 10 s limit with the old zero kept, MAX from settled frames and not a knock, RESET MAX on the tap's frame, the temperature at +1.9 °C and +2.0 °C; SETUP's rows and rectangles, LENGTH `OFF`, 10, held to 500 and no further, SIGN keeping the zero and swapping MAX, ALONG AXIS dropping them, one save on BACK and none without a change, a failed save's hint, the setup loaded on the next visit; FLIP CAL's three steps and result lines, BACK and AGAIN disabled screen down, HOLD TO KEEP not at 975 ms and at 1000 ms, its save, an equal result writing nothing, a failed save, AGAIN, BACK without keeping; the widest texts and the static fit checks; the control tables of TOOLS and the three views (`runIdTests`); MENU's eight rows (`runMenuTests`, `runUiTests`); version 6, the version 5 carry-over, the angle fields' validation and `settingsSaveAngle()` (`runSettingsTests`, `runClaimTests`); 99 mutations: 95 fail the suite, 88 at first and 7 after tests were added; two were lines that changed nothing and are removed; of the two left, the armed check before the save is unreachable and `--- MM` for the travel is equivalent; after the review, HOLD DEFAULTS keeping the four fields and SETUP's BACK saving while they differ from flash, with its hint (`settingsAngleDirty()`), 19 mutations of the changed code, 17 failing the suite, the armed check and the spare byte's copy equivalent | not tested on hardware: nothing of the meter has run on the board; bench tests B1 to B9 of the note are open, the accuracy is not measured |
| Touch handling, high-contrast theme, throttle gauge | tap on release, repeats, screen residue, gauge reach | hardware use recorded: the reports #22 to #24 named in the v2.3 commits; the fixes on hardware: not recorded |
| List and hold widgets (`src/ui_list.*`, `src/ui_hold.*`; slice A of [menu.md](docs/design/menu.md)) | the list: the scroll range and its clamp, the press rule at the viewport's edges, the row rectangle, the axis lock at 6 px, a tap on release and its slide-off, the scroll from the press point, 16 px per coarse step, the factor, the stop at a rail and the anchor after it, the clipped drawing; the hold: 1000 ms and not 999 ms, the reset on a lift and a slide off, a press that slid on, one firing per press, no timing for a press that landed while the control was disabled or whose first frame it missed, the progress bar's extent; through the screens: AM32's swipe on every field of the table, KV at 16 px a step and its rail, the AM32 and SERVO SET scroll ranges and edge rows (checks that 4c91db9 also passes), DWELL's rail on SET, HOLD TO WRITE and HOLD TO SAVE at 999 ms and 1000 ms, HOLD DEFAULTS at 975 ms and 1000 ms, each hold's colour and bar, a press on a disabled HOLD TO START and HOLD TO APPLY never timed, nor by AM32 WRITE one on a disabled `WRITE...`, HOLD TO SAVE's refusal while armed | not tested on hardware |
| Menu structure B, slice 1 (`src/ui_leave.*`; `UiBackGuard`, `uiStripTwoLines()`, `UI_COL_*` and the layout constants in `src/ui_widgets.*`; `uiListDrawField()` in `src/ui_list.*`; [menu-b.md](docs/design/menu-b.md)) | the leave page: its three phases, the current step, STAY, LEAVE, NEXT SERVO, RESTART DSHOT at 999 ms and 1000 ms into the released phase and across a `millis()` wrap, a press that landed during the wait or while releasing never restarting DShot, LEAVE drawn disabled and ignoring its tap while `leaveEnabled` is clear, LEAVE taking only a press that landed while it was enabled and is still enabled at the lift, the steps, bar, notes and buttons in each phase, the room above the notes; the guard: the first BACK arming, the second leaving, a press elsewhere and no change left disarming, its two lines for one and two changes; the two strip lines at y 42 and y 52 and their clear; the field row in each value state and edge, selected or not, scale 2 up to 104 px; the role names in both themes; the constants' values; SERVO TESTER's supply line drawn through `uiStripTwoLines()` | host only; slices 2 to 6 put the blocks on the screens |
| Menu structure B, slice 2, colour roles (`UI_COL_CHANGED`, `uiChip()`, `uiChangeText()` in `src/ui_widgets.*`; the screens; [menu-b.md](docs/design/menu-b.md#slice-2-as-built)) | the role names, seven colours, in both themes; the chip square, 12 px tall, y 42 to 53, PROG MODE 65 px from x 167; the change text's dim old value and cyan new value; on AM32 CONFIG, KST SERVO, HITEC SERVO and SERVO SET a row's value by its state, as read, changed with a 3 px cyan bar, a soft rule's amber bar, read-only grey, selected or not, and the editor bar's value; SERVO SET's pending row cyan until HOLD TO APPLY and grey while RUNNING; Hitec's label without `*`; the KST and Hitec headlines amber, PROG MODE on blue, KST's IN DOUBT on red, REVERT amber, the write pages' old and new values; the tester's KISS and EDT tags, the DShot rate, SWIPE = THROTTLE and MAX; AM32's POWER-CYCLE THE ESC, HEX, WRITE VERIFIED green and a refused write's result not green; SETTINGS' captions, values, SAVED and the save hold's red; SD LOG's START, RETRY MOUNT and USB DRIVE labels and the read counter; servo B's horn, letter and selector, SPEED's fill, the command labels, the sweep count | host only: the colours have not been looked at on the panel |
| Menu structure B, slice 3 (`src/ui_settings.*`, `src/settings.cpp`, `src/ui_menu.*`, `src/kst_view.cpp`, the pin words of `src/ui_servo.cpp`, `src/ui_kst.cpp`, `src/ui_hitec.cpp`, `src/ui_am32.cpp`; [menu-b.md](docs/design/menu-b.md) decisions 9, 10 and 15 b, e, f, h, j, k) | SIG PIN and AUX PIN live whatever KISS is, every step a swap, with KISS on and off, the wiring following; their J5 line and SERVO SUPPLY's J4 line under a raised value, none under OWN; `settingsValidate()` moving AUX off SIG with KISS on and off, to the next free pin after SIG, KISS kept, and a pin the board does not free switching KISS off; SERVO SUPPLY a core section with and without KST SERVO on MENU; MENU's caption, `DSHOT RESTARTING ON SIG GP0` in amber over rows drawn disabled under their own status, and the caption back once Free; `WAIT: DSHOT RESTARTING` on SERVO TESTER, KST SERVO, HITEC SERVO and AM32 CONFIG; `DSHOT600` at each rate; `NOTHING SAVED: DEFAULTS IN USE`; the four KST labels; SERVO TESTER's caption with the role alone where it would reach RUNNING; the SIG words of each claim message; the connector lines against the board header's table; KISS TELEM moving neither pin; `SWAP THE ESC'S SIGNAL AND KISS WIRES` with KISS on; `IDLE 2s` and `DSHOT1200` sharing no pixel | host only |
| Menu structure B, slice 5: the write rule and the guard, decisions 6, 7 and 13's AM32 titles of [menu-b.md](docs/design/menu-b.md#slice-5-as-built) (`src/ui_am32.*`, `src/am32_backup.*`, `src/ui_kst.*`, `src/ui_hitec.*`, `src/ui_servo.cpp`) | `WRITE...` on AM32, KST and Hitec opening nothing while pressed or on a slide off and the write page on a tap, the tap's press not timed there; `RESTORE...` opening the KST and Hitec restore pages and restoring only on their hold; AM32 WRITE's title, count, four change lines, `AND 1 MORE`, the backup line with a card, without one, with USB holding it and once the backup is on the card, HOLD TO WRITE at 999 ms and 1000 ms, `WRITING: DO NOT UNPLUG.`, the result lines at y 196 and y 208 in green or amber; AM32's editor bar with the old value and without the label where it does not fit; `AM32 HEX` and its BACK to the list; the guard on AM32's list and on SET: one and two changes, a press elsewhere disarming it, REVERT and HOLD TO APPLY leaving nothing to guard, the second BACK dropping the edits with nothing written; BACK disabled and drawn so on AM32 WRITE while writing, on KST WRITE and HITEC WRITE while writing and while waiting for the flush, live again under the result, no leave page after the result; a stall during the KST and Hitec flush wait ending it, the next write sending nothing until its own hold; AM32 WRITE refusing settings changed under it and drawing HOLD TO WRITE disabled when it lists no change; the next steps under OK (`CHARGE THE CELL. TAP OK, WRITE AGAIN.`, the flush failures' `TAP OK, WRITE AGAIN.`) and KST's stalled screen in doubt; 46 mutations, each failing the suite | host only |
| Menu structure B, slice 4: the layout table, decision 14 of [menu-b.md](docs/design/menu-b.md) (`src/ui_settings.cpp`, `src/ui_servo.cpp`, `src/ui_am32.cpp`, `src/ui_sdlog.cpp`, `src/ui_kst.cpp`, `src/ui_hitec.cpp`, `src/hitec_view.h`) | each moved rectangle by id: SETTINGS' HOLD TO SAVE 6/150 and HOLD DEFAULTS 162/72, SERVO SET's HOLD TO APPLY 6/150, DEFAULTS 162/72 and `-` and `+` 44 × 44 px, AM32's `-` and `+` 44 × 44 px and OK 6/228 at y 268, SD LOG's START 6/228, RETRY MOUNT 6/112 and USB DRIVE 122/112 (`testLayoutTable()`, `testAm32LayoutTable()`); no two controls overlapping; each label inside its button; HOLD DEFAULTS' two lines and HOLD TO SAVE's `C_RED` fill; AM32's OK keeping the result up when the hold's finger lifts on it and acting on a later tap; the hex dump from x 8; `1 CHANGE` and `5 CHANGES` on KST's write page, `1 CHANGE` and `n CHANGES` on Hitec's, Hitec's 4 rows (`hitecPageLayout()`), Hitec's strip in the order model, backup, pin, version with two spaces, a classic strip keeping `BACKUP IN RAM` and then `BACKUP HT001` beside the PROG MODE chip, the room 4 px before the chip (`uiChipX()`), y 56 to 61 blank on the main view and the list after the READ wait's two strip lines; AM32's OK drawn at rest under the hold's finger and pressed under a fresh press; 23 mutations, each failing the build or the suite | host only |
| Menu structure B, slice 7: the tester, owner decision 4 and decisions 11 (the MAX row), 15 a to c, 17, 19 and 20 of [menu-b.md](docs/design/menu-b.md#slice-7-as-built) (`src/ui.cpp`, `src/ui.h`) | `14 POLES` and `MAX 20%` in the text colour, each underlined at y 122 and y 244 while disarmed and while the claim is Free; `TesterCtl::Poles` and `TesterCtl::Max` at their rectangles, acting then and only then; a tap on either opening SETTINGS, BACK there returning to the tester, drawn whole, and SETTINGS from MENU still returning to MENU; a press that slides off opening nothing; the tap turning LATCH off, leaving the motor disarmed and the throttle at 0, and the tester painting nothing over the cleared frame; armed: no underline, a tap opening nothing and keeping the arm; a claim not Free: no underline, a tap opening nothing; the 3D note taking the pole count and its target, MAX still acting; `LATCH` and `LATCHED`; armed, a press on MAX driving the track and a swipe from the pole count driving the pad; MENU's tap painting no tester row; a finger resting on MAX from the splash opening SETTINGS on its lift; 26 mutations, each failing the suite, and one equivalent | host only |
| Menu structure B, slice 6: the leave page, decisions 5 and 21 and owner decisions 3, 25 and 30 of [menu-b.md](docs/design/menu-b.md#slice-6-as-built) (`src/ui_kst.*`, `src/ui_hitec.*`, `src/ui_servo.*`, `src/ui_leave.*`) | on KST SERVO, HITEC SERVO and SERVO TESTER: the page before LEAVE, releasing and released, each step's text, number, colour and bar, the notes in amber before LEAVE and grey after, the strip's `SIG GP0`; STAY to the list; LEAVE ending the claim, its record removal, the page releasing in place; NEXT SERVO and BACK TO SERVO keeping the claim, the cache on again, a new search or session, Hitec's READ wait for a D series servo on its own supply; RESTART DSHOT in LEAVE's rectangle, drawn disabled and reported disabled until 1000 ms after the release, live from 1000 ms to 1025 ms, then MENU and Free; a tap in the wait, a second tap of a double tap on LEAVE, and a press that landed in the wait and lifted after it restarting nothing; BACK drawn disabled and doing nothing in every phase; a page with no servo in the mode opening at `1 UNPLUG THE SERVO.`; HOLD TO REBOOT and READ AGAIN at the right rectangle at scale 1; the switched supply's countdown note and no NEXT SERVO line; the control table in each phase; Hitec's LEAVE removing the record of a write refused at red and keeping that of a failed write; BACK after a failed Hitec entry and during KST's REBOOT opening the page releasing; no REBOOT note without the switch; the supply note clamped to 2.0 s left; 46 mutations, 44 failing the suite and two equivalent, listed in the note | host only |
| Menu structure B, slice 8: SERVO TESTER's title, decision 13 of [menu-b.md](docs/design/menu-b.md#slice-8-as-built) (`SERVO_UI_TITLE` in `src/ui_servo.h`, `src/ui_servo.cpp`, `src/ui_registry.cpp`) | `SERVO TESTER` drawn in the header on the main view and the leave page, plain panel from its end to BACK; the title MENU's row label and `UI_SERVO.title`; 13 characters or fewer, ending 4 px or more before BACK; 10 mutations, each failing the build or the suite | host only |
| Menu structure B, slice 9: MENU's seven rows and PROGRAMMER, owner decisions 1 and 2 and decisions 15 d, 16, 18, 19, 23 and 31 to 35 of [menu-b.md](docs/design/menu-b.md#slice-9-as-built) (`src/ui_registry.*`, `src/ui_menu.*`, `src/ui_servo_prog.*`, `src/ui_screen.h`, `src/ui.cpp`, `src/ui_hitec.*`, `src/ui_kst.*`, `src/ui_am32.cpp`, `src/ui_servo.cpp`) | the seven rows in their order, the tool rows without a status, SD LOG whole at 0 px and SETTINGS whole after a 36 px drag; RX SIGNALS drawn disabled under `NOT BUILT` while the claim is Free, Stopping, Held and Restoring, a tap and a press doing nothing, the strip's caption kept while Free; SERVO PROGRAMMER drawn disabled while the claim restores; PROGRAMMER claiming nothing, DShot running on SIG while it shows, its title, strip, three entries and their lines with OWN, GP28 and GP29, HITEC CLASSIC drawn disabled and inert without the switch, a press drawn pressed and a slide-off opening nothing, BACK to MENU, the control table; each entry opening its screen and claiming SIG on the tap's frame; HITEC SERVO's `RELEASING SIG GP0 FROM DSHOT`, the READ wait with no job, no line opened and no write before READ, READ starting the job, BACK there opening the leave page releasing and NEXT SERVO returning to the wait, the ESC refusal asked again every frame with BACK to MENU, BACK while DShot drains to MENU with DShot resumed, the job starting after the claim with the switched supply and for CLASSIC, the supply off at the bind, NEXT SERVO keeping the classic family, NEXT SERVO after a D series read returning to the READ wait with no line opened and nothing written, `pinClaimKeep()` leaving HITEC's job unasked and SERVO's wanted, the note `NEXT SERVO: POWER IT AFTER THE TAP.`, READ ignoring a press that landed while DShot drained, RESTART DSHOT to MENU; the classic entry opened by hand without the switch starting its job and refused by it; 51 mutations, 49 failing the suite and two equivalent, listed in the note | host only |
| Menu structure B, slice 10: the documentation no slice owned, [slice 10 as built](docs/design/menu-b.md#slice-10-as-built) (`docs/design/`, README.md, rule 9 of `test/check_docs.py`) | rule 9 against the tree: no removed screen text, view, MENU path or image in README.md, `wiki/*.md`, a `fakeDumpFrame()` call or `tools/make_previews.py`; each kind added to README.md, a German wiki page, `tools/make_previews.py` and `test/test_menu.cpp` fails it, 20 lines | no firmware change |
| Control tables (`src/ui_screen.h`; slice B of [menu.md](docs/design/menu.md)) | every screen's table on each view the host suite reaches, the ids, rectangles and enabled flags, `uiCurrentScreen()` after each navigation; the UI tests tap by id | host only: the firmware does not call the tables |
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
Slice 1 of [usb-bridge.md](docs/design/usb-bridge.md#slice-1-as-built)
ports the first branch's frame slot and ring.

## Next steps

State on 2026-11-14: one pull request is open as a draft, #49, the
touch-loss stop of item 23; all seven CI jobs pass on `main` at 22c1e4b
(#51). The owner answered the questions of items 18 to 21 and 24 on
2026-11-14. Eight of the decisions are built: the settings carried over
from versions 2 to 4 (item 19), the log numbering (item 20), the write
refusal on a red BOARD voltage (item 21), and all five of item 24: the
tester's 3D note, HITEC SERVO's claim at opening, the servo tester's hold
past the travel, SETTINGS' 1 s defaults hold and AM32 CONFIG's read-only
view. The rest
waits on building where it changes anything, and items 22 and 23 wait on
the bench. Still open to the owner:
the converter module's name (item 19) and the second contact once it is
measured (item 21). The order
of work the owner decided: the eight small decisions of items 20, 21 and 24
first, then menu structure B.
The bench parts, in the order they depend on each other:

1. The pins of item 15: DShot on GP0, KISS on GP1, the backlight, EN on
   GP28 or GP29 with the converter fitted.
2. The pin claim (item 8), which every tool below uses, and the SDIO driver
   (item 6), which KST and Hitec use for their backups and files.
3. The servo tester (item 9).
4. KST bring-up (item 10, design note step 7), which also checks the
   converter.
5. Hitec bring-up (item 14, design note step 6): the classic line on a
   scope, then one write with read-back per family before a write is
   relied on.

A `v*` tag runs Release; a tag without a pre-release suffix then publishes
the manual (item 7), a suffixed one does not. Release, by the owner's
decision of 2026-11-14: v0.1.0-rc1 as a pre-release, an image for testing
with no manual published, and v0.1.0 after the bench checks above.
v0.1.0-rc1 exists: the tag points at a93e1c1, and its Release run of
2026-11-14 published a GitHub pre-release with
`MiniRcBench-v0.1.0-rc1-touch-lcd-2.8.uf2` (796,672 bytes),
`SHA256SUMS.txt` and `THIRD_PARTY_NOTICES.txt`. The Docs run it started
was skipped, as a suffixed tag's is, so no manual is published. That image
has not run on hardware. v0.1.0 waits on the bench checks; the owner pushes
the tag. Menu structure B is built in its 10 slices, host tests only; it has
not run on hardware (item 18).

## Open items

1. **[Issue #1](https://github.com/subtilitas/DshotDisplay/issues/1),
   "Initialisation fails on some BL_Heli32 ESC".** Opened
   2026-09-11 by DerGeheimdienst. Open, not reproduced by the owner. One
   comment, the owner's of 2026-10-16, asks the reporter to arm and then plug
   the ESC in while armed; the reporter has not answered. This
   is an external report, not a confirmed defect. Conditions as
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
   which DROPPED FRAMES counts. BUF PEAK and WORST FLUSH, on the SD LOG
   screen while a log runs and in each log's `.TXT`, are the numbers to size
   it from.
3. **KISS line voltage.** The KISS specification puts the line at 3.6 V.
   GP1, the KISS pin, tolerates 5.5 V while the board is powered and 3.63 V
   while it is off. The level an ESC drives is not measured. The firmware
   enables the internal pull-up on the KISS pin.
4. **Settings flash and the USB card reader on hardware.** Not recorded.
5. **Design questions left open** in
   [kiss-telemetry.md](docs/design/kiss-telemetry.md) and
   [blackbox-logging.md](docs/design/blackbox-logging.md): which eRPM source
   the main readout uses above the crossover (about 5,500 RPM on a 14-pole
   motor), whether consumption is shown from ESC power-on or from arming,
   whether a slow frame type is worth adding.
6. **SDIO driver on hardware.** The driver in `src/sdio/` is not tested on
   hardware. Bench tests needed:
   - initialisation on an SDSC card (2 GB or less) and on SDHC and SDXC
     (extended capacity) cards;
   - sustained logging at 7.3 kB/s, with BUF PEAK and WORST FLUSH read off the
     SD LOG screen while recording, or from the log's `.TXT`;
   - USB DRIVE: a host reads files, which takes multi-block reads;
   - the card pulled out while logging, while idle and while a host reads,
     then put back;
   - 10 MHz in the firmware, and 10 MHz and 25 MHz in `tools/sdtest`.
7. **The manual is not published.** The repository's wiki exists with one
   hand-made page, Home, dated 2026-10-29. `docs.yml` publishes `wiki/` after
   a successful Release run of a tag without a pre-release suffix, or by
   hand. The one `v*` tag, v0.1.0-rc1, has a suffix: the Docs run its
   Release run started on 2026-11-14 was skipped, so the wiki holds none of
   `wiki/`.
8. **Pin claim on hardware.** The PIO and pad calls of the drain, the
   teardown and the rebuild run only in the firmware; the host suite runs the
   decisions they act on. Bench tests needed, with an oscilloscope on the ESC
   signal pin: AM32 opened within 250 ms of the last throttle frame (zero
   frames for at least 250 ms after it, then no edges); the line's level
   after the teardown, which stops, releases and puts the pull-up back, so it
   should idle high; a SETTINGS pin change during the same window; and an AM32
   session left with BACK (DShot back, the EDT enable as its first
   frames). Leaving sends the bootloader no RUN command (`am32BlRun()` has
   no caller). What the ESC does after the screen is left is not recorded,
   in the hardware use of ab79c67 and bd90e17 either, which sent no RUN.

9. **Servo tester on hardware.** Nothing of it has run on a chip. Bench
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
      through the leave page, then DShot back on RESTART DSHOT;
    - STOP on SET while RUNNING: level 0 within one frame, as on SERVO;
    - a real servo on each profile, and whether it reads 3.3 V pulses.
10. **KST programming on hardware.** No frame of the line driver has been on
    a pin. Bring-up is step 7 of
    [kst-programming.md](docs/design/kst-programming.md#work-breakdown): the
    reply level at the connector, the owner's servo's power-on window, frames
    and replies on a scope, read reliability, one write with read-back, then
    a power cycle and a read. The switched supply, step 5, and its card
    phase are tested on the host only.
11. **KST card-phase cap.** At the SD specification's SDXC
    times every counted phase on FAT32 is under the cap of 15 s, the
    longest 13.3 s; on exFAT the first write that keeps an old record takes
    16.5 s ([kst-programming.md](docs/design/kst-programming.md), "The cap").
    The blackbox logs in `LOGS/` do not count; logs an earlier firmware wrote
    to the root do, up to 135.0 s with 1000 of them. Those figures count the
    phases without the write cache of
    [sd-write-cache.md](docs/design/sd-write-cache.md), which runs during the
    KST SERVO and AM32 CONFIG claims with a card mounted: the writes no
    longer block core0, the reads still do. The phases with the cache are not
    counted. A phase past the cap switches the supply off and loses the
    backup in memory; whether a card reaches it is not measured.
12. **SD write cache on hardware.** Built and run in the host suite only;
    the glue and `src/card_cache.cpp` are built by the firmware only. Bench
    tests needed: busy polled once per loop pass on a scope, a card swapped
    during a flush, power pulled during a flush and the volume checked on a
    computer, and a card swapped between two writes of a session, which the
    serial number read before each write is to catch
    ([sd-write-cache.md](docs/design/sd-write-cache.md#card-swap)).
13. **Device backups on hardware.** [backups.md](docs/design/backups.md) is
    built and runs in the host suite only. Bench tests needed: a backup of
    each device on a FAT32 and an exFAT card, the ring turned past 50 on a
    card, a backup read back on a computer.
14. **Hitec programming, steps 1 to 5 of 6 built; bring-up open.**
    [hitec-programming.md](docs/design/hitec-programming.md) records the
    design on rcbench 3e29dc1b, with writes, and the owner's decisions of
    2026-10-23, 2026-10-28 and 2026-10-29. Built: rcbench's Hitec files in
    `lib/rcbench/` with their 115 test cases, the core1 job
    (`src/hitec_job.*`) on them, tested against a simulated servo, the
    files on the card (`src/hitec_files.*`), the HITEC SERVO screen on MENU,
    its README section and the wiki pages Hitec-Programming and
    Hitec-Programmierung. CLASSIC opens with SERVO SUPPLY at GP28 or GP29, by
    the owner's decision of 2026-10-29: SIG is a fault tolerant pin and the
    servo supply is 5 V; the classic line voltage is not measured. At the SD
    specification's SDXC times the Hitec first write that keeps an old record
    takes 17.9 s on exFAT, over the card-phase cap of 15 s. No write plan of
    rcbench's Hitec session or panel has run on a servo, so step 6, the
    bring-up, runs one write with read-back per family before a write is
    relied on (design note, "Work breakdown", step 6).
15. **Signal pins GP0 and GP1 and the enable on GP28 or GP29 on hardware.**
    No hardware report above is recorded on these pins. Not tested on
    hardware: DShot, the AM32 editor, KST and Hitec on GP0; KISS and servo B
    on GP1; the servo pulses on PWM slice 0; the converter's enable input
    (EN) driven from GP28 or GP29; the first boot over a version 3 settings
    block; the backlight on pio0 at 20 kHz. Bench tests needed: an ESC on GP0
    with **LINK** above 0/S, a KISS frame on GP1, both servo channels on a
    scope, EN high and low at each pin with the converter's output
    following, the backlight at BACKLIGHT 16 and 255 on GP16.
16. **Current sensor on hardware.** Slice 1 of
    [current-sense.md](docs/design/current-sense.md) is built and runs in
    the host suite only, slice 2 draws the sensor and the boost
    converter in `docs/wiring.svg`, and slices 3 to 5, the servo tester
    switching the supply, the cuts on the sensor's readings and the move
    timing, run in the host suite only: no INA219 has been on the
    board's bus, no boost converter on GP28 or GP29, and the
    first boot over a version 4 settings block, which carries over, is not
    tested on hardware.
    The datasheet facts the design marks "to confirm" are not checked
    against SBOS448G. Bench tests needed (design note, "Bench tests"):
    - B1: the board's pull-ups on SDA and SCL, the rise time at 400 kHz with
      the J3 lead and the module, the longest lead that keeps 300 ns;
    - B2: the INA219 found at 0x40 beside the touch controller, one frame's
      I2C traffic on a scope, touch with and without the module;
    - B3: touch with J3 empty, with the module's VCC lead open, with SDA held
      low through 100 Ω during a run, and IN- leakage with VS at 0 V;
    - B4 and B5: current against a reference meter at 0 A to 3 A and the
      code at 3.5 A; bus voltage against a multimeter at 5 V, idle and at
      2 A;
    - B6 to B13, with slices 3 to 6: the converter, its disconnect and
      start-up time, the servo tester's switch-on, its pulses at 4.5 V and
      its switch-off at STOP, a trip and BACK, its current limit, the
      owner's servos, the cuts, move timing, the heartbeat with the supply
      on, a 60 s log, the voltage at the servo at a stall.
17. **Restart reason on hardware.** LAST RESTART has not run on a chip.
    Bench tests needed, reading the row after each: power applied from off
    (`POWER-ON`), the RUN pin pulled low (`RUN PIN`), a supply dip below the
    brown-out level (`BROWN-OUT`), a debugger's reset, a UF2 copy
    (`UPDATE`, per the boot ROM source), `picotool reboot` (`REBOOT`), and
    a UF2 copy, then a glitch reset. Not known: whether a watchdog reset
    through the power-on state machine changes CHIP_RESET, whether the chip
    clears older CHIP_RESET flags when it records a new one, and whether a
    reset that leaves the watchdog block alone (GLITCH, possibly
    HZD_SYS_RESET_REQ) clears the watchdog's REASON register; if it does
    not, that glitch reset reads `UPDATE` or `REBOOT`, not `GLITCH`, and a
    HZD_SYS_RESET_REQ reset reads them, not `DEBUGGER`. No watchdog is
    armed. A persistent fault log is not built: the owner decided on
    2026-11-14 not to build one now and to revisit it after the board has
    run on the bench, with the questions listed in
    [diagnostics.md](docs/design/diagnostics.md).

18. **Menu structure B is built, host tests only; not on hardware.**
    Slices 1 to 10 of
    [menu-b.md](docs/design/menu-b.md) are built: slice 2 is the colour
    roles, slice 3 the names and SETTINGS (decisions 9, 10 and 15 b, e, f, h,
    j, k), slice 4 the layout table, slice 5 the write rule and the guard
    (decisions 6, 7 and 13's AM32 titles; [slice 5 as
    built](docs/design/menu-b.md#slice-5-as-built) lists its readings),
    slice 7 the tester's readouts, their
    underline, the return to the tester and LATCH (owner decision 4 and
    decisions 11 (the MAX row), 15 a to c, 17, 19 and 20, with
    [its readings](docs/design/menu-b.md#slice-7-as-built)), slice 6 the
    leave page (decisions 5 and 21 and owner decisions 3, 25 and 30, with
    [its readings](docs/design/menu-b.md#slice-6-as-built)), slice 8 SERVO
    TESTER's title, slice 9 MENU's seven rows and PROGRAMMER, the chooser
    of the servo programmers (owner decisions 1 and 2 and decisions 15 d,
    16, 18, 19, 23 and 31 to 35, with
    [its readings](docs/design/menu-b.md#slice-9-as-built)), slice 10 the
    documentation no slice owned and rule 9 of `test/check_docs.py`
    ([slice 10 as built](docs/design/menu-b.md#slice-10-as-built)); slices
    2 to 9 run in the host suite only, none on the panel, and
    slice 2's colours have not been looked at on the panel, in either theme.
    In the high-contrast palette several of slice 2's pairs differ in
    brightness only, black against a dark grey: the KISS and EDT tags, the
    DShot rate with and without EDT requested,
    `SWIPE = THROTTLE` armed and disarmed, and SETTINGS' captions against
    its labels. Slice 2 leaves points for the owner, listed in the note's
    [slice 2 as built](docs/design/menu-b.md#slice-2-as-built): the uses of
    lime and the failures in amber or text that decision 11's tables do not
    name; the "changed" role's "or saved", which no screen draws; the amber
    headlines of steps that need nothing from the user; AM32's `? (n)`
    values drawn plain; Hitec's D series models rcbench does not name,
    whose strip shows the model alone with a number of 3 or more digits.
    The owner answered the
    note's eight questions on 2026-11-14 (decisions 23 to 30): MENU by
    device, BEEP THE ESC, AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, SD
    LOG, SETTINGS; the tester's readouts inert while armed; BACK on the
    leave page drawn disabled; a changed, unwritten value in `C_CYAN` on
    every screen; each slice updates its own README and wiki lines and
    images; the KST labels FAILSAFE MOVE, FAILSAFE POSITION, FAILSAFE TIME,
    REVERSED; OWN for SERVO SUPPLY's no-switch value; RESTART DSHOT after
    1000 ms. Three decisions
    change the note's design, and the sections they touch are marked to be
    redesigned: the docs go with each slice, against the note's
    recommendation, so the slice table is reworked and slice 10 shrinks;
    HITEC SERVO takes SIG when it opens (item 24), so the chooser's Hitec
    entries take the pin, which changes the note's decision 19; and a MENU
    row RX SIGNALS after SERVO PROGRAMMER (item 19) gives seven rows, BEEP
    THE ESC, AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD
    LOG, SETTINGS. The owner decided on 2026-11-14 that MENU keeps its
    42 px rows and scrolls 36 px with seven rows, as today's seven-row MENU
    does, SETTINGS reached with a short drag; this revises the six rows
    that never scroll of 2026-11-13 (the note's decision 33). Two more
    decisions of the owner of 2026-11-14 shaped slice 9 (the note's
    decisions 34 and 35): HITEC D SERIES with SERVO SUPPLY at OWN claims SIG
    on the tap and is powered after it, the screen waiting in Held for READ,
    so no DShot frame reaches a servo powered by the user; and RX SIGNALS
    stands in MENU drawn disabled, `NOT BUILT`, until its tools exist.
19. **The design notes' questions, answered on 2026-11-14.** The owner
    answered the questions of four design notes; each note records the
    answers as decisions:
    - [menu-b.md](docs/design/menu-b.md): all eight, and the seventh MENU
      row that followed from them (item 18). Built in its 10 slices, host
      tests only.
    - [rx-signals.md](docs/design/rx-signals.md): build the S.BUS (Futaba's
      serial bus for receiver channels) monitor and an S.BUS output, no PPM
      (pulse-position modulation) output; the monitor reads UART0 on GP1
      with the pad's input inverted, shows µs by rcbench's `sbus_to_us()`
      with the bar in raw units; one MENU row, RX SIGNALS, opens a chooser
      of MONITOR and OUTPUT. The output's encoder is written and tested in
      rcbench first, then copied; its throttle channel obeys MAX THROTTLE.
      The monitor's core1 job, slice 2, and its screen, slice 3, are built and
      host-tested (`src/sbus_job.*`, `src/ui_sbus.*`, `src/sbus_view.*`):
      MENU's RX SIGNALS row opens the chooser (`src/ui_rx_signals.*`), whose
      MONITOR opens S.BUS MONITOR and whose OUTPUT is drawn disabled,
      `NOT BUILT`. The UART inversion on the pin, a receiver's level and
      frames on GP1 and the screen's reading of them are not tested on
      hardware (the note's slice 6, the bring-up). The output is designed
      in [sbus-output.md](docs/design/sbus-output.md), not built (item
      31).
    - [current-sense.md](docs/design/current-sense.md): the converter takes
      one LiPo cell, 3.0 V to 4.2 V, into a synchronous boost with load
      disconnect, 5.0 V, at least 3 A out; the shunt stays 0.1 Ω; the servo
      tester always switches the supply with SERVO SUPPLY at a pin; the
      protection is rcbench's limit run, above 1.0 A for 400 ms and a hard
      cut at 3.0 A, the hard cut acting at once in the KST and Hitec claims,
      during an operation too. The note recommended the stall rule and
      warns that the limit run trips on many servos against a stop: bench
      test B8 checks it. Settings are carried over from versions 2, 3 and
      4 into version 5, new fields at their defaults, and since the angle
      meter's slice 4 from versions 2 to 5 into version 6; a block of a version
      the firmware does not know is discarded and announced as now. The
      migration, the first in `settingsLoad()`, is built and host-tested
      (`settingsCarriedOver()`, `runSettingsTests`); the first boot over an
      older block is not tested on hardware (items 15 and 16). Open to the
      owner: the converter module, which bench tests B6 and B7 check before
      it is relied on. The 5 V boost board of `boost_5v_en.step` names no
      chip and the owner does not know it, so its input range, current,
      load disconnect and EN threshold are known only from B6 and B7.
      Slice 2, the wiring sheet, is built: J3 the INA219's connector,
      part C the boost from one cell with the INA219 on its output. Slice 3,
      the servo tester on the switched supply, is built and host-tested.
      Slice 4, the cuts on the sensor's readings, is built and host-tested.
      Slice 5, the move timing, is built and host-tested. Slice 6, the log,
      waits on building.
    - [diagnostics.md](docs/design/diagnostics.md): no fault log now; the
      owner revisits it after the board has run on the bench, with the
      note's seven questions.

    The design questions of item 5 are not addressed to the owner.
20. **Card and log, decided on 2026-11-14,** from the usability audit of
    2026-11-14:
    - **A KST or Hitec write with no card** stays allowed as now: the write
      goes through with the only copy of the servo's old values in memory,
      lost at LEAVE, REBOOT (KST) or a power loss, and the write page says
      so in amber. Nothing to build.
    - **Log numbering**, built: a new log takes the highest
      `LOGnnnnn.BFL` in `LOGS/` plus 1, read in one directory scan on the
      first start on a FatFs mount; after `LOG99999.BFL` no log starts. The
      scan and the kept number run over FatFs R0.15 on the host
      (`make -C test lognum`); on a card the scan's time is not measured.
    - Not a question, recorded here: a USB cable pulled with the board on its
      battery leaves the card with the host. TinyUSB 0.18.0's RP2040 port
      (`FORCE_VBUS_DETECT` 1 in `dcd_rp2040.c`) reports the pull as a bus
      suspend, the same event a sleeping host sends, and never calls
      `tud_umount_cb()` for it. A suspend is not handled; EJECT on SD LOG
      takes the card back. Whether this board has a VBUS sense line that
      could tell the two apart is not verified.
21. **Two owner decisions, one taken.**
    - Decided on 2026-11-14 and built: a KST, Hitec or AM32 write is
      refused while the BOARD voltage is red, the band from 2.50 V up to
      below 3.30 V (`BOARD_CELL_IMPLAUSIBLE_MV` and `BOARD_CELL_CRIT_MV` in
      `src/config.h`, drawn by `uiBoardCell()` with 50 mV of hysteresis;
      `uiBoardWriteRefused()` in `src/ui.h`). Below 2.50 V the reading says
      nothing about a cell and nothing is refused; amber refuses nothing.
      The reading with USB power alone is not measured, so a board on USB
      with no cell may refuse writes (open item 22).
    - Open: whether the tester disarms on a second contact, once the
      CST328's report with two fingers on the glass is measured. The
      driver reads finger 1 only.
22. **The splash's touch line, the BOARD colour and the write refusal on
    hardware.** All three run in the host suite only. Not measured: what
    the divider on GP27 reads with USB power and no cell, and the CST328's
    answers to its probe and its finger reads on a board whose controller
    is missing or slow to start.
23. **The touch-loss stop waits for a bench check.** Pull request #49
    disarms the tester and stops the servo tester when the touch controller
    does not answer for 4 reads in a row and 100 ms. It is not merged. Not
    known: whether the CST328 answers reads while no finger is on the glass
    for minutes. If it does not, the change disarms a motor latched at a
    throttle after the finger lifts. The check, with that branch's firmware:
    - power up and leave the tester untouched for 5 minutes:
      `TOUCH NOT ANSWERING` does not appear;
    - propeller off, arm with the latch on (HOLD on that branch, LATCH on
      `main`) at a low throttle and lift the finger:
      the motor runs until DISARM or the 30 s idle disarm.

    Until then a controller that stops answering leaves a latched motor
    running until the idle disarm, 30 s after the last contact.
24. **Five owner decisions on existing screens, taken on 2026-11-14.**
    All five are built.
    - **AM32 3D MODE**, built. The tester shows `3D MODE ESC: 0-50%
      REVERSED AT 2X` (the text of esc-commands.md decision 9) in amber
      after a list read with 3D MODE on, or a write of 3D MODE on that the
      ESC accepted, verified or not, in this power-up, and arms as before
      (`esc3dNote()`, `src/esc_3d.h`); ESC COMMANDS' 3D ON sets it too.
      Only a list read or a verified write with 3D MODE off clears it, by
      the owner's answer of 2026-11-14 to the review of PR #55. Not
      verified on hardware; a swapped ESC is not detected.
    - **HITEC SERVO and DShot.** Built: HITEC SERVO takes SIG when it opens
      (`PinUserInfo::deferJob`, `pinClaimStartJob()` in `src/pin_claim.h`).
      In menu structure B this changes decision 19 of
      [menu-b.md](docs/design/menu-b.md): the chooser's Hitec entries take
      the pin on their tap, built in slice 9, and a D series servo on its
      own supply waits in Held for READ (the note's decision 34).
    - **Servo tester limits**, built. A TYPE, MIN or MAX step that leaves
      the profile past its travel and changes TYPE or takes MIN or MAX
      further out than the applied profile waits for HOLD TO APPLY
      (`servoTravelNeedsHold()`); so does SERVOS ONE to TWO on such a
      profile, by the owner's answer of 2026-11-14 to the review of PR #55.
      Not checked on a servo.
    - **SETTINGS RESET**, built: a 1 s hold on the shared hold widget,
      labelled `HOLD DEFAULTS` by slice 4 of menu structure B.
    - **AM32 layouts the table does not know.** Built: a layout other than
      2 to 4 opens read-only, the bytes in hex, marked `NOT VERIFIED.`, with
      a backup file; the view has no way to the write, and the write
      refuses such settings (`S_VIEW`, `writeRefused()` in
      `src/ui_am32.cpp`).

    Not a question, recorded here: an ESC set to `EDT ARM` does not run on
    the tester, which sends zero frames after its EDT enable. Sending the
    enable in place of zero frames while armed at zero throttle is
    proposed and not built; it needs a check on hardware.

25. **Rounded corners are not drawn.** `gfxRoundRect()` and
    `gfxRoundFrame()` in `src/gfx.cpp` take a radius and draw none: their
    corner solver stops at the first column it tries, so every row's inset
    is 0, and a button's fill and frame are square in pixels. Seen by
    reading the code, by a scratch program that finds the corner pixel of a
    6 px radius fill and frame lit, and by the host suite, which passes with
    `uiChip()` drawn through `gfxRoundRect()` at a 4 px radius. Whether to
    draw radii, or drop them, is open; the chips are square by decision 11
    either way.
26. **Run summary per log, the owner's choice of 2026-11-14,** from the
    feature survey (item 28). Designed in
    [run-summary.md](docs/design/run-summary.md), slices 1 to 5 built: a
    summary per blackbox log, kept by the logger on core0, written as
    `LOGS/LOGnnnnn.TXT` beside the `.BFL` at a clean stop and shown on SD
    LOG; optionally the board's PCF85063 RTC dating the files on the card.
    Also: every file and directory the firmware writes after a power-on
    has FAT date and time 0 until the clock is set in that power-up, as
    `get_fattime()` returns 0 while the clock is not set; not checked on a
    card. The owner answered the
    note's eight questions on 2026-11-15: the run shows on SD LOG only,
    in rows 2 to 8 after a stop, not on the tester; the SD LOG figures are
    the held values; the maxima cover the whole log; mAh from current
    shows as USED FROM CURRENT; the clock is set over USB from
    `tools/set_clock.py`; no cell is fitted in the RTC socket J2, so the
    clock is lost whenever 3.3 V goes away and files are dated only after
    the clock is set in that power-up; local time with no time zone is
    accepted. Slice 1 is built, host tests only
    ([slice 1 as built](docs/design/run-summary.md#slice-1-as-built)): the
    summary core in `src/run_summary.*`, the held and raw extremes over
    three new values, the per-frame KISS or EDT source with `MIXED`, the
    EDT-only figure when KISS sends 0, the minimum voltage while armed, the
    KISS counter's rise and the charge from current, the armed time, the
    `.TXT` text with both clock lines `not set`, and SD LOG's row texts.
    Slice 2 is built, host tests only
    ([slice 2 as built](docs/design/run-summary.md#slice-2-as-built)): the
    logger feeds the summary every frame, ends it at the stop or at a failed
    write, and after a clean stop writes `LOGS/LOGnnnnn.TXT` beside the
    `.BFL`; `SdLogStatus` carries the last summary. Slice 3 is built,
    host tests only
    ([slice 3 as built](docs/design/run-summary.md#slice-3-as-built)):
    SD LOG's rows 2 to 8 show the last run after a stop, MAX RPM to
    SUMMARY with their source tags, MAX TEMP at 90 °C and above and
    `NOT SAVED` in red, and the counters, CARD and MOUNT while a log runs
    and while no summary shows; the tester shows nothing of the run.
    Slice 4 is built, host tests only
    ([slice 4 as built](docs/design/run-summary.md#slice-4-as-built)): the
    PCF85063A driver in `src/clock.*` reads the clock once at start-up on the
    shared I2C bus, sends the software reset when the oscillator-stop flag is
    set, and keeps a running clock's time by the microsecond timer;
    `get_fattime()` returns it while the clock is set and 0 otherwise, with
    the SD library's `my_rtc.c` out of the build; the `.TXT` carries the
    clock at the start and the stop. Slice 5 is built, host tests only
    ([slice 5 as built](docs/design/run-summary.md#slice-5-as-built)): the
    USB serial port takes `CLOCK 2026-11-14 15:42:07` and `CLOCK?`, parsed
    strictly in `src/clock_line.*` and answered with one line, the board's
    own answers echoed back ignored; `clockSet()`
    writes the part in four transfers and reads it back; a set that reads
    back becomes the kept time and counts one set more, so a log across it
    reads `(clock set during the log)`; `tools/set_clock.py` sends the
    computer's local time; SETTINGS → BOARD shows `CLOCK`, the time to the
    minute or `NOT SET`, with `SET AT EACH POWER-UP` under it. Slice 6, the
    bench bring-up, waits.

27. **Angle meter on the board's IMU, designed; slices 1 to 5 built, the
    bring-up open.** The owner's choice of 2026-11-14 from the
    feature survey (item 28): incidence, decalage, dihedral and control-surface
    throw, in degrees and mm, from the QMI8658 at 0x6B on I2C1.
    [angle-meter.md](docs/design/angle-meter.md) describes the driver on core0
    after the touch and current-sensor reads, the angle from the accelerometer
    alone, ZERO, a three-pose flip calibration, the screen and six slices on
    rcbench's `servo_rudder.c` and `imu.c`: copies, driver, model, screen, docs
    and bring-up. The owner answered its seven questions on 2026-11-15: an
    eighth MENU row, `TOOLS`, opening a chooser whose first entry is ANGLE
    METER; the IMU model added in rcbench first; the three-pose flip, then the
    edge poses if bench test B6 shows a gain error over 0.2°; the calibration,
    LENGTH, ALONG AXIS and SIGN kept in the settings block; 0.1° and 0.1 mm; no
    holder in the first build; the screen only. Slice 1 is built ([slice 1 as
    built](docs/design/angle-meter.md#slice-1-as-built)): rcbench's
    `servo_rudder`, its IMU model `imu` with `imu_feed_angles()` and the
    WitMotion decoder `imu.h` includes, copied unchanged from rcbench e98345aa
    into `lib/rcbench/` with their tests, 25 cases, run by the host suite. Slice
    2 is built ([slice 2 as
    built](docs/design/angle-meter.md#slice-2-as-built)): `src/qmi8658.*`
    probes, resets and configures the part and reads one burst and its time
    stamp a frame, ticked from `uiTick()` after the current sensor;
    `senseNote()` serves every client of the bus, and the bus is recovered with
    CURRENT SENSE at `NONE` too; `BOARD_IMU_AXES` is a placeholder until bench
    test B3. Host-tested against a register model of the part; it has not run
    on a board. Slice 3 is
    built ([slice 3 as built](docs/design/angle-meter.md#slice-3-as-built)):
    `src/angle_model.*` averages the samples in fours, calibrates them per chip
    axis, maps them by `BOARD_IMU_AXES`, takes `theta` and the tilt about the
    chosen edge into rcbench's `imu.c`, and keeps ZERO, MAX and the travel
    through `servo_rudder.c`; the three-pose flip finds the offsets and the
    normal's gain. The firmware builds it with the rcbench copies. The cases of
    rcbench's `test_imu.c` and `test_imu_filter.c` that need `imu.h` alone are
    adapted, 9 and 10 cases. Slices 4 and 5 are built ([slice 4 as
    built](docs/design/angle-meter.md#slice-4-as-built)): ANGLE METER,
    `src/ui_angle.*`, behind MENU's sixth row `TOOLS`, `src/ui_tools.*`, opens
    the driver and shows the angle, ZERO, MAX each side, the travel in mm, the
    IMU's temperature, SETUP with ALONG AXIS, LENGTH and SIGN, and FLIP CAL with
    HOLD TO KEEP; BACK writes CTRL7 00h. The settings block is version 6 with
    the four fields, and a version 5 block is carried over. README's section,
    the wiki pages Angle-Meter and Winkelmesser and the preview
    `docs/angle-preview.png`. Host tests only. Slice 6, the bench tests B1 to
    B9, is open. Which QMI8658 variant is fitted, its byte order, its axes on
    the board, the board's mass and the real accuracy are not verified.

28. **The owner's feature choice of 2026-11-14.** From the feature survey
    of 2026-11-14 the owner chose six features for the build list, each to
    get a design note first, then slices with reviews:
    1. an ESC commands page and an RPM-per-volt and kV estimate on the
       tester;
    2. receiver inputs on RX SIGNALS: servo pulse width and frame rate on
       AUX, then CRSF/ELRS, iBUS and SUMD decoders, written in rcbench
       first;
    3. an IMU (inertial measurement unit) angle meter on the board's
       QMI8658: incidence, decalage, dihedral, surface throw;
    4. a run summary per log, optionally dated with the board's RTC
       (real-time clock);
    5. a USB one-wire bridge for desktop ESC tools, designed in item 29;
    6. Bluejay and BLHeli_S settings, designed in item 30.

    Not chosen now: a PWM ESC mode with a tachometer. For (1),
    [esc-commands.md](docs/design/esc-commands.md) designs an ESC COMMANDS page,
    beacons 1 to 5, direction and 3D mode, each change saved in the ESC behind a
    1 s hold, and an RPM/V reading and a kV estimate on the tester. Slices 1
    and 2 are built, host tests only: the command slot on core1 ([slice 1 as
    built](docs/design/esc-commands.md#slice-1-as-built)) and the ESC
    COMMANDS page as MENU's first row, with the 3D note in `src/esc_3d.h`
    and its new text ([slice 2 as
    built](docs/design/esc-commands.md#slice-2-as-built)). Slice 3 is built,
    host tests only: `RPM/V` on the armed tester's ERPM line and `KV EST` in the
    pole count's place, from 75 % throttle held 1 s, in `src/kv_est.h` ([slice 3
    as built](docs/design/esc-commands.md#slice-3-as-built)); no figure has been
    compared with a motor. The page's controls stay live while the slot is busy,
    the EDT enable's retry every 1000 ms among it, about 11 ms with an ESC that
    sends no EDT; a request made then waits in the slot until it is free, or
    expires after 2000 ms. They are drawn disabled only with no ESC answering,
    the motor turning, the ESC starting, or the page's own request waiting or
    going out (slice 2 as built, last choice).
    The owner answered the note's ten questions on 2026-11-15: `ESC COMMANDS` in
    place of `BEEP THE ESC` as MENU's first row; a save after each change; the
    hold on the page; the 3D note stays until a restart or an AM32 read;
    commands and beacons refused with no ESC answering; `RPM/V` on the ERPM line
    and `KV EST` in place of the pole count; the estimate from 75 % throttle and
    not held after the disarm; the ESC information reply left out; the 3D note
    text `3D MODE ESC: 0-50% REVERSED AT 2X`; no direction note. Its firmware
    facts are read from source and the BLHeli_32 command specification; none has
    been checked on an ESC.

29. **USB bridge for desktop ESC tools, slices 1 to 3 of 4 built.** Item
    (5) of the owner's feature choice (item 28), the port of DshotDisplay's
    `feat/usb-bridge` at 6693084. [usb-bridge.md](docs/design/usb-bridge.md)
    designs a USB BRIDGE screen that takes SIG through the pin claim, as a core1
    job with a PIO line, and connects the ESC's bootloader to the board's
    existing CDC serial port, with the console off while it runs. Passthrough at
    19200 baud 8N1 with the wire's own echo, DshotDisplay's bridge, serves the
    AM32 configurator in Direct mode; ESC-Configurator speaks only MSP and the
    4-way interface through a flight controller, so it needs a second mode the
    board answers itself, not in DshotDisplay. DshotDisplay's bridge tests pass
    on the host at 6693084 (28 checks); it has no hardware record. The owner
    answered the note's twelve questions on 2026-11-15, each as recommended: the
    entry is the chooser `ESC CONFIG`, holding `ESC SETTINGS` and `USB BRIDGE`
    (with item 30); both modes; the mode from the PC's first byte; one serial
    port; `MRCB` as the MSP name; no RUN on leaving; BACK disabled during a
    write; a build-option traffic log; `SERIAL_TELEMETRY` stays 1; a leave page
    after an unfinished write; no bridge while USB DRIVE holds the card; the 3D
    note follows a bridge write. Slice 1 is built, host tests only
    ([slice 1 as built](docs/design/usb-bridge.md#slice-1-as-built)): the pin
    user, the core1 job with the frame slot and the ring, and the PIO line,
    16 words, compared with escwire's `ew_line`, which gives no echo. Slice 2
    is built, host tests only
    ([slice 2 as built](docs/design/usb-bridge.md#slice-2-as-built)):
    passthrough with the console off, the screen USB BRIDGE opened from ESC
    CONFIG, BACK's write rules and the leave page, the traffic log as a build
    option, DshotDisplay's bridge tests ported. Slice 3 is built, host tests
    only ([slice 3 as built](docs/design/usb-bridge.md#slice-3-as-built)):
    the mode from the PC's first byte, the MSP answers, the 4-way interface on
    escwire's bootloader frames for the one ESC on SIG, its writes held to
    passthrough's BACK and note rules, and the 3D note from AM32 settings
    pages in both modes. Slice 4, the bring-up, waits on hardware.
    BLHeliSuite is closed source and not verified; nothing has run on an ESC
    or against a desktop tool.
30. **Bluejay and BLHeli_S settings, slices 1 to 4 of 6 built.** Item (6) of the
    owner's feature choice of 2026-11-14 (item 28).
    [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md) grows the AM32 editor
    into the `ESC SETTINGS` screen: its greeting is already a valid greeting for
    the EFM8 bootloader of both firmwares, whose answer, `471d` and the
    signature E8B1, E8B2 or E8B5, hands the ESC to rcbench's
    `protocols/escwire`, copied unchanged with 210 of its host cases and run by
    a core1 job; quiet seconds between short bursts of greetings let a Bluejay
    enter, and keep an AM32 in, its bootloader. Bluejay 0.21 and BLHeli_S 16.3
    to 16.7 are written, older layouts shown, with a backup before the first
    write, a re-read and a read-back. Read from both firmwares' source: a write
    cut between erase and program, then a restart of the ESC, leaves the 48 tag
    bytes 0xFF and the page read-only. The owner answered the note's eight
    questions on 2026-11-15: the one settings screen is `ESC SETTINGS`, in the
    chooser `ESC CONFIG` beside `USB BRIDGE`; 1000 ms quiet, then 3 greetings in
    turn; LEAVE sends `00 01`; no repair here, and rcbench is asked for an EFM8
    restore; backups `BJnnn.TXT` and `BHnnn.TXT`; a write without a card runs;
    older layouts read-only; a leave page after an answer with no LEAVE. Slice
    1 is built ([slice 1 as built](docs/design/bluejay-blheli-s.md#slice-1-as-built)):
    rcbench's escwire core, its line driver `ew_line` and the servo file store,
    copied unchanged from rcbench 2b4f934a into `lib/rcbench/`, with rcbench's
    12 tests of them, 210 cases; the firmware builds the core and the line
    driver against SDK 2.1.1 with `STRICT=ON`, and nothing in `src/` calls
    them. Slice 2 is built, host tests only
    ([slice 2 as built](docs/design/bluejay-blheli-s.md#slice-2-as-built)):
    the classification of the answer, an EFM8 answer named with nothing read,
    the run cue and the leave page it brings, and the `ESC CONFIG` chooser
    with `USB BRIDGE` drawn disabled, which slice 2 of usb-bridge.md made
    live. Slice 3 is built, host tests only
    ([slice 3 as built](docs/design/bluejay-blheli-s.md#slice-3-as-built)):
    the connect cycle, the escwire job on core1, the page read into a
    read-only list with its strip notes, ESC HEX and the tester's 3D note,
    the refusals on the connect page, and LEAVE's `00 01` from the list.
    Slice 4 is built, host tests only
    ([slice 4 as built](docs/design/bluejay-blheli-s.md#slice-4-as-built)):
    the write, with the backup as `BJnnn.TXT` or `BHnnn.TXT` before it, its
    results, READ AGAIN, the guarded BACK on a half-written page and the
    heartbeat's stop. Slices 5, the bring-up, and 6, the restore, wait on
    being built. Nothing of it has run on an ESC.
31. **S.BUS output: the encoder and the core1 job built, host tests only;
    the bootloader probe and the screen not built.**
    The owner's decisions 1, 4, 5 and 6 of
    [rx-signals.md](docs/design/rx-signals.md#decisions) (item 19).
    [sbus-output.md](docs/design/sbus-output.md) designs S.BUS OUTPUT,
    opened from OUTPUT on RX SIGNALS: a frame every 14 ms on GP0 through
    UART0's TX, the pad's output inverted by OUTOVER in the same register
    write as the UART function, rcbench's `sbus_encode()` and
    `sbus_from_us()` copied unchanged from rcbench b88593d3 (on rcbench's
    `main` since pull request 847, 19 host cases); the decoder's code is
    the same as at 5ef42efd. Slices 1 and 2 are built
    ([slice 2 as built](docs/design/sbus-output.md#slice-2-as-built)): the
    copy, and the job `src/sbus_out_job.*` with `PinUser::SbusOut`,
    `Core1Job::SbusOut` and `CORE1_JOB_NO_GP0`, which nothing opens. The
    design: the pin user `SbusOut` with the claim's ESC check in every
    wiring; 16 sliders on two pages;
    the throttle channel, 1 to 4, starting at raw 172 and held under
    172 + 1639 × MAX THROTTLE / 2000, raw 499 (1200 µs) at the default;
    CH17, CH18 and the failsafe and frame-lost bits on SET; frames stop on
    STOP, BACK, a heartbeat lapse, more than 30 s without touch with the
    throttle or a switch channel off its START value, and touch loss once
    pull request #49 is merged (item 23). Read from source, not run on an
    ESC: AM32 2.22 and Bluejay 0.21 take no throttle from an S.BUS line.
    BLHeli_S 16.7 may detect it as Multishot; in a model of the line, with
    channel 16 at 1500 µs or CH17 on, it calibrates a 140 µs throttle range
    into its flash, arms and meets its motor-start condition in each of 200
    runs, after about 40 s at the median (4.5 s to 199 s); the claim's ESC
    check cannot see a BLHeli_S ESC. The owner
    decided on 2026-11-15 (the note's decision 1): S.BUS OUTPUT is built
    only together with a bootloader probe, the connect cycle of
    [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#decisions)
    decision 2 on GP0 before the first START of a visit, START refused
    while an EFM8 or AM32 bootloader answers. An answer sets MENU's run
    cue, and after an EFM8 answer the leave page is ESC SETTINGS' decision
    8 page, which asks for the ESC's battery to come off before DShot
    returns. The probe costs about 1.1 s per visit and misses an unpowered
    ESC. No question is open. Slice 3, the probe, is next: its
    prerequisites, slices 1 to 3 of ESC SETTINGS (item 30), are built.
    Nothing of the output has run on hardware.

## Known limitations

- **A Bluejay or BLHeli_S ESC that answered waits in its bootloader.**
  ESC SETTINGS reads its page; BACK from the list sends LEAVE, `00 01`, and
  every other BACK gives SIG back to DShot only from the leave page, whose
  steps say to unplug the ESC's battery first
  ([bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#the-firmware-as-it-is-today)).
  The bench cannot see whether the battery was unplugged: RESTART DSHOT
  with the ESC still powered sends DShot into its bootloader, and so does a
  board restart while it waits. LEAVE has no answer: DShot returns once it
  went out, and an ESC that did not take it still waits (bench item B11);
  MENU's run cue stays until an eRPM frame after LEAVE.
  By the bootloader's source a garbled frame passes its CRC check about 1
  time in 256 for arbitrary bytes; what DShot frames read as is that note's
  bench item B12. Not seen on an ESC.
- **A cut Bluejay or BLHeli_S write loses the tags at the ESC's next
  start.** A write cut between the erase and the end of the program leaves
  the page erased or half written; while the ESC stays powered, READ AGAIN
  and WRITE restore it from the session's copy, tags included. A power cycle
  first makes the firmware store its defaults with the 48 tag bytes 0xFF,
  and the page is read-only here from then on; the tags return only with a
  firmware flash for that ESC on a computer. A session that ended with the
  page half written, by a heartbeat stop, a driver failure or the guard's
  second BACK, leaves a page no later session writes: the screen says
  `SESSION ENDED: RESTORE ON A COMPUTER.` and keeps the old page for the
  rest of the visit on `OLD PAGE`, as the backup on the card does. The
  restore of a backup, slice 6, waits on rcbench
  ([bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#why-an-interrupted-write-loses-the-tags)).
- **`tools/sdtest` is built by no CI job**, by the owner's decision of
  2026-10-28: it is a bench bring-up tool and never released, so a red job
  for it is not worth its cost. A change that breaks it shows only in a local
  build (`ninja -C build sdtest`).
- **KST: the screen waits for the card's reads.** With the write cache on,
  the first write's writes queue and the screen keeps drawing while they
  flush; its reads, and every card call without the cache (no card at the
  claim's start), still block core0. Without the cache: the first write's
  card calls run
  on core0, and the screen does not redraw until they end: up to 1.0 s on a
  card with access and busy times of 10 ms; at the SD specification's full
  times up to 13.3 s on FAT32 and 16.5 s on exFAT, up to 135.0 s with 1000
  logs an earlier firmware wrote to the root. FatFs block counts times card
  times (`make -C test kstcount`), not measured.
- **One ring for every device's backups.** 50 sessions of one device push out
  the backups of every other device before them. A ring that has turned and
  lost `BACKUP/LAST` guesses 050 as its latest and writes over 001 next,
  which need not be the oldest ([backups.md](docs/design/backups.md)).

- **Two free pins.** The board frees two GPIOs, GP0 and GP1. SIG, the ESC's,
  takes one and AUX, KISS's and servo B's, the other, so there is no third
  choice: a step of SIG PIN or AUX PIN swaps the two. KISS is off by default and
  one tap on MENU → SETTINGS turns it on. A build-time pin override outside
  the free set, or both wires on one pin, fails the build.
- **Bidirectional DShot only.** An ESC that does not accept the inverted
  bidirectional signal is not driven.
- **Saved settings outrank `config.h`.** On a board with saved settings, a
  changed default in `config.h` has no effect after reflashing. SETTINGS
  marks the whole block, not each row: while no change is pending, its note
  line reads `SAVED SETTINGS IN USE` or `NOTHING SAVED: DEFAULTS IN USE`.
- **Power loss during a log.** The size reaches the directory entry at
  each sync, every 2000 ms (`SD_LOG_SYNC_MS`), the first 2000 ms after the
  start. A log cut within its first 2 s reads 0 bytes, and the chain
  `f_expand()` wrote to the FAT, up to 16 MiB, stays allocated with no
  entry pointing to it until a file system check. Cut later, with the
  16 MiB run `f_expand()` reserved, it is 16 MiB long, holds what reached
  the card, lagging by up to the 8192-byte ring, then old card data; only a
  stop truncates it. Without the run, or past 16 MiB, its size is the one
  at the last sync. From the code and FatFs, not measured; the wiki tells
  the user to stop the recording first.
- **One contact slot.** The touch driver reads the controller's first
  contact slot only. Which contact the CST328 puts there with two on the
  glass is not measured (open item 21), so the manual says to lift the
  throttle finger before DISARM.
- **Logs past the pre-allocation.** 16 MiB lasts about 38 minutes at
  7.3 kB/s. Past that, or when `f_expand()` finds no contiguous space, FatFs
  allocates clusters as the file grows. The stall that causes is not measured,
  and a failed pre-allocation is not reported.
- **USB DRIVE.** Read-only. Refused while armed or recording, with the reason
  shown for 4000 ms. Arming is refused while a host holds the card. A cable
  pulled with the board on its battery does not give the card back: EJECT
  does (open item 20).
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
binary, `dshot_tests`. `test/test_main.cpp` runs forty-one suites:

| Suite | File | Covers |
| --- | --- | --- |
| `runAm32Tests` | `test_am32.cpp` | AM32 init string, its trailer the CRC of `BLHeli` alone, CRC-16/ARC, settings address per MCU (microcontroller) family, field codec, timing advance, the backup's text and its CRC-32, the result texts and the longest of them; the greeting's answer classified (`blAnswerClassify()`): the three AM32 families, the EFM8 signatures E8B1, E8B2 and E8B5 with and without the echo before them; slice 3's loose rule: E8B3, a C1 in place of 0x30, an answer cut after the 0xE8, a glitch byte after it and garbled bytes still EFM8, the last of two answers counting, E9B2, 0xE8 five bytes on, a short answer and another start not understood, an AM32 answer with a glitch byte not understood; the pieces of an EFM8 answer (`blAnswerEfm8Fragment()`): `471`, `71` with 0xE8 two bytes on, 0xE8 with B1, B2 or B5, the answer with its `d` or its `4` lost, noise and an AM32 answer none, an AM32 answer with a glitch byte one; the connect cycle (`greet_cycle.h`): quiet at 999 ms, greeting at 1000 ms, 3 greetings, quiet again from the third, across a `millis()` wrap |
| `runUiTests` | `test_ui.cpp` (includes `ui_am32.cpp`) | the AM32 backup through the screen, screens, throttle modes, safety interlocks, core1 frame decision, EDT enable, EDT enable at pump start against a simulated ESC, touch, MENU's strip, its EDT chip in its three states and ESC COMMANDS' row, SETTINGS' wiring, contrast and save and its VERSION row, SD LOG with its RETRY MOUNT label and its two mount hints and the last run in its rows 2 to 8, USB DRIVE, the text probes of `fakes.h` in high contrast, the splash's version and ESC pin line, the tester's `BOARD` caption, the board voltage's text rounded to 0.01 V and clamped to five characters, HOLD TO ARM drawn disabled while a USB host holds the card, the header's BACK drawn as the control table reports it on every screen the suite checks, the AM32 write page (`WRITE...` on a tap, its changes, count, backup line and HOLD TO WRITE), its result inside the panel with its power-cycle line, ESC SETTINGS' BACK guard and ESC HEX, ESC SETTINGS' strip notes, next-step lines and connect lines and MENU's run cue, the connect cycle through the screen: no greeting before 1000 ms, `LINE HIGH, NOTHING SENT`, 3 greetings, the tries counted in bursts only, an AM32 answer in a burst opening the list; a Bluejay and a BLHeli_S ESC read through the escwire job against rcbench's simulated ESC: the transport letting go, the job binding, `EFM8 BOOTLOADER` in place of the power-cycle lines, the read-only list, its strip, rows, disabled WRITE... and REVERT, its notes, ESC HEX scrolled to its last line, the tester's 3D note set and cleared, the read-only lists of an older layout, lost tags and a wrong marker, a page empty and a page not known named on the connect page, BACK from the list sending `00 01` and DShot returning once the job is idle with the run cue kept until an eRPM frame after it, an answer with its `d` lost bringing the leave page and its strip `EFM8  SIG GP0`, a refused bind leaving SIG on its pull-up, a LEAVE that does not go out opening the leave page, an ESC that stops answering greeted again, an answer with a glitch byte taken as an EFM8's; the run cue an EFM8 answer sets, BACK then opening the leave page with DShot down, from the connect page while reading, after a page not taken, the AM32 list's guard and the read-only view after the cycle greets again, STAY back to each, its steps, STAY, LEAVE cutting the job or letting the transport go, RESTART DSHOT live 1000 ms later, restarting DShot and clearing the cue, an AM32 claim with no EFM8 answer giving DShot back within BACK, the tester's 3D note, ESC SETTINGS' read-only view of a layout outside 2 to 4 and its write refused while the BOARD voltage is red; slice 4's EFM8 write through the screen (`testEfm8Write()`): the editor bar, its steps, rails and swipe, the guard on edits, ESC WRITE's changes, count and backup line, a tap and a short hold writing nothing, the backup saved before the write and holding the page as first read, WRITING with BACK disabled, the verified result, no second backup, the 3D note set and cleared by writes, no card, a card that refuses the backup, the BOARD voltage, a changed page and READ AGAIN keeping the edits, the power cut after the erase, the half-written strip and guard, the page as read written back with its tags, programs that do not take, the guard's second BACK aborting with no LEAVE and no DShot, a heartbeat lapse after the erase, the read-only lists' backups and their strip lines, screenshots |
| `runKissTests` | `test_kiss.cpp` | KISS CRC-8, decode, resynchronisation, request payload, KISS/EDT merge, expiry, EDT enable rule |
| `runGfxTests` | `test_gfx.cpp` | dirty bands, glyph rendering, clip rectangle, lines at any angle and thickness, arcs of dots |
| `runBlackboxTests` | `test_blackbox.cpp` | variable-byte and zig-zag encodings, header, frames, time prediction |
| `runLogRingTests` | `test_log_ring.cpp` | ring buffer, all-or-nothing writes, wraparound, high-water mark |
| `runSettingsTests` | `test_settings.cpp` | settings block, validation, board id check, pin defaults, the version 6 block layout, blocks of versions 2, 3, 4 and 5 carried over, versions 1 and 7 reset, the servo supply, current sensor and angle meter clamps, `settingsSaveAngle()` |
| `runServoSupplyTests` | `test_servo_supply.cpp` | GP28 and GP29 with no switch and with one: untouched, undriven, bind, on, off, release, a change to none, a change from GP28 to GP29, the mode to pin map |
| `runSenseTests` | `test_sense.cpp` (includes `cst328.cpp`), `fake_i2c.cpp` | the INA219 driver on the I2C fake: codec, probe, readings at the range ends, NACK, timeout, stuck bus and recovery through rcbench's `sense_bus.c`, the read-back and a reset, clocks per frame, the sample's age across a `millis()` wrap, the readout's text, touch every frame beside the sensor |
| `runSupplyGuardTests` | `test_supply_guard.cpp` | the cuts on the current sensor's readings as pure rules (`src/supply_guard.cpp`): hard, stall, collapse and sensor lost at each threshold, either sign, a clipped reading, only new and fresh readings, a gap, the start-up reading, a `millis()` wrap |
| `runMoveTimerTests` | `test_move_timer.cpp` | the servo tester's move timing as pure rules (`src/move_timer.cpp`): when a step is timed, the level before and the threshold, the destination's holding level, rcbench's arrival, settled, late and unseen through `servo_move.c`, the hold, the ends from outside, the SET line |
| `runQmi8658Tests` | `test_qmi8658.cpp` (includes `cst328.cpp`), `fake_i2c.cpp` | the QMI8658 driver on the I2C fake's register model: the codec, the probe's three outcomes, the reset's waits, the configuration and its read-back, the 2000 ms between resets, the samples and the time stamp check, the faults through the shared bus record, BACK's CTRL7 00h, touch and the INA219 on the same bus, the bus time per frame, `uiTick()` ticking the driver |
| `runAngleUiTests` | `test_angle_ui.cpp` | TOOLS and ANGLE METER through the real UI, the IMU driver and the QMI8658 model: MENU's TOOLS row and the chooser, the driver opened and CTRL7 00h on BACK, the angle, the tilt, TOO STEEP, SETTLED and MOVING, stale, no part, another part and a part lost, ZERO and its 10 s limit, MAX and RESET MAX, the temperature warning, SETUP's rows, LENGTH's steps and its save, FLIP CAL's steps, touches ignored screen down, HOLD TO KEEP and its save, the widest texts |
| `runAngleModelTests` | `test_angle_model.cpp` | the angle meter's model on synthetic gravity vectors: the axis map, `theta` and the tilt, TOO STEEP and amber, frames, stale, SETTLED, ZERO and its 10 s limit, MAX, the wrap at ±180°, the travel, the temperature since the zero, the calibration and the errors it removes, the flip's arithmetic and steps, NaN and degenerate inputs, the texts in tenths |
| `runUsbMscTests` | `test_usb_msc.cpp` | card handover state machine, RPM smoothing |
| `runDshotFrameTests` | `test_dshot_frame.cpp` | DShot frame layout, checksum, inverted checksum, EDT enable word, throttle map (from rcbench) |
| `runDshotTelemTests` | `test_dshot_telem.cpp` | line and group code, eRPM and EDT decode, checksum against single-bit errors, sampler (from rcbench); three-state poll, counters, every normalised eRPM frame decoded with EDT on |
| `runDshotEdtTests` | `test_dshot_edt.cpp` | extended telemetry schedule against an AM32 model (from rcbench) |
| `runEscCmdTests` | `test_esc_cmd.cpp` | the command slot on core1 against AM32, Bluejay and BLHeli_S models: a hold's frames, the reply check frame by frame, lost and damaged frames, the arm, eRPM, settle and wait guards, the EDT enable's order, the beacons' gaps, a `millis()` wrap, a bind, a teardown and a drain |
| `runKvEstTests` | `test_kv_est.cpp` | the tester's RPM/V and kV estimate (`src/kv_est.h`, slice 3 of [esc-commands.md](docs/design/esc-commands.md)): the rounding, the clamp, the 0.5 V floor, the pole count, the 75 % minimum and the 2000 maximum, the 1000 ms settle and the `millis()` wrap, the 3D note, the texts and their widths |
| `runSdioTests` | `test_sdio.cpp` | SDIO driver: CRC7 and CRC16 vectors, framing, responses, CSD, four-bit packing, CRC status token; initialisation, reads and writes against a simulated card; one block written without waiting on busy (`sdh_write_start()`, `sdh_write_poll()`); the wait hook called from the busy wait |
| `runClaimTests` | `test_claim.cpp` (includes `core1_job.cpp`) | pin claim refusals at their 500 ms boundaries and their order, the handover decisions of `escTaskPoll()` one 1 ms slot at a time, the zero-throttle drain through the claim, Restoring, the order of the two cores' steps against fake jobs, BACK in every state, RETRY, the unplug-page rule, restore wiring, the job start guard, the heartbeat rule across a millis() rollover, no settings save during a claim, the leave page per claim (`pinClaimWantLeavePage()`: an AM32 claim without it resumes DShot within `pinClaimEnd()`, with it waits for RESTART DSHOT, set only by the holder before it leaves, gone with the claim), the AM32 screen on the claim |
| `runServoTests` | `test_servo.cpp` (includes `ui_servo.cpp`) | the servo model, the PWM driver against a register fake, the job core and the job through the claim, the slider, and the SERVO, SET and leave screens through the UI, SET's hold for a profile further past its travel, SET's BACK guard on a change waiting for HOLD TO APPLY, BACK drawn disabled on the leave page in every phase, BACK TO SERVO live once the job lets go and RESTART DSHOT 1000 ms later, the switched supply's strip line and the two-servo hint, the cuts on the current sensor's readings with their chips and notes, with screenshots |
| `runKstTests` | `test_kst.cpp`, `test_kst_seek.cpp`, `test_kst_job.cpp` (includes `kst_job.cpp`), `test_kst_files.cpp`, `test_kst_view.cpp`, `test_kst_ui.cpp` | the search against rcbench's simulated servo: power-ups at 0 s, 3.0 s and 29.9 s found on the next attempt, attempts of 116.6 ms to 118.8 ms, no frame within 3 ms of a window, the 30 s timeout across a clock wrap, STOP SEARCH mid-frame, the heartbeat lapse, a reply at a bad rate kept and not found, the loss rule; the core1 job from the job table on the simulated line: the bind and its four refusals, search, RETRY, timeout, the heartbeat, read, verify, edit, restore, abort, a lost servo, leave and release with 100 ms of low line, the command block, each command in each state, and no call of `kst_session_write()`, `kst_plan_edit()` or `kst_plan_restore()`; the files over an in-memory FAT volume: the mount rule, no card and a USB-held card with no FatFs call, the first write's backup and record, an old record kept and never removed when it stays, a rename reported failed that took effect, a rename that left both names, a full card, a write-protected card as this board reports it and as FatFs can, a full directory, a file or directory in a file's way, a one-time write or sync error with no file or new directory left behind, a close error that leaves its file locked, the record found and loaded, the backup and an old record in `BACKUP/`, SAVE AS in `KST/` up to 50 files; what the screen shows: 27 rows in four groups, each value with its unit, the stepper within the hard limits, a text for every rule, result and file error; the screen through MENU: the claim, the search, STOP SEARCH and RETRY, the read, an edit and REVERT, `WRITE...` opening nothing while pressed or on a slide off and the write page on a tap, the backup and the record before the first write frame, the verify and the record removed, a hard limit written as its limit, the write waiting for the backup's flush and a failed flush, FILES with SAVE AS, LOAD, a backup row and its LOAD, SAVE AS refused at 50 files, a refused file, a write in doubt and `RESTORE...` with the restore page's hold, BACK in every view, disabled during a write and during its flush, the leave page in each phase, BACK drawn disabled on it, STAY, NEXT SERVO, the 2 s off, LEAVE and RESTART DSHOT 1000 ms after the release, a lost servo, no card, a USB-held card, a card that takes no file, an unknown layout read-only, a record found at entry; the card phase: `CardBegin` and `CardEnd` in each job state, the card stamp and its 15 s cap, the first write's card calls inside a phase |
| `runHitecTests` | `test_hitec.cpp`, `test_hitec_job.cpp`, `test_hitec_files.cpp`, `test_hitec_view.cpp`, `test_hitec_ui.cpp`, `fake_hitec_line.cpp` | the Hitec core1 job against a simulated HS-5645MG and D485HW: the bind and its refusals, the entries with and without the switched supply, the full read and its backup, writes of each family, a read-back that differs, a write cut by a lost servo, Abort, the heartbeat, LEAVE, the release's 2,000 ms off, the command block, the card phase; the files on the card against the in-memory volume: SAVE AS, LOAD, the first write with and without an old record, a later write's record, the removal; the screen's rows, values, stepper, soft rules and texts, the write page's layout (`hitecPageLayout()`) with the edges the screen cannot reach; HITEC SERVO through MENU and PROGRAMMER: the claim on the entry's tap, the READ wait of a D series servo on its own supply and the job's start at once otherwise, the entry and read, an edit, the ID note in the editor bar and on the write page, the write page under a full set of notes: the horn line, the ID note, FILE OF ANOTHER MODEL and MODEL NOT MEASURED always, the ID row first, AND n MORE, `WRITE...` and `RESTORE...` opening their pages on a tap, the write and its result, BACK disabled during the write and during its flush, FILES, REPAIR, a stall, the leave page in each phase with STAY, READ AGAIN, LEAVE, NEXT SERVO and RESTART DSHOT 1000 ms after the release, BACK drawn disabled on it through the 2 s off |
| `runSbusJobTests` | `test_sbus_job.cpp`, `sbus_line.h` | the S.BUS job's core on a line model of a receiver: nothing counted before 2000 µs of quiet line and counted from 2000 µs, frames at 7 ms and 14 ms, the mean interval over 16 and its restart after 100 ms, the flags, each UART error flag and its rank, overrun, footers, a cut frame, the gap at 2000 µs and not 1999 µs, the micros() wrap; the job from the job table through the claim on the UART0 fake: the pad and the UART set up, GP0 untouched, streams with a pass every 1000 µs, a late pass, the FIFO's overrun, the heartbeat lapsed, each wiring, NO GP1, CLOCK and RETRY, KISS live, an ESC on SIG, busy while held, the release and the cleared status, the unplug page, a bind 1 ms into a live frame, quick and slow, a new visit |
| `runBridgeTests` | `test_bridge.cpp`, `fake_bridge_line.cpp` | the USB bridge's job on a line fake with the PIO model's cycle counts and a model ESC: the clock divider and its lower bound, the frame slot's refusals and handover, a frame whole on the wire with passes 1 ms and 5 ms apart, the echo kept and dropped, a differing and a lost echo counted once per frame, the receive FIFO and the ring full, the ring's wrap and nothing read while closed, the release finishing the frame under way, dropping one not started and closing at 250 ms and not 249 ms; `PinUser::Bridge`'s row and claim rules; the claim through the job table from Stopping to Free, DShot asked back only once the job is idle and never bound while the line is open, Free with only `pinClaimState()` after BACK, BACK while Stopping, SIG on GP1, the leave page given and kept, RETRY after a refused bind |
| `runUsbBridgeTests` | `test_usb_bridge.cpp`, `fake_cdc.cpp`, `fake_bridge_line.cpp` | the USB bridge's USB side on a CDC fake with 64-byte packets and TinyUSB's buffers and the real `usb_dev.cpp`, through the claim and the job on the line fake: each PC frame named, the greeting not a RUN, a SET BUFFER's payload; DshotDisplay's bridge tests ported: a frame whole on the wire, echo then ACK, the reply's offsets from the end, SET BUFFER not stalling, a late reply during a repaint and a whole one in the ring, the Python tool's 25 ms reads on a 258-byte write, the counts, the input dropped at the open; the AM32 configurator's Direct rule on a model bootloader; the gather, a frame across a 30 ms pass, 1999 µs and 2000 µs, 320 and 80 bytes; the modes, 1999 ms and 2000 ms idle, DTR falling after it was set, a tool that never sets it; the write wait at 2999 ms and 3000 ms and at its answer, the unfinished write and RUN; BACK with the PC connected, nothing read or sent after it; the console off from the open to Free, through the leave page and BRIDGE; a refused bind; a collision; LOST; a payload split by a USB gap and RUN only with its CRC; the run cue from the open, through a RUN, to its eRPM boundary; console text held at the open; a `CLOCK` line setting the clock with the console on the port and going to the wire as data with the bridge on it; no frame handed to the wire outside the claim and DShot never up with the line, on every pass |
| `runBridgeUiTests` | `test_bridge_ui.cpp` | USB BRIDGE through ESC CONFIG and the real UI: the claim on the tap, `RELEASING SIG GP0`, the strip, chip, mode, counters, frames line, last command and six lines with their widths, PORT OPEN and CLOSED, the counts' 250 ms, the closed line, a write with BACK disabled, the leave page's steps, note, BRIDGE and RESTART DSHOT at 1000 ms, a write ended by RUN, BACK while Stopping, a failed bind and RETRY, USB DRIVE and its eject, another claim, LOST, SIG on GP1, the leave page at BACK with no traffic, between an INIT's handover and its answer, after MSP alone and after a read-only session on AM32 and EFM8, without the FLASH note, MENU's run cue, every view's control table, screenshots |
| `runSbusUiTests` | `test_sbus_ui.cpp`, `sbus_line.h` | the S.BUS view at its boundaries: `NO SIGNAL` at 100 ms and not at 99 ms, `NO FRAMES`, `FAILSAFE` above `FRAME LOST`, the 250 ms hold from the rise and not from a fall, across a `millis()` wrap, the chips' texts and fills, the ink of the channels, the bar's fill in raw units, values by `sbus_to_us()`, the four lines and their caps; RX SIGNALS through MENU: title, strip, MONITOR and OUTPUT with their lines, OUTPUT disabled and unpressed, the claim on MONITOR's tap; S.BUS MONITOR on the UART0 fake with core1 passes every 500 µs: Stopping and the caption, the empty bars, frames at 14 ms and 7 ms with every bar and value, CH17, FRAME LOST held and gone, failsafe in red, the receiver gone in grey, NO FRAMES on a refused footer, parity, framing and break counts, the leave page releasing and released, BACK TO MONITOR with the counts and the hold cleared, RESTART DSHOT at 1000 ms and not at 975 ms with GP1 left plain; GP1 as SIG with the ESC check and its end at 500 ms, the leave page's GP1 lines; KISS on GP1; no role on GP1; the job's NO GP1; the clock refused and RETRY; the Busy refusal before the ESC check; BACK while Stopping; every frame's control table |
| `runSbusOutJobTests` | `test_sbus_out_job.cpp` | the S.BUS output's ceiling and roles; its core: SAFE, the START frame for each throttle channel, the clamps, the 14,000 µs schedule with late passes, the 7,000 µs bound, the FIFO's wait and the micros() wrap, the heartbeat trip, a refused frame; the job from the job table through the claim on the UART0 fake's transmitter and the GPIO fake's control and pad registers: the bind write by write, the frames read back by rcbench's decoder, STOP, the release after the last frame and its order, the backstop, the heartbeat at 249 and 250 ms, each wiring, NO GP0, CLOCK and RETRY, the ESC check in both wirings, KISS, MAX THROTTLE's ceiling against a wrong one from the screen; no other code line under `src/` names the output |
| `runWidgetTests` | `test_widgets.cpp` | the list (`ui_list`) and the hold (`ui_hold`) driven with touch snapshots, each rule at its boundary; the header's BACK drawn disabled as `uiButton()` draws a disabled control; slice 1 of menu structure B: the leave page (`ui_leave`), the BACK guard, the two strip lines, the field row, the colour role names and the layout constants |
| `runIdTests` | `test_ids.cpp` | the control tables of `ui_screen.h` on the tester, MENU, ESC COMMANDS at Ready and at NoAnswer, PROGRAMMER, RX SIGNALS with MONITOR acting and OUTPUT not, ESC CONFIG with ESC SETTINGS and USB BRIDGE acting, USB BRIDGE opened by id, TOOLS, ANGLE METER's main view, SETUP and FLIP CAL, SETTINGS, SD LOG in three card states, ESC SETTINGS in its handover and its list, and SERVO's three views: ids unique, rectangles inside the panel and apart, BACK at its rectangle and drawn live or disabled as the table says, the enabled flags against the state they follow; the id helpers of `ui_ids.h`, and that they report an absent or misgated control; the layout table of menu structure B on SETTINGS, SD LOG and SERVO SET: each footer, edge and editor rectangle by id and each label inside its button |
| `runBackupTests` | `test_backup_ring.cpp` | the ring of `BACKUP/`: its rules, saves of two devices and a rename under another device's number, a rename FatFs reports failed that took effect, two files under one number, the list of one device from the latest, a gap, no marker and an unreadable one, a directory under a ring name, names outside the ring, a marker or a read-only oldest file the card refuses, a change of directory that fails, load, no card and a USB-held card with no FatFs call; Bluejay's `BJ` and BLHeli_S' `BH` in the one ring, renamed over other devices' numbers |
| `runSdCacheTests` | `test_sd_cache.cpp` | the SD write cache over a fake card: the queue's order, a read of a queued sector, one swap check per batch, no command while busy, read cache hits and the oldest pushed out, a miss that waits for the block in flight, a full queue, retries, a failed block, a changed serial number, refusals with the cache off |
| `runDiagTests` | `test_diag.cpp` | the restart reason's decoding from CHIP_RESET, the watchdog's REASON, the enable query and the boot type, its order, its texts' widths, the capture through the fakes, the uptime's text |
| `runEscwireJobTests` | `test_escwire_job.cpp` | the ESC settings job of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-3-as-built) from the job table against rcbench's simulated ESC (`test/fake_escwire_port.h`): the bind driving the line high in the same call, refused opens by resource with SIG put on its pull-up and a pin the driver lacks left alone and named PIN, a command held at the bind, Bluejay 208 entered no sooner than 500 ms after, read and left, `00 01` and 300 ms of low line, BLHeli_S on E8B1, commands out of turn, a Stop not yet taken kept, Stop in mode, a silent ESC, the heartbeat stopping a read and an entry, refusing commands and letting a leave finish, the release cutting a read and waiting out a leave; slice 4's write: Bluejay on E8B2 and E8B5 and BLHeli_S written and read back in rcbench's order, the guard's refusals, the read-only pages refused, a changed page, the ESC gone before and after the erase, programs that do not take, a corrupt byte, erases not heard, a driver failure, lapses before and after the erase, the release in a write, and after each the flash outside the settings page unchanged (`fakeEwOnlySettingsPage()`) |
| `runEfm8ViewTests` | `test_efm8_view.cpp` | the rows of BLHeli_S 32 and 33 and Bluejay 33, 200 and 208 by group and offset, every layout listed whole, the values in capitals, DIRECTION's 3D words and `RAW`, the narrowest gap between a label and its value over every value of every row, 23 px, the strip's caption and read-only lines, a version too long left out, the refusals' statuses and lines with their widths, the selected rows' notes; the read-only lines with their backup, the editable rows of BLHeli_S 33 and Bluejay 208, a field's steps and coarse factor, the changes counted, every write result's lines and their widths; the backup's text, read back by rcbench's parser, of each page kind, and its save as `BJ001.TXT` |
| `runRunSummaryTests` | `test_run_summary.cpp` | the run summary of [run-summary.md](docs/design/run-summary.md#slice-1-as-built) through escMerge() at 500 Hz: held maxima with one and two outliers not counted and three counted, the raw extreme, a value without a new arrival counted once, stale eRPM, KISS and EDT at their windows, a KISS gap filled by EDT tagged `MIXED` with the held value's own source, KISS at 0 giving EDT's figure, the minimum voltage while armed with 0.00 V ignored and a disarm restarting the run, maxima over the whole log, KISS eRPM only without DShot eRPM, the counter across an ESC restart, a gap and a stale frame from before the start, the charge from current against hand sums with 600 ms and 1200 ms stalls, armed time with edges, 75 minutes past the `micros()` and `millis()` wraps, the ends and the file states, the `.TXT` name, the note's example text byte for byte, the clock lines not set, set and set during the log, the longest text under 1000 bytes, SD LOG's row texts and their fits |
| `runClockTests` | `test_clock.cpp`, `fake_i2c.cpp` | the clock of [run-summary.md](docs/design/run-summary.md#slice-4-as-built) on the I2C fake's PCF85063A model: BCD, the calendar from 2000 to 2099, the decode and its causes' order, the text, the FAT packing at its year limits, the boot read and the kept time, OS's software reset, the other causes left alone, the bus errors at boot and a stuck bus, the INA219 and the CST328 beside it; the weekday, the set and its read-back, the set on a failing bus, the `CLOCK` line's parser and reader, a set during a log ([slice 5](docs/design/run-summary.md#slice-5-as-built)) |
| `runMenuTests` | `test_menu.cpp` | MENU, SETTINGS and the navigation through the real UI: SD LOG's BACK to MENU, every registry entry opened and left, MENU's disarm, ESC COMMANDS as its first row, ESC CONFIG, SERVO, SERVO PROGRAMMER and SETTINGS drawn disabled while the claim restores, MENU's scroll over registries of six and seven rows and its return at 0 px, the eight rows by device with TOOLS whole at 0 px and SD LOG and SETTINGS reached with a drag, TOOLS drawn disabled while the claim restores, RX SIGNALS acting with no status while the claim is Free and drawn disabled while it is Stopping or Held, its chooser opened and left, ESC CONFIG as MENU's second row, drawn disabled while a claim holds, its chooser claiming nothing, its title, strip, entries and lines, USB BRIDGE acting and opening the bridge with the claim on its tap, the claim on ESC SETTINGS' tap and their BACK to MENU, PROGRAMMER's entries, lines and BACK with SERVO SUPPLY at OWN, GP28 and GP29, every MENU status and SETTINGS value within its widest, the SETTINGS scroll range, each row by id, DIAGNOSTICS' LAST RESTART and UPTIME, a cut control under 40 px, HOLD TO SAVE and the UNSAVED note at every scroll position, HOLD DEFAULTS' 1 s hold, its `DEFAULTS LOADED - RESTART TO UNDO` note and what ends it, ESC SETTINGS' own wait beside another claim, SERVO SUPPLY as a core section, the strip following AUX and the claim |
| `runEscCmdUiTests` | `test_esc_cmd_ui.cpp` | ESC COMMANDS through MENU against the fake command slot: its row and its refusal while a claim holds, the layout and every text's width, each control at each readiness, BEEP 1 to 5, the four holds at 999 and 1000 ms with the save, each outcome's lines, the readiness line beside an outcome, the 3D note set and kept, BACK while a request waits |

The fake critical section in `test/fakes.cpp` marks a section at
`critical_section_init()` and counts each `critical_section_enter_blocking()`
or `critical_section_exit()` on an unmarked one. `main()` calls
`core1JobInit()` before the first suite, as `src/main.cpp` does before core1
starts, and ends the run with the check `uses before critical_section_init()`,
which fails unless the count is 0. Removing the `critical_section_init()` call
of `core1_job.cpp`, `servo_job.cpp`, `kst_job.cpp`, `hitec_job.cpp`,
`sbus_job.cpp`, `sbus_out_job.cpp` or `sense.cpp` fails that check.
`esc_task.cpp` is not linked into the host suite (PIO and UART), so its
`critical_section_init()` is not covered by it.

`make run` also builds and runs `test/test_usb_msc_cb.cpp`, a program of
its own that links the real `src/usb_msc.cpp` against stubs of the logger,
the card and TinyUSB and drives its callbacks: the suite links a fake of that
file's runtime half for the screens. It builds and runs
`test/test_bridge_log.cpp` too, `src/bridge_log.cpp` and
`src/usb_bridge.cpp` built with `USB_BRIDGE_LOG` 1 against the CDC fake: the
dump's every line, a PC that stops reading, the dump before the console.

`test/servo.mk`, `test/kst.mk`, `test/hitec.mk`, `test/rx.mk`,
`test/sense.mk`, `test/escwire.mk`, `test/angle.mk` and `test/bridge.mk` list
each feature's sources and test files for `test/Makefile`, as
`src/servo.cmake`, `src/kst.cmake`, `src/hitec.cmake`, `src/rx.cmake`,
`src/sense.cmake`, `src/escwire.cmake`, `src/angle.cmake` and
`src/bridge.cmake` do for
`CMakeLists.txt`. A feature's C
sources and C test programs are built as C11:
`make run` also runs rcbench's `test_servo_sweep.c` (14 cases), its move
test `test_servo_move.c` (18 cases), copied unchanged, and the
adapted `test_out_pwm_map.c` (5 cases) from `test/rcbench/`, and rcbench's KST
tests, copied unchanged: `test_kst_wire.c` (28 cases), `test_kst_reg.c` (51),
`test_kst_plan.c` (40), `test_kst_session.c` (77), `test_kst_pio.c` (17),
`test_kst_file.c` (57) and `test_kst_store.c` (28), and rcbench's Hitec
tests, copied unchanged: `test_hitec_wire.c` (11 cases), `test_hitec_pre.c`
(18), `test_hitec_pio.c` (23), `test_hitec_reg.c` (14),
`test_hitec_reg_plan.c` (9), `test_hitec_session.c` (15),
`test_hitec_session_write.c` (13) and `test_hitec_card.c` (12), and
`hitec_headers.cpp`, the core's headers compiled as C++11, and rcbench's
S.BUS test, copied unchanged: `test_sbus.c` (19 cases), and
rcbench's escwire tests, copied unchanged: `test_ew_a.c` (9 cases),
`test_ew_id.c` (9), `test_ew_map.c` (9), `test_ew_guard.c` (15),
`test_ew_session.c` (13), `test_ew_session_write.c` (26), `test_ew_file.c`
(28), `test_ew_file_load.c` (10), `test_ew_pio.c` (28), `test_ew_text.c`
(20), `test_ew_session_text.c` (34) and `test_ew_file_e32.c` (9), and
`escwire_headers.cpp`, the core's headers compiled as C++11, and rcbench's
angle meter tests, copied unchanged: `test_servo_rudder.c` (4 cases),
`test_imu_angles.c` (7) and `test_witmotion.c` (14), and adapted from rcbench's
to the cases that need `imu.h` alone, `test_imu.c` (9) and `test_imu_filter.c`
(10). CI holds the KST, Hitec
and escwire line fixtures to pioasm's words.

Other targets in `test/Makefile`: `syntax` typechecks the device-only sources
with `-Werror`; `pioemu` runs `test/pio_sdio_emu.py`, the SDIO PIO programs
assembled from `src/sdio/sd_pio.pio` and run one system clock at a time
against a card model, 18 runs over three clock rates, three card output
delays and two card latencies, `test/pio_backlight_emu.py`, the backlight
PIO program at every level from 0 to 255 and on 20 writes mid-period, and
`test/pio_bridge_emu.py`, the USB bridge's two programs on one pad, 42 runs,
and `make` runs all three; `bbcheck` encodes 2000 frames and decodes them with
Betaflight's `blackbox_decode`; `kstcount` runs `src/kst_files.cpp` and
`src/hitec_files.cpp` over FatFs R0.15 from the SD library's pinned commit,
fetched into `test/obj/`, on a RAM disk that counts blocks, and checks the
tables in [kst-programming.md](docs/design/kst-programming.md) and
[hitec-programming.md](docs/design/hitec-programming.md), which
`kstcount-write` rewrites; `run` also runs `test/test_set_clock.py`, the
host tests of `tools/set_clock.py` with a fake port, without pyserial;
`lognum` runs `src/sd_log_next.cpp`, and
`src/sd_log.cpp` with its run summary, over the same FatFs on a RAM disk
with two card images and write faults (`test/ram_disk.*`); `previews`
and `wiki` rebuild screenshots; the fake clock is not reset between tests, so
a shot of a screen that draws from `millis()` (the KST and ESC SETTINGS
connect spinners, SETTINGS' UPTIME) sets it first with `fakeClockAlign()` or
`fakeClockSetMs()` and does not change when a test is added or removed;
`coverage` builds an instrumented suite. Scripts: `test/check_docs.py`,
`test/check_uf2.sh`, `test/check_uf2_selftest.sh`, `test/check_blackbox.py`,
`test/pio_sdio_emu.py`, `test/pio_backlight_emu.py`, `test/pio_bridge_emu.py`,
`test/kst_card_count.cpp`, `tools/check_wiki.py`, `tools/coverage.py`,
`tools/make_previews.py`, `tools/make_wiring.py`, `tools/notices.py`,
`tools/stack_depth.py`, `test/test_set_clock.py`. The tool
`tools/set_clock.py` sets the board's clock over USB.

**CI** (`.github/workflows/ci.yml`; on push to `main` or `master`, on pull
requests, by hand):

| Job | What it does |
| --- | --- |
| Firmware (`touch-lcd-2.8`) | builds with arm-none-eabi-gcc 13.3 and `STRICT=ON`; flash under 1 MiB and RAM (random-access memory) under 400,000 bytes; checks from the DWARF compile units that no file of the SD library's SDIO, ZuluSCSI-firmware or SdFat directories is in the image; writes `THIRD_PARTY_NOTICES.txt` with `tools/notices.py` and checks it against the image; compares the PIO models' assembler with the SDK's `pioasm`, for the USB bridge's line too; holds the KST, Hitec and escwire line fixtures to `pioasm`'s words; checks the deepest stack chains of the Hitec, ESC settings and S.BUS output jobs against 1,792 bytes (`tools/stack_depth.py`); checks the two pin overrides reach CMake and that `-DDSHOT_PIN`, `-DKISS_PIN`, `-DDSHOT_PIN_LCD_2`, and `-DKISS_PIN_LCD_2` are refused; validates the UF2 |
| Host tests | `make`, which runs the SDIO, backlight and USB bridge PIO models and `test/test_set_clock.py` too; the suite again with both pin macros overridden; `make kstcount` against the KST and Hitec design notes' card-phase tables; `make lognum`, the next log's file and the logger with its run summary over FatFs; wiki screenshots and `check_wiki.py` |
| Blackbox round-trip | builds `blackbox_decode` from source, runs `make bbcheck` |
| Config permutations | typechecks all of `src/` with `-Werror` for 32 combinations, with the headers in `lib/rcbench/` on the include path: AM32 transmit mode × AM32 low-jump × rotation 0 or 2 × KISS on or off × SD logging on or off; checks the landscape guard fires |
| Workflow lint | actionlint 1.7.7 and a duplicate-key check on every workflow in `.github/workflows/` |
| Docs | `test/check_docs.py`, its rule 9 refusing a screen text, view or image menu structure B removed in README.md and the wiki; `tools/make_wiring.py --check` against `docs/wiring.svg`, and its refusal of nine changed sources for the INA219's connector and address (`SENSOR_MUTANTS`); then Doxygen with zero warnings allowed |
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
A tag with a suffix, such as `v0.1.0-rc1`, is published as a GitHub
pre-release. Each run is titled `Release <tag>`, which Docs reads to find
the tag. The version is 0.1.0. The one MiniRcBench release is the
pre-release v0.1.0-rc1 of 2026-11-14, the tag at a93e1c1, an image for
testing with no manual published, by the owner's decision of 2026-11-14;
its image has not run on hardware. v0.1.0 follows the bench checks.

**Docs** (`.github/workflows/docs.yml`): mirrors `wiki/` to the GitHub wiki
after every successful Release run of a tag without a pre-release suffix,
from a tag push or started by hand, and can be started by hand itself. A
pre-release publishes no manual. The wiki is
published per release on purpose, so it describes firmware that can be
downloaded.

Not covered by any job: hardware behaviour, `tools/sdtest`, the DShot and
KISS PIO programs beyond compilation, and the SDIO ones beyond the cycle-level
model.

<!-- coverage-table:start -->
| File | Lines | Covered | Coverage |
| --- | ---: | ---: | ---: |
| `src/am32_backup.cpp` | 54 | 54 | 100.0% |
| `src/am32_bl.h` | 16 | 16 | 100.0% |
| `src/am32_eeprom.cpp` | 125 | 124 | 99.2% |
| `src/am32_eeprom.h` | 11 | 11 | 100.0% |
| `src/angle_model.cpp` | 255 | 255 | 100.0% |
| `src/backup_ring.cpp` | 154 | 152 | 98.7% |
| `src/bl_answer.cpp` | 35 | 35 | 100.0% |
| `src/blackbox_encode.cpp` | 113 | 113 | 100.0% |
| `src/bridge_4way.cpp` | 304 | 288 | 94.7% |
| `src/bridge_log.h` | 5 | 5 | 100.0% |
| `src/bridge_msp.cpp` | 144 | 132 | 91.7% |
| `src/bridge_wire.cpp` | 125 | 125 | 100.0% |
| `src/card_dir.cpp` | 96 | 96 | 100.0% |
| `src/clock.cpp` | 124 | 124 | 100.0% |
| `src/clock.h` | 14 | 14 | 100.0% |
| `src/clock_line.cpp` | 64 | 63 | 98.4% |
| `src/core1_job.cpp` | 70 | 70 | 100.0% |
| `src/cst328.cpp` | 69 | 69 | 100.0% |
| `src/diag.cpp` | 31 | 31 | 100.0% |
| `src/dshot/dshot_edt.c` | 33 | 33 | 100.0% |
| `src/dshot/dshot_frame.c` | 25 | 25 | 100.0% |
| `src/dshot/dshot_telem.c` | 143 | 143 | 100.0% |
| `src/efm8_backup.cpp` | 36 | 36 | 100.0% |
| `src/efm8_view.cpp` | 182 | 181 | 99.5% |
| `src/esc_3d.cpp` | 2 | 2 | 100.0% |
| `src/esc_merge.cpp` | 44 | 44 | 100.0% |
| `src/esc_task.h` | 262 | 262 | 100.0% |
| `src/escwire_job.cpp` | 152 | 150 | 98.7% |
| `src/gfx.cpp` | 213 | 211 | 99.1% |
| `src/greet_cycle.cpp` | 9 | 9 | 100.0% |
| `src/hitec_files.cpp` | 248 | 237 | 95.6% |
| `src/hitec_job.cpp` | 419 | 409 | 97.6% |
| `src/hitec_view.cpp` | 265 | 261 | 98.5% |
| `src/kiss_telem.cpp` | 37 | 35 | 94.6% |
| `src/kst_files.cpp` | 163 | 158 | 96.9% |
| `src/kst_job.cpp` | 435 | 417 | 95.9% |
| `src/kst_seek.cpp` | 126 | 126 | 100.0% |
| `src/kst_view.cpp` | 160 | 154 | 96.3% |
| `src/kv_est.h` | 56 | 56 | 100.0% |
| `src/log_ring.cpp` | 43 | 39 | 90.7% |
| `src/move_timer.cpp` | 199 | 199 | 100.0% |
| `src/pin_claim.cpp` | 132 | 132 | 100.0% |
| `src/pin_claim.h` | 15 | 14 | 93.3% |
| `src/plat.h` | 10 | 10 | 100.0% |
| `src/qmi8658.cpp` | 142 | 142 | 100.0% |
| `src/qmi8658.h` | 7 | 7 | 100.0% |
| `src/rpm_filter.h` | 17 | 17 | 100.0% |
| `src/run_summary.cpp` | 361 | 357 | 98.9% |
| `src/run_summary.h` | 2 | 2 | 100.0% |
| `src/sbus_job.cpp` | 109 | 109 | 100.0% |
| `src/sbus_job.h` | 5 | 5 | 100.0% |
| `src/sbus_out_job.cpp` | 164 | 164 | 100.0% |
| `src/sbus_view.cpp` | 73 | 73 | 100.0% |
| `src/sd_log.h` | 34 | 34 | 100.0% |
| `src/sdio/sd_cache.c` | 132 | 129 | 97.7% |
| `src/sdio/sd_host.c` | 214 | 214 | 100.0% |
| `src/sdio/sd_proto.c` | 135 | 135 | 100.0% |
| `src/sense.cpp` | 216 | 216 | 100.0% |
| `src/sense.h` | 3 | 3 | 100.0% |
| `src/servo_job.cpp` | 343 | 343 | 100.0% |
| `src/servo_model.cpp` | 198 | 198 | 100.0% |
| `src/servo_pwm.cpp` | 55 | 55 | 100.0% |
| `src/servo_supply.cpp` | 40 | 40 | 100.0% |
| `src/settings.cpp` | 238 | 237 | 99.6% |
| `src/supply_guard.cpp` | 41 | 41 | 100.0% |
| `src/theme.cpp` | 11 | 11 | 100.0% |
| `src/touch.h` | 4 | 4 | 100.0% |
| `src/ui.cpp` | 538 | 537 | 99.8% |
| `src/ui.h` | 1 | 1 | 100.0% |
| `src/ui_am32.cpp` | 1179 | 1152 | 97.7% |
| `src/ui_angle.cpp` | 360 | 355 | 98.6% |
| `src/ui_bridge.cpp` | 247 | 243 | 98.4% |
| `src/ui_esc_cmd.cpp` | 196 | 191 | 97.4% |
| `src/ui_esc_config.cpp` | 48 | 48 | 100.0% |
| `src/ui_hitec.cpp` | 1132 | 1074 | 94.9% |
| `src/ui_hold.cpp` | 36 | 36 | 100.0% |
| `src/ui_input.cpp` | 27 | 27 | 100.0% |
| `src/ui_kst.cpp` | 1161 | 1126 | 97.0% |
| `src/ui_leave.cpp` | 78 | 78 | 100.0% |
| `src/ui_list.cpp` | 141 | 141 | 100.0% |
| `src/ui_menu.cpp` | 117 | 116 | 99.1% |
| `src/ui_rx_signals.cpp` | 49 | 49 | 100.0% |
| `src/ui_sbus.cpp` | 235 | 234 | 99.6% |
| `src/ui_screen.h` | 8 | 8 | 100.0% |
| `src/ui_sdlog.cpp` | 253 | 249 | 98.4% |
| `src/ui_servo.cpp` | 690 | 690 | 100.0% |
| `src/ui_servo_prog.cpp` | 65 | 65 | 100.0% |
| `src/ui_settings.cpp` | 444 | 439 | 98.9% |
| `src/ui_slider.cpp` | 109 | 109 | 100.0% |
| `src/ui_tools.cpp` | 45 | 45 | 100.0% |
| `src/ui_widgets.cpp` | 76 | 76 | 100.0% |
| `src/usb_bridge.cpp` | 342 | 337 | 98.5% |
| `src/usb_dev.cpp` | 37 | 35 | 94.6% |
| `src/usb_msc.h` | 37 | 35 | 94.6% |
| **Total** | **15413** | **15152** | **98.3%** |

Generated by `tools/coverage.py`. CI runs it with `--check` and fails on drift.
<!-- coverage-table:end -->
