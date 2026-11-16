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

## Unreleased

## 0.1.0 - 2026-11-15

### Added

- After v0.1.0-rc1: a touch controller that stops answering stops the outputs a
  finger would have to stop. The touch driver records the time of its last
  answer (`touchAnsweredMs()`, which core1's servo job reads too) and counts its
  failed finger reads in a row (`touchFailedReads()`, 0 after `touchInit()`).
  With four or more failed reads in a row and 100 ms or more since the last
  answer (`TOUCH_LOST_MS`, four UI frames), or no answer since `touchInit()`,
  the touch counts as not answering (`uiTouchLost()`, taken once a UI frame
  before any screen's tick). Then an armed tester disarms by the idle
  interlock's path, LATCH off; arming is refused and HOLD TO ARM is drawn
  disabled; `TOUCH NOT ANSWERING` replaces the swipe hint, and the 3D note, in
  amber. SERVO TESTER stops a running servo as STOP does, on the main view and
  on SET, refuses HOLD TO START and shows the same note on its dial, before
  every other note there, over `THE SERVO IS STOPPED.` when the stop or core1's
  250 ms cut (the heartbeat, or the touch silent with the switched supply) ended
  a run and `HOLD TO START WORKS ONCE IT ANSWERS.` otherwise; a Start waiting
  for a log to open is not sent while the touch does not answer, and the log
  ends; with the switched supply this comes before core1's 250 ms touch cut,
  which stays behind it for UI frames too slow to count four failed reads in
  250 ms. S.BUS OUTPUT stops its frames as STOP does, on the main view and on
  SET, reads `STOPPED: TOUCH DID NOT ANSWER` (`SbusOutStop::Touch`) until the
  next HOLD TO START, and refuses HOLD TO START while it lasts. A core0 stall
  alone, such as a card write, does not count: it is one failed read at most.
  ESC COMMANDS, the servo programmers, ESC SETTINGS and the USB bridge do not
  stop on it: their operations end by themselves, a write is not cut, and the
  bridge sends what the PC sends, with DShot off. Before, a latched throttle ran
  until the idle interlock, 30 s after the last touch, and S.BUS OUTPUT's frames
  ran on, on the START frame without end. Whether the controller answers with no
  finger on the glass for minutes is not measured (STATUS.md open item 23, bench
  test `OI-23`); by the owner's decision of 2026-11-15 the stop is in 0.1.0
  before that check. Host tests `testTouchLoss()` and `testTouchLossStall()` in
  `test/test_ui.cpp`, `testTouchLossStops()`, the switched supply's touch cases
  and the touch lost after a guard's cut in `testCutScreens()` in
  `test/test_servo.cpp`, `testTouchLoss()` in `test/test_sbus_out_ui.cpp` and
  the driver against the CST328 model, mutation-checked; the wiki shows
  `tester-touch-lost.png` and `sbus-out-touch-lost.png`.
- After v0.1.0-rc1: the DShot pump's three modes, slice D1 of the DShot
  lifetime design (`docs/design/dshot-lifetime.md`). Core0 asks for Run
  (`escTaskStart()`, which replaces `escTaskResume()`), Park
  (`escTaskPark()`) or Release (`escTaskSuspend()`), and core1 reports
  `escTaskRunning()`, `escTaskParked()` and `escTaskSuspended()`, the last
  true only in Release, so no core1 job binds on a parked pin. No mode is
  reported while core1 changes the pads, so core0 never reads "parked" with
  SIG not driven low. Park drains for more than 250 ms after the last
  throttle frame as a release does, then drives SIG low from the SIO
  (single-cycle input/output) block at 2 mA, with the output value 0 set
  before the pad becomes an output, and puts AUX in input, pull-down,
  input buffer off, the buffer off before the pull-down goes on
  (RP2350-E9). Release from Park frees the pins with no frame; Run from
  Park binds once with the EDT enable first; a wiring change in Park parks
  again, puts the old SIG in input, pull-down, input buffer off, and writes
  no pad whose state does not change, so SIG stays driven low across a
  bitrate-only change (`escParkPlan()`). A configure that lands during a
  bind or a park is rebuilt on the next pass: core1 clears the request
  before it reads the wiring. `escTaskBoundMs()` is the `millis()` of
  core1's last bind, 0 while parked or released. A teardown clears
  `initError`, so `DSERR` names the current bind only. Nothing calls
  `escTaskPark()` yet: the boot asks for Run and the pin claim for Release
  and Run, so DShot runs from boot as before. Host tests in
  `test/test_claim.cpp`; not run on hardware.
- After v0.1.0-rc1: the wiki shows the six screens no page showed, in both
  languages. USB BRIDGE's `RELEASING SIG GP0` strip, its failed bind
  (`SIG GP0 COULD NOT BE TAKEN (PIO)` with `RETRY`), the card held by USB
  DRIVE (`EJECT THE USB DRIVE FIRST.`), the lost bytes
  (`BYTES LOST: THE BRIDGE FELL BEHIND.`) and the leave page after a session
  that only read are on USB Bridge and USB-Brücke; the tester disarmed, with
  telemetry, is on The Screens and Die Bildschirme. The pages gain the three
  reasons of a failed bind (`PIO`, `CLOCK`, `REFUSED`) and that `BACK` leaves
  without a leave page then, a sentence on when the leave page has no FLASH
  note, and one on the grey `SWIPE = THROTTLE` while disarmed.
- After v0.1.0-rc1: the hardware bench plan,
  [docs/bench-plan.md](docs/bench-plan.md). It gathers the bench tests of
  the design notes' bench and bring-up sections and of STATUS.md's open
  items and verification table, 142 tests, into ten sessions that build on
  each other: the board and the SD card, then DShot and the pin claim, the
  ESC tools, the servo tester with the INA219 and the converter, the servo
  programmers, the receiver and the angle meter, and last the supply cuts
  with the servo log, the S.BUS bootloader probe against ESCs before an FC,
  the USB bridge with desktop tools, and the destructive tests on spare
  parts. Each test keeps its note's ID, with its setup and pins, steps, the
  note's pass criterion or "criterion not set in the note", what to record
  and where the result goes; an equipment list and a results template come
  with it. No test has run. README.md, STATUS.md's open items and the wiki's
  Home page in both languages link it. `test/check_docs.py`'s rule 8 checks
  the links of `docs/*.md` as well.
- After v0.1.0-rc1: the servo tester's log on the card, slice 6 of
  [current-sense.md](docs/design/current-sense.md#slice-6-as-built)
  (`src/servo_log.*`), with CURRENT SENSE at INA219 and a card mounted. Each run
  from HOLD TO START to its end writes `LOGS/SRVnnnnn.CSV`, one row per reading,
  `ms,pulse_a_us,pulse_b_us,current_ma,bus_mv,flags`, and an end line `#
  end=STOP rows=2400 dropped=0 up_ms=40` that names the end (STOP, BACK, a cut,
  a trip, a supply that did not come up), the rows taken and dropped and the
  supply's start-up time. The numbers count apart from `LOGnnnnn.BFL`. HOLD TO
  START draws `PREPARING THE LOG...`, opens the log in the next UI frame and
  sends the Start in the frame after, with the heartbeat and touch read afresh,
  so a slow open never makes core1 refuse the Start. The open reserves 4 MiB in
  one run of the card, or 64 KiB, mapped for FatFs's fast seek, and flushes
  every block of that to the card before the Start; past 2000 ms, checked before
  the FAT search, between the 4 MiB and the 64 KiB try and at each flush step,
  it is given up, `NO LOG: CARD TOO SLOW`, and the run starts without a log; its
  file is removed at the run's end only from the card it was opened on, by the
  volume's serial number. The SD write cache is on from the open to the close;
  during the run core0 writes one whole 512-byte block inside the reserve, only
  while nothing is queued or in flight, and makes no f_sync(), so the run reads
  nothing from the card. A running blackbox log stops at HOLD TO START: SD LOG
  reads `RECORDING STOPPED BY SERVO TESTER`, the run summary `Ended by: SERVO
  TESTER` (`SdLogEnd::ByServo`, `RunSumEnd::ByServo`). The dial's second line
  shows `LOGGING TO SRV00012.CSV` while RUNNING, or in amber the rows dropped,
  the 4 MiB limit, no room in one piece, a failed write, a slow card, a card
  that did not answer, or a log that did not open. A card removed or a failed
  block stops the writing, and the card is mounted again at the run's end. The
  write cache's swap check, the read of the volume's serial number before each
  batch, no longer waits for the card: `sdh_read_start()` and `sdh_read_poll()`
  in `src/sdio/sd_host.*`, with the bus's `rx_ready()` (`busRxReady()` in
  `src/sdio/sd_pio.c`), start it in one flush step and collect it in a later
  one. A read with a CRC, frame, timeout or no-response error is tried again, 3
  reads in all, with CMD12 after a block that did not come in time but no wait
  for its busy, and none after a whole block with a CRC error, which would cost
  5 ms, as `sdh_read()` tries a block; another error or a different number fails
  the batch at once. Every wait of core0 on the cache calls the driver's wait
  hook, so KST SERVO's and HITEC SERVO's card stamp stays fresh across a check
  read that times out. `sdLogNameNumberOf()` and the scan and open of
  `src/sd_log_next.*` take the kind of log. Host tests in
  `test/servo_log_fatfs.cpp` (`make lognum`) over FatFs R0.15, `card_cache.cpp`
  and `sd_cache.c` with a card model, and in `test/test_servo.cpp`, and the read
  in `test/test_sdio.cpp` and `test/test_sd_cache.cpp`, mutation-checked; README
  and the wiki pages Servo-Tester, Servotester, SD-Logging and SD-Aufzeichnung
  describe it. A real card's latency and the cuts' timing with the log on are
  not tested on hardware.

- After v0.1.0-rc1: S.BUS OUTPUT, slice 4 of
  [sbus-output.md](docs/design/sbus-output.md#slice-4-as-built), tested on the
  host only; nothing of it has driven a pin. OUTPUT on RX SIGNALS opens it,
  with the lines `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.` in
  place of `NOT BUILT`. The screen claims the role that is GP0, shows
  `CHECKING SIG GP0 FOR AN ESC.` while the bootloader probe runs and
  `AN ESC BOOTLOADER ANSWERS ON SIG GP0.` `UNPLUG THE ESC BATTERY AND LEAD.`
  `START WAITS UNTIL NONE ANSWERS.` while one answers, and offers HOLD TO
  START (1000 ms) only while `sbusOutProbePassed()` holds in that visit. 16
  channels on sliders over two pages, delta drag in 5 µs steps; the throttle
  channel, CH3 by default, stops at MAX THROTTLE's ceiling, raw 499 (1200 µs)
  at the default, on the screen as on core1; CH17, CH18, the failsafe and
  frame-lost bits and the throttle channel on SET (`OUTPUT SET`). STOP acts on
  the press; the job's heartbeat stops the frames when the screen stalls for
  250 ms (`STALLED`); more than 30 s with no touch while the throttle or a
  switch channel is off its START value stops them too.
  `GP0 GOES TO A FLIGHT CONTROLLER.` `NEVER TO AN ESC. PROPS OFF.` show while
  SAFE; after a stop the second stays under its reason, and SET's footer
  carries it while the frames are off. BACK opens the leave page at
  `UNPLUG THE FLIGHT CONTROLLER.`; after an EFM8 bootloader answered, it stops
  the frames and opens ESC SETTINGS' step
  `UNPLUG THE ESC BATTERY, THEN LEAVE.` first. BACK TO OUTPUT probes again;
  RESTART DSHOT on that page clears the EFM8 mark. The README and the wiki
  pages RX Signals and RX-Signale describe it with its warnings, and
  `tools/check_wiki.py` fails a page of that pair that drops one. The host
  suite's source test now holds the one START to HOLD TO START gated by the
  probe's gate, where it held the output unreachable; `sbusOutJobHeld()` lets
  the tests read each command the screen sent. The UART0 fake counts the
  characters waiting in its TX FIFO from the first that may still wait, so a
  30 s run of frames costs no rescans.
- After v0.1.0-rc1: the S.BUS output's bootloader probe, slice 3 of
  [sbus-output.md](docs/design/sbus-output.md#slice-3-as-built), tested on
  the host only; nothing opens the output yet. Every bind of the S.BUS
  output's job first runs ESC SETTINGS' connect cycle on GP0, 1000 ms on the
  pull-up and then 3 greetings, rcbench's first contact, on ESC SETTINGS'
  escwire line driver from core1, and repeats it until a cycle passes in
  which no greeting got a byte back; only then does UART0 take GP0. The line
  goes back to the pull-up after every greeting, so each quiet second is on
  the pull-up. The job takes no command during the probe and none sent before
  it, so START is impossible until such a cycle has passed in that bind, and
  BACK TO OUTPUT probes again. An answer from an EFM8 bootloader (Bluejay,
  BLHeli_S), a piece of one, or an AM32 bootloader sets MENU's run cue,
  `POWER-CYCLE THE ESC TO RUN IT`, until an eRPM frame after it; an EFM8
  answer marks the claim for the leave page's step that asks for the ESC's
  battery to come off and calls `pinClaimWantLeavePage()`. The screen of
  slice 4 is to offer START only through `sbusOutProbePassed()`. UART0 now
  starts at the bind with no pad on it, the pull-down comes when the probe's
  line closes, and a refused bind, for the UART's clock or the line's
  resources, leaves GP0 on the pull-up. The unreachability test also fails a
  line under `src/` that reaches a pin user or a core1 job by a cast, a
  pointer cast or a walk over the user rows or the job table. The escwire
  line's fake writes the driver's pad changes to the GPIO fake and serves
  rcbench's AM32 model. CI's stack check of the job reads the probe's call
  graphs: 376 bytes of 1,792.
- After v0.1.0-rc1: MSP and the 4-way interface on the USB bridge, slice 3
  of [usb-bridge.md](docs/design/usb-bridge.md#slice-3-as-built), tested on
  the host only. The PC's first byte chooses the mode: `$` MSP (the MultiWii
  Serial Protocol), 0x2F the 4-way interface, any other passthrough.
  `src/bridge_msp.*` answers the MSP requests of ESC-Configurator and the
  AM32 configurator as a flight controller named `MRCB` with one motor, and
  refuses `MSP_SET_MOTOR` with `MOTOR COMMAND FROM THE PC REFUSED.` on the
  screen for 5 s. `src/bridge_4way.*` runs the 4-way interface for the one
  ESC on SIG: each frame is checked whole, its CRC included, and turned into
  escwire's bootloader frames, with 4 ms of quiet line between SET BUFFER
  and its data; ESC 1 and up, Atmel and C2 commands are refused. A 4-way
  write or erase disables BACK and sets the `FLASH THE ESC AGAIN BEFORE USE.`
  note as passthrough's do, until `cmd_DeviceReset`. Both modes read byte
  0x12 of AM32 settings pages for the tester's 3D note (decision 12). The
  screen reads `4-WAY INTERFACE` and names MSP and 4-way commands on its
  last line. ESC CONFIG's second line under `USB BRIDGE` reads
  `MSP, 4-WAY AND PASSTHROUGH.`. `blAnswerAm32Settings()` gives an AM32
  family's settings address, which `am32_bl.cpp` now uses too.
  `test/test_bridge_proto.cpp` runs both against model AM32 and BLHeli_S
  bootloaders and models of the two tools' sequences.
- After v0.1.0-rc1: the S.BUS output's core1 job, slice 2 of
  [sbus-output.md](docs/design/sbus-output.md#slice-2-as-built), tested on
  the host only; nothing opens it. `Core1Job::SbusOut`
  (`src/sbus_out_job.*`) sends rcbench's encoded frames from UART0's TX on
  GP0, inverted 8E2 at 100,000 baud, one every 14 ms and never two under
  7 ms apart. The bind puts the pull-down on GP0, starts UART0 at 8E2 with
  its receiver off, and writes FUNCSEL, the function select, and OUTOVER,
  the output override, in one write of GP0's control register, so the pad
  never carries the UART's high idle. START
  sends core1's frame: the throttle channel, one of 1 to 4, at raw 172, the
  other sticks at 992 and channels 5 to 16 at 172; every channel is clamped
  to raw 172 to 1811 and the throttle to 172 + 1639 × MAX THROTTLE / 2000.
  Core1 takes the ceiling from MAX THROTTLE itself, so a wrong ceiling
  from a screen cannot pass it. STOP and a heartbeat lapse of 250 ms end
  the frames. The release waits for the frame on the wire, at most 3 ms,
  with a 10 ms backstop, puts GP0 on SIO, the single-cycle I/O block that
  drives GPIOs from software, as a low input with the pull-down, and only
  then stops UART0. The bind first turns SIO's output on GP0 off, as the
  release does.
  `PinUser::SbusOut` claims the role that is GP0 with the leave page and
  the ESC check in either wiring; `CORE1_JOB_NO_GP0` refuses a wiring with
  no role on GP0. No screen claims it: the bootloader probe of slice 3
  comes before any START. The UART0 fake gains its transmitter and UARTCR,
  the GPIO fake records each control-register write and keeps the pad
  register, its pulls included, from the RP2350's reset value 0x116,
  and `test/test_sbus_out_job.cpp` checks the bind and the release write by
  write, reads the frames sent back with rcbench's decoder, and fails if
  any code line under `src/` but the job's declarations, its table line and
  the claim's row names the output. CI
  checks the job's stack on core1.
- After v0.1.0-rc1: rcbench's S.BUS encoder, slice 1 of
  [sbus-output.md](docs/design/sbus-output.md#slice-1-as-built). The S.BUS
  files in `lib/rcbench/protocols/sbus/` and `test/rcbench/test_sbus.c` are
  replaced, unchanged, by rcbench's at commit b88593d3: `sbus_encode()`,
  which packs a frame and refuses one with a channel above 2047,
  `sbus_from_us()`, the inverse of `sbus_to_us()`, and `SBUS_FOOTER`. The
  decoder's code is the same as at 5ef42efd, and the S.BUS monitor runs
  unchanged on it. `test_sbus.c` has 19 cases, 12 of them the decoder's.
- After v0.1.0-rc1: move timing from the current sensor, slice 5 of
  [current-sense.md](docs/design/current-sense.md#slice-5-as-built)
  (`src/move_timer.*`), with CURRENT SENSE at INA219. The servo tester's
  core1 job times each step of `MIN`, `CENTRE` or `MAX` at SPEED 100 % with
  rcbench's `servo_move.c`, copied unchanged at `5ef42efd` with
  `servo_sim.c` and its test `test_servo_move.c`, 18 cases. SERVO SET shows
  the last step on its note band's second line when free, dim:
  `MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`, `LATE` or `UNSEEN` in the time's
  place, `--` for a figure whose conditions do not hold; before the first
  step after START, the supply's start-up time. With CURRENT SENSE at NONE
  nothing is timed, and a move under way when it turns NONE is void as lost
  (not reachable through the screens). A step is timed 500 ms after the servo
  came to rest: the last timed move's arrival or the end of a late or unseen
  move's window, or 3061 ms after any other change of the pulse. The time is
  an upper bound on when the current shows the arrival, by up to 61 ms of the
  sensor's lag, one read and two conversion cycles, and a PWM frame; a servo
  easing its drive near the target can show the arrival before the horn stops.
  The timing changes no pulse and no cut. Host tests in
  `test/test_move_timer.cpp` and `test/test_servo.cpp`, against rcbench's
  servo model too, mutation-checked; README and the wiki pages Servo-Tester
  and Servotester describe it. Not tested on hardware.
- After v0.1.0-rc1: a Bluejay or BLHeli_S ESC's settings are written, slice 4
  of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-4-as-built).
  On Bluejay 208 and BLHeli_S 33 the list edits the fields rcbench's map lets
  a write change, 15 and 12, with the editor bar and swipes, and `WRITE...`
  opens `ESC WRITE`. Its HOLD TO WRITE saves the page as first read in the
  session to the card as `BACKUP/BJnnn.TXT` or `BHnnn.TXT`, rcbench's ESC
  settings text (`src/efm8_backup.*`, the prefixes in `src/backup_ring.*`),
  and then the escwire job writes the draft (`EwJobOp::Write`): re-read,
  erase, program and read-back, up to 3 procedures, on the settings page only.
  A card that refuses the backup or the BOARD voltage in red stops the write.
  The results, `WRITE VERIFIED. BACKUP BJ018` over `LEAVING STARTS THE ESC
  WITH THEM.`, `READ FAILED: NOTHING ERASED.`, `ESC PAGE CHANGED: NOT
  WRITTEN.`, `WRITE FAILED: PAGE AS READ.`, `HALF WRITTEN: KEEP THE ESC
  POWERED.` and `NO ANSWER IN THE WRITE: KEEP POWER.`, each with its next
  step; `READ AGAIN` in `WRITE...`'s place after a changed page or a lost
  answer; on a half-written page the list edits nothing, `WRITE...` writes the
  draft again or, after REVERT, the page as read, and BACK is guarded, the
  second aborting the session with no LEAVE and opening the leave page. A
  heartbeat lapse lets a write that has sent its erase run to its read-back.
  The read-only lists save their backup as they open, `SHOWN ONLY. BACKUP
  BJ018`, and skip a copy equal to the ring's latest. A session that ends with
  its page half written says `SESSION ENDED: RESTORE ON A COMPUTER.` and keeps
  the old page for the visit, on `OLD PAGE` and under a later list's ESC HEX.
  ESC CONFIG's line reads `BLUEJAY, BLHELI_S: READ AND WRITTEN.` Tested on
  rcbench's simulated ESC only; nothing has been written to an ESC.
- After v0.1.0-rc1: the angle meter's screen and its documentation, slices 4
  and 5 of [angle-meter.md](docs/design/angle-meter.md#slice-4-as-built),
  tested on the host only. ANGLE METER (`src/ui_angle.*`) opens from MENU →
  TOOLS (`src/ui_tools.*`), a chooser of the bench tools that claim no pin,
  as decision 1 of the note puts it in MENU's sixth row. It opens the QMI8658
  driver and shows the model's reading: the angle in tenths of a degree from
  the zero or from level, `---` while there is no reading or the axis is
  tilted 70° or more, the axis's tilt on the strip, amber from 45°, the chip
  `SETTLED`, `MOVING`, `TOO STEEP`, `STARTING` or `NO IMU`, the travel in mm
  with LENGTH set, MAX each side with its travel and the IMU's temperature
  since the zero. ZERO takes a settled reading a second after the finger
  lifts, or none within 10 s; RESET MAX clears MAX. SETUP sets ALONG AXIS,
  LENGTH, SIGN and opens FLIP CAL, whose three poses end in a result that
  the 1 s HOLD TO KEEP applies and saves; touches are ignored while the
  board lies screen down. BACK on the main view writes CTRL7 00h. The four
  fields are saved by `settingsSaveAngle()`, which writes them over the
  settings as last saved. The screen states no accuracy: none is measured.
  The preview `docs/angle-preview.png`, and the wiki pages Angle-Meter and
  Winkelmesser. Not tested on hardware.
- After v0.1.0-rc1: the cuts on the current sensor's readings, slice 4 of
  [current-sense.md](docs/design/current-sense.md#slice-4-as-built)
  (`src/supply_guard.*`), with CURRENT SENSE at INA219. The servo tester
  stops on one reading of 3.0 A or more of either sign or a clipped one
  (`OVERCURRENT`), on readings above 1.0 A for 400 ms (`STALL`), and with
  SERVO SUPPLY at `GP28` or `GP29` on readings under 4.0 V for 1000 ms after
  the supply came up (`LOW SUPPLY`) and on a sensor not online or 250 ms
  without a reading (`NO SENSOR`, `CURRENT SENSOR LOST: SUPPLY OFF.`). The
  pulses end and the enable pin goes low in the same core1 pass; the chip,
  magenta, and the dial's note stay until HOLD TO START. With `OWN` only
  the first two apply, and while RUNNING with the sensor not online the dial
  reads `NO SENSOR READING: NO CURRENT CUTS.` KST SERVO and HITEC SERVO switch the supply off on the
  hard and the collapse cut while it is on, during a read or a write too:
  the session is power-cycled, Hitec cuts the transaction on the line, and
  the page names the cut over RETRY; no switch-on follows until RETRY. Each
  new reading counts once, and a stale one never. Host tests in
  `test/test_supply_guard.cpp`, `test/test_servo.cpp`,
  `test/test_kst_job.cpp`, `test/test_hitec_job.cpp` and the KST and Hitec
  screen suites, mutation-checked; README and the wiki pages Servo-Tester,
  The-Screens, KST-Programming, Hitec-Programming and Troubleshooting with
  their German pairs describe them. Not tested on hardware.
- After v0.1.0-rc1: the board's clock is set from a computer over the USB
  serial port, slice 5 of
  [run-summary.md](docs/design/run-summary.md#slice-5-as-built).
  `tools/set_clock.py` finds the board by its USB IDs, 2E8A:4006, or takes
  `--port`, and sends the computer's local time as
  `CLOCK 2026-11-14 15:42:07`; `--query` sends `CLOCK?`, `--list` names the
  boards found. The firmware reads the line in `usbDevTask()`'s pass,
  one line a pass (`src/clock_line.*`), parses it strictly and answers
  `CLOCK SET` and the time, `CLOCK NOW` and the time or `CLOCK NOT SET`, or
  `CLOCK REFUSED:` with `BAD DATE`, `BAD LINE`, `RTC NOT ANSWERING` or
  `RTC READ-BACK DIFFERS`; other lines are ignored, and so are lines that
  start as an answer does, so a terminal echoing the answers back sets
  nothing. `usbDevClockLineEnable()` turns the line off for the USB bridge.
  `clockSet()` writes the PCF85063A in four transfers through `senseNote()`,
  the software reset, STOP with CLKOUT off, the time with OS clear, STOP
  released, and reads it back, stopping the part when the read-back differs; a
  set that reads back becomes the kept time and counts one set more, so a log
  across it reads `(clock set during the log)`. A set is accepted during a
  log. SETTINGS → BOARD gains a read-only `CLOCK` row, the time to the
  minute or `NOT SET`, with the caption `SET AT EACH POWER-UP`: the board
  has no cell on J2. Host tests in `test/test_clock.cpp`, `test/test_menu.cpp`
  and `test/test_set_clock.py`, which `make -C test run` runs. README and
  the wiki pages SD-Logging, SD-Aufzeichnung, The-Screens and
  Die-Bildschirme describe the set. Not tested on hardware.
- After v0.1.0-rc1: RPM/V and the kV estimate on the tester, slice 3 of
  [esc-commands.md](docs/design/esc-commands.md#slice-3-as-built)
  (`src/kv_est.h`, decisions 6 and 7). While armed the ERPM line reads
  `ERPM 156800  RPM/V 1400`, the RPM over the ESC's voltage, KISS where
  fresh, EDT otherwise, and `KV EST` takes the pole count's place:
  RPM × 2000 / (V × t), t the throttle sent, 0 to 2000, rounded and clamped
  to 99999. A figure needs eRPM within 500 ms, an RPM above 0 and a fresh
  voltage above 0.5 V, else `RPM/V --` and `KV EST --`; the estimate also
  needs t from 1500 (75 %) to 2000, within 20 of one value for 1000 ms
  (`KV_EST_MIN_THROTTLE`, `KV_EST_SETTLE_BAND`, `KV_EST_SETTLE_MS`), so that
  a finger resting on the bar does not hold it off, the filtered eRPM
  within 0.5 % of the last reply (`KV_EST_ERPM_SETTLE_PERMILLE`), and no 3D
  note, and reads `KV EST 75%+` below 1500. Nothing is kept after the
  disarm. An ESC in 3D mode the tester does not know of gives a wrong figure
  with no warning, about × 0.67 on AM32 at 75 %. Host tests in
  `test/test_kv_est.cpp` and `test/test_ui.cpp`; the wiki's `tester-kv.png`
  and `docs/ui-preview.png`. Not measured on a motor.
- After v0.1.0-rc1: a Bluejay or BLHeli_S ESC's settings are read, slice 3
  of [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-3-as-built).
  ESC SETTINGS' connect page greets in bursts of 3 between seconds of quiet
  line, `LINE HIGH, NOTHING SENT` (`src/greet_cycle.*`). An EFM8 answer, now
  `471` with 0xE8 four bytes on anywhere in the reply, ends the AM32
  transport and starts the claim's core1 job, `Core1Job::EscWire`
  (`src/escwire_job.*`, `src/escwire_port.*`), which runs rcbench's escwire
  session on `ew_line`: its own first contact, the identification and the
  read of the page, `EFM8 E8B2 ANSWERED: READING.` meanwhile. A Bluejay or
  BLHeli_S page opens as a list, every row read-only, with its group, its
  value in capitals and DIRECTION's `3D` and `3D REVERSED`
  (`src/efm8_view.*`), WRITE... and REVERT drawn disabled, ESC HEX of 112 or
  255 bytes, the strip's caption, read-only lines and row notes, and the
  tester's 3D note. A page rcbench does not take is named on the connect
  page, `EFM8 E8B2: SETTINGS PAGE EMPTY` or `EFM8 E8B2: PAGE NOT KNOWN` and
  the like. BACK from the list sends LEAVE, `00 01`, and gives SIG back to
  DShot; MENU's run cue stays until an eRPM frame, as LEAVE has no answer.
  Every other BACK after an EFM8 answer opens the leave page, as before, and
  so does BACK after a reply that holds a piece of an EFM8 answer, `471`,
  `71` and 0xE8 two bytes on, or 0xE8 with 0xB1, 0xB2 or 0xB5. A refused
  bind puts the pull-up back on SIG. A session that ends without the page greets
  again. The job stops an entry or read on a heartbeat lapse and lets a
  LEAVE finish. Nothing writes a Bluejay or BLHeli_S ESC. Host tests in
  `test/test_escwire_job.cpp`, `test/test_efm8_view.cpp`,
  `test/test_am32.cpp` and `test/test_ui.cpp` against rcbench's simulated
  ESC; CI checks the job's stack on core1. Not tested on hardware.
- After v0.1.0-rc1: the angle meter's model, slice 3 of
  [angle-meter.md](docs/design/angle-meter.md#slice-3-as-built)
  (`src/angle_model.*`), pure. It averages the samples of the QMI8658 IMU
  (inertial measurement unit) in fours, calibrates them per chip axis, maps them
  to the board's edges by `BOARD_IMU_AXES`, and feeds `theta`, `atan2(a_u, a_z)`
  about the edge ALONG AXIS names, and the tilt of that edge into rcbench's IMU
  model at IMU FILTER 3. A reading is settled after 1000 ms still; from a tilt
  of 45° it is amber, from 70° `---`. ZERO takes the first settled frame at
  least 1000 ms after the finger lifts, or none within 10 s; MAX holds the
  largest settled deflection each side; SIGN turns the reading and swaps the
  sides; LENGTH gives the trailing edge's travel; the IMU's temperature since
  the zero warns from 2.0 °C. The three-pose flip, screen up, turned end for
  end, screen down, finds each axis' offset and the normal's gain.
  `src/angle.cmake` builds it into the firmware with rcbench's `servo_rudder.c`,
  `imu.c` and `witmotion.c`, `-Wtype-limits` off for `imu.c`, and CI's syntax
  checks get the WitMotion include directory. Nothing calls the model yet: the
  screen, slice 4, does. Host tests in `test/test_angle_model.cpp`; the cases of
  rcbench's `test_imu.c` (9) and `test_imu_filter.c` (10) that need `imu.h`
  alone are adapted into `test/rcbench/`. Not tested on hardware.
- After v0.1.0-rc1: the host suite sets the fake clock before each screenshot
  that draws from the clock: `fakeClockAlign()` before the KST search
  spinner and the ESC SETTINGS connect spinner (`shot_kst_search`,
  `shot_kst_switched`, `shot_am32_connect`, `shot_am32_connect_garbled`,
  `shot_am32_efm8`) and
  `fakeClockSetMs()` before SETTINGS' UPTIME (`shot_settings_scrolled`). The
  previews no longer change when a test is added or removed. No firmware
  change.
- After v0.1.0-rc1: the USB bridge's passthrough and its screen, slice 2 of
  [usb-bridge.md](docs/design/usb-bridge.md#slice-2-as-built), tested on the
  host only. ESC CONFIG's `USB BRIDGE` opens USB BRIDGE (`src/ui_bridge.*`),
  which claims SIG unless USB DRIVE holds the card (`EJECT THE USB DRIVE
  FIRST.`). While the claim runs, `src/usb_bridge.*` takes the serial port
  from the console (`usbDevConsole()` in `src/usb_dev.*`), hands each PC frame
  to the wire after 2 ms of silence and writes the wire's bytes, the echo
  included, back to the PC; the console has the port again once the claim is
  free. The screen shows the port, `WAITING` or `PASSTHROUGH`, the bytes each
  way, `FRAMES`, `LOST` and the last command. BACK is drawn disabled while a
  PROGRAM, ERASE or SET BUFFER payload waits for its answer, at most 3000 ms,
  and every other BACK of the running bridge opens the leave page `POWER THE
  ESC OFF.`, `RESTART DSHOT.`, as the servo tools' BACK does; MENU shows
  `POWER-CYCLE THE ESC TO RUN IT` from the open until RESTART DSHOT or an eRPM
  frame. RUN counts only as 4 bytes with its CRC. While the bridge holds
  the port the clock's `CLOCK` line is off and goes to the ESC as data.
  `USB_BRIDGE_LOG`, 0 by default, builds DshotDisplay's traffic log
  (`src/bridge_log.*`), dumped on the port after BACK. DshotDisplay's bridge
  tests are ported to a CDC fake (`test/fake_cdc.*`,
  `test/test_usb_bridge.cpp`); `test/test_bridge_ui.cpp` runs the screen,
  `test/test_bridge_log.cpp` the log built on. MSP and the 4-way interface
  followed in slice 3.
- After v0.1.0-rc1: the USB bridge's line and claim, slice 1 of
  [usb-bridge.md](docs/design/usb-bridge.md#slice-1-as-built), tested on the
  host only; nothing opens it yet. `PinUser::Bridge` claims SIG with no ESC
  check and no leave page of its own and runs `Core1Job::Bridge`
  (`src/bridge_wire.*`): DshotDisplay's frame slot and 1024-byte ring
  between the cores, on a 19200 baud 8N1 line of two PIO state machines in
  one block (`src/bridge_line_port.*`). The transmitter,
  `src/bridge_line.pio`, is 7 words and sets the pad's direction by
  side-set: driven through a frame, an input with its pull-up between
  frames. `pio_uart_rx` receives on the same pad, and the job matches the
  echo against the bytes sent, counting a frame whose echo differs or comes
  short as a collision. The release finishes the frame under way, at most
  250 ms. `test/pio_bridge_emu.py` runs both programs on one pad and CI
  holds its assembly to pioasm; `test/test_bridge.cpp` runs the job on a
  line fake and through the claim.
- After v0.1.0-rc1: the board's real-time clock dates the files on the
  card and the run summary's clock lines, slice 4 of
  [run-summary.md](docs/design/run-summary.md#slice-4-as-built).
  `src/clock.*` reads the PCF85063A at 0x51 once at start-up, after
  `senseInit()`, with its transfers noted to the current sensor's bus
  record; a running clock's time is kept by the microsecond timer. With the
  oscillator-stop flag set, as after a power-up without a cell on J2, the
  firmware sends the part's software reset and turns CLKOUT off; with STOP,
  EXT_TEST, 12-hour mode or a register out of range it writes nothing.
  `get_fattime()` returns the kept time while the clock is set and 0
  otherwise, and the SD library's `my_rtc.c` is left out of the firmware.
  The `.TXT` carries `Clock at start` and `Clock at stop` as
  `2026-11-14 15:42:07` or `not set`. Nothing sets the clock yet (slice 5),
  so a board whose clock lost its power keeps FAT date 0. Host tests in
  `test/test_clock.cpp` against a register-level PCF85063A model in
  `test/fake_i2c.*`, and the clock lines in `test/test_run_summary.cpp` and
  `test/sd_log_fatfs.cpp`. README and the wiki pages SD-Logging and
  SD-Aufzeichnung describe the file dates. Not tested on hardware.
- After v0.1.0-rc1 (a93e1c1): ESC COMMANDS, slice 2 of
  [esc-commands.md](docs/design/esc-commands.md#slice-2-as-built), MENU's
  first row in place of BEEP THE ESC (decision 1, `src/ui_esc_cmd.*`). BEEP
  1 to 5 are taps, one beacon each; NORMAL and REVERSED, and 3D OFF and
  3D ON, are 1000 ms holds that send 7, 8, 9 or 10, then 12, the save
  (decisions 2 and 3). The page claims no pin and opens only while the pin
  claim is free; opening it disarms. Its controls act while an ESC answers,
  the motor is stopped and the ESC has answered for 1500 ms; a request made
  while the slot is busy, an EDT enable among them, waits there, at most
  2000 ms. Otherwise one amber line says why:
  `NO ESC ANSWERS ON SIG GP0.` (decision 5),
  `MOTOR TURNING: WAIT UNTIL IT STOPS.` or `ESC STARTING: WAIT 1.5 S.`.
  While its request waits or its sequence goes out, BACK is drawn disabled.
  BACK and the BEEP taps act only on a press that landed while they were
  live. Six message lines name what was sent or why not, among them
  `SENT 8 (REVERSED), THEN 12 (SAVE).`, `SAVE NOT SENT: POWER-CYCLE THE ESC.`
  and `NOT SENT: THE ESC WAS NOT READY.`. 3D ON, sent or stopped after the
  ESC may have acted, sets the tester's 3D note before BACK is read; 3D OFF
  leaves it (decision 4); no direction note (decision 10). Host tests in
  `test/test_esc_cmd_ui.cpp` and `test/test_ids.cpp`;
  `docs/esccmd-preview.png`. Not tested on hardware.
- After v0.1.0-rc1: SD LOG shows the last log's run after a stop, slice 3
  of [run-summary.md](docs/design/run-summary.md#slice-3-as-built). Rows 2
  to 8 read `MAX RPM`, `MAX CURRENT`, `MAX TEMP`, `MIN VOLTAGE`, `USED` or
  `USED FROM CURRENT`, `ARMED` and `SUMMARY` from `SdLogStatus::summary`
  through `runSumRowText()`, the RPM at MOTOR POLES at the stop. A source
  tag beside a value reads `KISS` in the text colour or `EDT` and `MIXED`
  dim; an unknown value reads `--`, dim. `MAX TEMP` at 90 °C and above and
  `NOT SAVED: CARD FULL` or `NOT SAVED: WRITE FAILED` are red. FRAMES,
  WRITTEN, DROPPED FRAMES, BUF PEAK, WORST FLUSH, CARD and MOUNT show while
  a log runs and while no summary shows: before the first log and after a
  mount or USB DRIVE. The screen redraws when the summary changes. The
  tester shows nothing of the run. README and the wiki pages SD-Logging and
  SD-Aufzeichnung describe the rows; the wiki's `log-last.png` and
  `log-card-full.png` show them. Host tests in `test/test_ui.cpp`. Not
  tested on hardware.
- After v0.1.0-rc1: the QMI8658 IMU (inertial measurement unit) driver, slice 2
  of [angle-meter.md](docs/design/angle-meter.md#slice-2-as-built)
  (`src/qmi8658.*`, `src/angle.cmake`). While the angle meter is open it reads
  WHO_AM_I at 0x6B on I2C1, writes a soft reset, waits for dQY_L 80h after 15 ms
  or for 1750 ms, writes CTRL1 40h, CTRL2 08h, CTRL5 07h and CTRL7 01h one byte
  each and reads each back, and from 100 ms later reads one 11-byte burst and
  the time stamp again every UI frame, after touch and the current sensor; the
  first whole burst is a baseline, so the first sample taken was latched after
  the 100 ms; a burst whose time stamp changed is dropped, a repeated time stamp
  is not counted. Its transfers go to the shared bus record; three failed frames
  take it offline, probed again every 1000 ms, and no reset follows another
  within 2000 ms. Closing it writes CTRL7 00h. No screen opens it yet, so
  nothing is sent to 0x6B. The board header gains `BOARD_IMU_AXES`, a
  placeholder until bench test B3. Host tests in `test/test_qmi8658.cpp` against
  a register model of the part in `test/fake_i2c.*`; `test/angle.mk` lists them.
  Not tested on hardware.
- After v0.1.0-rc1: rcbench's files for the angle meter, slice 1 of
  [angle-meter.md](docs/design/angle-meter.md#slice-1-as-built), copied
  unchanged from rcbench e98345aa into `lib/rcbench/`: `servo_rudder.*`, the
  deflection from a zero, its travel in mm and the largest deflection each
  side; `imu.*`, the IMU (inertial measurement unit) model with
  `imu_feed_angles()`, which takes three angles and a time; and
  `witmotion.*`, the WitMotion decoder `imu.h` includes. Their tests,
  `test_servo_rudder.c`, `test_imu_angles.c` with `wit_frame.h` and
  `test_witmotion.c`, 25 cases, are in `test/rcbench/`, run by `make -C test
  run` through `test/angle.mk`. Slice 3, above, builds them into the
  firmware with their caller and adapts the cases of rcbench's `test_imu.c`
  and `test_imu_filter.c` that need `imu.h` alone.
- After v0.1.0-rc1: each blackbox log that stops clean gets its run
  summary beside it, `LOGS/LOGnnnnn.TXT`, slice 2 of
  [run-summary.md](docs/design/run-summary.md#slice-2-as-built).
  `sdLogTick()` hands every frame's snapshot, merged reading and arm state
  to `runSumAdd()`; the stop ends the summary as `STOP`, `DISARM`,
  `KST SERVO`, `HITEC SERVO` or `ESC SETTINGS`, and once the `.BFL` has
  closed clean writes the text, built in the drained write buffer, with
  `FA_CREATE_ALWAYS` (`src/sd_log_summary.*`). A failed write during the
  log ends the summary there, not saved, and no `.TXT` is tried. A `.TXT`
  that cannot be saved, on a full card or after a failed write, leaves the
  `.BFL` as it was and the logger `READY`, and is removed when the card
  still allows it. `.TXT` files do not move the next log's number.
  `SdLogStatus` carries the last summary for SD LOG, which does not show it
  yet; a mount and USB DRIVE forget it. `make -C test lognum` runs
  `src/sd_log.cpp` over FatFs R0.15 on a RAM disk (`test/sd_log_fatfs.cpp`,
  `test/ram_disk.*`). README and the wiki pages SD-Logging and
  SD-Aufzeichnung describe the file. Not tested on hardware.
- After v0.1.0-rc1: the run summary's core, slice 1 of
  [run-summary.md](docs/design/run-summary.md#slice-1-as-built), in
  `src/run_summary.*`. From each logged frame's telemetry it keeps the
  maximum eRPM, current and temperature over the whole log and the minimum
  voltage while armed, each as a raw extreme and as a held one, counted
  only when three consecutive new values reach it; the source per frame,
  KISS while fresh and EDT in its gaps, tagged `MIXED` when both counted,
  and EDT's own figure when every fresh KISS frame carried 0; the KISS
  counter's rise and the charge from current; the armed time. It builds the
  `LOGnnnnn.TXT` text, both clock lines `not set`, and SD LOG's row texts.
  Host tests in `test/test_run_summary.cpp`. The logger does not call it
  yet: no `.TXT` is written and SD LOG does not change. Not tested on
  hardware.
- After v0.1.0-rc1: the design note for the S.BUS output,
  [sbus-output.md](docs/design/sbus-output.md), designed and not built:
  S.BUS OUTPUT behind OUTPUT on RX SIGNALS sends a frame every 14 ms on GP0
  through UART0 with the pad's output inverted, with rcbench's encoder to be
  copied unchanged from b88593d3, 16 channels on sliders, the throttle
  channel starting at its stop and held under MAX THROTTLE, CH17, CH18 and
  the failsafe and frame-lost bits, HOLD TO START and the frame stops of the
  servo tester. It records, read from source, that AM32 and Bluejay take no
  throttle from an S.BUS line and that BLHeli_S may take it as Multishot,
  calibrate, arm and start its motor. By the owner's decision of 2026-11-15
  it is built only with a bootloader probe on GP0 before START, after the
  ESC SETTINGS read slices for Bluejay and BLHeli_S. Documentation only.
- After v0.1.0-rc1 (a93e1c1): the DShot command slot on core1, slice 1 of
  [esc-commands.md](docs/design/esc-commands.md#slice-1-as-built). The
  beacon's slot in `EscPump` becomes one sequence: beacons 1 to 5 as 6
  frames, then 260, 260, 280, 280 or 1020 ms of zero throttle; command 12
  alone; commands 7 to 10 with 12 after them, each a block of 10 frames
  followed by 300 ms of zero throttle. Core0 asks with
  `escRequestCommand()`, reads `escCommandReady()` and `escCommandState()`
  and drops a waiting request with `escCancelCommand()`. A sequence starts
  only disarmed, with an eRPM answer younger than 500 ms, eRPM 0, the ESC
  answering for 1500 ms (`ESC_CMD_SETTLE_MS`) and no EDT enable or other
  sequence on the wire; a request that cannot start within 2000 ms
  (`ESC_CMD_WAIT_MS`) expires. Frames 1 to 6 of each block of 7 to 10 and 12
  need an answer, eRPM or EDT: one of frames 1 to 5 may go unanswered, a
  second or none to frame 6 stops the block, reported as not sent before
  frame 6 of the first block and as a save not sent after it. An arm drops
  the sequence; no scheduled EDT enable goes out inside one. The rules are
  pure in `src/esc_task.h`, among them `escFrameWire()`, which sets the
  telemetry bit on every command frame. Host tests in
  `test/test_esc_cmd.cpp` run the slot against AM32, Bluejay and BLHeli_S
  models read from their sources. No page sends commands yet; MENU's BEEP
  THE ESC is the only caller. Not tested on hardware.
- After v0.1.0-rc1 (a93e1c1): a Bluejay or BLHeli_S ESC that answers the
  greeting is recognised, slice 2 of
  [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-2-as-built).
  `src/bl_answer.*` classifies the answer: an AM32 family, the EFM8
  bootloader of Bluejay and BLHeli_S by its signature E8B1, E8B2 or E8B5,
  or neither; `am32BlAnswer()` returns it. The connect page reads
  `EFM8 E8B2 ANSWERED: NOT READ YET.`; nothing is read. The answer sets
  MENU's run cue and gives the pin claim a leave page
  (`pinClaimWantLeavePage()`), so BACK opens the leave page of the note's
  decision 8, `UNPLUG THE ESC BATTERY, THEN LEAVE.` and
  `PLUG IT IN AFTER RESTART DSHOT.`, and DShot returns to SIG only from its
  RESTART DSHOT, which clears the cue. MENU's second row is the chooser
  `ESC CONFIG` (`src/ui_esc_config.*`), holding `ESC SETTINGS` and
  `USB BRIDGE`, the latter drawn disabled with `NOT BUILT`. Host tests in
  `test/test_am32.cpp`, `test/test_claim.cpp`, `test/test_ui.cpp`,
  `test/test_menu.cpp` and `test/test_ids.cpp`; the screenshots
  `esc-chooser`, `am32-efm8` and `am32-leave`. Not tested on hardware.
- After v0.1.0-rc1 (a93e1c1): S.BUS MONITOR, slice 3 of
  [rx-signals.md](docs/design/rx-signals.md#slice-3-as-built), opened from
  MENU → RX SIGNALS → MONITOR (`src/ui_sbus.*`). It claims the role that is
  GP1 for the S.BUS job and shows a receiver's link state on the strip's chip
  (`NO SIGNAL`, `NO FRAMES`, `FAILSAFE`, `FRAME LOST`, `OK`), its 16 channels
  as bars over the raw 11-bit value with the value in µs by rcbench's
  `sbus_to_us()`, CH17 and CH18, the mean frame interval and rate, and the
  frame, frame-lost, failsafe, parity, framing, break, overrun, resync and
  footer counts. The bars are lime while frames arrive, red in failsafe and
  dim once no frame has come for 100 ms. With SIG on GP1 the screen does not
  claim while an ESC answers there within 500 ms
  (`AN ESC ANSWERS ON SIG GP1.`); KISS frames on GP1, no role on GP1 and a
  refused UART clock have their own lines, the last with RETRY. BACK opens
  the leave page at `1 UNPLUG THE RECEIVER.`, with BACK TO MONITOR and
  RESTART DSHOT. What it shows comes from the pure `src/sbus_view.*`. RX
  SIGNALS (`src/ui_rx_signals.*`) is the chooser behind the MENU row, with
  MONITOR and OUTPUT, the S.BUS output, drawn disabled with `NOT BUILT`.
  Host tests in `test/test_sbus_ui.cpp` run both through MENU on the UART0
  fake; the preview `docs/sbus-preview.png` and the wiki pages RX-Signals and
  RX-Signale are new. Not tested on hardware.
- After v0.1.0-rc1: S.BUS MONITOR's wait message says when to plug the
  receiver in. With an ESC on SIG GP1 it reads `AN ESC ANSWERS ON SIG GP1.`
  `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` (a third line
  at y 108); the KISS message shares the second and third lines. Bidirectional
  DShot drives GP1 until the claim holds, and the chip shows only once the
  job runs. README, the wiki pages and
  [rx-signals.md](docs/design/rx-signals.md#slice-3-as-built) follow.
- After v0.1.0-rc1: the host suite fails a core1 job that never calls
  `critical_section_init()`. The fake critical section in `test/fakes.cpp`
  marks a section at init and counts each enter or exit on an unmarked one
  (`fakeCriticalSectionMisuse()`); `test/test_main.cpp` calls
  `core1JobInit()` before the first suite, as `src/main.cpp` does, and fails
  the run unless the count is 0. On the chip an uninitialised section uses a
  NULL spin lock pointer. `esc_task.cpp`'s init is not covered: that file is
  not linked into the host suite.
- The design note for the USB bridge,
  [usb-bridge.md](docs/design/usb-bridge.md), designed and not built: a
  desktop ESC tool reaches the ESC's bootloader on SIG through the board's
  USB serial port, by passthrough at 19200 baud with the wire's echo, ported
  from DshotDisplay's `feat/usb-bridge` at 6693084, or through MSP
  (MultiWii Serial Protocol) and the 4-way interface for ESC-Configurator.
  The owner answered its twelve questions on 2026-11-15.
- The design note for Bluejay and BLHeli_S settings,
  [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md), designed and not
  built: AM32 CONFIG's connect page recognises an EFM8 bootloader's answer
  and hands the ESC to rcbench's `protocols/escwire`, to be copied
  unchanged; the fields per firmware and layout, the backup before the
  first write, the read-back, and why a cut write followed by a restart of
  the ESC loses its 48 tag bytes. The owner answered its eight questions
  on 2026-11-15.
- After v0.1.0-rc1: rcbench's escwire core (`protocols/escwire/`), its
  RP2350 line driver `ew_line` and the servo file store
  (`shared/logfile/servo_store.*`), copied unchanged from rcbench commit
  2b4f934a into `lib/rcbench/`, with rcbench's 12 tests of them in
  `test/rcbench/` (210 cases) and `escwire_headers.cpp` built as C++11
  ([bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#slice-1-as-built),
  slice 1). `src/escwire.cmake` builds the core and the line driver into the
  firmware and `test/escwire.mk` the core and the store into the host suite.
  Nothing calls them yet. CI holds the escwire line fixture to pioasm's
  words, as it does the KST and Hitec fixtures.
- The design note for the angle meter,
  [angle-meter.md](docs/design/angle-meter.md), designed and not built:
  incidence, decalage, dihedral and control-surface throw in degrees and
  mm from the board's QMI8658 IMU (inertial measurement unit) on I2C1, with
  ZERO, a three-pose flip calibration and rcbench's `servo_rudder.c` and
  `imu.c`. The owner answered its seven questions on 2026-11-15.
- After v0.1.0-rc1: the owner's decisions of 2026-11-15 on the questions of
  five design notes (run summary, angle meter, ESC commands, USB bridge,
  Bluejay and BLHeli_S), with the eight-row MENU they give recorded in
  [menu-b.md](docs/design/menu-b.md#menu); documentation only.
- STEP models of two modules the bench is built with, drawn part by part with
  the print of both sides: `hardware/mechanical/models/ina219_cjmcu219.step`,
  the INA219 current sensor board, 24.2 × 21.9 mm, 225 solids, and
  `boost_5v_en.step`, the 5V boost converter board, 13.7 × 21.7 mm, 69
  solids. `gen_models.py` writes them from rcbench's specs, copied unchanged
  to `lib/rcbench/hardware/` at rcbench commit 1b6029b8 together with that
  commit's `step_gen.py`; sources and limits are in
  [hardware/mechanical/README.md](hardware/mechanical/README.md#modules).
- A STEP model of the board for enclosure design,
  `hardware/mechanical/models/rp2350_touch_lcd_2_8.step`: 50.54 × 73.06 ×
  10.03 mm, 1039 solids with flat and cylindrical faces. It is a facsimile
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
  channels. The S.BUS job below calls it.
- After v0.1.0-rc1 (a93e1c1): the S.BUS monitor's core1 job, slice 2 of
  [rx-signals.md](docs/design/rx-signals.md) (`src/sbus_job.*`,
  `src/rx.cmake`). While its pin claim holds, the job puts GP1 on UART0 with
  the pad's input inverted and the pull-down on, at 100,000 baud, 8 data bits,
  even parity and 2 stop bits; drains the receive FIFO on every pass of core1
  into rcbench's decoder; and publishes the last frame's 16 channels and flags,
  the frames decoded, the mean of the last 16 intervals between frames, the
  frame-lost and failsafe counts, and the parity, framing, break, overrun,
  resync and footer counts (`sbusJobStatus()`). A byte with a parity, framing
  or break error is counted and not decoded. A bind into a receiver already
  sending counts nothing: the bind empties the receive FIFO after setting the
  format, and nothing is counted or decoded until the line has been quiet for
  2000 µs. The job drives no pin: GP0, UART0's transmit pin, keeps its
  function. Its bind refuses when no role is GP1 (`CORE1_JOB_NO_GP1`, new in
  `src/core1_job.h`) or when UART0's rate is more than 1 % from 100,000 baud
  (`CORE1_JOB_CLOCK`); its release turns UART0 off, leaves GP1 an input with no
  inversion and the pull-down on, and clears the status, at once. Its pin user,
  `PinUser::Sbus`, asks for SIG and AUX in the default wiring, so a KISS wire
  live on GP1 refuses it and two servos cannot claim while it holds; it has no
  ESC check and shows its unplug page before DShot restarts. No screen opens it
  yet: the S.BUS MONITOR screen is slice 3, and MENU's RX SIGNALS row reads
  `NOT BUILT`. The firmware now builds the decoder. Not tested on hardware; the
  inversion on the pin is not measured.
- After v0.1.0-rc1 (a93e1c1): host tests of the S.BUS job,
  `test/test_sbus_job.cpp`, on a line model of a receiver, `test/sbus_line.h`,
  and a UART0 fake in `test/fakes.cpp`: a 32-entry receive FIFO with its
  overrun, the baud divisors of SDK 2.1.1 from a settable `clk_peri`
  (`fakeClockSet()`), a receiver that hears the line only with GP1 on the UART
  function and its input inverted and reads each character in the format set,
  8N1 after `uart_init()` for a set time (`fakeUartInitTakes()`), the wait of
  `uart_set_format()` on a running UART, and a character caught in its middle.
  The suite's clock counts microseconds (`fakeAdvanceUs()`), and the GPIO fake
  records the input override, which `gpio_set_function()` zeroes as the SDK
  does.
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

- After v0.1.0-rc1: the four choosers run on one widget, the hub
  (`src/ui_hub.*`), slice 1 of menu structure C, a proposed design note,
  `docs/design/menu-c.md` on the branch `ccr-menu-rcb`. PROGRAMMER, ESC
  CONFIG, RX SIGNALS and TOOLS are four tables of one entry type,
  `UiHubEntry`, in `src/ui_registry.cpp`: a label, two plain lines, the
  screen and its entry point, a gate with the two lines that say why, and
  the power line by SERVO SUPPLY. One build-time check, `hubEntriesFit()`,
  takes the place of the four: a label within 18 characters, a line within
  37, the lines under an entry ending 4 px or more above the next entry, and
  the last entry's on the panel. No screen changes: titles, strips, entries,
  lines, rectangles, control ids and taps are the same, and the 1381 frames
  the host suite draws on the four screens, 37 of them distinct, are equal
  byte for byte before and after, as are all 182 screenshots. A hub holds
  one page of three entries and does not scroll; the list of slice 4, MENU's
  new rows and the navigation of slice 2 are not built. `UiProgEntry`,
  `UiRxEntry`, `UiEscCfgEntry` and `UiToolsEntry` are gone, and the layout
  constants `UI_PROG_ENTRY_*`, `UI_PROG_LINE_*` and `UI_PROG_LABEL_CHARS` are
  `UI_HUB_ENTRY_*`, `UI_HUB_LINE_*` and `UI_HUB_LABEL_CHARS`. Host tests:
  `runHubTests` in `test/test_hub.cpp`, 161 checks; 63 mutations of the
  widget, the fit check, the tables and the four screens, 62 failing the
  suite or the build and one equivalent, the limit of three entries,
  which the room under a fourth entry also refuses.
- After v0.1.0-rc1: the release notes gain a "Hardware testing" section: no
  MiniRcBench image has run on hardware. They name the image by its tag and
  the ESC pin setting as SIG PIN. The README's "Prebuilt image" and the wiki's
  Home page say that no MiniRcBench image has run on hardware. The README and
  the KISS design notes name `pico-bidir-dshot` as the driver the owner's
  hardware reports cover; the Telemetry and Telemetrie pages call it the DShot
  driver of DshotDisplay v2.3. The README and the Telemetry pages state that
  the driver in `src/dshot/` has not run on hardware in MiniRcBench and that
  rcbench ran it at DShot600 on one AM32 ESC. The wiki footer says a
  pre-release publishes no manual.
- After v0.1.0-rc1: the documentation follows the code. STATUS.md's "Next
  steps" states what is open now: the tags
  (v0.1.0-rc1 exists, v0.1.0-rc2 is not tagged), the bench plan as the next
  work and what waits on the owner. The design index and the status lines of
  the design notes name the built slices, and the notes of the write cache
  and the backups name ESC SETTINGS in place of AM32 CONFIG. The write
  cache's note says that the swap check guards against a card that answers
  and that how a real card behaves is not verified. README's feature list
  describes the Bluejay and BLHeli_S write as built and names the servo log;
  the wiki's Home page, in English and German, names ESC COMMANDS, ESC
  SETTINGS for all three firmwares, S.BUS OUTPUT, SETTINGS with DIAGNOSTICS,
  the servo log and the run summary, with links. menu.md's tree and MENU show
  the eight rows and their live entries; the wiki's screens pages title the
  ESC SETTINGS section by its name. The unused `UI_ESC_CFG_NOT_BUILT` macro is
  removed, and the comments on MENU's row count and scroll range read eight
  rows and 78 px.
- After v0.1.0-rc1: the run's end after a servo log's open given up asks the
  write cache for a fresh swap check, then flushes the queued blocks, then
  reads the volume's serial number (`endGiveUp()` in `src/servo_log.cpp`,
  `cardCacheRecheck()`, `sdc_recheck()` in `src/sdio/sd_cache.c`). The swap
  check runs once a batch and stays passed until the queue empties, so a card
  swapped after it, with blocks still queued, would have taken them; now the
  next block waits for a new check and another card fails the flush, which
  drops the queue and mounts the card again, with no close and no removal. A
  serial number that does not read once no longer mounts the card again and
  drops the open's blocks: the file stays on the card with the size the open
  synced and no rows, closed when the read error leaves the card ready (a CRC
  or frame error). A fatal read error marks the card not ready and FatFs
  refuses the close; the host model does not cover that case. The card is
  mounted again only when the flush fails or the serial number differs. A swap
  inside a batch during a run is not covered
  ([sd-write-cache.md](docs/design/sd-write-cache.md#card-swap)). Host tests in
  `test/servo_log_fatfs.cpp` and `test/test_sd_cache.cpp`; the mutants and the
  survivors are in the slice 6 note.

- After v0.1.0-rc1: the S.BUS OUTPUT source test refuses, in the gate of HOLD
  TO START, the 11 alternative tokens `or`, `and`, `not`, `bitor`, `bitand`,
  `compl`, `xor`, `or_eq`, `and_eq`, `xor_eq` and `not_eq` and any assignment
  (an `=` that is not part of `==`, `!=`, `<=` or `>=`), as it refuses `|` and
  `?`, with a failing sample for each form. `test/test_bridge_proto.cpp` reads
  at 0xFFFF on a model BLHeli_S, which advances its address by the bytes read
  and programmed, with no SET ADDRESS sent; the advance is from the BLHeli_S
  and Bluejay bootloader source (`rd1` and `pro5` of `BLHeliBootLoad.inc`,
  checked by review against bitdump/BLHeli and bird-sanctuary/bluejay).

- After v0.1.0-rc1: the models, CAD files and drawings move from MIT to
  CC BY-NC-SA 4.0 (Creative Commons Attribution-NonCommercial-ShareAlike 4.0
  International): everything under `hardware/cad/`; in `hardware/mechanical/`
  the STEP models, the renders and the Python scripts that generate or trace
  them or check the models; the wiring drawing `docs/wiring.svg` (and its
  copy `wiki/img/wiring.svg`, made by `tools/make_previews.py`) with
  `tools/make_wiring.py`, which writes its identifier into the SVG's first
  line; and the copies of rcbench's model generators in
  `lib/rcbench/hardware/`, which stay unchanged. The README in
  `hardware/mechanical/` and `check_photo.py` stay MIT, as do the firmware,
  tests, other tools and documentation. The legal code is
  [hardware/LICENSE-CAD.txt](hardware/LICENSE-CAD.txt). The `.py` files gain
  `SPDX-License-Identifier: CC-BY-NC-SA-4.0`; the STEP files carry no line
  and the licence file covers them. `test/check_docs.py` checks the
  identifiers in every tracked file, the legal code by SHA-256 (Secure Hash
  Algorithm 256-bit) and that no file outside those paths carries the new
  licence.
- After v0.1.0-rc1: MENU has eight rows, `TOOLS` the sixth before SD LOG,
  by decision 1 of [angle-meter.md](docs/design/angle-meter.md#decisions)
  and the MENU of [menu-b.md](docs/design/menu-b.md#menu). It scrolls 78 px:
  SD LOG is whole after a drag of 34 px, SETTINGS after a drag of 76 px.
- After v0.1.0-rc1: the settings block is version 6, 44 bytes: version 5's
  fields and the angle meter's ALONG AXIS, SIGN, LENGTH and calibration
  (decision 4 of [angle-meter.md](docs/design/angle-meter.md#decisions)). A
  whole block of version 5, as of versions 2 to 4, is carried over, CURRENT
  SENSE with it and the angle fields at their defaults. HOLD DEFAULTS on
  SETTINGS keeps the angle fields, which SETTINGS does not show.
- After v0.1.0-rc1: a pin claim without the leave page restores DShot only
  once its core1 job has let the pin go, so DShot is never rebuilt on a pin
  a job's state machines still drive. Any call into the claim,
  `pinClaimState()` included, moves it on, so it needs no poll from the
  screen that ended it. AM32, which has no job, restores
  within BACK as before; the USB bridge, slice 1 of
  [usb-bridge.md](docs/design/usb-bridge.md#slice-1-as-built), is the first
  user with a job and no page.
- After v0.1.0-rc1 (a93e1c1): the servo tester switches the servo's supply
  with SERVO SUPPLY at `GP28` or `GP29`, slice 3 of
  [current-sense.md](docs/design/current-sense.md#slice-3-as-built) and
  decision 20 of [servo-tester.md](docs/design/servo-tester.md). Before, it
  never drove the enable pin and the strip read `GP28 SUPPLY STAYS OFF HERE.`
  The job drives the pin low from its bind, high at HOLD TO START and low at
  STOP, at the heartbeat trip and at BACK, which makes it an undriven input
  again after the last pulse; the pulses wait at level 0 until the INA219
  reads 4.5 V more than 18 ms, one conversion, after the switch-on, or
  500 ms without a sensor, and with the
  sensor and no 4.5 V by 500 ms the supply goes off
  (`SUPPLY NOT UP: NO 4.5 V IN 500 MS.`). A touch controller silent for
  250 ms (`touchAnsweredMs()`, new in `src/touch.h`) or a stuck I2C bus
  switches it off and latches `STALLED`; either, and CURRENT SENSE at INA219
  with the part not found, refuses HOLD TO START, and the dial says why.
  With the sensor a reading
  above 2.5 V 2000 ms after a switch-off shows `GP28 SUPPLY NOT OFF`. The
  strip's second line names the phase, `GP28 SUPPLY ON AT START`,
  `COMING UP` or `ON`; SAFE's notes warn of an ESC's throttle and BEC wire;
  SET shows `SUPPLY UP IN 25 MS`. With `OWN` nothing changes. The rules are
  pure (`servoPowerStep()` in `src/servo_job.*`); host tests in
  `test/test_servo.cpp`, mutation-checked. Part C of `docs/wiring.svg`
  names the servo tester; README, the wiki pages Servo-Tester, The-Screens,
  Wiring and Setup with their German pairs, and the design notes follow.
  Not tested on hardware.
- After v0.1.0-rc1 (a93e1c1): the tester's 3D note reads
  `3D MODE ESC: 0-50% REVERSED AT 2X` (33 characters, 197 px), in place of
  `3D MODE ESC: THROTTLE SCALE DIFFERS` (esc-commands.md decision 9). Its
  flag moves from `src/ui_am32.cpp` to `src/esc_3d.*` (`esc3dNote()`,
  `esc3dSetNote()`), which ESC SETTINGS and ESC COMMANDS both write.
- After v0.1.0-rc1 (a93e1c1): MENU's BEEP THE ESC, its 350 ms flash and
  `escRequestBeep()` are gone; ESC COMMANDS carries BEEP 1 to 5.
  `docs/menu-preview.png` shows MENU with no ESC answering in place of the
  flash. Rule 9 of `test/check_docs.py` fails when README.md or a wiki page
  names BEEP THE ESC or the old 3D note text, or an image `menu-beep*`.
- After v0.1.0-rc1 (a93e1c1): a command request that still waits when the
  DShot pump lets go of SIG, for a pin claim or a wiring change, is dropped
  (`escPumpTeardown()` in `src/esc_task.h`) and does not start on the next
  bind's ESC. It reads expired when no sequence was running; a sequence the
  teardown cut keeps its outcome. Host test in `test/test_esc_cmd.cpp`.
- After v0.1.0-rc1: the shared I2C1 bus is recovered with CURRENT SENSE at
  `NONE` too: `senseTick()` runs the recovery before its mode test, since
  touch and the IMU share the bus. `senseNote()` replaces `senseNoteTouch()`,
  takes the transfers of the touch driver and the IMU driver alike and
  returns the mapped result. A host test in `test/test_sense.cpp` runs the
  recovery at `NONE`.
- After v0.1.0-rc1 (a93e1c1): the wiring sheet draws the current sensor and
  the bench converter, slice 2 of
  [current-sense.md](docs/design/current-sense.md#slice-2-as-built). The
  connector table in `src/board_rp2350_touch_lcd_2_8.h` marks J3's I2C pins
  `^`, the INA219's connector, and J4's `*`, which `docs/wiring.svg` now
  labels "do not use". Part C of `docs/wiring.svg` shows a synchronous
  boost with output disconnect from one LiPo (lithium-polymer) cell of its
  own, with a fuse at the cell, and the optional INA219 on its output, VIN+
  from VOUT and VIN- to the servo's plus, its header on J3.
  `tools/make_wiring.py` reads the address and the shunt from `src/sense.h`,
  and its `--check` fails when the marker is on more than one connector, J3
  carries another pin, or the address is reserved or one of the board's
  own. The README's wiring table and switched supply, and the wiki pages
  Wiring and Setup with their German pairs, describe the boost, the sensor
  and J3; the converter module is not named and its ratings are not known.
  Not on hardware.
- After v0.1.0-rc1 (a93e1c1): MENU's BEEP THE ESC goes through the command
  slot (`escRequestBeep()` calls `escRequestCommand(n, false)`). It shows
  `REFUSED` while no ESC answers, while the motor turns and for 1500 ms after
  an ESC starts answering, where before it sent the beacon with no check of
  the link. A press while an EDT enable or an earlier beacon and its gap go
  out waits, at most 2000 ms. Host test in `test/test_menu.cpp`.
- After v0.1.0-rc1 (a93e1c1): AM32 CONFIG is `ESC SETTINGS`, opened from
  MENU → ESC CONFIG, by decision 1 of
  [bluejay-blheli-s.md](docs/design/bluejay-blheli-s.md#decisions) and its
  call A: its hex view and write page are titled `ESC HEX` and `ESC WRITE`,
  the list's strip names the firmware, `AM32 2.16` in place of `FW 2.16`,
  and SD LOG reads `RECORDING STOPPED BY ESC SETTINGS`. An AM32 session
  with no EFM8 answer leaves as before, DShot back within BACK. The
  greeting's comment in `src/am32_bl.cpp` says what F4 7D is: CRC-16/ARC of
  `BLHeli` alone.
- After v0.1.0-rc1 (a93e1c1): the leave page belongs to the pin claim, not
  only to its user: `PinUserInfo::unplug` sets it when a claim begins and
  `pinClaimWantLeavePage()` for the rest of one that began without it
  (`src/pin_claim.*`).
- After v0.1.0-rc1 (a93e1c1): MENU's RX SIGNALS row acts: it opens the RX
  SIGNALS chooser, has no status, and is drawn disabled while the pin claim
  is not free, as SERVO PROGRAMMER's row. `UI_RX_SIGNALS` moves from
  `src/ui_menu.cpp` to `src/ui_rx_signals.cpp`, and `UI_MENU_NOT_BUILT`
  becomes `UI_RX_NOT_BUILT`, OUTPUT's line. A chip on an amber or ink fill
  draws its label in `C_PAPER` (`uiChip()`).
- After v0.1.0-rc1 (a93e1c1): the documentation of menu structure B that no
  slice owned, slice 10 ([menu-b.md](docs/design/menu-b.md#slice-10-as-built));
  menu structure B is built, host tests only. menu.md's tree and MENU
  sections describe the seven rows, PROGRAMMER and its entries, RX SIGNALS
  drawn disabled, the strip with SIG, AUX and `DSHOT600`, and the rows drawn
  disabled under `DSHOT RESTARTING ON SIG GP0`. Revision lines that point to
  menu-b.md stand on menu.md decisions 1, 12 and 18, pin-claim.md
  decisions 3 and 4, kst-programming.md decision 18, servo-tester.md
  decisions 7, 8 and 14 and hitec-programming.md decision 6, and where
  menu.md named the unplug page, `PINS BUSY`, RESET and the old SETTINGS
  rows; rx-signals.md's MENU section reads as the record of 2026-11-13.
  README's "Tests" lists the 111 screens the suite renders, by screen, in
  place of "47 screens"; "Layout" names SERVO TESTER's views; "The user
  manual" says how the wiki's images are named. Rule 9 of
  `test/check_docs.py` fails when README.md or a wiki page names a screen
  text, view or MENU path that menu structure B removed, or an image of a
  removed screen, and when a `fakeDumpFrame()` call or
  `tools/make_previews.py` renders one. No firmware change.
- After v0.1.0-rc1 (a93e1c1): MENU's seven rows and PROGRAMMER, slice 9 of
  menu structure B ([menu-b.md](docs/design/menu-b.md#slice-9-as-built),
  owner decisions 1 and 2 and decisions 15 d, 16, 18, 19, 23 and 31 to 35).
  MENU's rows go by device: BEEP THE ESC, AM32 CONFIG, SERVO TESTER, SERVO
  PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS; SD LOG is whole at 0 px and
  SETTINGS is reached with a 36 px drag. The tool rows show no pin status,
  `GP0` or `GP0+1`: the strip names the pins. KST SERVO and HITEC SERVO
  leave MENU; SERVO PROGRAMMER opens PROGRAMMER (`src/ui_servo_prog.*`), a
  chooser of KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC from
  `UI_PROGRAMMERS` in `src/ui_registry.cpp`, with the models, the power line
  (`POWER THE SERVO UP AFTER THE TAP.` or `THE BENCH SWITCHES IT ON GP28.`)
  and, for HITEC CLASSIC without the switched supply, the amber lines that
  say why it is drawn disabled. The chooser claims nothing, and its tap
  claims SIG on that frame through `uiOpenTool()`. RX SIGNALS stands drawn
  disabled with `NOT BUILT` in every claim state and opens nothing. HITEC
  SERVO's family view, `HitecView::Family`, `HitecCtl::Classic` and
  `HitecCtl::DSeries` are gone; `uiHitecEnterD()` and
  `uiHitecEnterClassic()` replace `uiHitecEnter()`. A D series servo on its
  own supply is powered after the tap: HITEC SERVO stops in Held with
  `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.` over
  `POWER THE SERVO UP, THEN TAP READ.`, and READ (`HitecCtl::Read`) starts
  the job; with the switched supply, and for a classic servo, the job starts
  as soon as DShot has stopped. While an ESC answers, HITEC SERVO reads
  `AN ESC ANSWERS ON SIG GP0: UNPLUG IT.` on its main view, as KST SERVO
  does. NEXT SERVO on the leave page asks for the job as for the first
  servo: `pinClaimKeep()` returns a user that defers its job, HITEC, to
  Held with no job, so a D series servo on its own supply returns to the
  READ wait, and the page's note reads `NEXT SERVO: POWER IT AFTER THE
  TAP.` in place of `NEXT SERVO: POWER IT BEFORE THE TAP.` READ takes only
  a press that landed while it acted. Host tests in `test/test_menu.cpp`
  (`testSettingsRowVisible()`, `testRxSignalsRow()`, `testServoProg()`),
  `test/test_hitec_ui.cpp` (`testEntry()`), `test/test_kst_ui.cpp`,
  `test/test_ui.cpp`, `test/test_ids.cpp`, `test/test_claim.cpp`
  (`testDeferredJob()`) and the helpers `openChooser()`, `openProgrammer()`
  and `progEntryOf()` in `test/ui_ids.h`. The wiki shows `prog-chooser.png`,
  `prog-chooser-switched.png`, `hitec-read-wait.png` and `hitec-esc.png` in
  place of `hitec-family.png`, `hitec-family-esc.png` and
  `hitec-family-classic.png`.
- After v0.1.0-rc1 (a93e1c1): the leave page, slice 6 of menu structure B
  ([menu-b.md](docs/design/menu-b.md#slice-6-as-built), decisions 5 and 21
  and owner decisions 3, 25 and 30). KST SERVO, HITEC SERVO and SERVO TESTER
  leave through one page, `src/ui_leave.*`, in place of their leave and
  unplug pages. It numbers its steps, the current one amber with a 3 px bar:
  `1 POWER THE SERVO OFF, THEN LEAVE.` (KST, `OWN`),
  `1 LEAVE SWITCHES THE SUPPLY OFF.` (switched supply) or
  `1 LEAVE RESTARTS THE SERVO.` (Hitec, `OWN`), then `UNPLUG THE SERVO.` with
  `DSHOT RESTARTS ON SIG GP0 AND READS` / `AS A 970 US PULSE AT 1000 HZ.`,
  then `RESTART DSHOT.`; a page with no servo in programming mode, and SERVO
  TESTER's, opens at `1 UNPLUG THE SERVO.` STAY returns to the tool; LEAVE
  ends the claim as before; RESTART DSHOT takes LEAVE's rectangle, in place of
  the 160 × 40 px button at (40, 200), and acts on a press that lands 1000 ms
  or more after the pin is let go, so a double tap on LEAVE restarts nothing.
  NEXT SERVO (KST, Hitec) and BACK TO SERVO keep the claim, as BACK on the
  unplug page did. BACK on the page is drawn disabled in every phase and does
  nothing. HOLD TO REBOOT and READ AGAIN sit right of STAY, 112 px wide at
  scale 1, before LEAVE only. Notes: `2 CHANGES NOT WRITTEN.`,
  `THE SERVO IS IN DOUBT: RESTORE FIRST.` (KST) and
  `A WRITE DID NOT END VERIFIED.` (Hitec), amber, before LEAVE;
  `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`, never more than 2.0 s left, or
  `RELEASING THE PIN`, grey, while the pin goes (`RELEASING THE PINS` on
  SERVO TESTER with AUX); `NEXT SERVO: POWER IT AFTER THE TAP.` (KST) and
  `NEXT SERVO: POWER IT BEFORE THE TAP.` (Hitec), grey, once released with
  `OWN`. The strips name `SIG GP0`. `KstView::Unplug` and
  `HitecView::Unplug` are gone; `KstCtl::Stay`, `KstCtl::NextServo`,
  `HitecCtl::Stay`, `HitecCtl::NextServo` and `ServoCtl::BackToServo` are
  new. Host tests in `test/test_kst_ui.cpp` (`testLeaveAndBack()`,
  `leaveReleases()`), `test/test_hitec_ui.cpp`, `test/test_servo.cpp`
  (`testLeavePage()`), `testLeaveSupplyNote()` in `test/test_widgets.cpp`,
  and the probes `leaveStepIs()`, `leaveNoteIs()`, `leaveNoteBlank()`,
  `leaveWideIs()` and `leaveLeftIs()` in `test/ui_ids.h`. The wiki shows
  `leave-kst.png`, `leave-kst-pending.png`, `leave-kst-released.png`,
  `leave-kst-switched.png`, `leave-kst-supply-off.png`, `leave-hitec.png`,
  `leave-hitec-failed.png`, `leave-hitec-released.png` and `leave-servo.png`
  in place of `kst-leave.png`, `kst-leave-pending.png`,
  `kst-leave-switched.png`, `hitec-unplug.png` and `servo-unplug.png`.
- After v0.1.0-rc1 (a93e1c1): the tester of menu structure B, slice 7 of
  [menu-b.md](docs/design/menu-b.md#slice-7-as-built) (owner decision 4 and
  decisions 11 (the MAX row), 15 a to c, 17, 19 and 20). The pole count reads
  `14 POLES` in the text colour, in place of `14P` in grey. While the tester is
  disarmed and no other screen holds the signal pins, `14 POLES` and `MAX 20%`
  are underlined, and a tap on either opens SETTINGS; the tap disarms as MENU
  does, through `uiOpenTool()` in `src/ui.cpp`, and BACK on SETTINGS opened this
  way returns to the tester. Armed, or while DShot restarts after a servo tool,
  they show no underline and a tap on them does nothing; while the 3D note takes
  the row only `MAX` opens SETTINGS. The throttle latch `HOLD` reads `LATCH`
  (`TesterCtl::Latch`), and the track's caption with it on reads `LATCHED` in
  place of `THROTTLE REL`. A tap that leaves the tester for MENU or SETTINGS
  paints no tester region over the cleared frame. BOARD and the disabled HOLD TO
  ARM while a USB host holds the card, decision 15 b and c, were built before
  and are unchanged. Host tests `testTesterReadouts()` and `testSplashTouch()`
  in test_ui.cpp; `checkControlTable()` in `test/ui_ids.h` lets a readout lie
  over the pad or the track while the two do not both act. MENU and the readouts
  share one refusal for a busy pin claim, `uiPinsRefuse()` in `src/ui_screen.h`.
  A finger resting on a readout at power-up opens SETTINGS when it lifts, as one
  on MENU opens MENU.
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
- After v0.1.0-rc1 (a93e1c1): the write rule and the BACK guard, slice 5 of
  menu structure B ([menu-b.md](docs/design/menu-b.md#slice-5-as-built),
  decisions 6, 7 and 13's AM32 titles):
  - AM32 CONFIG, KST SERVO and HITEC SERVO write a device only from their
    write page. The list's button reads `WRITE...` and opens the page on a
    tap, in place of `HOLD TO WRITE` (and AM32's `NO CHANGES`); only the
    page's HOLD TO WRITE, 1 s, writes. FILES' `HOLD TO RESTORE` is
    `RESTORE...`, a tap that opens the restore page, whose HOLD TO RESTORE
    restores; KST's restore no longer starts on the FILES hold itself.
  - AM32 CONFIG gains `AM32 WRITE`: each change as label and `old -> new`,
    at most 4 and then `AND n MORE`, the count on the strip, a backup line
    (`BACKUP FIRST: BACKUP/AM001.TXT`, `BACKUP: BACKUP/AM001.TXT`, or in
    amber `NO CARD: OLD VALUES LOST ON LEAVING` and
    `USB DRIVE: OLD VALUES LOST ON LEAVING`), `WRITING: DO NOT UNPLUG.` and
    the result from y 196, a result that is not verified in amber. The
    editor bar names the value as read, `PWM FREQ  24KHZ ->`. The hex view
    is titled `AM32 HEX` and its BACK returns to the list.
  - BACK on AM32 CONFIG's list with edits not written, and on SERVO SET
    with a change waiting for HOLD TO APPLY, arms a guard: two amber lines,
    `2 CHANGES NOT WRITTEN.` / `BACK AGAIN DISCARDS THEM.` or
    `1 CHANGE NOT APPLIED.` / `BACK AGAIN DISCARDS IT.`; a second BACK
    drops the changes, any other press disarms it.
  - BACK on the KST, Hitec and AM32 write pages is drawn disabled and does
    nothing while the write runs or waits for the card's flush; the leave
    queued by BACK during a KST or Hitec write, and its
    `LEAVING AFTER THE WRITE ENDS.`, are gone.
  - AM32 WRITE writes what it shows: settings changed under the page are
    refused, `CHANGED SINCE THE PAGE: NOT WRITTEN`, and a page that lists no
    change draws HOLD TO WRITE disabled; no path through the screen reaches
    either.
  - The next steps under OK name the current steps: AM32's red BOARD
    voltage reads `CHARGE THE CELL. TAP OK, WRITE AGAIN.` in place of
    `CHARGE THE BOARD'S CELL, HOLD AGAIN.`; KST's and Hitec's failed
    flushes end `TAP OK, WRITE AGAIN.` and a card that took no file
    `CHECK THE CARD, TAP OK, WRITE AGAIN.`, in place of
    `CHECK THE CARD, HOLD AGAIN.`; KST's stalled screen in doubt reads
    `... LOAD KST003 IN FILES, TAP WRITE... AND HOLD.`
  `Am32Ctl::WriteHold`, `am32BackupNext()`; host tests
  `testAm32WritePage()`, `testAm32PageWritesWhatItShows()`,
  `testAm32Guard()` in `test/test_ui.cpp`, `testSetGuard()` in
  `test/test_servo.cpp`, `testFlushWaitEndsWithThePage()` and checks in
  `test/test_kst_ui.cpp` and `test/test_hitec_ui.cpp`; the wiki shows
  `am32-write.png`, `am32-discard.png` and `kst-restore.png`.
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

- After v0.1.0-rc1: the `KV EST` field no longer flickers between the figure
  and `--` when the eRPM replies are noisy. Once the filtered eRPM has been
  within 0.5 % of the last reply, the check holds until the throttle leaves
  its band of 20, another condition fails, the gap passes 3 %
  (`KV_EST_ERPM_HOLD_PERMILLE`) or the reply is 0, so a stall reads `--` on
  its first frame (`KvEstView::erpmHeld`). The claim that one eRPM step is
  0.20 % to 0.39 % held only up to 234,375 eRPM; above it a step is
  1 / period. Host-tested.
- After v0.1.0-rc1: MENU shows the run cue `POWER-CYCLE THE ESC TO RUN IT`
  after a USB BRIDGE bind that failed, or a BACK before the bind. The bridge
  set it only at the open, so DShot returned with no warning on an ESC that
  AM32 2.22 can put in its bootloader after about 2 s without a signal. The
  cue is set when the claim begins (`usbBridgeRunCueSet()`). Host-tested.
- After v0.1.0-rc1: USB BRIDGE's leave page keeps the note
  `FLASH THE ESC AGAIN BEFORE USE.` after `BRIDGE`. The bridge's open
  cleared the unfinished write, so after a write with no RUN, `BRIDGE` and
  `BACK` gave a page without the note while the ESC's flash was still cut.
  The unfinished write now lasts through the reopen, in passthrough and the
  4-way interface, and is cleared only by a RUN, `cmd_DeviceReset`'s among
  them, or by the page's RESTART DSHOT
  (`usbBridgeWriteUnfinishedClear()`), which ends the claim: the next entry
  to USB BRIDGE starts with none. A RUN counts once core1 has taken it from
  the frame slot, not at its handover: a RUN handed over shortly before BACK
  is never sent, and the note stays. A board reset or a power loss in the
  middle of a flash still restarts with DShot on SIG and no note.
  Host-tested, not on hardware.
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
