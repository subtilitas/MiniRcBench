**English** | [Deutsch](Winkelmesser)

# Angle Meter

`MENU → TOOLS → ANGLE METER` measures angles with the accelerometer on the
board itself, from the direction of gravity while the board lies still: a
wing's incidence, the decalage between wing and stabiliser, dihedral, and a
control surface's throw in degrees and as the trailing edge's travel in mm.
It takes no signal pin and needs no wiring.

Not tested on hardware: nothing of the meter has run on a board yet. The
accuracy of its readings is not measured. The screen shows tenths of a
degree and of a millimetre; that is how fine the display is, not how right
the reading is. Until the bench measures the meter against known angles,
treat a reading as a comparison of two settings, not as an exact angle.

<img src="img/tools-chooser.png" width="240" alt="TOOLS: ANGLE METER with INCIDENCE, DECALAGE, DIHEDRAL, THROW under it"> <img src="img/angle-main.png" width="240" alt="ANGLE METER after a zero: -20.0 degrees from the zero, -13.7 MM with a length of 40 mm, MAX -20.0 degrees">

---

## The main view

| Line | What it shows |
|---|---|
| Strip | The board edge that lies along the axis, `LONG EDGE` or `SHORT EDGE`, and that axis's tilt from the horizontal in whole degrees, amber from 45°. The chip: `SETTLED` (green), `MOVING` (a frame with a dim label, no fill), `TOO STEEP`, `STARTING` or `NO IMU` (amber) |
| Angle | The angle in tenths of a degree, from the zero, or from level before a zero; the line under it reads `FROM ZERO` or `NOT ZEROED: FROM LEVEL`. `---` while the meter starts, when no reading has come for 300 ms, and while the axis is tilted 70° or more |
| Travel | With LENGTH set in SETUP, how far the trailing edge has moved, and `L 40 MM, HINGE TO TRAILING EDGE`. Without it, `SET LENGTH IN SETUP FOR MM` |
| MAX | The largest settled angle each side since ZERO or RESET MAX, the negative side first, `--` for a side with none; under it their travel |
| Temperature | The temperature of the board's sensor, and after a zero its change since then, amber from 2.0 °C |

`SETTLED` means the angle has stayed within 0.5° for one second. Take
readings only while it shows.

- **ZERO** waits for a settled reading after you lift your finger: the
  hint reads `ZERO WHEN SETTLED: HOLD STILL`, and the zero is taken from
  the first settled reading at least 1 s after the lift, so the tap's own
  jolt has passed. With no settled reading within 10 s it reads
  `NOT SETTLED IN 10 S: NO ZERO`, and the old zero stays. ZERO clears MAX.
  It is drawn as an outline while the sensor does not answer.
- **RESET MAX** clears MAX.
- **SETUP** opens SETUP.
- **BACK** turns the sensor off and returns to TOOLS. The zero and MAX are
  gone then: the next visit starts from level, since the model may have
  moved in between.

The meter cannot tell that the model or the board moved after the zero. A
zero holds only while both stay where they were.

<img src="img/angle-steep.png" width="240" alt="ANGLE METER with the axis tilted 70 degrees: --- and TOO STEEP, TURN THE AXIS NEARER LEVEL."> <img src="img/angle-zero-timeout.png" width="240" alt="ANGLE METER after ZERO found no settled reading: NOT SETTLED IN 10 S: NO ZERO">

---

## SETUP

<img src="img/angle-setup.png" width="240" alt="ANGLE SETUP: ALONG AXIS LONG, LENGTH 40, SIGN INVERT, CALIBRATION FLIP with NOT CALIBRATED">

| Row | Values | What it does |
|---|---|---|
| ALONG AXIS | `LONG`, the default, or `SHORT` | Which edge of the board lies along the axis the angle turns about. Changing it drops the zero and MAX |
| LENGTH, MM | `OFF`, the default, or 10 to 500 | The surface's length from the hinge line to the trailing edge, measured by you. `-` and `+` repeat while held |
| SIGN | `NORMAL`, the default, or `INVERT` | Turns the sign of the reading, so you can make up or down positive. The zero and MAX stay; the MAX sides swap |
| CALIBRATION | `FLIP` | Opens FLIP CAL. Under it: `CALIBRATED` or `NOT CALIBRATED` |

A change applies at once. `BACK` returns to the main view and saves the
setup to the board when it differs from what the board has saved. If that
save fails, the main view reads `SETUP NOT SAVED TO FLASH.`, also on later
visits, and the values stay in use until the board restarts. `BACK` on SETUP
tries the save again, with or without a change, and `HOLD TO SAVE` on
SETTINGS stores them too. The setup and the calibration are kept across a
restart. `HOLD DEFAULTS` on SETTINGS keeps them, since SETTINGS does not
show them.

### Which edge along the axis

The board is 73 mm long and 51 mm wide.

| Use | The axis | Place the board | ALONG AXIS |
|---|---|---|---|
| Throw | the hinge line | long edge along the hinge | `LONG` |
| Throw, hinge line shorter than 73 mm | the hinge line | long edge across the hinge | `SHORT` |
| Incidence, decalage | the span | long edge along the chord, on a straight bar | `SHORT` |
| Dihedral | the fuselage's long axis | long edge along the span | `SHORT` |

The angle is the turn about that axis. It stays right while the axis is
tilted, but the reading gets noisier as the tilt grows: the strip turns
amber from 45°, and from 70° the meter shows `---` with `TOO STEEP`. A
rudder's hinge is upright, so lay the model on its side, or raise the tail
until the hinge is at most 45° from the horizontal.

---

## Measuring

Take both readings of a pair with the same end of the board towards the
same end or side of the model. A board turned end for end between them
turns the sign of the second reading.

- **Throw.** The board on the control surface at neutral, as close to the
  hinge as the surface allows. Check the neutral by eye with the board on
  it. ZERO, then move the stick to each end: the angle each side, MAX each
  side, and with LENGTH set the trailing edge's travel in mm.
- **Incidence and decalage.** The meter measures the surface it rests on,
  not the chord: the top of an airfoil is not parallel to its chord. Lay a
  straight bar across the leading and trailing edges, or use a jig that
  holds the chord line, and put the board on the bar. Bar on the
  stabiliser, ZERO; the same bar on the wing near the root: the reading is
  the decalage. With the fuselage's datum as the zero, each reading is that
  surface's incidence.
- **Dihedral.** The board on one wing panel near the root, ZERO, then on the
  other panel: the reading is the angle between the panels.

## Holding the board

- The board rests on its four standoffs, a footprint of about 41 × 60 mm.
  Whether a plug in a socket on the back, the LiPo's among them, stands
  taller than the standoffs is not checked: look that the board rests on
  all four standoffs and not on a plug.
- Run the board from its LiPo during a measurement. A USB cable is stiff
  and heavy against a light surface and pulls the board. Let the cell ride
  on its lead beside the board, or fix it outside the standoffs' footprint;
  a cell taped under the board becomes a soft base.
- A spring clip on the surface's edge, or a rubber band round the surface
  and the board's ends, past the glass, holds it. Whether a band across the
  glass counts as a touch is not known.
- The board's weight loads the surface and turns it through the servo's
  gear play. The board's weight is not measured. Check the play: press the
  surface lightly one way, release, read; then the other way, release,
  read. Two settled readings that differ show the play the reading carries.
- The board must not reach past the hinge onto the fixed surface.

---

## FLIP CAL, the calibration

<img src="img/angle-flip.png" width="240" alt="FLIP CAL with its result: OFFSET X 30  Y -20  Z 40 MG, GAIN Z 1.020, HOLD TO KEEP and AGAIN">

A zero removes every error that shifts all readings alike. The sensor's
offsets also make a large angle wrong; the flip finds them. It needs a flat,
steady surface, level or not.

1. `STEP 1 OF 3`: lay the board on the surface, screen up, and keep it
   still for 2 s.
2. `STEP 2 OF 3`: lift it, turn it round, end for end, and put it back
   screen up on the same spot. Keep it still for 2 s.
3. `STEP 3 OF 3`: turn it screen down for 3 s, then screen up again. While
   it lies screen down every touch is ignored and `BACK` and `AGAIN` are
   drawn as outlines, so the glass on the table taps nothing.

The page then shows the offset of each of the sensor's three axes in mg and
the gain of the axis through the screen. Hold `HOLD TO KEEP` for 1 s to use
and save it: `KEPT AND SAVED.`. A new calibration drops the zero and MAX.
`AGAIN` starts over; `BACK` returns to SETUP without keeping it. A result
with an offset beyond 0.2 g or a gain outside 0.88 to 1.12 is not offered:
`THE POSES GAVE NO CALIBRATION`. How well the flip works on the fitted
sensor, and whether these limits suit it, is not measured.

---

## When it does not read

| Screen | Meaning |
|---|---|
| `NO IMU` and `NO ANSWER FROM THE IMU AT 0X6B.` | The board's sensor does not answer, or stopped answering. The meter asks again every second |
| `NOT A QMI8658: ID 0X..` | Another part answers at the sensor's address. Nothing is written to it |
| `STARTING` and `---` | The sensor is starting, or no reading came for 300 ms |
| `TOO STEEP` and `---` | The axis is tilted 70° or more. Turn it nearer level |
| `MOVING` | The angle has not stayed within 0.5° for 1 s. Keep the board and the model still |

<img src="img/angle-no-imu.png" width="240" alt="ANGLE METER with no sensor answering: NO IMU and NO ANSWER FROM THE IMU AT 0X6B.">
