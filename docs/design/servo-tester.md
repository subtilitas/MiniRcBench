# Servo tester

Status: built, host tests only, not tested on hardware. This document is
slice 2 of [pin-claim.md](pin-claim.md). Work breakdown items 1 to 4 and 6 are
built: the pure model `src/servo_model.*`, the PWM driver `src/servo_pwm.*`,
the core1 job `src/servo_job.*` in the job table of `core1_job.cpp`, the files
copied from rcbench in `lib/rcbench/`, `gfxLine()` and `gfxArcDots()` in
`src/gfx.*`, the slider `src/ui_slider.*`, the screens in `src/ui_servo.cpp`,
the live SERVO button on CFG, and the user documentation. Item 5, bring-up on
hardware, is open; [STATUS.md](../../STATUS.md) lists its bench tests. Where
the code and the original plan differ, the Decisions section says how.

Terms: PWM is pulse-width modulation, here the RP2350's hardware PWM slices.
A servo pulse is a high level of 400 µs to 2500 µs repeated at a frame rate.
ESC is electronic speed controller, DShot the digital ESC protocol the tester
drives, UI user interface, SD Secure Digital, GPIO general-purpose
input/output, SDK the Raspberry Pi Pico software development kit and CI
continuous integration. SIG and AUX are the two signal pins as
[pin-claim.md](pin-claim.md) defines them: the ESC pin and the KISS telemetry
pin.

Source: the interaction and the limits come from rcbench, commit 453cf334:
`shared/ui/servo_screen.c`, `shared/ui/ui_slider.c`,
`shared/servo/servo_sweep.c`, `firmware/iomcu/src/out_pwm.c`,
`shared/outputs/out_pwm_map.c` and `shared/outputs/outputs.h`. rcbench is MIT
licensed by the same holder.

## What it does

- Drives one servo on SIG, or two servos on SIG and AUX, with standard servo
  pulses from the hardware PWM.
- Manual position: drag the drawn horn; MIN, CENTRE and MAX buttons; a pulse
  range and a frame rate set on the SET screen.
- Sweep: square, sine or triangle about the centre, with rate, amplitude,
  dwell and an optional count of movements (`servo_sweep.c`).
- SPEED: a slew limit on every movement, from 10 % to 100 %.

It does not measure the servo. There is no position feedback, so the horn is
drawn where the pulse puts it, not where the servo is.

## Safety model: SAFE and RUNNING

A servo that jumps to a position is a hazard on a mounted linkage, the same
way a throttle step is on a motor. The screen has the motor screen's two
states under other names.

| | Motor (main screen) | Servo |
|---|---|---|
| Off state | DISARMED: throttle-zero frames | SAFE: the pins bound to PWM at level 0, so no edges and the line low (decision 6) |
| Starting | HOLD TO ARM, `ARM_HOLD_MS` 1000 ms, throttle at zero for `ARM_ZERO_THROTTLE_MS` | HOLD TO START, `ARM_HOLD_MS` 1000 ms, with the progress bar AM32's HOLD TO WRITE draws |
| First output | throttle 0 | the CENTRE pulse on every bound pin, whatever was commanded before |
| Stopping | DISARM, on press, instant | STOP, on press, instant: level 0 from the next frame |
| UI stalls 250 ms | throttle forced to zero | level 0, sweep ends, `Tripped` latched while RUNNING |
| Idle | auto-disarm after `IDLE_DISARM_MS`, 30 s | none: a sweep may run unattended as an endurance test |

rcbench does the same: until the bench is ARMED the coprocessor writes a
pulse of length zero to every PWM pin, ARM is a hold, and at the arm every
surface goes to its rest, the centre. rcbench holds ARM for 2 s; this screen
uses MiniRcBench's 1 s so that every hold on the device is one length
(decision 1).

A hold fires once. HOLD TO START needs a new press after it has fired, and
the press that STOPs the servo does not start it again, however long it is
held.

STOP writes level 0 at once, and the compare register is double-buffered, so a
pulse already on the wire completes. The last pulse ends at most one frame
later: 20 ms at 50 Hz. The pins stay bound to PWM at level 0.

A release or a rebind waits for that last pulse. The job writes level 0 and
keeps the slice running for `servoQuietMs()` after the write, two frames plus
1 ms (41 ms at 50 Hz, 9 ms at 333 Hz), before it switches the pins away from
PWM. The wait spans the wrap that latches level 0 and one full frame after
it, so the slice never cuts a pulse short. A pin that never carried a pulse
since its bind is released at once.

While SAFE, two lines at the top of the dial band say
`THE SERVO MOVES TO CENTRE AT START.` and `PULSES ON GP29 ARE THROTTLE TO AN
ESC.` (the live SIG pin). The first gives the rule: starting at the last
commanded position instead would move the servo as far from wherever it
rests, to a place nobody chose this session; centre is the position a servo
is mounted and trimmed at. The second covers what the ESC check of
[pin-claim.md](pin-claim.md#interlocks) misses: an ESC that does not answer
in bidirectional DShot, and an ESC plugged in after the claim.

What a servo does with no pulses is the servo's own: hold, go limp, or move
to a failsafe position. It is not recorded per servo.

## Pulse limits

All of these are pure functions in `src/servo_model.cpp` and host-tested.

| Rule | Value | Source |
|---|---|---|
| Pulse floor and ceiling | 400 µs and 2500 µs, refused outside | `OUT_FLOOR_US`, `OUT_CEILING_US` |
| Frame rate | 40 Hz to 560 Hz | rcbench PWM driver table: below 40 Hz a servo steps audibly; 560 Hz is a 760 µs tail servo's rate |
| Pause after a pulse | 1000 µs, 500 µs for the heli profiles | `PWM_MIN_PAUSE_US`, `HELI_MIN_PAUSE_US` |
| Fastest rate for a range | `1,000,000 / (MAX + pause)`, under the profile's own ceiling | `max_rate_for()`: 333 Hz for 2000 µs |
| Pulse against the frame | at most period − 1 µs: 1784 µs at 560 Hz, 2499 µs at 400 Hz | `out_pwm_pulse_max()` |
| Order | MIN < CENTRE < MAX | |
| Warning before applying | a heli profile, or a rate above 60 Hz | `SAFE_RATE_HZ`: an analogue servo overheats |

The profiles are rcbench's:

| TYPE | MIN | CENTRE | MAX | Rates offered | Default | Ceiling |
|---|---|---|---|---|---|---|
| STANDARD PWM | 1000 µs | 1500 µs | 2000 µs | 50, 60, 100, 150, 200, 250, 300, 333 Hz | 50 Hz | pause rule |
| NARROW 760 | 660 µs | 760 µs | 860 µs | as STANDARD | 50 Hz | pause rule |
| WIDE | 800 µs | 1500 µs | 2200 µs | as STANDARD | 50 Hz | pause rule |
| HELI CYCLIC | 820 µs | 1520 µs | 2220 µs | 50, 120, 200, 333 Hz | 333 Hz | 333 Hz |
| HELI TAIL 760 | 410 µs | 760 µs | 1110 µs | 200, 333, 560 Hz | 560 Hz | 560 Hz |

Steppers only reach legal values, as SETUP's pin steppers only reach free pins:
MIN, CENTRE and MAX move in 10 µs steps and stop at the floor, the ceiling and
each other; RATE steps through the offered rates up to the ceiling the range
allows. There is no invalid value to reject. In numbers:

- MIN stops at 400 µs and 10 µs under CENTRE; CENTRE stops 10 µs over MIN and
  10 µs under MAX (decision 9);
- MAX stops 10 µs over CENTRE and at `servoMaxPulseFor()`: 2500 µs, or less
  where the rate leaves less room after the pause, for example 2003 µs at
  333 Hz. A step that would pass a stop changes nothing, so every value stays
  on the 10 µs grid;
- RATE stops at the last offered rate at or under `servoMaxRateFor()` of MAX;
- TYPE stops at the ends of the table and loads the type's MIN, CENTRE, MAX
  and default rate.

`servoProfileValid()` holds the same rules, and `servoJobProfile()` refuses a
profile that breaks one.

Positions are commands from 0 to 1000 with the centre at 500, as in rcbench
(`OUT_SPAN`). A command maps onto the narrowest range centred on CENTRE that
holds MIN and MAX, clamped to MIN..MAX (`cmd_range()`), so an asymmetric
CENTRE rests the servo at CENTRE. On the dial, −90° is MIN and +90° is MAX,
each side scaled to its own half of the travel (`deg_to_us()`).

SPEED runs from 10 % to 100 %. Below 100 % the pulse slews at
`2 × 1000 × SPEED / 100` command units a second: at 10 % a full MIN to MAX
travel takes 5 s. 100 % is no slew, rcbench's default, so a user who never
touches SPEED gets the servo at its own rate. The SET screen says
`SPEED LIMITS THE SWEEP` when `sweep_slew_limited()` is true. A change of SPEED
while RUNNING reaches core1 as its own command, `Slew`, so a move under way
takes the new rate at once.

## The horn is the control

rcbench `servo_screen.c`: "The horn is the control: dragging the drawn arm
commands the servo." The screen keeps that interaction and its rules.

- A press on the dial commands the angle under the finger, and every move
  after it commands the new angle. It is absolute: the servo goes where the
  finger is.
- On release the servo holds the last position. There is no spring.
- A press on the dial while SAFE is refused and takes no drag. A servo started
  under a finger moves nothing until the next press (rcbench: "the press is
  the dial's and takes no drag").
- A drag under way at START or STOP ends. After the edge the first input is a
  new press (rcbench `servo_screen_set_armed()`: "A finger on the dial does not
  carry its drag across the edge").
- A drag, MIN, CENTRE, MAX, STOP, BACK, a heartbeat trip and a profile change
  each end a running sweep (rcbench `stop_sweep()`: "a drag, CENTRE, RELEASE,
  a curve restarted, a disarm, leaving, the link going").
- A press that begins off the dial never becomes a drag, however far it
  travels across it (`ui_slider.h`: "a press that starts anywhere else never
  becomes a drag"). `TouchState.downX` and `downY` give the press origin.
- A press inside the hit area but past ±90° commands the end of travel. The
  arc's ends saturate, which is the dial's version of the throttle track's
  `THR_EDGE_SAT`: both rails are reachable without reaching the rim.

### Against the throttle drag

| | Throttle gauge (`ui.cpp`) | Servo horn |
|---|---|---|
| Surface | track 224 × 34 px at y 248, 8 px slop above, 1 px below | annulus around the shaft, 24 px to 124 px radius, ±104° of straight up, clipped to the dial band |
| Spring mode | absolute: finger position is throttle, back to 0 on release | none |
| HOLD mode | relative, by distance dragged (`HOLD_DRAG_SENSITIVITY_PCT`), so a touch never jumps the motor | always absolute; the release holds |
| Large pad | relative swipe with a 6 px deadzone (`PAD_DEADZONE_PX`) | none |
| Edge | 16 px saturation at each end (`THR_EDGE_SAT`) | ±90° to ±104° saturates at MIN or MAX |
| Jump limit | relative modes, ceiling (`maxThrottle`) | SPEED slew, pulse range |
| Gate | armed | RUNNING |

The horn is absolute on purpose. A servo's position is the thing being
tested, and a position control that does not put the servo where the finger
is would not say what the servo does. The jump that absolute input allows is
bounded by the pulse range and slowed by SPEED.

### Two servos

Two horns share one dial: A in `C_LIME`, B in `C_CYAN`. A selector of three
buttons, `A`, `B` and `AB`, sits in the readout row; `AB` is selected when the
screen opens. The selection decides what the drag, MIN, CENTRE, MAX and SWEEP
drive:

- `A` or `B`: that servo follows; the other holds its pulse. `B` alone follows
  unmirrored, REVERSE B or not (decision 10).
- `AB`: both follow to the same angle. REVERSE B on the SET screen mirrors B,
  for a pair mounted face to face.

The selected horn is drawn 5 px wide with its grip, the other at 1 px. A pin
with no pulse has no horn: there is nothing to say where that servo is.
The touch driver reports one contact (`TouchState`, from the CST328), so one
finger drives one selection; there is no second drag.
START starts both pins at CENTRE. STOP stops both.

Both servos share one profile and one frame rate. SIG (GP29) and AUX (GP28)
are channels B and A of PWM slice 6, which forces one rate anyway.

## Screens

Entry: CFG → SETTINGS → SERVO, in the destination row
([pin-claim.md](pin-claim.md), decision 2). The main screen is the root
and gets no entry; CFG is its navigation.

### SERVO

```
y   0 +------------------------------------------+
      | SERVO                            [ BACK ]|  header, UI_HDR_H 30 px
   34 | GP29  50 HZ  1000-2000 US          (SAFE)|  strip: caption, chip
   58 | THE SERVO MOVES TO CENTRE AT START.      |  two notes while SAFE
   56 |                                          |
      |               .   -   -   .              |
      |          .          |          .         |  dial band 56..199
      |       .             |             .      |  arc radius 96 px
      |     .               |               .    |  shaft at (120, 184)
      |    MIN              o              MAX   |  0 deg is straight up
  184 |                  (shaft)                 |
  200 |               1500 US                    |  readout, scale 2
  227 | SPEED [=======O-----------------]  100%  |  slider, track 150 x 22 px
  254 | [ MIN ] [CENTRE] [ MAX ] [ SWEEP ]       |  4 x 53 px, 30 px tall
  287 | [   HOLD TO START    ]       [  SET  ]   |  150 px and 72 px, 32 px tall
  320 +------------------------------------------+
```

Two-servo readout row, 200..223:

```
  200 | A 1500  B 1620              [A] [B] [AB] |  34 x 22 px each
  214 | SWEEP 3/10                               |  while sweeping
```

- Header and BACK are `uiHeader()`. The strip carries the live pin, the rate
  and the range as a caption (`GP29+28` for two servos), and a chip: `SAFE` in
  `C_GREEN`, `RUNNING` in `C_RED`, `STALLED` in `C_MAGENTA` after a heartbeat
  trip, when the notes read `STOPPED: SCREEN STALLED` and
  `HOLD TO START AGAIN.`
- The dial: arc, MIN and MAX ticks, and the horn at the pulse core1 reports,
  so a slewed move is drawn as it happens.
- The readout: the pulse in µs, or `NO PULSE`; while sweeping, `SWEEP 3/10`
  or `SWEEP 3` on the right, the ends reached and the count asked for. With
  two servos the readout is one line at scale 1 and the sweep count goes under
  it.
- SPEED: the rcbench slider with tap-to-set, because SPEED commands nothing by
  itself (`servo_screen.c` turns it on for the same reason).
- MIN, CENTRE, MAX fire on release, like every MiniRcBench button, and are
  drawn disabled while SAFE. SWEEP reads `SWEEP`, or `STOP SWEEP` while one
  runs; a stopped sweep leaves the servo where the curve was. A sweep starts
  its curve at the centre, so a servo held elsewhere moves to the centre
  first, at SPEED.
- HOLD TO START becomes `STOP` while RUNNING, in `C_RED`, and acts on press.
  The bottom-left position is the main screen's ARM position.
- SET opens the SET screen. The claim and the pulses continue under it.

Layout constants get `static_assert`s for every gap, as `ui.cpp` and
`ui_am32.cpp` have, and every button clears `UI_TAP_MIN`, 22 px. Each region
repaints only when the values it shows change, and a change of view clears
the panel first.

While the pin is unavailable, the dial band holds the message from
[pin-claim.md](pin-claim.md) and HOLD TO START is drawn disabled. The
messages the screen shows there: the ESC answering on SIG, wrapped over three
lines; `DISARM FIRST` and `PINS BUSY`, which the UI does not reach;
`RELEASING DSHOT PIN` during the drain; `TAKING GP29` until the job binds; and
`GP29 COULD NOT BE TAKEN (PWM)` with a RETRY button, for a first bind or a
rebind core1 refused.

### SET

The AM32 list idiom: a scrolling list of rows (26 px), an editor bar with
large − and + (hold to repeat, `repeatFires()`) and a horizontal swipe for
coarse steps, and a footer.

```
y   0 | SERVO SET                        [ BACK ]|
   36 | STOP TO CHANGE THE PROFILE               |  note band, two lines
   56 | PULSE   SERVOS                      ONE  |
      | PULSE   TYPE               STANDARD PWM  |
      | PULSE   RATE                      50 HZ  |
      | PULSE   MIN                     1000 US  |
      | PULSE   CENTRE                  1500 US  |
      | PULSE   MAX                     2000 US  |  list 56..215, scrolls
      | PULSE   REVERSE B                    OFF |
      | SWEEP   SHAPE                       SINE |
      | SWEEP   RATE                     0.50 HZ |
      | SWEEP   AMPLITUDE                  100 % |
      | SWEEP   DWELL                     500 MS |
      | SWEEP   MOVES                    ENDLESS |
  218 | [ - ]           RATE 50 HZ           [ + ]|  editor bar, 44 px
  268 | [  HOLD TO APPLY  ]      [ DEFAULTS ]    |  footer, 46 px, SAFE
  268 | [  HOLD TO APPLY  ]      [   STOP   ]    |  footer, 46 px, RUNNING
```

- PULSE rows change only while SAFE. A new rate needs the PWM slice rebuilt
  (`servoPwmBind()` refuses a second rate on a bound pin), and a range changed
  under a moving servo moves it. While RUNNING their values are drawn grey,
  their steppers are drawn disabled and the note band says
  `STOP TO CHANGE THE PROFILE`.
- SWEEP rows change at any time and apply to the next SWEEP. Ranges: SHAPE
  square, sine, triangle; RATE 0.05 Hz to 5.00 Hz in 0.05 Hz steps
  (`SWEEP_MHZ_MIN`, `SWEEP_MHZ_MAX`); AMPLITUDE 0 % to 100 % of half the
  travel in 5 % steps (0 to `SWEEP_AMPLITUDE_MAX`, 500 units); DWELL 0 ms to
  5000 ms in 100 ms steps; MOVES endless or 1 to 9999 (decision 12).
- A − or + step is one step; a sideways swipe of 16 px is one coarse step:
  10 steps for MIN, CENTRE, MAX and the SWEEP numbers, one for a choice.
- A TYPE or RATE change whose result needs the warning (a heli TYPE, a RATE
  above 60 Hz) is held as pending. The note band names the risk on two lines,
  `ABOVE 60 HZ AN ANALOGUE` `SERVO OVERHEATS.` or `A HELI PROFILE CAN
  DESTROY` `A SERVO NOT MADE FOR IT.`, and HOLD TO APPLY (1000 ms) applies it.
  Every other change applies at once. BACK drops a pending change
  (decision 11).
- SERVOS TWO is drawn disabled when the claim refuses AUX: KISS live or SIG
  and AUX on one GPIO. With the SERVOS row selected, the note band shows the
  screen texts of the pin-claim design
  ([pin-claim.md](pin-claim.md#when-the-pin-is-unavailable)); with another row
  selected it says `TWO SERVOS ARE NOT OFFERED: SEE SERVOS`.
- DEFAULTS returns to STANDARD PWM at 50 Hz, one servo, REVERSE B off, and a
  sine at 0.50 Hz, 100 %, 500 ms, endless. It changes the PULSE rows, so it
  is offered only while SAFE.
- While RUNNING the right footer button is `STOP`, in `C_RED`, and acts on
  press, as STOP on SERVO does (decision 16). The press that stops the servo
  is not also a tap on DEFAULTS: DEFAULTS needs a new press. The SWEEP rows
  stay adjustable while RUNNING, and so does SPEED on SERVO.

The note band is two lines under the header, rows 34 to 55, where the other
screens have a one-line strip: the warnings and the pin-claim texts do not fit
one line of 240 px.

### The unplug page

BACK on SERVO, once a job has held the pins, shows the page of
[pin-claim.md](pin-claim.md#restoring-dshot): `UNPLUG THE SERVO.`, `DSHOT
RESTARTS ON GP29 AND READS AS` `A 970 US PULSE AT 1000 HZ.`, and RESTART
DSHOT, a tap. Until core1 has released the pins the page says
`RELEASING THE PINS`, and BACK on it does nothing. Once they are released,
BACK on the page returns to SERVO and keeps the claim; the job binds again,
SAFE. BACK from a claim still draining, or with no
claim, leaves at once: nothing was driven.

## Where it runs

Core0 owns the screen and sends commands. Core1 owns the pins
([pin-claim.md](pin-claim.md), "What core1 does").

```c
enum class ServoCmdKind : uint8_t { Safe, Start, Position, Sweep, SweepStop, Slew };

struct ServoProfile {             // servo_model.h
    ServoType type;               // the rates offered and the pause
    uint16_t  minUs, centreUs, maxUs;
    uint16_t  rateHz;
    uint8_t   servos;             // 1 or 2
    bool      reverseB;
};

struct ServoCmd {
    uint16_t     seq;        // set by servoJobCommand(); core1 echoes it
    ServoCmdKind kind;
    uint8_t      mask;       // bit 0 servo A, bit 1 servo B
    uint16_t     command;    // 0..1000, centre 500
    uint16_t     slewPerS;   // command units a second, 0 for none
    sweep_cfg_t  sweep;      // servo_sweep.h
};

struct ServoStatus {
    uint16_t ackSeq;
    bool     running, tripped, sweeping;
    uint8_t  bindError;      // 0, or CORE1_JOB_PWM for a refused rebind
    uint16_t outUs[2];       // what each pin carries now, 0 for no pulse
    uint32_t sweepMoves;
};

bool     servoJobProfile(const ServoProfile *p);  // core0; false while RUNNING,
                                                  // with a command untaken, or invalid
uint16_t servoJobCommand(const ServoCmd *c);      // core0; returns the seq held
void     servoJobStatus(ServoStatus *out);        // core0, under the job's lock
```

The three blocks, command, profile and status, sit under one critical
section of the job's own, initialised on core0 through the init hook
`Core1JobOps` has for it. One command is held at a time and a new one
replaces one core1 has not taken, except a `Safe`: an untaken STOP stays and
a command sent after it is dropped.

Every pass of `loop1()`, the job:

1. takes a new profile if one was handed over: writes level 0, waits
   `servoQuietMs()` for the last pulse, releases both pins and binds them
   again at the new rate, or leaves both unbound and reports `bindError`;
   the trip latch is kept across the rebind;
2. takes a new command if `seq` moved (`servoCoreTake()`); `Start` is
   refused while the heartbeat has lapsed and with SIG unbound, `Sweep` while
   SAFE; a `Position` while SAFE sets a target that moves nothing until
   `Start` replaces it;
3. checks `core1UiAlive()`; on a lapse while RUNNING it ends the pulses and
   the sweep and latches `tripped` (`servoCoreStep()`);
4. computes the target: the sweep curve at `millis()`, or the position;
5. moves the output towards it by the slew owed since the last pass;
6. writes the compare register when the pulse changed (`servoPwmWrite()`);
   the slice latches it at the end of the frame;
7. publishes the status.

Steps 2 to 5 are the pure functions `servoCoreTake()` and `servoCoreStep()`
on a plain struct, `ServoCore`, which the host suite drives directly. The
`Start` with the heartbeat lapsed is refused in step 2, so no trip is
latched; a `Position` or `Sweep` taken with it lapsed is ended in step 3 of
the same pass.

A drag reaches core1 at the UI rate, 40 Hz, one `Position` command per frame.
Core1 renders each at the next PWM frame and slews between them, so the
servo does not step at 40 Hz below 100 % SPEED.

Profile and bind: `servoJobProfile()` in SAFE releases and rebinds both pins
at the new rate. A refused bind leaves both pins unbound and reports
`bindError`; the screen shows `GP29 COULD NOT BE TAKEN (PWM)` and its RETRY
hands the profile over again. The first bind, at the claim, takes the profile
the screen handed over on entry; a command sent before the bind is not taken.

The claim asks for SIG and AUX together. When it refuses AUX, for a live KISS
wire or one GPIO for both, the screen claims SIG alone and offers one servo
for the rest of its visit (decision 7). While the screen waits for the pins,
for example while an ESC answers, the claim refuses for that reason first, so
TWO can be chosen until a claim finds AUX refused. The job binds AUX only for
a profile with two servos.

## Porting

| rcbench file | Here | How |
|---|---|---|
| `shared/servo/servo_sweep.c`, `include/servo_sweep.h` | `lib/rcbench/shared/servo/` | copied as they are |
| `shared/outputs/out_pwm_map.c`, `include/out_pwm_map.h` | `lib/rcbench/shared/outputs/` | copied as they are |
| `firmware/iomcu/src/out_pwm.c`, `.h` | `src/servo_pwm.cpp`, `.h` | adapted: 2 bindings instead of the bank's slot count, `link_pages.h` and `outputs.h` dropped, the stamped write left out, the bind, release, write and sharing rules kept; the pulls are switched off at the bind, and the release leaves an input with the pull-down on |
| `shared/ui/ui_slider.c`, `include/ui_slider.h` | `src/ui_slider.cpp`, `.h` | adapted: the event logic kept (delta drag, tap-to-set, presets, the two press contracts); `touch_event_t` replaced by `TouchState` (`pressed` is DOWN, `down` is MOVE, `released` is UP, one contact); drawing rewritten on `gfx.h` and the theme colours; the scale ticks and the unused step left out |
| `shared/ui/servo_screen.c` (6957 lines) | `src/servo_model.cpp` (pure) and `src/ui_servo.cpp` | rewritten. Taken: the profile table, `max_rate_for()`, `dangerous()`, `cmd_range()`, `deg_to_us()`, `us_to_deg()`, `slew_of()`, `on_the_dial()` rotated to point up, the drag and stop rules. Left out: the encoder feedback, the limit run, the automatic test, the supply and PD mini rows, the power plot, the keypad, the tabs, the pause and resume with its link lineage (core0 and core1 share memory, so no command is ever in flight long enough to need it) |
| `test/host/test_servo_sweep.c` | `test/rcbench/` | copied as it is, with rcbench's harness `greatest.h` |
| out_pwm_map cases of `test/host/test_outputs.c` | `test/rcbench/test_out_pwm_map.c` | adapted: the map cases only |

`gfx.cpp` gains a thick line and an arc of dots for the horn and the dial.
The other primitives are rectangles, rounded rectangles, lines along an axis,
text and seven-segment digits.

Copied files keep their text and their `SPDX-License-Identifier: MIT` line.
`lib/rcbench/README.md` lists each copied file with its rcbench path and the
commit. Adapted files carry `Adapted from rcbench <path>, commit 453cf334,
MIT.` at the top of their header comment. Both projects are MIT under the
same holder, so no licence changes. `tools/notices.py` counts `lib/rcbench/`
as MiniRcBench's own code, under its licence. The copied headers are on the
include path of the firmware (`src/servo.cmake`), of the host suite
(`test/servo.mk`) and of the config permutations in CI.

## Testing

Built. Ported, built as C11 in `test/rcbench/` and run by `make run`:
`test_servo_sweep.c`, 14 cases, and the out_pwm_map cases, 5.

New, `test/test_servo.cpp`, which includes `ui_servo.cpp` to read its
file-static state. `servo_pwm.cpp` runs against a register fake in
`test/fakes.cpp` that records each pin's function, level and pull and each
slice's wrap, divider and enable. The fake models the counter at 1 MHz from
the slice's enable, the wrap, and the compare latched at each wrap from the
writes before it, and counts a pulse cut short when a pin leaves PWM or its
slice stops mid-pulse. What the SDK does with those calls on the chip is not
tested on the host. The job runs from the job table, through the
claim and the fake DShot pump, with the test calling `core1JobPoll()` between
UI frames:

- limits: each profile within 400 µs to 2500 µs; steppers stop at the floor,
  the ceiling and each other; the rate ceiling (333 Hz for 2000 µs plus
  1000 µs); the pulse against the frame (1784 µs at 560 Hz); the warning set;
- mapping: command to µs with a symmetric and an asymmetric CENTRE; ±90° to
  MIN and MAX; saturation from 90° to 104°; REVERSE B;
- hit test: inside the annulus, inside 24 px, outside 124 px, past ±104°,
  below the dial band, and a press that began off the dial;
- the job against a fake PWM: level 0 while SAFE; CENTRE on every bound pin at
  START; slew per SPEED; sweep start and stop; heartbeat trip latched, not
  cleared by the heartbeat coming back nor by a rebind, cleared by a new
  visit to the screen;
- the last pulse: no pulse cut short at a release from RUNNING, at a rebind
  from RUNNING then SAFE, nor at `pinClaimEnd()` while RUNNING; the claim
  stays `Leaving` until the job is idle, and BACK on the unplug page does
  nothing until then; the status reads SAFE during the wait; a released pin
  is an input with the pull-down on;
- a new visit, refused while an ESC answers or before the job binds, does
  not show the last visit's trip;
- the stop rules: a drag, MIN, CENTRE, MAX, STOP, BACK, a profile change and a
  trip each end a sweep; a drag across START and STOP ends; a press while SAFE
  takes no drag;
- two servos: the selector, `AB` with REVERSE B.

The screen tests are in `test/test_servo.cpp` too, through the real UI from
CFG: the SERVO, SET and unplug screens render, the disabled states draw
disabled and do not act, HOLD TO START fires at 1000 ms and not at 999 ms nor
on a slide off, STOP fires on press, the KissLive and SamePin refusals reach
the SET screen through the claim, an ESC answering on SIG holds the claim off
until 500 ms after its last frame, and the unplug page comes before DShot
restarts. `test/test_ui.cpp` checks the CFG row: SERVO live and opening its
screen, KST disabled (decision 14). `make previews` adds
`docs/servo-preview.png`.

## Work breakdown

1. `servo_model.*` and its tests. Pure; no hardware.
2. `servo_pwm.*`, `servo_job` in `core1_job.cpp`'s dispatch, the vendored
   sweep and map, the ported tests.
3. `gfx` line and arc, `ui_slider.*`.
4. `ui_servo.cpp`, SET, the CFG entry enabled, UI tests, preview.
5. Bring-up: a scope on SIG and AUX at 50 Hz and 560 Hz; one
   servo, two servos; STOP latency; heartbeat trip by halting core0 in a
   debugger. Open; [STATUS.md](../../STATUS.md) lists the bench tests.
6. README section "Servo tester" with the wiring diagram and the unplug
   warning; wiki pages `Servo-Tester.md` and its German counterpart, and
   their entries in `_Sidebar.md` and the screens pages.

Items 1 to 4 and 6 are built.

## Decisions

Decisions of the owner, 2026-10-17: recommendations 1 to 5 are accepted.

1. **START hold length.** 1000 ms (`ARM_HOLD_MS`), one hold length on the
   device.
2. **Persist the profile.** Not stored in flash. Every boot starts at
   STANDARD PWM 50 Hz, so a heli profile or a high rate is never applied to
   the next servo by accident, and `SETTINGS_VERSION` stays 2.
3. **One profile or two.** One shared profile with REVERSE B. SIG and AUX
   share a slice and therefore a rate.
4. **Sweep pause.** Not in this slice. STOP SWEEP holds the servo where the
   curve was, and SWEEP starts a new curve from the centre.
5. **SPEED default.** 100 %, no slew, as rcbench starts.

6. **SAFE output.** No pulses: the pins bound to PWM at level 0, no edges,
   as rcbench does. Decision of the owner, 2026-10-18. A servo held at a
   pulse nobody commanded this session is worse than one let go; that is
   rcbench's rule for `out_pwm_write(pin, 0)`.

Choices made in building slice 2, where the design above was silent or the
code needed a rule it did not give:

7. **AUX refused.** The screen asks for SIG and AUX; on `KissLive` or
   `SamePin` it claims SIG alone. Two cases follow:
   - The claim of SIG alone goes through. TWO is not offered for the rest of
     the visit: the screen claims only while it holds no claim, and BACK on
     the unplug page keeps the roles it holds. A profile set to TWO before
     the claim falls back to one servo.
   - The claim of SIG alone is refused too. A later frame asks for SIG and
     AUX again, and when that claim goes through the AUX refusal is cleared
     and TWO is offered without leaving the screen. This is a race the code
     handles but the UI does not reach: `pinClaimCheck()` tests
     `EscAnswering` before `KissLive` and `SamePin`, so a refusal of SIG
     alone right after a refusal of AUX needs the facts to change between
     the two calls. `testClaimCorners()` sets it up by hand.

   While the screen waits for the pins, for example while an ESC answers, the
   claim of both returns `EscAnswering`, no AUX refusal is recorded and TWO
   can be chosen. When the ESC goes quiet with AUX refused, the screen takes
   SIG alone and drops a TWO profile to one servo.
8. **Profile at entry.** Every entry to SERVO starts at STANDARD PWM, 50 Hz,
   one servo, SPEED 100 % and the default sweep, not only every boot. BACK
   leads through the unplug page, so the next entry is likely a different
   servo, and decision 2's reason applies to it.
9. **Gap between MIN, CENTRE and MAX.** One step, 10 µs. rcbench keeps 50 µs;
   the design states only the order. A stepper that would cross a limit does
   not move, so values stay on the 10 µs grid.
10. **REVERSE B.** Applies when both servos follow (`AB`). With `B` selected
    alone, B follows the finger unmirrored, so its horn stays under the
    finger.
11. **The warning.** Only a TYPE or RATE change whose result needs it waits
    for HOLD TO APPLY; a MIN change under an applied heli profile applies at
    once. BACK from SET drops a pending change. DEFAULTS is offered only
    while SAFE; decision 16 puts STOP in its place while RUNNING.
12. **SWEEP steps.** RATE 0.05 Hz, AMPLITUDE 5 %, DWELL 100 ms, MOVES 1; a
    swipe step is 10 of them.
13. **Commands.** `ServoProfile` carries the type; `ServoCmdKind` gains
    `Slew`, so SPEED applies to a move under way; `servoJobCommand()` returns
    the seq and never replaces an untaken `Safe`; `servoJobProfile()` returns
    whether it took the profile. The trip latches only while RUNNING, since
    while SAFE it stops nothing. STOP SWEEP leaves the targets where the
    curve was; with SPEED below 100 % the outputs finish their slew to them.
14. **Where the screen tests live.** In `test_servo.cpp`, not
    `test_ui.cpp`, so slice 3 does not edit the same test file; `test_ui.cpp`
    checks the CFG row only.
15. **Core1JobOps init.** A fourth member, `init`, called by
    `core1JobInit()` on core0, initialises a job's critical section before
    core1 starts. Null for a job with nothing to initialise.

Decision of the owner, 2026-10-18:

16. **STOP on SET.** While RUNNING, the SET footer shows STOP in place of
    DEFAULTS, and it behaves like STOP on SERVO: level 0 on press, the sweep
    ends. The SWEEP rows and SPEED stay adjustable while RUNNING.

Further choices made in building slice 2:

17. **The last pulse at a release.** `Core1JobOps::release` takes the time
    and returns whether the pins are released. The servo job returns false
    until `servoQuietMs()` has passed since level 0 was written, and the
    claim stays in `Leaving`, or the rebind waits, until then. Core1's
    `loop1()` is not blocked: each pass asks again. The DShot pump is
    suspended for the whole claim.
18. **The trip latch.** `tripped` is kept across a rebind and across a
    release, and is cleared by `Start` or by a new visit to the screen:
    `servoJobNewSession()` clears the published latch at once, so the chip
    of a refused or draining visit never reads STALLED, and core1's own
    latch at the next bind. A profile change does not hide a trip.
19. **Released pins.** `servoPwmRelease()` leaves the pin an input with the
    pull-down on, so the servo lead does not float between the release and
    DShot's bind. DShot sets the pull-up on SIG at its bind, and the KISS
    receiver sets the pull-up on AUX when KISS is on.

Decision of the owner, 2026-10-18, on the switched servo supply that
[kst-programming.md](kst-programming.md#switched-servo-supply) designs:

20. **Switched supply.** A change after KST programming uses it here too:
    SAFE is supply off, HOLD TO START switches it on, and STOP and a
    heartbeat trip switch it off. Not built; until then the servo's supply is
    the user's.
