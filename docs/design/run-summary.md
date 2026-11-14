# Run summary per log, dated by the board's clock

Status: planned, not built; the owner's choice of 2026-11-14 from the
feature survey, feature 4 of 6, built as a design note and then slices
with reviews. It waits on the owner's answers to the eight
[open questions](#open-questions).

Each blackbox log gets a run summary: the highest RPM, current and
temperature, the lowest voltage, the charge used and the armed time. SD LOG
shows it once the log stops, and the logger writes it as a small text file
beside the log, `LOGS/LOG00012.TXT` beside `LOGS/LOG00012.BFL`. As an
option, the board's PCF85063 real-time clock dates the logs, the summaries
and the device backups on the card.

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

- Code at `main` d4606a6: `src/sd_log.*`, `src/sd_log_next.*`,
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
frame (`escSnapshot()` in `sdLogTick()`), per source, as the log keeps its
voltage and current columns per source; the source is chosen at the stop.
So each number can be checked against the log in logwiju. A field counts
only while it is fresh by the firmware's existing windows: KISS for
500 ms (`KISS_STALE_MS`), an EDT field for 1000 ms (`EDT_STALE_MS`), DShot
eRPM for 500 ms (`ESC_LINK_STALE_MS`).

Each value but the minimum voltage covers the whole log, from START or
arming to the stop (open question 3). The minimum voltage counts only
while the tester is armed, whatever question 3 decides: before arming and
after disarming it would record the ESC powering up or browning out as
the pack is plugged or pulled, not the sag under load.

**Source at the stop.** For voltage, current and temperature the summary
keeps a KISS figure and an EDT figure. KISS's is reported when KISS was
fresh and its field was above 0 in at least one frame; otherwise EDT's,
when EDT's field was fresh; otherwise the value is unknown. A KISS field
that stays 0 for the whole log is an ESC without that sensor, and does
not hide EDT's figure as it does on the tester. The KISS counter has the
same rule.

**Held values.** A maximum or minimum counts a value only when it holds
for three consecutive new values of its field: the summary keeps the last
three, and the held maximum is the highest of their lowest, the held
minimum the lowest of their highest. A new value is one whose arrival
time (`lastRpmMs`, `kissLastMs`, `edtVoltsMs` and the other EDT stamps of
`EscTelemetry`) differs from the last sample's. One damaged frame then
sets no maximum. Three values span 4 to 6 ms of DShot eRPM at the
logger's 500 Hz, 60 ms of KISS, and an unknown time of EDT, which the ESC
paces. The raw extreme, every value counted, is kept as well and written
to the file beside the held one, as it is the one a viewer finds in the
`.BFL`. The held value is the one SD LOG shows (open question 8).

| Value | From | Rule | Unknown when |
|---|---|---|---|
| Max RPM | DShot eRPM; KISS eRPM only when no DShot eRPM was fresh in the whole log | the held maximum eRPM, divided by half the MOTOR POLES value in force at the stop | neither eRPM was fresh in three consecutive new values |
| Max current | KISS or EDT, chosen at the stop | the held maximum | no source qualifies |
| Max temperature | KISS or EDT, chosen at the stop | the held maximum | no source qualifies |
| Min voltage | KISS or EDT, chosen at the stop | the held minimum above 0.00 V, armed only | no source qualifies, or the tester was never armed |
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
  [display](#on-the-device)).
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
  recording; so `enterError()` closes the summary too, with the stop time,
  Duration and Armed up to the failure. A start that fails opens no log
  and keeps the last log's summary, as it keeps the FILE row's number.

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

RAM, estimated from the fields above, not measured:

| Item | Bytes |
|---|---:|
| the running summary: per source the raw and held extremes, the last three new values and their arrival times, zero flags; the KISS counter's last value and rise; two 64-bit current sums; armed, known-current and total time in 64-bit µs; start and stop clock readings | about 300 |
| the text of the file | 0: built in the logger's ring, `s_ringBuf`, 8192 bytes |
| the clock: the reading at boot, its timer value, a flag | 16 |

The ring is free when the text is built. The stop drains it (64 flushes of
512 bytes at most, 32,768 bytes, more than the ring holds), and the text
is written only after the `.BFL` has closed clean; `sdLogTick()` writes
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
matter when no card mounts, and then no summary shows (open question 1).
After a log a card error ended, the rows show the run up to the failure,
`SUMMARY` reads `NOT SAVED`, and the note lines say how it ended.

| Row | Label | Value, widest | Tag |
|---|---|---|---|
| 0 | `STATUS` | unchanged | |
| 1 | `FILE` | unchanged, `LOG00012.BFL (LAST)` | |
| 2 | `MAX RPM` | `60000000` | `KISS` when from KISS, none from DShot |
| 3 | `MAX CURRENT` | `655.35A`, from EDT `255A` | `KISS` or `EDT` |
| 4 | `MAX TEMP` | ``255`C`` | `KISS` or `EDT` |
| 5 | `MIN VOLTAGE` | `655.35V` | `KISS` or `EDT` |
| 6 | `USED` or `USED FROM CURRENT` | `65535mAh`, `>99999mAh` | `KISS`, `EDT` or `MIXED` |
| 7 | `ARMED` | `999:59:59` | |
| 8 | `SUMMARY` | `LOG99999.TXT`, `NOT SAVED: CARD FULL`, `NOT SAVED: WRITE FAILED` | |

- Values follow the tester's tiles (`drawTelemetry()`): `14.82V`,
  `23.41A`, ``51`C`` (the font draws a backtick as a degree sign), `312mAh`.
  EDT current is in whole amperes, `23A`, as in the file, where the tester
  draws `23.00A`; voltage has two decimals from both sources. The tag says
  which step applies. An unknown value is `--`.
- The held values show; the raw ones are in the file only.
- `USED` shows the KISS counter's rise when it rose; otherwise the figure
  from current, labelled `USED FROM CURRENT` (open question 4); otherwise
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
the tester shows a line of the run is open question 2; this note designs
SD LOG only.

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
  Control_1, once the supply is stable, then a STOP and START on the bus
  (7.2, 7.2.1.3, figure 8). It sets every register to its reset value, the
  time included.
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

- whether a cell is fitted on the owner's board, and which type the board
  is meant for (open question 6);
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
- the crystal's load capacitance, so whether CAP_SEL's 7 pF is right, and
  the drift: not measured.

Without a cell, the clock is lost each time 3V3 goes away, and the OS flag
says so at the next boot.

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
A page on the device to set the date with steppers is open question 5.

The clock holds the local time it was given: no time zone, no daylight
saving (open question 7). FAT stores local time as well.

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
clock is built and set.

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
  changes during the log; KISS current, voltage or temperature at 0 for the
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
2. Set from a computer, power off with and without a cell on J2, read it
   back; the time after 24 h against the computer.
3. A log's `.BFL` and `.TXT` dates on Windows, Linux and macOS, and a file
   written before the clock was set.
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
   the CLOCK row; README and wiki. Changes with open question 5.
6. **Bring-up** (bench). The bench list above.

Slices 1 to 3 do not depend on the clock; slices 4 and 5 do not depend on
the summary. Each slice updates STATUS.md and CHANGELOG.md.

## Open questions

1. Where the run shows: rows 2 to 8 of SD LOG after a stop, with FRAMES,
   WRITTEN, DROPPED FRAMES, BUF PEAK and WORST FLUSH moving to the `.TXT`
   and CARD and MOUNT hidden, until the next START or mount (recommended);
   or a page of its own, which needs a control SD LOG has no room for?
2. Does the tester show the last run in one line after an auto log stops,
   or is SD LOG the only place?
3. Max RPM, current and temperature and the charge used over the whole
   log (recommended: the `.TXT` agrees with the `.BFL`), or only while
   armed? The minimum voltage counts only while armed either way.
4. With EDT only, mAh comes from 1 A steps, up to 28 mAh off per 100 s by
   the step alone: show it on SD LOG as USED FROM CURRENT (recommended), or
   only in the `.TXT`?
5. Setting the clock: a line over USB from `tools/set_clock.py`
   (recommended), a page on the device, or both?
6. Is a cell fitted to the RTC socket J2 on the owner's board, and which
   type? Without one the clock is lost whenever 3.3 V goes away.
7. The clock keeps local time with no time zone, set again by hand at a
   daylight-saving change: acceptable?
8. Maxima and the minimum on SD LOG: the held value, counted only when
   three consecutive new values reach it, so one damaged frame sets
   nothing (recommended); or the raw extreme, which agrees with the
   `.BFL` and a viewer? The file holds both either way.
