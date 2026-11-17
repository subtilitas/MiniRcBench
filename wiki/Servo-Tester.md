**English** | [Deutsch](Servotester)

# Servo Tester

This screen drives one servo, or two at once, with standard servo pulses: a
high level of 400 µs to 2500 µs, repeated 50 to 560 times a second. You move
the servo by dragging the horn on the screen, with three buttons, or with a
sweep. It does not measure the servo: there is no position feedback, so the
horn on the screen is drawn where the pulse puts it, not where the servo is.

<img src="img/servo-running.png" width="240" alt="The servo screen, running">

> **Unplug the servo before ESC TESTER or ESC COMMANDS.** Those two screens
> drive the ESC pin with DShot. A servo on that pin reads it as a pulse of
> about 970 µs at 1000 Hz: it drives towards one end of its travel, at a
> frame rate that overheats an analogue servo. Everywhere else, from power-up
> on, the board holds the ESC pin low and sends nothing. The leave page asks
> you to unplug the servo before you go on.

---

## Wiring

The servo needs its own supply. The board supplies the signal and ground only.
With SERVO SUPPLY at `GP28` or `GP29` that supply is the bench's switched
converter, part C of the [wiring sheet](Wiring), which the screen switches
itself ([The switched supply](#the-switched-supply)).

```
  servo supply, 4.8 V to 6.0 V (8.4 V for an HV (high-voltage) servo)
     +  ----------------------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo A signal (orange or white)
  board AUX  ------------------------------------------  servo B signal, two servos only

  SIG GP0 = J4 pin 9 or J5 pin 2, AUX GP1 = J4 pin 10 or J5 pin 1,
  GND = J4 pin 1 or 5, or J5 pin 4
```

> **Never connect a servo lead straight across to J5.** J5 pin 3 is the
> board's 3.3 V supply, in the place of a servo plug's plus: see
> [Setup](Setup#the-adapter-cable).

- The servo's middle wire goes to its supply, never to a pin of the board.
  The board's 5 V pin is its USB supply, and a servo can draw more than 1 A.
- Switch the board on before the servo supply, and the servo supply off
  before the board.
- Join the two grounds once, at the board.
- Servo A is on SIG, the ESC pin, and servo B on AUX, the KISS pin, as set
  under **MENU → SETUP → SIG PIN** and **AUX PIN**, which show the J5 pin
  under the value. GP0 and GP1 are the defaults. AUX PIN acts whether KISS is
  on or off, and a step of either row swaps the two.
- Unplug any ESC from SIG. SERVO's strip says so in amber,
  `NO ESC CHECK: UNPLUG ANY ESC FIRST.`: DShot is off outside ESC TESTER and
  ESC COMMANDS, so an ESC answers only within 500 ms of leaving one of them.
  The screen waits while an ESC answers on SIG, but it cannot see every
  ESC.

---

## Opening it

Open **MENU → SERVO**, then tap **SERVO TESTER**, the first entry (see
[The Screens](The-Screens#servo)). The screen is titled `SERVO TESTER`, as
its entry. `SERVO`, the leave page's last button, returns to SERVO at the end
of a session.

The screen takes the two signal pins, held low until then. If an ESC answers on
SIG, it waits and says `AN ESC ANSWERS ON SIG GP0.` Unplug the ESC; the
screen takes the pins by itself 500 ms later. Servo pulses of 1000 µs to
2000 µs are a valid throttle signal for many ESCs.

<img src="img/servo-esc.png" width="240" alt="Waiting while an ESC answers">

The screen opens **SAFE**: the pins are held low and carry no pulses. What a
servo does with no pulses is up to the servo: it holds, goes limp, or moves to
its own failsafe position.

<img src="img/servo-safe.png" width="240" alt="The servo screen, SAFE">

The line under the title shows the pin by role and number, the frame rate
and the pulse range, `SIG GP0  50 HZ  1000-2000 US`, or
`SIG+AUX  50 HZ  1000-2000 US` with two servos, and a chip: `SAFE` (green),
`RUNNING` (red), `STALLED` (magenta), or after a cut of the current sensor
`OVERCURRENT`, `STALL`, `LOW SUPPLY` or `NO SENSOR` (magenta, see
[The cuts](#the-cuts)). At 333 Hz with a MIN of 1000 µs or
more the line has no room for the number and reads
`SIG  333 HZ  1000-2000 US`, or `SIG+AUX  333 HZ` with two servos.

While SAFE with one servo, the right of the readout row reads `TWO SERVOS:`
and `SET > SERVOS`: the SERVOS row of the SET screen switches servo B on (see
[Two servos](#two-servos)). The hint is not shown while AUX is refused.

### The switched supply

With **SERVO SUPPLY** at `GP28` or `GP29` under SETUP, the servo tester
switches the converter the KST and Hitec programmers use, part C of the
wiring sheet. Not tested on hardware.

- **SAFE:** the converter is off. The line under the title reads
  `GP28 SUPPLY ON AT START` in amber, and the dial warns
  `PULSES ON GP0 ARE THROTTLE TO AN ESC,` / `AND ITS BEC WIRE GETS THE SUPPLY.`
- **`HOLD TO START`:** the converter goes on at once, the line reads
  `GP28 SUPPLY COMING UP`, and the servo gets no pulse yet. The buttons and
  the dial wait; `STOP` works.
- **Supply up:** with the current sensor (SETUP > CURRENT SENSE `INA219`)
  at the first reading of 4.5 V or more, without it after 500 ms. The servo
  goes to CENTRE and the line reads `GP28 SUPPLY ON`. The SET screen shows
  how long it took, `SUPPLY UP IN 25 MS`, until the first timed move
  ([Move timing](#move-timing)) takes its place.
- **Off again:** at `STOP`, when the screen stalls for 250 ms, when the touch
  panel does not answer for 250 ms (`STOPPED: TOUCH DID NOT ANSWER`), when
  the I2C bus is stuck (`STOPPED: TOUCH BUS STUCK`), at a cut of the
  current sensor ([The cuts](#the-cuts)), and at `BACK`.
- **Did not come up:** with the sensor and no 4.5 V within 500 ms, the
  converter goes off and the dial reads `SUPPLY NOT UP: NO 4.5 V IN 500 MS.`
- **Sensor not found:** with CURRENT SENSE at `INA219` and no sensor,
  `HOLD TO START` is drawn disabled and the dial reads
  `START WAITS FOR THE CURRENT SENSOR.` Set CURRENT SENSE to `NONE` to start
  without it. A stuck I2C bus or a silent touch panel also holds the start:
  `START WAITS: THE I2C BUS IS STUCK.` or `START WAITS: TOUCH DOES NOT ANSWER.`
- **Not off:** if the sensor still reads more than 2.5 V 2 s after the
  switch-off, the line reads `GP28 SUPPLY NOT OFF`. The converter does not
  disconnect its output, or EN is held high: check the wiring.

<img src="img/servo-supply.png" width="240" alt="The servo screen with SERVO SUPPLY at GP28 while SAFE: GP28 SUPPLY ON AT START under the title and the ESC warning on the dial"> <img src="img/servo-no-sensor.png" width="240" alt="START WAITS FOR THE CURRENT SENSOR. with HOLD TO START disabled"> <img src="img/servo-not-off.png" width="240" alt="GP28 SUPPLY NOT OFF under the title, 4.90 V in the readout"> <img src="img/servo-set-up.png" width="240" alt="SERVO SET with SUPPLY UP IN 25 MS under STOP TO CHANGE THE PROFILE">

---

## Starting and stopping

**Hold `HOLD TO START` for 1 s.** A bar fills while you hold. At the end
every connected servo goes to CENTRE at once, whatever it was told before. The
chip reads `RUNNING` and the button becomes `STOP`.

**`STOP` acts on touch.** The pulses end with the next frame: the last pulse
ends within one frame, 20 ms at 50 Hz. Touching `STOP` and holding on does not
start the servo again; start it with a new press.

If the screen stops responding for 250 ms, the pulses end by themselves, the
chip reads `STALLED`, and the screen says `STOPPED: SCREEN STALLED`. Nothing
restarts until you hold `HOLD TO START` again.

<img src="img/servo-stalled.png" width="240" alt="Stopped after a stall">

If four reads of the touch controller in a row have failed, over 100 ms or more,
a running servo stops as on `STOP`, sweeping or holding a position, on the main
screen and on SET. A touch that does not answer cannot reach `STOP`, and the
servo tester has no idle stop. The dial reads `TOUCH NOT ANSWERING` in amber
until the controller answers, over `THE SERVO IS STOPPED.` when this stop or the
cut after 250 ms of a stalled screen or, with the switched supply, of a silent
touch panel ended a run, and over `HOLD TO START WORKS ONCE IT ANSWERS.`
otherwise; `HOLD TO START` is drawn disabled meanwhile. This note comes before
every other note on the dial, `PREPARING THE LOG...` included.

---

## Moving the servo

| Control | Effect |
|---|---|
| Press on the dial, drag | The servo goes to the angle under your finger: the left end is MIN, straight up CENTRE, the right end MAX. Past the ends it stays at MIN or MAX |
| Release | The servo stays where it is |
| `MIN` `CENTRE` `MAX` | On release, to that end or the centre |
| `SWEEP` | A sweep about the centre; the button becomes `STOP SWEEP` |
| `SPEED` | How fast the servo is moved, 10 % to 100 %. Its fill is the faint colour of the frames |

- The dial and the buttons work only while `RUNNING`; while `SAFE` they are
  drawn as outlines and do nothing.
- A drag must start on the dial. A press that starts elsewhere and slides onto
  the dial moves nothing.
- `SPEED` 100 % sends each new position at once, and the servo moves at its
  own speed. Below 100 % the pulse moves gradually: at 10 % a full travel from
  MIN to MAX takes 5 s.
- A sweep starts at the centre and runs its curve on the board's second core,
  so the screen does not slow it down. While it runs, the readout shows
  `SWEEP 3` or `SWEEP 3/10` in lime, a live reading: the ends reached, and
  the count you set.
  `STOP SWEEP` leaves the servo where the curve was. A drag, `MIN`, `CENTRE`,
  `MAX` and `STOP` end a sweep too.

<img src="img/servo-sweep.png" width="240" alt="A sweep running">

---

## Two servos

Set `SERVOS` to `TWO` on the SET screen. Servo B is on AUX. Both
share one profile and one frame rate.

<img src="img/servo-two.png" width="240" alt="Two servos">

`A`, `B` and `AB` choose what the dial and the buttons move: one servo, or
both to the same angle. The chosen horn is drawn wide. Servo A's horn and
letter are lime, servo B's blue, and `B` fills blue while it is chosen. With
`REVERSE B` on, B
moves the opposite way when both follow, for a pair mounted face to face.

Two servos need AUX free. If KISS frames arrive on it, the screen offers one
servo only, and the SET screen says why: `KISS WIRE ON AUX GP1 IS LIVE.`
SETUP never puts SIG and AUX on one pin; pins set that way some other
route read `SIG AND AUX ARE THE SAME GPIO.` While the screen waits for the
pins, for example while an ESC answers, it cannot know whether AUX is free,
and `TWO` can be chosen. If the screen then finds AUX taken, it takes SIG
alone and goes back to one servo, and `TWO` stays off until you
open the screen again. This holds for a `TWO` still waiting for
`HOLD TO APPLY` too: it goes back to one servo, and a wait that held only for
`TWO` ends.

<img src="img/servo-samepin.png" width="240" alt="TWO not offered: both pins are one GPIO">

---

## Current

With an INA219 current sensor module on J3 and SETUP → CURRENT SENSE at
`INA219`, the row under the dial shows the servo supply's current, `0.42 A`,
and under it the voltage on the servo's side of the sensor, `5.02 V`. The
sensor is optional and not tested on hardware. Its wiring is in the design
note `docs/design/current-sense.md`, "Place in the circuit": the module's
VCC on J3 pin 3, 3.3 V, never 5 V, and the servo's current through the
module's screw terminal only.

<img src="img/servo-sense.png" width="240" alt="The servo screen with the current and the voltage right of the pulse">

- With two servos on one supply the row shows their sum, `A+B 0.84 A`.
- `>3.2 A` in amber: the current is at or past the end of the sensor's
  range, 3.2 A, and the true value is not known.
- `-- A`: no reading for 250 ms. The sensor is missing, or the bus is
  failing; SETUP shows `NOT FOUND` under CURRENT SENSE.
- While a sweep runs, its count moves to the top of the dial.

The current is read 40 times a second, each reading the mean over 17 ms.

### The cuts

With the current sensor the servo tester stops by itself on its readings.
Not tested on hardware: no INA219 and no converter has been on the board.

| Chip | Dial | When |
|---|---|---|
| `OVERCURRENT` | `STOPPED: CURRENT 3.0 A OR MORE` | one reading of 3.0 A or more, either direction, or a reading at the end of the range |
| `STALL` | `STOPPED: OVER 1.0 A FOR 400 MS` | readings above 1.0 A for 400 ms, none at or below it between |
| `LOW SUPPLY` | `STOPPED: UNDER 4.0 V FOR 1000 MS` | with SERVO SUPPLY at `GP28` or `GP29`: readings under 4.0 V for 1000 ms after the supply came up |
| `NO SENSOR` | `CURRENT SENSOR LOST: SUPPLY OFF.` | with SERVO SUPPLY at `GP28` or `GP29`: the sensor stops answering, or 250 ms pass with no reading |

- The pulses end at once, and with `GP28` or `GP29` the converter goes off
  in the same step of the board's second core. The chip stays until the
  next `HOLD TO START`; the dial's second line reads `HOLD TO START AGAIN.`
- While the sensor is not online after a cut, the dial's second line reads
  `START WAITS FOR THE CURRENT SENSOR.` and `HOLD TO START` is drawn disabled; once
  it answers again, `HOLD TO START AGAIN.`
- With `OWN` only `OVERCURRENT` and `STALL` apply: the bench knows neither
  the voltage of your supply nor whether its current runs through the
  sensor. What the servo does without pulses is the servo's own. If the
  sensor stops answering while RUNNING with `OWN`, nothing is cut, and the
  dial reads `NO SENSOR READING: NO CURRENT CUTS.` in dim.
- The stall cut also trips on many servos held at MIN or MAX against their
  own end stop, where they draw stall current. Not measured; the design
  note's bench test B8 checks it.
- A cut takes up to about 60 ms after a step of current. The cuts protect
  the servo and the sensor's shunt; they are no fuse.
- Beside `OVERCURRENT`, `LOW SUPPLY` and `NO SENSOR` the line under the
  title reads by role alone, `SIG  50 HZ  1000-2000 US`, and at 333 Hz with
  one servo `SIG  333 HZ`.

<img src="img/servo-overcurrent.png" width="240" alt="OVERCURRENT in magenta, STOPPED: CURRENT 3.0 A OR MORE on the dial"> <img src="img/servo-no-sensor-cut.png" width="240" alt="NO SENSOR in magenta, CURRENT SENSOR LOST: SUPPLY OFF. over START WAITS FOR THE CURRENT SENSOR.">

### Move timing

With the current sensor the servo tester times how long the servo takes to
reach the position you tap, from its current: a moving servo draws more than
a resting one. The SET screen shows the last move on the second line under
its title, in dim:

`MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`

| Figure | Meaning |
|---|---|
| `MOVE 0.42 S` | from the tap to the first reading that shows the servo back at rest |
| `MOVE LATE` | the servo moved, but its current did not settle within 3 s |
| `MOVE UNSEEN` | the current did not change within 3 s: the servo did not move, or it moved too lightly to see |
| `PEAK 1.21 A` | the highest reading from the tap to the arrival |
| `HOLD 0.12 A` | the mean current in the half second after the arrival |
| `--` | that figure is not known: see below |

How to get a timed move:

1. Start the servo with the sensor (SETUP > CURRENT SENSE `INA219`).
2. Set `SPEED` to 100 %.
3. Wait 4 s after `HOLD TO START`, after a drag, a sweep or a move at a lower
   `SPEED`; after a timed move, half a second is enough.
4. Tap `MIN`, `CENTRE` or `MAX`, wait for the servo, then open `SET`.

- A tap too soon, a tap at a lower `SPEED`, or a move ended by `STOP`, a
  cut, another tap or drag, or a lost sensor shows
  `MOVE -- S  PEAK -- A  HOLD -- A`. `HOLD` shows `--` for the first half
  second after the arrival.
- The first time the servo goes to an end where it pushes against its stop,
  it reads `LATE`: the bench does not yet know how much it draws there. The
  next move to that end is timed.
- The time is an upper bound on when the current shows the arrival. The
  sensor shows it up to 61 ms late (computed: one reading and two of the
  sensor's averaging periods), and the pulse leaves up to 20 ms after the
  tap at 50 Hz. A servo that eases off near the target can fall to its
  holding current while the horn still creeps, so the current can show the
  arrival before the horn stops. How close the time comes to the horn's
  real arrival is not measured.
- With CURRENT SENSE at `NONE` nothing is timed, and the line stays as it
  is.
- The line goes at the next `HOLD TO START`, and it does not show while the
  screen has a two-line warning there.

<img src="img/servo-set-move.png" width="240" alt="SERVO SET with a timed move, MOVE, PEAK and HOLD, under STOP TO CHANGE THE PROFILE">

### The log on the card

With the sensor (SETUP > CURRENT SENSE `INA219`) and a card in the slot,
every run from `HOLD TO START` to its end is written to the card as
`LOGS/SRVnnnnn.CSV`: one line for each reading of the sensor, 40 a second,
in columns a spreadsheet opens.

```text
ms,pulse_a_us,pulse_b_us,current_ma,bus_mv,flags
12345,1500,0,420,5020,0
```

| Column | Meaning |
|---|---|
| `ms` | time of the reading since `HOLD TO START`, in milliseconds |
| `pulse_a_us`, `pulse_b_us` | the pulse on SIG and on AUX in µs; 0 for no pulse or no second servo |
| `current_ma` | the current in mA, signed |
| `bus_mv` | the supply voltage at the sensor in mV |
| `flags` | 1: the current was beyond the sensor's range; 2: the first reading after the sensor restarted |

The last line says how the run ended, how many readings were taken and
how many were dropped, and how long the supply took to come up:
`# end=STOP rows=2400 dropped=0 up_ms=40`. `end=` is `STOP`, `BACK`,
`OVERCURRENT`, `STALL`, `LOW_SUPPLY`, `NO_SENSOR`, `STALLED`,
`TOUCH_SILENT`, `BUS_STUCK` or `NO_START`; `up_ms=-` when the bench does not
switch the supply.

- The numbers count on from the highest `SRVnnnnn.CSV` in `LOGS`, apart from the
  blackbox logs' `LOGnnnnn.BFL`.
- After `HOLD TO START` the dial reads `PREPARING THE LOG...` and `THE SERVO
  STARTS AFTER IT.` while the bench opens the file; the servo starts right
  after, unless the touch controller stops answering meanwhile. A card that
  takes longer than 2 s for that gets no log: `NO LOG: CARD TOO SLOW`, and the
  servo starts without one.
- While the servo runs, the dial's second line reads `LOGGING TO SRV00012.CSV`
  in dim. In amber: `SRV00012.CSV: 12 ROWS DROPPED` (the card was slower than
  the readings), `LOG STOPPED AT 4 MIB` (the file's limit, about an hour), `LOG
  STOPPED: NO ROOM IN ONE PIECE` (the card had less than 4 MiB free in one
  piece, and the 64 KiB it gave are used), `LOG STOPPED: CARD WRITE FAILED` (the
  card was pulled, or failed), `NO LOG: NO 64 KIB IN ONE PIECE`, `NO LOG: CARD
  TOO SLOW`, `NO LOG: THE CARD DID NOT ANSWER`, `NO LOG: THE FILE DID NOT OPEN`
  or `NO LOG: SRV99999.CSV EXISTS`. The servo runs on in every case.
- A blackbox recording that was running stops at `HOLD TO START`; LOGS then
  reads `RECORDING STOPPED BY SERVO TESTER`. The last run that LOGS shows
  stays the blackbox recording's.
- Stop the servo before cutting the power or pulling the card. A run cut by a
  power loss leaves a file of 4 MiB, or 64 KiB: the readings that reached the
  card, then whatever the card held there before. The last readings, up to about
  16 KiB, are lost. A card swapped in during a run that got `NO LOG` keeps its
  own files; the file the bench had begun may then stay on the first card.
- Writing the log never makes the bench wait for the card: the sensor is read
  and the cuts act as without the log, within 0.1 ms in the host model. How long
  a real card takes is not measured on the board.


<img src="img/servo-log.png" width="240" alt="SERVO TESTER running, LOGGING TO SRV00012.CSV above the dial">
<img src="img/servo-log-prep.png" width="240" alt="SERVO TESTER after HOLD TO START: PREPARING THE LOG... while the file opens">

---

## The SET screen

`SET` opens the settings. The servo keeps running under it.

<img src="img/servo-set.png" width="240" alt="The SET screen">

Tap a row to select it. `-` and `+` change it one step, and repeat while held.
Swipe the row sideways for big steps. Drag up or down to scroll. Rows are
40 px tall, so three show at a time.

| Row | Values |
|---|---|
| SERVOS | ONE or TWO |
| TYPE | STANDARD PWM, NARROW 760, WIDE, HELI CYCLIC, HELI TAIL 760 |
| RATE | the frame rates the type offers, up to what the range allows |
| MIN, CENTRE, MAX | 400 µs to 2500 µs, 10 µs steps, in that order |
| REVERSE B | OFF or ON |
| SHAPE | SQUARE, SINE or TRIANGLE |
| RATE (sweep) | 0.05 Hz to 5.00 Hz |
| AMPLITUDE | 0 % to 100 % of half the travel |
| DWELL | 0 ms to 5000 ms at each end |
| MOVES | ENDLESS, or 1 to 9999 ends |

- The PULSE rows change only while `SAFE`. While `RUNNING` the screen says
  `STOP TO CHANGE THE PROFILE` and draws their values faint.

  <img src="img/servo-set-running.png" width="240" alt="SET while running: the PULSE rows locked, STOP in the footer">

- While `RUNNING`, `DEFAULTS` becomes `STOP` and acts on touch, as on the
  servo screen. The SWEEP rows stay adjustable.

- A heli type, or a frame rate above 60 Hz, can destroy a servo not made for
  it: above 60 Hz an analogue servo overheats. Such a change waits, the screen
  names the risk, and `HOLD TO APPLY` (1 s) applies it. `BACK` drops it,
  after a warning: the first `BACK` stays on SET and puts
  `1 CHANGE NOT APPLIED.` and `BACK AGAIN DISCARDS IT.` in amber on the
  strip, or `n CHANGES NOT APPLIED.` and `BACK AGAIN DISCARDS THEM.`,
  counting the rows that wait; the second `BACK` returns to the servo screen and
  drops the change. Any other touch takes the warning away, and the change
  keeps waiting. With nothing waiting, `BACK` returns at once.
  While it waits, the row's value is dark on a cyan fill, in the row and in
  the editor bar, with a 3 px cyan bar at the row's left edge; a value that
  applied is cyan text, the colour of a setting, selected or not.
- A pulse past a servo's travel drives it against its end stop, where it stalls
  and heats. With TYPE WIDE, MIN below 900 µs or MAX above 2100 µs the screen
  says `PAST ITS TRAVEL A SERVO STALLS AND HEATS. TRY MIN AND MAX HORN-FREE.` in
  amber. HELI CYCLIC shows it at its own 820 µs to 2220 µs. NARROW 760 and HELI
  TAIL 760 are measured against their own ranges: the note shows when MIN lies
  more than 100 µs below the type's MIN, or MAX more than 100 µs above the
  type's MAX. A TYPE, MIN or MAX change that leaves the profile past its travel
  waits when it changes TYPE or takes MIN or MAX further out than the applied
  profile: the note is its warning, on every row, and `HOLD TO APPLY` (1 s)
  applies it. Stepping further while it waits keeps it waiting; stepping back
  inside the margin, or back to the applied value, applies at once when nothing
  else waits. A step toward the centre applies at once. `BACK` drops a waiting
  change, after the same warning. A heli or 60 Hz change shows its own warning
  first, and HELI CYCLIC's one hold covers both. With TWO servos the hold covers
  both. Changing SERVOS from ONE to TWO on a profile past its travel waits for
  the hold too, since it drives a second servo there; on a profile within its
  travel, and from TWO to ONE, it applies at once. Once applied, the note shows
  while TYPE, MIN or MAX is selected, and on other rows when no other note does.
  The travel of a servo is in its data sheet; the bench does not measure it.
- The SWEEP rows change at any time and apply to the next `SWEEP`. When
  `SPEED` is slower than the curve, the screen says `SPEED LIMITS THE SWEEP`.
- `DEFAULTS`, offered only while `SAFE`, returns to STANDARD PWM at 50 Hz,
  one servo, and a sine at 0.50 Hz.

<img src="img/servo-set-warning.png" width="240" alt="A heli profile waiting for HOLD TO APPLY"> <img src="img/servo-set-travel.png" width="240" alt="TYPE WIDE waiting for HOLD TO APPLY behind the end-stop note">

Nothing on this screen is stored. Every time you open the servo screen it
starts at STANDARD PWM, 50 Hz, one servo.

---

## Leaving

`BACK` ends the pulses and opens the leave page, whose first step asks you to
unplug the servo: `1 UNPLUG THE SERVO.`, with
`ESC TESTER'S DSHOT ON SIG GP0 READS` and `AS A 970 US PULSE AT 1000 HZ.`, then
`2 BACK TO SERVO.` The last pulse completes first, which takes up to 41 ms at 50
Hz. A weak pull-down then holds SIG low until the last button drives it low; AUX
rests on its pull-down. The strip names the pins the screen holds,
`SIG GP0  AUX GP1`, or `SIG GP0` when it holds SIG alone.

<img src="img/leave-servo.png" width="240" alt="The leave page: 1 UNPLUG THE SERVO., NEXT SERVO and SERVO">

Until the last pulse has completed the page reads `RELEASING THE PINS`, or
`RELEASING THE PIN` with SIG alone, and both buttons are drawn disabled. Then
`NEXT SERVO` acts: it returns to the servo screen, SAFE, and keeps the
pins. Unplug the servo, then tap `SERVO`, the hub's title: it acts 1 s after
the pins are let go, so a quick second tap after `BACK` ends nothing. SIG is
driven low, no DShot starts, and SERVO, the hub, returns. A servo left on
SIG meets DShot at the next ESC TESTER or ESC COMMANDS. The header's top-right button reads `LEAVE` before the release and acts as `LEAVE`; while the claim releases it does not act; after the release it reads `BACK` and, 1 s later, returns to the hub as the wide button does. A button that does not act is a dim frame with a dim label.

---

## When it does not work

- **`AN ESC ANSWERS ON SIG GP0.`** An ESC is on SIG, the ESC pin. Unplug it.
- **`SIG GP0 COULD NOT BE TAKEN (PWM)`.** The board could not set up its
  pulse output. Tap `RETRY`.

  <img src="img/servo-bind.png" width="240" alt="SIG GP0 COULD NOT BE TAKEN (PWM), with RETRY">

- **`TWO` cannot be chosen.** The KISS wire on AUX is live, or both pins were
  set to one pin some other way than SETUP. The SET screen names the cause
  when `SERVOS` is selected.
- **The servo does not move.** Check that the chip reads `RUNNING`, that the
  servo has its own supply, and that its ground is joined to the board's.
  With the switched supply, check that the line under the title reads
  `GP28 SUPPLY ON`.
  Whether a servo accepts the board's 3.3 V pulses is not recorded per servo.

Nothing of the servo tester has been tested on hardware yet.
