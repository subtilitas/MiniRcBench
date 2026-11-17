**English** | [Deutsch](Die-Bildschirme)

# The Screens

The firmware has sixteen screens. `MENU` is the root and the first screen
after the splash: six rows, with no `BACK`. Its first row, `ESC TESTER`,
opens the tester, whose `MENU` button returns to `MENU`. The next three
rows each open a hub, a list of the tools for one kind of device: ESC
PROGRAM the ESC tools, SERVO the servo tools, RECEIVER the receiver signal
tools. Every tool is two taps from `MENU`. A tool is left with `BACK`,
which returns to the hub that opened it, and the hub's `BACK` returns to
`MENU`.
SETUP also opens from the tester's pole count and ceiling; `BACK` then
returns to the tester. DShot runs only while `ESC TESTER` or `ESC COMMANDS` is
open, or SETUP over the tester. Everywhere else, `MENU` included and from
power-up on, the ESC signal pin is held low and nothing is sent.

```
Splash ──3 s──> MENU (the root, no BACK)
MENU ──┬── ESC TESTER ──> Tester ──MENU──> MENU
       │                    └── 14 POLES or MAX 20% (disarmed) ──> SETUP ──BACK──> Tester
       ├── ESC PROGRAM > ──> ESC PROGRAM ──BACK──> MENU
       │    ├── ESC COMMANDS ──BACK──> ESC PROGRAM
       │    ├── ESC SETTINGS ──> ESC SETTINGS ──BACK──> ESC PROGRAM
       │    │   (a Bluejay or BLHeli_S list: BACK──> LEAVE sent──> ESC PROGRAM)
       │    │   (elsewhere after a Bluejay or BLHeli_S answer: BACK──> leave page ──LEAVE, ESC PROGRAM──> ESC PROGRAM)
       │    └── USB BRIDGE ──> USB BRIDGE ──BACK──> ESC PROGRAM
       │        (once the line is bound: BACK──> leave page ──ESC PROGRAM──> ESC PROGRAM)
       ├── SERVO > ──> SERVO ──BACK──> MENU
       │    ├── SERVO TESTER ──BACK──> leave page ──SERVO──> SERVO
       │    ├── ANGLE METER ──BACK──> SERVO   (ANGLE SET, FLIP CAL: BACK goes one level up)
       │    ├── KST / CHASERVO ──> KST SERVO   ──BACK──> leave page ──LEAVE, SERVO──> SERVO
       │    ├── HITEC D SERIES ──> HITEC SERVO ──BACK──> leave page ──LEAVE, SERVO──> SERVO
       │    └── HITEC CLASSIC  ──> HITEC SERVO, as D SERIES
       │   (with no servo in programming mode the leave page opens at UNPLUG THE SERVO.)
       │   (ESC HEX, ESC WRITE, SET: BACK goes one level up)
       ├── RECEIVER > ──> RECEIVER ──BACK──> MENU
       │    ├── MONITOR ──> S.BUS MONITOR ──BACK──> leave page ──RECEIVER──> RECEIVER
       │    └── OUTPUT  ──> S.BUS OUTPUT  ──BACK──> leave page ──RECEIVER──> RECEIVER   (SET: BACK goes one level up)
       ├── LOGS       ──BACK──> MENU
       └── SETUP     ──BACK──> MENU
```

**BACK is at the top right**, in the header band, on every screen except
`MENU` and the tester. The header is 36 px tall and `BACK` is 64 × 32 px.
`MENU` is the root: its header reads `MENU` and has no `BACK`, since
nothing lies behind it. The tester has no header; its `MENU` button is its
way back. On the leave page of SERVO TESTER, KST SERVO, HITEC
SERVO, S.BUS MONITOR and ESC SETTINGS, `BACK` is drawn disabled, an outline with
a grey label, and a tap on it does nothing: each way on is a labelled button,
`STAY`, `NEXT SERVO`, `BACK TO MONITOR` or the hub's title. The
same holds on `ESC WRITE`, `KST WRITE` and `HITEC WRITE` while a write runs or
waits for the SD card: no `BACK` leaves a device half written.

**The leave page** is one page from the last step on a servo, on a receiver, or
on an ESC whose Bluejay or BLHeli_S bootloader answered, back to the hub with
the signal pin held low. It numbers its steps; the current one is amber with a
bar at the left edge. Before `LEAVE` the buttons read `STAY`, which returns to
the tool, and `LEAVE`. After `LEAVE` the next step is current, and the wide
button reads the hub's title, `SERVO`, `RECEIVER` or `ESC PROGRAM`, in
`LEAVE`'s place; the last step reads `BACK TO` and that title. It and the left
button, `NEXT SERVO` or `BACK TO MONITOR` (none on ESC SETTINGS or USB
BRIDGE), are drawn disabled until the pin is let go; the hub's title then
waits 1 s more, so a double tap on `LEAVE` never ends the claim. A press that
landed during the wait does nothing when it lifts. The last button holds the
pin low and starts no DShot: a servo left on SIG meets DShot at the next
`ESC TESTER` or `ESC COMMANDS`, which the unplug step names.

<img src="img/leave-kst.png" width="240" alt="The KST leave page before LEAVE: step 1 POWER THE SERVO OFF, THEN LEAVE., STAY and LEAVE"> <img src="img/leave-kst-released.png" width="240" alt="The KST leave page after LEAVE: step 2 current, NEXT SERVO live, SERVO still grey">

**BACK names what it would drop.** On ESC SETTINGS' list with changes not
written, and on SERVO SET with a change waiting for `HOLD TO APPLY`, the
first `BACK` stays and puts two amber lines on the strip, for example
`2 CHANGES NOT WRITTEN.` and `BACK AGAIN DISCARDS THEM.`; a second `BACK`
drops the changes, and any other touch keeps them.

**The colours follow a table of roles.** Green is safe or done: `SAFE`,
`EDT ON`, `WRITE VERIFIED`, `SAVED`. Lime is a live reading: RPM, the throttle
fill, the servo pulse, a hold's progress bar. Red is output on or a failure:
`ARMED`, `RUNNING`, `STOP`, a tool's hold button while it is held, `REC`, a
write in doubt. Amber is a warning or the step to take now. Magenta is an alert,
such as `STALLED`. Blue is a setting that stays on: `LATCH`, a
toggle that is on, `PROG MODE`, servo B. Cyan is the accent, as on rcbench,
and has four uses. It is the way back: `BACK` in a header and the tester's
`MENU` button are cyan with a dark label, and a `BACK` that does nothing is
drawn disabled, not cyan. It is the mark `>` on a `MENU` row that opens a
hub. It is a setting of the bench, as cyan text: the values and section
captions of SETUP, the values of SERVO SET and OUTPUT SET, LENGTH on ANGLE
SETUP. And it is a value changed and not yet written or applied: dark text
on a cyan fill, with a cyan bar at its row's left edge. A value read from an
ESC or a servo is in the text colour. Labels, captions and the buttons that
open or command something are in the text colour or grey. A button has
rounded corners. A chip on the strip under a title is square, so it does not
look like a button; the tester's arm badge and its `SDERR` chip are square
too. Not every use follows the table: lime also marks states such as LOGS'
`READY` and the card served over USB, and many failures are amber or in the
text colour, for example `CARD ERROR` on LOGS and AM32's `WRITE FAILED`.

**The normal theme takes its colours from rcbench,** the larger bench of the
same owner: a near-black screen with a blue cast and blue-grey neutrals. A
label on a coloured fill, such as `ARMED`, `STOP` or `PROG MODE`, is near
black. A header ends in a thin rule. A hint, such as `SWIPE = COARSE`, is in
the dim colour of the labels; a disabled label and a read-only value are in
the fainter colour of the frames. A chip with no state to report, `NO ESC`,
`PORT CLOSED` or `MOVING`, is a frame with a dim label and no fill. The `-`
and `+` under a list and Hitec's `REPAIR` look like every other button that
commands something. Where these pages say "grey", the colour is one of the
two blue-grey neutrals. The colours are computed and not seen on the panel:
how the panel shows them is not measured. High contrast keeps its own
colours. Its frames and labels are near black, so a button that does not
act has a dashed frame there, with its rounded corners whole, and a slider
that does not act a hollow thumb. Cyan is a dark cyan there: `BACK` has a
white label, and a changed value is white text on a dark cyan fill.

---

## The tester

<img src="img/tester-armed.png" width="240" alt="The tester screen">

`MENU`'s first row, `ESC TESTER`, opens the tester. It opens **disarmed**,
at throttle 0, with `LATCH` off, every time. It has no header: its `MENU`
button, at the bottom right, returns to `MENU` and first disarms, sets the
throttle to 0 and turns `LATCH` off. While the signal pins are in use, the
`ESC TESTER` row is drawn as an outline and a tap on it opens nothing.

DShot starts when the tester opens: the ESC hears the EDT enable, then
throttle 0, and `HOLD TO ARM` waits 1.5 s after DShot started, so the ESC has
seen DShot before it can arm. Meanwhile `HOLD TO ARM` is drawn as an outline
and the bar reads `DSHOT STARTING: WAIT 1.5 S` in amber; a press that lands
then does nothing, so lift and press again once the button is live. The
earliest arm is 2.5 s after the tester opens. If the board has no free
hardware to send DShot with (two free state machines of one PIO block, the
chip's programmable input/output unit; `DSERR` in the status bar), the bar
reads `DSHOT COULD NOT START` in amber and `HOLD TO ARM` stays an outline.
`MENU` stops DShot: the ESC hears throttle 0 for at least 250 ms after the
last throttle, then nothing, and the pin is held low.

**Top row.** From left to right:

- the arm badge: `SAFE` (green) or `ARMED` (red);
- `DSHOT` and the DShot bitrate in kbit/s, for example `DSHOT600`, as set
  under SETUP → DSHOT, dim, and in the text colour once an EDT (Extended
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
  2.55 V. While it is red, KST SERVO, HITEC SERVO and ESC SETTINGS write
  nothing and say `BOARD VOLTAGE IS RED: NOT WRITTEN.`; amber refuses
  nothing, and no arm is refused on it.
  During the last 5 s before the idle interlock disarms, `IDLE ns` in amber
  replaces the voltage and `BOARD`.

**RPM.** Mechanical RPM (revolutions per minute), computed from the eRPM
(electrical RPM) the ESC (electronic speed controller) reports and the pole
count. The `ERPM` value and the pole count, for example `14 POLES`, are shown
below it. Green digits: eRPM arrived within the last 500 ms. Dark red zero: it
did not, and the line reads `NO TELEMETRY`. The row under it, right, is the
swipe hint: `SWIPE = THROTTLE`, or `SWIPE ACTIVE` while a swipe moves the
throttle.

**RPM/V and the kV estimate.** While armed, the `ERPM` line also reads
`RPM/V`, the RPM divided by the ESC's supply voltage, the value of the
`VOLTAGE` tile: `ERPM 156800  RPM/V 1400`. The row below reads `KV EST` in
place of the pole count: an estimate of the motor's kV, its speed constant in
RPM per volt with no load, worked out as RPM × 2000 / (voltage × throttle),
with the throttle as sent, 0 to 2000. It is an estimate under two
assumptions: the ESC's duty equals the throttle, and the motor runs with no
propeller.

- `KV EST 1758`: the estimate, rounded to a whole number.
- `KV EST 75%+`: the throttle is below 75 %, where the estimate does not
  show. At the default `MAX 20%` it never shows; raise `MAX` in SETUP
  for the reading, propeller off.
- `KV EST --`: no figure. An input is missing, the throttle moved by more
  than 1 % within the last second, or the motor is still speeding up.

Both figures need eRPM within the last 500 ms, an RPM above 0 and a voltage
above 0.5 V from KISS (within 500 ms) or EDT (within 1000 ms); otherwise the
line reads `RPM/V --` and the row `KV EST --`. The estimate also needs the
throttle at 75 % or more, within 1 % (20 of 2000) of one value for 1 s, so that
a finger resting on the bar does not hold it off, a motor that has reached its
speed (the smoothed eRPM within 0.5 % of the last reply; once met, it holds
until the throttle moves out of its band, a condition fails, or the smoothed
eRPM is more than 3 % from the last reply or the reply is 0, so noisy replies
do not flicker the figure and a stall shows `KV EST --` at once), and no 3D
note; the 3D note takes the row. Both figures stop at 99999. Neither is kept
after the disarm: disarmed, the line reads `ERPM` alone and the row `14 POLES`.

What the estimate cannot know:

- **A propeller.** With one on, it reads low by the load's share.
- **The ESC's duty.** At 75 % on AM32 2.22 the design note works out
  × 0.92 to × 1.08 of the rated kV for the SINE STARTUP and MIN DUTY
  settings it covers, with RUNNING LEVEL 10 and on a target built without
  startup boost, from AM32's source; not measured. For Bluejay, BLHeli_S and
  BLHeli_32 the throttle-to-duty map is not verified.
- **3D mode it was not told of.** An ESC set to 3D by another tool, or in an
  earlier power-up of the bench, shows no 3D note and gives a wrong figure
  with no warning: on AM32 at 75 % the motor runs at a duty of about 50 %,
  and the estimate reads about × 0.67 of the rated kV.
- **Losses.** The voltage drops in the ESC and the wiring, friction and iron
  losses lower the speed; by how much is not known.
- **The pole count.** A wrong `MOTOR POLES` scales both figures: 12 set on a
  14-pole motor reads 16.7 % high.

Neither figure has been measured on a motor from this board.

<img src="img/tester-kv.png" width="240" alt="The tester armed at 79 %: ERPM 156800  RPM/V 1400 and KV EST 1758 under the RPM readout">

After ESC SETTINGS has read an ESC with 3D MODE on, or a Bluejay or
BLHeli_S ESC with DIRECTION at 3D or 3D REVERSED, or written 3D MODE on
(accepted by the ESC, verified or not) or DIRECTION at 3D (verified, or
left half written), or after `3D ON` on
[ESC COMMANDS](#esc-commands), in this power-up, the row under the `ERPM` line
reads `3D MODE ESC: 0-50% REVERSED AT 2X` in amber, in place of the pole count
and the swipe hint, armed or not; `NO TELEMETRY` stays on its own line above
it. While a swipe on the pad is engaged, `SWIPE ACTIVE` in green takes the
row, and the note returns on release; `TOUCH NOT ANSWERING` (Touch, below)
takes it too, until the controller answers. In 3D mode the tester's 0 to 50 %
runs the motor the reverse of its set direction at twice the throttle, so 50 %
is full power reversed; above 50 % it runs the set direction from low. Keep the
propeller off. Arming is not refused on it. The note does not know that the
ESC on the pin was swapped for another one; only a verified write or a list
read with 3D MODE off or DIRECTION at NORMAL or REVERSED, or a restart,
removes it. `3D OFF` on ESC COMMANDS, the read-only view of a layout not
known and a Bluejay or BLHeli_S page whose settings marker is wrong leave
it. See
[AM32 Configuration](AM32-Configuration#fields-with-a-note).

<img src="img/tester-3d.png" width="240" alt="The tester armed, with 3D MODE ESC: 0-50% REVERSED AT 2X in amber under the RPM readout">

**Six tiles.** Voltage, current, ESC temperature, stress, ESC status and link
quality. See [Telemetry](Telemetry).

**Throttle.** The gauge, the ceiling and the commanded percentage. The
percentage is of the full DShot range (0–2000 = 0–100 %): at a 20 % ceiling, a
full-width drag reads `20%`. The ceiling reads `MAX 20%` in the text colour
beside `THROTTLE`; with `LATCH` on the caption reads `LATCHED`. While disarmed
the bar reads `ARM TO ENABLE`.

**Pole count and ceiling open SETUP.** While the tester is disarmed,
`14 POLES` and `MAX 20%` are underlined, and a tap on either opens SETUP. The
tap disarms as the `MENU` button does, and `BACK` on SETUP returns to the
tester. While armed the underline goes, `KV EST` takes the pole count's place,
and a tap on them does nothing: the pole count lies in the throttle pad and
`MAX` reaches into the bar's grab area, so the press drives the throttle as
there. While another tool holds the signal pins the underline goes too and a
tap does nothing, as `MENU` draws SETUP disabled then; with the tester
open no tool can take them. While the 3D note takes
the row, only `MAX` opens SETUP. Disarmed, the hint `SWIPE = THROTTLE` is
in the dim label colour; armed, it takes the text colour.

<img src="img/tester-disarmed.png" width="240" alt="The tester disarmed with telemetry: SAFE in green, 14 POLES and MAX 20% underlined, SWIPE = THROTTLE dim, ARM TO ENABLE in the bar">

**Buttons.** `HOLD TO ARM` / `DISARM`, `LATCH` (the throttle stays where the
finger leaves it; blue while on), `MENU`.

While a computer holds the SD card over USB, arming is refused: `HOLD TO ARM`
is drawn as an outline, and the bar reads `EJECT THE USB DRIVE TO ARM` in
amber. Eject the drive on the computer, or press `EJECT` on LOGS, and the
button is live again (see [SD Logging](SD-Logging)). Arming is refused the
same way for 1.5 s after DShot starts, and while the touch controller does
not answer.

<img src="img/tester-card-held.png" width="240" alt="The tester while a computer holds the card: HOLD TO ARM drawn as an outline, the bar reading EJECT THE USB DRIVE TO ARM">

**Touch.** The firmware reads only the touch controller's first contact
slot. Which contact the controller puts there while two fingers touch the
glass is not measured, so a press made while the throttle finger rests on
the glass may not register. Lift the throttle finger before pressing
`DISARM`. A wet screen, or a lead lying across the glass, can count as a
finger and block every tap.

When the touch controller has missed four reads in a row and has not
answered for 100 ms or more, `TOUCH NOT ANSWERING` in amber replaces the
swipe hint; `NO TELEMETRY` keeps its line. While it shows, the tester cannot
be armed, `HOLD TO ARM` is drawn as an outline, and an armed tester
disarms: the throttle goes to zero and `LATCH` goes off, as when the idle
interlock disarms. The note goes when the controller answers again; the
tester stays disarmed. SERVO TESTER and S.BUS OUTPUT stop on the same rule
(below).

Not measured: whether the touch controller keeps answering with no finger
on the glass for minutes. If it does not, this note would appear and a
motor latched with `LATCH` would stop shortly after the finger lifts. The
bench check is listed in STATUS.md in the repository, open item 23.

<img src="img/tester-touch-lost.png" width="240" alt="The tester with TOUCH NOT ANSWERING in amber under the RPM readout">

---

## MENU

<img src="img/menu.png" width="240" alt="The MENU screen">

The first screen after the splash, and the root: the header reads `MENU`
and has no `BACK`. The tester's `MENU` button returns here and first
**disarms**, sets the throttle to 0 and turns `LATCH` off. DShot is off on
`MENU`: the ESC signal pin is held low, and `MENU` shows no EDT chip. The
chip is on [ESC COMMANDS](#esc-commands), which runs DShot.

Under the title, a strip names the two signal pins, SIG for the ESC and AUX
for the KISS wire or a second servo, and the DShot rate the tester uses, for
example `SIG GP0  AUX GP1  DSHOT600`.

After a session of ESC SETTINGS, USB BRIDGE or S.BUS OUTPUT in which an
ESC's bootloader may still wait, a second line under the caption reads
`POWER-CYCLE THE ESC TO RUN IT` in amber, until `START DSHOT` is tapped on the
start step below, the last button of the tool's leave page is tapped, an eRPM
frame arrives, or the board restarts
([AM32 Configuration](AM32-Configuration#leaving)). A USB BRIDGE visit that
ends with no leave page, before the line was used, clears it unless a byte
came from the ESC or the line already stood when the visit began.

While that line stands, ESC TESTER and ESC COMMANDS open on a start step
with DShot off, because a waiting bootloader can read DShot as a command:
`1 POWER-CYCLE THE ESC.` over `ITS BOOTLOADER MAY STILL WAIT` and
`AND READ DSHOT AS A COMMAND.` Power-cycle the ESC, then tap `START DSHOT`,
which is grey for the first second. The tap clears the line on MENU and
starts DShot; on the tester, `HOLD TO ARM` then waits 1.5 s as after any
open. `MENU` on the tester and `BACK` on ESC COMMANDS leave the step at once
with DShot still off and the line kept, so the next open shows the step
again. Nothing else on the screen works on the step. This is tested on the
host only.

<img src="img/tester-start-step.png" width="240" alt="ESC TESTER on the start step: POWER-CYCLE THE ESC., START DSHOT grey, MENU"> <img src="img/esccmd-start-step.png" width="240" alt="ESC COMMANDS on the start step: the step, START DSHOT grey, BACK">

Below the strip is a list of six rows, 40 px tall at a 42 px pitch:
`ESC TESTER` first, then one row for each kind of device on the signal pin,
the ESC, a servo, a receiver, then the log and the settings. Each row has a
name on the left. `LOGS` and `SETUP` show their state on the right.
`ESC PROGRAM`, `SERVO` and `RECEIVER` show `>` there instead: each opens a
hub, a list of tools, one level down.

**No scrolling.** The six rows fit the screen: every row is whole and takes
a tap at once. MENU has no scroll bar, and a drag moves nothing.

| Row | On the right | A tap |
|---|---|---|
| **ESC TESTER** | nothing | Opens the tester, disarmed, at throttle 0, with `LATCH` off. See [The tester](#the-tester) |
| **ESC PROGRAM** | `>` | Opens ESC PROGRAM, the hub of the ESC tools: ESC COMMANDS, ESC SETTINGS and USB BRIDGE. See [ESC PROGRAM](#esc-program) |
| **SERVO** | `>` | Opens SERVO, the hub of the servo tools: SERVO TESTER, ANGLE METER, KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC. See [SERVO](#servo) |
| **RECEIVER** | `>` | Opens RECEIVER, the hub of the receiver signal tools: MONITOR and OUTPUT. See [RECEIVER](#receiver) |
| **LOGS** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a computer holds the card | Opens the logging screen |
| **SETUP** | `UNSAVED` while a value differs from flash; else `UPDATED` while an update's reset or carry-over of the settings is not saved over; else nothing. All amber | Opens SETUP |

Every tool is two taps from MENU: the hub's row, then the tool's entry.
`BACK` on a tool returns to its hub, and `BACK` on the hub to `MENU`.

While the signal pins are still in use, the strip reads in amber
`SETTING SIG GP0 LOW` for the moment the pin is set low after a leave page,
or `RELEASING SIG GP0` while a tool lets the pin go, as the USB bridge does
for up to 250 ms. ESC TESTER and SETUP are then drawn as an outline under
their usual state, and a tap on such a row opens nothing. They act again once
the pin is free. LOGS claims no pin and stays available. ESC PROGRAM, SERVO
and RECEIVER open as always; the hub draws its entries as outlines
meanwhile, with the same amber line as its strip's second line.

<img src="img/menu-parking.png" width="240" alt="MENU while the pin is set low: SETTING SIG GP0 LOW on the strip, ESC TESTER and SETUP drawn as outlines, ESC PROGRAM, SERVO and RECEIVER filled, each with its mark">

`UNSAVED` on the SETUP row means a value differs from flash. The save button
is on SETUP.

<img src="img/menu-unsaved.png" width="240" alt="UNSAVED on the SETUP row, with changes not yet written to flash">

`UPDATED` on the SETUP row means a firmware update changed the saved
settings. Either it carried settings of an older layout over, version 2, 3,
4 or 5: the values the old layout has are kept, checked as on a save, and
the newer ones, `CURRENT SENSE` and the angle meter's setup and calibration
among them, are at their defaults. Or it
found a layout it does not know and discarded it: every value, the pole
count, `MAX THROTTLE`, `SIG PIN`, KISS, `SERVO SUPPLY` and `CURRENT SENSE`
among them, is at its compiled default. Check them in SETUP and hold
`HOLD TO SAVE`; a save that lands ends the marker, also with nothing
changed. See
[Troubleshooting](Troubleshooting#my-settings-reset-themselves). The splash
says the same at start-up (see [First Run](First-Run#powering-up)).

<img src="img/menu-updated.png" width="240" alt="UPDATED on the SETUP row after an update discarded the saved settings">

### ESC PROGRAM

<img src="img/esc-chooser.png" width="240" alt="ESC PROGRAM: ESC COMMANDS, ESC SETTINGS and USB BRIDGE, each with its two lines">

`MENU → ESC PROGRAM` opens a hub of three entries, one button each with two
lines under it: **ESC COMMANDS**, which sends DShot commands, with
`BEEP 1 TO 5, DIRECTION, 3D MODE.` and `SENT AS DSHOT COMMANDS.` under it; **ESC
SETTINGS**, which opens the ESC settings screen, AM32, Bluejay and BLHeli_S read
and written; and **USB BRIDGE**, which lets a desktop ESC tool reach the ESC,
with `A DESKTOP ESC TOOL OVER USB.` and `MSP, 4-WAY AND PASSTHROUGH.` under it.
The strip names the signal pin, `SIG GP0`. ESC PROGRAM takes no pin; the tap on
ESC SETTINGS or USB BRIDGE takes SIG, and ESC COMMANDS takes none. `BACK`
returns to `MENU`; `BACK` on any of the three tools returns to ESC PROGRAM.
While the signal pin is still in use the three entries are outlines and the
strip's second line reads `RELEASING SIG GP0` or `SETTING SIG GP0 LOW` in
amber. See [ESC COMMANDS](#esc-commands) below,
[AM32 Configuration](AM32-Configuration) and [USB Bridge](USB-Bridge).

### ESC COMMANDS

<img src="img/esccmd.png" width="240" alt="ESC COMMANDS at rest: BEEP 1 to 5, NORMAL and REVERSED, 3D OFF and 3D ON, and the six dim lines"> <img src="img/esccmd-3d-on.png" width="240" alt="ESC COMMANDS after 3D ON: SENT 10 (3D ON), THEN 12 (SAVE). and three amber lines">

`MENU → ESC PROGRAM → ESC COMMANDS` sends DShot commands to the ESC on SIG.
It takes no pin. DShot starts on SIG when it opens and stops when `BACK`
returns to ESC PROGRAM; the pin is then held low. The strip reads
`SIG GP0  DSHOT600` and carries the EDT chip: `EDT ON` in green while
Extended DShot Telemetry frames arrive, `EDT OFF` in red while eRPM frames
arrive and EDT frames do not, and `NO ESC`, a frame with a dim label, while no
eRPM frame has arrived for 500 ms. The firmware sends the EDT enable itself;
the chip shows received frames.

- **BEEP 1 to 5**, a tap each: the ESC plays its beacon tone 1 to 5. Use it
  to hear which ESC is connected and that it takes commands.
- **NORMAL** and **REVERSED** under DIRECTION, a 1 s hold each: the spin
  direction, command 7 or 8, then 12, which saves it in the ESC.
- **3D OFF** and **3D ON** under 3D MODE, a 1 s hold each: command 9 or 10,
  then 12.

A hold fills red while held, and a bar under it fills over the second; it
acts at 1 s. A direction or 3D change takes 620 ms on the wire. The buttons
act while the ESC is ready, otherwise they are drawn as outlines and an
amber line says why: `NO ESC ANSWERS ON SIG GP0.`,
`MOTOR TURNING: WAIT UNTIL IT STOPS.` or `ESC STARTING: WAIT 1.5 S.` (AM32
takes a command only after a second of zero throttle). For the first 1.5 s
after DShot starts, no answer reads `DSHOT STARTING ON SIG GP0.` instead: the
ESC needs that time to see DShot. A press while the
board is still sending something else, such as the 260 ms to 1020 ms
pause after a beacon, waits: it goes out once the wire is free, or is
dropped after 2 s, and the lines then read
`NOT SENT: THE ESC WAS NOT READY.`. While a command of the page waits or
goes out, the buttons and `BACK` are grey, and a press that starts on them
then does nothing when it lifts.

The lines under the buttons say what was sent, for example
`SENT 8 (REVERSED), THEN 12 (SAVE).`, or why it was not:
`NOT SENT: THE ESC STOPPED ANSWERING.` before the ESC can have acted,
`SAVE NOT SENT: POWER-CYCLE THE ESC.` when the change may sit in the ESC
unsaved, and `NOT SENT: THE ESC WAS NOT READY.` when it never started.
After 3D ON the lines say that the tester shows its 3D note and that a
restart of the board, or ESC SETTINGS reading 3D MODE off, clears it.

The ESC sends no answer to a command, so the bench cannot see the
direction. Check it on the tester with 3D off: propeller off, a strip of
tape on the bell, arm, run at 5 % for a second, disarm. On AM32 the save
plays beacon 1's tone for normal and beacon 2's for reversed. After a power
cycle of the ESC, a direction that holds was saved. After `3D ON` the tester
shows its 3D note ([The tester](#the-tester)); `3D OFF` leaves the note until
the board restarts or ESC SETTINGS reads 3D MODE off. None of these commands
has been sent to an ESC from this board yet.

<img src="img/esccmd-sending.png" width="240" alt="ESC COMMANDS while beacon 3 goes out: every button and BACK grey"> <img src="img/esccmd-no-esc.png" width="240" alt="ESC COMMANDS with no ESC answering: NO ESC ANSWERS ON SIG GP0. in amber, every button grey">

### SERVO

<img src="img/servo-hub.png" width="240" alt="SERVO at the top: SERVO TESTER, ANGLE METER and KST / CHASERVO whole, each with its line, HITEC D SERIES cut at the screen's edge, the scroll bar at the right"> <img src="img/servo-hub-scrolled.png" width="240" alt="SERVO dragged to its end with SERVO SUPPLY at OWN: KST / CHASERVO and HITEC D SERIES with POWER THE SERVO UP AFTER THE TAP., HITEC CLASSIC as an outline with NEEDS THE CONVERTER OF WIRING PART C and AND SETUP > SERVO SUPPLY GP28/29. in amber"> <img src="img/servo-hub-switched.png" width="240" alt="SERVO at its end with SERVO SUPPLY at GP28: the three programmers with THE BENCH SWITCHES IT ON GP28., HITEC CLASSIC live">

`MENU → SERVO` opens SERVO, the hub of the five servo tools, in the order of
use: **SERVO TESTER** drives a servo, **ANGLE METER** measures a surface's
throw, and **KST / CHASERVO**, **HITEC D SERIES** and **HITEC CLASSIC** program
a servo. The strip reads `SIG GP0  AUX GP1  SERVO SUPPLY OWN`, or the pin of the
switched supply. SERVO takes no pin, and DShot is off while it shows: SIG is
held low. A second line on the strip reads `NO ESC CHECK: UNPLUG ANY ESC FIRST.`
in amber: with DShot off the bench cannot see an ESC on SIG, and a servo pulse
is a valid throttle for one. Unplug any ESC before you tap an entry. The tap on
an entry opens the tool, and a tool that needs SIG takes it on that tap.

**Scrolling.** Three entries fit the screen. SERVO TESTER, ANGLE METER and
KST / CHASERVO are whole when SERVO opens. HITEC D SERIES shows only its
top 18 px and takes no tap there: it shows that the list goes on. Drag
SERVO up to reach the two Hitec entries; the list moves 146 px at most. A
3 px scroll bar at the right edge shows the position. A tool that returns
to SERVO finds the list where it was left.

Under each button a line says what the tool does or which servos it programs.
Under the three programmers a line says when the servo is powered:
`POWER THE SERVO UP AFTER THE TAP.` with SERVO SUPPLY at `OWN`, or
`THE BENCH SWITCHES IT ON GP28.` with the switched supply. A servo powered
before the tap would see the programmer's first frames on its tap. HITEC CLASSIC
needs the switched supply: under `OWN` it is drawn as an outline, does nothing,
and says why in amber, `NEEDS THE CONVERTER OF WIRING PART C` and
`AND SETUP > SERVO SUPPLY GP28/29.`

| Entry | Line under it | Opens |
|---|---|---|
| **SERVO TESTER** | `ONE SERVO ON SIG, OR TWO WITH AUX.` | [Servo Tester](Servo-Tester) |
| **ANGLE METER** | `INCIDENCE, DECALAGE, DIHEDRAL, THROW` | [Angle Meter](Angle-Meter) |
| **KST / CHASERVO** | the power line | [KST Programming](KST-Programming) |
| **HITEC D SERIES** | `D485HW, D645MW AND THE OTHERS`, the power line | [Hitec Programming](Hitec-Programming) |
| **HITEC CLASSIC** | `HS-5XXX, HS-7XXX`, the power line or the two amber lines | [Hitec Programming](Hitec-Programming#classic-servos) |

`BACK` returns to `MENU`. `SERVO`, the last button of the leave page at the
end of a session of the servo tester or a programmer, and `BACK` in a tool
before it holds SIG, return to SERVO; so does `BACK` on the angle meter.
While the signal pin is still in use, SERVO opens with its entries drawn as
outlines and `SETTING SIG GP0 LOW` or `RELEASING SIG GP0` in amber in place
of the NO ESC CHECK line; a tap on an entry opens nothing until the pin is
free.

### RECEIVER

<img src="img/rx-chooser.png" width="240" alt="RECEIVER: MONITOR with S.BUS FROM A RECEIVER, ON GP1., OUTPUT with S.BUS TO A FLIGHT CONTROLLER. and ON GP0, NEVER TO AN ESC.">

`MENU → RECEIVER` opens a hub of two entries: **MONITOR**,
which opens S.BUS MONITOR, and **OUTPUT**, which opens S.BUS OUTPUT, an
S.BUS output to a flight controller, with its warning under it:
`S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.` The strip names
both signal pins, `SIG GP0  AUX GP1`, over the amber line
`NO ESC CHECK: UNPLUG ANY ESC FIRST.`, as on SERVO. RECEIVER takes no pin,
and DShot is off while it shows. With SIG on GP1 MONITOR carries a second
line, `GP1 IS SIG: PLUG IN AFTER THE TAP.`: GP1 is held low until the tap.
`BACK` returns to `MENU`; `RECEIVER`, the last button of either tool's
leave page, returns to RECEIVER. While the signal pin is still in use both
entries are outlines, as on SERVO.
See [Receiver](Receiver).

---

## SETUP

<img src="img/settings.png" width="240" alt="The SETUP screen">

Motor and wiring settings, display settings, the board, diagnostics, and the
button that writes all settings to flash. One list, in sections, scrolls under
a finger. It opens from `MENU`, or from a tap on the tester's pole count or
ceiling while disarmed; `BACK` returns to where it was opened from. The
section names and the values beside `-` and `+` are cyan, the colour of a
setting; read-only values are grey.

| Section | Row | What it does |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | 2–28, even values. The RPM readout divides the eRPM by half of it |
| ESC TESTER | **MAX THROTTLE** | The throttle ceiling, `5%` to `100%` in steps of 5 % |
| WIRING | **SIG PIN** | SIG, the GPIO (general-purpose input/output pin) of the ESC's signal wire, with its connector pin under it: `GP0` over `J5 PIN 2`, `GP1` over `J5 PIN 1`. Only the board's two free GPIOs are offered, GP0 and GP1, so a step swaps SIG and AUX. Over the tester DShot moves to the new pin at once; from MENU the next ESC TESTER uses it; see the note line below |
| WIRING | **DSHOT** | The DShot rate in kbit/s: 150 / 300 / 600 / 1200. Lower it if the `LINK` error rate rises |
| WIRING | **KISS TELEM** | Whether to listen on the optional third telemetry wire, on AUX |
| WIRING | **AUX PIN** | AUX, the GPIO of the KISS wire and of the servo tester's second servo, with its J5 pin under it. Acts whether KISS TELEM is on or off. A step swaps SIG and AUX, so the two never share a pin |
| SERVO SUPPLY | **SERVO SUPPLY** | `OWN`, `GP28` or `GP29`: `OWN` means the servo has a supply of its own and the bench switches nothing; `GP28` or `GP29` means a DC-DC converter (direct-current voltage converter) for the servo's supply has its enable on that pin, shown under the value: `J4 PIN 11` for GP28, `J4 PIN 12` for GP29. `OWN` by default. - and + step through the three. With `GP28` or `GP29`, SERVO TESTER, KST SERVO and HITEC SERVO switch the servo's supply |
| SERVO SUPPLY | **CURRENT SENSE** | `NONE` or `INA219`: whether an INA219 current sensor module is on J3. `NONE` by default. A tap switches between the two. With `INA219` a caption under the name reads `FOUND`, `NOT FOUND`, or `OTHER PART` for another chip at the sensor's address. An INA219 left on another configuration by other firmware also shows `OTHER PART`: power-cycle the module. The servo tester, KST SERVO and HITEC SERVO then show the supply's current |
| DISPLAY | **CONTRAST** | `NORMAL` or `HIGH`. High contrast is for daylight |
| DISPLAY | **BACKLIGHT** | 16–255, in steps of 32 |
| BOARD | **BUILT FOR** | Read-only. The board the firmware is built for, `RP2350-TOUCH-LCD-2.8` |
| BOARD | **VERSION** | Read-only. The firmware version, for example `0.1.0` |
| BOARD | **CLOCK** | Read-only. The board's clock to the minute, `2026-11-14 15:42`, or `NOT SET` in grey. The line under it, `SET AT EACH POWER-UP`: the board has no cell for its clock, so a computer sets it again after each power-up with `tools/set_clock.py` ([SD Logging](SD-Logging#the-clock)). Redrawn once a minute while the row shows |
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

- **The ESC pin's warning**, at y 230, only while DShot runs under SETUP,
  opened over the tester: in amber, the pin DShot runs on, for example
  `UNPLUG ANY SERVO FROM GP0 NOW`, and after a SIG PIN step the new pin, from
  the step on. A servo left there, for example after the two-servo tester,
  moves. Opened from MENU, DShot is off and the line does not show, after a
  SIG PIN step too: the next ESC TESTER sends DShot on the new pin. The list
  then ends 12 px higher, at y 227, and scrolls 12 px further; its top rows
  stay where they are. With KISS TELEM on, a step swaps SIG and AUX, so DShot
  runs on the wire the ESC's KISS pad was on: while SIG is off the pin in use
  at start-up, the line reads `SWAP THE ESC'S SIGNAL AND KISS WIRES` instead,
  opened over the tester or from MENU.
- **LINK**: live packets per second and error rate. Green means packets are
  arriving, so the ESC pin is correct. Amber means more than 5 % errors.
  Opened from MENU, where DShot is off, it reads `OFF` in grey.
- The note line: one text, the first that applies, in this order:
  1. `SAVE FAILED - FLASH REFUSED THE WRITE` (red) or
     `NOT WHILE ARMED - DISARM TO SAVE` (red), until the next save or
     leaving SETUP;
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

<img src="img/settings-carried.png" width="240" alt="SETUP after an update carried the saved settings over: CARRIED OVER BY UPDATE - CHECK, SAVE on the note line">

<img src="img/settings-pin.png" width="240" alt="SETUP over the tester after SIG PIN moved to GP1: UNPLUG ANY SERVO FROM GP1 NOW above LINK, UNSAVED on the note line">

- `HOLD TO SAVE` (150 × 46 px) and `HOLD DEFAULTS` (72 × 46 px), its label
  on two lines, `HOLD` over `DEFAULTS`, in amber.

**Changes apply at once. Only storing them needs the hold.** A new bitrate
rebuilds the DShot driver before the next frame. `HOLD TO SAVE` for 1 s writes
to flash. `HOLD DEFAULTS` for 1 s replaces the live settings with the
compiled defaults, the pole count and the throttle ceiling included; the
angle meter's setup and calibration, which SETUP does not show, keep their
values, and flash is untouched. A tap does nothing, and sliding off the button
cancels the hold. A bar over the button fills while it is held, and the button
turns red, as every hold does. Leaving the screen keeps the
defaults in use. The saved values load only when the board starts, so
restarting the board without `HOLD TO SAVE` returns to them.

**LAST RESTART.** The firmware reads the reason once, when it starts. A
board that restarts on its own while a motor spins and then reads
`BROWN-OUT` points at the supply. The firmware arms no watchdog, so
`WATCHDOG` does not appear with this firmware. According to the boot ROM's
source, not verified on hardware, the restart after copying a firmware file
onto the board reads `UPDATE`. Not tested on hardware: which reason a given
restart shows on the board is not recorded.

<img src="img/settings-supply.png" width="240" alt="SETUP scrolled to SERVO SUPPLY, set to GP29"> <img src="img/settings-scrolled.png" width="240" alt="SETUP scrolled to the end: BOARD and DIAGNOSTICS">

<img src="img/settings-sense.png" width="240" alt="SETUP: CURRENT SENSE at INA219, FOUND">

<img src="img/settings-contrast.png" width="240" alt="SETUP in high contrast"> <img src="img/tester-contrast.png" width="240" alt="The tester screen in high contrast">

*High contrast applies to every screen and sets the backlight to 255 while
it is on.*

---

## LOGS

<img src="img/log-ready.png" width="240" alt="The LOGS screen, card ready">

Recording state and counters, and after a stop the last log's run.
`BACK` returns to `MENU`. See [SD Logging](SD-Logging).

---

## ESC SETTINGS

<img src="img/am32-list.png" width="240" alt="The AM32 settings list">

Reads, edits and writes the settings of an AM32, Bluejay or BLHeli_S ESC over
the signal wire.
`WRITE...` opens `ESC WRITE` on a tap: it lists each change, old and new,
and its `HOLD TO WRITE`, 1 s, writes, with a backup on the SD card first.
`HEX` shows the bytes under the title `ESC HEX`. Settings in a layout the
tester does not know, other than 2 to 4, open read-only: the bytes in hex,
`NOT VERIFIED.`, a backup on the SD card and no write. `BACK` returns to
ESC PROGRAM, after a warning when changes are not written. See
[AM32 Configuration](AM32-Configuration).

---

## SERVO TESTER

<img src="img/servo-safe.png" width="240" alt="The servo screen, SAFE">

Drives one or two servos with servo pulses on SIG and AUX, the ESC and KISS
pins. The strip names them, `SIG GP0  50 HZ  1000-2000 US`, or
`SIG+AUX  50 HZ  1000-2000 US` with two servos. It opens SAFE; `HOLD TO START`
for 1 s starts the pulses at the centre, `STOP` acts on touch. When four reads
of the touch controller in a row have failed, over 100 ms or more, a running
servo stops as on `STOP`, the dial reads `TOUCH NOT ANSWERING` until it answers
again, and `HOLD TO START` is drawn as an outline. While SAFE with one servo,
the readout reads `TWO SERVOS:` and `SET > SERVOS`, where the second servo is
switched on. With SERVO SUPPLY at `GP28` or `GP29` the screen switches the
converter: on at `HOLD TO START`, off at `STOP`, at a stall and at `BACK`. The
strip's second line says where it is, in amber: `GP28 SUPPLY ON AT START`,
`GP28 SUPPLY COMING UP`, `GP28 SUPPLY ON`, or `GP28 SUPPLY NOT OFF` when the
current sensor still reads more than 2.5 V 2 s after the switch-off. `SET` opens
the profile and sweep settings; its `BACK` with a change waiting for
`HOLD TO APPLY` warns first, `1 CHANGE NOT APPLIED.` `BACK` opens the leave page
at `1 UNPLUG THE SERVO.`; `NEXT SERVO` returns to the tester, and `SERVO` holds
the pins low and returns to SERVO, the hub. With CURRENT SENSE at `INA219` the
row under the dial shows the servo supply's current and voltage in place of the
two-servo hint, and the sensor's readings stop the servo: the chip then reads
`OVERCURRENT`, `STALL`, `LOW SUPPLY` or `NO SENSOR` in magenta. See [Servo
Tester](Servo-Tester).

---

## KST SERVO

<img src="img/kst-list.png" width="240" alt="The KST field list">

Opens from `MENU → SERVO → KST / CHASERVO`, whose tap takes SIG.
Reads and writes the settings of a KST or Chaservo servo over the signal wire
on SIG, the ESC pin. It searches for the servo while you power it up, reads
its 32 registers, and lists 27 fields to step. A tap on `WRITE...` opens
the write page, and its `HOLD TO WRITE`, 1 s, writes, with a backup on the SD
card first. `BACK` opens the leave page, whose first step asks you to power
the servo off; `LEAVE` then makes `UNPLUG THE SERVO.` the current step,
`NEXT SERVO` starts a new search, and `SERVO` holds the pin low and returns
to SERVO, the hub. With CURRENT SENSE at `INA219` the strip shows the current
after the supply's state, `SUPPLY ON 0.08 A`. See [KST
Programming](KST-Programming).

---

## HITEC SERVO

<img src="img/hitec-list.png" width="240" alt="The Hitec field list">

<img src="img/hitec-read-wait.png" width="240" alt="HITEC SERVO for a D series servo on its own supply: SIG GP0 IS THE SERVO'S: DSHOT IS OFF. in green, POWER THE SERVO UP, THEN TAP READ. in amber, and READ">

Opens from `MENU → SERVO → HITEC D SERIES` or `HITEC CLASSIC`.
Reads and writes the settings of a Hitec D series or classic servo over the
signal wire on SIG, the ESC pin. The tap on the hub's entry takes the pin,
which no DShot drives on the hub; the strip reads
`RELEASING SIG GP0 FROM DSHOT` until the board has let go of it. A D series
servo on its own supply (SERVO SUPPLY at `OWN`) must run before the screen
reads it, so the screen then waits: the strip
reads `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.` in green and
`POWER THE SERVO UP, THEN TAP READ.` in amber. Power the servo up and tap
`READ`. With the switched supply, and for a classic servo, the bench powers
the servo itself and the screen goes on at once. While an ESC answers on
SIG the screen takes nothing and reads `AN ESC ANSWERS ON SIG GP0: UNPLUG
IT.` in amber; `BACK` then returns to SERVO, the hub. The screen
enters the servo and reads it. A D series servo gives 21 registers and 20
parameters, 11 of them editable. One servo goes on the lead, with no pull-up
resistor. A tap on `WRITE...` opens the write page, and its `HOLD TO WRITE`, 1
s, writes, with a backup on the SD card first. `BACK` opens the leave page;
`LEAVE` then makes `UNPLUG THE SERVO.` the current step, `NEXT SERVO` starts
a new session for the same family, and `SERVO` holds the pin low and returns
to SERVO, the hub.
With CURRENT SENSE at `INA219` the strip shows the current after `SUPPLY ON`.
Not tested on hardware. See [Hitec Programming](Hitec-Programming).

---

## S.BUS MONITOR

<img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR with frames: the OK chip, 16 lime bars and their values">

Shows a receiver's S.BUS output on GP1: the link chip on the strip, 16
channel bars over the raw value with the value in µs, the digital channels
CH17 and CH18, the frame rate and the error counts. It takes GP1: as AUX in
the default wiring, as SIG when SIG PIN is GP1, where it waits while an ESC
answers. `BACK` opens the leave page at `1 UNPLUG THE RECEIVER.`;
`BACK TO MONITOR` starts again with every count at 0, and `RECEIVER` holds
the pins low and returns to RECEIVER. See [Receiver](Receiver).

---

## S.BUS OUTPUT

<img src="img/sbus-out-running.png" width="240" alt="S.BUS OUTPUT running: the RUNNING chip, the throttle's ceiling and the flags, eight sliders with lime values, STOP">

Sends S.BUS frames on GP0 to a flight controller, never to an ESC: 16
channels on sliders over two pages, and on SET the throttle channel, CH17,
CH18 and the failsafe and frame-lost flags. It takes GP0, then
checks it for an ESC's bootloader before `HOLD TO START` is offered. The
throttle channel stops at MAX THROTTLE's ceiling. `STOP`, a stalled screen,
30 s without touch away from the first frame, and a touch controller that
does not answer, as on the tester, stop the frames; the last reads
`STOPPED: TOUCH DID NOT ANSWER`. `BACK`
opens the leave page at `1 UNPLUG THE FLIGHT CONTROLLER.`; `BACK TO OUTPUT`
checks GP0 again, and `RECEIVER` holds the pins low and returns to
RECEIVER. See
[Receiver](Receiver).

---

## ANGLE METER

<img src="img/angle-main.png" width="240" alt="ANGLE METER after a zero: -20.0 degrees, -13.7 MM, MAX -20.0 degrees, the IMU's temperature">

Reads angles from gravity with the board's own accelerometer: the angle
about the axis at scale 5, the axis's tilt and the `SETTLED` chip on the
strip, the trailing edge's travel in mm with LENGTH set, MAX each side and
the board's temperature. `ZERO`, `RESET MAX` and `SET` in the footer;
ANGLE SET holds ALONG AXIS, LENGTH, SIGN and the three-pose calibration, FLIP
CAL. `BACK` on the main view turns the accelerometer off and returns to
SERVO, the hub; on ANGLE SET and FLIP CAL it goes one level up. Not tested on
hardware; the accuracy is not measured. See [Angle Meter](Angle-Meter).

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
