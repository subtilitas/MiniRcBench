**English** | [Deutsch](Servotester)

# Servo Tester

This screen drives one servo, or two at once, with standard servo pulses: a
high level of 400 µs to 2500 µs, repeated 50 to 560 times a second. You move
the servo by dragging the horn on the screen, with three buttons, or with a
sweep. It does not measure the servo: there is no position feedback, so the
horn on the screen is drawn where the pulse puts it, not where the servo is.

<img src="img/servo-running.png" width="240" alt="The servo screen, running">

> **Unplug the servo before DShot runs.** Outside this screen the ESC pin
> carries DShot. A servo on that pin reads it as a pulse of about 970 µs at
> 1000 Hz: it drives towards one end of its travel, at a frame rate that
> overheats an analogue servo. That happens when the board **boots** with a
> servo on the ESC pin, which the splash screen warns of with
> `UNPLUG ANY SERVO FROM GP0 NOW`, and when you leave this screen. The screen
> asks you to unplug it first.

---

## Wiring

The servo needs its own supply. The board supplies the signal and ground only.

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
  under **MENU → SETTINGS → SIG PIN** and **AUX PIN**, which show the J5 pin
  under the value. GP0 and GP1 are the defaults. AUX PIN acts whether KISS is
  on or off, and a step of either row swaps the two.
- Unplug any ESC from SIG. The screen waits while an ESC answers on
  it, but it cannot see every ESC.

---

## Opening it

Open **MENU → SERVO TESTER**.

The screen takes the two signal pins from the ESC driver. If an ESC answers on
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
`RUNNING` (red) or `STALLED` (magenta). At 333 Hz with a MIN of 1000 µs or
more the line has no room for the number and reads
`SIG  333 HZ  1000-2000 US`, or `SIG+AUX  333 HZ` with two servos.

While SAFE with one servo, the right of the readout row reads `TWO SERVOS:`
and `SET > SERVOS`: the SERVOS row of the SET screen switches servo B on (see
[Two servos](#two-servos)). The hint is not shown while AUX is refused.

With **SERVO SUPPLY** at `GP28` or `GP29` under SETTINGS, a second line under
the title reads `GP28 SUPPLY STAYS OFF HERE.` in amber. The servo tester never
switches the converter of the KST and Hitec programmers on, so a servo that
only has that converter as its supply gets no power on this screen. Give it a
supply of its own, as in [Wiring](#wiring).

<img src="img/servo-supply.png" width="240" alt="The servo screen with SERVO SUPPLY at GP28: GP28 SUPPLY STAYS OFF HERE. under the title">

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

---

## Moving the servo

| Control | Effect |
|---|---|
| Press on the dial, drag | The servo goes to the angle under your finger: the left end is MIN, straight up CENTRE, the right end MAX. Past the ends it stays at MIN or MAX |
| Release | The servo stays where it is |
| `MIN` `CENTRE` `MAX` | On release, to that end or the centre |
| `SWEEP` | A sweep about the centre; the button becomes `STOP SWEEP` |
| `SPEED` | How fast the servo is moved, 10 % to 100 % |

- The dial and the buttons work only while `RUNNING`; while `SAFE` they are
  drawn as outlines and do nothing.
- A drag must start on the dial. A press that starts elsewhere and slides onto
  the dial moves nothing.
- `SPEED` 100 % sends each new position at once, and the servo moves at its
  own speed. Below 100 % the pulse moves gradually: at 10 % a full travel from
  MIN to MAX takes 5 s.
- A sweep starts at the centre and runs its curve on the board's second core,
  so the screen does not slow it down. While it runs, the readout shows
  `SWEEP 3` or `SWEEP 3/10`: the ends reached, and the count you set.
  `STOP SWEEP` leaves the servo where the curve was. A drag, `MIN`, `CENTRE`,
  `MAX` and `STOP` end a sweep too.

<img src="img/servo-sweep.png" width="240" alt="A sweep running">

---

## Two servos

Set `SERVOS` to `TWO` on the SET screen. Servo B is on AUX. Both
share one profile and one frame rate.

<img src="img/servo-two.png" width="240" alt="Two servos">

`A`, `B` and `AB` choose what the dial and the buttons move: one servo, or
both to the same angle. The chosen horn is drawn wide. With `REVERSE B` on, B
moves the opposite way when both follow, for a pair mounted face to face.

Two servos need AUX free. If KISS frames arrive on it, the screen offers one
servo only, and the SET screen says why: `KISS WIRE ON AUX GP1 IS LIVE.`
SETTINGS never puts SIG and AUX on one pin; pins set that way some other
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

With an INA219 current sensor module on J3 and SETTINGS → CURRENT SENSE at
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
  failing; SETTINGS shows `NOT FOUND` under CURRENT SENSE.
- While a sweep runs, its count moves to the top of the dial.

The current is read 40 times a second, each reading the mean over 17 ms.

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
  `STOP TO CHANGE THE PROFILE` and draws their values grey.

  <img src="img/servo-set-running.png" width="240" alt="SET while running: the PULSE rows locked, STOP in the footer">

- While `RUNNING`, `DEFAULTS` becomes `STOP` and acts on touch, as on the
  servo screen. The SWEEP rows stay adjustable.

- A heli type, or a frame rate above 60 Hz, can destroy a servo not made for
  it: above 60 Hz an analogue servo overheats. Such a change waits, the screen
  names the risk, and `HOLD TO APPLY` (1 s) applies it. `BACK` drops it.
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
  change. A heli or 60 Hz change shows its own warning first, and HELI CYCLIC's
  one hold covers both. With TWO servos the hold covers both. Changing SERVOS
  from ONE to TWO on a profile past its travel waits for the hold too, since it
  drives a second servo there; on a profile within its travel, and from TWO to
  ONE, it applies at once. Once applied, the note shows while TYPE, MIN or MAX
  is selected, and on other rows when no other note does. The travel of a servo
  is in its data sheet; the bench does not measure it.
- The SWEEP rows change at any time and apply to the next `SWEEP`. When
  `SPEED` is slower than the curve, the screen says `SPEED LIMITS THE SWEEP`.
- `DEFAULTS`, offered only while `SAFE`, returns to STANDARD PWM at 50 Hz,
  one servo, and a sine at 0.50 Hz.

<img src="img/servo-set-warning.png" width="240" alt="A heli profile waiting for HOLD TO APPLY"> <img src="img/servo-set-travel.png" width="240" alt="TYPE WIDE waiting for HOLD TO APPLY behind the end-stop note">

Nothing on this screen is stored. Every time you open the servo screen it
starts at STANDARD PWM, 50 Hz, one servo.

---

## Leaving

`BACK` ends the pulses and asks you to unplug the servo. The last pulse
completes first, which takes up to 41 ms at 50 Hz. A weak pull-down then
holds SIG low until DShot runs again. With two servos it also holds AUX low
until the KISS receiver takes it, or for good with KISS off.

<img src="img/servo-unplug.png" width="240" alt="UNPLUG THE SERVO">

Unplug the servo, then tap `RESTART DSHOT`. The ESC driver takes the pins back
and `MENU` returns. `BACK` on this page returns to the servo
screen instead, SAFE, and keeps the pins. Until the last pulse has completed
the page reads `RELEASING THE PINS`, and `BACK` is drawn disabled, an outline
with a grey label: it acts from the moment `UNPLUG THE SERVO.` shows.

---

## When it does not work

- **`AN ESC ANSWERS ON SIG GP0.`** An ESC is on SIG, the ESC pin. Unplug it.
- **`SIG GP0 COULD NOT BE TAKEN (PWM)`.** The board could not set up its
  pulse output. Tap `RETRY`.

  <img src="img/servo-bind.png" width="240" alt="SIG GP0 COULD NOT BE TAKEN (PWM), with RETRY">

- **`TWO` cannot be chosen.** The KISS wire on AUX is live, or both pins
  were set to one pin some other way than SETTINGS. The SET screen names the cause when `SERVOS` is selected.
- **The servo does not move.** Check that the chip reads `RUNNING`, that the
  servo has its own supply, and that its ground is joined to the board's.
  With `GP28 SUPPLY STAYS OFF HERE.` under the title, the switched converter
  is off on this screen.
  Whether a servo accepts the board's 3.3 V pulses is not recorded per servo.

Nothing of the servo tester has been tested on hardware yet.
