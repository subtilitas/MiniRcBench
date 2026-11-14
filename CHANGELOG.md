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

- A STEP model of the board for enclosure design,
  `hardware/mechanical/models/rp2350_touch_lcd_2_8.step`: 50.54 × 73.06 ×
  10.03 mm, 822 solids with flat and cylindrical faces. It is a facsimile
  drawn from measurements; its sources and limits are in
  [hardware/mechanical/README.md](hardware/mechanical/README.md).
  `gen_models.py` writes it from `touch_lcd_2_8.py` with rcbench's
  `step_gen.py`, copied to `lib/rcbench/hardware/kicad/tools/`; the Docs job
  runs `gen_models.py --check`.
- After v0.1.0-rc1 (a93e1c1): the tester shows
  `3D MODE ESC: THROTTLE SCALE DIFFERS` in amber on the RPM area's bottom row,
  in place of the pole count and the swipe hint, armed or not, while the pad is
  not engaged (`SWIPE ACTIVE` takes the row while a drag is engaged), after
  AM32 CONFIG read an ESC whose settings have 3D MODE on (a read that opens the
  list) or wrote 3D MODE on and the ESC accepted the write, whether its
  read-back verified, failed or differed, in this power-up. Only a list read or
  a verified write with 3D MODE off clears it, and so does a restart; a failed
  read or a refused write leaves it. Arming is unchanged. A swapped ESC is not
  detected. By the owner's decision of 2026-11-14. `uiAm32Esc3d()` in
  `src/ui_am32.*`; host test `testAm32TesterNote()`; the wiki shows
  `tester-3d.png`.
- The Release workflow publishes a tag with a suffix, such as `v0.1.0-rc1`,
  as a GitHub pre-release, and the Docs workflow publishes no manual for it.
- The splash shows `TOUCH CONTROLLER NOT FOUND` in amber while the touch
  controller has not answered since `touchInit()`: the driver records
  whether it has answered (`touchAnswered()`), and the splash reads the
  touch every 25 ms (`uiSplashTick()`) and clears the line at the first
  answer. That read goes into a state of its own, so a finger resting on
  the glass when the tester appears gives no press edge: it does not arm or
  move the throttle, and its lift still ends a tap on a button that acts on
  release.
  Host test `testSplashTouch()`, with a touch fake that stops answering
  (`fakeTouchAnswers()`); the wiki shows `splash-no-touch.png`.
- The tester's BOARD voltage is amber below 3.50 V and red below 3.30 V,
  with 50 mV of hysteresis (`uiBoardCell()`, `BOARD_CELL_*` in
  `src/config.h`), taken from the value as drawn, rounded to centivolts.
  Below 2.50 V, implausible for a working cell and possible with USB power
  alone, it stays in the text colour, and turns red again from 2.55 V. The
  thresholds are a lithium-polymer cell's usual low range, not measured on
  this board. Host test `testBoardCellColour()`.
- The manual says the driver reads the controller's first contact slot
  only and that what the controller puts there with two contacts is not
  measured, so the throttle finger lifts before DISARM; a wet screen or a
  lead on the glass can block every tap (README, wiki First-Run,
  The-Screens and their German pairs). Troubleshooting and Fehlersuche have
  a section for a screen that does not react to touch.
- The wiki pages SD-Logging and SD-Aufzeichnung say to stop a recording
  before cutting the board's power or pulling the card, and what a log cut
  by a power loss holds: 0 bytes within its first 2 s, with its reserved
  space left allocated until a file system check; later its reserved
  16 MiB, holding what reached the card, lagging by up to the 8192-byte
  buffer, then old card data; without the reservation, or past 16 MiB, the
  size at the last sync. `docs/design/blackbox-logging.md` answers its open
  question from the code; the hardware check stays open.
- SD LOG says how the last log ended when it ended on its own or a tool
  took the card, on amber lines above START: a card's fault until the next
  start or mount, a tool's stop until the next start
  (`sdLogMountForget()`). `RECORDING STOPPED: CARD
  FULL` / `FREE SPACE ON A COMPUTER, RETRY MOUNT` when FatFs wrote fewer
  bytes than asked, `RECORDING STOPPED: CARD WRITE FAILED` / `REMOVED?
  REINSERT, THEN RETRY MOUNT` when a write, sync or close failed, `NO LOG
  STARTED: THE FILE DID NOT OPEN` / `CHECK THE CARD, THEN RETRY MOUNT`, and
  `RECORDING STOPPED BY KST SERVO`, `HITEC SERVO` or `AM32 CONFIG` when the
  tool's claim stopped the log (`SdLogEnd`, `sdLogStopFor()`,
  `sdLogWriteEnd()`; `cardCacheBegin()` takes the tool).
- The splash reads `SETTINGS RESET BY UPDATE` and `CHECK MENU > SETTINGS` in
  amber, MENU's SETTINGS row `UPDATED`, and SETTINGS' note line `RESET BY
  UPDATE - CHECK AND SAVE`, after the load discarded a whole settings block
  of another version, until a save lands (`settingsResetByUpdate()`). Blank
  flash and a damaged block show none of them. After v0.1.0-rc1 only a
  block of a version with no layout takes this path; versions 2 to 4 carry
  over (see Changed).
- SETTINGS repeats the splash's warning, `UNPLUG ANY SERVO FROM GP1 NOW`
  for example, in amber on a line of its own at y 230 while the live ESC pin
  is not the one the splash named (`settingsBootDshotPin()`). The list then
  ends at y 227 and scrolls 12 px further; the note line keeps UNSAVED and
  SAVED.
- A USB host that unconfigures the device gives the card back as an eject
  does (`tud_umount_cb()`). A program of its own,
  `test/test_usb_msc_cb.cpp`, links the real `src/usb_msc.cpp` and drives
  its callbacks.

- AM32 CONFIG says what a value does and what to do next. Two amber lines
  replace the ESC name on the strip: while 3D MODE is on, `3D MODE: 50%
  THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.`; with TEMP
  LIMIT, CURRENT LIMIT or LOW VOLT CUTOFF selected and off, `PROTECTION OFF:
  THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.`; with PROTOCOL at any value
  but AUTO, DSHOT or 10 and above, `THE TESTER SENDS DSHOT ONLY:` / `THE ESC
  WILL NOT RUN HERE.` A write result that is not verified has `NOT VERIFIED.
  TAP OK, WRITE AGAIN.` under it, and `NO BACKUP ON THE CARD: NOT WRITTEN` has
  `CHECK THE CARD, THEN WRITE AGAIN.`. The connect page ends a reply it cannot
  parse with `IS THE ESC RUNNING AM32?` and adds `NOT AN AM32 ESC, OR BLANK
  SETTINGS.` / `NOTHING WILL BE WRITTEN.` under `NO VALID SETTINGS`. After a
  session whose handshake succeeded, MENU's strip shows `POWER-CYCLE THE ESC
  TO RUN IT` in amber under its caption until an eRPM frame stamped after the
  handshake arrives or the board restarts (`uiAm32RunCue()`). The AM32 facts
  are taken from the AM32 source, not measured. Host tests in
  `test/test_am32.cpp` and `testAm32Guidance()` in `test/test_ui.cpp`; the
  wiki's AM32 pages show `am32-connect.png`.
- Warnings that J5 pins 2, 3 and 4, SIG, 3V3 and GND, are in the order of a
  servo plug: a servo or ESC lead joined straight across puts the ESC's
  BEC output or the servo supply on the board's 3.3 V. In the README and
  the wiki pages Setup, Wiring and First-Run and their German pairs, with
  a pointer in the wiring sections of Servo-Tester, KST-Programming and
  Hitec-Programming and their German pairs;
  `docs/wiring.svg` labels J5 pin 3 never connect, in red, and J3 pin 3
  INA219 VCC only. The drawing, the README pin sketch and the wiki pages
  Wiring and Verdrahtung name the optional INA219, at 0x40, as the only part
  to connect to the I2C bus the touch controller, IMU and RTC share, in
  place of "do not use".
- The wiki pages AM32-Configuration and AM32-Konfiguration list every result
  of a write, what the code saw, that OK keeps the edits for another hold,
  and what to do when a result repeats.
- The wiki pages Telemetry, Troubleshooting and their German pairs give the
  ESC STATUS colours and the steps on WARN, ERROR or ALERT: disarm, let the
  ESC cool, read ESC TEMP and CURRENT. NO TELEMETRY starts with the
  power-cycle the ESC needs after AM32 CONFIG.

- The building blocks of menu structure B, slice 1 of
  [menu-b.md](docs/design/menu-b.md), with nothing drawn differently:
  the leave page widget `src/ui_leave.*`, with its three phases, numbered
  steps and RESTART DSHOT's wait as a parameter; the BACK guard
  (`UiBackGuard`, `uiBackGuard()`, `uiBackGuardDraw()`) and
  `uiStripTwoLines()` in `src/ui_widgets.*`; the field row
  `uiListDrawField()` in `src/ui_list.*`, with the "changed" colour as a
  parameter; the colour role names `UI_COL_*` and the layout constants
  `UI_EDGE`, `UI_FOOT_Y`, `UI_FOOT_H` and `UI_EDIT_BTN_W`. LEAVE takes only
  a press that lands while the screen holds it enabled (`leaveEnabled`) and
  is still enabled at the lift; a press that landed while LEAVE was disabled
  draws no pressed look and does nothing on its lift. No screen uses the
  leave page, the guard or the field row yet. SERVO TESTER draws its supply
  line through `uiStripTwoLines()`; BACK and the footers of AM32 CONFIG, KST
  SERVO and HITEC SERVO take the layout constants at their values. Host
  tests in `test/test_widgets.cpp`. The design note, with its eight open
  questions and the slices each one holds, is in `docs/design/`.
- SETTINGS → DIAGNOSTICS, after BOARD, with two read-only rows. LAST RESTART
  names why the chip last restarted: `POWER-ON`, `BROWN-OUT`, `RUN PIN`,
  `WATCHDOG`, `UPDATE`, `REBOOT`, `DEBUGGER`, `GLITCH`, `POWER-DOWN` or
  `UNKNOWN`, in amber for a brown-out, a glitch or a watchdog.
  `src/diag.cpp` reads the power manager's CHIP_RESET register, the
  watchdog's REASON register, `watchdog_enable_caused_reboot()` and
  `rom_get_last_boot_type()` once at boot and decodes them with a pure
  function: the watchdog first, `UPDATE` after a flash update boot, then the
  flags of resets that leave the power manager alone before those of resets
  that reset it. UPTIME shows the time since boot as `H:MM:SS`, redrawn once a
  second while it shows. The firmware arms no watchdog. SETTINGS scrolls to
  620 px instead of 500 px. Host tests: `runDiagTests` and
  `testSettingsDiagnostics`, against fakes of the two registers and the two
  SDK calls. Not tested on hardware
  ([diagnostics.md](docs/design/diagnostics.md), which also lists the open
  questions for a persistent fault log).
- The firmware version on the device: `VERSION 0.1.0` on the splash under
  the board's name, and a read-only VERSION row in SETTINGS' BOARD section.
- A splash line in amber that names the ESC pin, on which DShot starts when
  the splash ends: `UNPLUG ANY SERVO FROM GP0 NOW`.
- `BOARD` beside the tester's voltage at the top right, which is the board's
  own battery, not the ESC's pack.
- SERVO TESTER: with SERVO SUPPLY at `GP28` or `GP29`, a second strip line
  `GP28 SUPPLY STAYS OFF HERE.` in amber; while SAFE with one servo, the
  readout's hint `TWO SERVOS:` `SET > SERVOS`, unless CURRENT SENSE is
  `INA219`, whose readout then takes that place on the row.
- AM32 CONFIG: `POWER-CYCLE THE ESC TO APPLY.` under a verified write.
- Host tests for each of these texts, their pixel widths and their states,
  and `fakeTextIs()`, `fakeTextCentredIs()` and `fakeInkExtent()` in
  `test/fakes.h`, which check drawn text against a redraw and the extent of
  drawn pixels.
- rcbench's S.BUS decoder (`protocols/sbus/sbus.c`, `include/sbus.h`), copied
  unchanged from rcbench commit 5ef42efd into `lib/rcbench/`, with rcbench's
  test of it, `test_sbus.c` (12 cases), in `test/rcbench/`, listed in
  `test/rx.mk` ([rx-signals.md](docs/design/rx-signals.md), work breakdown
  slice 1, the S.BUS part). S.BUS is Futaba's serial bus for receiver
  channels. Nothing in `src/` calls it yet.
- The INA219 current sensor on J3, the board's shared I2C1 bus (slice 1 of
  [current-sense.md](docs/design/current-sense.md)): `src/sense.*` reads the
  shunt and bus registers on core0 once per UI frame after the touch read,
  identifies the part by its configuration, 399Fh or 19EFh, before it writes
  a register, writes the configuration 19EFh at each probe, reads it back once
  a second, recovers a stuck bus with 9 clocks on SCL, and publishes each
  reading for core1 as a `SenseSample` that counts as none at 250 ms; slices
  3 and 4 read it. Nothing is written to another chip at 0x40 but the
  register pointer the read needs. rcbench's `sense_bus.c`
  and `sense_bus.h`, copied unchanged from rcbench commit 5ef42efd into
  `lib/rcbench/shared/sense/`, decide on faults; the touch driver reports its
  transfers to them. SETTINGS → CURRENT SENSE, `NONE` or `INA219`, below
  SERVO SUPPLY, with the caption `FOUND`, `NOT FOUND` or `OTHER PART`. The
  readout: the current and the bus voltage on the servo tester's readout
  row, `A+B` with two servos, `>3.2 A` in amber when clipped; the current
  after `SUPPLY ON` on the KST SERVO and HITEC SERVO strips; `-- A` for no
  reading. The host suite runs on an I2C fake with an INA219 and a CST328
  model (`test/fake_i2c.*`, `test/test_sense.cpp`). Not tested on hardware.
- The wiki pages Setup and Einrichtung: the parts for each tool, the
  adapter cable from J5 to a servo plug, and installing the `.uf2` image.
- A wiring diagram, `docs/wiring.svg`, in the README and on the wiki pages
  Wiring and Verdrahtung: the board's connectors pin by pin, an ESC with KISS
  telemetry, a servo on a hand-switched supply, and a servo on the switched
  supply with its 10 kΩ pull-down. `tools/make_wiring.py` draws it from the
  firmware's pin sources; the Docs CI job runs its `--check`.

- rcbench's Hitec programming core (`protocols/hitec/`), its RP2350 line
  driver, the Hitec servo file text (`shared/bench/hitec_card.*`) and the
  CRC-16 it uses (`shared/link/link_crc.*`), copied unchanged from rcbench
  commit 3e29dc1b into `lib/rcbench/`, with rcbench's eight tests of them in
  `test/rcbench/` (115 cases) and `hitec_headers.cpp` built as C++11
  ([hitec-programming.md](docs/design/hitec-programming.md), work breakdown
  step 1). A CI step holds the KST and Hitec line fixtures to pioasm's
  words.
- The Hitec core1 job (`src/hitec_job.*`, work breakdown step 2): rcbench's
  Hitec session on SIG through `src/hitec_line_port.*`, the entry with the
  switched supply, the full read and its backup, writes with
  read-back, the heartbeat and lapse rules, the close rule and the card
  phase, as `PinUser::Hitec` and `Core1Job::Hitec`; HITEC SERVO starts
  it. `tools/stack_depth.py` and a CI step find the job's deepest stack
  chain on the Cortex-M33 from GCC's call graph and fail above 1,792 bytes.
- The Hitec files on the card (`src/hitec_files.*`, work breakdown step 3):
  SAVE AS files `HITEC/HTnnn.TXT` and the backup `BACKUP/HTnnn.TXT` in
  rcbench's servo file text, the record of each write as `HITEC/HITEC.TXT`,
  an old record moved into the ring, LOAD of a file, a backup or a record.
  `make -C test kstcount` counts the Hitec card phases into
  [hitec-programming.md](docs/design/hitec-programming.md).
- The HITEC SERVO screen (`src/ui_hitec.*`, `src/hitec_view.*`, work
  breakdown step 4), the fifth MENU row: the family choice, the entry and
  the read, the field list with rcbench's rows, units and soft rules, the
  write page with the backup and the record before the first write frame,
  FILES with SAVE AS, LOAD, FACTORY VALUES and HOLD TO RESTORE, REPAIR of a
  classic checksum, the leave and the unplug page. CLASSIC opens with
  SETTINGS → SERVO SUPPLY at `GP28` or `GP29`, and is drawn disabled with
  `NEEDS SERVO SUPPLY GP28 OR GP29` under `NONE`. MENU holds seven rows and
  scrolls 36 px to reach its last row. `docs/hitec-preview.png` shows the
  screen.
- The Hitec user documentation (work breakdown step 5): the README section
  "Hitec servo programming" with its wiring, results, files, classic status
  and current limitations, the MENU row, the Hitec trademark notice and
  credit, and the wiki pages Hitec-Programming and Hitec-Programmierung with
  their entries in the sidebar, the home page and the screens pages.
- SETTINGS → SERVO SUPPLY, `NONE`, `GP28` or `GP29`, in a section of its
  own between WIRING and DISPLAY: whether a DC-DC converter's enable for the
  KST and Hitec servo's supply is wired, and to which pin
  (`src/servo_supply.*`). The chosen pin is an undriven input outside a
  claim.
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
- SERVO TESTER → SET warns of a pulse past a servo's travel: with TYPE WIDE,
  MIN below 900 µs or MAX above 2100 µs (HELI CYCLIC on its own 820 µs to
  2220 µs; NARROW 760 and HELI TAIL 760 with MIN or MAX more than 100 µs
  outside the type's own range) the note band says
  `PAST ITS TRAVEL A SERVO STALLS AND` `HEATS. TRY MIN AND MAX HORN-FREE.`
  in amber, ahead of the dim notes while TYPE, MIN or MAX is selected and
  after them on the other rows. The change still applies at once. After
  v0.1.0-rc1 a change that takes the profile further past its travel waits
  for HOLD TO APPLY, with this note as its warning (see Changed).
  `servoPastTravel()` in `src/servo_model.*`; host tests in
  `test/test_servo.cpp`.

### Changed

- After v0.1.0-rc1 (a93e1c1): the uses that decision 11's tables name take
  their role's colour, slice 2 of menu structure B (`docs/design/menu-b.md`,
  decisions 8, 11 and 26). A value edited and not yet written or applied is cyan
  with a 3 px cyan bar at its row's left edge on AM32 CONFIG (amber before), KST
  SERVO and HITEC SERVO (lime before, Hitec's `*` gone) and on SERVO SET's
  row that waits for HOLD TO APPLY; a soft rule's bar stays amber; the
  editor bars follow, and selection no longer turns a value lime. The KST and
  Hitec write pages show the old value dim and the new one cyan. Chips are
  square and 12 px tall; KST and Hitec read `PROG MODE` on blue (`MODE` on
  green before) and KST's `IN DOUBT` is red. Headlines such as `POWER UP` and
  `POWER-CYCLE THE ESC` are amber. In the text colour now: the KISS source
  tag, the DShot rate with EDT requested, `SWIPE = THROTTLE` while armed, the
  tester's `MAX 20%`, AM32's HEX label, SD LOG's START, RETRY MOUNT and USB
  DRIVE labels, SETTINGS' captions and the MAX THROTTLE, SIG PIN, AUX PIN and
  SERVO SUPPLY values (slice 3's names for ESC PIN and KISS PIN, lime and
  cyan before), SERVO TESTER's MIN, CENTRE, MAX, SWEEP and SET labels. Servo B's
  horn, letter and selector are blue; SPEED's fill is grey; the sweep count
  and SD LOG's read counter are lime; SETTINGS' `SAVED` and AM32's
  `WRITE VERIFIED` are green; HOLD TO SAVE fills red while held, as every
  hold. KST's and Hitec's REVERT labels are amber. KST SERVO and HITEC SERVO
  name a backup not yet on the card `BACKUP IN RAM`, in place of
  `BACKUP IN MEMORY`, on the strip, in `NO CARD: THE BACKUP IN RAM IS LOST.`
  and in KST's `NO CARD: REBOOT LOSES THE BACKUP` / `IN RAM.`, so
  `D485HW  BACKUP IN RAM  GP0` fits beside the wider chip. `UI_COL_CHANGED`,
  `uiChip()` and `uiChangeText()` in `src/ui_widgets.*`; the rows through
  `uiListDrawField()`. Host tests in `test/test_widgets.cpp`,
  `test/test_ui.cpp`, `test/test_menu.cpp`, `test/test_servo.cpp`,
  `test/test_kst_ui.cpp` and `test/test_hitec_ui.cpp`; the wiki images are
  regenerated.
- After v0.1.0-rc1 (a93e1c1): one layout table, decision 14 of
  [menu-b.md](docs/design/menu-b.md#decisions), slice 4 of menu structure B.
  SETTINGS' footer is HOLD TO SAVE 150 × 46 px from x 6 and HOLD DEFAULTS,
  formerly HOLD RESET, 72 × 46 px from x 162, its label `HOLD` over
  `DEFAULTS` in amber; HOLD TO SAVE fills `C_RED` while held, as every hold
  (`SettingsCtl::Reset` is `SettingsCtl::Defaults`). SERVO TESTER → SET's
  footer is HOLD TO APPLY 6/150 and DEFAULTS 162/72, and its `-` and `+`
  are 44 × 44 px, as on AM32 CONFIG, whose result OK is the wide footer
  button, 228 × 46 px at y 268, `OK` at scale 2; it takes, and draws
  pressed, only a press that lands after the write ended, since the hold's
  finger lifts on it. SD LOG's
  START and its row of RETRY MOUNT and USB DRIVE (112 px each) span x 6 to
  233. AM32's hex dump starts at x 8. The KST and Hitec write pages count
  `1 CHANGE`, `2 CHANGES`; Hitec lists 4 changes at most before
  `AND n MORE`, as KST does; HITEC SERVO's list strip reads model, backup,
  pin and, on a classic servo, version, two spaces apart, and drops items
  from the right, so a classic strip keeps the backup's state; the room ends
  4 px before the chip (`uiChipX()` in `src/ui_widgets.*`). Every Hitec view
  clears the strip band, y 40 to 61, so the family view's second strip line
  leaves no rows under the strip. Host tests
  `testLayoutTable()` in `test/test_ids.cpp`, `testAm32LayoutTable()` and
  checks in `test_menu.cpp`, `test_kst_ui.cpp`, `test_hitec_ui.cpp` and
  `test_hitec_view.cpp`.
- After v0.1.0-rc1 (a93e1c1): names and SETTINGS of menu structure B, slice 3
  of [menu-b.md](docs/design/menu-b.md) (decisions 9, 10 and 15 b, e, f, h,
  j, k):
  - SETTINGS' WIRING rows read SIG PIN, DSHOT, KISS TELEM and AUX PIN. SIG
    PIN and AUX PIN show their J5 pin under the value (`J5 PIN 2` for GP0,
    `J5 PIN 1` for GP1), both act whatever KISS is, and a step of either onto
    the other's GPIO swaps SIG and AUX, in place of skipping it.
    `settingsValidate()` moves AUX off SIG in a stored block, whatever KISS
    is, and KISS keeps its switch; an AUX pin the board does not free still
    switches KISS off and takes the default. The stored block is unchanged,
    version 5.
  - SERVO SUPPLY is a core section of SETTINGS after WIRING, no longer KST
    SERVO's (`UI_KST.settings` is null). Its first value reads `OWN`, the
    stored 0, and `GP28` and `GP29` show `J4 PIN 11` and `J4 PIN 12` under
    the value. KST's switched search that finds no servo says
    `SETTINGS > SERVO SUPPLY AT OWN`.
  - MENU's strip reads `SIG GP0  AUX GP1  DSHOT600` in place of
    `GP0  600 KBIT/S`. While the pin claim is not free it reads
    `DSHOT RESTARTING ON SIG GP0` in amber, and the rows that claim the pins
    and SETTINGS are drawn disabled under their own status in place of
    `PINS BUSY`. A tool refused for another claim, which no path through the
    screens reaches, reads `WAIT: DSHOT RESTARTING`.
  - The tester's top row reads `DSHOT600` in place of `DS600`, and its
    `IDLE ns` countdown starts at y 11 in place of y 7, under the rate:
    `DSHOT1200` ends at x 156, right of where `IDLE 5s` starts.
  - With KISS on, SETTINGS' line over LINK reads
    `SWAP THE ESC'S SIGNAL AND KISS WIRES` in place of the servo warning
    while SIG is off the boot pin: a step of SIG PIN or AUX PIN swaps the
    two, so DShot drives the wire the ESC's KISS pad was on.
  - SETTINGS' note line reads `NOTHING SAVED: DEFAULTS IN USE` in place of
    `COMPILED DEFAULTS IN USE`.
  - KST's field labels REVERSION, ALLOW UNCONT, UNCONTROLLED POS and
    UNCONTROLLED TIME read REVERSED, FAILSAFE MOVE, FAILSAFE POSITION and
    FAILSAFE TIME, the words of their warnings. The card files are
    unchanged.
  - The screens name SIG by role and number: SERVO TESTER's caption
    `SIG GP0  50 HZ  1000-2000 US` and `SIG+AUX  50 HZ  1000-2000 US`, with
    the role alone, `SIG  333 HZ  1000-2000 US`, or `SIG+AUX  333 HZ`, where
    the line would reach the widest chip; `TAKING SIG GP0` and
    `SIG GP0 COULD NOT BE TAKEN (PWM)` on SERVO TESTER, KST SERVO and HITEC
    SERVO; `AN ESC ANSWERS ON SIG GP0.` and `RELEASING SIG GP0` on SERVO
    TESTER and KST SERVO; HITEC SERVO's family view
    `AN ESC ANSWERS ON SIG GP0: UNPLUG IT.` and
    `RELEASING SIG GP0 FROM DSHOT`; the KST and Hitec strips,
    `SIG GP0  TRY 12  1.4 S`,
    `SIG GP0  SUPPLY ON`; AM32 CONFIG's `RELEASING SIG GP0`; SET's
    `KISS WIRE ON AUX GP1 IS LIVE.` and
    `SIG AND AUX ARE THE SAME GPIO.` / `SET SIG PIN OR AUX PIN IN SETTINGS.`
  Host tests `testSettingsKiss()`, `testSettingsServoSupply()` and
  `testAm32ClaimWords()` in test_ui.cpp, `testSupplySection()` in
  test_menu.cpp, `testKissNeedsItsOwnPin()` in test_settings.cpp,
  `testPinNames()` in test_servo.cpp, `testSettingsConnectorLines()` in
  test_ui.cpp against the board header's connector table,
  `testSwitchedTimeout()` in
  test_kst_ui.cpp, `testBusyRefusal()` in test_hitec_ui.cpp and the label
  checks in test_kst_view.cpp; the wiki shows `menu-restarting.png` in place
  of `menu-pins-busy.png`.
- After v0.1.0-rc1 (a93e1c1): SERVO TESTER is titled `SERVO TESTER`, its MENU
  row's label, in place of `SERVO`, on the main view and the unplug page
  (decision 13 of [menu-b.md](docs/design/menu-b.md#decisions), slice 8 of
  menu structure B). `SERVO_UI_TITLE` in `src/ui_servo.h`, also MENU's
  label in `src/ui_registry.cpp`; host checks in
  `testScreenOpens()` and `testUnplugPage()` in `test/test_servo.cpp`; the
  previews and wiki images of the servo tester are regenerated.
- After v0.1.0-rc1 (a93e1c1): SERVO TESTER → SET: a TYPE, MIN or MAX step that
  leaves the profile past its travel and changes TYPE or takes MIN or MAX
  further out than the applied profile waits for `HOLD TO APPLY`, with the amber
  end-stop note as its warning; the heli and 60 Hz warnings come first. A step
  back inside the margin or to the applied value applies at once, as does a step
  toward the centre. SERVOS ONE to TWO on a profile past its travel waits too;
  TWO to ONE, and ONE to TWO within the travel, apply at once. A claim that
  takes SIG alone drops a pending TWO to one servo as it drops an applied one,
  and a wait that held only for TWO ends (`claimPins()`). By the owner's
  decisions of 2026-11-14, decisions 21 and 22 of `docs/design/servo-tester.md`.
  `servoTravelNeedsHold()` in `src/servo_model.*`; the wiki shows
  `servo-set-travel.png`.
- After v0.1.0-rc1 (a93e1c1): SETTINGS: RESET reads `HOLD RESET` and loads the
  defaults after a 1 s hold on the shared hold widget (`src/ui_hold.*`), with a
  bar over the button and a `C_RED` fill while held, as on every other hold of
  `src/ui_hold.*` but HOLD TO SAVE; a tap loads nothing and a slide off cancels.
  By the owner's decision of 2026-11-14. Host test `testResetHold()`.
- After v0.1.0-rc1 (a93e1c1): stored settings of versions 2, 3 and 4 carry
  over into version 5, by the owner's decision of 2026-11-14, in place of
  being discarded whole as v0.1.0-rc1 does and the entries below say
  (`settingsCarriedOver()`). DshotDisplay 2.x's version 2 block carries
  over too. Each older layout is a table of byte offsets in
  `src/settings.cpp`, recovered from `src/settings.h` at 8a5eebb (version
  2), 5af4ddf (3) and 3cc8798 (4); DshotDisplay 2.0 to 2.3 write the same
  version 2 layout. A block carries over only whole: the magic, its version,
  that layout's payload size and its CRC-32 right. Every field it has keeps
  its value through `settingsValidate()`; newer fields take their defaults;
  version 3's SERVO SUPPLY of 1, an enable on GP0, loads as `NONE`. Flash
  keeps the old block until a save lands, and HOLD TO SAVE writes a version
  5 block even with nothing changed. Until then the splash reads `SETTINGS
  CARRIED OVER FROM AN UPDATE` / `CHECK MENU > SETTINGS, THEN SAVE` in amber,
  MENU's SETTINGS row `UPDATED` and SETTINGS' note line `CARRIED OVER BY
  UPDATE - CHECK, SAVE`. A block of version 1, above 5, or of version 2, 3 or
  4 with another layout's payload size still takes the reset path. Host
  tests `testCarryOver()` in `test/test_settings.cpp` build each version
  byte by byte; the wiki shows `splash-carried.png` and
  `settings-carried.png`.
- After v0.1.0-rc1 (a93e1c1): a new log takes the highest `LOGnnnnn.BFL`
  in `LOGS/` plus 1, by the owner's decision of 2026-11-14, in place of the
  lowest unused number from 00001 to 09999, so the highest number is the
  newest log. The first start on a FatFs mount reads `LOGS/` once with
  `f_readdir()`, about one sector per 16 short-named files, in place of one
  `f_stat()` per number. The next number is kept with the mount's
  `FATFS::id` and read again after a mount, after FatFs mounts the card
  again on its own after a card fault, and after any logging error
  (`src/sd_log_next.cpp`). A name counts in any letter case
  (`sdLogNameNumber()`, `sdLogNextNumber()` in `src/sd_log.h`). After
  `LOG99999.BFL` no log starts and SD LOG reads `NO LOG STARTED:
  LOG99999.BFL EXISTS` / `MOVE LOGS TO A COMPUTER, RETRY MOUNT`
  (`SdLogEnd::NoFreeNumber`). The file opens with `FA_CREATE_NEW`, never
  over an existing file. Host tests `testLogNumbering()` for the rule and
  SD LOG, and `make -C test lognum`, the scan and the kept number over
  FatFs R0.15 on FAT32 and exFAT with a card swapped between mounts.
- After v0.1.0-rc1 (a93e1c1): KST SERVO, HITEC SERVO and AM32 CONFIG refuse
  a write, and KST and Hitec a
  restore, while the BOARD voltage is red (`BoardCell::Critical`,
  `uiBoardWriteRefused()` in `src/ui.h`), by the owner's decision of
  2026-11-14. KST and Hitec ask at the end of the hold, refused there with
  no file on the card and no frame to the servo, and again just before
  their files and their write go out after a card flush, refused there with
  the backup and the record possibly on the card and no frame out. AM32
  CONFIG asks once, before its backup. A write already running is not
  stopped: cut off, it would leave the servo partly written and unverified.
  The KST and Hitec write pages read
  `BOARD VOLTAGE IS RED: NOT WRITTEN.` and
  `CHARGE THE BOARD'S CELL, HOLD AGAIN.` in amber; AM32 CONFIG shows the two
  lines as its write's result. Amber, the text colour and no cell (below
  2.50 V) refuse no write. HITEC SERVO shows the horn line until a write
  has gone out, not after a refused hold. Host tests `testBoardLow()` in
  `test/test_kst_ui.cpp` and `test/test_hitec_ui.cpp` and
  `testAm32BoardLow()`; the wiki shows `kst-board-low.png`,
  `hitec-board-low.png` and `am32-board-low.png`.
- After v0.1.0-rc1 (a93e1c1): AM32 CONFIG opens settings in a layout
  revision outside 2 to 4, 1 or 5 to
  8, read-only, by the owner's decision of 2026-11-14, where the connect
  page used to show `LAYOUT n NOT KNOWN` with
  `THE TESTER KNOWS LAYOUTS 2 TO 4.`: a view of its own (`S_VIEW`) with the
  48 bytes in hex and no field, the strip `LAYOUT 5 NOT KNOWN: READ-ONLY.`
  over `NOT VERIFIED. BACKUP AM001` (or `NO CARD: NO BACKUP.`,
  `BACKUP FAILED.`), and the backup saved to the card as the view opens.
  BACK is the view's one control, and the screen has no transition from it
  to the write; the write refuses settings read in such a layout
  (`LAYOUT NOT KNOWN: NOT WRITTEN`). The backup of such a block has no field
  line and reads `Layout not known: fields not read, not verified`
  (`am32FieldShown()` shows no field outside layouts 2 to 4). Host test
  `testAm32ReadOnly()`, with a write forced past the view; the wiki shows
  `am32-readonly.png` in place of `am32-layout.png`.
- After v0.1.0-rc1 (a93e1c1): HITEC SERVO claims SIG when the screen opens, before the family tap, by
  the owner's decision of 2026-11-14: DShot drains and stops, and the pin
  stays an input with the pull-up on while the family view shows. The
  strip reads `GP0 IS THE SERVO'S: DSHOT IS OFF.` in green over
  `CONNECT THE SERVO, CHOOSE ITS FAMILY`; while an ESC answers on SIG,
  `AN ESC ANSWERS ON GP0: UNPLUG IT.` over
  `DSHOT STAYS ON THE PIN UNTIL THEN.` in amber with both families
  disabled. The tap starts the Hitec job for that family
  (`pinClaimStartJob()`, `PinUserInfo::deferJob` in `src/pin_claim.h`).
  BACK on the family view passes the unplug page and RESTART DSHOT; BACK
  there returns to the family view. D SERIES' note under `NONE` reads
  `POWER THE SERVO UP, THEN TAP.` Host tests `testDeferredJob()` in
  `test/test_claim.cpp` and `testFamily()` in `test/test_hitec_ui.cpp`; the
  wiki shows `hitec-family-esc.png`.
- SD LOG's FILE row keeps the last log's name after it ends, marked
  `(LAST)`, beside the counters that log left, until the next start or
  mount; a mount resets the name and the counters (`sdLogBegin()`). The
  rows are 19 px apart, for the two note lines.
- The tester draws `SDERR` on an amber chip in the status bar, between the
  arm badge and `BOARD`.
- KST SERVO and HITEC SERVO: a refused backup names the likely cause and the
  next step. `THE CARD DID NOT TAKE THE BACKUP (REMOVED OR SWAPPED?). NOTHING
  WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`, the same with `A FILE`,
  and `THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO.
  CHECK THE CARD, HOLD AGAIN.` replace the texts that ended in `NOTHING WAS
  WRITTEN TO THE SERVO.` `HitecResultText` holds 128 bytes.
- KST SERVO and HITEC SERVO: the write page's backup line reads `NO CARD:
  OLD VALUES LOST AT LEAVE`, `CARD ON USB: OLD VALUES LOST AT LEAVE` and `NO
  BACKUP ON THE CARD: LOST AT LEAVE` in amber, in place of the dim `... IN
  MEMORY` lines. The write is still allowed.
- The AM32 previews show an ESC with 3D MODE off, the ESC name on the
  strip; `am32-3d.png` shows the 3D note on the wiki's AM32 pages.
- AM32 CONFIG's `BIDIRECTIONAL` row is `3D MODE`: byte 0x12 is AM32's
  `bi_direction`, the reversible mode, not bidirectional DShot. TEMP LIMIT at
  141 and CURRENT LIMIT at 0, 101 or 102 read `OFF`, in the list and the
  editor: AM32 applies no limit outside 70 to 140 °C and outside 1 to 100
  (2 to 200 A) from 2.21, 1 to 99 before, where 100 reads `OFF` too.
- SETTINGS' note line reads `DEFAULTS LOADED - RESTART TO UNDO` after a
  RESET tap, while the live settings are still what RESET left and differ
  from what the next boot loads; any other change ends it, made in SETTINGS
  or elsewhere, and so does every completed HOLD TO SAVE, failed saves
  included, and a RESET over settings a restart would load anyway does not
  show it. RESET
  keeps its name and its single tap. After v0.1.0-rc1 it reads `HOLD RESET`
  and loads the defaults only after a 1 s hold (see Changed). Host tests in
  `test/test_menu.cpp`.
- HITEC SERVO's ID note states the consequence: the editor bar reads
  `BENCH ADDRESSES ID 0. A WRITE MAY LOCK OUT READ AND RESTORE.` and the
  write page `THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS
  SERVO AGAIN.`, in place of `THE BENCH ADDRESSES ID 0. ANOTHER ID IS
  NOT TESTED.` The wiki pages Hitec-Programming and Hitec-Programmierung no
  longer say that every D series servo takes a frame sent to ID 0: whether a
  servo with another ID answers it is not measured. Host tests in
  `test/test_hitec_ui.cpp`.
- SD LOG's mount button reads `RETRY MOUNT`, as on the KST and Hitec FILES
  pages. With NO CARD, mount result 3 NOT READY and no card size, the note
  line says `INSERT A CARD, THEN RETRY MOUNT` when nothing answered the
  card initialisation (CMD8 and the first CMD55), and
  `CARD NOT USABLE: RESEAT OR REPLACE`, in amber, when a card answered and
  failed it (`sdh_card_t::answered`, `sd_sdio_init_result()`,
  `SdLogStatus::cardInit`); a card that initialises and fails otherwise
  gets no hint. The screen also redraws when only the mount result
  or the card size changes, so MOUNT and CARD no longer show a stale value.
  Host tests in `test/test_ui.cpp`.
- README and the wiki pages AM32-Configuration and AM32-Konfiguration state
  what leaving AM32 CONFIG does to the ESC: no RUN command goes to the
  bootloader, and what the ESC does after the screen is left is not
  recorded. Both ask for a power cycle of the ESC after leaving, before
  driving a motor.
- `docs/design/diagnostics.md` and `src/diag.h` state that it is not known
  whether a glitch reset, or possibly a HZD_SYS_RESET_REQ reset, clears the
  watchdog's REASON register, so a UF2 copy followed by a glitch reset may
  read `UPDATE` or `REBOOT` instead of `GLITCH`, and one followed by a
  HZD_SYS_RESET_REQ reset instead of `DEBUGGER`; the bench list has the
  glitch check.
- MENU lists SETTINGS as the sixth row and SD LOG as the seventh, a swap.
  Six rows fit the 240 × 320 screen and the seventh needs a 36 px drag, so
  SETTINGS, which holds the motor pole count, the throttle ceiling and the
  pins, showed 6 px and its amber `UNSAVED` status was hidden after leaving
  it. SD LOG's state shows on the tester's status bar (`REC`, `SD`, `NOSD`,
  `SDERR`) and logging starts on arming, so it loses least below the fold.
  Reaching SD LOG takes a 36 px drag of MENU.
- MENU's EDT chip reads `NO ESC` on `C_GRID` while no eRPM frame has arrived
  for 500 ms, the normal state with no ESC connected. Red `EDT OFF` now means
  an ESC answers eRPM without EDT; green `EDT ON` is unchanged.
- HITEC SERVO's family view names the hardware CLASSIC needs:
  `NEEDS THE CONVERTER OF WIRING PART C` and
  `AND SETTINGS > SERVO SUPPLY GP28/29.`, in place of
  `NEEDS SERVO SUPPLY GP28 OR GP29`. With SERVO SUPPLY at `NONE`, D SERIES
  reads `POWER THE SERVO UP BEFORE THE TAP.` in place of
  `ON THE SERVO'S OWN SUPPLY`.
- SETTINGS scrolls 500 px, 100 px more than before, for the VERSION row and
  the CURRENT SENSE row.

- The panel backlight on GP16 runs from a PIO state machine on pio0 at
  20 kHz (`src/backlight.pio`), no longer from PWM slice 0, which it shares
  with GP0 and GP1. `test/pio_backlight_emu.py` checks every level, and the
  Firmware job holds its words to `pioasm`.
- The signal pins move to GP0 and GP1, and the switched servo supply's
  enable to GP28 or GP29. GP0 and GP1 are "Digital IO (FT)" pins, fault
  tolerant: they take up to 5.5 V while IOVDD, the I/O supply, is at 3.3 V
  (RP2350 datasheet, Table 1674). GP28 and GP29 are ADC (analog-to-digital
  converter) pins and not fault tolerant. SIG, the ESC wire and servo A,
  starts on GP0 (J4 pin 9, J5 pin 2) instead of GP29 (J4 pin 12); AUX, the
  KISS telemetry wire and servo B, on GP1 (J4 pin 10, J5 pin 1) instead of
  GP28 (J4 pin 11). `BOARD_FREE_GPIO_MASK` is `0x00000003`; UART0, whose
  pins they are, is unused. The servo tester's pulses run on PWM slice 0,
  GP0 on channel A and GP1 on channel B. SETTINGS → SERVO SUPPLY is a
  stepper, `NONE`, `GP28` (J4 pin 11) or `GP29` (J4 pin 12). The override
  pair in the examples and in CI is
  `-DDSHOT_PIN_LCD_2_8=1 -DKISS_PIN_LCD_2_8=0`. The settings block is
  version 4: a stored block of an earlier version is discarded whole and
  the compiled defaults load, so every saved setting returns to its default
  once after the update (v0.1.0-rc1; carried over after it, see Changed).
  The release notes give the ESC on GP0, KISS on GP1, the menu path
  MENU → SETTINGS, and GP28 and GP29 as supply enables only.
- The settings block is version 5: it adds CURRENT SENSE in the byte that was
  padding. A version 4 block is discarded whole and the compiled defaults load,
  so every saved setting returns to its default once after the update
  (v0.1.0-rc1; carried over after it, see Changed).
- With CURRENT SENSE at `INA219`, the servo tester's sweep count moves from
  the readout row to the first line of the dial, where the current takes
  its place.
- Hitec CLASSIC opens whenever SERVO SUPPLY is `GP28` or `GP29`, by the
  owner's decision of 2026-10-29: SIG is a fault tolerant pin, rated 5.5 V
  while the board is powered, and the servo supply is 5 V. The
  `HITEC_CLASSIC_MEASURED` switch and the `LINE LEVEL NOT MEASURED` text
  are removed. A `static_assert` in `src/ui_hitec.cpp` holds every free pin
  to the fault tolerant pads, GP0 to GP25.
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
- The settings block is version 3: it adds `servoSupply`, the SERVO SUPPLY row.
  The first boot after the update discards the stored version 2 block and starts
  from the compiled defaults, every setting and the ESC pin included
  (v0.1.0-rc1; carried over after it, see Changed). DshotDisplay, flashed back,
  discards a version 3 block the same way.
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
  vendor and product IDs (0x2E8A, 0x4006), and the settings block's magic `DSHT`
  and its place, the last 4 kB sector. Its layout version is 5, so settings
  saved by DshotDisplay 2.x (version 2) do not load: the first boot returns
  every setting to its default once (v0.1.0-rc1; carried over after it, see
  Changed).
- MiniRcBench supports one board, the Waveshare RP2350-Touch-LCD-2.8, and
  builds one image for it. The splash and SETTINGS → BUILT FOR name the board
  the image is built for.
- The settings block's board id is a record checked against the 2.8"'s, 28;
  validation rewrites any other value and re-judges the stored pins against
  the board's free pins, GP0 and GP1. The host suite pins the block layout
  DshotDisplay 2.x writes.
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
  are for the 2.8" only: SIG on GP0, AUX on GP1, PWM slice 0 channels A
  and B. `kst-programming.md` records the owner's decisions on the search
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

- Functions and constants nothing in the firmware called or read:
  `am32BlErasePage()`, `am32BlEscType()` and `am32BlAddrDiv4()` with its
  flag, `gfxFrame()`, `hitecLineIsOpen()`, `pioUartRxActive()`,
  `platReboot()`, `sdLogReleased()`, `st7789Sleep()`, `uiAm32Active()`,
  `uiKstAvailable()`, `uiServoAvailable()`, `usbDevMounted()`,
  `AM32_EEPROM_ADDR`, `SV_HOLD_MS` and `AM32_DEBUG`, with their fakes. The
  README said `AM32_DEBUG` mirrored every transport step to USB serial;
  nothing read it. `am32BlRun()` stays: it is the command that would start
  the ESC firmware when AM32 CONFIG is left, an open question.
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

- AM32 CONFIG showed LOW VOLT CUTOFF, VARIABLE PWM and BRAKE ON STOP as off
  and on, and AM32 stores three values in each: a stored 2 read ON, and a
  press of - or + made it 1, changing the mode. They are three-value
  choices now, with the configurator's names shortened to fit: LOW VOLT
  CUTOFF `OFF`, `CELL`, `ABSOLUTE`; PWM TYPE `FIXED`, `VARIABLE`, `BY RPM`;
  BRAKE ON STOP `OFF`, `BRAKE`, `ACTIVE`. The third value is offered from
  AM32 2.21, 2.18 and 2.19 on (see the next item for 2.21); on older
  firmware, read from bytes 0x03 and 0x04, the editor stops at the second
  value and a stored third reads `? (2)`.
  LOW VOLT CUTOFF's PROTECTION OFF note covers `? (2)`, which older AM32
  does not cut off on. An unedited byte is written back as read.
- AM32 CONFIG offered values the ESC's firmware does not act on. ABSOLUTE
  low-voltage cutoff is offered from AM32 2.21, not 2.19: 2.19 and 2.20
  compare the battery voltage in 10 mV with the raw byte, so it never trips;
  a stored 2 there reads `? (2)` with the PROTECTION OFF note, and ABS VOLT
  CUT reads the plain byte, not volts, below 2.21. CURRENT LIMIT at 100
  (200 A) reads `OFF` below 2.21. PWM FREQ stops at 48 kHz below 2.19. The
  PROTOCOL note also covers SERIAL, EDT ARM, DRONECAN and stored values 6
  to 9, on which AM32 drops every throttle from this tester: SERIAL and 5 to
  9 never unlock it, and EDT ARM locks it again on the zero frames the
  tester sends after the EDT enable. The editor offers 0 to 4 as the
  configurator does. ACTIVE BRAKE reads 0.5 % a step, as AM32 uses it. A
  stored number outside the range the tester offers on the ESC's firmware
  reads `? (n)`, as does an on/off byte of 2 or more. A firmware version
  byte AM32 did not write counts as the oldest. Settings in layout revision
  1 or above 4 do not open: the connect page shows `LAYOUT n NOT KNOWN`
  with `THE TESTER KNOWS LAYOUTS 2 TO 4.` and `NOTHING WILL BE WRITTEN.`
  (v0.1.0-rc1; read-only after it, see Changed).
  A field whose byte the ESC's firmware does not read is not listed: 0x2A
  to 0x2D below 1.80, 0x2E below 1.94, 0x2F below 2.16 (AlkaMotors commits
  9909ea7 and aa590e8 add those reads with the version), and below 1.80 the
  strip says `FIRMWARE BELOW 1.80 OR UNKNOWN:` /
  `FIELDS IT MAY NOT READ ARE HIDDEN.`; the backup file lists only the
  fields the firmware reads. CURRENT LIMIT acts from 2 at 1.80. ADVANCE
  steps in the old 0..3 encoding below 2.19, which reads nothing else, and a
  stored 10 to 42 reads `? (n)` there; edits wrote the 10..42 encoding on
  every firmware before. The garbled-reply page
  names the bytes once in its body; the strip shows the status too.
- AM32 CONFIG's KV read 20 KV low: AM32 takes raw x 40 + 20. MAX RAMP,
  MIN DUTY, ABS VOLT CUT and ACTIVE BRAKE showed raw bytes over 0 to 255;
  they read 0.1 %/ms, 0.5 %, 0.5 V and % steps over the configurator's
  ranges. DEAD BAND loses its `US`: AM32 counts it in input units.
- AM32 CONFIG's PROTOCOL names were one off: AM32's `enum inputType` is
  zero-based, so 0 is AUTO and 1 DSHOT, where the screen showed UNSET and
  AUTO. The names are now `AUTO`, `DSHOT`, `SERVO`, `SERIAL`, `EDT ARM` and,
  for a stored 5, `DRONECAN`; the editor offers 0 to 4.
- AM32 CONFIG's connect page showed `REPLY NOT UNDERSTOOD:` under
  `NO VALID SETTINGS` and `READ FAILED (...)` too, since the bytes of a good
  handshake stayed in the transport's buffer; it shows it now only for a
  handshake that failed with bytes.
- HITEC SERVO's write page always draws `KEEP THE SERVO HORN CLEAR.`, the
  ID note, `FILE OF ANOTHER MODEL` and `MODEL NOT MEASURED` when they
  apply. Before, the horn line came after the notes and was dropped once
  they reached y 180, for example on a D485HW write that changes ID 0 to 1
  and reverses DIRECTION. `hitecPageLayout()` in `src/hitec_view.cpp`
  reserves their lines, and one for the soft rules, before the changed
  values take what is left, a changed ID listed first; each soft rule is
  drawn whole or skipped, and the last line counts those left out,
  `AND n MORE RULES: SEE EACH ROW.` The page's ID note is two lines:
  `THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS SERVO AGAIN.`
  Host tests in `test/test_hitec_view.cpp` and
  `test/test_hitec_ui.cpp`.
- The wiki pages KST-Programming and KST-Programmierung no longer say the
  screen never switches the servo's supply: with SERVO SUPPLY at GP28 or
  GP29 it does. The English Home and Setup pages and Einrichtung say the
  image does not check which board it runs on. The Screens pages say that
  SETTINGS RESET stays in use after leaving the screen, until a restart
  without HOLD TO SAVE loads the saved values.

- On the unplug page of SERVO TESTER, KST SERVO and HITEC SERVO, the
  drawing reads `pinClaimUnplugDue()` once and takes both the page's text
  and BACK's enabled flag from that read; the control table does the same.
  Each used to read it twice, first for the text, then for BACK. Core1 can
  turn it from false to true between the two reads, so one frame could show
  `RELEASING THE PIN` (`RELEASING THE PINS` on SERVO TESTER) with BACK drawn
  live, and the control table could report BACK live without RESTART DSHOT.
- README, Hitec leave steps: with SERVO SUPPLY at `NONE` the unplug page
  reads `RELEASING THE PIN`, and BACK is drawn disabled until
  `UNPLUG THE SERVO.` shows.
- BACK on the unplug page of SERVO TESTER, KST SERVO and HITEC SERVO is
  drawn disabled, an outline with a grey label, until the job has let the
  pins go, as the control table already reported. It looked live and did
  nothing. `uiHeader()` takes BACK's enabled flag; each screen passes the
  expression its table reports and its tap is gated on.
- The tester's HOLD TO ARM is drawn disabled, an outline, while a USB host
  holds the card, as its control table already reported, and the throttle
  bar reads `EJECT THE USB DRIVE TO ARM`. It looked live and did nothing.
- AM32 CONFIG's write result fits the 240 px panel: a result longer than
  18 characters is drawn at scale 1, on one line; build-time checks keep
  every result within 37 characters. `WRITE VERIFIED. BACKUP AM001`, 334 px
  at scale 2, and `NO BACKUP ON THE CARD: NOT WRITTEN` ran off both edges.
- KST SERVO's search with the switched supply names an interrupted write:
  its two warning lines take the place of the view's first two explanation
  lines, where they found no free line and were dropped.
- A blackbox log whose last flush or directory sync fails ends in the error
  state; it read as a clean stop.
- MENU's BEEP flash, the backlight dip and SD LOG's refusal note no longer
  return 2^31 ms (24.9 days) after they ended, or after a flash or note that
  leaving its screen cut short.
- A BEEP request made while core1 takes the one before it is kept: the
  requests are counted, and core1 clears only the one it took.
- The settings write refuses a block larger than one flash page, 256 bytes;
  the guard allowed up to 4096 bytes into a 256-byte buffer. No caller
  passes more than a page.
- The build refuses a log buffer that is no multiple of the 512-byte chunk:
  such a buffer stopped logging at its first wrap.
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
- Manual corrections: the no-ESC pictures show NO TELEMETRY and `--` in the
  five ESC tiles; the backup ring is described as shared by KST, AM32 and
  Hitec; the AM32 connect failures `LAYOUT n NOT KNOWN` and `READ FAILED`
  are listed; the Troubleshooting page has a section for restarts, and
  Still stuck asks for LAST RESTART.

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
