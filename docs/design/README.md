# Design notes

These documents record the design decisions behind each feature: protocol
details, pin choices, and what runs on which core. The document for a planned
feature describes the system as it will be; the document for a built feature
describes the code as it is. Release history is in
[CHANGELOG.md](../../CHANGELOG.md).

A document that no longer matches the code is a bug. When a feature changes,
update its document, or fold it into the README and delete it.
[STATUS.md](../../STATUS.md) holds the verification record for the whole
firmware; the status lines here summarise it per feature.

| Document | Status |
|---|---|
| [kiss-telemetry.md](kiss-telemetry.md) | Implemented. Working on hardware (owner report). Off by default. |
| [blackbox-logging.md](blackbox-logging.md) | Implemented. Hardware use recorded with the previous SDIO driver; none recorded with the current one. Buffer sizing unmeasured. |
| [sdio-driver.md](sdio-driver.md) | Implemented, not tested on hardware. MIT SDIO driver written from the SD specification; replaces the SD library's GPL-derived one. Host tests against a simulated card; the PIO programs in a cycle-level model. |
| [pin-claim.md](pin-claim.md) | Slices 1 and 2 built, host tests only, not tested on hardware: a screen takes the ESC and KISS pins from DShot while it is open, after a 250 ms zero-throttle drain; the AM32 editor and the servo tester run on it. Shared hardware facts and the slice plan. |
| [servo-tester.md](servo-tester.md) | Built, host tests only, not tested on hardware. Servo PWM (pulse-width modulation) output, manual position, sweep, two servos, on the pin claim. |
| [menu.md](menu.md) | Slices A to C built, host tests only, not tested on hardware: one list widget, `ui_list`, and one hold widget, `ui_hold`; a control table per screen that the host tests tap by id; MENU, a list of tools and actions from a registry, and SETTINGS, one list in sections; a 36 px header and BACK of 64 x 32 px. KST programming builds on them. |
| [kst-programming.md](kst-programming.md) | Steps 1 to 5 of 7 built, host tests only: rcbench's KST core, line driver and file store copied into `lib/rcbench/` with their tests; the search and the core1 job on them, tested against rcbench's simulated servo; the files on the card over FatFs, tested against an in-memory volume; the KST SERVO screen; the optional bench-switched servo supply, its enable on GP28 or GP29, with its card phase. The README and wiki pages of step 6 exist; step 7, the bring-up on hardware, is open. Depends on the pin claim and on the menu restructure, both built. |
| [backups.md](backups.md) | Built, host tests only, not tested on hardware. One `BACKUP/` directory and one ring of 50 numbers for the device backups of KST SERVO (`KSTnnn.TXT`), AM32 CONFIG (`AMnnn.TXT`) and Hitec (`HTnnn.TXT`), `BACKUP/LAST` naming the latest; KST's SAVE AS in `KST/` and Hitec's in `HITEC/`, at most 50 files each; the blackbox logs in `LOGS/`. The write cache runs during the three claims. |
| [sd-write-cache.md](sd-write-cache.md) | Built, host tests only, not tested on hardware. While the KST SERVO, AM32 CONFIG or HITEC SERVO claim holds, SD block writes go into a queue in SRAM and a flush step on core0 sends them in FatFs's order without waiting on busy; a 16-block read cache; a volume serial check before each batch. The device is written only once the flush ends clean. |
| [current-sense.md](current-sense.md) | Slice 1 built, the driver and the readout, host-tested; slices 2 to 6 not built; not tested on hardware; the owner's decisions of 2026-11-13 recorded. An INA219 at 0x40 on J3, the shared I2C1 bus, read by core0 after the touch read, on the output of one boost converter with output disconnect whose enable stays on GP28 or GP29. Six slices: the driver and readout, the wiring sheet, the servo tester on the switched supply, the overcurrent and stall cuts, move timing from current with rcbench's `servo_move.c`, and a CSV log. The owner's decisions of 2026-11-14: one LiPo cell into the boost, the 0.1 Ω shunt as sold, the servo tester always switching the supply, rcbench's limit run as the protection (1.0 A for 400 ms, 3.0 A hard; the hard cut acts at once in the KST and Hitec claims, during an operation too; the protection section to be redesigned), and settings carried over from versions 2 to 4 into 5 (built, host tests only). One open point: the converter module. |
| [hitec-programming.md](hitec-programming.md) | Steps 1 to 5 of 6 built, host tests only: rcbench's Hitec files and their tests in `lib/rcbench/`, the core1 job on them, the files on the card, the HITEC SERVO screen on MENU, CLASSIC enabled with SERVO SUPPLY at GP28 or GP29, and its README section and wiki pages; step 6, the bring-up, is open; the owner's decisions of 2026-10-23, 2026-10-28 and 2026-10-29 recorded. Programming Hitec classic and D series servos over SIG with rcbench's Hitec module at 3e29dc1b: session, register model, wire and PIO line driver. The named parameters are edited and written by plans with read-back, with a backup into the ring of `BACKUP/` before a session's first write, a record of each write and 1 s holds. No write plan of rcbench's has run on a servo; bring-up runs one write with read-back per family first. The classic entry uses the switched servo supply of KST step 5. Depends on the pin claim and on KST steps 4 and 5. |
| [diagnostics.md](diagnostics.md) | Built, host tests only, not tested on hardware. SETTINGS → DIAGNOSTICS: LAST RESTART, the restart reason decoded from the power manager's CHIP_RESET register and the SDK's two watchdog queries, and UPTIME. No watchdog is armed. A persistent fault log is not built now, by the owner's decision of 2026-11-14; its questions wait until the board has run on the bench. |
| [menu-b.md](menu-b.md) | Slices 1 to 4, 7 and 8 of 10 built, host tests only. Slice 1: the leave page widget, the BACK guard, two strip lines, the field row, the colour role names and the layout constants. Slice 2: the colours of decision 11's tables, a changed value cyan with a cyan bar on the field rows of AM32 CONFIG, KST SERVO, HITEC SERVO and SERVO SET, square 12 px chips, PROG MODE on blue. Slice 3: SIG PIN and AUX PIN with their connector lines, SERVO SUPPLY a core section of SETTINGS reading OWN, MENU's strip with DSHOT600 and its busy caption, DSHOT600 on the tester, NOTHING SAVED: DEFAULTS IN USE and four KST labels. Slice 4: decision 14's layout table, the footers, editor buttons, button edges and wide OK, HOLD DEFAULTS, the write pages' CHANGES and Hitec's strip. Slice 7: the tester's pole count and ceiling, underlined while disarmed, open SETTINGS, whose BACK returns to the tester, and the latch reads LATCH. Slice 8: decision 13's title on SERVO TESTER, its MENU label, on the main view and the unplug page. Menu structure B, chosen by the owner on 2026-11-13: six MENU rows with one SERVO PROGRAMMER row and its chooser, one leave page, SETTINGS from the tester's readouts, and rules for BACK, writes, unwritten edits, colours, pin names, layout and words. The owner answered the eight open questions on 2026-11-14; slices 5, 6, 9 and 10 wait on being built. Three of the owner's decisions of 2026-11-14 change the design, marked to be redesigned: each slice updates its own README and wiki lines, HITEC SERVO takes SIG on opening, and a seventh MENU row, RX SIGNALS, with which MENU scrolls 36 px, revising the six-row rule by the owner's decision of 2026-11-14. |
| [rx-signals.md](rx-signals.md) | Slice 1a built, host tests only: rcbench's S.BUS decoder and its test (12 cases) copied into `lib/rcbench/` and `test/rcbench/`; the rest planned. The owner's decisions of 2026-11-14: the S.BUS monitor and an S.BUS output, no PPM output; one MENU row, RX SIGNALS, with a chooser of MONITOR and OUTPUT. The monitor is designed: a receiver's S.BUS on GP1 through UART0 with the pad's input inverted, rcbench's decoder, 16 channels in µs with the bar in raw units, flags, frame rate and error counts. The S.BUS output, inverted 8E2 at 100,000 baud on GP0, is not designed; its encoder is written in rcbench first, and its throttle channel obeys MAX THROTTLE. The PPM output's design stays as a record, not built. |

## Shared context

KISS telemetry, the serial protocol of the KISS ESC (electronic speed
controller) line, sent on its own wire, supplies finer voltage and current
than EDT (Extended DShot Telemetry, carried in the replies of the DShot digital
ESC protocol). The log is the main use of that resolution: a voltage trace in
0.25 V steps hides a 200 mV sag.

MiniRcBench runs on one board, the Waveshare RP2350-Touch-LCD-2.8.

SD (Secure Digital) logging runs over four-bit PIO (the RP2350's programmable
input/output) SDIO (Secure Digital input/output) at 10 MHz, on the driver in
`src/sdio/` ([sdio-driver.md](sdio-driver.md)), which has no hardware record
yet. A USB (Universal Serial Bus) host can read the card as a read-only drive.

KISS telemetry is implemented. The owner reports it working on hardware, the
display's own telemetry request included; whether that report covers the
RP2350-Touch-LCD-2.8 is not recorded. The receiver is a PIO state machine, so
any free GPIO (general-purpose input/output) can carry the wire. The board
frees two GPIOs, GP0 and GP1, both fault tolerant to 5.5 V while the board
is powered. The ESC takes GP0 and KISS defaults to GP1, switched off until
the user turns it on. GP28 and GP29 carry only the optional switched servo
supply's enable.

What remains is measurement. BUF PEAK and WORST FLUSH under sustained logging
should size the ring buffer; 8 kB (`SD_LOG_BUFFER_BYTES`, 8192 bytes) is
unmeasured, and the SD LOG screen shows both numbers so they can be read off
the device. On the KISS side, the line voltage is not measured.
