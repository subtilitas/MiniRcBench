**English** | [Deutsch](Die-Bildschirme)

# The Screens

The firmware has twelve screens. The tester is the root. `MENU` opens the
list of tools, and every tool is left with `BACK`, which returns to `MENU`.
SERVO PROGRAMMER opens PROGRAMMER, the choice of the KST and Hitec
programmers. RX SIGNALS opens the choice of the receiver signal tools.
SETTINGS also opens from the tester's pole count and ceiling; `BACK` then
returns to the tester.

```
Tester ──MENU──> MENU ──┬── ESC COMMANDS ──BACK──> MENU
   ▲                    ├── AM32 CONFIG  ──BACK──> MENU
   ▲                    ├── SERVO TESTER ──BACK──> leave page ──RESTART DSHOT──> MENU
   │                    ├── SERVO PROGRAMMER ──> PROGRAMMER ──BACK──> MENU
   │                    │    ├── KST / CHASERVO ──> KST SERVO   ──BACK──> leave page ──LEAVE, RESTART DSHOT──> MENU
   │                    │    ├── HITEC D SERIES ──> HITEC SERVO ──BACK──> leave page ──LEAVE, RESTART DSHOT──> MENU
   │                    │    └── HITEC CLASSIC  ──> HITEC SERVO, as D SERIES
   │                    │   (with no servo in programming mode the leave page opens at UNPLUG THE SERVO.)
   │                    │   (AM32 HEX, AM32 WRITE, SET: BACK goes one level up)
   │                    ├── RX SIGNALS ──> RX SIGNALS ──BACK──> MENU
   │                    │    ├── MONITOR ──> S.BUS MONITOR ──BACK──> leave page ──RESTART DSHOT──> MENU
   │                    │    └── OUTPUT     (drawn disabled: NOT BUILT)
   │                    ├── SD LOG       ──BACK──> MENU
   │                    └── SETTINGS     ──BACK──> MENU
   └──── BACK on MENU ──┘
Tester ──14 POLES or MAX 20% (disarmed)──> SETTINGS ──BACK──> Tester
```

**BACK is at the top right**, in the header band, on every screen except the
tester. The header is 36 px tall and `BACK` is 64 × 32 px. The tester is the
root and has no header. On the leave page of SERVO TESTER, KST SERVO,
HITEC SERVO and S.BUS MONITOR, `BACK` is drawn disabled, an outline with a
grey label, and a tap on it does nothing: each way on is a labelled button,
`STAY`, `NEXT SERVO`, `BACK TO SERVO`, `BACK TO MONITOR` or
`RESTART DSHOT`. The same holds on
`AM32 WRITE`, `KST WRITE` and `HITEC WRITE` while a write runs or waits for
the SD card: no `BACK` leaves a device half written.

**The leave page** is one page from the last step on a servo, or on a
receiver, to DShot on SIG again. It numbers its steps; the current one is
amber with a bar at the left edge. Before `LEAVE` the buttons read `STAY`,
which returns to the tool, and `LEAVE`. After `LEAVE` the next step is
current, and the wide button reads `RESTART DSHOT` in `LEAVE`'s place. It
and the left button, `NEXT SERVO`, `BACK TO SERVO` or `BACK TO MONITOR`,
are drawn disabled until the pin is let go; `RESTART DSHOT` then waits
1 s more, so a double tap on `LEAVE` never restarts DShot into a servo
still on the lead. A press that landed during
the wait does nothing when it lifts.

<img src="img/leave-kst.png" width="240" alt="The KST leave page before LEAVE: step 1 POWER THE SERVO OFF, THEN LEAVE., STAY and LEAVE"> <img src="img/leave-kst-released.png" width="240" alt="The KST leave page after LEAVE: step 2 current, NEXT SERVO live, RESTART DSHOT still grey">

**BACK names what it would drop.** On AM32 CONFIG's list with changes not
written, and on SERVO SET with a change waiting for `HOLD TO APPLY`, the
first `BACK` stays and puts two amber lines on the strip, for example
`2 CHANGES NOT WRITTEN.` and `BACK AGAIN DISCARDS THEM.`; a second `BACK`
drops the changes, and any other touch keeps them.

**The colours follow a table of roles.** Green is safe or done: `SAFE`,
`EDT ON`, `WRITE VERIFIED`, `SAVED`. Lime is a live reading: RPM, the throttle
fill, the servo pulse, a hold's progress bar. Red is output on or a failure:
`ARMED`, `RUNNING`, `STOP`, a tool's hold button while it is held, `REC`, a
write in doubt. Amber is a warning or the step to take now. Magenta is an alert,
such as `STALLED`. Cyan is a value changed and not yet written or applied, with
a cyan bar at its row's left edge. Blue is a setting that stays on: `LATCH`, a
toggle that is on, `PROG MODE`, servo B. Labels, captions and the buttons that
open or command something are in the text colour or grey. A chip on the strip
under a title is square, so it does not look like a button. Not every use
follows the table: lime also marks states such as SD LOG's `READY` and the
card served over USB, and many failures are amber or in the text colour, for
example `CARD ERROR` on SD LOG and AM32's `WRITE FAILED`.

---

## The tester

<img src="img/tester-armed.png" width="240" alt="The tester screen">

**Top row.** From left to right:

- the arm badge: `SAFE` (green) or `ARMED` (red);
- `DSHOT` and the DShot bitrate in kbit/s, for example `DSHOT600`, as set
  under SETTINGS → DSHOT, dim, and in the text colour once an EDT (Extended
  DShot Telemetry) enable has been sent. `DSERR` in amber replaces it when
  the display could not set up its DShot output; nothing is sent to the ESC
  then;
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
  2.55 V. While it is red, KST SERVO, HITEC SERVO and AM32 CONFIG write
  nothing and say `BOARD VOLTAGE IS RED: NOT WRITTEN.`; amber refuses
  nothing, and no arm is refused on it.
  During the last 5 s before the idle interlock disarms, `IDLE ns` in amber
  replaces the voltage and `BOARD`.

**RPM.** Mechanical RPM (revolutions per minute), computed from the eRPM
(electrical RPM) the ESC (electronic speed controller) reports and the pole
count. The `ERPM` value and the pole count, for example `14 POLES`, are shown
below it. Green digits: eRPM arrived within the last 500 ms. Dark red zero: it
did not, and the line reads `NO TELEMETRY`.

After AM32 CONFIG has read an ESC with 3D MODE on, or written 3D MODE on
(accepted by the ESC, verified or not), or after `3D ON` on
[ESC COMMANDS](#esc-commands), in this power-up, the row under the `ERPM` line
reads `3D MODE ESC: 0-50% REVERSED AT 2X` in amber, in place of the pole count
and the swipe hint, armed or not; `NO TELEMETRY` stays on its own line above
it. While a swipe on the pad is engaged, `SWIPE ACTIVE` in green takes the
row, and the note returns on release. In 3D mode the tester's 0 to 50 % runs
the motor the reverse of its set direction at twice the throttle, so 50 % is
full power reversed; above 50 % it runs the set direction from low. Keep the
propeller off. Arming is not refused on it. The note does not know that the
ESC on the pin was swapped for another one; only a verified write or a list
read with 3D MODE off, or a restart, removes it. `3D OFF` on ESC COMMANDS and
the read-only view of a layout not known leave it. See
[AM32 Configuration](AM32-Configuration#fields-with-a-note).

<img src="img/tester-3d.png" width="240" alt="The tester armed, with 3D MODE ESC: 0-50% REVERSED AT 2X in amber under the RPM readout">

**Six tiles.** Voltage, current, ESC temperature, stress, ESC status and link
quality. See [Telemetry](Telemetry).

**Throttle.** The gauge, the ceiling and the commanded percentage. The
percentage is of the full DShot range (0–2000 = 0–100 %): at a 20 % ceiling, a
full-width drag reads `20%`. The ceiling reads `MAX 20%` in the text colour
beside `THROTTLE`; with `LATCH` on the caption reads `LATCHED`. While disarmed
the bar reads `ARM TO ENABLE`.

**Pole count and ceiling open SETTINGS.** While the tester is disarmed,
`14 POLES` and `MAX 20%` are underlined, and a tap on either opens SETTINGS.
The tap disarms as `MENU` does, and `BACK` on SETTINGS returns to the tester.
While armed the underline goes and a tap on them does nothing: the pole count
lies in the throttle pad and `MAX` reaches into the bar's grab area, so the
press drives the throttle as there. While DShot restarts after a servo tool
the underline goes too and a tap does nothing, as `MENU` draws SETTINGS
disabled then. While the 3D note takes the row, only `MAX` opens SETTINGS.

**Buttons.** `HOLD TO ARM` / `DISARM`, `LATCH` (the throttle stays where the
finger leaves it; blue while on), `MENU`.

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
throttle to 0 and turns `LATCH` off.

Under the title, a strip names the two signal pins, SIG for the ESC and AUX
for the KISS wire or a second servo, and the DShot rate, for example
`SIG GP0  AUX GP1  DSHOT600`, and shows the EDT chip. The chip is read-only,
square and 12 px tall:

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

Below the strip is a list of seven rows, 40 px tall at a 42 px pitch, by
what sits on the signal pin: the ESC, a servo, then the bench. Each row has
a name on the left and its state on the right; a tool's row shows no state,
since the strip names the pins. `BACK` returns to the tester.

**Scrolling.** Six rows fit the screen. SD LOG is the sixth, whole and
tappable without a drag. The seventh, SETTINGS, shows only 6 px at first.
Drag MENU up 36 px to bring SETTINGS and its `UNSAVED` status into view; it
takes a tap only then. A 3 px scroll bar at the right edge shows the
position.

| Row | State on the right | A tap |
|---|---|---|
| **ESC COMMANDS** | nothing | Opens ESC COMMANDS: the DShot beacons, the direction and 3D mode. See [ESC COMMANDS](#esc-commands) |
| **AM32 CONFIG** | nothing | Opens the AM32 editor |
| **SERVO TESTER** | nothing | Opens the servo tester |
| **SERVO PROGRAMMER** | nothing | Opens PROGRAMMER, the choice of servo programmer. See [PROGRAMMER](#programmer) |
| **RX SIGNALS** | nothing | Opens RX SIGNALS, the choice of the receiver signal tools. See [RX SIGNALS](#rx-signals) |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a computer holds the card | Opens the logging screen |
| **SETTINGS** | `UNSAVED` while a value differs from flash; else `UPDATED` while an update's reset or carry-over of the settings is not saved over; else nothing. All amber | Opens SETTINGS |

While the signal pins are still in use, for example while DShot restarts
after the servo tester, the KST programmer or the Hitec programmer, the strip
reads `DSHOT RESTARTING ON SIG GP0` in amber. ESC COMMANDS, AM32 CONFIG,
SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS and SETTINGS are then drawn as an
outline
under their usual state, and a tap on such a row opens nothing. They act
again once DShot runs. SD LOG claims no pin and stays available.

<img src="img/menu-restarting.png" width="240" alt="MENU dragged up while DShot restarts: DSHOT RESTARTING ON SIG GP0 on the strip, ESC COMMANDS, AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS and SETTINGS drawn as outlines">

`UNSAVED` on the SETTINGS row means a value differs from flash. The save button
is on SETTINGS.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED on the SETTINGS row, with changes not yet written to flash">

`UPDATED` on the SETTINGS row means a firmware update changed the saved
settings. Either it carried settings of an older layout over, version 2, 3
or 4: the values the old layout has are kept, checked as on a save, and
the newer ones, `CURRENT SENSE` among them, are at their defaults. Or it
found a layout it does not know and discarded it: every value, the pole
count, `MAX THROTTLE`, `SIG PIN`, KISS, `SERVO SUPPLY` and `CURRENT SENSE`
among them, is at its compiled default. Check them in SETTINGS and hold
`HOLD TO SAVE`; a save that lands ends the marker, also with nothing
changed. See
[Troubleshooting](Troubleshooting#my-settings-reset-themselves). The splash
says the same at start-up (see [First Run](First-Run#powering-up)).

<img src="img/menu-updated.png" width="240" alt="UPDATED on the SETTINGS row after an update discarded the saved settings">

### ESC COMMANDS

<img src="img/esccmd.png" width="240" alt="ESC COMMANDS at rest: BEEP 1 to 5, NORMAL and REVERSED, 3D OFF and 3D ON, and the six dim lines"> <img src="img/esccmd-3d-on.png" width="240" alt="ESC COMMANDS after 3D ON: SENT 10 (3D ON), THEN 12 (SAVE). and three amber lines">

`MENU → ESC COMMANDS` sends DShot commands to the ESC on SIG. It takes no
pin: DShot keeps running while it shows. The strip reads `SIG GP0  DSHOT600`
and carries MENU's EDT chip.

- **BEEP 1 to 5**, a tap each: the ESC plays its beacon tone 1 to 5. Use it
  to hear which ESC is connected and that it takes commands.
- **NORMAL** and **REVERSED** under DIRECTION, a 1 s hold each: the spin
  direction, command 7 or 8, then 12, which saves it in the ESC.
- **3D OFF** and **3D ON** under 3D MODE, a 1 s hold each: command 9 or 10,
  then 12.

A hold fills red while held, and a bar under it fills over the second; it
acts at 1 s. A direction or 3D change takes 620 ms on the wire. The buttons
act only while the ESC is ready, otherwise they are drawn as outlines and
an amber line says why: `NO ESC ANSWERS ON SIG GP0.`,
`MOTOR TURNING: WAIT UNTIL IT STOPS.` or `ESC STARTING: WAIT 1.5 S.` (AM32
takes a command only after a second of zero throttle). After a beacon the
buttons stay grey for 260 ms, or up to 1020 ms after beacon 5. While a
command waits or goes out, `BACK` is grey too.

The lines under the buttons say what was sent, for example
`SENT 8 (REVERSED), THEN 12 (SAVE).`, or why it was not:
`NOT SENT: THE ESC STOPPED ANSWERING.` before the ESC can have acted,
`SAVE NOT SENT: POWER-CYCLE THE ESC.` when the change may sit in the ESC
unsaved, and `NOT SENT: THE ESC WAS NOT READY.` when it never started.

The ESC sends no answer to a command, so the bench cannot see the
direction. Check it on the tester with 3D off: propeller off, a strip of
tape on the bell, arm, run at 5 % for a second, disarm. On AM32 the save
plays beacon 1's tone for normal and beacon 2's for reversed. After a power
cycle of the ESC, a direction that holds was saved. After `3D ON` the tester
shows its 3D note ([The tester](#the-tester)); `3D OFF` leaves the note until
the board restarts or AM32 CONFIG reads 3D MODE off. None of these commands
has been sent to an ESC from this board yet.

<img src="img/esccmd-sending.png" width="240" alt="ESC COMMANDS while beacon 3 goes out: every button and BACK grey"> <img src="img/esccmd-no-esc.png" width="240" alt="ESC COMMANDS with no ESC answering: NO ESC ANSWERS ON SIG GP0. in amber, every button grey">

### PROGRAMMER

<img src="img/prog-chooser.png" width="240" alt="PROGRAMMER with SERVO SUPPLY at OWN: KST / CHASERVO and HITEC D SERIES with POWER THE SERVO UP AFTER THE TAP., HITEC CLASSIC as an outline with NEEDS THE CONVERTER OF WIRING PART C and AND SETTINGS > SERVO SUPPLY GP28/29. in amber"> <img src="img/prog-chooser-switched.png" width="240" alt="PROGRAMMER with SERVO SUPPLY at GP28: every entry with THE BENCH SWITCHES IT ON GP28., HITEC CLASSIC live">

`MENU → SERVO PROGRAMMER` opens PROGRAMMER, one button per servo
programmer: **KST / CHASERVO**, **HITEC D SERIES** and **HITEC CLASSIC**.
The strip reads `SIG GP0  SERVO SUPPLY OWN`, or the pin of the switched
supply. PROGRAMMER takes no pin: DShot keeps running on SIG while it shows,
so connect the servo unpowered, or let the bench power it. The tap on an
entry opens the programmer, which takes SIG on that tap.

Under each button a line says when the servo is powered:
`POWER THE SERVO UP AFTER THE TAP.` with SERVO SUPPLY at `OWN`, or
`THE BENCH SWITCHES IT ON GP28.` with the switched supply. A servo powered
before the tap would take DShot's frames as a 970 µs pulse at 1000 Hz and
move. HITEC CLASSIC needs the switched supply: under `OWN` it is drawn as an
outline, does nothing, and says why in amber,
`NEEDS THE CONVERTER OF WIRING PART C` and
`AND SETTINGS > SERVO SUPPLY GP28/29.`

`BACK` returns to `MENU`. So does `RESTART DSHOT` at the end of a
programming session, and `BACK` in a programmer before it holds SIG.

### RX SIGNALS

<img src="img/rx-chooser.png" width="240" alt="RX SIGNALS: MONITOR with S.BUS FROM A RECEIVER, ON GP1., OUTPUT as an outline with S.BUS TO A FLIGHT CONTROLLER. and NOT BUILT">

`MENU → RX SIGNALS` opens a choice of two, as PROGRAMMER: **MONITOR**,
which opens S.BUS MONITOR, and **OUTPUT**, an S.BUS output, drawn as an
outline with `NOT BUILT` under it: it does nothing. The strip names both
signal pins, `SIG GP0  AUX GP1`. RX SIGNALS takes no pin. `BACK` returns
to `MENU`, and so does `RESTART DSHOT` at the end of the monitor. See
[RX Signals](RX-Signals).

---

## SETTINGS

<img src="img/settings.png" width="240" alt="The SETTINGS screen">

Motor and wiring settings, display settings, the board, diagnostics, and the
button that writes all settings to flash. One list, in sections, scrolls under
a finger. It opens from `MENU`, or from a tap on the tester's pole count or
ceiling while disarmed; `BACK` returns to where it was opened from.

| Section | Row | What it does |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, even values. The RPM readout divides the eRPM by half of it |
| ESC TESTER | **MAX THROTTLE** | The throttle ceiling, `5%` to `100%` in steps of 5 % |
| WIRING | **SIG PIN** | SIG, the GPIO (general-purpose input/output pin) of the ESC's signal wire, with its connector pin under it: `GP0` over `J5 PIN 2`, `GP1` over `J5 PIN 1`. Only the board's two free GPIOs are offered, GP0 and GP1, so a step swaps SIG and AUX. DShot moves to the new pin at once; see the note line below |
| WIRING | **DSHOT** | The DShot rate in kbit/s: 150 / 300 / 600 / 1200. Lower it if the `LINK` error rate rises |
| WIRING | **KISS TELEM** | Whether to listen on the optional third telemetry wire, on AUX |
| WIRING | **AUX PIN** | AUX, the GPIO of the KISS wire and of the servo tester's second servo, with its J5 pin under it. Acts whether KISS TELEM is on or off. A step swaps SIG and AUX, so the two never share a pin |
| SERVO SUPPLY | **SERVO SUPPLY** | `OWN`, `GP28` or `GP29`: `OWN` means the servo has a supply of its own and the bench switches nothing; `GP28` or `GP29` means a DC-DC converter (direct-current voltage converter) for a programmed servo's supply has its enable on that pin, shown under the value: `J4 PIN 11` for GP28, `J4 PIN 12` for GP29. `OWN` by default. - and + step through the three. With `GP28` or `GP29`, KST SERVO and HITEC SERVO switch the servo's supply |
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
  12 px further; its top rows stay where they are. With KISS TELEM on, the
  step swapped SIG and AUX, so DShot now runs on the wire the ESC's KISS pad
  was on: the line reads `SWAP THE ESC'S SIGNAL AND KISS WIRES` instead.
- **LINK**: live packets per second and error rate. Green means packets are
  arriving, so the ESC pin is correct. Amber means more than 5 % errors.
- The note line: one text, the first that applies, in this order:
  1. `SAVE FAILED - FLASH REFUSED THE WRITE` (red) or
     `NOT WHILE ARMED - DISARM TO SAVE` (red), until the next save or
     leaving SETTINGS;
  2. `ADJUSTED TO THIS BOARD'S LIMITS` (amber), until the next change;
  3. `DEFAULTS LOADED - RESTART TO UNDO` (amber) after `HOLD DEFAULTS`, while a
     restart would bring the saved values back, until another change or a
     `HOLD TO SAVE`, failed or not;
  4. `UNSAVED - HOLD SAVE TO KEEP` (amber) after a change;
  5. `SAVED` (green) after a save, until the next change;
  6. `RESET BY UPDATE - CHECK AND SAVE` (amber) after an update discarded
     the saved settings, until a save lands;
  7. `CARRIED OVER BY UPDATE - CHECK, SAVE` (amber) after an update carried
     settings of an older layout over, until a save lands;
  8. `SAVED SETTINGS IN USE`, or `NOTHING SAVED: DEFAULTS IN USE` with
     nothing stored.

<img src="img/settings-carried.png" width="240" alt="SETTINGS after an update carried the saved settings over: CARRIED OVER BY UPDATE - CHECK, SAVE on the note line">

<img src="img/settings-pin.png" width="240" alt="SETTINGS after SIG PIN moved to GP1: UNPLUG ANY SERVO FROM GP1 NOW above LINK, UNSAVED on the note line">

- `HOLD TO SAVE` (150 × 46 px) and `HOLD DEFAULTS` (72 × 46 px), its label
  on two lines, `HOLD` over `DEFAULTS`, in amber.

**Changes apply at once. Only storing them needs the hold.** A new bitrate
rebuilds the DShot driver before the next frame. `HOLD TO SAVE` for 1 s writes
to flash. `HOLD DEFAULTS` for 1 s replaces the live settings with the
compiled defaults, the pole count and the throttle ceiling included; flash
is untouched. A tap does nothing, and sliding off the button cancels the
hold. A bar over the button fills while it is held, and the button turns
red, as every hold does. Leaving the screen keeps the
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
`WRITE...` opens `AM32 WRITE` on a tap: it lists each change, old and new,
and its `HOLD TO WRITE`, 1 s, writes, with a backup on the SD card first.
`HEX` shows the bytes under the title `AM32 HEX`. Settings in a layout the
tester does not know, other than 2 to 4, open read-only: the bytes in hex,
`NOT VERIFIED.`, a backup on the SD card and no write. `BACK` returns to
`MENU`, after a warning when changes are not written. See
[AM32 Configuration](AM32-Configuration).

---

## SERVO TESTER

<img src="img/servo-safe.png" width="240" alt="The servo screen, SAFE">

Drives one or two servos with servo pulses on SIG and AUX, the ESC and KISS
pins. The strip names them, `SIG GP0  50 HZ  1000-2000 US`, or
`SIG+AUX  50 HZ  1000-2000 US` with two servos. It opens SAFE;
`HOLD TO START` for 1 s starts the pulses at the centre, `STOP` acts on
touch. While SAFE with one servo, the readout reads `TWO SERVOS:` and
`SET > SERVOS`, where the second servo is switched on. With SERVO SUPPLY at
`GP28` or `GP29` the strip's second line reads `GP28 SUPPLY STAYS OFF HERE.`
in amber: the servo tester never switches the converter on, so the servo
needs a supply of its own here. `SET` opens the profile and sweep settings;
its `BACK` with a change waiting for `HOLD TO APPLY` warns first,
`1 CHANGE NOT APPLIED.`
`BACK` opens the leave page at `1 UNPLUG THE SERVO.` before the ESC driver
takes the pins back; `BACK TO SERVO` returns to the tester, and
`RESTART DSHOT` returns to `MENU`. With CURRENT SENSE at `INA219` the
row under the dial shows the servo supply's current and voltage in place of
the two-servo hint. See
[Servo Tester](Servo-Tester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="The KST field list">

Opens from `MENU → SERVO PROGRAMMER → KST / CHASERVO`, whose tap takes SIG.
Reads and writes the settings of a KST or Chaservo servo over the signal wire
on SIG, the ESC pin. It searches for the servo while you power it up, reads
its 32 registers, and lists 27 fields to step. A tap on `WRITE...` opens
the write page, and its `HOLD TO WRITE`, 1 s, writes, with a backup on the SD
card first. `BACK` opens the leave page, whose first step asks you to power
the servo off; `LEAVE` then makes `UNPLUG THE SERVO.` the current step,
`NEXT SERVO` starts a new search, and `RESTART DSHOT` returns to `MENU`.
With CURRENT SENSE at `INA219` the strip shows the current after the
supply's state, `SUPPLY ON 0.08 A`. See [KST Programming](KST-Programming).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="The Hitec field list">

<img src="img/hitec-read-wait.png" width="240" alt="HITEC SERVO for a D series servo on its own supply: SIG GP0 IS THE SERVO'S: DSHOT IS OFF. in green, POWER THE SERVO UP, THEN TAP READ. in amber, and READ">

Opens from `MENU → SERVO PROGRAMMER → HITEC D SERIES` or `HITEC CLASSIC`.
Reads and writes the settings of a Hitec D series or classic servo over the
signal wire on SIG, the ESC pin. The tap on PROGRAMMER takes the pin from
the ESC driver; the strip reads `RELEASING SIG GP0 FROM DSHOT` until DShot
has stopped. A D series servo on its own supply (SERVO SUPPLY at `OWN`)
must run before the screen reads it, so the screen then waits: the strip
reads `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.` in green and
`POWER THE SERVO UP, THEN TAP READ.` in amber. Power the servo up and tap
`READ`. With the switched supply, and for a classic servo, the bench powers
the servo itself and the screen goes on at once. While an ESC answers on
SIG the screen takes nothing and reads `AN ESC ANSWERS ON SIG GP0: UNPLUG
IT.` in amber; `BACK` then returns to `MENU`. The screen
enters the servo and reads it. A D series servo gives 21 registers and 20
parameters, 11 of them editable. One servo goes on the lead, with no pull-up
resistor. A tap on `WRITE...` opens the write page, and its `HOLD TO WRITE`, 1
s, writes, with a backup on the SD card first. `BACK` opens the leave page;
`LEAVE` then makes `UNPLUG THE SERVO.` the current step, `NEXT SERVO` starts
a new session for the same family, and `RESTART DSHOT` returns to `MENU`.
With CURRENT SENSE at `INA219` the strip shows the current after `SUPPLY ON`.
Not tested on hardware. See [Hitec Programming](Hitec-Programming).

---

## S.BUS MONITOR

<img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR with frames: the OK chip, 16 lime bars and their values">

Shows a receiver's S.BUS output on GP1: the link chip on the strip, 16
channel bars over the raw value with the value in µs, the digital channels
CH17 and CH18, the frame rate and the error counts. It takes GP1 from
DShot: as AUX in the default wiring, as SIG when SIG PIN is GP1, where it
waits while an ESC answers. `BACK` opens the leave page at
`1 UNPLUG THE RECEIVER.`; `BACK TO MONITOR` starts again with every count at
0, and `RESTART DSHOT` returns to `MENU`. See [RX Signals](RX-Signals).

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
  the board, and writing settings to an ESC or a servo. A device is written
  only from its write page: a tap on `WRITE...` or `RESTORE...` opens it, it
  lists the changes, and only its hold writes. Starting a servo and applying
  a risky servo profile need the same 1 s hold.
