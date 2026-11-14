# Where MiniRcBench stands

The running record: what exists, how far each part is verified, and what is
open. Two parts of this file are held to the tree by a checker. Every relative
link here and in [CHANGELOG.md](CHANGELOG.md) is checked by
`test/check_docs.py`. The coverage table under "Tests and CI" (continuous
integration) is written and checked by `tools/coverage.py`. Nothing else is
checked: the architecture figures, pin numbers, suite and job lists and
verification states are written by hand, against the code and the issue tracker
as of 2026-11-14. The KST and Hitec card-phase tables in
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
servo supply whose enable is on GP28 or GP29. Steps 1 to 5 of
[kst-programming.md](docs/design/kst-programming.md) are built:
rcbench's KST core, line driver and file store are copied into `lib/rcbench/`,
with rcbench's tests of them, 298 cases; the search (`src/kst_seek.h`) and the
core1 job (`src/kst_job.h`) run the session on the line, tested against
rcbench's simulated servo; `src/kst_files.h` puts the SAVE AS files and the
record of a write in `KST/` through FatFs, and the session's backup into the
ring of `BACKUP/` that the AM32 editor shares (`src/backup_ring.h`,
[backups.md](docs/design/backups.md)), tested against an in-memory volume;
`src/ui_kst.cpp` is MENU → KST SERVO, run through MENU in the host suite.
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
36 px tall and every tool's BACK returns to MENU.
Menu structure B ([menu-b.md](docs/design/menu-b.md)), the owner's choice of
2026-11-13, has slices 1 to 4 and 8 of 10 built. Slice 1: the leave page widget
(`src/ui_leave.*`), the BACK guard and two strip lines (`src/ui_widgets.*`),
the field row (`src/ui_list.*`), the colour role names and the layout
constants; no screen uses the leave page or the guard yet. Slice 2: the uses
that decision 11's tables name in their roles' colours (`UI_COL_*`); the rows
of AM32 CONFIG, KST SERVO, HITEC SERVO and SERVO SET drawn through the field
row, a changed value `C_CYAN` with a 3 px bar; square 12 px chips;
`PROG MODE` on blue. Slice 3, names and SETTINGS: SIG PIN and AUX PIN with
their J5 pin, a step of either swapping the two; SERVO SUPPLY a core section
of SETTINGS, its first value `OWN`; MENU's strip `SIG GP0  AUX GP1  DSHOT600`
and, while the claim is not Free, `DSHOT RESTARTING ON SIG GP0` over rows
drawn disabled; `DSHOT600` on the tester; `NOTHING SAVED: DEFAULTS IN USE`;
four KST labels. Slice 4, the layout table (the note's decision 14): the
footers of SETTINGS and SERVO SET, the `-` and `+` of SERVO SET and AM32
CONFIG, AM32's result OK as the wide button, SD LOG's button edge, HOLD
DEFAULTS in place of HOLD RESET, the red HOLD TO SAVE, the write pages'
`CHANGES` and Hitec's 4 rows and strip. Slice 8: SERVO TESTER titled
`SERVO TESTER`, its MENU label, on the main view and the unplug page. The
owner answered the note's eight questions on 2026-11-14; slices 5 to 7, 9
and 10 wait on being built (open item 18).
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
| Work | USB (Universal Serial Bus) device task, touch, the INA219 current sensor read after touch, UI (user interface) rendering, SD (Secure Digital) card logging, settings, serial telemetry dump, the pin claim's state machine | DShot frame pump, eRPM and EDT decode, KISS request and receive; while a pin claim holds, the claiming screen's job instead: the servo job for SERVO, the KST job for KST, the Hitec job for HITEC, none for AM32 |
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
job (`src/kst_job.h`) for KST and the Hitec job (`src/hitec_job.h`) for
HITEC; the AM32 transport runs on core0.
Leaving rebuilds the pump from the wiring it had, disarmed. For SERVO, KST and
HITEC the claim restarts DShot only after a page that tells the user to unplug
the servo.

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
(Futaba's serial bus for receiver channels) decoder and the current sensor's
bus record, `sense_bus.c`, are rcbench files at commit 5ef42efd, all copied
unchanged into `lib/rcbench/`
([lib/rcbench/README.md](lib/rcbench/README.md)). The firmware does not build
the S.BUS decoder yet. The SDIO driver in `src/sdio/` is part of the tree,
MIT, written from the SD Association's Physical Layer Simplified
Specification v9.10 and the RP2350 datasheet
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
| SD card | PIO SDIO on pio1, 4 bits, 10 MHz, GP19 to GP24 |
| GPIOs free for the two wires | 2: GP0, GP1 (`BOARD_FREE_GPIO_MASK` `0x00000003`), the pins of UART0 (universal asynchronous receiver-transmitter 0), which is unused |
| ESC signal default | GP0, J4 pin 9 and J5 pin 2 |
| KISS default | GP1, J4 pin 10 and J5 pin 1, off |
| Switched servo supply enable | GP28, J4 pin 11, or GP29, J4 pin 12, under SETTINGS → SERVO SUPPLY; none by default |
| Signal pin level | GP0 and GP1 are "Digital IO (FT)", fault tolerant: 5.5 V while IOVDD, the I/O supply, is at 3.3 V, 3.63 V while it is at 0 V (RP2350 datasheet, Table 1674). GP28 and GP29 are ADC (analog-to-digital converter) pins and not fault tolerant; they carry only the enable, a 3.3 V output |
| Battery sense | GP27, ADC input 1, 3:1 divider |
| Power latch | GP26, asserted first thing in `setup()` |
| Other devices | PCF85063 RTC (real-time clock), I2S (Inter-IC Sound) audio codec, neither used |

The ESC and KISS pins are changeable on MENU → SETTINGS within the free set,
and the two wires never share a pin.

**Mechanical model.**
[hardware/mechanical/models/rp2350_touch_lcd_2_8.step](hardware/mechanical/models/rp2350_touch_lcd_2_8.step)
is a STEP (Standard for the Exchange of Product model data) facsimile of the
board, 50.54 × 73.06 × 10.03 mm, 662 solids: glass, display stack, board with
its cut-outs, 4 standoffs, the two flat cables and every rear part 1.0 mm or
more long. Waveshare publishes no 3D file of this board. The stack, the
outline and the connectors are measured in Waveshare's STEP file of the
ESP32-S3-Touch-LCD-2.8, which shares them; the other parts are placed from
Waveshare's rear photo, good to 0.3 mm. No dimension is verified with a
caliper on a board. Sources, limits and commands are in
[hardware/mechanical/README.md](hardware/mechanical/README.md).
`hardware/mechanical/gen_models.py` writes the file from
`touch_lcd_2_8.py`, and its `--check` runs in the Docs job. Read back with
OpenCASCADE (cadquery-ocp 8.0.1.1.0) by `check_models.py`, outside CI: 662 valid solids, none
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
| KISS telemetry: request, PIO receiver, decode, merge, KISS/EDT tags | CRC, decode, resynchronisation, request payload, merge; the PIO receiver is typechecked only | hardware-tested (owner report) with `pico-bidir-dshot`; see below |
| DShot bitrate label in the status bar, `DSHOT600`, `DSERR` | label at each bitrate, colour, `DSERR` | not recorded |
| SDIO card driver in `src/sdio/`: protocol, PIO and DMA bus, library callbacks | CRC7 and CRC16 against the specification's examples, command framing, response parsing, CSD (card-specific data register), four-bit packing, CRC status token; initialisation, reads and writes against a simulated card (standard capacity SDSC v1 and v2, high capacity SDHC, no card, voltage mismatch, timeouts, removal); the two PIO programs run in a cycle-level model against a card model (`test/pio_sdio_emu.py`: CMD0, R1, R2, single and multiple block reads, CMD12, a write and its CRC status token, at 400 kHz, 10 MHz and 25 MHz), a model and not proof of hardware timing; the bus code in `sd_pio.c` is compiled only, and the model's host side follows it by hand | not tested on hardware; open item 6 lists the bench tests |
| Blackbox logging to SD, MENU → SD LOG screen | encoder, ring buffer, round trip through `blackbox_decode`, screen states; how a log ended (`sdLogWriteEnd()`: a short write with `FR_OK` is a full card, any error a failed write), each end's two note lines and their colour, a USB DRIVE refusal ahead of them, START clearing them, a mount clearing a card fault and keeping a tool's stop (`sdLogMountForget()`), a mount hint under a tool's stop, a mount resetting FILE and the counters beside it, AM32 CONFIG's, KST SERVO's and HITEC SERVO's claims recording the stop, the FILE row's `(LAST)`, `SDERR` on an amber chip while armed, a new log's number (`sdLogNameNumber()`, `sdLogNoteName()`, `sdLogNextNumber()`: the name rule in any letter case, four and six digits, other extensions and long names ignored, `LOG00000.BFL`, `LOG99999.BFL` refusing the start with its note), against the fake logger; the directory, the scan, the kept number and the open (`src/sd_log_next.cpp`) over FatFs R0.15 on a RAM disk, FAT32 and exFAT, with a card swapped or emptied after a fault and a write-protected card (`make -C test lognum`): the clearing, the recording and the forgetting on each error in `sd_log.cpp` are typechecked only | hardware use recorded with the previous SDIO driver on the 2.8" board; see below |
| USB card reader (read-only mass storage) | handover state machine, screen; `src/usb_msc.cpp` itself in `test/test_usb_msc_cb.cpp`: `tud_umount_cb()` while serving reclaims the card as an eject does, nothing while idle | not recorded. On the TinyUSB 0.18.0 port a pulled cable reaches the firmware as a suspend, which is not handled (open item 20) |
| AM32 settings editor over the signal wire (AM32 is an open-source ESC firmware) | init string, CRC-16/ARC, 39-field codec, timing advance encodings, write interlock; PROTOCOL zero-based as AM32's `enum inputType`, TEMP LIMIT and CURRENT LIMIT `OFF` where AM32 applies no limit, `3D MODE` at byte 0x12; LOW VOLT CUTOFF, PWM TYPE and BRAKE ON STOP as three values with the third gated by firmware and an unedited byte written back as read; KV, MAX RAMP, MIN DUTY, ABS VOLT CUT and ACTIVE BRAKE on AM32's scales; the strip's notes for 3D MODE, a protection off and a PROTOCOL without DShot, the next step under a write that is not verified, the connect page's lines for a reply it cannot parse and for `NO VALID SETTINGS`, MENU's `POWER-CYCLE THE ESC TO RUN IT` until an eRPM frame after the handshake (`testAm32Guidance()`); the tester's `3D MODE ESC: THROTTLE SCALE DIFFERS` set by a list read and by a write of 3D MODE on that the ESC accepted, its read-back failing or differing included, cleared by a list read and a verified write with it off, kept by an edit not written, a refused write, a failed read, a write of it off not verified, a read that does not open the list and the read-only view of a layout not known, drawn armed and disarmed and beside `NO TELEMETRY`, `SWIPE ACTIVE` in its place while the pad is engaged and the note back on release (`testAm32TesterNote()`); the firmware gates (ABSOLUTE from 2.21, the 200 A limit from 2.21, PWM FREQ above 48 kHz from 2.19), the PROTOCOL note for 2 to 9 (EDT ARM included: a zero frame relocks it), layouts 1 and 5 to 8 opened in the read-only view: the bytes in hex, no field, its strip in each backup state, the backup with no field line saved as it opens, BACK its one control, a hold and taps where the list's controls stand writing nothing, a write forced into the write state refused on the layout (`testAm32ReadOnly()`), bytes 0x2A to 0x2D listed from 1.80, 0x2E from 1.94, 0x2F from 2.16 with a note below 1.80 (AlkaMotors commits 9909ea7, f917c01, aa590e8), ADVANCE in 0..3 below 2.19, every row tapped on 14 firmware versions and three layouts, a stored number outside the firmware's range read as the byte, every field over all 256 bytes on eight firmware versions against a literal table of ranges, arbitrary blocks written back through the screen; the facts behind them read from AM32's source (am32-firmware/AM32 tags v2.05, v2.16, v2.18 to v2.22 and `main` 077b6a4; the configurator at 06955fe; AlkaMotors/AM32-MultiRotor-ESC-firmware tags v1.77 to v2.01), not measured; the screen on the pin claim, the latched GPIO, the drain before the handover; the list on `ui_list` and the swipe of 16 px per coarse step; the backup before the first write (`src/am32_backup.*`): its text and checksum, `BACKUP/AM001.TXT` holding the settings as read at entry, a card that refuses it stopping the write, no second backup in a session | hardware use recorded: ab79c67, bd90e17, both before the screen moved onto the pin claim; the claim and its drain, the swipe of 16 px per coarse step and the backup: not tested on hardware |
| Device backups: the ring of `BACKUP/` (`src/backup_ring.*`), the card calls and the directory scope (`src/card_dir.*`); [backups.md](docs/design/backups.md) | the ring's rules, saves of two devices, a rename under another device's number, the list of one device, gaps, no marker, directories and names outside the ring, refusals, no card and a USB-held card (`runBackupTests`); the card phases over FatFs R0.15 itself (`make -C test kstcount`) | not tested on hardware |
| Blackbox logs in `LOGS/` (`SD_LOG_DIR_PATH`) | the next log's file over FatFs R0.15 (`make -C test lognum`); the rest of `sd_log.cpp` is typechecked only | not tested on hardware |
| SD write cache for the backup claims (`src/sdio/sd_cache.*`, `src/card_cache.*`, the glue in `src/sdio/sd_card_glue.c`, `sdh_write_start()` and `sdh_write_poll()`); [sd-write-cache.md](docs/design/sd-write-cache.md) | the cache against a fake card (`runSdCacheTests`), the asynchronous block write against the simulated card (`runSdioTests`); the KST write waiting for the flush, a failed flush, the claim turning the cache on and off, a running log stopped, AM32's write refused after a failed flush, against a fake of `card_cache.h`; the glue and `card_cache.cpp` are built by the firmware only | not tested on hardware |
| Pin claim (`src/pin_claim.*`) and core1 job dispatch (`src/core1_job.*`), the zero-throttle drain in `src/esc_task.*` | each refusal rule at its 500 ms boundary and their order; the handover decisions of `escTaskPoll()` (`escHandoverStep()`, `escDrainSlot()`, `escPumpFrameSent()`, `escPumpBind()`) one 1 ms slot at a time: 250 zero frames, the teardown 251 ms after the last throttle frame, the bind disarmed; the order of the two cores' steps against fake jobs, BACK and RETRY, Restoring, the unplug-page rule, the restore wiring, the start guard, the heartbeat rule across a millis() rollover; the PIO and pad calls around those decisions are compiled only | not tested on hardware; open item 8 |
| MENU layout (`src/ui_menu.cpp`, `src/ui_registry.cpp`): BEEP THE ESC, AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO, SETTINGS and SD LOG as 40 px rows, scrolled 36 px to reach SD LOG | every registry entry opens and BACK returns to MENU; opening MENU disarms; BEEP's flash and refusal on its row; the rows drawn disabled and `DSHOT RESTARTING ON SIG GP0` while the claim restores, and a tap on a row opens nothing; six rows do not scroll and seven scroll 36 px; SETTINGS, the sixth row, is whole and takes a tap at 0 px, and SD LOG, the seventh, is cut until MENU is dragged; the UNSAVED status shows on MENU at 0 px after leaving SETTINGS; the label and status fit check at build time | not recorded |
| Servo tester: the pure model (`src/servo_model.*`), the PWM driver (`src/servo_pwm.*`), the core1 job (`src/servo_job.*`), the slider and the screens (`src/ui_slider.*`, `src/ui_servo.cpp`); rcbench's sweep and PWM fold in `lib/rcbench/` | profiles, limits at their boundaries, steppers to their stops, command, angle and pulse mappings, SPEED, the dial's hit area; the PWM driver's bind, write, release and sharing rules against a register fake; SAFE at level 0, CENTRE at START, STOP on the next pass, the slew, sweeps, REVERSE B, the trip at 250 ms across a millis() wrap, kept across a rebind, the profile rebind; no pulse cut short at a release or a rebind, against a model of the counter and the latched compare; released pins pulled down; the screens through the UI: HOLD TO START at 1000 ms, the drag rules, the SET warning and HOLD TO APPLY, a TYPE, MIN or MAX change further past the travel and SERVOS ONE to TWO past it held for HOLD TO APPLY, a step back or toward the centre and TWO to ONE applied at once (`servoTravelNeedsHold()`), a pending TWO dropped to one servo when the claim takes SIG alone, a wait that held only for TWO ended and a heli, 60 Hz or travel wait beside it kept, the end-stop note and its place among SET's notes, STOP on SET, the AUX refusals through the claim, the unplug page; rcbench's own sweep and fold tests; the SDK calls are run against the fake only | not tested on hardware; open item 9 |
| KST programming: rcbench's session, line driver and file store (`lib/rcbench/`), the search (`src/kst_seek.*`), the core1 job (`src/kst_job.*`) and its line (`src/kst_line_port.*`), the files on the card (`src/kst_files.*`), the screen (`src/ui_kst.cpp`, `src/kst_view.*`) | rcbench's seven tests, 298 cases, the line's PIO program among them in a model (`test_kst_pio.c`); the search and the job against rcbench's simulated servo, the files against an in-memory FAT (File Allocation Table) volume, the backup in `BACKUP/` and SAVE AS in `KST/` up to 50 files among them, the card phases' block counts over FatFs R0.15 itself (`make -C test kstcount`), the screen through MENU against both (`runKstTests`); the line driver and `kst_line_port.cpp` are built by the firmware and typechecked only | not recorded: no frame of the line driver has been on a pin (`kst_line.h`) |
| SETTINGS layout (`src/ui_settings.cpp`): sections ESC TESTER, WIRING, SERVO SUPPLY, DISPLAY, BOARD and DIAGNOSTICS in one scrolling list | the scroll range, 0 px at opening and 620 px at most, 632 px while the ESC pin's warning shows; each row by id; no scroll from a press on a control; a cut control takes a press only while 40 px show; HOLD TO SAVE in place at every scroll position and saving at the end of the list; the pole count reaches the eRPM divisor, the ceiling steps by 5 %, and HOLD DEFAULTS restores both; HOLD DEFAULTS loads nothing on a tap, at 975 ms or after a slide off, loads at 1000 ms once per press, and draws its bar, its `C_RED` fill and its two-line label (`testResetHold()`); the footer at 6/150 and 162/72, y 268, 46 px, and HOLD TO SAVE's `C_RED` fill | not recorded |
| Restart reason and uptime (`src/diag.*`), SETTINGS → DIAGNOSTICS: LAST RESTART and UPTIME; [diagnostics.md](docs/design/diagnostics.md) | the decoding of every CHIP_RESET flag alone and in pairs, the flags of resets that leave the power manager alone before those that reset it (POR and GLITCH read GLITCH), the watchdog before them: enable caused, REASON with a flash update boot (UPDATE) and with any other boot type or an error (REBOOT); every text within 10 characters; the capture through fakes of `powman_hw->chip_reset`, `watchdog_hw->reason`, `watchdog_enable_caused_reboot()` and `rom_get_last_boot_type()`; the uptime's text up to 1193046:28:15; the rows' place, text and each reason's colour, no control on either, UPTIME repainted once a second while it shows, not within the second and not while scrolled out; the SDK bit values held by `static_assert` in the firmware build | not tested on hardware; open item 17. No watchdog is armed, so `WATCHDOG` cannot appear |
| Settings in flash (MENU → SETTINGS) | block validation, CRC, board id check, pin defaults on GP0 and GP1 and not GP28 or GP29, the version 5 block layout with CURRENT SENSE at byte 13, the servo supply clamp at 2, the current sensor clamp at 1; blocks of versions 2, 3 and 4 built byte by byte from the layouts in git and carried over (`settingsCarriedOver()`): each field, the newer ones at their defaults, version 3's GP0 supply as none, validation of out-of-range values, a damaged CRC, payload byte or magic and another magic with its CRC right discarded with no notice, a size field of another layout taking the reset path, flash untouched until a save, a save with nothing changed writing a version 5 block that the next boot loads with no notice; the splash's two lines, `UPDATED` on MENU and the note line for it; a reset by an update (`settingsResetByUpdate()`) for versions 1 and 6 and for a version 2, 3 or 4 block of another layout's size, not for blank flash, a bad CRC, a wrong size of this version or a payload over `SETTINGS_OTHER_MAX`, its CRC at the next 4-byte boundary, ended by a save that lands and not by one the flash refuses; the splash's two lines, `UPDATED` on MENU and the note line; the ESC pin's warning in SETTINGS against the boot pin, on its own line at y 230 beside UNSAVED, SAVED, SAVE FAILED and DEFAULTS LOADED, the list ending at y 227 and scrolling 632 px while it shows | not recorded; 46a64a7 states no hardware coverage |
| KST switched servo supply with its enable on GP28 or GP29 (`src/servo_supply.*`), the SERVO SUPPLY row, the switched search, REBOOT, the close rule, the lapse rule and the card phase (`src/kst_seek.*`, `src/kst_job.*`, `src/ui_kst.cpp`, `sdh_wait_hook` in `src/sdio/sd_host.h`) | the enable pin untouched with `NONE` from boot; with `GP28` or `GP29` an undriven input (SIO, output off, pulls off, input off) at boot and at every SETTINGS change, a change from GP28 to GP29 switching only GP29; low at bind, high and low between bind and release, an undriven input after release; the mode to pin map; the row stepped NONE, GP28, GP29, applied at once, saved and reset; against a model of the converter and a servo with a power-on window (`test/kst_supply_sim.h`): found on the first power-up, 14 power-ups and the timeout, off 2000 ms before every switch-on, the entry delay at -40 ms and +300 ms, a stop, a lapse and the timeout in a wait, REBOOT's 2000 ms off and 1000 ms on, refused while busy and without a switch, a lapse in the mode, during an operation and in REBOOT's off wait, the close 2000 ms after the switch-off, time off before a leave counted, the enable pin untouched with `NONE`; the screens: the switched search, the leave page and HOLD TO REBOOT, the unplug page's wait, the stalled screen and RETRY; the card phase: `CardBegin` and `CardEnd` in each job state, stamps holding the supply on for 900 ms and off 250 ms after the last, the 15,000 ms cap at 14,900 and 15,100 ms, closed by a command and by LEAVE, the first write's 300 ms card calls inside a phase with and without stamps, the hook null with `NONE`, the busy wait calling the hook | not tested on hardware |
| Hitec programming: rcbench's Hitec session, register model and line driver (`lib/rcbench/`), the core1 job (`src/hitec_job.*`) and its line (`src/hitec_line_port.*`), the files on the card (`src/hitec_files.*`), the HITEC SERVO screen (`src/ui_hitec.*`, `src/hitec_view.*`) | rcbench's 115 cases; the job against a simulated servo (`runHitecTests`); the deepest stack chain of the job on the Cortex-M33 checked in CI against 1,792 bytes; the files against an in-memory volume and the card phases' block counts over FatFs R0.15 (`make -C test kstcount`); the screen through MENU by control id, the claim on SIG from the screen's first frame with the job deferred to the family tap (`testDeferredJob()`, `testFamily()`) | not tested on hardware; CLASSIC needs SERVO SUPPLY at GP28 or GP29; the classic line voltage not measured |
| INA219 current sensor on J3, the shared I2C1 bus (`src/sense.*`, slice 1 of [current-sense.md](docs/design/current-sense.md)): the codec, the probe, the reading on core0 after the touch read, the read-back, the recovery, the `SenseSample` published for core1, which slices 3 and 4 read (nothing on core1 reads it yet); rcbench's `sense_bus.c` (5ef42efd) in `lib/rcbench/shared/sense/`; the touch driver's transfers noted; SETTINGS → CURRENT SENSE; the readout on SERVO TESTER, KST SERVO and HITEC SERVO | against an I2C fake with an INA219 and a CST328 model (`test/fake_i2c.*`, `runSenseTests`): 19EFh written at the probe, the shunt code to µA on 0.1 Ω at ±1, ±31999, ±32000 and beyond, the bus code to mV, the clip, a negative current; 399Fh and 19EFh accepted as an INA219, 4127h, 7127h and any other value as another part read every second, nothing but the register pointer written to it over 3000 ms, an INA219 put in its place found, FFFFh as none, a refused set-up; absent and probed again 1000 ms on, not 999 ms; three NACKs to offline, two timeouts to stuck, a held SDA, a recovery at once and every 100 ms, a part freed by its clocks, touch transfers noted, a touch abort with SDA held as a line low and the recovery run, a short count through the wire; the read-back once a second, a power-on reset found within two, the reset flag; 220 clocks for the probe, 96 for a reading, 84 for touch; the worst bus time of a frame in each failure mode and with one timeout after each count of transfers that answer, with timeouts moving the clock: the sensor at most 1480 µs, the probe's last data read timing out (1335 µs with the 19EFh write timing out, 1290 µs in a frame with the read-back, 1170 µs in a reading), touch at most 2073 µs, its data read timing out; a read's abort with SDA low for the block's own STOP a NACK, not a line held low; the recovery's START and STOP; the sample's 250 ms age, not at 249 ms, across a `millis()` wrap; the readout's texts and widths; touch read every frame with the sensor present, absent and failing; the SETTINGS row by id, saved, reset, nothing sent to 0x40 with NONE; the readout on the three screens | not tested on hardware: no INA219 has been on the board's bus; open item 16 |
| Touch handling, high-contrast theme, throttle gauge | tap on release, repeats, screen residue, gauge reach | hardware use recorded: the reports #22 to #24 named in the v2.3 commits; the fixes on hardware: not recorded |
| List and hold widgets (`src/ui_list.*`, `src/ui_hold.*`; slice A of [menu.md](docs/design/menu.md)) | the list: the scroll range and its clamp, the press rule at the viewport's edges, the row rectangle, the axis lock at 6 px, a tap on release and its slide-off, the scroll from the press point, 16 px per coarse step, the factor, the stop at a rail and the anchor after it, the clipped drawing; the hold: 1000 ms and not 999 ms, the reset on a lift and a slide off, a press that slid on, one firing per press, no timing for a press that landed while the control was disabled or whose first frame it missed, the progress bar's extent; through the screens: AM32's swipe on every field of the table, KV at 16 px a step and its rail, the AM32 and SERVO SET scroll ranges and edge rows (checks that 4c91db9 also passes), DWELL's rail on SET, HOLD TO WRITE and HOLD TO SAVE at 999 ms and 1000 ms, HOLD DEFAULTS at 975 ms and 1000 ms, each hold's colour and bar, a press on a disabled HOLD TO START, HOLD TO APPLY and HOLD TO WRITE never timed, HOLD TO SAVE's refusal while armed | not tested on hardware |
| Menu structure B, slice 1 (`src/ui_leave.*`; `UiBackGuard`, `uiStripTwoLines()`, `UI_COL_*` and the layout constants in `src/ui_widgets.*`; `uiListDrawField()` in `src/ui_list.*`; [menu-b.md](docs/design/menu-b.md)) | the leave page: its three phases, the current step, STAY, LEAVE, NEXT SERVO, RESTART DSHOT at 999 ms and 1000 ms into the released phase and across a `millis()` wrap, a press that landed during the wait or while releasing never restarting DShot, LEAVE drawn disabled and ignoring its tap while `leaveEnabled` is clear, LEAVE taking only a press that landed while it was enabled and is still enabled at the lift, the steps, bar, notes and buttons in each phase, the room above the notes; the guard: the first BACK arming, the second leaving, a press elsewhere and no change left disarming, its two lines for one and two changes; the two strip lines at y 42 and y 52 and their clear; the field row in each value state and edge, selected or not, scale 2 up to 104 px; the role names in both themes; the constants' values; SERVO TESTER's supply line drawn through `uiStripTwoLines()` | host only: no screen uses the leave page, the guard or the field row |
| Menu structure B, slice 2, colour roles (`UI_COL_CHANGED`, `uiChip()`, `uiChangeText()` in `src/ui_widgets.*`; the screens; [menu-b.md](docs/design/menu-b.md#slice-2-as-built)) | the role names, seven colours, in both themes; the chip square, 12 px tall, y 42 to 53, PROG MODE 65 px from x 167; the change text's dim old value and cyan new value; on AM32 CONFIG, KST SERVO, HITEC SERVO and SERVO SET a row's value by its state, as read, changed with a 3 px cyan bar, a soft rule's amber bar, read-only grey, selected or not, and the editor bar's value; SERVO SET's pending row cyan until HOLD TO APPLY and grey while RUNNING; Hitec's label without `*`; the KST and Hitec headlines amber, PROG MODE on blue, KST's IN DOUBT on red, REVERT amber, the write pages' old and new values; the tester's KISS and EDT tags, the DShot rate, SWIPE = THROTTLE and MAX; AM32's POWER-CYCLE THE ESC, HEX, WRITE VERIFIED green and a refused write's result not green; SETTINGS' captions, values, SAVED and the save hold's red; SD LOG's START, RETRY MOUNT and USB DRIVE labels and the read counter; servo B's horn, letter and selector, SPEED's fill, the command labels, the sweep count | host only: the colours have not been looked at on the panel |
| Menu structure B, slice 3 (`src/ui_settings.*`, `src/settings.cpp`, `src/ui_menu.*`, `src/kst_view.cpp`, the pin words of `src/ui_servo.cpp`, `src/ui_kst.cpp`, `src/ui_hitec.cpp`, `src/ui_am32.cpp`; [menu-b.md](docs/design/menu-b.md) decisions 9, 10 and 15 b, e, f, h, j, k) | SIG PIN and AUX PIN live whatever KISS is, every step a swap, with KISS on and off, the wiring following; their J5 line and SERVO SUPPLY's J4 line under a raised value, none under OWN; `settingsValidate()` moving AUX off SIG with KISS on and off, to the next free pin after SIG, KISS kept, and a pin the board does not free switching KISS off; SERVO SUPPLY a core section with and without KST SERVO on MENU; MENU's caption, `DSHOT RESTARTING ON SIG GP0` in amber over rows drawn disabled under their own status, and the caption back once Free; `WAIT: DSHOT RESTARTING` on SERVO TESTER, KST SERVO, HITEC SERVO and AM32 CONFIG; `DSHOT600` at each rate; `NOTHING SAVED: DEFAULTS IN USE`; the four KST labels; SERVO TESTER's caption with the role alone where it would reach RUNNING; the SIG words of each claim message; the connector lines against the board header's table; KISS TELEM moving neither pin; `SWAP THE ESC'S SIGNAL AND KISS WIRES` with KISS on; `IDLE 2s` and `DSHOT1200` sharing no pixel | host only |
| Menu structure B, slice 4: the layout table, decision 14 of [menu-b.md](docs/design/menu-b.md) (`src/ui_settings.cpp`, `src/ui_servo.cpp`, `src/ui_am32.cpp`, `src/ui_sdlog.cpp`, `src/ui_kst.cpp`, `src/ui_hitec.cpp`, `src/hitec_view.h`) | each moved rectangle by id: SETTINGS' HOLD TO SAVE 6/150 and HOLD DEFAULTS 162/72, SERVO SET's HOLD TO APPLY 6/150, DEFAULTS 162/72 and `-` and `+` 44 × 44 px, AM32's `-` and `+` 44 × 44 px and OK 6/228 at y 268, SD LOG's START 6/228, RETRY MOUNT 6/112 and USB DRIVE 122/112 (`testLayoutTable()`, `testAm32LayoutTable()`); no two controls overlapping; each label inside its button; HOLD DEFAULTS' two lines and HOLD TO SAVE's `C_RED` fill; AM32's OK keeping the result up when the hold's finger lifts on it and acting on a later tap; the hex dump from x 8; `1 CHANGE` and `5 CHANGES` on KST's write page, `1 CHANGE` and `n CHANGES` on Hitec's, Hitec's 4 rows (`hitecPageLayout()`), Hitec's strip in the order model, backup, pin, version with two spaces, a classic strip keeping `BACKUP IN RAM` and then `BACKUP HT001` beside the PROG MODE chip, the room 4 px before the chip (`uiChipX()`), y 56 to 61 blank on the main view and the list after the family view; AM32's OK drawn at rest under the hold's finger and pressed under a fresh press; 23 mutations, each failing the build or the suite | host only |
| Menu structure B, slice 8: SERVO TESTER's title, decision 13 of [menu-b.md](docs/design/menu-b.md#slice-8-as-built) (`SERVO_UI_TITLE` in `src/ui_servo.h`, `src/ui_servo.cpp`, `src/ui_registry.cpp`) | `SERVO TESTER` drawn in the header on the main view and the unplug page, plain panel from its end to BACK; the title MENU's row label and `UI_SERVO.title`; 13 characters or fewer, ending 4 px or more before BACK; 10 mutations, each failing the build or the suite | host only |
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
the tag. Menu structure B waits on being built, not on the bench (item 18).

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
   which DROPPED FRAMES counts. BUF PEAK and WORST FLUSH on the SD LOG screen
   are the numbers to size it from.
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
     SD LOG screen;
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
      through the unplug page, then DShot back on RESTART DSHOT;
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
    the host suite only: no INA219 has been on the board's bus, and the
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
    - B6 to B13, with slices 2 to 6: the converter, its disconnect and
      start-up time, its current limit, the owner's servos, the cuts, move
      timing, the heartbeat with the supply on, a 60 s log, the voltage at
      the servo at a stall.
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

18. **Menu structure B waits on building.** Slices 1 to 4 and 8 of
    [menu-b.md](docs/design/menu-b.md) are built: slice 2 is the colour
    roles, slice 3 the names and SETTINGS (decisions 9, 10 and 15 b, e, f, h,
    j, k), slice 4 the layout table, slice 8 SERVO TESTER's title; slices 2
    to 4 and 8 run in the host suite only, and slice 2's colours have not
    been looked at on the
    panel, in either theme. In the high-contrast palette several of slice
    2's pairs differ in brightness only, black against a dark grey: the KISS
    and EDT tags, the DShot rate with and without EDT requested,
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
    1000 ms. Slices 5 to 7, 9 and 10 wait on being built. Three decisions
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
    that never scroll of 2026-11-13 (the note's decision 33).
19. **The design notes' questions, answered on 2026-11-14.** The owner
    answered the questions of four design notes; each note records the
    answers as decisions:
    - [menu-b.md](docs/design/menu-b.md): all eight, and the seventh MENU
      row that followed from them (item 18). Waits on building.
    - [rx-signals.md](docs/design/rx-signals.md): build the S.BUS (Futaba's
      serial bus for receiver channels) monitor and an S.BUS output, no PPM
      (pulse-position modulation) output; the monitor reads UART0 on GP1
      with the pad's input inverted, shows µs by rcbench's `sbus_to_us()`
      with the bar in raw units; one MENU row, RX SIGNALS, opens a chooser
      of MONITOR and OUTPUT. The output's encoder is written and tested in
      rcbench first, then copied; its throttle channel obeys MAX THROTTLE.
      The monitor waits on building (slices 2 and 3); the output waits on
      its encoder in rcbench and on its design, of which the note lists
      the points to settle.
    - [current-sense.md](docs/design/current-sense.md): the converter takes
      one LiPo cell, 3.0 V to 4.2 V, into a synchronous boost with load
      disconnect, 5.0 V, at least 3 A out; the shunt stays 0.1 Ω; the servo
      tester always switches the supply with SERVO SUPPLY at a pin; the
      protection is rcbench's limit run, above 1.0 A for 400 ms and a hard
      cut at 3.0 A, the hard cut acting at once in the KST and Hitec claims,
      during an operation too. The note recommended the stall rule and
      warns that the limit run trips on many servos against a stop: bench
      test B8 checks it. Settings are carried over from versions 2, 3 and
      4 into version 5, new fields at their defaults; a block of a version
      the firmware does not know is discarded and announced as now. The
      migration, the first in `settingsLoad()`, is built and host-tested
      (`settingsCarriedOver()`, `runSettingsTests`); the first boot over an
      older block is not tested on hardware (items 15 and 16). Open to the
      owner: the converter module, which bench tests B6 and B7 check before
      it is relied on. Slices 2 to 6 wait on building.
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
    for minutes. If it does not, the change disarms a HOLD-latched motor
    after the finger lifts. The check, with that branch's firmware:
    - power up and leave the tester untouched for 5 minutes:
      `TOUCH NOT ANSWERING` does not appear;
    - propeller off, arm with HOLD at a low throttle and lift the finger:
      the motor runs until DISARM or the 30 s idle disarm.

    Until then a controller that stops answering leaves a HOLD-latched
    motor running until the idle disarm, 30 s after the last contact.
24. **Five owner decisions on existing screens, taken on 2026-11-14.**
    All five are built.
    - **AM32 3D MODE**, built. The tester shows `3D MODE ESC: THROTTLE
      SCALE DIFFERS` in amber after a list read with 3D MODE on, or a write
      of 3D MODE on that the ESC accepted, verified or not, in this
      power-up, and arms as before (`uiAm32Esc3d()`). Only a list read or a
      verified write with 3D MODE off clears it, by the owner's answer of
      2026-11-14 to the review of PR #55. Not verified on hardware; a
      swapped ESC is not detected.
    - **HITEC SERVO and DShot.** Built: HITEC SERVO takes SIG when it opens,
      before the family is tapped (`PinUserInfo::deferJob`,
      `pinClaimStartJob()` in `src/pin_claim.h`). In menu structure B this
      changes decision 19 of [menu-b.md](docs/design/menu-b.md): the
      chooser's Hitec entries take the pin on their tap.
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

## Known limitations

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
binary, `dshot_tests`. `test/test_main.cpp` runs twenty-four suites:

| Suite | File | Covers |
| --- | --- | --- |
| `runAm32Tests` | `test_am32.cpp` | AM32 init string, CRC-16/ARC, settings address per MCU (microcontroller) family, field codec, timing advance, the backup's text and its CRC-32, the result texts and the longest of them |
| `runUiTests` | `test_ui.cpp` (includes `ui_am32.cpp`) | the AM32 backup through the screen, screens, throttle modes, safety interlocks, core1 frame decision, EDT enable, EDT enable at pump start against a simulated ESC, touch, MENU's strip, its EDT chip in its three states and BEEP, SETTINGS' wiring, contrast and save and its VERSION row, SD LOG with its RETRY MOUNT label and its two mount hints, USB DRIVE, the text probes of `fakes.h` in high contrast, the splash's version and ESC pin line, the tester's `BOARD` caption, the board voltage's text rounded to 0.01 V and clamped to five characters, HOLD TO ARM drawn disabled while a USB host holds the card, the header's BACK drawn as the control table reports it on every screen the suite checks, the AM32 write result inside the panel with its power-cycle line, AM32 CONFIG's strip notes, next-step lines and connect lines and MENU's run cue, the tester's 3D note, AM32 CONFIG's read-only view of a layout outside 2 to 4 and its write refused while the BOARD voltage is red, screenshots |
| `runKissTests` | `test_kiss.cpp` | KISS CRC-8, decode, resynchronisation, request payload, KISS/EDT merge, expiry, EDT enable rule |
| `runGfxTests` | `test_gfx.cpp` | dirty bands, glyph rendering, clip rectangle, lines at any angle and thickness, arcs of dots |
| `runBlackboxTests` | `test_blackbox.cpp` | variable-byte and zig-zag encodings, header, frames, time prediction |
| `runLogRingTests` | `test_log_ring.cpp` | ring buffer, all-or-nothing writes, wraparound, high-water mark |
| `runSettingsTests` | `test_settings.cpp` | settings block, validation, board id check, pin defaults, the version 5 block layout, blocks of versions 2, 3 and 4 carried over, versions 1 and 6 reset, the servo supply and current sensor clamps |
| `runServoSupplyTests` | `test_servo_supply.cpp` | GP28 and GP29 with no switch and with one: untouched, undriven, bind, on, off, release, a change to none, a change from GP28 to GP29, the mode to pin map |
| `runSenseTests` | `test_sense.cpp` (includes `cst328.cpp`), `fake_i2c.cpp` | the INA219 driver on the I2C fake: codec, probe, readings at the range ends, NACK, timeout, stuck bus and recovery through rcbench's `sense_bus.c`, the read-back and a reset, clocks per frame, the sample's age across a `millis()` wrap, the readout's text, touch every frame beside the sensor |
| `runUsbMscTests` | `test_usb_msc.cpp` | card handover state machine, RPM smoothing |
| `runDshotFrameTests` | `test_dshot_frame.cpp` | DShot frame layout, checksum, inverted checksum, EDT enable word, throttle map (from rcbench) |
| `runDshotTelemTests` | `test_dshot_telem.cpp` | line and group code, eRPM and EDT decode, checksum against single-bit errors, sampler (from rcbench); three-state poll, counters, every normalised eRPM frame decoded with EDT on |
| `runDshotEdtTests` | `test_dshot_edt.cpp` | extended telemetry schedule against an AM32 model (from rcbench) |
| `runSdioTests` | `test_sdio.cpp` | SDIO driver: CRC7 and CRC16 vectors, framing, responses, CSD, four-bit packing, CRC status token; initialisation, reads and writes against a simulated card; one block written without waiting on busy (`sdh_write_start()`, `sdh_write_poll()`); the wait hook called from the busy wait |
| `runClaimTests` | `test_claim.cpp` (includes `core1_job.cpp`) | pin claim refusals at their 500 ms boundaries and their order, the handover decisions of `escTaskPoll()` one 1 ms slot at a time, the zero-throttle drain through the claim, Restoring, the order of the two cores' steps against fake jobs, BACK in every state, RETRY, the unplug-page rule, restore wiring, the job start guard, the heartbeat rule across a millis() rollover, no settings save during a claim, the AM32 screen on the claim |
| `runServoTests` | `test_servo.cpp` (includes `ui_servo.cpp`) | the servo model, the PWM driver against a register fake, the job core and the job through the claim, the slider, and the SERVO, SET and unplug screens through the UI, SET's hold for a profile further past its travel, BACK drawn disabled on the unplug page until the job lets go, the switched supply's strip line and the two-servo hint, with screenshots |
| `runKstTests` | `test_kst.cpp`, `test_kst_seek.cpp`, `test_kst_job.cpp` (includes `kst_job.cpp`), `test_kst_files.cpp`, `test_kst_view.cpp`, `test_kst_ui.cpp` | the search against rcbench's simulated servo: power-ups at 0 s, 3.0 s and 29.9 s found on the next attempt, attempts of 116.6 ms to 118.8 ms, no frame within 3 ms of a window, the 30 s timeout across a clock wrap, STOP SEARCH mid-frame, the heartbeat lapse, a reply at a bad rate kept and not found, the loss rule; the core1 job from the job table on the simulated line: the bind and its four refusals, search, RETRY, timeout, the heartbeat, read, verify, edit, restore, abort, a lost servo, leave and release with 100 ms of low line, the command block, each command in each state, and no call of `kst_session_write()`, `kst_plan_edit()` or `kst_plan_restore()`; the files over an in-memory FAT volume: the mount rule, no card and a USB-held card with no FatFs call, the first write's backup and record, an old record kept and never removed when it stays, a rename reported failed that took effect, a rename that left both names, a full card, a write-protected card as this board reports it and as FatFs can, a full directory, a file or directory in a file's way, a one-time write or sync error with no file or new directory left behind, a close error that leaves its file locked, the record found and loaded, the backup and an old record in `BACKUP/`, SAVE AS in `KST/` up to 50 files; what the screen shows: 27 rows in four groups, each value with its unit, the stepper within the hard limits, a text for every rule, result and file error; the screen through MENU: the claim, the search, STOP SEARCH and RETRY, the read, an edit and REVERT, HOLD TO WRITE not at 750 ms and not on a slide off, the backup and the record before the first write frame, the verify and the record removed, a hard limit written as its limit, the write waiting for the backup's flush and a failed flush, FILES with SAVE AS, LOAD, a backup row and its LOAD, SAVE AS refused at 50 files, a refused file, a write in doubt and HOLD TO RESTORE, BACK in every view and during a write, BACK drawn disabled on the unplug page through the 2 s off, LEAVE and RESTART DSHOT, a lost servo, no card, a USB-held card, a card that takes no file, an unknown layout read-only, a record found at entry; the card phase: `CardBegin` and `CardEnd` in each job state, the card stamp and its 15 s cap, the first write's card calls inside a phase |
| `runHitecTests` | `test_hitec.cpp`, `test_hitec_job.cpp`, `test_hitec_files.cpp`, `test_hitec_view.cpp`, `test_hitec_ui.cpp`, `fake_hitec_line.cpp` | the Hitec core1 job against a simulated HS-5645MG and D485HW: the bind and its refusals, the entries with and without the switched supply, the full read and its backup, writes of each family, a read-back that differs, a write cut by a lost servo, Abort, the heartbeat, LEAVE, the release's 2,000 ms off, the command block, the card phase; the files on the card against the in-memory volume: SAVE AS, LOAD, the first write with and without an old record, a later write's record, the removal; the screen's rows, values, stepper, soft rules and texts, the write page's layout (`hitecPageLayout()`) with the edges the screen cannot reach; HITEC SERVO through MENU: the family and its notes, the entry and read, an edit, the ID note in the editor bar and on the write page, the write page under a full set of notes: the horn line, the ID note, FILE OF ANOTHER MODEL and MODEL NOT MEASURED always, the ID row first, AND n MORE, the write and its result, FILES, REPAIR, a stall, LEAVE, BACK drawn disabled on the unplug page through the 2 s off |
| `runWidgetTests` | `test_widgets.cpp` | the list (`ui_list`) and the hold (`ui_hold`) driven with touch snapshots, each rule at its boundary; the header's BACK drawn disabled as `uiButton()` draws a disabled control; slice 1 of menu structure B: the leave page (`ui_leave`), the BACK guard, the two strip lines, the field row, the colour role names and the layout constants |
| `runIdTests` | `test_ids.cpp` | the control tables of `ui_screen.h` on the tester, MENU, SETTINGS, SD LOG in three card states, AM32 CONFIG in its handover and its list, and SERVO's three views: ids unique, rectangles inside the panel and apart, BACK at its rectangle and drawn live or disabled as the table says, the enabled flags against the state they follow; the id helpers of `ui_ids.h`, and that they report an absent or misgated control; the layout table of menu structure B on SETTINGS, SD LOG and SERVO SET: each footer, edge and editor rectangle by id and each label inside its button |
| `runBackupTests` | `test_backup_ring.cpp` | the ring of `BACKUP/`: its rules, saves of two devices and a rename under another device's number, a rename FatFs reports failed that took effect, two files under one number, the list of one device from the latest, a gap, no marker and an unreadable one, a directory under a ring name, names outside the ring, a marker or a read-only oldest file the card refuses, a change of directory that fails, load, no card and a USB-held card with no FatFs call |
| `runSdCacheTests` | `test_sd_cache.cpp` | the SD write cache over a fake card: the queue's order, a read of a queued sector, one swap check per batch, no command while busy, read cache hits and the oldest pushed out, a miss that waits for the block in flight, a full queue, retries, a failed block, a changed serial number, refusals with the cache off |
| `runDiagTests` | `test_diag.cpp` | the restart reason's decoding from CHIP_RESET, the watchdog's REASON, the enable query and the boot type, its order, its texts' widths, the capture through the fakes, the uptime's text |
| `runMenuTests` | `test_menu.cpp` | MENU, SETTINGS and the navigation through the real UI: SD LOG's BACK to MENU, every registry entry opened and left, MENU's disarm, BEEP's flash, AM32, SERVO and SETTINGS drawn disabled while the claim restores, MENU's scroll over registries of six and seven rows and its return at 0 px, SETTINGS whole at 0 px and SD LOG cut until dragged, every MENU status and SETTINGS value within its widest, the SETTINGS scroll range, each row by id, DIAGNOSTICS' LAST RESTART and UPTIME, a cut control under 40 px, HOLD TO SAVE and the UNSAVED note at every scroll position, HOLD DEFAULTS' 1 s hold, its `DEFAULTS LOADED - RESTART TO UNDO` note and what ends it, AM32's own wait beside another claim, SERVO SUPPLY as a core section, the strip following AUX and the claim |

`make run` also builds and runs `test/test_usb_msc_cb.cpp`, a program of
its own that links the real `src/usb_msc.cpp` against stubs of the logger,
the card and TinyUSB and drives its callbacks: the suite links a fake of that
file's runtime half for the screens.

`test/servo.mk`, `test/kst.mk`, `test/hitec.mk`, `test/rx.mk` and
`test/sense.mk` list each feature's sources and test files for
`test/Makefile`, as `src/servo.cmake`, `src/kst.cmake`, `src/hitec.cmake` and
`src/sense.cmake` do for `CMakeLists.txt`. A feature's C
sources and C test programs are built as C11:
`make run` also runs rcbench's `test_servo_sweep.c` (14 cases) and the
adapted `test_out_pwm_map.c` (5 cases) from `test/rcbench/`, and rcbench's KST
tests, copied unchanged: `test_kst_wire.c` (28 cases), `test_kst_reg.c` (51),
`test_kst_plan.c` (40), `test_kst_session.c` (77), `test_kst_pio.c` (17),
`test_kst_file.c` (57) and `test_kst_store.c` (28), and rcbench's Hitec
tests, copied unchanged: `test_hitec_wire.c` (11 cases), `test_hitec_pre.c`
(18), `test_hitec_pio.c` (23), `test_hitec_reg.c` (14),
`test_hitec_reg_plan.c` (9), `test_hitec_session.c` (15),
`test_hitec_session_write.c` (13) and `test_hitec_card.c` (12), and
`hitec_headers.cpp`, the core's headers compiled as C++11, and rcbench's
S.BUS decoder test, copied unchanged: `test_sbus.c` (12 cases). CI holds
the KST and Hitec line fixtures to pioasm's words.

Other targets in `test/Makefile`: `syntax` typechecks the device-only sources
with `-Werror`; `pioemu` runs `test/pio_sdio_emu.py`, the SDIO PIO programs
assembled from `src/sdio/sd_pio.pio` and run one system clock at a time
against a card model, 18 runs over three clock rates, three card output
delays and two card latencies, and `test/pio_backlight_emu.py`, the backlight
PIO program at every level from 0 to 255 and on 20 writes mid-period, and
`make` runs both; `bbcheck` encodes 2000 frames and decodes them with
Betaflight's `blackbox_decode`; `kstcount` runs `src/kst_files.cpp` and
`src/hitec_files.cpp` over FatFs R0.15 from the SD library's pinned commit,
fetched into `test/obj/`, on a RAM disk that counts blocks, and checks the
tables in [kst-programming.md](docs/design/kst-programming.md) and
[hitec-programming.md](docs/design/hitec-programming.md), which
`kstcount-write` rewrites; `lognum` runs `src/sd_log_next.cpp` over the
same FatFs on a RAM disk with two card images; `previews` and `wiki`
rebuild screenshots;
`coverage` builds an instrumented suite. Scripts: `test/check_docs.py`,
`test/check_uf2.sh`, `test/check_uf2_selftest.sh`, `test/check_blackbox.py`,
`test/pio_sdio_emu.py`, `test/pio_backlight_emu.py`,
`test/kst_card_count.cpp`, `tools/check_wiki.py`, `tools/coverage.py`,
`tools/make_previews.py`, `tools/make_wiring.py`, `tools/notices.py`,
`tools/stack_depth.py`.

**CI** (`.github/workflows/ci.yml`; on push to `main` or `master`, on pull
requests, by hand):

| Job | What it does |
| --- | --- |
| Firmware (`touch-lcd-2.8`) | builds with arm-none-eabi-gcc 13.3 and `STRICT=ON`; flash under 1 MiB and RAM (random-access memory) under 400,000 bytes; checks from the DWARF compile units that no file of the SD library's SDIO, ZuluSCSI-firmware or SdFat directories is in the image; writes `THIRD_PARTY_NOTICES.txt` with `tools/notices.py` and checks it against the image; compares the PIO model's assembler with the SDK's `pioasm`; holds the KST and Hitec line fixtures to `pioasm`'s words; checks the Hitec job's deepest stack chain against 1,792 bytes (`tools/stack_depth.py`); checks the two pin overrides reach CMake and that `-DDSHOT_PIN`, `-DKISS_PIN`, `-DDSHOT_PIN_LCD_2`, and `-DKISS_PIN_LCD_2` are refused; validates the UF2 |
| Host tests | `make`, which runs the SDIO and backlight PIO models too; the suite again with both pin macros overridden; `make kstcount` against the KST and Hitec design notes' card-phase tables; `make lognum`, the next log's file over FatFs; wiki screenshots and `check_wiki.py` |
| Blackbox round-trip | builds `blackbox_decode` from source, runs `make bbcheck` |
| Config permutations | typechecks all of `src/` with `-Werror` for 32 combinations, with the headers in `lib/rcbench/` on the include path: AM32 transmit mode × AM32 low-jump × rotation 0 or 2 × KISS on or off × SD logging on or off; checks the landscape guard fires |
| Workflow lint | actionlint 1.7.7 and a duplicate-key check on every workflow in `.github/workflows/` |
| Docs | `test/check_docs.py`; `tools/make_wiring.py --check` against `docs/wiring.svg`; then Doxygen with zero warnings allowed |
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
| `src/am32_backup.cpp` | 45 | 45 | 100.0% |
| `src/am32_bl.h` | 16 | 16 | 100.0% |
| `src/am32_eeprom.cpp` | 125 | 124 | 99.2% |
| `src/am32_eeprom.h` | 11 | 11 | 100.0% |
| `src/backup_ring.cpp` | 154 | 152 | 98.7% |
| `src/blackbox_encode.cpp` | 113 | 113 | 100.0% |
| `src/card_dir.cpp` | 96 | 96 | 100.0% |
| `src/core1_job.cpp` | 70 | 70 | 100.0% |
| `src/cst328.cpp` | 66 | 66 | 100.0% |
| `src/diag.cpp` | 31 | 31 | 100.0% |
| `src/dshot/dshot_edt.c` | 33 | 33 | 100.0% |
| `src/dshot/dshot_frame.c` | 25 | 25 | 100.0% |
| `src/dshot/dshot_telem.c` | 143 | 143 | 100.0% |
| `src/esc_merge.cpp` | 44 | 44 | 100.0% |
| `src/esc_task.h` | 157 | 157 | 100.0% |
| `src/gfx.cpp` | 213 | 211 | 99.1% |
| `src/hitec_files.cpp` | 248 | 237 | 95.6% |
| `src/hitec_job.cpp` | 382 | 372 | 97.4% |
| `src/hitec_view.cpp` | 265 | 261 | 98.5% |
| `src/kiss_telem.cpp` | 37 | 35 | 94.6% |
| `src/kst_files.cpp` | 163 | 158 | 96.9% |
| `src/kst_job.cpp` | 380 | 364 | 95.8% |
| `src/kst_seek.cpp` | 126 | 126 | 100.0% |
| `src/kst_view.cpp` | 160 | 154 | 96.3% |
| `src/log_ring.cpp` | 43 | 39 | 90.7% |
| `src/pin_claim.cpp` | 119 | 119 | 100.0% |
| `src/pin_claim.h` | 15 | 14 | 93.3% |
| `src/plat.h` | 8 | 8 | 100.0% |
| `src/rpm_filter.h` | 17 | 17 | 100.0% |
| `src/sd_log.h` | 34 | 34 | 100.0% |
| `src/sdio/sd_cache.c` | 132 | 129 | 97.7% |
| `src/sdio/sd_host.c` | 214 | 214 | 100.0% |
| `src/sdio/sd_proto.c` | 135 | 135 | 100.0% |
| `src/sense.cpp` | 215 | 215 | 100.0% |
| `src/sense.h` | 3 | 3 | 100.0% |
| `src/servo_job.cpp` | 199 | 199 | 100.0% |
| `src/servo_model.cpp` | 198 | 198 | 100.0% |
| `src/servo_pwm.cpp` | 55 | 55 | 100.0% |
| `src/servo_supply.cpp` | 40 | 40 | 100.0% |
| `src/settings.cpp` | 179 | 178 | 99.4% |
| `src/theme.cpp` | 11 | 11 | 100.0% |
| `src/touch.h` | 4 | 4 | 100.0% |
| `src/ui.cpp` | 474 | 473 | 99.8% |
| `src/ui.h` | 1 | 1 | 100.0% |
| `src/ui_am32.cpp` | 421 | 418 | 99.3% |
| `src/ui_hitec.cpp` | 1135 | 1079 | 95.1% |
| `src/ui_hold.cpp` | 36 | 36 | 100.0% |
| `src/ui_input.cpp` | 27 | 27 | 100.0% |
| `src/ui_kst.cpp` | 1153 | 1112 | 96.4% |
| `src/ui_leave.cpp` | 64 | 64 | 100.0% |
| `src/ui_list.cpp` | 141 | 141 | 100.0% |
| `src/ui_menu.cpp` | 134 | 133 | 99.3% |
| `src/ui_screen.h` | 5 | 5 | 100.0% |
| `src/ui_sdlog.cpp` | 220 | 216 | 98.2% |
| `src/ui_servo.cpp` | 590 | 590 | 100.0% |
| `src/ui_settings.cpp` | 423 | 418 | 98.8% |
| `src/ui_slider.cpp` | 109 | 109 | 100.0% |
| `src/ui_widgets.cpp` | 75 | 75 | 100.0% |
| `src/usb_msc.h` | 37 | 35 | 94.6% |
| **Total** | **9769** | **9588** | **98.1%** |

Generated by `tools/coverage.py`. CI runs it with `--check` and fails on drift.
<!-- coverage-table:end -->
