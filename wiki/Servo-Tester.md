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
> servo on the ESC pin, and when you leave this screen. The screen asks you to
> unplug it first.

---

## Wiring

The servo needs its own supply. The board supplies the signal and ground only.

```
  servo supply, 4.8 V to 6.0 V (8.4 V for an HV (high-voltage) servo)
     +  ----------------------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board GP29 ------------------------------------------  servo A signal (orange or white)
  board GP28 ------------------------------------------  servo B signal, two servos only

  GP29 = J4 pin 12, GP28 = J4 pin 11, GND = J4 pin 1 or 5
```

- The servo's middle wire goes to its supply, never to a pin of the board.
  The board's 5 V pin is its USB supply, and a servo can draw more than 1 A.
- Switch the board on before the servo supply, and the servo supply off
  before the board.
- Join the two grounds once, at the board.
- Servo A is on the ESC pin and servo B on the KISS pin, as set under
  `CFG → SETUP`. GP29 and GP28 are the defaults.
- Unplug any ESC from the ESC pin. The screen waits while an ESC answers on
  it, but it cannot see every ESC.

---

## Opening it

Open `CFG → SERVO`.

The screen takes the two signal pins from the ESC driver. If an ESC answers on
the ESC pin, it waits and says `AN ESC ANSWERS ON GP29.` Unplug the ESC; the
screen takes the pins by itself 500 ms later. Servo pulses of 1000 µs to
2000 µs are a valid throttle signal for many ESCs.

<img src="img/servo-esc.png" width="240" alt="Waiting while an ESC answers">

The screen opens **SAFE**: the pins are held low and carry no pulses. What a
servo does with no pulses is up to the servo: it holds, goes limp, or moves to
its own failsafe position.

<img src="img/servo-safe.png" width="240" alt="The servo screen, SAFE">

The line under the title shows the pin, the frame rate and the pulse range,
and a chip: `SAFE` (green), `RUNNING` (red) or `STALLED` (magenta).

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

Set `SERVOS` to `TWO` on the SET screen. Servo B is on the KISS pin. Both
share one profile and one frame rate.

<img src="img/servo-two.png" width="240" alt="Two servos">

`A`, `B` and `AB` choose what the dial and the buttons move: one servo, or
both to the same angle. The chosen horn is drawn wide. With `REVERSE B` on, B
moves the opposite way when both follow, for a pair mounted face to face.

Two servos need the KISS pin free. If KISS frames arrive on it, or the ESC pin
and the KISS pin are the same pin under SETUP, the screen offers one servo
only, and the SET screen says why.

<img src="img/servo-samepin.png" width="240" alt="TWO not offered: both pins are one GPIO">

---

## The SET screen

`SET` opens the settings. The servo keeps running under it.

<img src="img/servo-set.png" width="240" alt="The SET screen">

Tap a row to select it. `-` and `+` change it one step, and repeat while held.
Swipe the row sideways for big steps. Drag up or down to scroll.

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
- The SWEEP rows change at any time and apply to the next `SWEEP`. When
  `SPEED` is slower than the curve, the screen says `SPEED LIMITS THE SWEEP`.
- `DEFAULTS`, offered only while `SAFE`, returns to STANDARD PWM at 50 Hz,
  one servo, and a sine at 0.50 Hz.

<img src="img/servo-set-warning.png" width="240" alt="A heli profile waiting for HOLD TO APPLY">

Nothing on this screen is stored. Every time you open the servo screen it
starts at STANDARD PWM, 50 Hz, one servo.

---

## Leaving

`BACK` ends the pulses and asks you to unplug the servo. The last pulse
completes first, which takes up to 41 ms at 50 Hz. A weak pull-down then
holds the ESC pin low until DShot runs again. It holds the KISS pin low until
the KISS receiver takes it, or for good with KISS off.

<img src="img/servo-unplug.png" width="240" alt="UNPLUG THE SERVO">

Unplug the servo, then tap `RESTART DSHOT`. The ESC driver takes the pins back
and the settings screen returns. `BACK` on this page returns to the servo
screen instead, SAFE, and keeps the pins.

---

## When it does not work

- **`AN ESC ANSWERS ON GP29.`** An ESC is on the ESC pin. Unplug it.
- **`GP29 COULD NOT BE TAKEN (PWM)`.** The board could not set up its pulse
  output. Tap `RETRY`.

  <img src="img/servo-bind.png" width="240" alt="GP29 COULD NOT BE TAKEN (PWM), with RETRY">

- **`TWO` cannot be chosen.** The KISS wire is live, or both pins are the same
  pin under SETUP. The SET screen names the cause when `SERVOS` is selected.
- **The servo does not move.** Check that the chip reads `RUNNING`, that the
  servo has its own supply, and that its ground is joined to the board's.
  Whether a servo accepts the board's 3.3 V pulses is not recorded per servo.

Nothing of the servo tester has been tested on hardware yet.
