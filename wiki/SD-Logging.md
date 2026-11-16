**English** | [Deutsch](SD-Aufzeichnung)

# SD Logging

The tester records telemetry to microSD in Betaflight blackbox format, at
500 frames per second. A frame averages 14.5 bytes (measured), which is
7.3 kB/s or 26 MB per hour.

<img src="img/log-screen.png" width="240" alt="The LOGS screen while recording">

---

## The card

Use a microSD card formatted FAT32 (File Allocation Table). The card is
mounted once at boot, and the board has no card-detect pin. Insert the card
before power-up, or press `RETRY MOUNT` on the LOGS screen after inserting
it.

Files are written to the directory `LOGS` as `LOG00001.BFL`, `LOG00002.BFL`,
and so on. Each new file takes the number after the highest `LOGnnnnn.BFL` in
`LOGS`, so the highest number is the newest log: with `LOG00001.BFL` to
`LOG00005.BFL` on the card and `LOG00002.BFL` deleted, the next log is
`LOG00006.BFL`. An empty or new `LOGS` starts at `LOG00001.BFL`. Names count
in any letter case (`log00007.bfl` is `LOG00007.BFL` to the card); other
names, such as `LOG0007.BFL`, `LOG00007.TXT` or a renamed log, are ignored.
Existing logs are not overwritten. Logs an earlier firmware wrote to the
card's root stay there and are not counted. Each file carries the time of
the board's real-time clock once the clock is set in that power-up (see
[The clock](#the-clock)). Before that, the files carry FAT date and time
0, which is no valid date; how a computer shows it is not verified.

The board reads `LOGS` once, at the first log after the card is mounted
(power-up, `RETRY MOUNT`, the end of `USB DRIVE`, a tool's remount) and at
the first log after a card error, and counts on from there. With many logs
on the card that first start reads more of the card: one 512-byte block per
16 files with short names. The LOGS screen names the last log of the
session (see `FILE` below).

After `LOG99999.BFL` no number is left: no log starts, and the note reads
`NO LOG STARTED: LOG99999.BFL EXISTS`. Take the card out, move the logs off
it on a computer (`USB DRIVE` is read-only), put it back and press
`RETRY MOUNT`.

The board reads the card over 4-bit SDIO (Secure Digital Input/Output) at
10 MHz.

---

## The clock

The board's real-time clock dates the logs, their summaries and the device
backups on the card. The board has no cell in its clock socket J2, so the
clock is lost each time the board loses power: set it again after each
power-up. There is no page on the device to set it. Connect the board to a
computer with its USB cable and run, in the firmware's source folder:

```text
python3 tools/set_clock.py
```

It needs Python 3 and the pyserial package (`python3 -m pip install
pyserial`), on Windows, Linux or macOS. It finds the board on USB by itself;
with no board or several found it says so, and `--port` names one, for
example `--port COM5` on Windows or `--port /dev/ttyACM0` on Linux.
`--list` names the boards it finds. It answers with the time it set,
`CLOCK SET 2026-11-14 15:42:07`. `--query` reads the clock without setting
it, and `--help` lists the options.

The clock takes the computer's local time: no time zone and no daylight
saving. After a daylight-saving change, set it again.

SETUP → BOARD → `CLOCK` shows the time to the minute, or `NOT SET` in
grey; the line under it reads `SET AT EACH POWER-UP`. Until the set, files
carry FAT date 0 and a run summary reads `Clock at start: not set`. A set
during a log is allowed: the log goes on, and its summary adds `(clock set
during the log)` to the stop's time.

A terminal program on the board's serial port can send the same lines:
`CLOCK 2026-11-14 15:42:07` sets the clock, `CLOCK?` reads it and is
answered `CLOCK NOW` and the time. The board ignores its own answers when a
terminal echoes them back. A wrong line is answered `CLOCK REFUSED: BAD
LINE`, a date that does not exist `CLOCK REFUSED: BAD DATE`, a clock chip
that does not answer `CLOCK REFUSED: RTC NOT ANSWERING`.

The clock has not run on the board yet: the set, the time it keeps and the
dates on a card are not verified.

---

## Recording

**Automatic:** arming starts a log, disarming stops it.

**Manual:** `START` on the LOGS screen starts a log at any time. A log
started by hand is not stopped by a disarm. `STOP` ends it.

The tester screen shows the state in its top row: `REC` (red) while recording,
`SD` while idle, `SDERR` after a card error, `NOSD` with no card. `SDERR` is
drawn on an amber chip, between the arm badge and `BOARD`, so a recording that
stopped on its own shows while armed. LOGS says what happened.

A log also ends when KST SERVO, HITEC SERVO or ESC SETTINGS opens: the tool
needs the card for its backup. LOGS then reads
`RECORDING STOPPED BY KST SERVO` (or `HITEC SERVO`, `ESC SETTINGS`). Press
`START` for a new log after the tool. The servo tester stops it too, at
`HOLD TO START` with the current sensor fitted, to write its own log,
`LOGS/SRVnnnnn.CSV` ([Servo Tester](Servo-Tester#the-log-on-the-card));
LOGS then reads `RECORDING STOPPED BY SERVO TESTER`.

**Stop the recording before cutting the board's power or pulling the card:**
disarm, or press `STOP` on LOGS, and wait until the top row reads `SD`. A
stop writes the end of the log, gives back the unused space, closes the file
and writes its run summary beside it. A log cut by a power loss or a pulled
card is not closed. What it holds depends on when it was cut:

- **Within its first 2 s:** it reads 0 bytes. The file's size on the card
  is written every 2 s while recording, the first time 2 s after the start.
  The space reserved for it, up to 16 MiB, stays marked as used on the card
  with no file pointing to it, until a file system check on a computer
  frees it.
- **Later:** it is 16 MiB long, the space reserved when it opened. It holds
  the data that reached the card, which is everything recorded except up
  to the last 8192 bytes, the firmware's write buffer. After that comes
  whatever the card held in that space before: old data, not part of the
  log.
- **Without the reservation, or past 16 MiB:** when the card had no free
  run of 16 MiB at the start, or the log grew past it, the file has the
  size written at the last of those 2 s steps; what came after it is not
  in the file.

These are facts of the firmware's code and of the FatFs library it uses,
not measured on a card. How a viewer handles such a file is not tested.

---

## The run summary

Each log that stops clean gets a text file beside it with the same number:
`LOG00012.TXT` beside `LOG00012.BFL`. Any text editor opens it. The stop
writes it once the log is closed: `STOP`, the disarm that ends a log arming
started, and a tool taking the card. A log that a card error ended gets
none. LOGS shows the summary after the stop (see
[The last run](#the-last-run)).

It holds one value a line:

| Line | Content |
|---|---|
| `Log`, `Firmware` | The log's file name and the firmware version |
| `Clock at start`, `Clock at stop` | The board's clock at the log's start and stop, as `2026-11-14 15:42:07`, or `not set` while the clock is not set; after a set during the log the stop's line adds `(clock set during the log)` ([The clock](#the-clock)) |
| `Ended by` | `STOP`, `DISARM`, `KST SERVO`, `HITEC SERVO`, `ESC SETTINGS` or `SERVO TESTER` |
| `Duration`, `Armed` | The log's length and the time armed within it, in seconds |
| `Max RPM` | The highest RPM: DShot eRPM (electrical RPM), or KISS eRPM in a log without DShot eRPM, divided by half the `MOTOR POLES` value at the stop |
| `Max current`, `Max temperature` | The highest current and ESC (electronic speed controller) temperature: KISS while KISS is live, EDT (Extended DShot Telemetry) in its gaps; EDT alone when KISS sent 0 in every frame, an ESC without that sensor |
| `Min voltage` | The lowest voltage while armed, from the same sources; a reading of 0.00 V does not count |
| `Used, ESC counter` | How far the KISS counter of milliampere hours (mAh) rose during the log |
| `Used, from current` | The current added up over time, with the time it was known |
| `Frames` to `Worst flush` | The counters LOGS shows while recording |

A maximum or minimum counts a value only when three new readings in a row
reach it, so one damaged reply sets none. The `raw` figure in brackets
counts every reading, as a viewer finds it in the `.BFL`. Each line names
its source, `DShot`, `KISS`, `EDT`, or `KISS and EDT` when EDT filled gaps
in KISS, and its step. A value with no reading reads `unknown`, with the
reason. `C` stands for degrees Celsius. An example:

```text
MiniRcBench run summary, format 1
Log: LOG00012.BFL
Firmware: 0.1.0
Clock at start: not set
Clock at stop: not set
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

The `.TXT` files do not change the log numbers: only `.BFL` names count. A
`.TXT` with the new log's number, left on the card after its log was
deleted on a computer, is replaced.

When the summary cannot be written, on a full card or after a failed write,
the log stays whole on the card and `STATUS` stays `READY`. No `.TXT` is
left, or, after a write that failed at the very end, an empty one. LOGS
then reads `NOT SAVED` in its `SUMMARY` row. The summary and its rows on
LOGS are tested with the firmware's host tests, the file over the FatFs
library on a simulated card; on a card and on the board they are not
tested.

---

## Reading the screen

Open it with **MENU → LOGS**, the fifth row. The row shows the same
state: `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a
computer holds the card.

<img src="img/log-ready.png" width="240" alt="Card mounted, ready to record">

| Row | Meaning |
|---|---|
| **STATUS** | `NO CARD`, `READY`, `RECORDING` or `CARD ERROR` |
| **FILE** | The open file. After a log ends, its file with `(LAST)`, for example `LOG00012.BFL (LAST)`, until the next `START` or a mount. `--` before the first log after power-up and after a mount (`RETRY MOUNT`, the end of `USB DRIVE`, a tool's remount): the card may be another one, so the name and the counters reset |
| **FRAMES** | Frames written so far |
| **WRITTEN** | Kilobytes written |
| **DROPPED FRAMES** | Frames the buffer could not hold. **Should be zero.** Red otherwise |
| **BUF PEAK** | Highest fill of the 8192-byte write buffer. Amber above 1/2, red above 3/4 |
| **WORST FLUSH** | The longest single write to the card, in ms. Amber above 50 ms |
| **CARD** | Card type and size, as the card reports them |
| **MOUNT** | The filesystem result code. `0 OK` means mounted |

`BUF PEAK` near the buffer size, or a `WORST FLUSH` longer than the buffer
covers (8192 bytes is about 1.1 s at 7.3 kB/s), means the card is too slow for
the buffer. `DROPPED FRAMES` above zero means the log already has gaps.

The usual cause is a slow card. Try another card first.

The rows from `FRAMES` to `MOUNT` show while a log runs, and whenever no
run summary shows: before the first log, after a mount and after
`USB DRIVE`. After a stop, the last log's run takes their place.

### The last run

<img src="img/log-last.png" width="240" alt="LOGS after STOP: FILE reads LOG00001.BFL (LAST), the rows below it the run">

Once a log stops, the rows below `FILE` show its run summary, the same
figures as its `.TXT`:

| Row | Meaning |
|---|---|
| **MAX RPM** | The highest RPM, divided by half the `MOTOR POLES` value at the stop |
| **MAX CURRENT** | The highest current, `23.41A` from KISS, whole amperes from EDT, `23A` |
| **MAX TEMP** | The highest ESC temperature. Red at 90 °C and above, as on the tester |
| **MIN VOLTAGE** | The lowest voltage while armed |
| **USED** | How far the KISS counter rose, in mAh |
| **USED FROM CURRENT** | In place of `USED` when the KISS counter did not rise: the current added up over time |
| **ARMED** | The time armed within the log, `H:MM:SS` |
| **SUMMARY** | The `.TXT`'s name, for example `LOG00012.TXT`; in red `NOT SAVED: CARD FULL` or `NOT SAVED: WRITE FAILED` when it was not written |

A maximum or minimum is the value held over three new readings in a row,
the figure the `.TXT` gives first; `--` when there is none. The word beside
a value names its source: `KISS`, `EDT`, or `MIXED` when EDT filled gaps in
KISS. RPM from DShot has none. A charge above 99999 mAh reads `>99999mAh`;
the `.TXT` has the figure. `FRAMES` to `WORST FLUSH` are in the `.TXT`
then. `CARD` and `MOUNT` stay hidden until the next `START` that opens a
log, or a mount: they matter when no card mounts, and then no run shows.

After a log a card error ended, the rows show its run up to the failure,
`SUMMARY` reads `NOT SAVED`, and the lines above `START` say how it ended. A
`START` that fails keeps the last run, as it keeps `FILE`. The tester shows
nothing of the run: after a log that arming started and disarming stopped,
open LOGS to see it.

### How the last log ended

<img src="img/log-card-full.png" width="240" alt="LOGS after a write found the card full: the run, NOT SAVED: CARD FULL">

When a log ends on its own, or a tool takes the card, amber lines above
`START` say how, and what to do. A disarm that ends a log arming started, and
`STOP`, show none. A card's fault stays until the next `START` or a mount
(`RETRY MOUNT`, the end of `USB DRIVE`, or a tool's remount: RETRY MOUNT on
KST SERVO or HITEC SERVO, or their remount after a failed card write). A
tool's stop has one line and stays
until the next `START`, across mounts, as it stays true of the last log;
without a card the mount hint takes the second line.

| Lines | Meaning | What to do |
|---|---|---|
| `RECORDING STOPPED: CARD FULL` / `FREE SPACE ON A COMPUTER, RETRY MOUNT` | A write found no free space | Delete files on a computer (`USB DRIVE` is read-only), put the card back, press `RETRY MOUNT` |
| `RECORDING STOPPED: CARD WRITE FAILED` / `REMOVED? REINSERT, THEN RETRY MOUNT` | A write, or the final sync or close, failed: a card that was pulled out, or a card fault. The two are not told apart | Reseat the card, press `RETRY MOUNT` |
| `NO LOG STARTED: THE FILE DID NOT OPEN` / `CHECK THE CARD, THEN RETRY MOUNT` | `START` could not make or read `LOGS`, or open the next file | Check the card on a computer, press `RETRY MOUNT` |
| `NO LOG STARTED: LOG99999.BFL EXISTS` / `MOVE LOGS TO A COMPUTER, RETRY MOUNT` | `LOGS` holds `LOG99999.BFL`, and no number follows it | Move the logs off the card on a computer (`USB DRIVE` is read-only), put the card back, press `RETRY MOUNT` |
| `RECORDING STOPPED BY KST SERVO` (`HITEC SERVO`, `ESC SETTINGS`) | The tool took the card for its backup | `START` for a new log |
| `RECORDING STOPPED BY SERVO TESTER` | The servo tester took the card for its own log | `START` for a new log |

A full card is told from a failed write by what the filesystem reports: a
write that finds no free cluster ends with fewer bytes written and no error.
The first three leave `STATUS` at `CARD ERROR` and the tester at `SDERR`.
While a `USB DRIVE` refusal shows, it takes the lower line and the note
waits.

### No card

<img src="img/log-nocard.png" width="240" alt="No card">

`NO CARD` with `MOUNT 3 NOT READY` means the card did not start. When
nothing answered on the bus, no card or one not seated, the note line above
`START` reads `INSERT A CARD, THEN RETRY MOUNT`. When a card answered and
still did not start, it reads `CARD NOT USABLE: RESEAT OR REPLACE` in amber:
take the card out and in again, or try another. `MOUNT 13 NO FILESYSTEM` with
a size under `CARD` means the card answers and the filesystem is the problem.
Format it as FAT32; the note line stays blank, as inserting a card does not
help.

### A card inserted after boot

<img src="img/log-mounted.png" width="240" alt="A card found by RETRY MOUNT">

Press `RETRY MOUNT`. Without it, a card inserted after power-up is not
mounted.

---

## Getting the logs off without a card reader

<img src="img/log-usb-serving.png" width="240" alt="The card served as a USB drive">

`USB DRIVE` on the LOGS screen presents the card to the computer on the USB
(Universal Serial Bus) cable as a removable drive. Copy the `.BFL` files and
their `.TXT` summaries off, eject the drive on the computer, and the tester
takes the card back. The screen shows the amount read so far, for example
`3.0 MB READ`, in lime: a live reading.

The board is a USB device already, for power and the serial port. The drive is
a second USB interface on the same cable.

**The drive is read-only.** The computer can copy files and cannot write. A
host filesystem driver therefore cannot change the card or the log numbering.
A "write protected" message from the computer is expected.

**The card is handed over, not shared.** FAT has no arbitration between two
writers. Before the drive appears the logger flushes, closes and unmounts, and
it does not touch the card until eject. Meanwhile the LOGS screen shows the
handover in place of its counters.

Without a card, `USB DRIVE` is drawn unavailable and does nothing.

**With a card, the button refuses in two cases, and names the reason:**

<img src="img/log-usb-refused.png" width="240" alt="USB DRIVE refused while recording">

| Caption | What to do |
|---|---|
| `DISARM FIRST` | Disarm. The handover pauses the logger |
| `STOP RECORDING FIRST` | Stop the recording |

**Arming is blocked while the computer holds the card.** `BACK` leaves the
screen for `MENU` without cancelling the handover, and `BACK` on `MENU` reaches
the tester. The tester does not arm until the card is ejected: `HOLD TO ARM`
is drawn as an outline, and the throttle bar reads
`EJECT THE USB DRIVE TO ARM`. A press that starts while the
computer holds the card does not arm, even after the eject. Press again.

Eject on the computer, or press `EJECT` on the screen. Both end the handover.
A computer that unconfigures the drive ends it too. A cable pulled with the
board on its battery does not: this firmware's USB stack sees it as a computer
that went to sleep, which keeps the card. Press `EJECT` then. Not tested on
hardware.

---

## Opening the logs

<a href="https://subtilitas.github.io/logwiju/"><img src="https://img.shields.io/badge/open%20your%20logs%20in-logwiju-07b0c8?style=for-the-badge" alt="Open your logs in logwiju"></a>

### [logwiju](https://subtilitas.github.io/logwiju/) — the intended viewer

logwiju runs in the browser with no install. The file is read locally and not
uploaded. Drop a `.BFL` file on the page to plot the fields against time.

### Betaflight Blackbox Explorer

The files are Betaflight blackbox logs, so
[Blackbox Explorer](https://blackbox.betaflight.com/) and the
`blackbox_decode` command-line tool open them too. Some field names come from
flight controllers, for example `motor[0]` for the throttle.

---

## What is in a log

Fields per frame:

| Field | Content |
|---|---|
| `loopIteration`, `time` | Frame counter and time |
| `motor[0]` | Commanded throttle |
| `eRPM[0]` | eRPM from bidirectional DShot |
| `eRPMkiss[0]` | eRPM from KISS |
| `vbatLatest`, `amperageLatest` | Voltage and current from KISS; 0 while KISS is not live |
| `vbatEdt`, `amperageEdt` | Voltage and current from EDT |
| `escTemperature[0]` | ESC temperature |
| `escConsumption` | Consumed mAh (KISS only) |
| `escStress` | EDT stress value |

The ESC status byte, the arm state and the link counters are not logged.

With a KISS wire fitted, both sources are recorded separately, so a log can
check one source against the other.

---

## If the card is not the problem

Logging is compiled in by default. A firmware built with `SD_LOG_ENABLE=0`
still shows the LOGS screen, and it always reports `NO CARD`.
