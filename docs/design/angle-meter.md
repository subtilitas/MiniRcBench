# Angle meter on the board's IMU

Status: designed, not built. The owner chose the angle meter on 2026-11-14
from the feature survey, as item 3 of 6: incidence, decalage, dihedral and
control-surface throw, in degrees and in mm, from the QMI8658 IMU on the
board, at 0x6B on I2C1. The owner answered its questions on 2026-11-15,
see [decisions](#decisions), first among them the entry: an eighth MENU
row, `TOOLS`, opening a chooser. Slices wait on being built.
No line of it is built, and nothing of it has run on a board. The sensor
facts come from QST's datasheets and Waveshare's schematic and demo code;
the ones this project has not seen on the bus are marked as such and listed
under [to confirm](#to-confirm). The real accuracy is unknown until the
bench measures it.

## Terms

IMU is inertial measurement unit; the QMI8658 holds a 3-axis accelerometer
and a 3-axis gyroscope. I2C is Inter-Integrated Circuit, the two-wire bus
of the touch controller; I2C1 is the RP2350's second I2C block, on GP6 (SDA,
serial data) and GP7 (SCL, serial clock). ODR is output data rate, LPF
low-pass filter, LSB least significant bit, rms root mean square, g the
acceleration of gravity, mg a thousandth of it. SA0 is the QMI8658 pin that
sets the low bit of its I2C address. GPIO is general-purpose input/output,
UI user interface, ESC electronic speed controller, LiPo a lithium polymer
cell, RTC real-time clock, STEP the CAD (computer-aided design) exchange
format of `hardware/mechanical/`.

Incidence is the angle of a wing's or stabiliser's chord line to a
reference, about the span. Decalage is the difference between the wing's
and the stabiliser's incidence. Dihedral is the angle of a wing panel to
the horizontal, about the fuselage's long axis. Throw is a control
surface's deflection from its neutral, about its hinge line, in degrees,
and as the trailing edge's travel in mm. The axis of a measurement is the
line the angle turns about: the span for incidence, the fuselage's long axis
for dihedral, the hinge line for a throw.

## Sources

| Source | What it gives |
|---|---|
| QST, QMI8658A datasheet, Rev A, 2022-06-20, document 13-52-25, linked from Waveshare's board wiki as `QMI8658A_Datasheet_Rev_A.pdf` | registers, address selection, ranges, ODR tables, noise, offsets, reset |
| QST, QMI8658C datasheet, Rev 0.6, 2021-01-13, Waveshare's `QMI8658C.pdf` | the same for the C part; several figures differ |
| Waveshare, `RP2350-Touch-LCD-2.8-Schematic.pdf`, the link in `src/board_rp2350_touch_lcd_2_8.h` | U4 QMI8658: SDO/SA0 to GND, CS to 3V3, VDD and VDDIO to 3V3, INT1 to GPIO8, INT2 to GPIO9 |
| Waveshare, `RP2350-Touch-LCD-2.8-Demo-V2.zip` (2026-10-14): `C/libraries/bsp/bsp_qmi8658.*`, `MicroPython/libraries/imu.py` | the address both demos use, 0x6B, and their register writes |
| `src/board_rp2350_touch_lcd_2_8.h` | `QMI8658_I2C_ADDR` 0x6B, `PIN_IMU_INT1` 8, `PIN_IMU_INT2` 9, `BOARD_I2C` i2c1 |
| [current-sense.md](current-sense.md#bus-sharing-in-software), `src/cst328.cpp`, `src/sense.cpp`, `src/ui.cpp` | how I2C1 is shared on core0 today |
| `hardware/mechanical/README.md`, `touch_lcd_2_8.py` | the board's size, its standoffs and rear parts, the IMU's place |
| rcbench at `origin/main`, commit d6bc9fbe (2026-11-14) | `shared/servo/servo_rudder.c`, `shared/bench/imu.c` and their tests |

Nothing of the datasheets is measured by this project. The datasheets give
their figures at VDD = VDDIO = 1.8 V and 25 °C; the board runs the part at
3.3 V (schematic).

## What it is for

- **Incidence and decalage.** The board on the stabiliser's chord, ZERO,
  then the board on the wing's chord: the reading is the wing's angle less
  the stabiliser's, the decalage. The same with the fuselage's datum as A
  gives each surface's incidence.
- **Dihedral.** The board on one panel near the root, ZERO, then on the
  other: the reading is the angle between the panels. With the fuselage
  level in roll and the zero taken on a level reference, each panel's
  angle to the horizontal.

- **Throw.** The board on a control surface at neutral, ZERO, then the stick
  to each end: the deflection each side in degrees, its largest value each
  side, and with the surface's length entered, the trailing edge's travel
  in mm.

Both readings of a pair are taken with the same end of the board towards
the same end or side of the model. A board turned end for end between them
reverses the sign of the second reading: on a wing with equal dihedral each
side, the angle between the panels would read 0. Which way is positive
depends on which end faces forward; `SIGN` in SETUP turns it
([which axis per use](#which-axis-per-use)).

It measures angles from gravity while the board is still. It does not
measure a moving surface's speed, a servo's centring, or anything about a
servo's signal: the servo tester and the KST and Hitec programmers do that
on the signal pins.

## The sensor

### Which part

The board header calls the part a QMI8658C; the schematic calls U4 a
QMI8658; Waveshare's wiki links the QMI8658A datasheet. Both parts answer
0x05 in WHO_AM_I (register 0x00). REVISION_ID (0x01) is given as 0x68 in
both register tables and as 0x79 (C) and 0x7C (A) in their register
descriptions. Which part is fitted is not verified; the driver reads both
registers and the bench records them. Where the two datasheets differ, the
design takes the setting that works on both.

### Address and wiring

| Fact | Value | Source, state |
|---|---|---|
| I2C address | 0x6B | board header, both Waveshare demos; not seen on a bus by this project |
| SDO/SA0 | tied to GND | schematic |
| Address from SA0 | A: SA0 low gives 0x6B, high or open 0x6A. C: open gives 0x6A, pulled up 0x6B | the datasheets disagree; the A datasheet, the schematic and 0x6B agree |
| INT1, INT2 | GPIO8, GPIO9 | schematic netlist and board header; wired, not used by this design |
| Supply | VDD and VDDIO at 3V3, 100 nF each | schematic |
| Host I2C | 100 kHz and 400 kHz modes | both datasheets |
| Place on the board | rear, about (−6.1, −15.1) mm from the display centre in the rear view's frame | `touch_lcd_2_8.py`, placed from Waveshare's photo, good to 0.3 mm |
| Chip axes against the board's edges | unknown | the model places the package, not its pin 1; bench test B3 |

The interrupt lines stay unused: the meter polls once a UI frame. In the A
datasheet INT1 and INT2 are high-impedance until CTRL1 bits 3 and 4 enable
them; the driver leaves both bits at 0 and does not configure GP8 or GP9.

### Registers used

| Register | Address | Use |
|---|---|---|
| WHO_AM_I | 0x00 | 0x05 expected |
| REVISION_ID | 0x01 | read and shown on the bench, not checked |
| CTRL1 | 0x02 | 0x40: address auto-increment on (bit 6), little-endian (bit 5 at 0), both interrupt pins off |
| CTRL2 | 0x03 | 0x08: ±2 g (aFS 000), ODR 31.25 Hz (aODR 1000), no self-test |
| CTRL5 | 0x06 | 0x07: accelerometer LPF on (bit 0), mode 11 (bits 2:1) |
| CTRL7 | 0x08 | 0x01: accelerometer on, gyroscope off; 0x00 on leaving |
| TIMESTAMP_LOW to TEMP_H to AZ_H | 0x30 to 0x3A | one 11-byte burst: 3 bytes sample time stamp, 2 bytes temperature, 6 bytes acceleration |
| RESET | 0x60 | 0xB0 starts a soft reset |
| dQY_L | 0x4D | 0x80 after a successful reset, in the A datasheet only |

Notes on the table:

- **Byte order.** CTRL1 bit 5 defaults to 1, big-endian. The A datasheet
  applies it to every interface; the C datasheet to SPI only. Waveshare's C
  demo writes CTRL1 0x40 and its MicroPython driver 0x60, and both decode
  little-endian. The driver writes 0x40 and reads it back, so the order is
  little-endian on either reading; bench test B2 checks the result.
- **Auto-increment.** Both datasheets name CTRL1 bit 6; the C datasheet's
  I2C section also names bit 7 of the register address. The driver sets
  bit 6 and sends 0x30 plain; bench test B2 shows whether the burst
  increments.
- **Configuration writes.** The A datasheet does not support burst writes
  to the configuration registers, so each is one write of one byte, read
  back after.
- **Gyroscope off.** With the gyroscope off the A part's accelerometer ODR
  comes from the internal oscillator, and aODR 1000 gives 31.25 Hz; with
  the gyroscope on it would give 28.025 Hz (A datasheet, Table 9 and CTRL2).
  The C datasheet lists aODR 1000 as 31.25 Hz in normal mode, with no
  separate rate for the gyroscope on.

### Accelerometer figures

From the datasheets, typical values; none measured here.

| Figure | QMI8658A Rev A | QMI8658C Rev 0.6 | What it means for an angle |
|---|---|---|---|
| Sensitivity at ±2 g | 16,384 LSB/g | 16,384 LSB/g | 1 LSB is 61 µg, 0.0035° near level |
| Noise density | 150 µg/√Hz | 200 µg/√Hz (Table 7), 100 µg/√Hz (Table 10) | see [accuracy](#accuracy) |
| LPF mode 11 at 31.25 Hz | 4.2 Hz (13.37 % of ODR) | 4.4 Hz (14.0 %) | the bandwidth used |
| Initial offset, board level | ±100 mg | ±100 mg | up to 5.7° of tilt before calibration |
| Offset temperature coefficient | ±1 mg/°C | ±0.5 mg/°C | up to 0.057° per °C |
| Initial sensitivity tolerance | ±6 % | ±6 % | see [accuracy](#accuracy) |
| Sensitivity temperature coefficient | ±0.04 %/°C | ±0.04 %/°C | small against the tolerance |
| Cross-axis sensitivity | ±1 % | ±2 % | |
| Non-linearity | ±0.75 % | ±0.75 % | |
| System turn-on from reset | 15 ms | 1.75 s | the start wait |
| Accelerometer turn-on | 3 ms + 3/ODR | 3 ms + 3/ODR | 99 ms at 31.25 Hz |
| Temperature output | 256 LSB/°C | 256 LSB/°C | the drift warning |

## Reading on the bus

### I2C1 today

`src/cst328.cpp` sets I2C1 up at 400 kHz (`TOUCH_I2C_HZ`) and reads the
touch controller, CST328 at 0x1A, once a UI frame from `uiTick()`, every
25 ms, on core0. `senseTick()` reads the optional INA219 at 0x40 directly
after it ([current-sense.md](current-sense.md#the-second-client)). The RTC,
PCF85063 at 0x51, is not used. Nothing on core1 touches I2C1.

### The third client

The IMU is read on core0 too, in `uiTick()` after `senseTick()`, and only
while the meter is open. The three clients run one after the other on one
core, so no lock is needed and the touch read keeps its place at the top of
the frame. Reading on core1 is ruled out for the reasons current-sense.md
gives: the SDK's blocking I2C calls are not safe from two cores, and core1
must never wait on a bus.

One frame's IMU reads, counted as current-sense.md counts them, 9 clocks a
byte plus 2 for start and stop and 1 for a repeated start:

| Transaction | Bytes on the wire | Clocks | 400 kHz |
|---|---|---|---|
| Burst 0x30 to 0x3A: address, register, repeated start, address, 11 data | 14 | 129 | 322.5 µs |
| Time stamp again, 0x30 to 0x32: address, register, repeated start, address, 3 data | 6 | 57 | 142.5 µs |
| A frame | 20 | 186 | 465 µs |

With touch, 210 µs, and the sensor, 240 µs, a frame holds the bus for
915 µs of 25 ms at 400 kHz, 3.66 %. The I2C block's own gaps and the
SDK's call time are not counted. The bus runs at 400 kHz. At 100 kHz the
burst's read call alone, the address and 11 data bytes, about 110 clocks,
would take about 1100 µs, longer than the timeout of 1000 µs a transfer
([errors](#errors)).

The second read guards against a sample that changes during the burst.
The A datasheet's locking mechanism (section 13.1) exists so that a host
can read the data registers "without the risk of mixing the two consecutive
data if the new data comes and updates to the sensor data registers during
the host read": without it, a sample can update in the middle of a read. A
burst whose time stamp differs from the time stamp read after it is
dropped; at 31.25 Hz, a 32 ms period, a burst of 322.5 µs meets an update
about once in 100 bursts. The time stamp is the datasheets' "sample time
stamp"; that it changes once a sample is to be confirmed (bench test B2).
A burst whose time stamp equals the last one taken is the same sample and
is not counted again.

The alternative not taken is SyncSample mode with the locking mechanism
(CTRL7 bit 7, and over I2C the CTRL9 commands that turn the internal AHB
clock gating off, section 13.2.1): it locks the registers during a read, at
the cost of the CTRL9 handshake in the driver and its tests. The time
stamp check needs neither.

### Start and stop

On opening the meter:

1. Read WHO_AM_I. No answer: `NO IMU`, a probe every 1000 ms. Another
   value: `NOT A QMI8658`, no write.
2. Write 0xB0 to RESET; the chip shows `STARTING`. 15 ms later, the A
   part's reset time, read 0x4D: 0x80 means the reset ended, and the
   driver goes on at once. 0x4D has to be read before the sensors are
   enabled, which may overwrite it (A datasheet, 7.4). Any other value, or
   none, and the driver waits until 1750 ms after the reset, the C part's
   system turn-on, and reads WHO_AM_I for 0x05 before it writes. No second
   reset follows sooner than 2000 ms after the first.
3. Write CTRL1, CTRL2, CTRL5, then CTRL7, one byte each, and read each
   back. A mismatch is `NO IMU` and a new start after 1000 ms.
4. Drop the samples of the first 100 ms, the accelerometer's turn-on of
   3 ms + 3/ODR.

The meter is open while its main view, SETUP or FLIP CAL shows: the two
pages are the meter's own, and going to them and back neither stops nor
resets the IMU. BACK on the main view writes CTRL7 0x00, which turns the
accelerometer off. The IMU draws its power-on default current from then on;
neither figure is measured.

### Errors

The driver makes its own SDK calls: `sense_bus.h` has a 16-bit register
write only, a burst the QMI8658A does not take on its configuration
registers (section 16.1), and its wire and record are static in
`sense.cpp`. Each transfer has a timeout of 1000 µs (`SENSE_TIMEOUT_US`),
and its SDK result goes to `senseNoteTouch()`, which maps it to
`sense_err_t` through `errAfter()`, the sensor's own mapping with its check
of the lines ([current-sense.md](current-sense.md#errors)), and notes it to
the shared bus record. Slice 2 renames it `senseNote()`, since it no longer
serves touch alone, and has it return the mapped value to the caller. So a
bus that fails any client is seen by all. The driver keeps its own count:
three failed transactions in a row take the IMU offline, and it is probed
every 1000 ms. While the record shows the bus stuck the driver sends
nothing; after a recovery the touch read, sent every frame, is the trial
transfer that clears the stuck mark.

The recovery needs one change. `senseTick()` returns at once unless
CURRENT SENSE is `INA219` (`src/sense.cpp`), so with the sensor off no
recovery ever runs. Slice 2 moves the check, `sense_bus_recovery_due()`,
then the recovery and `sense_bus_recovered()`, before the mode test, so
the bus is recovered whatever the setting, with a host test at `NONE`.

A frame whose first IMU transfer times out holds the bus 1000 µs longer;
the touch read before it is unchanged.

## The angle

### Static tilt, no fusion

The board is still while it measures, so the accelerometer reads gravity
alone, and the angle follows from the direction of that vector in the
board's frame. The gyroscope would help only while the board moves, and its
bias drifts; the meter does not use it and leaves it off. The part's own
fusion engine is not used either.

### The formula

Let `u` be the board's in-plane axis across the axis of the measurement,
`r` the in-plane axis along it, and `z` the board's normal, each the
calibrated acceleration in g ([calibration](#calibration)). The angle is

```
theta = atan2(a_u, a_z)                     about r
tilt  = atan2(a_r, sqrt(a_u^2 + a_z^2))     of r against the horizontal
```

`theta` is the rotation about `r`. When the board turns about a fixed line
along `r`, the part of gravity in the `u`-`z` plane turns by the same
angle, so a change in `theta` equals the rotation exactly, whatever the
tilt of the line, as long as the tilt stays under 90°. The tilt only
shrinks that part of gravity, to `cos(tilt)` g, so the noise of `theta`
grows by `1 / cos(tilt)`: 1.15 times at 30°, 1.41 at 45°, 2.0 at 60° and
2.9 at 70° (computed). The strip shows the tilt; it turns amber from 45°,
and from 70° the reading is `---` with `TOO STEEP`.

A rudder's hinge is vertical, so the tilt is near 90° and the meter cannot
measure it as it stands: the model goes on its side, or its tail is raised
until the hinge is at most 45° from the horizontal.

`theta` is wrapped to −180° up to 180°. A deflection is `theta` minus the
zero, wrapped again (`servo_rudder_defl()`).

### Which axis per use

The board is 73.06 mm long and 50.54 mm wide (STEP model). SETUP's
`ALONG AXIS` says which edge lies along the axis of the measurement, and so
which in-plane axis is `r`:

| Use | Axis | Board | ALONG AXIS |
|---|---|---|---|
| Throw, the default | hinge line | long edge along the hinge | `LONG` |
| Throw, hinge line shorter than 73 mm | hinge line | long edge across the hinge | `SHORT` |
| Incidence, decalage | span | long edge along the chord, on the bar | `SHORT` |
| Dihedral | fuselage's long axis | long edge along the span | `SHORT` |

`LONG` is the default. For a throw it puts the board's 50.54 mm width
across the surface, so its centre sits about 25 mm from the hinge line and
it overhangs the trailing edge least; the moment under
[holding the board](#holding-the-board) assumes it. `SHORT` serves a throw
only where the hinge line is shorter than the board.

The mapping of the chip's X, Y and Z to the board's long edge, short edge
and normal, with their signs, is a constant of the board header
(`BOARD_IMU_AXES`), set from bench test B3. `SIGN` in SETUP turns the sign
of the reading, so a pilot can make up or down positive.

### What the board rests on

The meter measures the plane it rests on, not the chord. For a throw the
difference cancels: the zero and the reading are taken at the same place
on the same surface. For incidence it does not: the upper surface of an
airfoil is not parallel to its chord, and the wing's and the stabiliser's
airfoils differ, so a reading on two upper surfaces is not their decalage.
For incidence and decalage the board rests on a straight bar laid on the
leading and trailing edges, or on a jig that holds the chord line, and the
same bar or jig serves both surfaces. A flat-bottomed airfoil can take the
board on its lower surface, which means the model upside down, or the board
held against the surface from below; either turns the sign of the reading,
and both readings of a pair are taken the same way.

A wing with dihedral turns its span axis away from the stabiliser's by the
dihedral angle; reading the wing near the root keeps that out of the
decalage.

## Zero, A and B

ZERO takes A. The reading is then the angle from A, which is B − A for a
decalage, the angle between panels for a dihedral and the deflection for a
throw. Without a zero the reading is the angle from level, within the
sensor's uncorrected offset.

The zero is not taken on the tap. A tap moves the board, so ZERO arms,
the hint reads `ZERO WHEN SETTLED: HOLD STILL`, and the zero is taken from
the first settled reading after the finger lifts. Without one within 10 s
the hint reads `NOT SETTLED IN 10 S: NO ZERO` and the old zero stays.

ZERO also clears MAX (`servo_rudder_zero()`). MAX holds the largest
deflection each side since ZERO or RESET MAX (`servo_rudder_track()`), taken
from settled readings only, so a knock while the surface is moved does not
set it.

The zero stays valid only while the model and the axis stay where they
were. The meter cannot tell that the model moved; the wiki says so.

ZERO and MAX last for one visit, by [decision 4](#decisions): SETUP and FLIP CAL
keep them, BACK from the main view drops them, and a new visit starts unzeroed
with no MAX, as the model may have moved in between. ALONG AXIS and SIGN stay as
set, and the settings block keeps them across a restart (decision 4).

## Calibration

### What a zero cancels and what it does not

A zero cancels everything that adds a constant to `theta`: the sensor's
offset at that angle, and the board's mounting, the chip's tilt against the
standoffs. It does not cancel what changes with the angle. Computed from
the datasheet's typical tolerances, for a deflection of 25° from a zero at
level:

| Error source | Size | Error at 5° | Error at 25° |
|---|---|---|---|
| Offset of `a_u` | 100 mg | −0.07° | −0.74° |
| Offset of `a_z` | 100 mg | −0.45° | −2.22° |
| Sensitivity of `u` against `z` | 6 % | +0.30° | +1.30° |
| Temperature since the zero | 1 mg/°C, 10 °C | about 0.6° | about 0.6° |

A throw needs the offsets of `u` and `z` removed; incidence and decalage,
a few degrees apart, need less.

### The flip

The flip removes the offsets of all three axes and gives the sensitivity of
`z`, on any flat, steady surface, level or not:

1. **Screen up.** The board rests on its standoffs. Settled for 2 s: the
   mean of each axis, P1.
2. **End for end.** The board turned 180° about its normal on the same
   spot. On a moved, then settled board for 2 s: P2. Turning the board
   about the surface's normal reverses the in-plane part of gravity, so
   `o_x = (P1x + P2x) / 2` and `o_y = (P1y + P2y) / 2` exactly, whatever
   the surface's slope, which is `(P1 − P2) / 2`. The chip's own tilt
   against the standoffs' plane turns with the board and does not reverse,
   so it lands in `o_x` and `o_y` with the sensor's offset. Subtracted as an
   offset, it is right near the angle the flip was made at and less right
   away from it, where a tilt acts as a rotation; bench test B6 measures
   how much that is.
3. **Screen down.** The board on its glass, then back screen up. Settled
   for 2 s with `a_z` below −0.8 g: P3. `o_z = (P1z + P3z) / 2`, and the
   sensitivity of `z` is `(P1z − P3z) / 2` divided by the cosine of the
   surface's slope. The glass and the standoffs need not be parallel: a
   small angle between them enters with its cosine, near 1.

The screen cannot be read in step 3, so the page moves on by itself: it
sees `a_z` turn negative, settle and turn positive again. Touches are
ignored while `a_z` is negative, so the glass on the table cannot tap a
control.

The sensitivities of `x` and `y` stay at 1; finding them needs each in-plane
axis upright both ways, the board on its edges. One long edge carries the
PWR, RESET and BOOT buttons, at x −22.43 mm in the model; standing the board
on it presses them. Those poses are hand-held against a square block, and
are decision 3's second step: taken only if bench test B6 shows a gain
error over 0.2° (decision 3).

The result is applied as `a' = (a − o) / k` per axis before the formula.
It is kept in the settings block (decision 4).

## Accuracy

Display resolution and accuracy are different things.

- **Resolution.** At ±2 g one LSB is 61 µg, 0.0035° near level.
- **Noise.** 150 µg/√Hz over 4.2 Hz is 307 µg rms a sample, 0.018° (A
  part); 200 µg/√Hz over 4.4 Hz is 420 µg, 0.024° (C part); computed, not
  measured. The samples are averaged into frames ([still and
  settled](#still-and-settled)); how far that lowers the noise depends on
  how the samples correlate at 4.2 Hz in 31.25 Hz, which is not computed
  here.
- **Systematic error.** Offset, sensitivity, cross-axis sensitivity and
  temperature, in the table under [calibration](#calibration), are about
  4 to 120 times the noise of a sample.
- **The holding.** A board that rocks on a curved surface, a bar that does
  not touch both edges, a clip that deflects the surface: none of these is
  in the sensor's figures.

The screen shows 0.1° and 0.1 mm. rcbench's RUDDER shows the IMU's degrees in
hundredths and every mm in tenths (`shared/ui/servo_rudder_panel.c`,
`num_text()`); 0.1° here is the owner's choice (decision 5) and waits on the
accuracy. The real accuracy is unknown until bench test B6 measures it against
known angles; until then the wiki gives no accuracy figure.

## Still and settled

The meter keeps rcbench's IMU model, `imu.c`: its low-pass filter, its
still time per axis and its stale time. The samples are averaged in fours
into frames at about 7.8 Hz, and each frame goes into the model as one
angle: `theta` on the model's X axis, the tilt on Y, 0 on Z. The model's
still time of X counts while `theta` stays within `IMU_STILL_DEG`, 0.5°, of
its first frame, and reads 0 until the filtered angle has come within
`IMU_SETTLE_DEG`, 0.05°, of the frame's. The chip reads `SETTLED` once that
still time is 1000 ms or more (`ANGLE_SETTLED_MS`, proposed), and `MOVING`
otherwise. No frame for `IMU_STALE_MS`, 300 ms, makes the reading stale:
`---`. The model's filter setting, `IMU FILTER` in rcbench, is a constant
here, 3 (proposed): each frame moves the filtered angle a third of the
rest, a time constant of about 2.5 frames, 0.32 s.

The averaging matters to the settle test: `IMU_SETTLE_DEG` is compared with
each frame, and a single sample's noise, 0.018°, is already a third of it.
The four-sample frames and the LPF keep the test from failing on noise
alone; bench test B4 checks it.

How the angles reach the model is decided (decision 2, option b):
`imu_feed()` takes only WitMotion byte frames, so rcbench gets a function
that takes three angles and a time, and this project copies that commit
unchanged.

## Holding the board

### Size, contact and mass

The board is 50.54 × 73.06 × 10.03 mm (STEP model, no caliper). On its
rear, 4 standoffs, M2.5, 4.03 mm tall, at x ±20.5 mm and y −32.9 and
27.1 mm, are taller than every other rear part, the wire sockets at 3.05 to
3.40 mm. So on a flat surface the board rests on its four standoffs, a
footprint of 41 × 60 mm. That is from the model; a plug in a socket may
stand taller than its socket, and none is measured.

The board's mass is not measured, with or without its LiPo; Waveshare gives
none. It loads what it sits on. On a control surface it adds a moment about
the hinge: 60 g, an assumed figure until the board is weighed, at 25 mm
from the hinge line is 14.7 mN·m, 0.15 kg·cm. Against a servo of 1.5 kg·cm,
an example figure for a small servo and not a measured one, that is a tenth
of its torque. Through the servo's gear lash and the surface's own give
that moment moves the surface, and the meter reads the loaded surface. The
wiki asks for the board as close to the hinge as the surface allows, for
the surface's neutral to be checked by eye with the board on it, and for a
check of the lash: the surface pressed lightly one way, then the other, and
released each time; two settled readings that differ show the lash the
reading carries.

### LiPo and cable

The LiPo plugs into the battery socket on the rear. A cell taped under the
board becomes the contact in place of the standoffs, and a soft one at
that. The cell rides on its lead beside the board, or is fixed outside the
standoffs' footprint. A USB cable, stiff and heavy against a light surface,
pulls the board; the wiki asks for the LiPo during a measurement.

### Clip and rubber band

A spring clip on a control surface holds the board on the surface's edge;
its mass adds to the board's. A rubber band round the surface and the board
holds it flat on the standoffs; a band across the glass may press the
touch panel, and whether the CST328 reports it as a touch is not known
(bench test B8). The band goes across the board's ends, past the glass,
as the wiki's drawing shows. The board must not reach past the hinge onto
the fixed surface, where it would meet the wing as the surface deflects;
on a surface narrower than the board it overhangs the trailing edge.

A printed cradle, designed from the STEP model, would give a defined flat
base, a slot for the band and a place for the LiPo; the owner decided
against one in the first build (decision 6).

## Millimetres

The trailing edge's travel is `L × sin(theta)` (`servo_rudder_mm()`), with
`L` the surface's length from the hinge line to the trailing edge, entered
in SETUP: `OFF` or 10 to 500 mm (`SERVO_RUDDER_LEN_MIN_MM`,
`SERVO_RUDDER_LEN_MAX_MM`), in 1 mm steps, with hold-to-repeat as AM32
CONFIG's editor bar repeats. It is what a ruler held upright at the
trailing edge reads: 20° on a 40 mm surface is 13.7 mm. The mm line shows
`SET LENGTH IN SETUP FOR MM` while `L` is `OFF`. The pilot measures `L`;
the meter cannot.

## Screen

Text is the 5 × 7 font: a string of n characters is (6 × n − 1) × scale px
wide and 7 × scale px tall. The degree sign is character 0x60, the
backtick, in the font table of `src/gfx.cpp`, written here as `` ` `` in
the strings and as ° in prose. Colour roles are those of `ui_widgets.h`;
the ones decision 11 of [menu-b.md](menu-b.md) does not name are
proposals.

### Main view

```
y   0 | ANGLE METER                     [  BACK  ]|  header 36 px; BACK 170..233 x 2..33
   40 | LONG EDGE  AXIS TILT 3`         [SETTLED] |  strip 40..55
   66 |                                  +12.4`   |  scale 5, y 66..100, right to x 231
  108 | FROM ZERO                                 |  scale 1, C_DIM
  122 |                                 +8.6 MM   |  scale 3, y 122..142, right to x 231
  148 | L 40 MM, HINGE TO TRAILING EDGE           |  scale 1, C_DIM
  166 | MAX                     -18.2`  +21.7`    |  values scale 2, y 166..179
  186 |                       -12.5 MM  +14.8 MM  |  scale 1, C_DIM, right to x 231
  204 | IMU 31.4`C, +0.6`C SINCE ZERO             |  scale 1, C_DIM
  252 | ZERO WHEN SETTLED: HOLD STILL             |  scale 1, the hint, C_AMBER
  268 | [  ZERO  ] [RESET MAX] [  SETUP  ]        |  x 6, 84, 162; y 268; 72 x 46 each
  320 +-------------------------------------------+
```

| Element | Text, widest | Characters | Width, scale | Place |
|---|---|---|---|---|
| Title | `ANGLE METER` | 11 | 130 px, 2 | x 8 to 137, clear of BACK at x 170 |
| Strip caption | `SHORT EDGE  AXIS TILT 75`` | 25 | 149 px, 1 | x 8 to 156; amber from 45° |
| Strip chip | `TOO STEEP` | 9 | 53 px + 12 px, 1 | `uiChip()`, x 167 to 231, 10 px clear of the caption; also `SETTLED` (`UI_COL_SAFE`), `MOVING` (`C_GRID`), `STARTING`, `NO IMU` (`UI_COL_WARN`) |
| Angle | `-180.0`` | 7 | 205 px, 5 | right-aligned to x 231, from x 27; `UI_COL_LIVE`; `---` when stale or too steep |
| Its caption | `NOT ZEROED: FROM LEVEL` | 22 | 131 px, 1 | x 8; `FROM ZERO` once zeroed |
| Travel | `-500.0 MM` | 9 | 159 px, 3 | right-aligned to x 231, from x 73 |
| Length line | `L 500 MM, HINGE TO TRAILING EDGE` | 32 | 191 px, 1 | x 8 to 198; `SET LENGTH IN SETUP FOR MM`, 26, 155 px, while `L` is `OFF` |
| MAX caption | `MAX` | 3 | 17 px, 1 | x 8, y 170 |
| MAX values | `-180.0`  +180.0`` | 16 | 190 px, 2 | right-aligned to x 231, from x 42; `--` a side with none |
| MAX travel | `-500.0 MM  +500.0 MM` | 20 | 119 px, 1 | right-aligned to x 231 |
| Temperature | `IMU -40.0`C, -99.9`C SINCE ZERO` | 31 | 185 px, 1 | x 8; `UI_COL_WARN` from a change of 2 °C, 0.11° at 1 mg/°C |
| Hint | `ZERO WHEN SETTLED: HOLD STILL` | 29 | 173 px, 1 | x 8, y 252; `NOT SETTLED IN 10 S: NO ZERO`, 28, 167 px |
| ZERO | `ZERO` | 4 | 46 px, 2 | button x 6 to 77 |
| RESET MAX | `RESET MAX` | 9 | 53 px, 1 | button x 84 to 155; scale 2 would be 106 px, past 72 |
| SETUP | `SETUP` | 5 | 58 px, 2 | button x 162 to 233 |

Each region repaints when its text changes. Layout constants get
`static_assert`s for each gap, as the other screens have.

**When the IMU is unavailable** the angle region shows, at scale 1 from
x 8: `NO ANSWER FROM THE IMU AT 0X6B.` (31 characters, 185 px), probed again
every 1000 ms; or `NOT A QMI8658: ID 0X00.` (23, 137 px) with the ID read.
`TOO STEEP` adds `TURN THE AXIS NEARER LEVEL.` (27, 161 px) under the
`---`.

### SETUP

Header `ANGLE SETUP`, 11 characters, 130 px. Rows as SETTINGS draws them
(`src/ui_settings.cpp`): 44 px rows at a 50 px pitch; toggles 94 px wide
from x 138, a toggle's text at most 86 px; a toggle row with a caption has
its label at y + 12 and the caption under it at y + 25, both at scale 1
from x 8; a stepper's value at scale 2 ending at x 132, its connector line
at y + 27 ending at x 132, at most 15 characters (`char sub[16]`).

```
y   0 | ANGLE SETUP                     [  BACK  ]|
   40 | ALONG AXIS             [    LONG     ]    |  label y 52; toggle: LONG, SHORT
      | LONG EDGE ON AXIS                         |  caption y 65, from x 8, C_DIM
   90 | LENGTH, MM          40  [  -  ] [  +  ]   |  OFF, 10 to 500
      |          HINGE TO EDGE                    |  y 117, scale 1, to x 132, C_DIM
  140 | SIGN                   [   NORMAL    ]    |  toggle: NORMAL, INVERT
  190 | CALIBRATION            [    FLIP     ]    |  label y 202; opens FLIP CAL
      | NOT CALIBRATED                            |  caption y 215, from x 8, C_DIM
  320 +-------------------------------------------+
```

| Text | Characters | Width, scale |
|---|---|---|
| `ALONG AXIS`, `CALIBRATION`, `LENGTH, MM` | 10, 11, 10 | 59 px, 65 px, 59 px, 1 |
| `LONG`, `SHORT` | 4, 5 | 46 px, 58 px, 2 |
| `SHORT EDGE ON AXIS` | 18 | 107 px, 1, x 8 to 114, clear of the toggle at x 138 |
| `500`, `OFF` | 3 | 34 px, 2, from x 98 |
| `HINGE TO EDGE` | 13 | 77 px, 1, from x 55 |
| `NORMAL`, `INVERT` | 6 | 70 px, 2 |
| `FLIP` | 4 | 46 px, 2 |
| `NOT CALIBRATED`, `CALIBRATED` | 14, 10 | 83 px, 59 px, 1 |

### FLIP CAL

Header `FLIP CAL`, 8 characters, 94 px. The strip names the step,
`STEP 1 OF 3` (11 characters, 65 px), with the main view's chip. The step's
lines at scale 1 from x 8, y 62 and 74, the live `theta` under them at
scale 3 for the pilot to watch it settle.

| Step | Lines | Widest |
|---|---|---|
| 1 | `LAY THE BOARD ON A FLAT SURFACE,` `SCREEN UP. HOLD STILL.` | 32 characters, 191 px |
| 2 | `TURN IT ROUND, SCREEN UP, SAME SPOT.` | 36, 215 px, to x 222 |
| 3 | `TURN IT SCREEN DOWN, WAIT 3 S,` `THEN SCREEN UP AGAIN.` | 30, 179 px |
| Result | `OFFSET X -999  Y -999  Z -999 MG` `GAIN Z 1.000` | 32, 191 px |

The footer is SETTINGS' layout: `HOLD TO KEEP`, 12 characters at scale 2,
142 px, in a 150 px button from x 6, and `AGAIN`, 58 px, in a 72 px button
from x 162. The 1 s hold keeps the result; it is kept in the
settings block (decision 4). BACK leaves without keeping it.

## Entry

Menu structure B gives MENU seven rows, as built, BEEP THE ESC, AM32 CONFIG,
SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS, scrolling 36 px
([menu-b.md](menu-b.md#menu), decisions 32 and 33). The owner decided on
2026-11-15 (decision 1) that the meter takes an eighth row, `TOOLS`, as in
option (b) of the table below. The resulting MENU, with the rows of the other
notes decided the same day, is the revision line in [menu-b.md](menu-b.md#menu).
`TOOLS` is built with the angle meter's first screen slice (design call,
2026-11-15). The table lists the options the decision chose from.

MENU's scroll range is rows × 42 px − 258 px (`uiListMaxScroll()`). At
scroll 0 row n starts at y 62 + 42 × (n − 1) in a viewport that ends at
y 320: row 6 at y 272 shows whole, row 7 at y 314 shows 6 px, and an eighth
row at y 356 shows nothing. Decision 16 orders the rows by what sits on SIG:
the ESC, a servo, then the bench. The meter drives no pin, so its row goes
with the bench rows, before SD LOG.

| Option | Rows | Where the meter is | Taps and drags from the tester | What moves | For | Against |
|---|---|---|---|---|---|---|
| a. A MENU row, `ANGLE METER` | 8, scroll 78 px | row 6, y 272, whole at scroll 0 | 2 taps | SD LOG to row 7, a drag of 34 px to show it whole; SETTINGS to row 8, a drag of 76 px | found at once; no new widget | revises decision 33's seven rows; SETTINGS wholly below the fold; each later tool adds a row |
| b. A MENU row, `TOOLS`, opening a chooser whose first entry is `ANGLE METER` | 8, scroll 78 px | chooser entry 1, under row 6 | 3 taps | as (a) | one row for later bench tools; the chooser pattern of SERVO PROGRAMMER and RX SIGNALS | one more tap; an eighth row all the same |
| c. `TOOLS` in SD LOG's row 6, its entries SD LOG and ANGLE METER | 7, scroll 36 px | chooser entry, under row 6 | 3 taps | SD LOG a tap deeper; its MENU status, `NO CARD` or `CARD ERROR`, leaves MENU for the chooser's entry | keeps seven rows and SETTINGS where it is | changes decision 32's row list; the card's state no longer on MENU |
| d. A row in SETTINGS, in a section after DIAGNOSTICS | 7, scroll 36 px | SETTINGS' last row; the list grows by a 20 px caption and a 50 px row, 820 px to 890 px, scrolling to 690 px | from the tester by way of MENU: 3 taps and 2 drags, MENU's 34 px and SETTINGS' up to 690 px, of which 2 taps and the 2 drags from MENU; from the tester's readouts: 2 taps and 1 drag of up to 690 px | nothing on MENU | MENU untouched | a tool among settings, found by few; the longest way in; BACK returns to SETTINGS |

Fits: `ANGLE METER` and `TOOLS` at scale 2 are 130 px and 58 px from x 16,
within `menuRowsFit()`'s rule with no status. A TOOLS chooser entry reads
`ANGLE METER` (130 px, scale 2, centred in 228 px) over
`INCIDENCE, DECALAGE, DIHEDRAL, THROW` (36 characters, 215 px, scale 1,
`C_DIM`), in the manner of the PROGRAMMER chooser's entries. The chooser's
title `TOOLS` is 58 px.

Chosen: (b), the recommendation. The USB bridge does not take the
chooser's second entry: it sits in the `ESC CONFIG` chooser of
[usb-bridge.md](usb-bridge.md#decisions). The `TOOLS` chooser holds
`ANGLE METER` alone for now.

## Interlocks

- **No pin claim.** The meter is a screen with `PinUser::None`, as SD LOG
  is. It drives no pin. The ESC stays disarmed at zero throttle, as for
  every tool opened from MENU, and DShot keeps running.
- **I2C1 on core0 only,** after touch and the current sensor, one burst and
  one time stamp read a frame, only while the meter is open. Its failures
  go to the shared bus record; a stuck bus gets the sensor's recovery.
- **The IMU is off outside the meter.** The meter is open while its main
  view, SETUP or FLIP CAL shows. CTRL7 is 0x00 after BACK from the main
  view, so no sample runs and no transfer goes to 0x6B while another
  screen is open.
- **Interrupt lines.** INT1 and INT2 stay disabled; GP8 and GP9 are not
  configured.
- **Flash.** The calibration, LENGTH, ALONG AXIS and SIGN are kept in
  the settings block (decision 4), and a flash save
  parks core1 and erases a sector, so no tap saves. SETUP's rows apply at
  once in RAM and are saved once, when BACK leaves SETUP and a value
  changed; LENGTH's hold-to-repeat writes nothing. FLIP CAL saves on its
  1 s HOLD TO KEEP. Both go through the settings save, and neither saves
  while the ESC is armed. The ESC is disarmed on the meter, as on
  SETTINGS.
- **Touches move the board.** ZERO waits for a settled reading after the
  finger lifts; the flip page ignores touches while the board lies screen
  down.
- **The buttons' edge.** The flip never asks the board to stand on an
  edge; the edge with PWR, RESET and BOOT would press them.

## Porting

| rcbench file, at d6bc9fbe | Lines | Here | How |
|---|---|---|---|
| `shared/servo/servo_rudder.c`, `include/servo_rudder.h` | 111, 80 | `lib/rcbench/shared/servo/` | copied unchanged |
| `test/host/test_servo_rudder.c` | 173, 4 cases | `test/rcbench/` | copied unchanged; needs only `greatest.h` |
| `shared/bench/imu.c`, `include/imu.h` | 352, 204 | `lib/rcbench/shared/bench/` | copied unchanged, after rcbench adds the three-angle function (decision 2) |
| `protocols/witmotion/witmotion.c`, `include/witmotion.h` | 256, 205 | `lib/rcbench/protocols/witmotion/` | only if rcbench's `imu.h` still includes `witmotion.h` after the new function (decision 2, option b) |
| `test/host/test_witmotion.c` | 451, 14 cases | `test/rcbench/` | only with `witmotion.c`; copied unchanged |
| `test/host/test_imu.c`, `test_imu_filter.c`, `wit_frame.h` | 352, 485, 46 | `test/rcbench/` | not copyable unchanged. `test_imu.c` includes `imu_rig.h`, which includes `ble_sim.h`, rcbench's BLE (Bluetooth Low Energy) simulator, `imu_ctl.h` and `witmotion.h`; `test_imu.c` itself includes `servo_rudder_auto.h` and `servo_gap.h`. Two cases of `test_imu_filter.c`, `a_connect_a_reconnect_and_a_bind_seed_the_filter` and `the_limits_1_and_50_hold_and_out_of_range_values_are_clamped`, need `imu_sensor.h`. The cases on `imu.h` alone are adapted, as `test_out_pwm_map.c` was |

rcbench's `servo_rudder_auto.c`, RUDDER AUTO, is not ported: it programs
the end points of a KST, Chaservo or Hitec D series servo until the
measured deflection meets a target, a feature of its own that the owner
has not chosen, and it depends on `servo_angle.h`, `servo_limrun.h`,
`servo_test.h` and `servo_gap.h`, none of which `lib/rcbench/` holds.

`lib/rcbench/README.md` gains the rows and the commit. rcbench records
`imu.c` as built and host-tested against a WitMotion BWT901CL's frames;
nothing of it has run with a QMI8658.

New code in `src/`:

| File | What |
|---|---|
| `qmi8658.h`, `.cpp` | probe, reset, configuration, the burst and the time stamp check, start and stop, the bus record |
| `angle_model.h`, `.cpp` | pure: the axis map, calibration, `theta` and tilt, frames into `imu.c`, zero, MAX, mm, temperature since the zero, the flip's steps |
| `ui_angle.h`, `.cpp` | the main view, SETUP and FLIP CAL |
| `angle.cmake` | the feature's firmware sources |

`board_rp2350_touch_lcd_2_8.h` gains `BOARD_IMU_AXES` and its comment loses
"unused by this firmware"; `uiTick()` gains the IMU's tick after
`senseTick()`.

## Tests

### Host

- **rcbench's tests.** `test_servo_rudder.c` unchanged; the IMU model's by
  decision 2.
- **Driver**, `test/test_qmi8658.cpp`, against a register-level fake of the
  part on the I2C stub of `test/fakes.*`: the probe's three outcomes;
  after the reset, 0x4D at 0x80 at 15 ms going on to the configuration,
  and any other value or no answer waiting to 1750 ms, then reading
  WHO_AM_I; 0x4D read before CTRL7 is written; no second reset sooner than
  2000 ms after the first, after a read-back mismatch too; each
  configuration write single and read back, a mismatch restarting; the
  burst decoded little-endian into signed counts; a repeated time stamp
  not counted twice; a time stamp that changes between burst and re-read
  dropping the burst; NACK, timeout and a held line mapped as `sense.cpp`
  maps them, noted to the bus record, offline after three; CTRL7 0x00 on
  BACK; no transfer to 0x6B while the meter is closed; the frame's bus time at
  400 kHz, as `testFrameTime` measures the sensor's.
- **Model**, `test/test_angle_model.cpp`, on synthetic gravity vectors:
  a rotation of δ about an axis tilted α gives δ within 0.001° for α 0° to
  70°, each `ALONG AXIS` and `SIGN`; `TOO STEEP` from 70° and not at
  69.9°; amber from 45°; the flip recovers offsets set in the fake exactly
  on a surface sloped 0° and 5°, and the gain of `z`; the error the table
  under [calibration](#calibration) computes, for an uncorrected offset
  and gain; the zero and the wrap at ±180°; MAX from settled frames only;
  mm by `servo_rudder_mm()`, `OFF` giving none; the temperature warning at
  2.0 °C and not at 1.9 °C; stale at 300 ms; the frames of four samples;
  the texts' rounding, never `-0.0`.
- **Screen**, through the chosen entry by control id: it opens, BACK turns
  the IMU off; SETUP and FLIP CAL and back keep the IMU running, ZERO and
  MAX, and BACK from the main view drops ZERO and MAX; ZERO armed until
  settled, the 10 s limit; RESET MAX; SETUP's rows; FLIP CAL's steps and
  touches ignored screen down; HOLD TO KEEP at 1000 ms and not at 999 ms;
  every string inside its rectangle; the preview `docs/angle-preview.png`.

Each slice's tests are mutation-checked: the code broken, the test seen to
fail, the code restored.

### Bench

Only the board settles these:

- **B1. Address and identity.** 0x6B answers; WHO_AM_I 0x05; REVISION_ID
  recorded, which tells the part.
- **B2. Byte order and burst.** Flat on a table, `a_z` reads ±1 g within
  0.1 g, and a known tilt reads on the expected axis; the time stamp
  changes once a sample.
- **B3. Axes.** Each edge up in turn: which chip axis reads +1 g, for
  `BOARD_IMU_AXES`.
- **B4. Noise and settle.** 60 s still on a table: the standard deviation
  of `theta` per frame, and how often `SETTLED` drops.
- **B5. Flip.** Repeated five times: the spread of each offset.
- **B6. Accuracy.** Against known angles, wedges whose angles come from
  caliper readings, `atan(h / L)`, at 0°, 5°, 10°, 20° and 30°, each way,
  with and without the flip; repeatability of zero, move and return, ten
  times.
- **B7. Temperature.** From a cold start, 30 min still: the drift of
  `theta` against the IMU temperature.
- **B8. Holding.** The board's mass with and without its LiPo; whether a
  band across the glass reads as a touch; whether the standoffs, not a
  plug, carry the board.
- **B9. Bus.** Touch and the INA219 working while the meter reads.

## Work breakdown

Each slice builds, passes the host suite and merges alone, in this order.

| Slice | Content | Files | Lines, rough |
|---|---|---|---|
| 1. Copies | `servo_rudder.*` and its test; the IMU model by decision 2; `test/angle.mk`; the `test/Makefile` lists, `lib/rcbench/README.md`, README, STATUS, CHANGELOG | 3 to 8 copied, 1 new, 5 edited | 360 to 1,830 copied, with tests; 80 new or changed |
| 2. Driver | `qmi8658.*`, `angle.cmake`, `BOARD_IMU_AXES`, the IMU tick in `uiTick()`, `senseNote()` in place of `senseNoteTouch()`, the bus recovery ahead of `senseTick()`'s mode test with its test at `NONE`, the register fake, `test_qmi8658.cpp`, the `-I` lists of `ci.yml` | 3 new in `src/`, 2 new in `test/`, about 8 edited | 300 in `src/`; 550 in `test/` |
| 3. Model | `angle_model.*`, `test_angle_model.cpp` | 2 new in `src/`, 1 new in `test/` | 300 in `src/`; 600 in `test/` |
| 4. Screen | `ui_angle.*`, the entry by decision 1, the settings fields by decision 4 with their carry-over, the screen tests, `test_ids.cpp`, the preview, `tools/make_previews.py` | 2 new in `src/`, about 10 edited | 700 in `src/`; 600 in `test/` |
| 5. Docs | the README section, the wiki pages in English and German and `tools/check_wiki.py`, CHANGELOG, STATUS, this note's status | 2 new, 5 edited | 350 of documentation |
| 6. Bring-up | bench tests B1 to B9 | none | none |

Slice 3 depends on slice 1; slice 2 on nothing but the tree; slice 4 on
both. Slice 5 can go with slice 4, as other features' screen slices carry
their pages.

## Decisions

Decisions of the owner, 2026-11-15, answering the open questions of
2026-11-14. Each is the note's recommendation.

1. **The entry.** Option (b): an eighth MENU row, `TOOLS`, as row 6 before
   SD LOG, opening a chooser whose first entry is `ANGLE METER`. 3 taps
   from the tester. SETTINGS moves to row 8 behind a drag of 76 px. The
   resulting MENU is recorded in [menu-b.md](menu-b.md#menu). This is the
   recommendation.
2. **rcbench's IMU model.** Option (b): a function that takes three
   angles and a time is added to `imu.c` in rcbench and tested there, and
   that commit is copied unchanged. This is the recommendation.
3. **Calibration.** Option (b): the flip of three poses, then option (c)
   if bench test B6 shows a gain error over 0.2°. This is the
   recommendation.
4. **What a restart keeps.** Option (d): the calibration, LENGTH, ALONG
   AXIS and SIGN in the settings block, `SETTINGS_VERSION` 5 to 6 with the
   carry-over of current-sense.md decision 14. ZERO and MAX for the visit
   only. This is the recommendation.
5. **Display resolution.** Option (a): 0.1° and 0.1 mm, until bench test
   B6 gives the accuracy. This is the recommendation.
6. **A holder.** Option (b): none in the first build; the wiki describes
   the standoffs, a straight bar for incidence, a clip and a band past the
   glass. This is the recommendation.
7. **Recording.** Option (a): the screen only. This is the recommendation.

## Open questions

None open. Questions 1 to 7 are answered by decisions 1 to 7.

## To confirm

Not verified by this project:

- which part is fitted, QMI8658A or QMI8658C, and its REVISION_ID;
- that 0x6B answers on the board's bus; the C datasheet's address rule
  would give 0x6A with SA0 at GND;
- the byte order with CTRL1 0x40 over I2C, and the burst's auto-increment
  by CTRL1 bit 6;
- the system turn-on, 15 ms (A) or 1.75 s (C), and 0x4D reading 0x80 after
  a reset (A only);
- the soft reset's value: 0xB0 in the A datasheet's RESET register
  description (5.9, Table 27), 0x0B in its chip reset process (7.4);
  Waveshare's C demo writes 0xB0, and so does this design;
- that the time stamp changes once a sample, and whether the output
  registers can change during a burst;
- the noise, offset, gain and temperature figures, all typical values at
  1.8 V, none measured at 3.3 V;
- the chip's axes and signs against the board's edges;
- the board's mass, with and without a LiPo;
- that the standoffs, not a plug, carry the board on a flat surface;
- whether a rubber band across the glass reads as a touch;
- the warm-up drift after power-on;
- the real accuracy of every reading;
- Waveshare's wiki lists a second version of the board (2026-10-14) with a
  CST3530 touch controller; which version this firmware's board is, and
  whether the second changes the IMU, is not stated.
