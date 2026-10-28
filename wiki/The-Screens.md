**English** | [Deutsch](Die-Bildschirme)

# The Screens

The firmware has seven screens. The tester is the root. `MENU` opens the list of
tools, and every tool is left with `BACK`, which returns to `MENU`.

```
Tester ──MENU──> MENU ──┬── AM32 CONFIG  ──BACK──> MENU
   ▲                    ├── SERVO TESTER ──BACK──> unplug page ──> MENU
   │                    ├── KST SERVO    ──BACK──> leave page ──> unplug page ──> MENU
   │                    ├── SD LOG       ──BACK──> MENU
   │                    └── SETTINGS     ──BACK──> MENU
   └──── BACK on MENU ──┘
```

`BEEP THE ESC` is a row of `MENU` that acts and opens no screen.

**BACK is at the top right**, in the header band, on every screen except the
tester. The header is 36 px tall and `BACK` is 64 × 32 px. The tester is the
root and has no header.

---

## The tester

<img src="img/tester-armed.png" width="240" alt="The tester screen">

**Top row.** From left to right:

- the arm badge: `SAFE` (green) or `ARMED` (red);
- `DS` and the DShot bitrate in kbit/s, as set under SETTINGS, cyan once an EDT
  (Extended DShot Telemetry) enable has been sent. `DSERR` in amber replaces
  it when the display could not set up its DShot output; nothing is sent to
  the ESC then;
- the recording state: `REC`, `SD`, `SDERR` or `NOSD`
  (see [SD Logging](SD-Logging));
- the board's own battery voltage, read through its 200k/100k divider. With
  only USB (Universal Serial Bus) power connected, what this reads is not
  measured. During the last 5 s before the idle interlock disarms, `IDLE ns`
  in amber replaces the voltage.

**RPM.** Mechanical RPM (revolutions per minute), computed from the eRPM
(electrical RPM) the ESC (electronic speed controller) reports and the pole
count. The `ERPM` value and the pole count are shown below it. Green digits:
eRPM arrived within the last 500 ms. Dark red zero: it did not, and the line
reads `NO TELEMETRY`.

**Six tiles.** Voltage, current, ESC temperature, stress, ESC status and link
quality. See [Telemetry](Telemetry).

**Throttle.** The gauge, the ceiling and the commanded percentage. The
percentage is of the full DShot range (0–2000 = 0–100 %): at a 20 % ceiling, a
full-width drag reads `20%`. While disarmed the bar reads `ARM TO ENABLE`.

**Buttons.** `HOLD TO ARM` / `DISARM`, `HOLD` (latching throttle), `MENU`.

---

## MENU

<img src="img/menu.png" width="240" alt="The MENU screen">

Reached with the tester's `MENU` button. Opening it **disarms**, sets the
throttle to 0 and turns `HOLD` off.

Under the title, a strip shows the ESC pin and the DShot rate, for example
`GP29  600 KBIT/S`, and the **EDT ON / EDT OFF** chip. The chip is read-only.
It is green while Extended DShot Telemetry frames arrive and red otherwise.
There is no enable button. The firmware sends the enable to every ESC that
starts answering, and repeats it every 1 s while eRPM arrives and EDT does not.
The ESC does not acknowledge the enable, so the chip shows *received frames*,
not sent commands.

Below the strip is a list of rows, 40 px tall. Each row has a name on the left
and its state on the right. `BACK` returns to the tester.

| Row | State on the right | A tap |
|---|---|---|
| **BEEP THE ESC** | `DISARMED`; for 350 ms after a tap, `SENT` or `REFUSED` | Sends the DShot beacon command, so the motor beeps. Use it to identify which ESC is connected |
| **AM32 CONFIG** | the ESC pin, `GP29`; `PINS BUSY` | Opens the AM32 editor |
| **SERVO TESTER** | the two signal pins, `GP29+28`; `PINS BUSY` | Opens the servo tester |
| **KST SERVO** | the ESC pin, `GP29`; `PINS BUSY` | Opens the KST programmer. See [KST Programming](KST-Programming) |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a computer holds the card | Opens the logging screen |
| **SETTINGS** | `UNSAVED` while a value differs from flash, else nothing; `PINS BUSY` while the pins are still restoring, and it does not open | Opens SETTINGS |

`PINS BUSY` means the signal pins are still in use, for example while DShot
restarts after the servo tester or the KST programmer. AM32 CONFIG, SERVO
TESTER, KST SERVO and SETTINGS show it, and a tap on such a row opens nothing.
The row reads its pins again once DShot runs. SD LOG claims no pin and stays
available.

<img src="img/menu-pins-busy.png" width="240" alt="MENU while the pins restore: PINS BUSY on AM32 CONFIG, SERVO TESTER, KST SERVO and SETTINGS">

BEEP works only while disarmed, and opening `MENU` disarms, so it is always
allowed from here. The command takes 6 ms (6 frames at 1 kHz). The screen
repaints every 25 ms, so the row is filled for 350 ms after each press:
white with `SENT`, or amber with `REFUSED`. `REFUSED` is the answer if the ESC
is armed. Opening `MENU` disarms, so it is not expected.

<img src="img/menu-beep-flash.png" width="240" alt="BEEP accepted"> <img src="img/menu-beep-refused.png" width="240" alt="BEEP refused because the ESC is armed">

`UNSAVED` on the SETTINGS row means a value differs from flash. The save button
is on SETTINGS.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED on the SETTINGS row, with changes not yet written to flash">

---

## SETTINGS

<img src="img/settings.png" width="240" alt="The SETTINGS screen">

Motor and wiring settings, display settings, and the button that writes all
settings to flash. One list, in sections, scrolls under a finger.

| Section | Row | What it does |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, even values. The RPM readout divides the eRPM by half of it |
| ESC TESTER | **MAX THROTTLE** | The throttle ceiling, `5%` to `100%` in steps of 5 % |
| WIRING | **ESC PIN** | GPIO (general-purpose input/output pin) for the signal wire. Only the board's two free GPIOs are offered, GP28 and GP29 |
| WIRING | **DSHOT KBAUD** | 150 / 300 / 600 / 1200. Lower it if the `LINK` error rate rises |
| WIRING | **KISS TELEM** | Whether to listen on the optional third telemetry wire |
| WIRING | **KISS PIN** | GPIO for that wire. Skips the ESC pin, so the two never coincide. Reads `--` and does not act while KISS TELEM is off |
| SERVO SUPPLY | **SERVO SUPPLY** | `NONE` or `GP0`: whether a DC-DC converter for the KST servo's supply has its enable wired to GP0, J5 pin 2. `NONE` by default. With `GP0`, KST SERVO switches the servo's supply |
| DISPLAY | **CONTRAST** | `NORMAL` or `HIGH`. High contrast is for daylight |
| DISPLAY | **BACKLIGHT** | 16–255, in steps of 32 |
| BOARD | **BUILT FOR** | Read-only. The board the firmware is built for, `RP2350-TOUCH-LCD-2.8` |

`MOTOR POLES` and `MAX THROTTLE` come first, so they need no scrolling. The `-`
and `+` buttons are 44 × 44 px; hold one to repeat.

**Scrolling.** Drag the list up or down from a spot that is not a button: a
section name, a row's name or value, or the gap between rows. A drag that
starts on `-`, `+` or a toggle never scrolls; it steps or switches the value.
A button cut by the edge of the list takes a press only while at least 40 px
of it show.

Under the list, these stay in place:

- **LINK**: live packets per second and error rate. Green means packets are
  arriving, so the ESC pin is correct. Amber means more than 5 % errors.
- The note line, which says which set is in use: `SAVED SETTINGS IN USE` or
  `COMPILED DEFAULTS IN USE`. After a change it reads
  `UNSAVED - HOLD SAVE TO KEEP`.
- `HOLD TO SAVE` (150 × 48 px) and `RESET` (70 × 48 px).

**Changes apply at once. Only storing them needs the hold.** A new bitrate
rebuilds the DShot driver before the next frame. `HOLD TO SAVE` for 1 s writes
to flash. `RESET` loads the compiled defaults, the pole count and the throttle
ceiling included, into the live settings only. Leaving without saving keeps
flash unchanged.

<img src="img/settings-supply.png" width="240" alt="SETTINGS scrolled to SERVO SUPPLY, set to GP0"> <img src="img/settings-scrolled.png" width="240" alt="SETTINGS scrolled to the end: DISPLAY and BOARD">

<img src="img/settings-contrast.png" width="240" alt="SETTINGS in high contrast"> <img src="img/tester-contrast.png" width="240" alt="The tester screen in high contrast">

*High contrast applies to every screen and sets the backlight to 255 while
it is on.*

---

## SD LOG

<img src="img/log-ready.png" width="240" alt="The SD LOG screen, card ready">

Recording state and counters. `BACK` returns to `MENU`. See
[SD Logging](SD-Logging).

---

## AM32

<img src="img/am32-list.png" width="240" alt="The AM32 settings list">

Reads, edits and writes the settings of an AM32 ESC over the signal wire.
`BACK` returns to `MENU`. See [AM32 Configuration](AM32-Configuration).

---

## SERVO

<img src="img/servo-safe.png" width="240" alt="The servo screen, SAFE">

Drives one or two servos with servo pulses on the ESC and KISS pins. It opens
SAFE; `HOLD TO START` for 1 s starts the pulses at the centre, `STOP` acts on
touch. `SET` opens the profile and sweep settings. `BACK` shows
`UNPLUG THE SERVO.` before the ESC driver takes the pins back, and
`RESTART DSHOT` then returns to `MENU`. See
[Servo Tester](Servo-Tester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="The KST field list">

Reads and writes the settings of a KST or Chaservo servo over the signal wire
on the ESC pin. It searches for the servo while you power it up, reads its 32
registers, and lists 27 fields to step. `HOLD TO WRITE` for 1 s opens the
write page, and a second hold of 1 s writes, with a backup on the SD card
first. `BACK` opens the leave page, which asks you to power the servo off;
`LEAVE` then shows `UNPLUG THE SERVO.`, and `RESTART DSHOT` returns to `MENU`.
See [KST Programming](KST-Programming).

---

## Things every screen does the same way

- **A tap acts on release, not on touch.** Sliding off a button before release
  cancels it. The exceptions act on touch: `DISARM`, and `STOP` on the servo
  screen and on its SET screen.
- **A pressed button shows a pressed outline.** A control that shows no outline
  did not register the touch.
- **`-` / `+` repeat while held, and speed up.** One step on press; after
  400 ms a step every 120 ms; after 1.2 s every 60 ms; after 2.5 s every
  25 ms.
- **Writing to flash needs a 1 s hold** with a progress bar: saving settings to
  the board, and writing settings to an ESC. Starting a servo and applying a
  risky servo profile need the same 1 s hold.
