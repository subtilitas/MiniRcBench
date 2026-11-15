# Run summary per log, dated by the board's clock

Status: designed; slices 1 and 2 of 6 built, host tests only: the core
([slice 1 as built](#slice-1-as-built)) and the logger feeding it and
writing the `.TXT` ([slice 2 as built](#slice-2-as-built)); SD LOG does not
show the run yet. The owner's choice of 2026-11-14 from the feature survey,
feature 4 of 6, built as a design note and then slices with reviews. The
owner answered its questions on 2026-11-15, see [decisions](#decisions);
slices 3 to 6 wait on being built.

Each blackbox log gets a run summary: the highest RPM, current and
temperature, the lowest voltage, the charge used and the armed time. SD LOG
shows it once the log stops, and the logger writes it as a small text file
beside the log, `LOGS/LOG00012.TXT` beside `LOGS/LOG00012.BFL`. As an
option, the board's PCF85063 real-time clock dates the logs, the summaries
and the device backups on the card. The owner's board has no cell in the RTC
socket J2, so its clock is set again after each power-up and files written
before the set have FAT date 0.

## Terms

ESC is electronic speed controller. DShot is the digital ESC protocol the
tester drives; bidirectional DShot carries eRPM (electrical revolutions per
minute) back on the same wire, and RPM is eRPM divided by half the motor's
pole count. EDT is Extended DShot Telemetry, voltage, current and
temperature frames sent between the eRPM frames. KISS telemetry is the
serial protocol of the KISS ESC line on its own wire. mAh is milliampere
hours. SD is Secure Digital. FAT is File Allocation Table, the file system
on the card; exFAT is its extended form. RTC is real-time clock, I2C the
two-wire Inter-Integrated Circuit bus, BCD binary-coded decimal, USB
Universal Serial Bus, CDC the USB communications device class (the serial
port), UI user interface, RAM random-access memory, UTC Coordinated
Universal Time, LiPo lithium polymer (the board's own cell). `.BFL` is the
blackbox log file of [blackbox-logging.md](blackbox-logging.md); `.TXT` is
the summary. logwiju is the browser-based viewer the blackbox note names
for `.BFL` files.

## Sources

- Code at `main` 33079ee: `src/sd_log.*`, `src/sd_log_next.*`,
  `src/ui_sdlog.*`, `src/esc_task.h`, `src/esc_merge.*`,
  `src/kiss_telem.h`, `src/dshot/dshot.h`, `src/ui.cpp`,
  `src/board_rp2350_touch_lcd_2_8.h`, `src/usb_dev.cpp`, `CMakeLists.txt`.
- The SD library, carlk3's `no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` at the pinned
  6c644cf (v3.6.2): `src/src/my_rtc.c`, `src/src/my_debug.c`,
  `src/include/ffconf.h`, and FatFs R0.15 `ff.c` in it.
- NXP, PCF85063A data sheet, Rev. 7.3, 14 July 2026, cited below by section.
  The board's part is the PCF85063ATL, the PCF85063A in the DFN2626-10
  package (data sheet, ordering information).
- Waveshare, RP2350-Touch-LCD-2.8 schematic, the file the board header
  names: its RTC block, read from the PDF rendered as an image.
- `hardware/mechanical/touch_lcd_2_8.py` and `_trace_print.py`: the rear's
  sockets and print.

Verified marks the facts read in these sources; "not verified" marks the
rest.

## What the tester has

Per value, the sources the firmware decodes today:

| Value | DShot | EDT | KISS |
|---|---|---|---|
| eRPM | every frame, 1 kHz; a period with a 9-bit mantissa, about 0.2 to 0.4 % steps | none | 100 eRPM steps, 50 Hz |
| Voltage | none | 0.25 V steps, 0 to 63.75 V | 0.01 V steps, 0 to 655.35 V |
| Current | none | 1 A steps, 0 to 255 A | 0.01 A steps, 0 to 655.35 A |
| Temperature | none | 1 °C steps, 0 to 255 °C | 1 °C steps, 0 to 255 °C |
| Consumption | none | none | 1 mAh steps, a 16-bit counter since the ESC powered on |

Verified in `src/dshot/dshot.h` (`DSHOT_EDT_MV_PER_COUNT` 250,
`DSHOT_EDT_A_PER_COUNT` 1, eight-bit payloads), `src/kiss_telem.h` (the
frame table), `escFoldReply()` in `src/esc_task.h`, and `src/config.h`
(`KISS_REQUEST_EVERY_N` 20: a KISS request every 20th frame). The eRPM
resolution is the blackbox note's.
How often an ESC sends each EDT frame is the ESC's choice and not known.

What the bench has on a given day:

- **EDT only**, the common case: bidirectional DShot and an ESC that
  answers the EDT enable. eRPM, voltage in 0.25 V steps, current in 1 A
  steps, temperature; no consumption counter.
- **EDT and KISS**: the third wire on AUX and SETTINGS → KISS TELEM on
  (off by default). While a KISS frame is fresh, the tester's readout takes
  voltage, current and temperature all three from KISS: `escMerge()`
  chooses the source per frame, not per field (`src/esc_merge.cpp`,
  verified). An ESC whose KISS frame carries 0 for a sensor it lacks
  therefore hides that EDT value on the tester.
- **eRPM only**: an ESC that does not answer the EDT enable. RPM is
  known, nothing else.
- **Nothing**: no ESC answering. A log started by hand still runs.

Which ESC firmwares send current in EDT, and whether an ESC without a
sensor sends 0 in its KISS frame, are not verified.

A damaged reply can pass the DShot reply's 4-bit checksum. Decoded, it can
read anything the format holds: a period of 1 µs is 60,000,000 eRPM
(`erpm_from_payload()`, `src/dshot/dshot_telem.c`), an EDT current 255 A,
an EDT temperature 255 °C. KISS frames carry a CRC-8 (cyclic redundancy
check), which lets fewer damaged frames through.

## The values

The logger computes the summary from the same snapshot that fills each
frame (`escSnapshot()` in `sdLogTick()`), and keeps the sources apart where
a KISS field can hide an EDT one, as the log keeps its voltage and current
columns per source. So each number can be checked against the log in
logwiju. A field counts
only while it is fresh by the firmware's existing windows: KISS for
500 ms (`KISS_STALE_MS`), an EDT field for 1000 ms (`EDT_STALE_MS`), DShot
eRPM for 500 ms (`ESC_LINK_STALE_MS`).

Each value but the minimum voltage covers the whole log, from START or
arming to the stop (decision 3). The minimum voltage counts only
while the tester is armed, whatever decision 3 says: before arming and
after disarming it would record the ESC powering up or browning out as
the pack is plugged or pulled, not the sag under load.

**Source per frame, and one fallback at the stop.** For voltage, current
and temperature the summary keeps two figures. The per-frame figure takes
KISS's field while a KISS frame is fresh and EDT's while it is not, as the
tester's readout and the current integral do; so a KISS gap over the
hardest part of a run leaves EDT's peak in it. It is reported tagged
`KISS`, or `MIXED` when EDT filled a gap. The EDT-only figure counts EDT's
field whenever it is fresh, and is reported only when KISS's field stayed
0 in every fresh KISS frame of the log: an ESC without that sensor, whose
zeros would otherwise hide EDT's value, as they do on the tester. Without
a fresh value of either kind the value is unknown. Both figures keep a
held and a raw extreme. The KISS counter has the same zero rule.

**Held values.** A maximum or minimum counts a value only when it holds
for three consecutive new values of its field: the summary keeps the last
three, and the held maximum is the highest of their lowest, the held
minimum the lowest of their highest. A new value is one whose arrival
time (`lastRpmMs`, `kissLastMs`, `edtVoltsMs` and the other EDT stamps of
`EscTelemetry`) differs from the last sample's. One damaged frame then
sets no maximum. From the first to the third value is 4 ms of DShot eRPM
at the logger's 500 Hz, 40 ms of KISS at 50 Hz, and an unknown time of
EDT, which the ESC paces. The raw extreme, every value counted, is kept as
well and written to the file beside the held one, as it is the one a
viewer finds in the `.BFL`; the raw minimum voltage, like the held one,
counts only while armed. The held value is the one SD LOG shows (decision 8).

| Value | From | Rule | Unknown when |
|---|---|---|---|
| Max RPM | DShot eRPM; KISS eRPM only when no DShot eRPM was fresh in the whole log | the held maximum eRPM, divided by half the MOTOR POLES value in force at the stop | neither eRPM was fresh in three consecutive new values |
| Max current | per frame, KISS while fresh, else EDT; EDT alone when KISS's current stayed 0 | the held maximum | no current was fresh |
| Max temperature | per frame, KISS while fresh, else EDT; EDT alone when KISS's temperature stayed 0 | the held maximum | no temperature was fresh |
| Min voltage | per frame, KISS while fresh, else EDT; EDT alone when KISS's voltage stayed 0 | the held minimum above 0.00 V, armed only | no voltage above 0.00 V was fresh while armed, or the tester was never armed |
| Used, ESC counter | KISS consumption | the rise of the counter over the log | no KISS frame was fresh |
| Used, from current | KISS current while KISS is fresh, else EDT current; EDT alone when KISS's current stays 0 | the integral of current over time | no current was fresh |
| Armed | the tester's arm state | the time armed within the log | never: 0 s is a value |
| Duration | the logger's clock | stop minus start | never |

Details:

- **RPM.** The eRPM is kept, not the RPM, and divided at the stop: a log
  started by hand survives MENU, so MOTOR POLES may change during it. The
  file names the pole count used. The logger samples eRPM at 500 Hz, every
  second DShot frame, so a peak shorter than the hold is not counted. The
  `.BFL` logs the raw eRPM field even when it is stale (verified,
  `sdLogTick()`); the summary does not count a stale one.
- **Voltage of 0.00 V.** A pack at 0 V powers no ESC, so a reading of
  0.00 V is taken as no reading in the minimum.
- **Temperature.** The tester draws ESC TEMP red at 90 °C and above
  (`drawTelemetry()` in `src/ui.cpp`); the summary uses the same rule.
- **Used, ESC counter.** The KISS counter runs from the ESC's power-on
  (`EscTelemetry::kissMah`), so the summary takes its rise during the log:
  the sum of each increase between fresh readings. A decrease is an ESC
  that restarted; the counting goes on from the new value. A counter that
  stays at 0 for the whole log is reported unknown, as an ESC that does
  not count; SD LOG then shows the figure from current (see
  [display](#on-the-device)). The counter has no hold: one damaged KISS frame
  that passes the CRC-8 with a wrong count adds its jump to the rise, and a
  falsely low one reads as a restart after which the next good frame adds the
  difference back; either can add up to 65535 mAh. Not measured.
- **Used, from current.** Each logged frame adds its current times the time to
  the next frame: current in 0.01 A units times microseconds, in a 64-bit sum;
  1 mAh is 3.6 × 10^8 of those units. The time between two frames is measured
  (`micros()`), not assumed to be 2 ms: a core0 stall stretches it. A sample's
  current holds for at most its source's stale window, 500 ms for KISS and
  1000 ms for EDT; longer gaps, and frames with no current, count as time
  without current, which the file states. A second sum takes EDT current
  alone; it is the one reported when KISS's current stays 0 for the whole log.
  With EDT's 1 A steps the step alone can be 1 A off for the whole run: 28 mAh
  per 100 s. Whether ESCs round or truncate the EDT current is not verified.
  One damaged EDT current frame holds its value until the next EDT current
  frame, at most 1000 ms: a false 255 A adds up to about 71 mAh. Not
  measured.
- **Armed.** The time within the log during which the tester was armed,
  summed per frame from `sdLogSetArmed()`'s state. `uiTick()` reports the
  arm state once per 25 ms frame, so each arm or disarm edge is good to
  25 ms. A log that arming started and disarming stopped is armed for
  almost its whole duration; a log started by hand counts only the armed
  part.
- **Duration and sums** run on the 64-bit microsecond timer
  (`time_us_64()`): the 32-bit `micros()` wraps after 71.6 minutes.
- **The end.** The summary ends where the log ends: when `sdLogStop()`
  begins, or when a write fails during the log. A failed write ends the
  log in `sdLogFlush()` (`enterError()`, then `f_close()`, verified), and
  `sdLogStop()` then returns at once, as the state is no longer
  recording; so `enterError()` closes the summary when a log was running,
  with the stop time, Duration and Armed up to the failure. `enterError()`
  also runs on a start that fails (`sdLogStart()`, verified); that start
  opens no log and keeps the last log's summary, as it keeps the FILE
  row's number.

## Where the statistics are kept

On core0, in the logger. `sdLogTick()` already takes the snapshot and the
merged reading 500 times a second; it hands both, with the arm state and
the time, as one sample to a pure update function, `runSumAdd()` in
`src/run_summary.*`, before it encodes the frame. A frame the ring then
drops still counts in the summary.

Core1 is not used. It would see every eRPM frame instead of every second
one, but its job is the 1 ms DShot slot and it never blocks; a second
writer to shared state would need the critical section on every frame, and
the summary would still have to reach core0 to be written. The gain, one
frame of eRPM, does not pay for that.

RAM: the running summary as `sizeof(RunSum)` gives it for the RP2350
(arm-none-eabi-gcc 13.3); the clock estimated from its fields, not
measured:

| Item | Bytes |
|---|---:|
| the running summary: for voltage, current and temperature a per-frame and an EDT-only figure, each with its raw and held extreme, the last three new values and their arrival times; the eRPM figure likewise; KISS zero flags and the gap-filled flags for `MIXED`; the KISS counter's last value and rise; two 64-bit current sums; armed, known-current and total time in 64-bit µs; start and stop clock readings | 464 |
| the text of the file | 0: built in the logger's ring, `s_ringBuf`, 8192 bytes |
| the clock: the reading at boot, its timer value, a flag | 16 |

The ring is free when the text is built. The stop drains it with up to 64
flushes; draining 8192 bytes takes at most 17, 16 whole chunks and at most
one short flush where the ring wraps. Slice 2 checks `logRingUsed()` is 0
before it builds the text. The text is written only after the `.BFL` has
closed clean; `sdLogTick()` writes
nothing while no log runs, and the next `sdLogStart()` resets the ring
(`logRingReset()`) before its first frame. All verified in
`src/sd_log.cpp`.

The summary of the last log stays in RAM for SD LOG until the next start
or mount, as the FILE row's number does today.

## The summary file

### Name and place

`LOGS/LOGnnnnn.TXT`, the number of its `.BFL`. `sdLogNameNumber()` counts
only names ending in `.BFL` (verified, `src/sd_log.h`), so the files do not
move the next log's number. The directory then holds two entries a log, and
the first start on a mount reads twice as many: about 2N / 16 sector reads
for N logs ([blackbox-logging.md](blackbox-logging.md#lifecycle)).

The file is opened with `FA_CREATE_ALWAYS`: a `.TXT` of that number with no
`.BFL`, left on a card whose log was deleted on a computer, is replaced.

### When it is written

In `sdLogStop()`, after the `.BFL`'s sync and close have succeeded
(the truncate before them is done and its result ignored, verified),
before the state returns to READY. Every clean stop writes one:
STOP on SD LOG, the disarm that stops an auto log, and a tool's claim
(`sdLogStopFor()`, which runs before `cardCacheBegin()` turns the write
cache on, verified in `src/card_cache.cpp`). The write is synchronous on
core0, as the stop's own sync and close are.

Every stop happens with the motor disarmed: STOP is on SD LOG, reached
through MENU, which disarms; the auto stop is the disarm; a claim is opened
from MENU. So the time the card takes delays the UI, not a running motor.
That time is not measured: an open, one cluster of data, a close, each
block bounded by the driver's 1 s busy timeout (`SDH_BUSY_TIMEOUT_US`).

### A full card or a card error

| How the log ended | The file |
|---|---|
| STOP, disarm, a tool | written; SD LOG shows its name |
| CARD FULL during the log (`SdLogEnd::CardFull`) | not tried, the card has failed the log; SD LOG shows the summary up to the failure, as not saved |
| a write that failed during the log (`SdLogEnd::WriteFailed`) | not tried; the summary up to the failure, not saved |
| the stop's sync or close failed (`SdLogEnd::WriteFailed`) | not tried; the summary up to the stop, not saved |
| the file's open, write or close fails | the logger stays READY, the `.BFL` is closed and whole; a short write is closed and, when the card still accepts it, removed (`f_unlink()`), so no cut summary stays on the card; not saved |

A failed summary does not set CARD ERROR: the log itself ended clean. The
next START finds out whether the card still works.

### Format

Plain ASCII (American Standard Code for Information Interchange) text, one
value a line, `Name: value unit`, line feeds as in the device backups
(`src/am32_backup.cpp`). Decimals follow the source's step: EDT current in
whole amperes, KISS current and both voltages in hundredths.

```text
MiniRcBench run summary, format 1
Log: LOG00012.BFL
Firmware: 0.1.0
Clock at start: 2026-11-14 15:42:07
Clock at stop: 2026-11-14 15:44:31
Ended by: STOP
Duration: 144.2 s
Armed: 102.5 s
Max RPM: 18450 RPM (DShot, eRPM 129150, 14 poles; raw 18460 RPM)
Max current: 23.41 A (KISS, 0.01 A steps; raw 23.55 A)
Max temperature: 51 C (KISS, 1 C steps; raw 51 C)
Min voltage: 14.82 V (KISS, 0.01 V steps, while armed; raw 14.79 V)
Used, ESC counter: 312 mAh (KISS, 1 mAh steps)
Used, from current: 305 mAh (KISS, current known for 144.0 s of 144.2 s)
Frames: 72100
Dropped frames: 0 (0 bytes)
Written: 1043 kB
Buffer peak: 2048 of 8192 bytes
Worst flush: 12 ms
```

A log started by hand with START, armed for part of it, and stopped with
STOP after MENU had disarmed. A log that arming started and disarming
stopped reads `Ended by: DISARM`, its Armed close to its Duration.

The lines in other cases:

| Case | Line |
|---|---|
| clock not set, and every log before slice 4 | `Clock at start: not set`, `Clock at stop: not set` |
| a set during the log | `Clock at stop: 2026-11-14 15:44:31 (clock set during the log)` |
| ended by disarming or a tool | `Ended by: DISARM`, `KST SERVO`, `HITEC SERVO`, `AM32 CONFIG` |
| RPM from KISS | `Max RPM: 18400 RPM (KISS, eRPM 128800 in 100 eRPM steps, 14 poles; raw 18400 RPM)` |
| EDT current | `Max current: 23 A (EDT, 1 A steps; raw 25 A)` |
| EDT voltage | `Min voltage: 14.75 V (EDT, 0.25 V steps, while armed; raw 14.50 V)` |
| KISS current stayed 0 | `Max current: 23 A (EDT, 1 A steps, KISS sent 0; raw 25 A)` |
| a KISS gap filled by EDT | `Max current: 31 A (KISS and EDT, 0.01 A and 1 A steps, peak from EDT; raw 32 A)` |
| never armed | `Min voltage: unknown (never armed)` |
| no eRPM | `Max RPM: unknown (no eRPM from DShot or KISS)` |
| no current | `Max current: unknown (no current from KISS or EDT)` |
| no temperature | `Max temperature: unknown (no temperature from KISS or EDT)` |
| no voltage | `Min voltage: unknown (no voltage from KISS or EDT)` |
| no KISS | `Used, ESC counter: unknown (no KISS frame)` |
| counter stayed 0 | `Used, ESC counter: unknown (KISS counter stayed 0)` |
| current from both | `Used, from current: 305 mAh (KISS and EDT, current known for ...)` |

`C` stands for degrees Celsius, as ASCII has no degree sign. The firmware
version is `FW_VERSION_STRING`. Frames to worst flush are the logger's
counters of `SdLogStatus`, so the bring-up numbers of
[blackbox-logging.md](blackbox-logging.md#bring-up) stay on the card after
SD LOG shows the run. The longest file, every line at its widest, is under
1000 bytes: one cluster on any card.

The clock lines are the clock's reading at the start and at the stop. A
set during the log moves the clock but not the timer, so the two lines
then differ from Duration, and the stop line says so. Slices 1 to 3 write
both lines as `not set`, so the format number stays 1 when slice 4 fills
them.

The text is built by a pure function, `runSumText()`, so the host suite
checks it byte for byte.

## On the device

SD LOG shows the run. Today its nine rows are STATUS, FILE, FRAMES,
WRITTEN, DROPPED FRAMES, BUF PEAK, WORST FLUSH, CARD and MOUNT, 19 px
each from y 44 (`LOG_ROW0_Y`), the label from x 8, the value to a right
edge at x 232, both scale 1 (`drawLogRow()`, verified). After a stop the
FILE row reads `LOG00012.BFL (LAST)`.

The design: while no log runs and the last log has a summary, rows 2 to 8
show the run; while a log runs, and whenever there is no summary (after a
mount, with no card), they show the counters as today. After a stop,
FRAMES, WRITTEN, DROPPED FRAMES, BUF PEAK and WORST FLUSH are in the
log's file; CARD and MOUNT are hidden until the next START or mount. They
matter when no card mounts, and then no summary shows (decision 1).
After a log a card error ended, the rows show the run up to the failure,
`SUMMARY` reads `NOT SAVED`, and the note lines say how it ended.

| Row | Label | Value, widest | Tag |
|---|---|---|---|
| 0 | `STATUS` | unchanged | |
| 1 | `FILE` | unchanged, `LOG00012.BFL (LAST)` | |
| 2 | `MAX RPM` | `60000000` | `KISS` when from KISS, none from DShot |
| 3 | `MAX CURRENT` | `655.35A`, from EDT `255A` | `KISS`, `EDT` or `MIXED` |
| 4 | `MAX TEMP` | ``255`C`` | `KISS`, `EDT` or `MIXED` |
| 5 | `MIN VOLTAGE` | `655.35V` | `KISS`, `EDT` or `MIXED` |
| 6 | `USED` or `USED FROM CURRENT` | `65535mAh`, `>99999mAh` | `KISS`, `EDT` or `MIXED` |
| 7 | `ARMED` | `999:59:59` | |
| 8 | `SUMMARY` | `LOG99999.TXT`, `NOT SAVED: CARD FULL`, `NOT SAVED: WRITE FAILED` | |

- Values follow the tester's tiles (`drawTelemetry()`): `14.82V`,
  `23.41A`, ``51`C`` (the font draws a backtick as a degree sign), `312mAh`.
  EDT current is in whole amperes, `23A`, as in the file, where the tester
  draws `23.00A`; voltage has two decimals from both sources. A `MIXED`
  value is drawn as the source of the sample that set it draws it. The tag
  says which step applies. An unknown value is `--`.
- The held values show; the raw ones are in the file only.
- `USED` shows the KISS counter's rise when it rose; otherwise the figure
  from current, labelled `USED FROM CURRENT` (decision 4); otherwise
  `--`.
- `USED FROM CURRENT` above 99,999 mAh reads `>99999mAh`; the file has
  the full figure.
- `ARMED` is `H:MM:SS`, as UPTIME (`diagUptimeText()`).
- A tagged value's right edge is x 197, its last pixel x 196; its tag
  starts at x 203 and its last pixel is at most x 231 (`MIXED`, the
  widest, 5 characters, 29 px). An untagged value keeps today's right
  edge, x 232 (`GFX_W - UI_MARGIN`).
- Colours by decision 11 of [menu-b.md](menu-b.md): values `C_TEXT`; the
  KISS tag `C_TEXT`, the EDT and MIXED tags `C_DIM`; `--` `C_DIM`;
  `MAX TEMP` at 90 °C and above `C_RED`, as the tester's tile; `NOT SAVED`
  `C_RED`, the role "save failed"; the file's name `C_TEXT`.

Fits, at (6 × characters − 1) × scale px, scale 1. A label's first pixel
is at x 8; a value's right edge is x 197 with a tag, x 232 without:

| Label | Its last pixel | Widest value | Its first pixel | Gap, px |
|---|---:|---|---:|---:|
| `MAX RPM`, 41 px | 48 | `60000000`, 47 px | 150 | 101 |
| `MAX CURRENT`, 65 px | 72 | `655.35A`, 41 px | 156 | 83 |
| `MAX TEMP`, 47 px | 54 | ``255`C``, 29 px | 168 | 113 |
| `MIN VOLTAGE`, 65 px | 72 | `655.35V`, 41 px | 156 | 83 |
| `USED FROM CURRENT`, 101 px | 108 | `>99999mAh`, 53 px | 144 | 35 |
| `ARMED`, 29 px | 36 | `999:59:59`, 53 px | 179 | 142 |
| `SUMMARY`, 41 px | 48 | `NOT SAVED: WRITE FAILED`, 137 px | 95 | 46 |

The note lines above START (y 217 and 228) and their texts do not change:
`RECORDING STOPPED: CARD FULL` and the others still say how the log ended.
The MENU status of SD LOG does not change.

After an auto log stops, the user is on the tester, not on SD LOG. Whether
the tester shows a line of the run is decided: it does not (decision 2),
and SD LOG is the only place the run shows.

## The clock

### The part

The PCF85063ATL at I2C address 0x51 on I2C1, GP6 and GP7, the bus of the
CST328 touch controller (0x1A), the QMI8658 IMU, inertial measurement unit
(0x6B, unused), and the optional INA219 (0x40); its INT on GP5, unused
(`src/board_rp2350_touch_lcd_2_8.h`, verified). No driver exists. From the
data sheet, verified:

| Address | Register | Used for |
|---|---|---|
| 00h | Control_1: EXT_TEST (7), STOP (5), SR (4), CIE (2), 12_24 (1), CAP_SEL (0) | the software reset; STOP while setting; written 00h after a set: normal mode, 24-hour mode, CAP_SEL = 0, 7 pF |
| 01h | Control_2: AIE, AF, MI, HMI, TF, COF[2:0] | written 07h: COF = 111, CLKOUT off, no interrupts |
| 04h | Seconds: OS (7), seconds 0 to 59 in BCD | OS = 1 means the clock's integrity is not guaranteed |
| 05h | Minutes, 0 to 59 in BCD | |
| 06h | Hours, 0 to 23 in BCD in 24-hour mode | |
| 07h | Days, 1 to 31 in BCD | |
| 08h | Weekdays, 0 to 6 | written from the date, not read |
| 09h | Months, 1 to 12 in BCD | |
| 0Ah | Years, 0 to 99 in BCD | 2000 to 2099 |

Further data sheet facts the driver relies on, verified:

- Address 1010001, A2h to write, A3h to read (8.5.1, table 38; 7.4).
- Reset values: 00:00:00 on 1 January 2000, OS = 1, STOP = 0, 12_24 = 0,
  CAP_SEL = 0, 7 pF (table 7, table 5).
- The OS flag is set while the oscillator is stopped, from power-on until
  the crystal runs, 200 ms to 2 s; it stays set until software clears it
  (7.3.1.1).
- Time registers 04h to 0Ah are read and written in one access each,
  completed within 1 s; the recommended read sets the pointer to 04h and
  reads seven bytes (7.4). The register address increments after each
  byte (sections 1 and 7), so one read from 00h covers 00h to 0Ah.
- CLKOUT puts out 32,768 Hz from power-on; COF = 111 turns it off
  (7.2.2.4). The supply current of section 12 is given with CLKOUT off.
- With STOP set the time does not count; after STOP is cleared the first
  increment comes 0.507813 s to 0.507935 s later (7.2.1.2).
- A year divisible by 4, 00 included, is a leap year (table 22's note),
  right for 2000 to 2099.
- The registers are certain to reset only when VDD starts from 0 V;
  otherwise the data sheet requires the software reset, 58h written to
  Control_1, once the supply is stable (7.2, 7.2.1.3), and recommends a
  STOP and START on the bus after it (figure 8). It sets every register to
  its reset value, the time included.
- One supply pin, VDD, 0.9 V to 5.5 V for the clock and 1.8 V to 5.5 V for
  the bus; no battery-switchover pin (section 2, table 3).

### The board's backup

From the schematic's RTC block, verified: VDD of U5 is fed from 3V3
through D4, a B5819WS Schottky diode; J2, a 2-pin socket, sits across VDD
and ground; C33, 1 µF, on VDD; the crystal Y2, 32.768 kHz, with 22 pF
(C22, C29) from each pin to ground. The rear print names the socket `RTC`
with `+` and `-` (`_trace_print.py`), and the mechanical model has it as
"RTC battery socket, 2 pins 1.0".

Not verified, and not known:

- which cell type the board is meant for: no cell is fitted on the owner's
  board (decision 6);
- whether a cell on J2 is charged through D4 while 3V3 is up: with no
  resistor drawn, D4 can drive current into a cell whose voltage is below
  3V3 minus D4's drop, which a primary lithium cell must not take;
- whether 3V3 stays up while the board is off on its LiPo with the power
  latch (GP26) released;
- how long C33 alone keeps VDD up: 1 µF from about 3 V down to the
  clock's 0.9 V is about 10 s at the data sheet's 0.22 µA typical, a
  figure given with CLKOUT off; with CLKOUT on, as after power-on, the draw
  is not known (pin 9 is unconnected as drawn, and pin 3, CLKOE, too), and
  D4's reverse leakage takes more; not measured. So VDD does not always
  start from 0 V;
- whether CLKOE, pin 3, is unconnected on the board as the drawing shows;
  if it floats, it breaks the data sheet's rule that inputs sit at VSS or
  VDD (table 3) and may draw current from the cell;
- the crystal's load capacitance, so whether CAP_SEL's 7 pF is right, and
  the drift: not measured.

The owner's board has no cell on J2 (decision 6). The clock is lost each
time 3V3 goes away, and the OS flag says so at the next boot. A power-up
therefore starts with the clock not set, and it runs only after the set
of the next section. A reset with 3V3 up, which keeps the clock, is the
one case in which the boot read finds a running clock.

### Reading it

- At boot, after `touchInit()` has set the bus up: one read of 00h to 0Ah,
  14 bytes on the wire (address, pointer, repeated start, address, 11
  data bytes), 129 clocks, 323 µs at 400 kHz, with a 1000 µs transfer
  timeout as the INA219's (`SENSE_TIMEOUT_US`). Clock counts by
  [current-sense.md](current-sense.md#transaction-times).
- The clock is not set when OS = 1, STOP = 1, EXT_TEST = 1, a register is
  out of range, or 0x51 does not answer. With OS = 1 the firmware sends the
  software reset, 58h, then Control_2 07h: the time is lost anyway, and the
  reset puts registers right that a VDD not starting from 0 V may have left
  wrong. With the other causes it leaves the part alone until a set.
- From then on the firmware keeps time from that reading plus the 64-bit
  microsecond timer. No I2C transfer of the clock runs during a log, unless
  a USB set lands in it, and none from inside FatFs. Over a session the
  timer's crystal error adds to the RTC's; neither is measured.
- After a set, the set time becomes the reading.

The bus stays a core0 bus, used between frames as the touch and INA219
reads are ([current-sense.md](current-sense.md#bus-sharing-in-software)).

### Setting it

A set is four transfers on core0, each its own START and STOP:

1. 58h to Control_1, the software reset (3 bytes, 29 clocks, 73 µs);
2. 20h, 07h from 00h: STOP = 1 and CAP_SEL = 0 in Control_1, CLKOUT off
   in Control_2 (4 bytes, 95 µs);
3. 04h to 0Ah, the time with OS = 0 (9 bytes, 208 µs);
4. 00h to Control_1: STOP released, the clock runs (73 µs).

The clock then runs within 1 s of the time sent, the half second of
7.2.1.2 included. CAP_SEL is written as its reset value, 7 pF; whether
that suits Y2 with C22 and C29 is for the bench to find from the drift.

The firmware's USB serial port reads nothing today (`cdcInChars()` returns
no data, verified). The design adds one line command on it, parsed on
core0 in `usbDevTask()`'s pass without waiting for input. Lines that do
not start with `CLOCK` are ignored.

| Sent | Answer |
|---|---|
| `CLOCK 2026-11-14 15:42:07` | `CLOCK SET 2026-11-14 15:42:07` |
| `CLOCK?` | `CLOCK 2026-11-14 15:42:09` or `CLOCK NOT SET` |
| a bad date or time | `CLOCK REFUSED: BAD DATE` |
| no answer at 0x51 | `CLOCK REFUSED: RTC NOT ANSWERING` |

`tools/set_clock.py` sends the computer's local time through the
`pyserial` package, the port in raw mode, on Windows, Linux and macOS
alike; it skips the 10 Hz telemetry lines on the same port and reads only
lines that start with `CLOCK`. A set is accepted at any time: it changes no
card and no motor state, and a log running across it keeps its durations,
which come from the timer; its file says the clock was set during it.

SETTINGS → BOARD gets a read-only row `CLOCK`: `2026-11-14 15:42`
(16 characters, 95 px) or `NOT SET` in `C_DIM`, repainted once a minute.
There is no page on the device to set the date (decision 5): a power-up
without a computer on the USB port leaves the clock not set.

The clock holds the local time it was given: no time zone, no daylight
saving (decision 7). FAT stores local time as well.

### Files before the clock is set

Files are dated only after the clock is set in that power-up. Until then
`get_fattime()` returns 0, so a file created or changed in that period
carries FAT date 0, as every file does today. A `.TXT` of a log that ran
in that period reads `Clock at start: not set` and, when the stop came
before the set, `Clock at stop: not set`. A log that a set interrupts
keeps the line the table of cases gives for it. A file that FatFs stamps
at a later sync, after the set, gets the set clock's time as its
modification time, so one `.BFL` can carry date 0 as creation time and a
real modification time. The order of the backup ring is then known from
the dates only for files written after a set.

### FAT dates today

Today every file and directory the firmware writes after a power-on has
date and time 0. A timer that another firmware image started may run on
across a reset that is not a power-on; whether it does is not verified.
Verified by reading the code: the library's `get_fattime()` in `my_rtc.c`
returns 0 when the RP2350's always-on timer is not running
(`aon_timer_is_running()`), and nothing in the firmware or the compiled
library files starts it (no call of `aon_timer_start()` or `time_init()`);
FatFs is built with `FF_FS_NORTC` 0, so it calls `get_fattime()`. A FAT
date of 0 is year 1980, month 0, day 0, which is no valid date; how
Windows, Linux and macOS show it is not verified. The host suite's FatFs
tests define their own `get_fattime()`, 2026-01-01.

[backups.md](backups.md#the-ring) says the files' dates cannot tell the
ring's order, as the board's clock is not used; that stays true until the
clock is built, and then only for files written after a set.

### FAT dates with the clock

`src/clock.cpp` defines `get_fattime()`, and `CMakeLists.txt` drops the
library's `my_rtc.c` from the `sd_fatfs` list. `my_rtc.c` also defines
`epochtime`, which the library's `crash.c` reads, and `crash.c` stays in the
build: `my_debug.c` calls its `capture_assert()` (line 149, verified). So
`src/clock.cpp` defines `epochtime` as well. The packing is a pure function,
`clockFatTime()`: year − 1980 in bits 31 to 25, month, day, hours, minutes,
and seconds / 2 in bits 4 to 0. With the clock not set it returns 0, as today.

Starting the always-on timer from the RTC and keeping `my_rtc.c` is the
other way. It is not taken: its conversion runs newlib's `localtime_r()` on
the target, which the host suite cannot run as built, and the timer is
otherwise unused.

FatFs R0.15 stamps the time when it creates or replaces a file (creation
and modification time), at each `f_sync()` and `f_close()` of a changed
file (modification time), and at `f_mkdir()` (verified, `ff.c`). So with
the clock set:

- a `.BFL` carries its start as the creation time and its last sync,
  every 2000 ms, or its stop as the modification time; a log cut by a power
  loss shows when it was last synced;
- a `.TXT` carries the stop;
- the device backups in `BACKUP/`, `BACKUP/LAST`, KST's and Hitec's files
  carry the time FatFs wrote their entries, also during the write cache,
  which queues blocks FatFs has already stamped;
- FAT times step by 2 s; exFAT's 10 ms and UTC-offset fields are FatFs's
  and not checked.

## Interlocks

- **USB DRIVE.** The handover is refused while recording (`mscRefusal()`),
  and the summary is written inside the stop, so the file is closed before
  a host can take the card. While a host holds the card no log runs and no
  summary is written. The clock can be set while a host holds the card; no
  file changes.
- **Logging never blocks the tester.** The summary adds a constant-time
  update per frame on core0 and no card or bus access during a log; a USB
  set during a log is four I2C transfers, 449 µs at 400 kHz. The
  file is written only at a stop, and every stop happens disarmed. Core1
  is not touched.
- **Tools.** A claim's stop writes the summary before the write cache
  turns on, so the summary's blocks do not queue behind a backup.
- **Card swap.** The board has no card-detect line. A mount forgets the
  summary with the FILE row's number, as the counters are forgotten today.
- **Settings flash.** A clock set writes the RTC, not flash, so it is not
  refused while armed and does not park core1.

## Tests

Host, each mutation-checked: break the code, see the test fail, restore.

- `test_run_summary.cpp`: maxima, minimum and their sources; a source that
  changes during the log; a KISS gap over the current peak, the peak kept from
  EDT and tagged `MIXED`; KISS current, voltage or temperature at 0 for the
  whole log giving EDT's figure; stale fields not counted; 0.00 V ignored; the
  minimum voltage while armed only; one and two outlying values not counted in
  a held maximum, three counted, and all of them in the raw one; a value
  repeated without a new arrival time counted once; KISS eRPM only without
  DShot eRPM; the summary ended by a failed write; the counter's rise across
  an ESC restart and across a counter that stays 0; the current integral
  against a hand sum, with a 600 ms gap on KISS and a 1200 ms gap on EDT;
  armed time with edges; 64-bit sums past 71.6 minutes; `runSumText()` byte
  for byte in each case of the table above; the display texts and their fits.
- FatFs on the RAM disk of `make -C test lognum`: the `.TXT` beside its
  `.BFL`, a stray `.TXT` replaced, a full volume (the file removed, the
  result reported), the next log's number unmoved by `.TXT` files.
- `test_ui.cpp`: SD LOG's rows after STOP, during a log, after a mount;
  `NOT SAVED` after CARD FULL; tags and colours; the redraw key.
- `test_clock.cpp`: BCD in and out; OS, STOP or EXT_TEST set, a register
  out of range and no answer read as not set; the software reset at boot
  only with OS set; the set's four transfers against an I2C fake;
  `clockFatTime()` at 2000-01-01, 2026-11-14 15:42:07 and 2099-12-31
  23:59:59, odd seconds; the serial line's parser on good, short, long and
  bad lines and lines that do not start with `CLOCK`; `get_fattime()` 0
  while not set.

Bench, after the slices:

1. The RTC answers at 0x51; OS is 1 after the first power-up.
2. Set from a computer, power off, power up: the clock reads not set, as
   the board has no cell on J2; the time after 24 h on 3V3 against the
   computer.
3. A log's `.BFL` and `.TXT` dates on Windows, Linux and macOS, and a file
   written before the clock was set, which carries FAT date 0.
4. A run with EDT only and one with KISS: the summary against the log in
   logwiju; the integral against the KISS counter.
5. The time a stop takes with the summary, on a slow card.

## Work breakdown

Sizes are estimates of changed lines, tests included: S under 300, M 300
to 800, L above.

1. **The summary core** (M). `src/run_summary.*`: the sample, the update,
   the held values, `runSumText()` with the clock lines `not set`, the
   display texts; its host tests.
2. **The logger** (M). `sdLogTick()` feeds samples; `sdLogStop()` and
   `enterError()` end the summary; `sdLogStop()` writes the file from the
   ring and records how the log ended (STOP, DISARM or the tool);
   the FatFs part over the RAM disk; `SdLogStatus` carries the summary.
   README and wiki (English and German) for the file.
3. **SD LOG** (M). The run rows, tags, colours, the redraw key, the fit
   checks; previews regenerated; README and wiki pages of SD LOG.
4. **The clock driver and FAT dates** (M). `src/clock.*`, the read at
   boot, `get_fattime()`, `my_rtc.c` out of the build, the dates in the
   `.TXT`; host tests. Without a set clock nothing changes on the card.
5. **Setting the clock** (S to M). The serial line, `tools/set_clock.py`,
   the CLOCK row; README and wiki. Decision 5 makes the line over USB the
   only way.
6. **Bring-up** (bench). The bench list above.

Slices 1 to 3 do not depend on the clock; slices 4 and 5 do not depend on
the summary. Each slice updates STATUS.md and CHANGELOG.md.

## Slice 1 as built

The summary core, `src/run_summary.h` and `src/run_summary.cpp`, with host
tests in `test/test_run_summary.cpp` (`runRunSummaryTests`). It is compiled
into the firmware, and nothing calls it: `sdLogTick()`, `sdLogStop()` and
`enterError()` hand it nothing until slice 2, and no `.TXT` reaches the
card. SD LOG does not change.

| Block | Where | Contract |
|---|---|---|
| The sample | `RunSumSample` | escSnapshot()'s block, escMerge()'s reading of it at `nowMs` with `KISS_STALE_MS` and `EDT_STALE_MS`, millis(), `time_us_64()` and the arm state |
| Start, update, end | `runSumBegin()`, `runSumAdd()`, `runSumEnd()`, `runSumFileResult()` | the start takes the time and the arm state; each sample counts the interval since the last; the end counts the last interval and how the log ended (`RunSumEnd`: STOP, DISARM, the three tools, CARD FULL, WRITE FAILED); samples after the end and a second end are ignored |
| The values | `runSumMaxErpm()`, `runSumMaxAmps()`, `runSumMaxTemp()`, `runSumMinVolts()`, `runSumUsedCounter()`, `runSumUsedCurrent()` | each value's held and raw figure and its source tag, by the rules of [the values](#the-values) |
| The file | `runSumText()`, `runSumTxtName()`, `RUN_SUM_FORMAT` | the text of [format](#format), both clock lines `not set`; `LOGnnnnn.TXT` |
| SD LOG's rows | `runSumRowText()`, `RunSumRow`, `RunSumRowText` | rows 2 to 8 of [on the device](#on-the-device): label, value or `--`, tag, and an alert flag for MAX TEMP at 90 °C and above and for `NOT SAVED` |

Choices made where the sections above leave room:

- **A new value.** A value is new when its arrival time differs from that
  of the last value counted from the same source; the per-frame figure
  keeps one time for KISS and one for EDT. The raw extreme counts new
  values only, as the held one does.
- **The window across sources.** The per-frame figure's last three values
  are taken in arrival order whatever their source. The held value carries
  the source of the value it holds, which `peak from` names.
- **The minimum voltage.** A disarmed sample empties both voltage windows,
  so a held minimum is three consecutive new values while armed. A value of
  0.00 V is skipped and does not break the run.
- **Fewer than three new values.** The held value is unknown; the line
  names the source, says `fewer than 3 new values in a row` and gives the
  raw value, and SD LOG shows `--`. The first line below is an example.
- **KISS sent 0.** The EDT-only figure is reported when at least one KISS
  frame was fresh, every fresh one carried 0 in the field, and EDT gave a
  value. With no fresh KISS frame the per-frame figure is EDT's alone and
  reads `EDT` without `KISS sent 0`; with KISS's zeros and no EDT value the
  line reports KISS's 0.
- **A `MIXED` minimum** reads `minimum from EDT` where a maximum reads
  `peak from EDT`, as in the second line below. Temperature has one step
  for both sources: `KISS and EDT, 1 C steps`.
- **The current's hold.** A sample's current holds over the interval to
  the next sample, for at most its source's stale window counted from the
  sample. The EDT-only sum keeps its own known time, which its line gives.
  A source tags the figure from current only when it held for more than
  0 µs.
- **The intervals.** The interval from the start to the first sample is
  armed when the log started armed, and without current. A sample stamped
  before the last one adds no time.
- **The KISS counter.** Its first value is the first fresh KISS frame of
  the log; a frame from before the start that is stale at the first sample
  is not one.
- **Rounding.** mAh to the nearest, half up; seconds to the nearest tenth;
  `Written` in kB of 1024 bytes, as SD LOG's WRITTEN row; ARMED on SD LOG
  in whole seconds, cut, as UPTIME (`diagUptimeText()`).
- **Ended by** has two texts more than the table of cases, `CARD FULL` and
  `WRITE FAILED`. A summary a card error ended writes no file, so they
  appear only in the text the function builds.
- **USED on SD LOG** shows the counter's rise when it is above 0 mAh; a
  counter that stayed flat reads `0 mAh` in the file, and SD LOG shows the
  figure from current. `>99999mAh` applies to both.
- **MAX RPM** is untagged from DShot; the fit check takes the tagged right
  edge, x 197, for every value but ARMED and SUMMARY, as the fit table
  does.

```text
Max current: unknown (KISS, 0.01 A steps, fewer than 3 new values in a row; raw 12.00 A)
Min voltage: 14.75 V (KISS and EDT, 0.01 V and 0.25 V steps, minimum from EDT, while armed; raw 14.50 V)
```

Measured by the host tests: the longest text, every field at its widest,
is 894 bytes; `runSumTxtName()`'s `LOGnnnnn.TXT` is not a log's name to
`sdLogNameNumber()`, so the files do not move the next log's number. The
FatFs part over the RAM disk is slice 2's.

The tests cover the list of [tests](#tests) for `test_run_summary.cpp`, the
note's example text byte for byte, and each case of the table of cases
that a sample can produce. 95 mutations of `src/run_summary.cpp` and
`src/run_summary.h` were run against the suite: 91 fail it, and 4 are
equivalent:

- MAX TEMP's alert without its `heldOk` test: the held value is 0 whenever
  `heldOk` is false;
- the KISS counter's arrival check removed: a frame counted again adds
  0 mAh and keeps the same last value;
- `centi()` without its sign check: no decoder gives a negative voltage or
  current;
- `runSumRpm()` with `poles > 2`: at 2 poles both divide by 1.

Not verified on hardware: nothing of this slice runs on the board yet.

## Slice 2 as built

The logger, `src/sd_log.cpp`, feeds the summary and ends it, and writes the
`.TXT` through `src/sd_log_summary.h` and `src/sd_log_summary.cpp`. The
host runs `sd_log.cpp` itself over FatFs R0.15 on a RAM disk:
`test/sd_log_fatfs.cpp`, beside `test/log_next_fatfs.cpp` in
`make -C test lognum`, both on the RAM disk of `test/ram_disk.*`. SD LOG
does not change; slice 3 draws the rows from `SdLogStatus::summary`.

| Block | Where | Contract |
|---|---|---|
| The feed | `sdLogTick()` | after escMerge(), before the frame is encoded: `RunSumSample` with the snapshot, the reading, millis(), `time_us_64()` and sdLogSetArmed()'s state; a frame the ring then drops is counted |
| The start | `sdLogStart()` | a start that opens its file begins the summary with `time_us_64()` and the arm state; a failed start keeps the last one |
| The end | `stopLog()`, `enterError()` | the stop ends the summary as it begins, before bbEnd(): STOP from sdLogStop(), DISARM from sdLogSetArmed(), the tool from sdLogStopFor(); a failed write of a running log ends it with CARD FULL or WRITE FAILED, not saved; MOTOR POLES (`settings()->poles`) is kept at the end |
| The file | `writeSummary()`, `sdLogWriteSummary()` | only after the `.BFL`'s sync and close succeeded; the text built by runSumText() in `s_ringBuf` once `logRingUsed()` is 0; `LOGS/LOGnnnnn.TXT` with `FA_CREATE_ALWAYS`; the result in `RunSum::file`; the state returns to READY whatever the result |
| The status | `SdLogStatus::summary`, `SdLogStatus::summaryPoles` | the last summary, nullptr while a log runs, before the first log, after a mount and after a release |

Choices made where the sections above leave room:

- **The status carries a pointer**, not a copy: `RunSum` is 464 bytes, and
  sdLogStatus() is called from SD LOG, the tester, the card tools and USB
  DRIVE, some of them on every UI frame. The summary lives in the logger on
  core0, where every caller runs.
- **A failure inside the stop.** The summary has ended as the stop began. A
  flush of the drain, the sync or the close that fails keeps that end
  (STOP, DISARM or the tool) and marks the file not saved with the cause,
  CARD FULL or WRITE FAILED, as the table of [a full card or a card
  error](#a-full-card-or-a-card-error) has it: the summary up to the stop.
- **One file object.** The `.TXT` is written through the `.BFL`'s `FIL`,
  closed by then, so the stop needs no second one.
- **An open refused with `FR_DENIED`** reads CARD FULL: FatFs gives it when
  the directory has no free entry and no cluster to grow by. A read-only
  file or a directory of that name, which only a computer makes, gives it as
  well and reads CARD FULL too. Any other failed open reads WRITE FAILED.
- **A short write** (CARD FULL) or a failed write (WRITE FAILED) closes the
  file and removes it. A failed close reads WRITE FAILED; FatFs then keeps
  the file open and locked, so its removal is refused until the next mount,
  after which the card holds an empty `.TXT` (seen over the RAM disk).
- **The ring.** The text is built only when `logRingUsed()` is 0; otherwise
  the file reads WRITE FAILED without a write. The drain always empties it,
  so the branch is not reached. That the text fits is a `static_assert` on
  the ring's 8192 bytes against the 1000 the text needs; a length from
  runSumText() of 8192 or more, a cut text, reads WRITE FAILED without a
  write as well, so a longer text cannot make the write read past the ring.

**Cost per frame, estimated from the compiled code, not measured.** In the
firmware built with arm-none-eabi-gcc 13.3, Release, `runSumAdd()` is 1296
bytes, 432 instructions, and `extAdd()` 250 bytes, 113 instructions. A
sample runs one pass of `runSumAdd()`, whose branches exclude each other in
part, at most 8 calls of `extAdd()`, of which a value that is not new
leaves after about 10 instructions and a new one runs about 65, and 4 calls
of `escFieldFresh()`; `sdLogTick()` adds one `time_us_64()`. With every
field new in every frame, KISS and EDT both fresh, that is at most about
1000 instructions: about 1500 cycles, 10 µs at the RP2350's 150 MHz (the
SDK's default, which the firmware does not change), 5 ms a second at the
logger's 500 Hz, 0.5 % of core0. With EDT only, a new eRPM each frame and
an EDT value now and then, it is about 300 instructions, 3 µs, 0.15 %. The
code runs from flash through the XIP (execute in place) cache; misses are
not counted. Core1 is not touched. The stop adds one runSumText(), an open,
one write of 552 to 641 bytes in the tests and a close, all while disarmed;
not measured on a card.

RAM: `s_sum`, 464 bytes (`nm` of the firmware); two bytes beside it;
`SdLogStatus` grows by a pointer and a byte.

**The `.BFL` is not touched by the summary.** The `.TXT` is opened only
after the `.BFL`'s close succeeded, under another name; the tests compare
the `.BFL` of a run whose `.TXT` fails with the same run on a card with
room, byte for byte.

The tests, `test/sd_log_fatfs.cpp`, on FAT32 and exFAT, the failures on
FAT32, with the clock past 2^32 µs so that micros() and `time_us_64()`
differ:

- a log started by hand, armed for part of it, MOTOR POLES changed during
  it, stopped 0.7 ms after its last frame: the summary's start, stop, armed
  time and poles, the `.TXT` equal to runSumText()'s text and its lines,
  the `.BFL` whole; no summary before the first log, while a log runs and
  after a mount;
- DISARM, armed for the whole log, and KST SERVO, HITEC SERVO and AM32
  CONFIG's claims;
- a stray `.TXT` of the new number, longer than the text, replaced; one of
  a higher number left alone; the next log's number from memory and after a
  mount unmoved by `.TXT` files;
- a failed start keeping the summary, a mount and USB DRIVE's release
  forgetting it;
- the `.TXT` not saved: a full card (no cluster, the empty file removed), a
  full `LOGS/` on a full card (the create refused with `FR_DENIED`), a
  failed first block write, a failed close (`FR_LOCKED` until a mount, then
  an empty `.TXT`); each with the logger READY, no card fault, the `.BFL`
  byte for byte, and the next start working;
- the log failing: CARD FULL and WRITE FAILED during the log, the summary
  ended at the failure and not saved, a later STOP changing nothing; the
  stop's sync failing, through a failed write of every block and through a
  failed write of the log's last block alone, the summary ended by STOP, not
  saved, and no `.TXT` tried; the drain's first flush failing with more
  than a 512-byte chunk in the ring, the summary ended by STOP at the
  stop's first moment, not saved.

The RAM disk fails a write whose sector begins with given bytes, and can
advance the test's clock by 1 ms a block write, so a summary ended later
than the stop's start shows in its stop time. A
throttle that differs from frame to frame keeps the log's blocks apart, and
the test checks the last block begins unlike the others.

56 mutations of `sd_log.cpp`'s wiring and of `sd_log_summary.*` were run
against `test/sd_log_fatfs.cpp`: 49 fail it, among them the stop's end of
the summary moved after the drain and the release keeping the summary. 6
are equivalent:

- the ring check removed, and the result before it Saved: the drain
  always empties the ring;
- a write error counted only by its byte count, and `wrote < len` for
  `wrote != len`: FatFs R0.15 returns fewer bytes than asked with every
  error and never more;
- the path's length check with `>`: the path is 18 bytes of 40;
- the check of runSumText()'s length against the ring removed: the text is
  under 1000 bytes of 8192.

1 is not killed: a failed close overriding a short write's CARD FULL with
WRITE FAILED. The RAM disk cannot fail that close alone: after a short write
of 0 bytes it writes only the directory block, which the log's own syncs
write as well.

Not verified on hardware: the summary on a card, the stop's time with the
`.TXT`, the cost per frame.

## Decisions

Decisions of the owner, 2026-11-15, answering the open questions of
2026-11-14:

1. **Where the run shows.** Rows 2 to 8 of SD LOG after a stop, option
   (a). FRAMES, WRITTEN, DROPPED FRAMES, BUF PEAK and WORST FLUSH move to
   the `.TXT` file; CARD and MOUNT are hidden until the next START or
   mount. This is the note's recommendation.
2. **Last run on the tester.** Option (b): SD LOG is the only place. The
   tester shows no line of the run after an auto log stops. No
   recommendation was given.
3. **What the maxima cover.** Option (a): the whole log, so the `.TXT`
   agrees with the `.BFL`. The minimum voltage counts only while armed.
   This is the recommendation.
4. **mAh with EDT only.** Option (a): SD LOG shows it as `USED FROM
   CURRENT`. This is the recommendation.
5. **Setting the clock.** Option (a): a line over USB from
   `tools/set_clock.py`. No page on the device. This is the
   recommendation.
6. **Cell on the RTC socket J2.** No cell is fitted. The clock is lost
   whenever 3.3 V goes away, and files are dated only after the clock is
   set in that power-up. Before the set a file has FAT date 0 and the
   summary reads `not set`; see
   [files before the clock is set](#files-before-the-clock-is-set). No
   recommendation was given.
7. **Local time, no time zone.** Option (a): acceptable. The pilot sets
   the clock again by hand at a daylight-saving change. No recommendation
   was given.
8. **Maxima and minimum on SD LOG.** Option (a): the held value, counted
   only when three consecutive new values reach it. This is the
   recommendation.

## Open questions

None open. Questions 1 to 8 are answered by decisions 1 to 8.
