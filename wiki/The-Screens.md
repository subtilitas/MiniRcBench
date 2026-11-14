**English** | [Deutsch](Die-Bildschirme)

# The Screens

The firmware has eight screens. The tester is the root. `MENU` opens the list of
tools, and every tool is left with `BACK`, which returns to `MENU`.

```
Tester ──MENU──> MENU ──┬── AM32 CONFIG  ──BACK──> MENU
   ▲                    ├── SERVO TESTER ──BACK──> unplug page ──> MENU
   │                    ├── KST SERVO    ──BACK──> leave page ──> unplug page ──> MENU
   │                    ├── HITEC SERVO  ──BACK──> leave page ──> unplug page ──> MENU
   │                    │   (on the family view BACK returns to MENU; during the entry, to the unplug page)
   │                    ├── SETTINGS     ──BACK──> MENU
   │                    └── SD LOG       ──BACK──> MENU
   └──── BACK on MENU ──┘
```

`BEEP THE ESC` is a row of `MENU` that acts and opens no screen.

**BACK is at the top right**, in the header band, on every screen except the
tester. The header is 36 px tall and `BACK` is 64 × 32 px. The tester is the
root and has no header. On the unplug page of SERVO TESTER, KST SERVO and
HITEC SERVO, `BACK` waits until the pins are let go: until then it is drawn
disabled, an outline with a grey label, and a tap on it does nothing.

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
  (see [SD Logging](SD-Logging)). `SDERR`, a recording that stopped on its
  own, is drawn on an amber chip, and stays clear of the arm badge, the RPM
  and the throttle;

  <img src="img/tester-sderr.png" width="240" alt="The tester armed, SDERR on an amber chip in the top row">

- `BOARD` and the board's own battery voltage, read through its
  200 kΩ/100 kΩ divider; at most 9.90 V. It is not the ESC's pack: that is
  the `VOLTAGE` tile. With only USB (Universal Serial Bus) power connected,
  what this reads is not measured, and the firmware cannot tell USB power
  from the cell: no pin of the board senses USB power. The voltage is amber
  below 3.50 V and red below 3.30 V, a lithium-polymer cell's usual low
  range, not measured on this board. The colour follows the value as drawn:
  `3.50V` is never amber, `3.30V` never red. It returns to the colour above
  once it is 50 mV over the threshold. With USB power alone the value can be
  low and then says nothing about a cell: below 2.50 V, implausible for a
  working cell, it is drawn in the normal text colour, and red again from
  2.55 V. No write and no arm is refused on it.
  During the last 5 s before the idle interlock disarms, `IDLE ns` in amber
  replaces the voltage and `BOARD`.

**RPM.** Mechanical RPM (revolutions per minute), computed from the eRPM
(electrical RPM) the ESC (electronic speed controller) reports and the pole
count. The `ERPM` value and the pole count are shown below it. Green digits:
eRPM arrived within the last 500 ms. Dark red zero: it did not, and the line
reads `NO TELEMETRY`.

After AM32 CONFIG has read an ESC with 3D MODE on, or written 3D MODE on and
read it back equal (`WRITE VERIFIED`), in this power-up, the row under the
`ERPM` line reads `3D MODE ESC: THROTTLE SCALE DIFFERS` in amber, in place of
the pole count and the swipe hint, armed or not; `NO TELEMETRY` stays on its own
line above it. While a swipe on the pad is engaged, `SWIPE ACTIVE` in green
takes the row, and the note returns on release. In 3D mode 50 % throttle is full
power one way, and above 50 % the motor runs the other way. Keep the propeller
off. Arming is not refused on it. The note does not know that the ESC on the pin
was swapped for another one; a read or verified write with 3D MODE off, or a
restart, removes it. See
[AM32 Configuration](AM32-Configuration#fields-with-a-note).

<img src="img/tester-3d.png" width="240" alt="The tester armed, with 3D MODE ESC: THROTTLE SCALE DIFFERS in amber under the RPM readout">

**Six tiles.** Voltage, current, ESC temperature, stress, ESC status and link
quality. See [Telemetry](Telemetry).

**Throttle.** The gauge, the ceiling and the commanded percentage. The
percentage is of the full DShot range (0–2000 = 0–100 %): at a 20 % ceiling, a
full-width drag reads `20%`. While disarmed the bar reads `ARM TO ENABLE`.

**Buttons.** `HOLD TO ARM` / `DISARM`, `HOLD` (latching throttle), `MENU`.

While a computer holds the SD card over USB, arming is refused: `HOLD TO ARM`
is drawn as an outline, and the bar reads `EJECT THE USB DRIVE TO ARM` in
amber. Eject the drive on the computer, or press `EJECT` on SD LOG, and the
button is live again (see [SD Logging](SD-Logging)).

<img src="img/tester-card-held.png" width="240" alt="The tester while a computer holds the card: HOLD TO ARM drawn as an outline, the bar reading EJECT THE USB DRIVE TO ARM">

**Touch.** The firmware reads only the touch controller's first contact
slot. Which contact the controller puts there while two fingers touch the
glass is not measured, so a press made while the throttle finger rests on
the glass may not register. Lift the throttle finger before pressing
`DISARM`. A wet screen, or a lead lying across the glass, can count as a
finger and block every tap.

---

## MENU

<img src="img/menu.png" width="240" alt="The MENU screen">

Reached with the tester's `MENU` button. Opening it **disarms**, sets the
throttle to 0 and turns `HOLD` off.

Under the title, a strip shows the ESC pin and the DShot rate, for example
`GP0  600 KBIT/S`, and the EDT chip. The chip is read-only:

| Chip | When |
|---|---|
| `EDT ON`, green | Extended DShot Telemetry frames arrive |
| `EDT OFF`, red | eRPM frames arrive and EDT frames do not |
| `NO ESC`, dim grey | no eRPM frame for 500 ms: no ESC connected, or none answering |

After an AM32 CONFIG session in which the ESC's bootloader answered, a second
line under the caption reads `POWER-CYCLE THE ESC TO RUN IT` in amber, until
an eRPM frame arrives or the board restarts
([AM32 Configuration](AM32-Configuration#leaving)).

<img src="img/menu-no-esc.png" width="240" alt="MENU with no ESC connected: the chip reads NO ESC"> <img src="img/menu-edt-off.png" width="240" alt="MENU with an ESC that sends eRPM and no EDT: the chip reads EDT OFF in red">

There is no enable button. The firmware sends the enable to every ESC that
starts answering, and repeats it every 1 s while eRPM arrives and EDT does not.
The ESC does not acknowledge the enable, so the chip shows *received frames*,
not sent commands.

Below the strip is a list of seven rows, 40 px tall at a 42 px pitch. Each
row has a name on the left and its state on the right. `BACK` returns to the
tester.

**Scrolling.** Six rows fit the screen. SETTINGS is the sixth, whole and
tappable without a drag, so its `UNSAVED` status stays in view. The seventh,
SD LOG, shows only 6 px at first. Drag MENU up 36 px to bring SD LOG into
view; it takes a tap only then. A 3 px scroll bar at the right edge shows the
position.

| Row | State on the right | A tap |
|---|---|---|
| **BEEP THE ESC** | `DISARMED`; for 350 ms after a tap, `SENT` or `REFUSED` | Sends the DShot beacon command, so the motor beeps. Use it to identify which ESC is connected |
| **AM32 CONFIG** | the ESC pin, `GP0`; `PINS BUSY` | Opens the AM32 editor |
| **SERVO TESTER** | the two signal pins, `GP0+1`; `PINS BUSY` | Opens the servo tester |
| **KST SERVO** | the ESC pin, `GP0`; `PINS BUSY` | Opens the KST programmer. See [KST Programming](KST-Programming) |
| **HITEC SERVO** | the ESC pin, `GP0`; `PINS BUSY` | Opens the Hitec programmer. See [Hitec Programming](Hitec-Programming) |
| **SETTINGS** | `UNSAVED` while a value differs from flash; else `UPDATED` while an update's reset of the settings is not saved over; else nothing. All amber. `PINS BUSY` while the pins are still restoring, and it does not open | Opens SETTINGS |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a computer holds the card | Opens the logging screen |

`PINS BUSY` means the signal pins are still in use, for example while DShot
restarts after the servo tester, the KST programmer or the Hitec programmer.
AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO and SETTINGS show it, and a
tap on such a row opens nothing.
The row reads its pins again once DShot runs. SD LOG claims no pin and stays
available.

<img src="img/menu-pins-busy.png" width="240" alt="MENU dragged up while the pins restore: PINS BUSY on AM32 CONFIG, SERVO TESTER, KST SERVO, HITEC SERVO and SETTINGS">

BEEP works only while disarmed, and opening `MENU` disarms, so it is always
allowed from here. The command takes 6 ms (6 frames at 1 kHz). The screen
repaints every 25 ms, so the row is filled for 350 ms after each press:
white with `SENT`, or amber with `REFUSED`. `REFUSED` is the answer if the ESC
is armed. Opening `MENU` disarms, so it is not expected.

<img src="img/menu-beep-flash.png" width="240" alt="BEEP accepted"> <img src="img/menu-beep-refused.png" width="240" alt="BEEP refused because the ESC is armed">

`UNSAVED` on the SETTINGS row means a value differs from flash. The save button
is on SETTINGS.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED on the SETTINGS row, with changes not yet written to flash">

`UPDATED` on the SETTINGS row means a firmware update discarded the saved
settings: every value, the pole count, `MAX THROTTLE`, `ESC PIN`, KISS,
`SERVO SUPPLY` and `CURRENT SENSE` among them, is at its compiled default.
Check them in SETTINGS and hold `HOLD TO SAVE`; a save that lands ends the
marker. The splash says the same at start-up (see
[First Run](First-Run#powering-up)).

<img src="img/menu-updated.png" width="240" alt="UPDATED on the SETTINGS row after an update discarded the saved settings">

---

## SETTINGS

<img src="img/settings.png" width="240" alt="The SETTINGS screen">

Motor and wiring settings, display settings, the board, diagnostics, and the
button that writes all settings to flash. One list, in sections, scrolls under
a finger.

| Section | Row | What it does |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, even values. The RPM readout divides the eRPM by half of it |
| ESC TESTER | **MAX THROTTLE** | The throttle ceiling, `5%` to `100%` in steps of 5 % |
| WIRING | **ESC PIN** | GPIO (general-purpose input/output pin) for the signal wire. Only the board's two free GPIOs are offered, GP0 and GP1. A step moves DShot to the new pin at once; see the note line below |
| WIRING | **DSHOT KBAUD** | 150 / 300 / 600 / 1200. Lower it if the `LINK` error rate rises |
| WIRING | **KISS TELEM** | Whether to listen on the optional third telemetry wire |
| WIRING | **KISS PIN** | GPIO for that wire. Skips the ESC pin, so the two never coincide. Reads `--` and does not act while KISS TELEM is off |
| SERVO SUPPLY | **SERVO SUPPLY** | `NONE`, `GP28` or `GP29`: whether a DC-DC converter (direct-current voltage converter) for a programmed servo's supply has its enable wired, and to which pin: GP28, J4 pin 11, or GP29, J4 pin 12. `NONE` by default. - and + step through the three. With `GP28` or `GP29`, KST SERVO and HITEC SERVO switch the servo's supply |
| SERVO SUPPLY | **CURRENT SENSE** | `NONE` or `INA219`: whether an INA219 current sensor module is on J3. `NONE` by default. A tap switches between the two. With `INA219` a caption under the name reads `FOUND`, `NOT FOUND`, or `OTHER PART` for another chip at the sensor's address. An INA219 left on another configuration by other firmware also shows `OTHER PART`: power-cycle the module. The servo tester, KST SERVO and HITEC SERVO then show the supply's current |
| DISPLAY | **CONTRAST** | `NORMAL` or `HIGH`. High contrast is for daylight |
| DISPLAY | **BACKLIGHT** | 16–255, in steps of 32 |
| BOARD | **BUILT FOR** | Read-only. The board the firmware is built for, `RP2350-TOUCH-LCD-2.8` |
| BOARD | **VERSION** | Read-only. The firmware version, for example `0.1.0` |
| DIAGNOSTICS | **LAST RESTART** | Read-only. Why the board last started: `POWER-ON`, `BROWN-OUT` (the supply dipped too low), `RUN PIN` (the reset input), `WATCHDOG`, `UPDATE` (the restart after copying a new firmware onto the board), `REBOOT` (a restart through the watchdog that another program asked for, for example picotool), `DEBUGGER`, `GLITCH` (a short supply glitch), `POWER-DOWN` or `UNKNOWN`. Amber for `BROWN-OUT`, `GLITCH` and `WATCHDOG` |
| DIAGNOSTICS | **UPTIME** | Read-only. Time since the board started, hours, minutes and seconds, for example `1:02:03`. Redrawn once a second while the row shows |

`MOTOR POLES` and `MAX THROTTLE` come first, so they need no scrolling. The `-`
and `+` buttons are 44 × 44 px; hold one to repeat.

**Scrolling.** Drag the list up or down from a spot that is not a button: a
section name, a row's name or value, or the gap between rows. A drag that
starts on `-`, `+` or a toggle never scrolls; it steps or switches the value.
A button cut by the edge of the list takes a press only while at least 40 px
of it show.

Under the list, these stay in place:

- **The ESC pin's warning**, at y 230, only while the ESC pin is not the one
  the splash named at start-up: the splash's warning for the new pin in
  amber, for example `UNPLUG ANY SERVO FROM GP1 NOW`. DShot runs on that pin
  from the step, and a servo left there, for example after the two-servo
  tester, moves. The list then ends 12 px higher, at y 227, and scrolls
  12 px further; its top rows stay where they are.
- **LINK**: live packets per second and error rate. Green means packets are
  arriving, so the ESC pin is correct. Amber means more than 5 % errors.
- The note line: one text, the first that applies, in this order:
  1. `SAVE FAILED - FLASH REFUSED THE WRITE` (red) or
     `NOT WHILE ARMED - DISARM TO SAVE` (red), until the next save or
     leaving SETTINGS;
  2. `ADJUSTED TO THIS BOARD'S LIMITS` or `KISS OFF: NEEDS A PIN OF ITS OWN`
     (amber), until the next change;
  3. `DEFAULTS LOADED - RESTART TO UNDO` (amber) after `HOLD RESET`, while a
     restart would bring the saved values back, until another change or a
     `HOLD TO SAVE`, failed or not;
  4. `UNSAVED - HOLD SAVE TO KEEP` (amber) after a change;
  5. `SAVED` (green) after a save, until the next change;
  6. `RESET BY UPDATE - CHECK AND SAVE` (amber) after an update discarded
     the saved settings, until a save lands;
  7. `SAVED SETTINGS IN USE` or `COMPILED DEFAULTS IN USE`.

<img src="img/settings-pin.png" width="240" alt="SETTINGS after ESC PIN moved to GP1: UNPLUG ANY SERVO FROM GP1 NOW above LINK, UNSAVED on the note line">

- `HOLD TO SAVE` (150 × 48 px) and `HOLD RESET` (70 × 48 px).

**Changes apply at once. Only storing them needs the hold.** A new bitrate
rebuilds the DShot driver before the next frame. `HOLD TO SAVE` for 1 s writes
to flash. `HOLD RESET` for 1 s replaces the live settings with the compiled
defaults, the pole count and the throttle ceiling included; flash is
untouched. A tap does nothing, and sliding off the button cancels the hold.
A bar over the button fills while it is held. Leaving the screen keeps the
defaults in use. The saved values load only when the board starts, so
restarting the board without `HOLD TO SAVE` returns to them.

**LAST RESTART.** The firmware reads the reason once, when it starts. A
board that restarts on its own while a motor spins and then reads
`BROWN-OUT` points at the supply. The firmware arms no watchdog, so
`WATCHDOG` does not appear with this firmware. According to the boot ROM's
source, not verified on hardware, the restart after copying a firmware file
onto the board reads `UPDATE`. Not tested on hardware: which reason a given
restart shows on the board is not recorded.

<img src="img/settings-supply.png" width="240" alt="SETTINGS scrolled to SERVO SUPPLY, set to GP29"> <img src="img/settings-scrolled.png" width="240" alt="SETTINGS scrolled to the end: BOARD and DIAGNOSTICS">

<img src="img/settings-sense.png" width="240" alt="SETTINGS: CURRENT SENSE at INA219, FOUND">

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
touch. While SAFE with one servo, the readout reads `TWO SERVOS:` and
`SET > SERVOS`, where the second servo is switched on. With SERVO SUPPLY at
`GP28` or `GP29` the strip's second line reads `GP28 SUPPLY STAYS OFF HERE.`
in amber: the servo tester never switches the converter on, so the servo
needs a supply of its own here. `SET` opens the profile and sweep settings.
`BACK` shows
`UNPLUG THE SERVO.` before the ESC driver takes the pins back, and
`RESTART DSHOT` then returns to `MENU`. With CURRENT SENSE at `INA219` the
row under the dial shows the servo supply's current and voltage in place of
the two-servo hint. See
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
With CURRENT SENSE at `INA219` the strip shows the current after the
supply's state, `SUPPLY ON 0.08 A`. See [KST Programming](KST-Programming).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="The Hitec field list">

Reads and writes the settings of a Hitec D series or classic servo over the
signal wire on the ESC pin. It opens on the family view: `D SERIES` acts, and
`CLASSIC` acts with SERVO SUPPLY at `GP28` or `GP29`; under `NONE` it is drawn
disabled with `NEEDS THE CONVERTER OF WIRING PART C` and
`AND SETTINGS > SERVO SUPPLY GP28/29.`: the converter of
[wiring](Wiring) part C comes first, then the setting. Under `NONE`, D SERIES
reads `POWER THE SERVO UP BEFORE THE TAP.`. The screen enters the servo
and reads it. A D series servo gives 21 registers and 20 parameters, 11 of
them editable. One servo goes on the lead, with no pull-up resistor.
`HOLD TO WRITE` for 1 s opens the write page, and a second hold of 1 s writes,
with a backup on the SD card first. `BACK` opens the leave page; `LEAVE` then
shows `UNPLUG THE SERVO.`,
and `RESTART DSHOT` returns to `MENU`. With CURRENT SENSE at `INA219` the
strip shows the current after `SUPPLY ON`. Not tested on hardware. See
[Hitec Programming](Hitec-Programming).

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
