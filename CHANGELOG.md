# Changelog

Notable changes to MiniRcBench, a bench tester for one ESC (electronic speed
controller) driven by bidirectional DShot, a digital ESC protocol in which the
ESC answers each frame on the same wire. MiniRcBench continues the
DshotDisplay firmware, which remains a separate project at
[github.com/subtilitas/DshotDisplay](https://github.com/subtilitas/DshotDisplay).
The format follows
[Keep a Changelog 1.1.0](https://keepachangelog.com/en/1.1.0/). Commit-level
history is in git; [STATUS.md](STATUS.md) records what is verified.

0.1.0 is the first MiniRcBench version and covers the commits after
DshotDisplay 2.3. The sections below it are DshotDisplay releases; their tags
are in the DshotDisplay repository, not in this one. Each of those sections
covers `git log <previous tag>..<tag>`. A date is the tag's own:
the tagger date for an annotated tag, the commit date for a lightweight one.
The annotated tags v1.0.0 and v1.0.1 carry the tagger date 2026-08-01, five
days before the commits they point at (2026-08-06). The pre-release tags
pre-release-001 and pre-release-002 point at commits inside 1.0.0 and are
folded into it.

Issue and pull request numbers in the DshotDisplay sections are the ones the
commit messages give. DshotDisplay's current GitHub tracker starts again at
issue #1, so those numbers, #1 to #25, do not resolve there. Issue #1 under
0.1.0 is in DshotDisplay's current tracker.

Acronyms used throughout: EDT is Extended DShot Telemetry, eRPM is electrical
revolutions per minute, RPM is revolutions per minute, SD is Secure Digital,
USB is Universal Serial Bus, UI is user interface, CI is continuous
integration, UF2 is the USB Flashing Format, PIO is the RP2350's programmable
input/output, SPI is Serial Peripheral Interface, SDIO is Secure Digital
input/output, I2C is Inter-Integrated Circuit, GPIO is general-purpose
input/output, CRC is cyclic redundancy check, UART is universal asynchronous
receiver-transmitter, SDK is software development kit, PR is pull request,
DMA is direct memory access, CSD is the card-specific data register, SCSI is
Small Computer System Interface. SDSC,
SDHC and SDXC are the standard, high and extended capacity SD card types.
KISS telemetry is the serial telemetry protocol of the KISS ESC line, sent on
its own wire. AM32 is an open-source ESC firmware. PWM is pulse-width
modulation. KST and Chaservo are servo brands that share one programming
protocol.

## 0.1.0 - Unreleased

### Added

- SETTINGS → SERVO SUPPLY, `NONE` or `GP0`, in a section of its own
  between WIRING and DISPLAY: whether a DC-DC converter's enable for the KST
  servo's supply is wired to GP0 (`src/servo_supply.*`). With `GP0`, GP0 is
  an undriven input outside a KST claim.
- KST SERVO with the switched supply: the search switches the servo off for
  2 s and on before each attempt; HOLD TO REBOOT on the leave page; every
  claim end waits 2 s with the supply off; a screen stall switches the
  supply off and shows the stalled screen.
- The KST card phase: with the switched supply, the screen's card calls
  run in a phase in which the SDIO driver's waits stamp a heartbeat
  (`sdh_wait_hook`, `kstCardStamp()`), so a card that keeps the screen
  waiting keeps the servo powered for at most 15 s
  (`KST_CARD_PHASE_MAX_MS`).
- Device backups in one ring ([backups.md](docs/design/backups.md)). KST
  SERVO and AM32 CONFIG save the device's settings, as read at entry, before
  the first write of a session: `BACKUP/KSTnnn.TXT` and `BACKUP/AMnnn.TXT`,
  one sequence of 50 numbers for every device, `BACKUP/LAST` naming the
  latest; once all 50 are taken the next backup writes over the oldest in
  place. A mounted card that refuses the backup stops the write.
- The AM32 backup: the 48-byte settings block as text, one line per field,
  the bytes in hex and a CRC-32 checksum line (`src/am32_backup.*`).
- KST's SAVE AS writes to `KST/` under the next free KSTnnn and refuses once
  `KST/` holds 50 files. FILES lists the record, the KST backups of
  `BACKUP/` and the files of `KST/`; a backup loads as a file.
- The SD write cache for the claims of KST SERVO and AM32 CONFIG
  ([sd-write-cache.md](docs/design/sd-write-cache.md)): card writes queue in
  SRAM, 32 blocks, and go to the card in FatFs's order, one block per pass of
  the main loop, without waiting on the busy line (`sdh_write_start()`,
  `sdh_write_poll()`); a 16-block read cache; the volume's serial number
  checked before each batch, and read again before every write to the device
  after the backup, so a card swapped since gets the backup first. The KST write page reads
  `SAVING THE BACKUP TO THE CARD.` and `BLOCKS LEFT: n` and writes the servo
  only once the flush ends clean; AM32 and SAVE AS flush on the spot. A
  blackbox log that runs when either claim opens stops.
- `src/card_dir.*`: the mount rule, the scope that makes one directory
  FatFs's current one for a card call, and the file calls the KST files and
  the backups share.
- `make -C test kstcount` (`test/kst_card_count.cpp`): the KST card phases
  and the AM32 backup in block reads and writes, from FatFs R0.15 at the SD
  library's pinned commit over a RAM disk that counts them, on FAT32 and
  exFAT with no log, 1000 logs in `LOGS/` and 1000 in the root. It writes its table into
  [kst-programming.md](docs/design/kst-programming.md), and the Host tests
  job fails when the table drifts.
- The KST SERVO screen (work breakdown step 4), MENU's fourth row.
  `src/ui_kst.cpp` claims SIG, searches, reads the servo and shows its 27
  named fields in TUNE, PULSE, FAILSAFE and LOCKED; − and + step a field
  within the hard limits, a sideways swipe by an eleventh of its range.
  HOLD TO WRITE opens a write page with the changes, their soft rules and
  the backup's name; a second 1 s hold writes. FILES lists, saves, loads and
  restores KST files on the card. BACK leaves through `POWER THE SERVO OFF.`
  and the unplug page. `src/kst_view.h` holds the rows, the values with
  their units, the stepper and the texts. The host suite runs the screen
  through MENU against rcbench's simulated servo; `docs/kst-preview.png`.
- rcbench's KST programming core (`protocols/kst/`), its RP2350 line driver
  and the KST file store, copied unchanged from rcbench commit 453cf334 into
  `lib/rcbench/`, with rcbench's seven tests of them in `test/rcbench/`
  (298 cases). The firmware and the host suite build them
  ([kst-programming.md](docs/design/kst-programming.md), work breakdown
  step 1). README.md gains the section "Legal notice and disclaimer" the
  copied headers name.
- The KST files on the SD card (work breakdown step 3). `src/kst_files.h`
  gives rcbench's file store its 5 card functions over FatFs, in `KST/`. The
  card counts as mounted while the logger has it and the USB card reader is
  off. Before a session's first write the backup is saved as the next
  KSTnnn.TXT and the image before the write as `KST/KST.TXT`; an older record
  is renamed to a KSTnnn.TXT first, and a verified write removes only the
  record its session wrote. A card that takes neither file stops the write; no
  card lets it run. The host suite runs them against
  an in-memory FAT (File Allocation Table) volume, and the FatFs stub header
  gains the directory, read, rename and remove calls.
- The KST search and the KST core1 job (work breakdown step 2).
  `src/kst_seek.h` repeats rcbench's entry sequence, 116.6 ms to 118.8 ms an
  attempt, until a servo answers, for 30 s. STOP SEARCH lets the frame on the
  wire finish; a heartbeat lapse lets the running attempt finish and starts no
  other. A servo that stops answering is power cycled in the session and
  searched for again. `src/kst_job.h` runs the
  session on core1 through a command block and a status block: search, read,
  verify, edit and restore in slices, abort, and leave with 100 ms of low line
  before the line closes. `src/kst_line_port.h` puts rcbench's RP2350 line
  driver behind the job. The job is in core1's job table; nothing starts it
  yet. The host suite runs both against rcbench's simulated servo.
- `src/version.h` holds the firmware version, 0.1.0. CMake takes the project
  version from it, and `picotool info` shows it as the program version.
  `release.yml` refuses a tag that does not name it, with or without a
  pre-release suffix.
- [STATUS.md](STATUS.md): architecture, the board, verification state of each
  part, open items with issue #1, known limitations, tests and CI.
- This changelog.
- `make -C test coverage` and `tools/coverage.py`: host-suite line coverage
  in README.md and a per-file table in STATUS.md. The CI job Coverage fails
  on a stale figure, below 93 % in total or below 85 % in any file.
- `.github/workflows/docs.yml` publishes `wiki/` to the GitHub wiki after
  every successful Release run, from a tag push or started by hand, and on
  request for a named tag.
- `test/check_docs.py` checks that every relative link in STATUS.md,
  CHANGELOG.md and `docs/design/*.md` points at a file that exists, and
  that an anchor into a Markdown file points at a heading that exists.
- `DSERR` in amber in the status bar when the DShot driver cannot claim its
  two PIO state machines. Nothing is sent to the ESC then.
- Host tests for the DShot core: the 36 cases of rcbench for the frame, the
  reply and the EDT enable schedule, and new cases for the three-state poll
  and the counters it feeds. The suite compiles the core as C11.
- An SDIO card driver in `src/sdio/`, MIT, written from the SD Physical Layer
  Simplified Specification v9.10 and the RP2350 datasheet
  ([design note](docs/design/sdio-driver.md)). Two PIO programs fill pio1, 13
  and 19 instructions on 2 state machines, with 2 DMA channels and no
  interrupt. It initialises SDSC version 1, SDSC version 2
  and SDHC/SDXC cards, reads and writes single and multiple blocks, and ends
  every wait with a timeout and an error code. Not tested on hardware.
- Host tests for the SDIO driver (`runSdioTests`): CRC7 and CRC16 against the
  specification's examples, command framing, response parsing, CSD capacity,
  four-bit packing, the write CRC status token, and initialisation, reads and
  writes against a simulated card, with no card, voltage mismatch, timeouts
  and removal.
- CI checks that no compile unit of an image comes from the SD library's
  SDIO, ZuluSCSI-firmware or SdFat directories.
- Every release carries `THIRD_PARTY_NOTICES.txt`: each component whose code
  is in the image, with its pinned version, its files there and its licence
  text, copied from the licence files of the Pico SDK checkout, the fetched SD
  library and the Arm GNU Toolchain. `tools/notices.py` writes it from the
  image's debug information and linker map, and fails on code that no
  component covers, on a linker map entry with a size that names no input,
  and on a checkout that is not the pinned commit. If a build links
  `libstdc++.a(cp-demangle.o)`, libiberty code under GPL-2.0-or-later with a
  link exception, the GCC section names it and carries that exception.
  `SHA256SUMS.txt` covers the file. The Release workflow checks it before
  publishing and after, and the Firmware job of CI writes and checks it on
  every push.
- `test/pio_sdio_emu.py` and `make -C test pioemu`, which `make` runs too: the
  two SDIO PIO programs, assembled from `src/sdio/sd_pio.pio`, run one system
  clock at a time at 150 MHz against a model of an SD card, written from the
  RP2350 datasheet and the SD Physical Layer Simplified Specification v9.10.
  It covers CMD0, R1 and R2 responses, CMD17 with data over the response,
  CMD18 over 3 blocks, CMD12 and its busy, and CMD24 with its CRC status
  token, at 400 kHz, 10 MHz and 25 MHz with card output delays of 0 to 2
  system clocks. A run fails on bus conflicts and on a line that changes
  within 1 system clock of its sampling point. It is a model, not proof of
  hardware timing. The Host tests job runs it, and the Firmware job holds its
  assembler to the SDK's `pioasm`.
- The pin claim, slice 1 of
  [docs/design/pin-claim.md](docs/design/pin-claim.md): `src/pin_claim.h`
  hands the ESC and KISS pins to one screen at a time and refuses a claim
  while armed, while another claim holds, while an ESC answers on the signal
  pin within 500 ms (servo and KST screens; AM32 is exempt), and, for two
  servos, while KISS frames arrive within 500 ms or both roles name one GPIO.
  The servo and KST screens restart DShot only after their unplug page.
- `src/core1_job.h`: core1 binds, steps and releases a job on the claimed
  pins from `loop1()`, after the DShot pump. A job starts only while the pump
  is suspended. `Core1JobOps::init` initialises a job's shared state on core0
  before core1 starts.
- A stub screen `src/ui_kst.cpp`, and per-feature source lists
  `src/servo.cmake`, `src/kst.cmake`, `test/servo.mk` and `test/kst.mk`.
- The servo tester, slice 2 of
  [docs/design/pin-claim.md](docs/design/pin-claim.md)
  ([design note](docs/design/servo-tester.md)), on CFG → SERVO. It drives
  one servo on the ESC pin, or two on the ESC and KISS pins, with hardware
  PWM pulses of 400 µs to 2500 µs at 50 Hz to 560 Hz, in rcbench's five
  profiles. The screen starts SAFE, the pins at level 0; HOLD TO START,
  1000 ms, puts CENTRE on every bound pin, and STOP acts on press. The drawn
  horn is the control. MIN, CENTRE and MAX move the servo to an end or the
  centre, a square, sine or triangle sweep runs with rate, amplitude, dwell
  and count, and SPEED sets a slew from 10 % to 100 %. A screen stall of
  250 ms stops the pulses until the next HOLD TO START. The SET screen
  changes the profile only while SAFE and holds a heli profile or a rate
  above 60 Hz behind a warning and HOLD TO APPLY; while RUNNING its footer
  offers STOP in place of DEFAULTS. BACK and a profile change wait for the
  last pulse to complete before they let the pins go, and a released pin is
  an input with the pull-down on. BACK shows the unplug page before DShot
  restarts. Not tested on hardware.
- `src/servo_model.*`, the servo tester's rules as pure functions;
  `src/servo_pwm.*`, adapted from rcbench's PWM driver; `src/servo_job.*`,
  the core1 job; `src/ui_slider.*`, adapted from rcbench's slider; and
  `gfxLine()` and `gfxArcDots()` in `src/gfx.*`.
- `lib/rcbench/`: rcbench's sweep curve and GPIO-to-PWM fold at commit
  453cf334, copied unchanged, MIT, the same copyright holder. Their rcbench
  tests run in `test/rcbench/` as part of `make -C test`. `tools/notices.py`
  counts them as MiniRcBench's own code.
- `escTaskKissPin()`, and `escUiAlive()`, the one heartbeat rule for the pump
  and every core1 job.
- `pinClaimRetry()`: RETRY after a failed job bind keeps the claim.
- Host tests for the pin claim (`runClaimTests`), the servo tester
  (`runServoTests`: the model, the PWM driver against a register fake, the
  job through the claim, the slider and the screens through the UI), and a
  stub suite `runKstTests`.
- `docs/servo-preview.png`, four servo screens rendered by the host suite.
- Build-time checks that no free GPIO is on the backlight's PWM slice, that
  the free GPIOs share one slice, and that no two of them share a channel.
- `src/ui_list.h` and `src/ui_list.cpp`, one scrolling list: scroll in
  pixels, the axis lock after 6 px, a tap on release, and one sideways
  swipe of 16 px per coarse step with a factor per row.
- `src/ui_hold.h` and `src/ui_hold.cpp`, one hold-to-act control: 1000 ms,
  one firing per press, and its 3 px progress bar.
- Host tests for the list and the hold (`runWidgetTests` in
  `test/test_widgets.cpp`), each rule at its boundary.
- `src/ui_screen.h`: every screen reports the controls it draws on the
  current frame, each with an id, the rectangle it is drawn at, and whether
  it acts; a drag surface's grab margin is in its enum's doc. `uiControls()`
  returns the open screen's table and `uiCurrentScreen()` names the screen.
  The tables are built from the rectangles and the gating expressions the
  screens use. The firmware does not call either function.
- `test/ui_ids.h`: host-test helpers that tap, press and hold a control by
  id and fail the test, naming the id, when it is absent or gated the other
  way. `runIdTests` in `test/test_ids.cpp` walks every screen the host
  build opens and checks each table: ids unique, rectangles inside the
  panel and apart, BACK at its rectangle.
- MENU, a list of rows from the screen registry: BEEP THE ESC, AM32 CONFIG,
  SERVO TESTER, SD LOG and SETTINGS, with a strip for the ESC pin, the DShot
  rate and the EDT chip. BEEP's flash is on its row. AM32 CONFIG, SERVO
  TESTER and SETTINGS read `PINS BUSY`, are drawn disabled and open nothing
  while the pin claim is not Free. SD LOG reads its card state, and SETTINGS
  reads `UNSAVED` while a change is not saved. A tap on a list row fires only
  when released on that row. A seventh row makes MENU scroll
  ([menu.md](docs/design/menu.md), slice C).
- SETTINGS, one scrolling list in sections: ESC TESTER, WIRING, DISPLAY and
  BOARD. MOTOR POLES and MAX THROTTLE lead it. The list scrolls under a drag
  that starts off a control. LINK, the note line, HOLD TO SAVE and RESET stay
  fixed below it.
- The screen registry, `src/ui_registry.cpp`, with one line per MENU row and a
  build-time check that each label clears its status, and `UiScreen`,
  `SettingsRow`, `SettingsSection` and `UiMenuEntry` in `src/ui_screen.h`. A
  new tool is a `UiScreen` in its own file and one registry line. New files:
  `src/ui_menu.cpp`, `src/ui_settings.cpp`, `src/ui_registry.cpp` and
  `src/ui_sdlog.cpp`.
- `UI_LIST_TAP_MIN`, 40 px, in `src/ui_widgets.h`: the smallest row of a list
  and the smallest control in a SETTINGS row. A control cut by the edge of a
  list takes a press only while 40 px of it show.
- `PIN_USERS` in `src/pin_claim.h`, one row per pin user: its core1 job,
  whether it shows the unplug page and whether it is refused while an ESC
  answers. `pinUserInfo()` reads it.
- Host tests for MENU, SETTINGS and the navigation (`runMenuTests` in
  `test/test_menu.cpp`): SD LOG's BACK, every registry entry opened and left,
  MENU's disarm, BEEP's flash, `PINS BUSY` on AM32, SERVO and SETTINGS,
  MENU's scroll over registries of six and seven rows and its return at
  0 px, every MENU status and SETTINGS value within its widest, the SETTINGS
  scroll range, each row by id, the 40 px rule for a cut control, HOLD TO
  SAVE and the UNSAVED note at every scroll position, BACKLIGHT's floor of
  16, a step that changes nothing, and AM32's own wait beside another claim.
- `docs/menu-preview.png`, and `docs/settings-preview.png` with a scrolled
  frame of SETTINGS.

### Changed

- The Hitec design rewritten on rcbench 3e29dc1b with writes, and renamed
  from `hitec-reading.md` to
  [hitec-programming.md](docs/design/hitec-programming.md), "Hitec servo
  programming": rcbench's Hitec session, register model, wire, PIO and
  RP2350 line driver; the named parameters edited and written by plans with
  read-back, a backup into the ring of `BACKUP/` before a session's first
  write, a record of each write, 1 s holds. The owner's decision of
  2026-10-28 replaces the no-write decision of 2026-10-23. No write plan of
  rcbench's has run on a servo; bring-up runs one write with read-back per
  family first. Planned, not built.
- The settings block is version 3: it adds `servoSupply`, the SERVO SUPPLY
  row. The first boot after the update discards the stored version 2 block
  and starts from the compiled defaults, every setting and the ESC pin
  included. DshotDisplay, flashed back, discards a version 3 block the same
  way.
- Blackbox logs are written to `LOGS/LOGnnnnn.BFL` (`SD_LOG_DIR_PATH`), so the
  card's root holds a few directories. Logs an earlier firmware wrote to the
  root stay there and are not counted.

- The project is MiniRcBench, at
  [github.com/subtilitas/MiniRcBench](https://github.com/subtilitas/MiniRcBench).
  The CMake project and target are `MiniRcBench`. Builds write
  `MiniRcBench-touch-lcd-2.8.uf2`, `.elf` and `.map`, and a release carries
  `MiniRcBench-<tag>-touch-lcd-2.8.uf2`. Over USB the product is `MiniRcBench`, the
  serial port `MiniRcBench Serial`, the drive `MiniRcBench SD Card`, and the
  card reader answers a SCSI inquiry with the product `MiniRcBench SD`. The
  splash reads `MINIRC` over `BENCH`. A blackbox log names `MiniRcBench` as
  the craft and in the firmware revision, `Betaflight 4.5.0 (MiniRcBench)`.
- Version numbers start at 0.1.0. The USB device release is 0x0010 and the
  SCSI product revision `0.1`, from `src/version.h`; DshotDisplay reported
  1.00 and `1.0` in every release.
- Unchanged, so that a board moves over without being set up again: the USB
  vendor and product IDs (0x2E8A, 0x4006), and the settings block in flash
  (magic `DSHT`, layout version 2, the last 4 kB sector). Settings saved by
  DshotDisplay 2.x on an RP2350-Touch-LCD-2.8 load.
- MiniRcBench supports one board, the Waveshare RP2350-Touch-LCD-2.8, and
  builds one image for it. The splash and the SETUP BOARD row name the board
  the image is built for.
- The settings block's board id is a record checked against the 2.8"'s, 28;
  validation rewrites any other value and re-judges the stored pins against
  GP28 and GP29. The host suite pins the block layout DshotDisplay 2.x
  writes.
- The CMake pin options are `DSHOT_PIN_LCD_2_8` and `KISS_PIN_LCD_2_8`. A
  configure step with `-DDSHOT_PIN`, `-DKISS_PIN`, `-DDSHOT_PIN_LCD_2` or
  `-DKISS_PIN_LCD_2` fails with a message naming the options the build
  takes. A `-DBOARD` is ignored.
- `src/sd_hw_config.c` configures the one card slot for SDIO from the board
  header's pin macros at compile time, and asserts the SDIO pin rule there.
  `tools/sdtest` is SDIO-only and links no board descriptor.
- The config-permutation job builds 32 combinations, without a board
  dimension.
- Bidirectional DShot runs on the MIT driver from rcbench, `protocols/dshot`
  at commit 453cf334, kept in `src/dshot/`, instead of `pico-bidir-dshot`
  1.0.2, which is GPL-3.0. No GPL code is fetched or linked for DShot.
- The DShot poll tells a good reply, no reply and a damaged reply (line code,
  group code or checksum) apart. The good, bad and missing packet counts and
  the error percentage keep their meaning.
- The DShot transmitter and receiver take two adjacent state machines and 16
  instructions on pio2, beside the KISS receiver; DShot was on pio0. pio1
  stays the SDIO driver's.
- eRPM is 60,000,000 divided by the reported period in µs.
  `pico-bidir-dshot` added 50 eRPM to every reading, about 7 RPM on a
  14-pole motor.
- The DShot driver decodes EDT frames as EDT only when its caller says so,
  because an EDT frame cannot be told from an eRPM frame by its bits. The
  DShot frame pump says so for every reply; see the false RPM under Fixed.
- The design notes in `docs/design/` describe the code as it is: KISS
  reception through a PIO receiver on any free GPIO, KISS off by default
  rather than impossible, which log fields carry which source, and the USB
  card reader. `pin-claim.md`, `servo-tester.md` and `kst-programming.md`
  are for the 2.8" only: SIG on GP29, AUX on GP28, PWM slice 6 channels B
  and A. `kst-programming.md` records the owner's decisions on the search
  timeout, 30 s, the reply level, 3.3 V by the owner's statement, and the
  pull-down.
- README.md links STATUS.md and this changelog, and credits the
  collaboration.
- README.md and every wiki page, English and German, describe the firmware
  in the present tense with values from `src/`. Statements that disagreed
  with the code are corrected, among them the AM32 status strings, the
  backlight range of 16 to 255, the 34 px throttle gauge and the log fields.
- The wiki is published by `docs.yml` instead of a job inside `release.yml`.
- `actions/cache` v6 (Node.js 24) in place of v4 (Node.js 20).
- The workflow lint's duplicate-key check covers every workflow file.
- The SD card runs on the driver in `src/sdio/` instead of the
  SDIO driver of `no-OS-FatFS-SD-SDIO-SPI-RPi-Pico`, whose README derives it
  from ZuluSCSI-firmware (GPL-3.0). The build compiles FatFs, the glue and the SPI driver from a copy of the
  library's `src/` without its SDIO, ZuluSCSI-firmware and SdFat directories,
  not through the library's CMakeLists.txt. No GPL code is compiled or linked
  into the image. The library's SPI card driver stays in the build because
  its card layer calls into it.
- The SDIO pins need DAT0 to DAT3 consecutive and nothing more; CLK and CMD
  can be any other GPIO below 32.
- Before the DShot pump gives up its pin, for the AM32 editor or after a SETUP
  pin change, it sends zero-throttle frames for at least 250 ms
  (`ARM_ZERO_THROTTLE_MS`) after the last throttle frame, then stops. The
  release turns the pin's pulls off, and the pump puts the pull-up back on it
  so the line idles high. An AM32 session opened sooner after a throttle
  waits for the rest of that time. The decisions are pure functions in
  `src/esc_task.h` (`escHandoverStep()`, `escDrainSlot()`), and a bind
  disarms in `escPumpBind()`; the host suite runs them one 1 ms slot at a
  time.
- The AM32 editor takes the signal pin through the pin claim.
- The settings screen puts BEEP and SD LOG in one row, 104 px each, and AM32,
  SERVO, KST and SETUP in the bottom row, 50 px each. KST is drawn disabled
  and does not act.
- A settings save is refused while a pin claim holds.
- The AM32 list and the SERVO SET list run on one list. AM32's sideways
  swipe moves a field one coarse step per 16 px, ⌈range ÷ (11 × step)⌉ ×
  step per coarse step: 24 for a field of 0 to 255, 4 for motor poles, so
  176 px cover any field's range. A 180 px swipe covered the range in
  proportion to the finger's travel. A coarse step that would pass the end
  of the range stops there, and travel past the end is not stored. A coarse
  step is made of the steps of `-` and `+`, so an ADVANCE byte in the
  pre-1.90 encoding keeps its angle. SERVO SET's swipe does not change.
- HOLD TO WRITE on AM32, HOLD TO START on SERVO, HOLD TO APPLY on SET and
  HOLD TO SAVE on SETUP run on one hold: 1000 ms, one firing per press, and
  its 3 px progress bar. HOLD TO ARM keeps its own timer.
- `test/test_ui.cpp`, `test/test_servo.cpp` and `test/test_claim.cpp` tap,
  press and hold controls by id through `test/ui_ids.h`, and check the
  open screen with `uiCurrentScreen()`. Their coordinate macros are gone, so
  a layout change moves the control tables and not the tests
  ([menu.md](docs/design/menu.md), slice B).
- CFG on the tester reads MENU. Opening MENU disarms, sets the throttle to
  zero and turns HOLD off, as CFG did.
- SETUP is SETTINGS, reached from MENU. Motor poles and the throttle ceiling
  moved to it, at the top, and the ceiling reads MAX THROTTLE. The old
  SETTINGS screen is MENU. Pole count and ceiling are two taps away (MENU,
  then SETTINGS) instead of one.
- Every header is 36 px tall, up from 30 px, and BACK is 64 × 32 px, up from
  50 × 22 px. The strip starts at y 40 and the body at y 62.
- The AM32 list and the SERVO SET list have rows of 40 px at a 42 px pitch,
  up from 26 px, so three rows show. A press on a row cut by the edge of the
  list counts only while 40 px of it show.
- SD LOG rows are 20 px tall, down from 21 px, so they clear the note line
  under the taller header.
- The servo dial's hit area ends at 122 px from its centre, down from 124 px,
  and its two notes are at y 64 and 76, down from y 58 and 70. The dial
  band starts at y 62.
- Every tool's BACK returns to MENU. The unplug page of SERVO returns to MENU
  through RESTART DSHOT.
- SETTINGS' controls are larger: `-` and `+` 44 × 44 px, toggles 94 × 44 px,
  HOLD TO SAVE 150 × 48 px and RESET 70 × 48 px.
- The pin users are a table, `PIN_USERS`, in place of a switch and two
  comparisons in `src/pin_claim.cpp` and `src/pin_claim.h`.
- `src/ui.cpp` holds the tester and the navigation. SD LOG moved to
  `src/ui_sdlog.cpp`.
- README.md and the wiki describe MENU and SETTINGS in place of the settings
  screen and SETUP.

### Removed

- The Waveshare RP2350-Touch-LCD-2 (2.0") board: its pin map
  (`src/board_rp2350_touch_lcd_2.h`), its board descriptor
  (`src/board_desc_lcd2.cpp`), the CST816D touch driver (`src/cst816.cpp`),
  its SD card configuration over hardware SPI1 and `SD_LOG_SPI_MHZ`, its pin
  defaults and options `DSHOT_PIN_LCD_2`, `KISS_PIN_LCD_2` and
  `KISS_ENABLE_LCD_2`, its image `MiniRcBench-<tag>-touch-lcd-2.uf2`, its CI
  job and its rendered frames. DshotDisplay keeps support for that board.
- The unified image, `MiniRcBench-<tag>-unified.uf2`, with `BOARD_UNIFIED`,
  the `BOARD` CMake cache variable, `src/board.h` and the board-selection
  calls `boardSelect()`, `boardCount()`, `boardAt()` and `boardIdAt()`.
- The board probe, `src/board_probe.h` and `src/board_probe.cpp`, with its
  call at boot and the safe stop that held the power latch, the display off
  and DShot silent when no board or both answered. The image is for the
  RP2350-Touch-LCD-2.8 only.
- Code that existed only to tell two boards apart: the touch driver dispatch
  (`src/touch.cpp`, `TouchDriver`), the descriptor fields for a shared touch
  reset, the SD interface and the SD pins, `settingsPinFreeOn()` and
  `settingsNextPinOn()`, `sdHwConfigApply()`, `tools/sdtest/touch_stubs.cpp`,
  the test Makefile's `both` target and board list, the host tests for board
  selection and the 2.0" descriptor, and CI's DWARF touch-driver and
  board-label assertions.
- The SETUP screen (`src/ui_setup.cpp`, `src/ui_setup.h`) and the old
  SETTINGS screen, with its destination row and its disabled KST button. MENU
  and SETTINGS replace them.

### Fixed

- SD LOG's BACK returns to MENU. In the code it returned to the tester, while
  the wiki page The Screens said Settings. A test pins it.
- HOLD TO ARM times only a press that lands on it while arming is allowed.
  The press that disarms, kept on the button, armed again 1000 ms later, and
  a press made while a USB host held the card armed 1000 ms after the host
  let go. README.md and the First Run and SD Logging wiki pages, English
  and German, state the rule.
- HOLD TO START, HOLD TO APPLY, HOLD TO WRITE and HOLD TO SAVE time only a
  press that lands on the button while it is live. A press on HOLD TO START
  while it was drawn disabled, before the servo claim held the pins,
  started the servo 1000 ms after the claim went live.
- `inputPressing()` and `inputTapped()` return false for a null touch
  instead of dereferencing it, so `uiPressing()`, `uiTapped()`, `uiHeader()`
  and `uiBackTapped()` treat a null touch as no touch, as `ui_widgets.h`
  says.
- The DShot pump reads the heartbeat stamp before `millis()`, and
  `escUiAlive()` counts a stamp less than 250 ms newer than the time it is
  judged at as alive. Core0 could stamp a heartbeat between core1's read of
  `millis()` and its read of the stamp; the age then read as 2^32 - 1 ms, and
  the pump sent throttle zero for that frame although the UI ran.
- The DShot pump drops a beacon that is owed or not yet latched when the ESC
  arms. No command frame goes out armed, so an arm before the beacon's last
  frame could leave it owed until disarm: no KISS request went out while
  armed, and the beep sounded after the disarm. The touch UI cannot reach
  this path: BEEP is on the settings screen, which CFG opens, and an arm
  takes a 1000 ms hold on the main screen.
- The ESC STATUS tile on the tester follows the theme. Its colour table was a
  function-level static filled on the first draw, so after a switch to high
  contrast under SETUP the tile kept the dark theme's colours. Without the
  guarded static the image also leaves out libstdc++'s exception code and C++
  name demangler: 44,352 bytes less code with the Arm GNU Toolchain 13.3.Rel1.
- The CI firmware artifact carries the linker map. The upload named
  `<image>.map`; the build writes `<image>.elf.map`.
- The status bar's DShot label showed the compiled default, `DS600`, at any
  bitrate set under SETUP. It shows the bitrate the pump runs at.
- `test/check_docs.py` run from any directory other than the repository root
  checked no files and reported success. It now resolves its paths from its
  own location and fails when it finds no files.
- `test/check_docs.py` checks links with single-quoted titles, angle-bracket
  targets and parentheses in the path, skips footnotes, and builds heading
  anchors from the rendered heading text.
- `test/check_docs.py` strips `~~~` code fences as well as backtick fences,
  builds heading anchors without `_` and `*` emphasis markers, and caps its
  exit status at 255, so a multiple of 256 problems does not exit 0.
- The release notes give GP28 as the KISS pin, off by default.
- The manual and README.md list two USB DRIVE refusals. Without a card the
  button is drawn unavailable, so `NO CARD TO SHARE` cannot appear.
- The manual gives `vbatLatest` and `amperageLatest` as KISS only, 0 while
  KISS is not live, as `src/sd_log.cpp` writes them.
- Two comments in `src/config.h` disagreed with the code: the SD
  preallocation of 16 MiB lasts about 38 min, and SETUP shows one note line
  for saved settings or compiled defaults.
- A false RPM with the new DShot driver after a restart with EDT still on in
  the ESC. Replies were decoded as EDT only once this board had sent an
  enable, 300 ms or more after eRPM arrived. An ESC that kept EDT on across a
  board restart, a rebuild of the DShot frame pump after a SETUP change of
  the ESC pin, the bitrate, KISS or the KISS pin, or an AM32 settings session
  sent EDT frames that read as eRPM until then: a voltage frame of 66 counts
  read as 227,272 eRPM, 32,467 RPM at 14 poles. The first 10 frames after
  every pump start, those rebuilds included, are the EDT enable, 10 ms at
  1 kHz, and every reply is decoded with EDT on from the first one. eRPM from
  AM32 and Bluejay, which normalise its exponent with EDT on or off, decodes
  the same; for other ESC firmware without EDT that is unknown. The repeated
  enable keeps its rules: 300 ms after link-up, then every 1000 ms while no
  EDT frame arrives.
- `tools/sdtest` links: it carries the mass-storage callbacks that the
  firmware's TinyUSB configuration requires. Its stub comment says what they
  answer: test_unit_ready not ready, a capacity of 0 blocks, and an error for
  read10, write10 and every SCSI command outside TinyUSB's built-in set.

---

The sections below are DshotDisplay releases.

## [2.3] - 2026-08-26

Merges PR #25.

### Changed

- The throttle gauge is 34 px tall instead of 26 px. The grab margin under
  the bar is 1 px instead of 8 px, so the throttle's touch area stops short
  of the button row. The buttons stay at y = 283. Asked for in #24.

### Fixed

- Both ends of the throttle gauge are reachable on the 2.0" board. In spring
  (absolute) mode the outer 16 px at each end saturate and the 192 px between
  them span zero to the ceiling. Before, a 20 % ceiling stopped at 19 % and
  60 % at 58 %, because the end columns lie under the case rim (#22). HOLD
  mode is unchanged.
- A control drawn as unavailable no longer acts (#23): KISS PIN on SETUP with
  KISS off, START and USB DRIVE on SD LOG without a card, REVERT on the AM32
  screen with nothing changed. Unavailable controls are drawn as an outline
  without fill.

## [2.2] - 2026-08-17

Merges PR #20 and PR #21. The USB card reader comes from
`feat/usb-mass-storage`.

### Added

- USB DRIVE on CFG → SD LOG hands the card to the USB host as a read-only
  drive. The board is a composite device: serial port and card reader on one
  cable. The reader reports no medium until USB DRIVE is pressed. The logger
  unmounts first and touches nothing until the host ejects. Writes are
  refused with a write-protect error. The press is refused while armed or
  recording, with the reason shown for 4 s. Arming is refused while a host
  holds the card. The screen shows transfer progress.
- The RPM readout is smoothed by an integer moving average that settles in
  about 0.2 s (`RPM_FILTER_SHIFT` 3). It starts from the first reading, not
  from zero, and resets when the link goes stale. Display only: the log stays
  unfiltered. `RPM_FILTER_SHIFT` 0 turns it off.

### Fixed

- The AM32 editor reads timing advance in both encodings AM32 uses: 0 to 3 in
  7.5° steps (configurators before 1.90) and 10 to 42 in 0.9375° steps (1.90
  and later). AM32's default byte of 26 showed as 195°, and the first edit
  clamped it into the old range. Edits now write the 1.90 encoding.
- The manual's troubleshooting page no longer says KISS telemetry cannot be
  enabled on the 2.8" board. It is off there by default and one tap turns it
  on.

## [2.0] - 2026-08-15

Merges PR #3, #7, #9, #10, #11, #15, #16 and #17, and the branch
`test/multi-board-support`. The SD card writer comes from the native Pico SDK
port on `dev/pico-sdk-native`. The SD LOG screen, the KISS readout and the
2.8" SDIO path come from `dev/multiboard-sdio`, the PIO KISS receiver from
`feat/runtime-config`.

### Added

- Support for the Waveshare RP2350-Touch-LCD-2.8 beside the 2.0" board: CST328
  touch on I2C1, panel on SPI1, power latch on GP26 asserted at boot. ESC on
  GP29 and KISS on GP28 by default.
- A unified image that runs on either board. Each boot it identifies the
  board by I2C acknowledges from devices that are always powered. When
  neither or both buses answer, it holds a safe state: latch held, no
  display, no DShot. The board is neither stored nor chosen. Releases carry
  three images: `touch-lcd-2`, `touch-lcd-2.8` and `unified`.
- Blackbox logging to the microSD card in Betaflight's format, as
  `LOGnnnnn.BFL`. 500 Hz frames, an I frame every 32, 12 fields: both eRPM
  sources, KISS and EDT voltage and current in separate fields, temperature,
  consumption and stress. 8 kB ring buffer that drops and counts frames
  rather than blocking, 512-byte card writes, 16 MiB pre-allocated per file,
  directory entry synced every 2 s. Logging starts and stops with arming;
  START and STOP work by hand. The 2.0" board uses hardware SPI at 12 MHz,
  the 2.8" four-bit PIO SDIO at 10 MHz.
- CFG → SD LOG: card state, FatFs result and card type when a mount fails,
  RETRY MOUNT, file, frames, kB written, dropped frames, BUF PEAK against the
  buffer size (amber above half, red above three quarters) and WORST FLUSH.
  A REC indicator in the status bar.
- KISS values on the main screen. Voltage, current and temperature tiles show
  hundredths and a KISS or EDT tag. Consumption appears only with KISS.
- CFG → SETUP: ESC pin, DShot bitrate, KISS on or off and its pin, contrast
  and backlight, with the ESC link state shown live. Pins step only through
  the board's free GPIOs and skip the pin the other wire holds. Settings are
  stored in a flash sector, versioned and CRC-checked, saved with a 1 s hold,
  refused while armed. A block that fails a check is discarded whole.
- A high-contrast theme for outdoor use.
- Touch feedback: buttons fire on release and cancel when the finger slides
  off; DISARM fires on touch-down. Pressed state on every button. Steppers
  repeat and accelerate while held. The backlight dips for 120 ms on arm and
  disarm. The idle disarm counts down its last 5 s. The throttle pad shows
  SWIPE ACTIVE once a drag engages.
- An EDT ON / EDT OFF chip in the settings title bar, driven by arriving EDT
  frames.
- A user manual in `wiki/`, in English and German, published to the GitHub
  wiki on each release.
- `tools/sdtest`, a stand-alone SD bring-up firmware with a pin integrity
  check. Not released.
- CMake options for the per-board pin defaults (`DSHOT_PIN_LCD_2`,
  `KISS_PIN_LCD_2`, `DSHOT_PIN_LCD_2_8`, `KISS_PIN_LCD_2_8`). An override
  outside the board's free GPIOs, or both wires on one pin, fails the build.

### Changed

- The firmware builds with the native Pico SDK 2.1.1 and CMake instead of
  Arduino. Images are built for the ARM cores only, in Release mode, and are
  named `DshotDisplay-<tag>-<board>.uf2`.
- The KISS receiver is a UART built from one PIO state machine instead of
  UART1. Any free GPIO can receive. KISS is on by default on the 2.0" board
  (GP5) and off on the 2.8" (GP28).
- EDT is enabled automatically. The first enable goes out once the ESC has
  answered eRPM for 300 ms while disarmed and stopped, then every 1 s while
  eRPM arrives without EDT. The enable is 10 consecutive frames and a BEEP
  press no longer cuts it short.
- Telemetry expires instead of holding its last value. An EDT field shows
  `--` 1 s after its last frame. KISS gives way to EDT after 500 ms. A stale
  status block shows `--`, never OK. The log writes zero for stale EDT
  fields.
- BEEP flashes white when accepted and amber when refused, and takes the whole
  button row.
- One button, header and row style on every screen. BACK sits in the header
  at the same place on every screen. Settings rows grew from 38 px to 44 px,
  START and STOP from 34 px to 44 px.
- The AM32 list scrolls by the pixel and follows the finger. A row is
  selected only by a tap that did not move.

### Fixed

- EDT readings stayed on screen after the ESC was unplugged or swapped.
- EDT was enabled only once, 1.5 s after boot, so an ESC connected or
  power-cycled later reported RPM and nothing else.
- EDT and BEEP showed no reaction to a press.
- Text near the bottom edge at scale 2 could write past the framebuffer.
- Some glyph pixels, such as the top row of `0`, were drawn but not sent to
  the panel.
- Opening the AM32 screen before core1 had built the DShot driver waited
  forever.
- A single failed touch read counted as a lift-off and could complete a tap
  under a resting finger. Two consecutive failures are now needed.

### Removed

- The Arduino build (`sketch.yaml`, arduino-cli) and the RISC-V image.
- The ENABLE EDT button, made redundant by the automatic enable.
- The `-DDSHOT_PIN` CMake option, which configured without effect. CMake now
  refuses it and names the per-board options.

## [1.1.0] - 2026-08-11

Tagged on `dev/telemetry-logging`. Merges PR #1 and PR #2.

### Added

- KISS telemetry. The firmware sets the DShot telemetry bit on every 20th
  frame (50 Hz), receives the 10-byte reply on UART1 at 115200 baud, checks
  its CRC-8 (polynomial 0x07) and merges it per field with EDT. In this
  release the KISS values reach no screen and no log.
- A blackbox log encoder, host-tested and checked in CI against Betaflight's
  `blackbox_decode`. Nothing writes to the SD card yet.
- Design notes for KISS telemetry and blackbox logging in `docs/design/`.

### Fixed

- Text clipped at the left edge drew at its unclipped width.

## [1.0.1] - 2026-08-01

### Fixed

- The release workflow rejected every valid UF2 image: it checked for the
  magic bytes `55463200` instead of `5546320a`. The check now covers all
  three UF2 magic numbers and runs on every CI push.

## [1.0.0] - 2026-08-01

The first release, for the RP2350-Touch-LCD-2 only. pre-release-001 marks the
commit that added the host tests and CI; pre-release-002 is the same commit as
v1.0.0.

### Added

- A bench tester: drag the on-screen throttle and the board sends
  bidirectional DShot to one ESC on GP4, decoding eRPM and EDT from the same
  wire. Core0 draws the UI at about 40 Hz; core1 sends frames at 1 kHz and
  zeroes the throttle if core0 stops sending heartbeats.
- Safety interlocks: boots disarmed, 1 s hold to arm with the throttle at
  zero, spring return on lift-off, 20 % throttle ceiling by default, 30 s
  idle disarm, disarm on entering settings.
- An AM32 settings editor over the signal wire: 19200 baud one-wire
  bootloader, 39 fields, writes read back and compared byte by byte.
- A host test suite (60 checks) and CI: firmware images for the ARM and
  RISC-V cores, host tests, 8 build-option permutations with warnings fatal,
  Doxygen.
- A release workflow that refuses failing tests, implausible images, untagged
  builds and incomplete releases.
- Screenshots rendered by the host suite from the UI code; a landscape
  rotation is a compile error.

[2.3]: https://github.com/subtilitas/DshotDisplay/compare/v2.2...v2.3
[2.2]: https://github.com/subtilitas/DshotDisplay/compare/v2.0...v2.2
[2.0]: https://github.com/subtilitas/DshotDisplay/compare/v1.1.0...v2.0
[1.1.0]: https://github.com/subtilitas/DshotDisplay/compare/v1.0.1...v1.1.0
[1.0.1]: https://github.com/subtilitas/DshotDisplay/compare/v1.0.0...v1.0.1
[1.0.0]: https://github.com/subtilitas/DshotDisplay/tree/v1.0.0
