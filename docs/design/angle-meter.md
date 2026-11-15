# Angle meter on the board's IMU

Status: designed; slices 1 to 5 built, host tests only. Slice 1, the copies from
rcbench ([slice 1 as built](#slice-1-as-built)). Slice 2, the driver ([slice 2
as built](#slice-2-as-built)). Slice 3, the model ([slice 3 as
built](#slice-3-as-built)): it calls the copies, and it adapts the cases of
rcbench's `test_imu.c` and `test_imu_filter.c` that need `imu.h` alone. Slice 4,
the screen ([slice 4 as built](#slice-4-as-built)): ANGLE METER behind MENU's
sixth row, `TOOLS`, opens the driver and shows the model's reading, and the
settings block, version 6, keeps the setup and the calibration. Slice 5, the
documentation, with it ([slice 5 as built](#slice-5-as-built)). Slice 6, the
bring-up, bench tests B1 to B9, waits. The owner chose the angle meter on
2026-11-14 from the feature survey, as item 3 of 6: incidence, decalage,
dihedral and control-surface throw, in degrees and in mm, from the QMI8658 IMU
on the board, at 0x6B on I2C1. The owner answered its questions on 2026-11-15,
see [decisions](#decisions), first among them the entry: an eighth MENU row,
`TOOLS`, opening a chooser. Nothing of the meter has run on a board. The sensor
facts come from QST's datasheets and Waveshare's schematic and demo code; the
ones this project has not seen on the bus are marked as such and listed under
[to confirm](#to-confirm). The real accuracy is unknown until the bench measures
it.

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
   3 ms + 3/ODR, 99 ms: read nothing for 100 ms, then take the time stamp
   of the first whole burst as the baseline, neither counted nor taken. The
   first sample taken is the next time stamp, latched after the 100 ms.

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
and its SDK result goes to `senseNote()`, which maps it to `sense_err_t`
through `errAfter()`, the sensor's own mapping with its check of the lines
([current-sense.md](current-sense.md#errors)), notes it to the shared bus
record and returns the mapped value to the caller. Touch and the IMU
driver both use it, so a bus that fails any client is seen by all. The
driver keeps its own count: three failed transactions in a row take the
IMU offline, and it is probed every 1000 ms. While the record shows the bus
stuck the driver sends nothing; after a recovery the touch read, sent every
frame, is the trial transfer that clears the stuck mark.

`senseTick()` (`src/sense.cpp`) runs the recovery ahead of its mode test:
`sense_bus_recovery_due()`, then the recovery and `sense_bus_recovered()`,
and only then the return unless CURRENT SENSE is `INA219`. So the bus is
recovered whatever the setting; a host test runs it at `NONE`.

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

ZERO and MAX last for one visit, by [decision 4](#decisions): SETUP and FLIP
CAL keep them unless ALONG AXIS or the calibration changes, BACK from the main
view drops them, and a new visit starts unzeroed with no MAX, as the model may
have moved in between. ALONG AXIS and SIGN stay as
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
2026-11-15), and is: [slice 4 as built](#slice-4-as-built). The table lists
the options the decision chose from.

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
`ANGLE METER`.

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
  the IMU off; SETUP and FLIP CAL and back keep the IMU running, and ZERO
  and MAX unless ALONG AXIS or the calibration changes, and BACK from the
  main view drops ZERO and MAX; ZERO armed until
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
  changes once a sample. Record when the first sample arrives after CTRL7:
  the host model latches one every 32 ms from the CTRL7 write, which is an
  assumption, not a measurement.
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

### Slice 1 as built

rcbench's files at commit `e98345aa`, the merge of its three-angle function
(decision 2), copied unchanged:

| Here | rcbench | Lines |
|---|---|---|
| `lib/rcbench/shared/servo/servo_rudder.c`, `include/servo_rudder.h` | `shared/servo/` | 111, 80 |
| `lib/rcbench/shared/bench/imu.c`, `include/imu.h` | `shared/bench/` | 377, 225 |
| `lib/rcbench/protocols/witmotion/witmotion.c`, `include/witmotion.h` | `protocols/witmotion/` | 256, 205 |
| `test/rcbench/test_servo_rudder.c` | `test/host/` | 173, 4 cases |
| `test/rcbench/test_imu_angles.c`, `wit_frame.h` | `test/host/` | 361, 7 cases; 46 |
| `test/rcbench/test_witmotion.c` | `test/host/` | 451, 14 cases |

`test/angle.mk` lists them for the host suite, which links the three C
files and builds each test as a C11 program of its own, run by
`make -C test run`; `test/Makefile` includes it. `lib/rcbench/README.md`
gains the table and the commit. Choices the note left open:

- **`witmotion.*` is copied.** At `e98345aa`, `imu.h` still includes
  `witmotion.h` (the porting table's condition), and `imu.c` decodes
  WitMotion frames through it.
- **The IMU model's test is `test_imu_angles.c`,** the test rcbench added
  with `imu_feed_angles()`, which needs `imu.h`, `wit_frame.h` and
  `greatest.h` only and so copies unchanged. The cases of `test_imu.c` and
  `test_imu_filter.c` that need `imu.h` alone, which the porting table says
  are adapted, are slice 3's ([slice 3 as built](#slice-3-as-built)). The
  path the meter uses is the one `test_imu_angles.c` tests.
- **The firmware builds them with their caller.** Nothing in slice 1 calls
  the files; slice 3's model does, and `src/angle.cmake` lists them with
  it.

Mutation check of the copies against their tests, each mutant built and run:
12 mutants, 7 in `servo_rudder.c` (the sine of the travel, the shortest
length, the wrap of a deflection, the side of a deflection, the largest
value, MAX cleared by a zero, and one that left a function unused and failed
the build), 4 in `imu_feed_angles()` (a NaN dropped, the wrap to ±180°, the
tick after the angle, the angle itself) and 1 in `witmotion.c` (the range
of the acceleration): every one fails a test.

### Slice 2 as built

Slice 2 depends on nothing but the tree.

`src/qmi8658.*`, the driver: the probe, the reset and its two waits, the
configuration, the burst and its time stamp check, start and stop, its
failure count. `src/angle.cmake` lists it for the firmware and
`test/angle.mk` for the host suite. `src/board_rp2350_touch_lcd_2_8.h`
gains `BOARD_IMU_AXES` and loses "unused by this firmware". `uiTick()`
ticks the driver after `senseTick()`. In `src/sense.*`, `senseNote()`
replaces `senseNoteTouch()`, returns the mapped value and serves the touch
driver and the IMU driver alike; `senseTick()` runs the recovery before its
mode test. The register model of the part is in `test/fake_i2c.*`, the bus
the INA219 and CST328 models already sit on, and its tests in
`test/test_qmi8658.cpp`; the recovery at `NONE` is tested in
`test/test_sense.cpp`. Choices the note left open:

- **Nothing opens the driver yet.** `qmi8658Start()` and `qmi8658Stop()` are
  the meter's calls; slice 4 makes them from the screen. Until then the tick
  in `uiTick()` returns at once and nothing is sent to 0x6B.
- **The probe of another part** reads WHO_AM_I again every 1000 ms
  (`QMI8658_RETRY_MS`), as the INA219's probe reads another part at 0x40
  every second. Nothing is written to it.
- **REVISION_ID** is a read of its own after WHO_AM_I, one byte each:
  address auto-increment is off until CTRL1 is written.
- **A new start** waits 1000 ms after the step that failed, or until 2000 ms
  after the last reset when that is later (`QMI8658_RESET_GAP_MS`). The
  reset counts from its attempt, since a write that failed may have reached
  the part. The rule holds across BACK and a new opening too. The time since
  the last reset is an unsigned difference, so a meter opened 30 days after
  it probes at once.
- **The waits run on the UI frame.** dQY_L is read on the first frame at
  least 15 ms after the reset, 25 ms later at the UI's rate, and the
  configuration follows in the same frame on 0x80. WHO_AM_I after the C
  part's turn-on is read on the first frame at least 1750 ms after the reset,
  and the configuration follows in the same frame.
- **A failure while starting** is `NO IMU` (`ImuState::Absent`) and a new
  start, whether a transfer failed or a value did not read back. The count
  of three applies to the part once it is configured.
- **The warm-up reads nothing.** For 100 ms after CTRL7 no burst is read, in
  place of reading samples and dropping them. The first whole burst after it
  is the baseline: its sample may have been latched inside the turn-on, up to
  32 ms before the read, so its time stamp is kept and the sample neither
  counted nor taken. The first sample taken has a new time stamp and so was
  latched after the 100 ms; in the register model, 128 ms after CTRL7, taken
  on the frame at 150 ms. The state reads `Online` from the first frame whose
  burst and time stamp read both answer, the baseline's included.
- **One failure a frame.** A failed transaction ends the IMU's frame, so a
  frame counts at most one. The count goes back to 0 only when the burst and
  the time stamp read both answer, so a time stamp read that fails every
  frame takes the part offline as a failed burst does.
- **The stuck bus.** While the record shows it stuck the driver sends
  nothing, not even as the trial after a recovery: the touch read is the
  trial. A stuck bus does not count as a failure of the IMU. The driver does
  not test the lines before a transfer, as the INA219's wire does; its
  results are mapped after each transfer by `errAfter()`, as touch's are.
- **The stop** writes CTRL7 0x00 when CTRL7 was written since the last reset,
  also after the write or its read-back failed, since a write that failed may
  have reached the part, and not when the reset was the last write:
  a reset leaves CTRL7 at its power-on value, 00h in this design's model of
  the part. Nothing is sent while the bus is stuck; the next opening's reset
  then turns the accelerometer off. A second `qmi8658Start()` while open
  changes nothing.
- **`BOARD_IMU_AXES`** is a placeholder, `1, 2, 3`: chip X along the long
  edge, Y along the short edge, Z along the normal, no sign turned. The
  chip's orientation is unknown until bench test B3. A `static_assert` checks
  that each axis appears once (`qmi8658AxesValid()`). Slice 3 uses it.
- **The register model** answers as the note describes the part: power-on
  CTRL1 20h (big-endian, auto-increment off), auto-increment and the data
  byte order by CTRL1, one byte taken from a write of several, the reset by
  B0h, dQY_L 80h after it and 00h once CTRL7 turns a sensor on, no answer for
  a boot time after a reset (15 ms by default, 1750 ms for the C part's
  test), a sample every 32 ms with a time stamp one higher whatever CTRL2
  says, and on request a sample latched after the third byte of a burst or a
  failed transfer that reaches the part before it fails. It is the note's
  reading of the datasheets, not a measured part.
- **No change to the `-I` lists of `ci.yml`.** The driver includes `sense.h`,
  whose rcbench include directory the lists already hold, and nothing of
  rcbench of its own.

Bus time, measured by `testFrameTime` in `test/test_qmi8658.cpp` at 400 kHz:
a reading frame is 129 and 57 clocks, 465 µs, as computed under
[the third client](#the-third-client). A healthy probe frame is 107 clocks,
267.5 µs; the configuration's frame, dQY_L and the four registers written and
read back, 311 clocks, 777.5 µs. The worst frame in any failure mode is that
frame with CTRL7's read-back timing out: 292 clocks and 1000 µs, 1730 µs.
The longest reading frame that fails is the time stamp's data read timing
out, 1373 µs.

Mutation check: 96 mutants of `src/qmi8658.cpp`, `senseNote()`,
`senseTick()`'s order and the tick in `uiTick()`, each built and run against
the host suite. 94 fail it, among them CTRL7's flag set only after its write
answered, each change to the baseline, the reset gap as a signed difference,
and the reset gap without its "a reset was written" flag, which the first
IMU test catches by opening the meter at millis() 0 before any reset. Two
survive. `<=` in place of `<` in the reset gap is equivalent: at exactly
2000 ms both give the same time. The sample count's skip of 0 when it wraps
comes after 2^32 samples, 4.4 years at 31.25 Hz.

Not verified on the board: everything under [to confirm](#to-confirm) that
concerns the part, the address, the byte order, the burst's auto-increment,
the reset's value and time, dQY_L, the time stamp, and bench tests B1, B2
and B9.

### Slice 3 as built

Slice 3 depends on slice 1.

`src/angle_model.*`, the model, pure: the driver's samples into frames of four,
the calibration, the axis map, `theta` and the tilt, the frames into rcbench's
`imu.c` by `imu_feed_angles()`, ZERO, MAX and the travel through
`servo_rudder.c`, the temperature since the zero, and the flip's steps and
arithmetic. `src/angle.cmake` lists it for the firmware with rcbench's
`servo_rudder.c`, `imu.c` and `witmotion.c` and their include directories;
`test/angle.mk` lists it for the host suite with `test/test_angle_model.cpp`.
The two lists of `-I` in CI's syntax checks of `src/` gain
`lib/rcbench/protocols/witmotion/include`, which `imu.h` needs; they held the
other two directories already. Nothing calls the model: the screen, slice 4,
gives it the driver's samples and shows its reading. Choices the note left
open:

- **The calibration is in the chip's axes.** Offsets and gains are kept per
  chip axis, X, Y and Z, and applied before the axis map, so a calibration
  kept stays right when bench test B3 sets `BOARD_IMU_AXES`. The flip takes
  the chip axis the map puts on the normal for P3 and for the gain.
- **A pose of the flip** is the mean of the frames of a still stretch of
  2000 ms (`ANGLE_FLIP_HOLD_MS`): each frame within `IMU_STILL_DEG`, 0.5°, of
  the stretch's first, less than `IMU_STALE_MS`, 300 ms, after the last, and
  with the normal above 0.8 g, or below −0.8 g for P3. The frames are the
  uncalibrated ones. Step 2 takes its pose only after a move since P1: a
  stretch broken, or a frame out of the pose. A board slid round flat on a
  level surface shows no move, as gravity does not change in the board's
  axes; the pilot lifts it. Whether a turn by hand breaks the stretch is not
  verified. After P3 the first frame above 0.8 g solves the poses.
- **The flip's limits**, proposals until bench test B5: offsets within
  ±0.2 g (`ANGLE_CAL_OFFSET_MAX_G`) and gains from 0.88 to 1.12, twice the
  datasheets' ±100 mg and ±6 %. A result outside them is `Failed`, as is one
  with a NaN or a slope of 90° or more, which give no finite gain. A
  calibration outside them is not applied.
- **ZERO's settled reading** is the first frame at least 1000 ms after the
  finger lifts (`angleZeroArm()`) that is settled, so the board has been still
  for the whole second after the lift. A frame 10000 ms after the lift times
  ZERO out before it can take the zero; the tick times it out with no frames.
- **SETTLED** is the still time of `theta` at 1000 ms or more, with the axis
  not too steep. MAX takes every settled frame, before a ZERO too, from
  level, as RESET MAX without a zero allows. A too steep reading takes no zero
  and no MAX, as it shows `---`. A stale reading is neither too steep nor
  amber.
- **SIGN is applied to the reading.** The zero and MAX stay in `theta`'s own
  sign, and SIGN turns the reading and swaps the MAX sides, so SETUP's SIGN
  keeps both. −180° stays −180° turned, as `imu_deg()` keeps it.
- **A new ALONG AXIS or calibration** drops the zero and MAX, disarms ZERO and
  seeds the filter: `theta` changes its axis or its offset, and a zero taken
  before would read wrong. A calibration applied as it was, the same values
  or none again, changes nothing. Going to SETUP or FLIP CAL and back without
  either change keeps them, as decision 4 says.
- **The frames.** Each sequence number is taken once, and seq 0 not at all. A
  sample 300 ms or more after the last starts the frame again. A frame is the
  mean of four samples at the fourth's time. One of less than 0.5 g
  (`ANGLE_MIN_G`) has no direction to take and is dropped; the reading ages
  as with no frame.
- **The travel** shows for LENGTH 10 to 500 mm; `OFF`, 0, and any other value
  give none.
- **The texts** in tenths (`angleTenthsText()`) round the float product
  `v × 10`, itself rounded to float, half away from zero (`lroundf()`): 0.35f
  shows 0.4. They never show `-0.0`, and show `---` for a value that is not
  finite or beyond ±99999.
- **`imu.c` in the firmware.** arm-none-eabi makes `imu_axis_t`, 0 to 2, one
  unsigned byte, so `imu.c`'s `(int)axis < 0` in `imu_deg()` and
  `imu_still_ms()` is always false and `-Wtype-limits` warns; the strict
  build stops on it. `src/angle.cmake` turns that warning off for `imu.c`
  alone, and the copy stays unchanged. Whether rcbench's own RP2350 build
  meets it is not checked.
- **`BOARD_IMU_AXES`** stays the placeholder `1, 2, 3` until bench test B3.

`theta`'s change equals the rotation about a line tilted 0° to 70° within
0.001° for each ALONG AXIS, computed in float from the vectors. Through the
16-bit counts of the samples it is within 0.005° divided by the cosine of the
tilt; the worst seen in the tests is 0.0017° so divided. The table under
[calibration](#calibration) is reproduced within 0.006°, and a calibration of
the same errors brings each to within 0.003° at 25°.

The cases of rcbench's tests that need `imu.h` alone, at `e98345aa`, are
adapted into `test/rcbench/` as `test_out_pwm_map.c` was, each file starting
with the line `Adapted from rcbench`:

| File | Cases | Left out, and why |
|---|---|---|
| `test_imu.c` | 9 of 9 | The line of `imu_stale_equals_the_gap_allowance` and the `_Static_assert` that compare `IMU_STALE_MS` with `servo_gap.h`, not ported, are left out; the case keeps its check of 300 ms. The rig's four frame builders of `imu_rig.h` are copied into the file; the rest of the rig drives the BLE simulator. `a_still_yaw_counts_while_pitch_creeps` takes `SERVO_RAUTO_STILL_MS` of `servo_rudder_auto.h`, defined in the file at its value at that commit, 500 ms |
| `test_imu_filter.c` | 10 of 11 | `a_connect_a_reconnect_and_a_bind_seed_the_filter` drives the sensor link `imu_sensor.h`, not ported; so do the last lines of `the_limits_1_and_50_hold_and_out_of_range_values_are_clamped`, left out, while the case's other lines stay |

Mutation check: 161 mutants of `src/angle_model.cpp`, each built and run
against `test_angle_model.cpp`. 158 fail it. Three survive, each equivalent
to the code: the axis map taken as `1, 2, 3` in place of `BOARD_IMU_AXES`,
which is `1, 2, 3` until bench test B3; `servo_rudder_init()` left out after
the `memset()` that already clears its state; and the zero's temperature
taken from the model's copy of the frame's temperature, set a line before.
8 mutants of `imu.c`, in the filter's clamp, the rate's silent window, the
battery, the content count, the still time's ceiling, the bad count, the
unzero and the version, each fail an adapted test; 7 of them pass
`test_imu_angles.c`.

Not verified on the board: the angle itself, the chip's axes (B3), the
settle on real noise (B4), the flip's spread and its limits (B5), the
accuracy against known angles, and with it whether decision 3's edge poses
are needed (B6), and the drift with temperature (B7).

### Slice 4 as built

Slice 4 depends on slices 2 and 3.

`src/ui_angle.*` is ANGLE METER, one screen of three views, the main view,
SETUP and FLIP CAL (`UiScreenId::Angle`). `src/ui_tools.*` is TOOLS
(`UiScreenId::Tools`), MENU's sixth row by decision 1, before SD LOG, built
as ESC CONFIG's chooser is: one 228 × 46 px button at y 62, its line under
it, the entries a table of `src/ui_registry.cpp` (`UI_TOOLS_ENTRIES`) with a
build-time fit check. `src/angle.cmake` and `test/angle.mk` list both. MENU
has the eight rows of [menu-b.md](menu-b.md#menu) and scrolls 78 px. The
settings block is version 6 by decision 4 (`src/settings.*`). Host tests in
`test/test_angle_ui.cpp`, `test/test_settings.cpp`, `test/test_ids.cpp`,
`test/test_menu.cpp` and `test/test_ui.cpp`; the preview
`docs/angle-preview.png`. Choices the note left open:

- **TOOLS' strip** reads `DSHOT RUNS ON SIG GP0`, 21 characters: neither the
  chooser nor the meter claims a pin. TOOLS opens only while the claim is
  Free (`freePins`), as the other choosers, and so does the meter, since its
  save writes flash, which `settingsSave()` refuses during a claim; MENU
  draws the row disabled meanwhile.
- **Opening** takes ALONG AXIS, SIGN, LENGTH and the calibration from the live
  settings, begins a visit (`angleOpen()`) and calls `qmi8658Start()`. Every
  frame the meter gives `qmi8658Latest()` to `angleSample()`, which takes
  each sample once, then `angleTick()`. The driver ticks in `uiTick()` before
  the tool, so a sample read on a frame reaches the model on that frame.
  `BACK` on the main view calls `qmi8658Stop()`, CTRL7 00h, and returns to
  MENU; SETUP and FLIP CAL leave the driver running.
- **The chip.** `STARTING` while the driver starts, and while it is online
  with no reading in the model; `NO IMU` for no part, another part or a part
  offline; then `TOO STEEP`, `SETTLED` or `MOVING`. `STARTING`, `NO IMU` and
  `TOO STEEP` are on `UI_COL_WARN`, `SETTLED` on `UI_COL_SAFE`, `MOVING` on
  `C_GRID`.
- **The angle** shows while the model has a reading and the axis is not too
  steep, `MOVING` or not, so the pilot sees the surface move; `---` in
  `C_DIM` otherwise, as the note's table says. `--` is MAX's text for a side
  with none. With no part, another part or a part offline, the angle's band
  holds the note's line at scale 1, at y 80.
- **The strip's tilt** is the filtered tilt's magnitude cut to whole degrees,
  so it reads `45°` when it turns amber and `70°` with `TOO STEEP`; `--`
  without a reading.
- **The travel** reads `--- MM` while LENGTH is set and the angle does not
  show, and nothing with LENGTH `OFF`; MAX's travel line shows only with
  LENGTH set, `--` for a side with no MAX.
- **The temperature line** shows from the first frame, `IMU 31.4°C`, and
  after a zero `IMU 31.4°C, +0.6°C SINCE ZERO`.
- **ZERO** acts while the driver is online, and is drawn disabled otherwise;
  RESET MAX and SETUP always act. The hint line reads ZERO's state, armed or
  timed out, and else a failed SETUP save. What a tap changes shows on the
  tap's frame: the model is read again after the taps.
- **SETUP** draws its rows with SETTINGS' measures and is not a list: four
  rows from y 40 at a 50 px pitch end at y 233, with no footer. Its controls
  are fixed ids (`AngleCtl`). The toggles have no filled state; `-` and `+`
  step LENGTH `OFF`, 10 to 500, `OFF` below 10, held at 500, with
  `repeatFires()`. A row writes the model and the live settings at once.
  `BACK` saves while the four fields differ from flash
  (`settingsAngleDirty()`), through `settingsSaveAngle()`: the block written
  is the settings as last loaded or saved with the four fields from the live
  ones, so a SETTINGS change not saved stays unsaved. So a save that failed
  is tried again on the next BACK, edit or not, on this visit or a later
  one. The main view's hint reads `SETUP NOT SAVED TO FLASH.` while the
  fields differ from flash; the values stay live, and SETTINGS' HOLD TO
  SAVE stores them too.
- **FLIP CAL** opens from CALIBRATION's `FLIP` with `angleFlipBegin()`;
  `BACK` calls `angleFlipEnd()` and returns to SETUP. Its strip reads
  `STEP 1 OF 3` to `STEP 3 OF 3`, then `RESULT` or `NO RESULT`; with no
  result the lines read `THE POSES GAVE NO CALIBRATION` and
  `WITHIN THE LIMITS. TAP AGAIN.` The result's offsets are whole mg, its gain
  the normal's chip axis to three decimals, `GAIN Z 1.020`. HOLD TO KEEP acts
  only with a result not yet kept; its 1 s hold applies it (`angleSetCal()`),
  writes it to the live settings and saves it, and the line at y 252 reads
  `KEPT AND SAVED.` or `APPLIED, NOT SAVED TO FLASH.`. A calibration applied
  and not saved is a changed field to SETUP, whose BACK tries the save again.
  AGAIN begins the flip again. While the model's last frame has the normal
  below 0 the page takes no touch, and BACK, AGAIN and HOLD TO KEEP are
  drawn and reported disabled.
- **The only exit** is BACK on the main view, which calls `qmi8658Stop()`;
  SETUP's and FLIP CAL's BACK stay in the meter.
- **Repaints.** Each view is a set of regions keyed on what they draw; a
  frame repaints a region whose key changed, and a new view repaints all.
- **The settings block, version 6,** has 44 bytes: version 5's 14 and, from
  byte 14, ALONG AXIS (0 `LONG`, 1 `SHORT`), SIGN (0, 1), LENGTH in mm
  (16 bits, 0 for `OFF`), the calibration's flag, a spare byte, then the
  three offsets and three gains as 32-bit floats from byte 20, aligned. The
  CRC is at byte 52. Validation sets ALONG AXIS and SIGN above 1 to 0, a
  LENGTH outside 10 to 500 to `OFF`, the spare byte to 0, and drops a
  calibration `angleCalValid()` refuses, or a flag other than 0 or 1, to
  none: the flag 0, offsets 0, gains 1. A version 5 block, recovered from
  `src/settings.h` at aafc31b, is carried over as versions 2 to 4 are,
  CURRENT SENSE from its byte 21; the angle fields take their defaults.
  SETTINGS does not show them, and its HOLD DEFAULTS keeps them: a reset there
  followed by a save of the meter would otherwise store a calibration of none.
- **No accuracy on the screen.** The texts state no accuracy; the wiki says
  that none is measured.

Mutation check: 99 mutants of `src/ui_angle.cpp`, `src/ui_tools.cpp`, the
angle fields' code in `src/settings.cpp` and the TOOLS row of
`src/ui_registry.cpp`, each built and run against the host suite. 88 failed
it at first. Of the 11 left, 7 fail it since tests were added that load the
setup and the calibration on a later visit, change LENGTH alone, tilt the
axis the other way, name the short edge on the strip and check the cell left
of a right-aligned text. Two were lines that changed nothing, a repaint flag
in the region test that a new view's cleared keys already cover and a touch
dropped on a view's first frame, which draws no press after a lift; both are
removed. Two survive. The armed check before the meter's save is unreachable:
MENU disarms before the meter opens, and the check stays as SETTINGS keeps
its own. `--- MM` written for the travel without a reading is equivalent:
`angleTenthsText()` gives `---` for the NaN the model then reports. For
HOLD DEFAULTS keeping the four fields and SETUP's BACK saving while they
differ from flash, 19 mutants of that code in `src/ui_angle.cpp`,
`src/settings.cpp` and `src/ui_settings.cpp` were run again, 9 of the
earlier ones gone with the code they changed. 17 fail the suite; the armed
check survives, and leaving out the spare byte in
`settingsCopyAngle()` is equivalent, since validation keeps that byte at 0.

Not verified on the board: everything the driver and the model leave open
(B1 to B7), the screen on the panel, a band across the glass as a touch
(B8), the bus with touch and the INA219 while the meter reads (B9), and
the settings block of version 6 written to and read from the board's flash.

### Slice 5 as built

README.md's section "Angle meter", its MENU table and rows, its settings
version, layout and tests; the wiki pages Angle-Meter and Winkelmesser, the
pair in `tools/check_wiki.py`, Home, the sidebar, Setup and Einrichtung, the
screens pages, First-Run, Erste-Schritte, SD-Logging, SD-Aufzeichnung,
Troubleshooting and Fehlersuche for the eight MENU rows and version 6;
CHANGELOG.md, STATUS.md and this note's status. The wiki describes the
standoffs, the LiPo and the cable, a straight bar for incidence, a clip and a
band past the glass (decision 6), the lash check, and that the meter cannot
tell a moved model; it gives no accuracy figure.

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
