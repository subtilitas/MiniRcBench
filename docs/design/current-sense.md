# Current sense and the bench converter

Status: slices 1 to 5 built, slices 1, 3, 4 and 5 host-tested and slice 2
checked by `tools/make_wiring.py --check` ([slice 2 as
built](#slice-2-as-built), [slice 3 as built](#slice-3-as-built), [slice 4 as
built](#slice-4-as-built), [slice 5 as built](#slice-5-as-built)), none
tested on hardware; slice 6 not built. The owner's decisions of 2026-11-13
and 2026-11-14 are recorded under [decisions](#decisions). On 2026-11-14 the
owner answered Q1 to Q5: one LiPo cell into a synchronous boost, the 0.1 Ω shunt
as sold, the servo tester switching the supply always, rcbench's limit run as
the protection, and the settings carried over from the older layouts. One point
is open: the converter module ([Q1](#q1-the-converter-module)); the 5 V boost
board the owner modelled names no chip, and the owner does not know it, so its
ratings come only from bench tests B6 and B7. Slice 6 waits on being
built. Slices 3 to 5 rely on no rating of the module: the 500 ms start-up
limit is this note's value until B6, and the cuts' thresholds are decision
13's and this note's. The settings carry-over of decision 14 is built
and host-tested. No INA219 has been on the board's bus, and no boost
converter has been on GP28 or GP29: every figure below is from a datasheet,
from rcbench's records or computed, and the ones that are not known are
stated as unknown. The slices are at the end ([slices](#slices)); each builds,
passes the host suite and merges alone.

Terms: INA219 is Texas Instruments' (TI) current and power monitor with an
I2C (Inter-Integrated Circuit) interface; SDA is the I2C serial data line
and SCL the serial clock line. PGA is the INA219's programmable gain
amplifier, ADC analogue-to-digital converter, LSB least significant bit,
POR power-on reset, NACK not acknowledged, SMBus System Management Bus.
EN is a converter's enable input, VIN its input voltage and VOUT its output
voltage. LiPo is lithium-polymer, a cell of it 3.0 V to 4.2 V, and 1S one
such cell. HV is high-voltage, a servo rated above 6.0 V. ESC is electronic
speed controller, BEC battery eliminator circuit, an ESC's regulated
output. PWM is pulse-width modulation, UI user interface, SD Secure
Digital, CSV comma-separated values, GPIO general-purpose input/output,
SIO the RP2350's single-cycle I/O block, IOVDD the RP2350's GPIO supply,
3.3 V on this board, and SDK the Pico software development kit, 2.1.1.
DShot is the digital ESC protocol the bench drives on SIG outside a claim,
and BOOTSEL the RP2350's USB (Universal Serial Bus) boot mode. The PD mini
is rcbench's USB PD (Power Delivery) supply module, which reports its own
voltage and current. SIG and AUX are the two signal pins of
[pin-claim.md](pin-claim.md).

Sources:

- TI INA219 datasheet, SBOS448G (revised December 2015), cited by section
  and table title. Section and table numbers are given as SBOS448G numbers
  them as far as known; each number marked "(to confirm)" is to be checked
  against the document.
- rcbench at `origin/main` 5ef42efdc, MIT, same holder: `shared/sense/`
  (`sense_bus.c`, `ina228.c`, `ina3221.c`, `sense_sched.c`),
  `shared/servo/` (`servo_move.c`, `servo_test.c`, `servo_brownout.c`,
  `servo_report.c`, `servo_limit.c`, `servo_limrun*.c`, `servo_sync.c`),
  `docs/Servo.md`, `STATUS.md`, and the INA219 module drawing
  `hardware/kicad/modules/ina219_mcu219.py`. rcbench has no INA219 codec.
- This repository: `src/servo_supply.{h,cpp}`, `src/cst328.cpp`,
  `src/touch.h`, `src/board_rp2350_touch_lcd_2_8.h`, `src/main.cpp`,
  `tools/make_wiring.py`, [kst-programming.md](kst-programming.md),
  [servo-tester.md](servo-tester.md), [pin-claim.md](pin-claim.md).

## What it does

- An INA219 on J3, the board's shared I2C1 bus, measures the servo supply's
  current and voltage on the converter's output, high side, between the
  converter and the servo's plus wire.
- One boost converter supplies every servo on the bench. GP28 or GP29
  switches its EN, as `SERVO SUPPLY` in SETTINGS says today. It takes the
  place of the converter of wiring part C.
- Core0 reads the sensor once per UI frame, after the touch read, and
  publishes each reading for core1, where slices 3 and 4 read it.
- The readings feed a readout, the servo tester's switched supply, a cut
  on overcurrent and stall, timing of a servo's move from its current, and
  a log on the card.

It does not measure the horn's angle, and it does not set the supply's
voltage: the converter has one output voltage. The tests of rcbench that
need either stay out ([rcbench's tests](#rcbenchs-tests-on-current-only)).

## Electrical

### Place in the circuit

```
  input +  ------- VIN   BOOST   VOUT --- IN+ [INA219 shunt] IN- --- servo +  (red)
  input -  ---+--- GND           EN  ---+---- GP28, J4 pin 11, or GP29, J4 pin 12
              |                         |
              |                    pull-down, at the converter
              |                         |
              +-------------------------+------------------------ servo -  (brown)
              |
  board GND --+   the one ground tap: J5 pin 4, or J4 pin 1 or 5
  board SIG  ----------------------------------------------------- servo signal

  J3 pin 1 SCL (GP7) ----- INA219 module SCL
  J3 pin 2 SDA (GP6) ----- INA219 module SDA
  J3 pin 3 3V3       ----- INA219 module VCC
  J3 pin 4 GND       ----- INA219 module GND, and nothing else
```

- IN+ goes to the converter's VOUT and IN- to the servo's plus wire. The
  servo's current flows through the module's screw terminal and its shunt.
  Swapped, the current reads negative; an idle current below -20 mA is
  reported as `SENSOR REVERSED`, rcbench's rule (`SERVO_TEST_NEG_IDLE_A`).
- The bus voltage is measured from IN- to the INA219's GND (SBOS448G,
  "Feature Description", 8.3, to confirm): the voltage on the servo's side
  of the shunt, measured against board ground.
- The module's GND goes to J3 pin 4 only. It is the INA219's reference and
  carries its supply current, at most 1 mA (SBOS448G, Electrical
  Characteristics, 7.5, to confirm). The servo's current returns through
  the converter's minus and never through J3. The one ground tap of part C
  stays as it is.
- The common module, the MCU-219 drawn in rcbench's KiCad library, also
  brings VIN- and VIN+ out on its 2.54 mm header, next to SDA (header order
  Vcc, Gnd, Scl, Sda, Vin-, Vin+; rcbench `ina219_mcu219.py`). The servo
  current goes through the screw terminal only, and the header's VIN- and
  VIN+ pins stay unconnected: a bridge from VIN- to SDA puts the servo
  supply on GP6 and on the touch controller.

### Address

The INA219's two address pins, A0 and A1, each tie to GND, VS (its
supply), SDA or SCL, for 16 addresses from 0x40 to 0x4F (SBOS448G,
"Serial Bus Address", table "Address Pins and Slave Addresses", Table 1,
to confirm). Both at GND is 0x40, the common module's default with its two
address bridges open. With the bridges, the module reaches 0x40, 0x41,
0x44 and 0x45.

| Address | Device | Source |
|---|---|---|
| 0x1A | CST328 touch controller | `CST328_I2C_ADDR`, board header |
| 0x40 | INA219 | this note |
| 0x51 | PCF85063 RTC (real-time clock) | `PCF85063_I2C_ADDR`, board header |
| 0x6B | QMI8658 IMU (inertial measurement unit) | `QMI8658_I2C_ADDR`, board header |

0x40 is none of the three on the board, and none of the I2C reserved
addresses, 0x00 to 0x07 and 0x78 to 0x7F. The address is a build constant,
`INA219_ADDR`, 0x40. The firmware addresses no other part at 0x40 to 0x4F.

### Shunt and range

The common module carries a 0.1 Ω shunt, marked R100, in a 2512 package
(rcbench `ina219_mcu219.py`, from a vendor photo). The shunt voltage
register's LSB is 10 µV at every PGA setting (SBOS448G, "Shunt Voltage
Register", 8.6.3.1, table "Shunt Voltage Register Format", to confirm), so
one code is 100 µA on 0.1 Ω. The ADC resolution is stated as 12 bits; how
many of the register's bits carry information at /2 to /8 is to confirm
against the datasheet. If 12 bits span ±320 mV at /8, the effective step
is 156 µV, 1.56 mA on 0.1 Ω.

The servo currents in the table are a sizing envelope, assumed, not
measured; per-servo values come from bench test B8. rcbench's records give
two points: an MS24 moved at 0.16 A to 0.18 A on the PD mini's readings,
and the same servo's highest 1 ms sample on an INA3221 reached that part's
end code, 1.637 A, so its peak lies above 1.6 A (rcbench `docs/Servo.md`,
"The meter" and "What is not measured").

| PG bits | Gain | Shunt range | Current range, 0.1 Ω | Full-scale code | Idle, 0.005 A to 0.05 A | Moving, 0.1 A to 1.5 A | Stall, 1 A to 3 A |
|---|---|---|---|---|---|---|---|
| 00 | /1 | ±40 mV | ±0.4 A | ±4000 | covered | clipped above 0.4 A | clipped |
| 01 | /2 | ±80 mV | ±0.8 A | ±8000 | covered | clipped above 0.8 A | clipped |
| 10 | /4 | ±160 mV | ±1.6 A | ±16000 | covered | covered | clipped above 1.6 A |
| 11 | /8 | ±320 mV | ±3.2 A | ±32000 | covered | covered | covered to 3.2 A |

PGA ranges: SBOS448G, "Configuration Register", 8.6.2.1, PG bits table
(to confirm). The register step stays 10 µV at every gain, so a lower gain
gains no step size; it only shortens the range. The bench runs at /8.

**Stall at 3.2 A.** /8 reads to 3.2 A; a current above it reads at the end
of the range. What the part reports past 320 mV is not stated in the
datasheet as far as known: a code at or beyond ±32000 counts as clipped,
never as a value, as rcbench's fake parts treat the end code
(`test/host/fake_ina.h`). A servo whose stall current is above 3.2 A reads
3.2 A and trips the hard limit ([protection](#protection)).

**Shunt power.** I²R on 0.1 Ω: 0.10 W at 1 A, 0.40 W at 2 A, 0.90 W at
3 A, 1.02 W at 3.2 A. The MCU-219's shunt rating is not stated by any
source in reach; a 2512 resistor is commonly rated 1 W to 2 W. Adafruit
states 2 W for its own INA219 module's shunt (to confirm on Adafruit's
product page). At 3.2 A the dissipation reaches a 1 W rating, so a stall
held for long is outside it; the stall cut of the owner's limit run holds
a current above 1.0 A to 400 ms ([protection](#protection)). Neither the
screw terminal's nor the module's trace current rating is known.

**Burden.** The shunt drops 0.10 V at 1 A and 0.30 V at 3 A. On a 5.0 V
supply the servo sees 4.90 V at 1 A, 2 % low, and 4.70 V at 3 A, 6 % low
and under the 4.8 V the servo tester's wiring range starts at
([pin-claim.md](pin-claim.md#wiring)). The leads and the terminal add an
unknown drop. The bus voltage reading is taken behind the shunt, so it
shows the voltage after the drop. The owner kept the 0.1 Ω shunt as sold
on 2026-11-14 ([decision 11](#decisions)).

### Bus voltage

The bus voltage register holds 13 bits in bits 15 to 3, 4 mV a step; bit 1
is CNVR (conversion ready) and bit 0 OVF (math overflow) (SBOS448G, "Bus
Voltage Register", 8.6.3.2, to confirm). BRNG selects a 16 V or a 32 V
full-scale range; the input itself is specified to 26 V (SBOS448G,
"Configuration Register", BRNG bit, and 7.1, to confirm). The bench sets
BRNG 0, 16 V: 5.0 V reads as code 1250, and an 8.4 V HV supply fits.

CNVR is cleared by a read of the power register or a write of the
configuration (to confirm); the bench reads neither in its cycle, so CNVR
is not used. OVF concerns the current and power registers, which need the
calibration register; the bench does not use them (below), so OVF is not
used either.

### Conversion time and averaging

ADC settings (SBOS448G, "Configuration Register", table "ADC Settings",
Table 5, to confirm; typical values):

| BADC or SADC | Conversion | Time |
|---|---|---|
| 0X00 | 9 bit | 84 µs |
| 0X01 | 10 bit | 148 µs |
| 0X10 | 11 bit | 276 µs |
| 0X11 or 1000 | 12 bit | 532 µs |
| 1001 | 12 bit, 2 samples averaged | 1.06 ms |
| 1010 | 4 samples | 2.13 ms |
| 1011 | 8 samples | 4.26 ms |
| 1100 | 16 samples | 8.51 ms |
| 1101 | 32 samples | 17.02 ms |
| 1110 | 64 samples | 34.05 ms |
| 1111 | 128 samples | 68.10 ms |

The maximum conversion times in the electrical characteristics are longer
than these typical values (to confirm). MODE 111 converts the shunt, then
the bus, continuously (SBOS448G, table "Mode Settings", Table 6, to
confirm); one cycle is the shunt time plus the bus time.

What each use needs, against a read every UI frame, 25 ms:

| Use | Needs | What the setting below gives |
|---|---|---|
| Readout | a value every 100 ms to 250 ms that represents the period | the mean of each 17.02 ms block, read every 25 ms |
| Protection | a stall seen within 100 ms; no gap a stall hides in | a step seen within one read and one cycle; a full step within about 60 ms (25 + 17.55 + 17.55 ms, computed) |
| Switch-on and switch-off read-back | the bus voltage within 50 ms | one 532 µs bus sample per 17.55 ms cycle |
| Move timing | rcbench's IOmcu samples at 1 kHz; its panel times moves from 10 readings a second | 40 readings a second: travel times are an upper bound on when the current shows the arrival, by up to one read and two cycles |
| Log | 40 rows a second | one row per reading |

The bench's configuration, written once at the probe:

| Field | Bits | Value | Meaning |
|---|---|---|---|
| RST | 15 | 0 | |
| BRNG | 13 | 0 | 16 V |
| PG | 12 to 11 | 11 | /8, ±320 mV |
| BADC | 10 to 7 | 0011 | bus, 12 bit, 532 µs |
| SADC | 6 to 3 | 1101 | shunt, 32 samples, 17.02 ms |
| MODE | 2 to 0 | 111 | shunt and bus, continuous |

That is 19EFh (`INA219_CONFIG_BENCH`). One cycle takes 17.55 ms; each
read returns the newest completed block, so each reading represents
17.02 ms of the 25 ms between reads, 68 %. A current pulse shorter than
about 8 ms can fall between two blocks. 64 samples, 34.05 ms, would cover
every millisecond but return one block twice in a row at 25 ms reads, and
CNVR, which would tell the two apart, is not read; 32 samples is the
longer block that a 25 ms read never sees twice.

The calibration register stays at its POR value, 0. The current register
is not read: current is the shunt code times 10 µV over the shunt
resistance, computed on core0, which saves one register read a frame. The
datasheet allows the shunt and bus registers to be read without
calibration (SBOS448G, "Simple Current Shunt Monitor Usage", 8.5.3, to
confirm). After a POR of its own the part runs on 399Fh: /8, 32 V range,
12 bit, continuous. Its shunt and bus codes keep their LSB there, so a
reading after an unseen reset is right in value, only shorter in its
average; the read-back of the configuration finds the reset within 1 s
([errors](#errors)).

### The bus on J3

| Item | Value | Source |
|---|---|---|
| J3 pin 1 | GP7, I2C1 SCL | board header connector table |
| J3 pin 2 | GP6, I2C1 SDA | board header |
| J3 pin 3 | 3.3 V | board header |
| J3 pin 4 | GND | board header |
| Bus clock | 400 kHz, `i2c_init(TOUCH_I2C, 400 * 1000)` | `src/cst328.cpp`, `touchInit()` |
| RP2350 pad pull-ups on SDA and SCL | on, `gpio_pull_up()` in `touchInit()`; 50 kΩ to 80 kΩ as kst-programming.md gives for the pads' pulls | `src/cst328.cpp` |
| Board pull-up resistors on SDA and SCL | unknown: neither the board header nor any document in the repository states them; the Waveshare schematic linked in the board header is the source to read | |
| Module pull-ups | commonly 10 kΩ to the module's VCC on the MCU-219 and Adafruit modules; to check on the module in hand | |
| INA219 bus speed | fast mode to 400 kHz, and a high-speed mode (SBOS448G, "Bus Timing Diagram Definitions", 7.6, to confirm) | |

**Pull-ups.** Fast mode allows 300 ns of rise time and 400 pF of bus
capacitance (NXP UM10204, I2C specification, fast-mode table). A 30 % to
70 % rise takes 0.8473 R C, so the pull-up and the capacitance meet
R C ≤ 354 ns. The pads' 50 kΩ alone allow 7 pF, which no board meets.
The touch driver runs the bus at 400 kHz, and STATUS.md records hardware
use of touch, without saying whether on this board. The board most likely
carries pull-up resistors; that is an inference, not a recorded fact.
The module's 10 kΩ, if fitted, adds in parallel. With 10 kΩ alone the
bus may carry 35 pF; with a board value of 4.7 kΩ in parallel, 3.2 kΩ, it
may carry 110 pF. The lowest pull-up
allowed is 967 Ω, (3.3 V − 0.4 V) / 3 mA, fast mode's low level at its
sink current.

**Lead length.** The capacitance of the board's own bus, of the J3 lead
and of the module is not known. A loose JST-SH lead is assumed at 50 pF/m
to 100 pF/m. A lead of 0.3 m then adds 15 pF to 30 pF. The design takes
0.3 m as the longest J3 lead until bench test B1 measures the rise time
with the lead and the module; if the edges miss 300 ns, the bus runs at
100 kHz, where standard mode allows 1000 ns.

**J3's pin order.** J3 numbers SCL, SDA, 3V3, GND from pin 1. The Qwiic
and STEMMA QT order is GND, VCC, SDA, SCL from pin 1. With the board
header's numbering, a straight Qwiic cable would put SCL on the module's
GND; where pin 1 sits on the connector is in the board maker's schematic
and not checked here. The bench uses a pigtail from J3 to the module's
2.54 mm header pins, never an off-the-shelf Qwiic cable.

### Supply and ground

The module takes 3.3 V from J3 pin 3. The INA219's supply range is 3.0 V
to 5.5 V and its supply current at most 1 mA (SBOS448G, 7.5, to confirm).
Its pull-ups then go to 3.3 V, so no level above IOVDD reaches GP6 and
GP7. The module is never fed from 5 V: its pull-ups would then pull GP6,
GP7, the touch controller, the IMU and the RTC to 5 V.

### INA219 ratings used

| Rating | Value | SBOS448G |
|---|---|---|
| Supply VS, absolute maximum | 6 V | 7.1, Absolute Maximum Ratings (to confirm) |
| IN+ to IN-, differential | -26 V to 26 V | 7.1 (to confirm) |
| IN+ and IN-, common mode | -0.3 V to 26 V | 7.1 (to confirm) |
| SDA | GND - 0.3 V to 6 V | 7.1 (to confirm) |
| SCL | GND - 0.3 V to VS + 0.3 V | 7.1 (to confirm) |
| Current into any pin | 5 mA | 7.1 (to confirm) |
| Supply range | 3.0 V to 5.5 V | 7.5 (to confirm) |
| Registers | 00h configuration (POR 399Fh), 01h shunt voltage, 02h bus voltage, 03h power, 04h current, 05h calibration (POR 0000h) | 8.6, "Register Maps", register set summary table (to confirm the table number) |
| Configuration fields | RST 15, BRNG 13, PG 12 to 11, BADC 10 to 7, SADC 6 to 3, MODE 2 to 0 | 8.6.2.1 (to confirm) |
| Register pointer | kept between transactions; a read of another register needs a pointer write first | 8.5, "Serial Interface", reading from the INA219 (to confirm) |
| Calibration | Cal = trunc(0.04096 / (Current_LSB × R_SHUNT)), not used here | 8.5.1, Equation 1 (to confirm) |
| SMBus timeout | not known to be stated for the INA219; the design does not rely on one | |

### The touch controller when the sensor fails

The touch controller shares SDA and SCL with the sensor. What each failure
does to it:

| Case | The bus | Touch |
|---|---|---|
| No module on J3 | idle; each probe is one address byte and a NACK, 27.5 µs at 400 kHz | unchanged |
| Module plugged, powered | as designed | unchanged; each frame's I2C time grows by 240 µs ([transaction times](#transaction-times)) |
| Module on SDA, SCL and GND with its VCC lead open | the module's pull-ups, if fitted, load SDA and SCL towards its unpowered VCC net, and SCL's rating of VS + 0.3 V suggests a clamp from SCL to VS (to confirm). The levels that result depend on the board's pull-ups, which are unknown | may fail; bench test B3. J3 carries all four wires in one plug, so the case needs a broken wire |
| The INA219 holds SDA low, for example after a board reset mid-byte | every transaction fails; the bus-stuck rule recovers it with 9 clocks on SCL and a STOP ([errors](#errors)) | fails until the recovery frees the bus. A part that holds SDA low for ever keeps touch dead until J3 is unplugged |
| A short on the J3 lead from 3.3 V to GND | the board's 3.3 V collapses | the board resets; EN goes undriven, the pull-down switches the converter off |

`touchPoll()` holds the previous contact for one failed read and reports
the finger up after two (`POLL_FAIL_LIMIT`). With the bus dead, nothing on
the screen can be tapped, STOP included, while core0 still runs and sends
the heartbeat. The supply therefore goes off on a stuck bus and on a touch
controller that has not answered for 250 ms
([safety and interlocks](#safety-and-interlocks)).

## The boost converter

### Boost and output disconnect

The owner's converter is a boost, one for the whole bench, with its EN on
GP28 or GP29. The existing requirement of part C stands
([kst-programming.md](kst-programming.md#hardware)): the converter
disconnects its output while disabled. A plain boost's inductor and diode
pass VIN, less a diode drop, to the output while it is disabled: with EN
low the servo stays powered, during DShot too, with the converter's
output on the lead's middle wire. A boost qualifies only as a synchronous
boost with load disconnect, or as a type with another output switch. A
three-pin module with VIN, GND and VOUT has neither an EN nor a disconnect
and does not qualify. The sensor detects the failure: with EN low and a
disconnect missing, the bus voltage reads VIN less a diode, not 0 V
([read-back](#read-back-from-the-bus-voltage)).

A boost works only while VIN stays below VOUT. For a 5.0 V output its input
must stay below 5.0 V less the converter's headroom, at every state of
charge; that limits the choice of input.

### Input

The owner chose one LiPo cell, 3.0 V to 4.2 V, on 2026-11-14 ([decision
10](#decisions)): the first row below. The module is not named yet
([Q1](#q1-the-converter-module)). The options and what each means, with
an efficiency of 85 % assumed for the input current:

| Input | 5.0 V rail | Input current at 5.0 V, 3 A out | HV servo |
|---|---|---|---|
| One LiPo cell, 3.0 V to 4.2 V | a true boost: VIN stays below VOUT over the whole discharge | 4.2 A at 4.2 V, 5.3 A at 3.3 V | 5.0 V only; 7.4 V or 8.4 V needs another output setting, which one converter at one voltage does not have |
| USB 5 V from a charger or power bank of its own (never J4 pin 2, the board's VBUS) | not reachable with a boost: USB's 4.75 V to 5.25 V overlaps the output, so a boost cannot regulate 5.0 V from it | 3.5 A at 5.0 V; a 15 W USB-C source gives 3 A at most, so a 3 A servo load is out of reach | a boost from USB to 6.0 V to 8.4 V is possible, at most about 1.5 A at 8.4 V from 15 W; it breaks decision 16's 5 V programming supply |
| A 2S pack, 6.0 V to 8.4 V, or a bench supply | not reachable with a boost: VIN is above VOUT. A buck or a buck-boost does it, such as the Pololu S13V30F5 of kst-programming.md | | as above |

The cell's own protection, its charging and a fuse at the cell are outside
the bench; a fuse is needed, since a LiPo cell delivers far more current
than the converter's wiring takes.

### Output voltage and current

- 5.0 V, for every servo the owner programs, HV servos included
  ([kst-programming.md](kst-programming.md#decisions), decision 16). The
  servo tester runs every servo at 5.0 V on this supply; an HV servo moves
  slower there than at its rating. A test at 7.4 V or 8.4 V uses
  `SERVO SUPPLY` at `OWN` and a supply of the user's.
- Rated for the servo's stall current: at least 3 A continuous at the
  lowest VIN used, which matches the INA219's range at /8. Below that, a
  stall drives the converter into its current limit, VOUT collapses, and
  the bench sees it as a low bus voltage ([protection](#protection)).
- A current limit of its own. The bench's cuts act after tens of
  milliseconds and protect the servo and the shunt; a short circuit is the
  converter's own current limit's and the input fuse's to stop.
- VOUT accuracy: ±3 % gives 4.85 V to 5.15 V, before the shunt's drop.

### Enable and its pull-down

The rule of [kst-programming.md](kst-programming.md#hardware) applies
unchanged, with VIN the boost's input:

- EN takes 3.3 V logic as high at every VIN used. A rising threshold given
  as a fraction of VIN that lands above about 2.0 V does not qualify.
- An EN with no internal pull-up: a pull-down of 10 kΩ to 100 kΩ from EN to
  the input's minus, at the converter.
- An EN with an internal pull-up R_PU to VIN: the pull-down R_PD must keep
  EN below the falling threshold V_F, at most 3.6 V, and at most 0.5 V with
  the board unpowered, at the highest VIN. From EN = VIN × R_PD /
  (R_PU + R_PD): R_PD ≤ R_PU × V_F / (VIN − V_F), and
  R_PD ≤ R_PU × 0.5 V / (VIN − 0.5 V). On one cell, 4.2 V, the second
  gives R_PD ≤ 0.135 R_PU. The 3.6 V limit asks only R_PD ≤ 6 R_PU at
  4.2 V and never binds on one cell. For an R_PU of 100 kΩ, an example
  only, R_PD is at most 13.5 kΩ, and 10 kΩ fits.
- The enable pin then drives 0.33 mA into a 10 kΩ pull-down at 3.3 V.

The PowerBoost 1000 Basic of the example modules pulls EN up to its cell
with a resistor whose value is not stated; it qualifies once the value is
known ([example modules](kst-programming.md#example-modules)). Its
continuous output rating, about 1 A in Adafruit's description (to
confirm), is below a stall.

### Read-back from the bus voltage

`servoSupplyIsOn()` reports the level the bench drives, with no
read-back. The bus voltage at IN- is a read-back:

| Check | Rule | Constant |
|---|---|---|
| On | after the switch-on, the first reading at or above 4.5 V | `SUPPLY_ON_MIN_MV` 4500 |
| Start-up time | from the switch-on command to that reading | measured, not assumed |
| Did not come on | no reading at or above 4.5 V within 500 ms of the switch-on | `SUPPLY_START_MAX_MS` 500, until bench test B6 measures the start-up time |
| Off | after the switch-off, the first reading at or below 1.0 V | `SUPPLY_OFF_MAX_MV` 1000 |
| Not off | a reading above 2.5 V for 2000 ms after the switch-off | a converter with no disconnect, or EN held high |

The resolution of the start-up time is one read and one cycle, 25 ms plus
17.55 ms; the bus sample within a cycle is a 532 µs snapshot. Bench test
B6 measures the start-up time against a scope, with a bring-up build that
polls the bus register alone every 1 ms (MODE 110, 532 µs a conversion).
The KST search's `KST_SUPPLY_ENTRY_DELAY_MS` and its window
([kst-programming.md](kst-programming.md#timing-of-the-switch-on)) then
rest on a measured start-up time, not an unknown one.

The switch-off decays with the load. With a servo connected the output
falls within milliseconds (expected, not measured); with no load the
output capacitors hold their charge for an unknown time. "Not off" is
therefore a warning on the screen, not a cut: EN is already low, and the
bench has nothing left to switch.

## Bus sharing in software

### I2C1 today

`src/cst328.cpp` is the only client of I2C1. `touchInit()` runs on core0
in `setup()`: `i2c_init()` at 400 kHz, the pads' pull-ups, the touch reset
and a probe. `touchPoll()` runs on core0 from `uiTick()`, every 25 ms
(`UI_PERIOD_MS`), with one 5-byte read of register D000h, a timeout of
2000 µs per transfer (`I2C_TIMEOUT_US`). The IMU and the RTC are not
used. Nothing on core1 touches I2C1.

### The second client

I2C1 stays a core0 bus. `src/sense.cpp` reads the INA219 in `uiTick()`,
directly after `touchPoll()`. The two clients run one after the other on
one core, so neither waits for the other and no lock is needed. The touch
read keeps its place at the top of the frame and is never delayed by the
sensor.

Reading on core1 would cost more than it gives: the SDK's blocking I2C
calls are not safe from two cores, so both clients would need a lock;
core1 would wait for a touch read of 210 µs, or 4 ms when the bus fails;
and core1 is the core that must never block. The cost of core0 is the
rate, 40 readings a second, and a reading delayed by whatever delays a UI
frame. The heartbeat covers a core0 that stops
([safety and interlocks](#safety-and-interlocks)).

`setup()` calls `senseInit()` after `touchInit()`, which has set the bus
up. `touchInit()` keeps the `i2c_init()`.

### Transaction times

Clocks per transaction, as rcbench counts them (`sense_sched.h`):
9 × (bytes) + 2 for a start and a stop, + 1 for a repeated start.

| Transaction | Bytes on the wire | Clocks | 100 kHz | 400 kHz |
|---|---|---|---|---|
| Touch, `rdRegs(D000h, 5)`: address, 2 register bytes, repeated start, address, 5 data bytes | 9 | 84 | 840 µs | 210 µs |
| INA219 register read: address, pointer, repeated start, address, 2 data bytes | 5 | 48 | 480 µs | 120 µs |
| INA219 register write: address, pointer, 2 data bytes | 4 | 38 | 380 µs | 95 µs |
| An address with no answer | 1 | 11 | 110 µs | 27.5 µs |
| The sensor's frame: shunt and bus registers | 10 | 96 | 960 µs | 240 µs |
| The probe: read, reset, read, configure, read back | 23 | 220 | 2200 µs | 550 µs |

Per frame, touch and sensor take 450 µs of 25 ms at 400 kHz, 1.8 %, and
1800 µs at 100 kHz, 7.2 %. Once a second the configuration is read back,
120 µs more. The RP2350 I2C block's own gaps between bytes and
transactions, and the SDK's call time, are not counted and not measured.

Worst case added to the frame:

- the sensor answering: 240 µs at 400 kHz, 960 µs at 100 kHz;
- the sensor absent: one NACK per register before the part goes offline,
  then one probe a second, 27.5 µs each;
- the bus timing out: each sensor transfer has a timeout of 1000 µs
  (`SENSE_TIMEOUT_US`), about 10.5 times the longest sensor transfer at
  400 kHz, a write of 95 µs, and about 2.6 times it at 100 kHz, 380 µs.
  A failed transfer ends the sensor's frame, so a frame takes the
  transfers that answered before it plus 1000 µs. Two timeouts in a row
  mark the bus stuck, but a touch read that answers clears the mark
  again, so the sensor keeps trying, one transfer a frame, until three
  failures take the part offline; then it is probed once a second.

The touch read itself is unchanged in each case. With the bus stuck it
times out on its own: a transfer of 2000 µs, a second only when the first
answered. A lost arbitration ends a transfer within a few clocks.

The host suite measures each failure mode's worst frame at 400 kHz
(`testFrameTime` in `test/test_sense.cpp`), with one timeout after each
count of transfers that answer:

| Frame | The transfer that times out | Bus time |
|---|---|---|
| Sensor, probe | the read-back of 19EFh, its data read, after 192 clocks | 1480 µs |
| Sensor, probe | the write of 19EFh, after 134 clocks | 1335 µs |
| Sensor, reading with the read-back | the bus register's data read, after 116 clocks | 1290 µs |
| Sensor, reading | the bus register's data read, after 68 clocks | 1170 µs |
| Sensor, any frame | its first transfer | 1000 µs |
| Touch | the data read of D000h, after 29 clocks | 2073 µs (2072.5 µs rounded up) |
| Touch | the pointer write | 2000 µs |

So a frame blocks core0 on the bus for at most 1480 µs for the sensor and
2073 µs (2072.5 µs rounded up) for touch. The recovery's 9 clocks, about
100 µs, and the wait for the lines after an abort, up to 10 µs, are GPIO time
and not counted.

### The reading on core1

Built in slice 1: the sample, its lock and its age rule (`senseSample()`,
`senseFresh()`). Built in slice 3: the servo job copies it on every pass and
reads the bus voltage back after each switch-on and switch-off. Built in
slice 4: the servo, KST and Hitec jobs cut the supply on its readings
(`src/supply_guard.*`). Built in slice 5: the servo job times each step from
its readings (`src/move_timer.*`).

```
struct SenseSample {           // src/sense.h
    uint32_t seq;              // +1 per reading; 0 for none yet
    uint32_t takenMs;          // millis() at the read, core0
    int32_t  currentUa;        // shunt code x 10 uV / shunt, signed
    uint16_t busMv;            // bus code x 4 mV, at IN-
    uint8_t  flags;            // clipped, reset seen; reversed: slice 3
    uint8_t  state;            // NONE, ABSENT, ONLINE, OFFLINE, STUCK
};
void senseSample(SenseSample *out);   // either core, under the lock
```

Core0 publishes each reading under a critical section of the sense
module's own, as `EscTelemetry` is shared. A core1 job copies it on every
pass of `loop1()`. A new `seq` is a new reading. Its age at core1: up to
35 ms of conversion (the newest block can have closed 17.55 ms before the
read and spans 17.02 ms), the 25 ms between reads, and the pass, under
1 ms during a servo claim (not measured). A step of current shows fully
within about 60 ms (computed). A reading older than 250 ms
(`SENSE_STALE_MS`, the heartbeat's own timeout) counts as none.

### Errors

rcbench's `sense_bus.c` makes the decisions, copied unchanged into
`lib/rcbench/shared/sense/`. Its wire is two callbacks; `src/sense.cpp`
supplies them over the SDK:

| SDK result | `sense_err_t` |
|---|---|
| the byte count asked | `SENSE_OK` |
| `PICO_ERROR_GENERIC`, an abort, or a short count, with SDA and SCL high within 10 µs after it: an address or byte not acknowledged | `SENSE_NACK` |
| `PICO_ERROR_GENERIC` with SDA or SCL low for 10 µs after it: arbitration lost to a part holding the line (DW_apb_i2c's abort behaviour, to be confirmed on hardware, bench test B3) | `SENSE_BUS_LOW` |
| `PICO_ERROR_TIMEOUT` | `SENSE_TIMEOUT` |
| SDA or SCL reads low before the transaction (`gpio_get()` on GP6 and GP7) | `SENSE_BUS_LOW`, nothing sent |

The SDK's read returns on an abort without waiting for the STOP the I2C
block sends after it, so the block's own STOP can still hold a line low
for a moment. `errAfter()` therefore polls the lines once a microsecond
for up to 10 µs (`SENSE_SETTLE_US`), and only a line low for all of it
counts. The bus runs at 400 kHz, where the STOP holds a line low for about
2.5 µs: the SDK sets SCL's low count to 3/5 of the period, 1.5 µs, and its
high count to the rest, 1.0 µs. 10 µs is a margin of about 4 times that,
and about the length of a STOP at 100 kHz, the ceiling.

- **NACK.** The address went out and the lines came back: the bus works and
  the part is missing. Three failed transactions in a row
  (`SENSE_FAILS`) take an online part offline, 75 ms at 40 readings a
  second; a part not online is probed every 1000 ms (`SENSE_RETRY_MS`).
- **Timeout and stuck bus.** Two timeouts in a row
  (`SENSE_STUCK_TIMEOUTS`), or a line held low, mark the bus stuck. While
  it is stuck nothing is sent, and a recovery runs at once and every
  100 ms (`SENSE_RECOVER_MS`): GP7, switched to SIO, is driven low and
  released 9 times at about 100 kHz, a STOP follows, and GP6 and GP7
  return to I2C with `i2c_init()` again. The next transaction tests the
  bus. The touch
  driver reports each of its transfers to the same record with
  `sense_bus_note()`, so a bus that fails touch is seen between sensor
  reads too. A touch NACK is not a stuck bus; a touch abort with a line
  low after it is, so a part holding SDA cannot hide behind the touch
  driver's lost arbitrations.
- **A reset of the part.** Once a second the configuration is read back.
  399Fh, or any value but 19EFh, twice in a row is a reset: the part goes
  offline, is probed and configured again, and the reading carries the
  reset flag. FFFFh, the bus that no part drives, counts the same.
- **Probe.** Read the configuration first. The INA219 has no identity
  register, so its value is the identity check: 399Fh, the POR value, or
  19EFh, the bench's own, which an INA219 keeps across a reset of the
  board alone (`ina219Identity()`). Any other value is another part: an
  INA226 or an INA3221 at 0x40 reads 4127h or 7127h after its reset
  (`ina3221.h`, `INA3221_CONFIG_RESET`; INA226 from its datasheet, to
  confirm), and a PCA9685's register 00h is its MODE1. Such a part is
  reported as `OTHER PART`. Nothing is written to it but the register
  pointer, 00h, that the read needs. The probe reads it again every
  1000 ms, so an INA219 put in its place is found without a change in
  SETTINGS. An INA219 left on another configuration, such as 019Fh, also
  reads as `OTHER PART` until its supply is cycled. FFFFh is no part. On
  an INA219's value the probe writes RST (8000h), reads 399Fh, writes
  19EFh and reads it back. How long the part needs after RST before it
  answers is not stated as far as known; the probe retries on the next due
  time.

### Detection and the SETTINGS row

A row `CURRENT SENSE` in the SERVO SUPPLY section of SETTINGS, below
`SERVO SUPPLY`: `NONE`, the default, or `INA219`. With `NONE` nothing is
sent to 0x40. With `INA219` the part is probed at boot after
`touchInit()` and again every 1000 ms while it is not online; the row's
caption shows `FOUND`, `NOT FOUND` or `OTHER PART`.

A row and not detection alone, because the cuts of
[protection](#protection) depend on the sensor. With detection alone, a
sensor that fails its probe at boot leaves the bench without a cut and
with no sign that the user expected one. With the row, a sensor declared
and not found is a fault the screens name, and the servo tester refuses
HOLD TO START on the switched supply until it answers or the row is set to
`NONE`.

The row adds a field to the settings block, `currentSense`, and
`SETTINGS_VERSION` goes from 4 to 5. By [decision 14](#decisions) a
whole block of version 2, 3 or 4 loads into version 5 with every field it
has, validated, and `currentSense` at 0, `NONE`, whatever the version 4
padding byte holds (`settingsCarriedOver()`); only a block of a version
the firmware does not know is discarded whole and the compiled defaults
load once, every setting included. The shunt is a
build constant, `INA219_SHUNT_MOHM`, 100; the address is `INA219_ADDR`, 0x40.
The list grows by one 50 px row ([menu.md](menu.md#settings)).

## Uses

### Readout

- Servo tester: on the readout row, right of the pulse, at scale 2,
  `0.42 A`; below it at scale 1 the bus voltage, `5.02 V`. With two
  servos the current is their sum and reads `A+B 0.84 A`. A clipped
  reading reads `>3.2 A` in amber.
- KST SERVO and HITEC SERVO: on the strip, after the supply's state,
  `SUPPLY ON 0.08 A`.
- SETTINGS: the CURRENT SENSE caption, as above.
- Not on SD LOG, which shows the blackbox log of the ESC tester; the
  sensor does not measure the ESC's battery.

A reading older than 250 ms, or none, reads `-- A`.

### The servo tester on the switched supply

Decision 20 of [servo-tester.md](servo-tester.md#decisions), built here in
slices 3 and 4 except the log of slice 6
([slice 3 as built](#slice-3-as-built), [slice 4 as
built](#slice-4-as-built)): with `SERVO SUPPLY` at `GP28` or `GP29`:

| State | Supply |
|---|---|
| SAFE | off |
| HOLD TO START | on; the CENTRE pulses start once the bus voltage reads 4.5 V, or 500 ms after the switch-on without a sensor |
| STOP, on SERVO and on SET | off at the press, with level 0 on the pins |
| Heartbeat trip, 250 ms | off, `STALLED` |
| A cut of [protection](#protection) | off, the cut's chip |
| BACK, claim end | off, then the pins' quiet wait, then the enable pin released: the DShot pump never runs with the converter on |

- The servo job binds the enable pin with `servoSupplyBind()` and
  releases it with `servoSupplyRelease()`, as the KST and Hitec jobs do.
  One core1 job runs at a time, so the jobs never drive the pin together.
- The SAFE notes add `THE BENCH POWERS THE SERVO AT START.` and keep
  `PULSES ON GP0 ARE THROTTLE TO AN ESC.`; the ESC warning of the KST
  search, `AN ESC ON THIS LEAD GETS THE CONVERTER'S OUTPUT ON ITS BEC
  WIRE.`, joins them.
- The start-up time of each switch-on shows on SET, `SUPPLY UP IN 40 MS`,
  and goes into the log.
- With `SERVO SUPPLY` at `OWN` the servo tester switches nothing, as
  today, and the readout works on whatever supply runs through the
  sensor.

### Protection

Rules on core1, in a pure module `src/supply_guard.{h,cpp}` fed each pass's
sample and the time; it judges each new reading once. Built in slice 4
([slice 4 as built](#slice-4-as-built)).

The servo tester's protection is rcbench's limit run, the owner's decision
of 2026-11-14 ([decision 13](#decisions)): readings above 1.0 A for 400 ms
and a hard cut at 3.0 A, not the stall rule this section recommended; the
hard cut acts at once in the KST and Hitec claims, during an operation too.
A known consequence, which this section warned of: the rule trips on many
servos held against a stop at MIN or MAX, which draw stall current there in
normal use. Bench test B8 checks it on the owner's servos.

| Cut | Condition | Constant | Applies |
|---|---|---|---|
| Hard | one reading at or above 3.0 A, either sign, or clipped | `GUARD_HARD_MA` 3000 | the servo tester; the KST and Hitec claims while the supply is on |
| Stall | readings above 1.0 A, either sign, for 400 ms, none at or below it between (decision 13; designed as 2.0 A for 1000 ms) | `GUARD_STALL_MA` 1000, `GUARD_STALL_MS` 400 | the servo tester |
| Collapse | after the start-up reading, readings under 4.0 V for 1000 ms, none at or above it between | `GUARD_LOW_MV` 4000, `GUARD_LOW_MS` 1000 | the servo tester on the switched supply; the KST and Hitec claims while the supply is on |
| Sensor lost | with CURRENT SENSE at INA219: the part not online, the bus stuck, or no reading for 250 ms | `SENSE_STALE_MS` 250 | the servo tester on the switched supply |

The stall rule as designed was rcbench's automatic test's
(`SERVO_TEST_STALL_ABORT_MS`, 1000 ms; STALL AT's default, 2.00 A).
rcbench's limit run uses 3000 mA
hard and 1000 mA for 400 ms (`servo_limrun.h`), for a search that drives
into a stop by design; the servo tester drives to MIN and MAX, where a
servo against its mechanical stop draws stall current in normal use, so
the tester's stall rule was the longer one. The owner chose the limit run's
values (decision 13).

What the bench does on a cut, in the servo tester: the supply off at once,
level 0 on the pins, the sweep ended, the chip `OVERCURRENT`, `STALL`,
`LOW SUPPLY` or `NO SENSOR` in `C_MAGENTA`, latched as `STALLED` is, until
the next HOLD TO START. With `SERVO SUPPLY` at `OWN` there is no supply
to cut: the cut ends the pulses, and what the servo does without pulses is
the servo's own.

In the KST and Hitec claims the hard cut and the collapse rule switch the
supply off even during a session operation: a current above 3 A is a
fault of the wiring or the servo, not a programming state. The jobs then
take the supply's loss as they take a lapse today: `kst_session_power_cycled()`,
or the Hitec job's off path. The stall rule does not apply there: a
programmed servo is at rest. Pin-claim decision 1, which lets a running
operation finish on a heartbeat lapse, is about the UI stopping, not about
a fault on the supply. The owner decided on 2026-11-14 that the hard cut
acts there too, during an operation ([decision 13](#decisions)). The
collapse rule's place in the claims is this note's choice; the owner has
not decided it.

Detection takes up to about 60 ms from a step of current
([the reading on core1](#the-reading-on-core1)). The cuts protect the
servo and the shunt; they are no fuse.

### rcbench's tests on current only

What rcbench measures and what it only models (rcbench `STATUS.md` and
`docs/Servo.md` at 5ef42efdc):

- The INA228 and INA3221 drivers, `sense_bus.c` and the sampling schedule
  are host-tested against a modelled bus; the front end's parts are
  "chosen, nothing fitted" ("Measurement front end").
- `docs/Servo.md` records bench runs of the automatic test on the PD mini
  with an MG90S, a 1102HB and an MS24, and a bench log of an MS24 with
  INA3221 windows on rcbench 0.14.0; the host suite replays those CSVs.
- The limit search, the limit run and the sync search are host-tested
  against servo models: "the IOmcu image does not call it yet"
  (`servo_limit.h`), "No firmware calls it" (`servo_sync.h`), and the limit
  run "has not been measured on a bench" (`docs/Servo.md`).

| rcbench test or file | With current only | Into MiniRcBench |
|---|---|---|
| `servo_move.c`: movement and arrival from current | possible: idle, moving and holding current, travel time from the command to the arrival, as an upper bound | copied unchanged with `servo_move.h`; its test `test_servo_move.c` and the model `servo_sim.{c,h}` it uses, unchanged; an adapter in the servo job feeds it readings in ms with `settle_n` 2 and `filter_n` 1, as rcbench's panel feeds the PD mini |
| `servo_limit.c`: a stop found by current | possible: the tester drives the pulses, the sensor sees the bind | copied unchanged with `mstime.h`; a later slice, not here |
| `servo_sync.c`: two servos on one surface by total current | possible: both servos on the one supply, one sensor, as `docs/Servo.md` says one sensor suffices | copied unchanged with `mstime.h`; a later slice, not here |
| `servo_test.c`, `servo_report.c`, `servo_str.c`: the automatic test | in part: one characterisation step at the converter's 5.0 V; no step at 4.8, 6.0, 7.4 or 8.4 V | an adapter that presents the converter as a supply with one set point, its readings as `servo_test_reading_t`; `servo_str.c` includes rcbench's `ui_text.def`, which needs a stub; not here |
| `servo_brownout.c`: the brown-out walk | impossible: it steps the supply down from 5.0 V in steps of 0.1 V to 1.0 V | |
| `servo_limrun*.c`: the limit run, with KST travel limits written | possible in principle, from the limit search and the KST session; its protections read INA3221 1 ms samples in 50 ms windows | adapter for the windows and for the KST port; not here |
| The output encoder in `servo_test.c`, `servo_rudder*.c`: angles, angle error, rudder deflection | impossible without an AS5600 on the horn or an IMU on the surface; the board's own IMU sits on the board | |
| `sense_bus.c` | the bus and part state used by the INA219 driver | copied unchanged with `sense_bus.h` |
| `ina228.c`, `ina3221.c`, `sense_sched.c` | other parts, and a 1 ms schedule for core1 of rcbench's IOmcu | not copied; the INA219 codec is new |

The copies go into `lib/rcbench/` at rcbench commit 5ef42efdc, listed in
`lib/rcbench/README.md` as a group of their own, with the
`SPDX-License-Identifier: MIT` line kept.

**Move timing in the servo tester.** MIN, CENTRE and MAX at SPEED 100 %
are step commands. Each one starts a `servo_move` measurement on core1:
the level before it is the mean of the readings of the 500 ms before the
command, the holding level that end's last hold, or the idle level before
one is known; the threshold is rcbench's,
max(20 mA, 3 × the idle noise). The SET screen's last line shows
`MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`, and `LATE` or `UNSEEN` per
rcbench's rules. The time is an upper bound on when the current shows the
arrival: late by up to one read and two cycles, 60.1 ms, plus the wait for
the next PWM frame, 20 ms at 50 Hz. rcbench's PD mini readings were late by
a median of 0.31 s; this meter lags by about a fifth of that. Built in slice 5
([slice 5 as built](#slice-5-as-built)), which settles what this paragraph
leaves open: when a step is timed, the holding level, the hold and the line's
place.

### Logging to the card

A servo tester run with the supply on writes `LOGS/SRVnnnnn.CSV`, one row
per reading:

```
ms,pulse_a_us,pulse_b_us,current_ma,bus_mv,flags
```

About 32 bytes a row, 40 rows a second: 1.3 kB/s, 2.5 blocks of 512 bytes
a second. The file opens at HOLD TO START and closes at STOP, a cut or
BACK. Rows go into a RAM ring of `log_ring.h` and leave through the write
cache of [sd-write-cache.md](sd-write-cache.md), turned on for a SERVO
claim with a log running, so that no card write blocks core0 past the
250 ms heartbeat while the supply is on. At the specification's 511 ms a
block the card takes 1.96 blocks a second against 2.5 coming in, and
the cache's 32 blocks fill in about 60 s of logging; a full queue
drops rows and counts them, and the log's last row says how many. The
blackbox log stays the ESC tester's; the sensor does not measure the ESC.

## Wiring sheet and parts list

`tools/make_wiring.py` draws `docs/wiring.svg` from the pin sources. What
changes:

- **J3.** The connector table in `src/board_rp2350_touch_lcd_2_8.h` marks
  GP6 and GP7 with `*`, "on the shared I2C bus, do not reuse", and
  `role()` maps every `*` pin to "shared I2C bus: do not use". The marker
  splits: J3's pins become the INA219's (`SCL`, `SDA`, `3V3` as "INA219
  VCC", `GND`), and J4 pins 7 and 8 stay "do not use", so the bus leaves
  the board at one connector. `role()` takes the connector as well as the
  name.
- **Part C.** The converter box reads `BOOST`, `5 V out`, `output
  disconnect`; its input box reads the input the owner chooses
  (one LiPo cell, [decision 10](#decisions)). An INA219 box sits on the
  VOUT wire: VIN+ from VOUT, VIN- to the servo's plus. A J3 stub with four
  wires runs to its header. The panel title names the servo tester too.
  The address comes from `INA219_ADDR` in `src/sense.h`, read by the
  script as it reads the enable pins.
- **Part C's notes.** "DC-DC: > 1 A; buck, or output disconnect" becomes
  "boost with output disconnect, 3 A"; "The servo tester does not switch
  it" goes once slice 3 is built.
- `README.md`, "Wiring diagram" and "Switched supply (optional)":
  the boost, the sensor, J3.
- `wiki/Wiring.md` and its German page: J3 is no longer "do not use"; part
  C as above.
- `wiki/Setup.md` and its German page, "What you need":

| Part | For | Notes |
|---|---|---|
| INA219 module, 0.1 Ω shunt, address 0x40 | current and voltage of the servo supply | Powered from J3 pin 3, 3.3 V. The servo's current through the screw terminal only |
| JST-SH 1.0 mm cable, 4 pins | connector J3, to the INA219 | A pigtail to the module's header; not a Qwiic cable |
| Boost converter with EN and output disconnect, 5 V, 3 A | the switched servo supply, every servo tool | Its input one LiPo cell, 3.0 V to 4.2 V (decision 10); the module is not named ([Q1](#q1-the-converter-module)); a 10 kΩ pull-down from EN to ground |

The Hitec classic row of the tool table names the boost converter instead
of "a buck converter or a type with output disconnect".

## Safety and interlocks

What switches the supply off, with `SERVO SUPPLY` at `GP28` or `GP29`:

| Cause | Where decided | Path |
|---|---|---|
| SAFE, STOP, BACK, claim end | core1 job | `servoSupplyOff()`, `servoSupplyRelease()` |
| Heartbeat lapse, 250 ms | core1 job, `core1UiAlive()` | off; the existing rule |
| A cut: hard, stall, collapse | core1, `supply_guard` | off, latched |
| Sensor lost while CURRENT SENSE is INA219, in the servo tester | core1, from `SenseSample.state` and age | off, `NO SENSOR` |
| Bus stuck, in the servo tester | core0 sets it in the sample's state; core1 acts | off: touch cannot reach STOP |
| Touch controller silent for 250 ms (10 polls), in the servo tester | core0 sets a flag beside the heartbeat; core1 acts | off: touch cannot reach STOP |
| Board reset, power-off, core1 hang | outside the firmware | EN undriven; the pull-down holds the converter off. A core1 hang with the supply on leaves it on until a reset (kst-programming.md, "A core1 hang") |

**The sensor fails mid-run.** The servo tester switches the supply off:
the run started with a cut it no longer has. The screen says
`CURRENT SENSOR LOST: SUPPLY OFF.` HOLD TO START is refused until the part
is online again. KST and Hitec claims show `-- A` and go on: their supply
rules do not depend on current.

**The unpowered board.**

| Case | EN | INA219 pins |
|---|---|---|
| Board off, converter input connected | the pull-down holds EN low; the converter is off, its output disconnected | IN+ and IN- at 0 V; VS 0 V; SDA and SCL at 0 V |
| Board off, a converter without a disconnect (does not qualify) | low | IN+ and IN- at VIN less a diode, up to about 4 V on one cell, with VS at 0 V. The common-mode rating, -0.3 V to 26 V, is stated without reference to VS (to confirm); bench test B3 checks the leakage |
| Board off, a servo supply of the user's through the sensor, `SERVO SUPPLY` at `OWN` | not wired | IN+ and IN- at that supply's voltage with VS at 0 V, as above |
| Board on, J3 lead out | as designed | the readings stop; sensor lost |
| Board in reset or BOOTSEL | undriven; the pull-down holds it low; BOOTSEL is bench test B6, as for kst-programming.md step 7 | not addressed |

GP28 and GP29 are not fault tolerant (IOVDD + 0.5 V, RP2350 datasheet,
Table 1680); the 0.5 V limit for an unpowered board applies to the
boost's EN as it does to every EN on part C.

## Tests

### Host fakes

- `test/fake_i2c.{h,cpp}` replaces the I2C stub in `test/fakes.cpp`, where
  every transfer fails. It holds the parts on the bus by address and
  counts clocks per transaction, 9 × bytes + 2 + 1 per repeated start.
- An INA219 model in it: the six registers and their POR values; RST,
  which restores POR; the pointer kept between transactions; BRNG and PG;
  the shunt code from a current and a shunt the test sets, 10 µV a code,
  held at ±32000 beyond /8's range, and limited by each PG setting's range;
  the bus code at IN-, the converter's VOUT less I × R, 4 mV a code;
  conversion timing from the configuration when the test gives the part a
  clock, as rcbench's `fake_ina.h` does for the INA3221, and with a current
  function the shunt code as its mean over the cycle's shunt conversion.
- Faults: absent (NACK); the next n transactions failing with a chosen
  code; SDA or SCL held low; a POR mid-run; another part at 0x40 that
  reads 4127h after its reset.
- A converter and servo model, after `test/kst_supply_sim.h`: the enable
  pin's fake level, a start-up time, a disconnect or none, a current
  limit; a servo whose current is idle, moving or stalled from rcbench's
  `servo_sim.{c,h}`, copied in slice 5, a fixed table before.
- `cst328.cpp` built in the host suite against the same fake bus, with a
  CST328 model that answers D000h, so a test runs touch and sensor on one
  bus. The scripted touch of `test/fakes.cpp` stays for the screen tests.

### What the host suite checks

- The codec: 19EFh and its fields; the shunt code to µA on 0.1 Ω at
  ±1, ±32000 and beyond; the bus code to mV; the clip flag; a negative
  current.
- The probe: 399Fh accepted, 4127h and 7127h reported as another part,
  FFFFh as no part; 19EFh written and read back; a NACK leaves the part
  absent and probed again 1000 ms later, not 999 ms.
- `sense_bus` decisions through the INA219: three NACKs to offline; two
  timeouts to stuck; a recovery every 100 ms; a touch transfer noted; a
  touch NACK not stuck.
- The read-back of the configuration every 1000 ms; a POR found within
  two read-backs; the reset flag.
- Bus time per frame: 96 clocks for the sensor, 84 for touch; with one
  timeout after each count of transfers that answer, at most 1480 µs for
  the sensor and 2073 µs (2072.5 µs rounded up) for touch.
- A read's abort with SDA low for the I2C block's own STOP: a NACK, not a
  line held low.
- The sample: `seq`, its age, 250 ms stale and not at 249 ms, across a
  `millis()` wrap.
- SETTINGS: the CURRENT SENSE row by control id, `NONE` and `INA219`,
  saved, reset; version 5; a version 4 block carried over with
  `currentSense` at `NONE` (decision 14); nothing sent to 0x40 with
  `NONE`. Built in `runSettingsTests`.
- The servo tester on the switched supply: the enable pin low at bind,
  high at START, low at STOP, at the trip and at every cut, released after
  the quiet wait; pulses only after 4.5 V; did not come on after 500 ms;
  not off after 2000 ms; the start-up time shown; START refused with the
  sensor declared and not found.
- The cuts at their boundaries: 2999 mA and 3000 mA; a clipped reading;
  stall at 399 ms and 400 ms above 1.0 A (decision 13); collapse likewise;
  sensor lost at 250 ms; a stuck bus and a silent touch controller each
  switching the supply off; KST and Hitec: the hard cut during a write,
  then the loss path.
- `servo_move` through the job: an MG90S-like move from `servo_sim`
  timed within one read and two cycles; UNSEEN with no current change.
- The log: rows per reading, the file closed at each end, a full queue
  counted.
- `tools/make_wiring.py --check`: J3's roles and part C from the sources.

### Bench tests

Only hardware settles these. Each names what it records.

- B1. The board's pull-ups on SDA and SCL, from the schematic and with an
  ohmmeter; the rise time of SDA and SCL at 400 kHz with the J3 lead and
  the module, on a scope; the longest lead that keeps 300 ns.
- B2. The INA219 found at 0x40 beside the touch controller; the time of
  one frame's I2C traffic on a scope; touch with and without the module.
- B3. Touch with J3 empty, with the module's VCC lead open, with SDA held
  low through 100 Ω to GND during a run (the supply goes off, the bus
  recovers); what `i2c_write_timeout_us()` returns while SDA is held, a
  lost arbitration, `PICO_ERROR_GENERIC`, or a timeout; IN- leakage with
  VS at 0 V and 5 V on IN+.
- B4. Current against a reference meter at 0 A, 0.1 A, 1 A, 2 A and 3 A
  through a resistive load: offset and gain error at /8; the code at
  3.5 A, beyond the range.
- B5. Bus voltage against a multimeter at 5 V, idle and at 2 A.
- B6. The converter: VOUT with EN low at the lowest and the highest VIN
  (the disconnect); the EN threshold, rising and falling, at 3.0 V and
  4.2 V VIN; EN from reset, in BOOTSEL and with the board off; the
  start-up time from EN high to 4.5 V on a scope and from the 1 ms poll;
  the decay after the switch-off with a servo and with none.
- B7. The converter at 1 A, 2 A and 3 A: VOUT, its current limit, its
  temperature after 1000 ms of stall.
- B8. The owner's servos: idle, moving and holding current at 5.0 V, and
  stall at an end stop; the shunt's temperature after a 1000 ms stall;
  whether the limit run of decision 13, above 1.0 A for 400 ms, trips at
  MIN and MAX against a stop, as the design expects on many servos.
- B9. The cuts: a servo held at its stop (stall at 400 ms); a 1.5 Ω load
  (3.3 A, hard cut); the time from the step to EN low on a scope.
- B10. A move timed by `servo_move` against a scope on SIG and the
  current, on an MG90S, beside rcbench's record of the same servo.
- B11. The heartbeat with the supply on: core0 halted in a debugger; the
  supply off within 250 ms.
- B12. A 60 s log with the supply on: no heartbeat trip, rows counted.
- B13. The voltage at the servo at a stall, behind the shunt and the leads.

## Slices

Sizes are estimates of new and changed lines, code and tests apart;
copies from rcbench are counted apart and are unchanged.

| Slice | What | Code | Tests | Copied | Depends on |
|---|---|---|---|---|---|
| 1 | The driver and the readout: `src/sense.{h,cpp}` with the INA219 codec, the core0 read after `touchPoll()`, the recovery, the sample for core1; `cst328.cpp` noting its transfers; the CURRENT SENSE row and settings version 5; the readout on the servo tester, KST and Hitec; `test/fake_i2c.*` | about 650 | about 750 | `sense_bus.{c,h}`, 617 lines | |
| 2 | The wiring sheet and the parts list: the J3 marker in the board header, `make_wiring.py` part C and J3's roles, `docs/wiring.svg`, README and wiki pages in both languages | about 150 | the `--check` | | 1 |
| 3 | The servo tester on the switched supply, decision 20: SAFE off, START on, STOP and the trip off, the read-back, the start-up time, the stuck-bus and silent-touch cuts, the screen texts | about 350 | about 500 | | 1 |
| 4 | Protection: `src/supply_guard.*`, the cuts in the servo job, the hard and collapse cuts in the KST and Hitec jobs, the chips and texts | about 300 | about 550 | | 3 |
| 5 | Move timing: the move measurement in the servo job, the SET line | about 200 | about 250 | `servo_move.{c,h}`, `servo_sim.{c,h}`, `test_servo_move.c`, 1,535 lines | 3 |
| 6 | The log: `LOGS/SRVnnnnn.CSV`, the write cache on for a SERVO claim with a log | about 300 | about 350 | | 3 |

Each slice updates this note's status line, its row in
[README.md](README.md), [STATUS.md](../../STATUS.md) and the README
sections it changes. Slice 3 marks servo-tester.md decision 20 and
kst-programming.md decision 10 built, and replaces kst-programming.md's
INA226 line under "Not in this section" with a link here. Slices 1 to 3
make the hardware usable; 4 to 6 can merge in any order after 3. Bring-up,
bench tests B1 to B13, follows slice 3 and is no slice of its own.

### Slice 2 as built

- **The marker.** The connector table in
  `src/board_rp2350_touch_lcd_2_8.h` marks J3's GP7 and GP6 with `^`, the
  sensor's connector, and J4's with `*`, "do not use". `role()` in
  `tools/make_wiring.py` takes the connector: on J3, `SCL` and `SDA`
  "INA219 SCL, 0x40" and "INA219 SDA, 0x40", 3V3 "3.3 V out: INA219 VCC",
  GND "ground: INA219 GND only"; J4 pins 7 and 8 "shared I2C: do not use".
- **The script's checks.** `sensor()` stops the script when `^` is on no
  connector or on more than one; when that connector carries anything but
  `PIN_I2C_SCL`, `PIN_I2C_SDA`, 3V3 and GND; when a pin on SCL or SDA
  elsewhere lacks `*`; when a marker sits on a pin off the bus; and when
  `INA219_ADDR` is a reserved address or one of the board header's
  `*_I2C_ADDR`. `define()` reads hex values with a `u` suffix.
- **Part C.** The converter box reads `BOOST`, `5 V out`, `output
  disconnect`, and its input box `one LiPo cell of its own, 3.0 V to
  4.2 V`. The INA219 box, dashed as optional, sits on the output wire:
  VIN+ from VOUT, VIN- to the servo's plus, its address from `INA219_ADDR`,
  its shunt from `INA219_SHUNT_MOHM`. A J3 stub to its right carries the
  four wires to the module's header, and the servo's minus comes from the
  ground rail round the stub, at the servo's right, so no wire crosses
  another. The caption under the converter reads `Needed: 5 V, at least
  3 A, output disconnect, EN high at 3.3 V`: requirements, not ratings,
  since no module is named ([Q1](#q1-the-converter-module)). The notes
  give the disconnect, a fuse at the cell, the cell charged off the bench,
  the pull-down rule on one cell (at most 0.135 times a pull-up at 4.2 V),
  the pigtail from J3, the screw terminal, and VOUT straight to the servo
  without the sensor.
- **Differing from the list above.** The panel title does not name the
  servo tester, and the note "The servo tester does not switch it" stays:
  the servo tester drives no enable pin until slice 3 (`src/ui_servo.cpp`,
  the supply line of its strip), so a title naming it would show a wiring
  that leaves its servo unpowered. Slice 3 changes both. The Setup row for
  the converter names KST and Hitec programming for the same reason.
  With slice 3 the panel title names the servo tester, the note says when
  EN is high in each tool, and the Setup rows name the servo tester.
- **Documents.** README.md: the pin sketch with `^`, the wiring table's
  part C, the script's checks, "Switched supply (optional)" with the
  boost, the cell, the sensor and the unnamed module, and a pointer from
  "Current sensor" to part C. Wiki: Wiring and Verdrahtung (the connector
  row, part C, the converter, the pull-down on one cell), Setup and
  Einrichtung (the boost converter, the INA219 and the J3 cable; the Hitec
  classic row names the boost).
- **Tests.** `python3 tools/make_wiring.py --check`, in the Docs CI job,
  fails when `docs/wiring.svg` differs from the drawing, and when
  `sensor()` takes one of the nine changed sources of `SENSOR_MUTANTS`:
  the marker on no connector and on two, GP0 in place of J3's GND, an unmarked
  bus pin on J4, a marker off the bus, another SCL pin, and the address at
  0x1A, 0x78 and 0x07. Nothing of it is on hardware: no INA219 and no boost
  converter has been wired to the board.

### Slice 3 as built

The servo tester on the switched supply, decision 20 of
[servo-tester.md](servo-tester.md#decisions) and decision 12 above. With
`SERVO SUPPLY` at `OWN` nothing of it applies: the job binds no enable pin,
GP28 and GP29 keep the state they had, and START, STOP and the trip act as
before.

- **The rules.** `servoPowerStep()` in `src/servo_job.cpp`, pure, after
  `servoCoreStep()` on every pass; `ServoPower` holds the phase, Off,
  Starting or On, the latched fault, the read-back and the start-up time.
  The job drives the enable pin from the phase (`drivePower()`) before it
  writes the pulses, so at a switch-off EN goes low in the same pass as
  level 0 on the pins.
- **Every path off.** Not RUNNING is Off: STOP on SERVO and on SET, the
  heartbeat trip, the cuts below, a refused rebind, which leaves the job
  SAFE. The release switches off at its first pass, waits for the last
  pulse, then makes the enable pin an undriven input (`servoSupplyRelease()`)
  before it lets SIG and AUX go. A bind the PWM refuses never binds the
  enable pin, and core1_job.cpp releases no job it did not bind. Boot and
  reset are unchanged: GP28 and GP29 come up with their pull-down on and
  their input off (kst-programming.md, "Off state"), and
  `servoSupplyInit()` makes the configured pin an undriven input; the
  converter's pull-down holds EN low until the job's bind.
- **On.** START switches on in the pass that takes it. The pulses stay at
  level 0, both outputs held at CENTRE and a sweep ended, until the supply
  is up: with a sensor, the first fresh reading taken more than
  `SUPPLY_READ_LAG_MS`, 18 ms, after the switch-on at or above
  `SUPPLY_ON_MIN_MV`, 4500 mV, whose age at the
  switch-on is the start-up time (`ServoStatus::upMs`); without one,
  `SUPPLY_START_MAX_MS`, 500 ms, after the switch-on. With a sensor and no
  such reading by 500 ms the pulses end, the supply goes off and the fault
  is NoStart.
- **The cuts of this slice.** While RUNNING, the touch controller's last
  answer 250 ms old or older (`touchAnsweredMs()`, judged by `escUiAlive()`,
  the heartbeat's rule) or the sample's state `Stuck` end the pulses, latch
  the trip and record the cause, TouchSilent or BusStuck. Both refuse START
  while they last, and so does CURRENT SENSE at INA219 with the part not
  online: absent, another part, not probed yet, or offline
  (`servoPowerStartBlock()`).
- **The read-back after a switch-off,** with a sensor: Off at the first
  fresh reading taken more than 18 ms after the switch-off at or below
  `SUPPLY_OFF_MAX_MV`, 1000 mV; NotOff at a reading above
  `SUPPLY_NOT_OFF_MV`, 2500 mV, taken `SUPPLY_NOT_OFF_MS`, 2000 ms, or more
  after it, until a reading at or below 1000 mV. A warning only. The
  read-back ends with the claim: after BACK nothing reads it.
- **The touch stamp.** `touchAnsweredMs()` in `src/touch.h`: `cst328.cpp`
  stamps `millis()` at a presence check that reads CACAh and at every finger
  read that completes, a volatile word core0 writes and core1 reads, as
  `escHeartbeatMs()`.
- **The screen.** The strip's second line, amber, names the phase:
  `GP28 SUPPLY ON AT START`, `GP28 SUPPLY COMING UP`, `GP28 SUPPLY ON`,
  `GP28 SUPPLY NOT OFF`; each at most 28 characters, 4 px clear of the
  widest chip. While SAFE the dial band's two notes read, by priority:
  the trip's cause in magenta over `HOLD TO START AGAIN.`
  (`STOPPED: SCREEN STALLED`, `STOPPED: TOUCH DID NOT ANSWER`,
  `STOPPED: TOUCH BUS STUCK`); `SUPPLY NOT UP: NO 4.5 V IN 500 MS.` over
  `CHECK THE CONVERTER ON GP28.`; `START WAITS FOR THE CURRENT SENSOR.` over
  `NONE IN CURRENT SENSE SKIPS IT.`, with HOLD TO START drawn disabled; else
  `PULSES ON GP0 ARE THROTTLE TO AN ESC,` over
  `AND ITS BEC WIRE GETS THE SUPPLY.` in amber. While the supply comes up
  MIN, CENTRE, MAX, SWEEP and the dial are disabled and STOP acts. SET's
  note band shows `SUPPLY UP IN 25 MS`, dim, on its second line when that
  line is free, from the last switch-on until START or a new visit.
- **Differing from the design above.** The dial band has room for two
  lines above the arc, not three: `THE BENCH POWERS THE SERVO AT START.`
  is said by the strip's `GP28 SUPPLY ON AT START`, the throttle warning and
  the KST search's BEC warning share the two lines, and
  `THE SERVO MOVES TO CENTRE AT START.` is left out with the switch.
  [menu-b.md](menu-b.md#decisions) decision 12's `GP28 SUPPLY STAYS OFF
  HERE.` is replaced by the phase, as the line is false with the switch; that
  decision carries a revision note. The start-up time does not go into a log; the
  log is slice 6. The sensor-lost cut and its `NO SENSOR` chip are slice 4's
  ([slice 4 as built](#slice-4-as-built)).
- **Choices the note does not settle.** A supply that did not come up is no
  trip: the chip reads SAFE, and the note stays until START or a new visit.
  The touch and bus cuts apply with the switch only, as the table of
  [safety and interlocks](#safety-and-interlocks) is headed; with `OWN`
  neither stops the pulses. `SenseSample::takenMs` is when core0 read the
  result, not when the INA219 measured it: the bus sample of a reading is
  up to one conversion cycle, 17.55 ms, older than the read. A reading
  therefore counts for the read-back only when taken more than
  `SUPPLY_READ_LAG_MS`, 18 ms, after the switch command, so the output's
  voltage from before the command never ends a start or a switch-off.
  Unlike KST's close rule there is no 2000 ms off wait before DShot: the
  table above gives BACK the quiet wait alone.
- **Tests.** In `test/test_servo.cpp`: the rules alone
  (`testPowerOwn()`, `testPowerStart()`, `testPowerNoStart()`,
  `testPowerOff()`, `testPowerStartBlock()`, `testPowerReadBack()`), each
  boundary at 499 ms and 500 ms, 4499 mV and 4500 mV, 1000 mV and 1001 mV,
  2500 mV and 2501 mV, 1999 ms and 2000 ms, a reading taken 18 ms and 19 ms
  after the switch command, a stale reading, a wrap; the job
  through the job table on the fake pads (`testJobSupply()`,
  `testJobSupplySensed()`): GP28 or GP29 low at the bind and never an output
  while high, high at START, low at STOP, at STOP while starting, at the
  heartbeat trip at 250 ms and not at 249 ms, at a silent touch controller
  at 250 ms, at a stuck bus from SDA held, at BACK's first pass and at BACK
  while the supply comes up, an undriven input after the quiet wait and
  after DShot's restart; the order of the writes from the pin fake's
  stamps: EN low before level 0 at STOP, the enable pin an input before SIG
  leaves PWM; undriven after a
  refused bind, low across a profile rebind; GP28 and GP29 untouched with
  `OWN`; the screens (`testSupplyLineAndHint()`, `testSupplyScreenFaults()`)
  with the fake INA219 following the enable pin, STOP replaced by HOLD TO
  START at a cut while the supply comes up, and the notes of a START refused
  by a stuck bus or a silent touch controller. In `test/test_sense.cpp`
  the touch stamp. 83 mutants of `src/servo_job.cpp`, `src/ui_servo.cpp`
  and `src/cst328.cpp`: 79 fail the suite, 4 are equivalent: the clamp of
  the start-up time, reached only after a core1 stall of 65 s; the status's
  start-up time before the first bind, which no screen shows unswitched;
  `upNote()`'s switch test, since the time is none whenever the switch is
  off but on the leave page; the dial's key of the notes' texts, whose every
  reachable change also changes RUNNING, the trip or a colour. Nothing of it
  is on hardware: the
  enable pin's reset state and BOOTSEL, the start-up time and the decay are
  bench test B6, the stuck bus B3, the trip with the supply on B11.

### Slice 4 as built

The cuts of [protection](#protection) and the sensor-lost cut of
[safety and interlocks](#safety-and-interlocks), with CURRENT SENSE at
`INA219`. With `NONE` nothing of it applies.

- **The rules.** `supplyGuardStep()` in `src/supply_guard.cpp`, pure, takes
  each pass's `SenseSample` and the time and returns the cut. It takes a
  reading once: one whose `seq` it has not seen and that `senseFresh()`
  calls fresh; the sample current at the arm (`supplyGuardArm()`) never
  counts. The hard cut is a reading at or above `GUARD_HARD_MA`, 3000 mA,
  either sign, or one with `SENSE_FLAG_CLIPPED`. The stall cut is a reading
  above `GUARD_STALL_MA`, 1000 mA, either sign, taken `GUARD_STALL_MS`,
  400 ms, or more after the first of a run of such readings. The collapse
  cut is a reading under `GUARD_LOW_MV`, 4000 mV, taken `GUARD_LOW_MS`,
  1000 ms, or more after the first of such a run, counted from the start-up
  reading on: the first fresh reading taken more than `SUPPLY_READ_LAG_MS`,
  18 ms, after the arm at or above `SUPPLY_ON_MIN_MV`, 4500 mV, the
  read-back's rule (`supplyReadingAfter()`; both constants are in
  `src/supply_guard.h`, which `src/servo_job.h` includes). A reading at or below
  the stall's threshold ends that run, one at or above 4000 mV the collapse's.
  The sensor-lost cut is a state but `Online`, or `SENSE_STALE_MS`, 250 ms,
  since the later of the arm and the newest reading's time. In one pass:
  lost, then hard, stall and collapse.
- **The servo tester.** `servoPowerStep()` arms the guard at the first pass
  RUNNING with a sensor declared, the switch-on's pass with the switch, and
  disarms it at the first pass not RUNNING. With the switch, a silent touch
  controller and a stuck bus come first, then every rule of the guard; with
  `OWN`, the hard and the stall cut only. A cut ends the pulses and the
  sweep, latches the trip and records `ServoPowerFault::Overcurrent`,
  `Stall`, `LowSupply` or `NoSensor`; with the switch the supply goes off in
  the same pass, and `drivePower()` takes the enable pin low before
  `output()` writes level 0, as at STOP. START clears the fault, a rebind
  keeps it, with `OWN` too, and a new screen session drops it.
- **The KST and Hitec jobs.** `supplySet()` arms the guard at each
  switch-on and disarms it at each switch-off; `guardPass()` runs first in
  every pass and in each pass of the release, before a command is taken,
  with the hard and the collapse rule. A cut switches the supply off in that
  pass, latches `KstStatus::cut` or `HitecStatus::cut`, and refuses every
  switch-on until a new search or entry (RETRY) or a new bind. KST:
  `kst_session_power_cycled()`, as a lapse; a search is stopped
  (`kstSeekStop()`); a search or an operation finishes the frame on the
  wire, since `kst_session_abort()` never cuts one and the line driver has
  no cut; then SearchEnded, at once when idle in the mode or in REBOOT's on
  wait. The stall's flags of the screen are kept as at a lapse
  (`keepStallFlags()`). Hitec: `hitecLineAbort()` cuts the transaction on
  the line, a write's too and the write a leave lets finish; once the line
  driver hands it back, in the same pass with the host fake, the session is
  initialised again and the job is EntryEnded. The close rule counts from the
  cut's switch-off.
- **The screens.** SERVO TESTER: the chip `OVERCURRENT`, `STALL`,
  `LOW SUPPLY` or `NO SENSOR` in `C_MAGENTA` in place of `STALLED`, and over
  `HOLD TO START AGAIN.` in the dial band, magenta,
  `STOPPED: CURRENT 3.0 A OR MORE`, `STOPPED: OVER 1.0 A FOR 400 MS`,
  `STOPPED: UNDER 4.0 V FOR 1000 MS` or `CURRENT SENSOR LOST: SUPPLY OFF.`;
  after any trip, `START WAITS FOR THE CURRENT SENSOR.` while the part is
  not online. KST SERVO and HITEC SERVO, on the view with RETRY: the strip
  `SIG GP0  SUPPLY OFF`, then in amber, wrapped at 37 characters,
  `THE SERVO DREW 3.0 A OR MORE, SO THE BENCH SWITCHED ITS SUPPLY OFF.` or
  `THE SUPPLY WAS UNDER 4.0 V FOR 1 S, SO THE BENCH SWITCHED IT OFF.`, and
  `RETRY POWERS IT UP AND READS IT AGAIN.`; KST adds the stalled screen's
  note of a write in doubt.
- **Choices the note does not settle.**
  - Either sign: the cuts read the current's size, so a reversed sensor
    still cuts.
  - With `OWN` the servo tester applies the hard and the stall cut: the
    bench knows neither the user's supply voltage nor whether its current
    runs through the sensor, and START is not refused for a missing sensor
    there. The KST and Hitec claims apply the cuts with the switch only:
    with `OWN` the bench cannot take the current away, and a cut write
    would leave the servo in doubt for nothing.
  - The collapse rule applies in the KST and Hitec claims, during an
    operation too, as the protection section proposes. A supply that never
    reaches 4.5 V arms no collapse rule; in the servo tester it is NoStart at
    500 ms.
  - The hard and the stall cut apply while the supply comes up too.
  - A run of the stall or the collapse rule starts again at a reading taken
    250 ms or more after the one before it: that reading had gone stale, so
    the time between is not known to be above or below the threshold.
  - The sensor-lost cut counts from the arm at the earliest, so a gap
    before START cuts nothing at START, and with a signed difference, since
    core0 can take a reading in the millisecond after core1 read its clock.
    `senseFresh()` calls such a reading stale until the next pass, so it is
    taken one pass late, never lost.
  - The KST and Hitec texts are amber, as their stall texts; the servo
    tester's chips are magenta, as the note gives.
  - Beside a chip wider than RUNNING, `OVERCURRENT` (from x 155 px),
    `LOW SUPPLY` (x 161 px) or `NO SENSOR` (x 167 px), the caption ends 4 px
    before the chip: by its role alone, `SIG  50 HZ  1000-2000 US`, and at
    333 Hz with one servo the travel goes too, `SIG  333 HZ`, decision 14's rule
    of [menu-b.md](menu-b.md#decisions). At SAFE, RUNNING and STALLED the
    caption is menu-b.md's. The strip's second line, at most
    `GP29 SUPPLY NOT OFF` or `GP28 SUPPLY ON AT START` after a cut, ends by
    x 145 px, clear of the chip.
  - With `OWN`, while RUNNING with the declared sensor not online, the dial
    band's second line reads `NO SENSOR READING: NO CURRENT CUTS.`, dim:
    the hard and the stall cut get no reading, and `OWN` has no
    sensor-lost cut. The KST and Hitec claims have no sensor-lost cut
    either; there the strip's current reads `-- A`, and the wiki says that
    the cuts then do not act.
  - The chip's redraw key holds the fault: START and a new cut can both
    fall between two screen frames, so the screen sees one trip follow
    another without RUNNING between them.
- **The Hitec job's stack.** CI's "Hitec job stack on core1" reads the call
  graphs of `src/sense.cpp` and `src/supply_guard.cpp` too, 11 files.
  The deepest chain from the job's three entry points is 1184 bytes of the
  1792 allowed, in the strict build of this slice.
- **Tests.** `test/test_supply_guard.cpp` (`runSupplyGuardTests`): each
  rule at its boundary, 2999.999 and 3000 mA of either sign, a clipped
  reading, 1000 and 1000.1 mA, 399 and 400 ms, 4499 and 4500 mV and 18 and
  19 ms for the start-up reading, 3999 and 4000 mV, 999 and 1000 ms, 249 and
  250 ms without a reading, a gap of 249 and 250 ms, a stale reading, a
  reading seen twice, the reading at the arm, a reading from the next
  millisecond, the clock's wrap. `test_servo.cpp`: the cuts in
  `servoPowerStep()` with the switch and with `OWN`, the order behind touch
  and the stuck bus; the job through the job table on the fake INA219, the
  enable pin low and SIG at level 0 in the pass that reads 3.0 A and in that
  order, 2.99 A, 3.6 A clipped, -3.6 A, the stall at the 17th reading of
  1.2 A and none at 1.0 A, the collapse at the 41st reading of 3.9 V and none
  at 4.0 V, a failed read and two failed reads in a row without a cut, no
  reading for 249 and 250 ms, the part unplugged, `OWN`; the screen's chips,
  notes and caption. `test_kst_job.cpp` and `test_hitec_job.cpp`: the hard
  cut during a read, a write, a search's entry, REBOOT's on wait, a leave's
  write and the release, the collapse at 1000 ms, 2.9 A for 1 s without a
  cut, no switch-on until RETRY, the close 2000 ms after the cut, nothing
  with `NONE`; on Hitec the transaction on the line cut.
  `test_kst_ui.cpp` and `test_hitec_ui.cpp`: both texts and RETRY. KST with
  `fakeKstFrameHold()`, which holds a frame on the wire: a cut while an
  entry frame is out, alone and with BACK in the same pass, the supply never
  on again; Hitec with `fakeHitecCutDelay()`, a cut transaction handed back
  late. The servo screen: START and a cut between two frames, the chip
  `OVERCURRENT` after `STALL`; the dim note with `OWN` and the sensor gone.
  Mutation check: 137 mutants of `src/supply_guard.cpp`,
  `src/servo_job.cpp`, `src/ui_servo.cpp`, `src/kst_job.cpp`,
  `src/hitec_job.cpp`, `src/ui_kst.cpp` and `src/ui_hitec.cpp` as they
  stand; 130 fail the suite and 7 do not. Equivalent: `>=` for `>` where
  the sensor-lost rule takes a reading's time as its reference, since a
  reading in the arm's millisecond gives the arm's time either way; `-ua`
  for `-(int64_t)ua` in the current's size, which differs only at
  `INT32_MIN`, undefined in C++ and the same value with GCC; Hitec's refusal
  of a switch-on while a cut is latched, since only an entry's POWER step
  switches on, and Enter clears the latch first. Not killed: KST's leave not
  taking over a cut's drain, which differs only after a cut in REBOOT's
  entry, whose frame ends long before the close 2000 ms later; Hitec's leave
  stepping the session before it polls a cut transaction back, which the
  session ends as cut either way in the fake; KST's note of a write in doubt
  after a cut, which no screen test reaches; the servo screen's `OWN` note
  shown with the switch too, which differs for at most one frame, since the
  sensor-lost cut ends RUNNING in the pass the part leaves `Online`. KST's
  refusal of a switch-on while a cut is latched is not equivalent: a leave
  that drains a cut search steps it, and `syncSeek()` asks for the supply
  the stopped search still wants; the test with `fakeKstFrameHold()` kills
  it. Five mutants of the first run hit code that is not in the files, the
  power cycle of a cut search and four paths of the Hitec cut in a leave;
  they are not among the 137.
- **Not on hardware.** No INA219 and no converter has been on the board:
  the time from a step of current to EN low, about 60 ms computed, is bench
  test B9; whether the stall cut trips at MIN and MAX on the owner's servos
  is B8; the voltage at a stall, B13. Whether a Hitec servo takes a cut
  transaction without harm, and how long the KST line driver's last frame
  runs into an unpowered servo, are not measured.

### Slice 5 as built

The move timing of [rcbench's tests on current
only](#rcbenchs-tests-on-current-only), "Move timing in the servo tester",
with CURRENT SENSE at `INA219`. With `NONE` nothing of it applies: no step is
timed and SET shows what it showed before.

- **The copies.** rcbench's `servo_move.{c,h}` and `servo_sim.{c,h}` and
  its test `test_servo_move.c`, 18 cases, at rcbench commit 5ef42efd,
  unchanged, in `lib/rcbench/shared/servo/` and `test/rcbench/`, listed in
  `lib/rcbench/README.md`. `servo_move.c` is built in the firmware
  (`src/servo.cmake`); `servo_sim.c` in the host suite only.
- **The rules.** `moveTimerStep()` in `src/move_timer.cpp`, pure, runs in
  every pass of the servo job after `servoPowerStep()`, so it sees the
  pulses every rule has decided; it reads the pass's `SenseSample` and the
  pulses, and writes only its own state, published as `ServoStatus::move`.
  A step is a Position command with `ServoCmd::step`, which MIN, CENTRE and
  MAX set on SERVO TESTER (`servoStepCmd()`). It is timed when it changes
  the pulses at SPEED 100 %, no move is under way, and the servo has been at
  rest for `MOVE_REST_MS`, 500 ms: at rest from the reading that showed the
  last timed move's arrival, from the end of its window, the command plus
  3061 ms, when it went late or unseen, and otherwise
  `servo_move_window_ms(MOVE_LAG_MS)`, 3061 ms, after any change of the
  pulses: the pulses' start at START, a drag, a
  sweep, a step at a lower SPEED, a step not timed. The level before is the
  mean of the readings taken in the 500 ms before the command, at least
  `MOVE_MIN_READINGS`, 10, of them, kept in a ring of 32; the threshold is
  `servo_move_threshold_a()` of their standard deviation. The move is
  rcbench's: `servo_move_begin()` with the command at the pass that takes
  it, `filter_n` 1, `settle_n` 2, `SERVO_MOVE_BAND_A`, and a window of
  `servo_move_window_ms(MOVE_LAG_MS)`, `MOVE_LAG_MS` 61 ms, one read of
  25 ms and two cycles of 17.55 ms rounded up: a conversion that straddles
  the arrival averages moving and holding current, the first one wholly
  after it ends up to 17.55 and 17.02 ms later, and core0 reads it up to
  25 ms after that; each new fresh reading of an
  online sensor goes to `servo_move_sample()` at its `takenMs`, clipped
  readings as `SERVO_MOVE_CLIP_HIGH` or `SERVO_MOVE_CLIP_LOW` at the
  range's end.
- **The holding level.** Each timed step stores its level before as the
  holding level of the pulses it leaves, keyed by SIG's and AUX's pulse, in
  9 slots, the oldest replaced. The destination's holding level is the one
  stored for its pulses this run; while none is, the idle level, the level
  before the last timed step that left the pulses the run started with,
  CENTRE; while that is not known either, the level before. START forgets
  them.
- **The figures.** MOVE is `end_t` less the command: the reading back at
  the level, or the first of the two of the settled rule. PEAK is
  `servo_move_t::peak` when the move has valued readings, none clipped,
  and is 0 mA or more. HOLD is the mean of the readings taken in the
  `MOVE_HOLD_MS`, 500 ms, after the reading that ended the move, at least
  10 of them, none clipped, 0 mA or more; it is closed by the first reading
  500 ms or more after. A late move has a PEAK and no HOLD; an unseen one
  neither.
- **Void.** A step not at rest, with fewer than 10 readings, at a lower
  SPEED, or with a level before below -20 mA (`MOVE_REVERSED_MA`, rcbench's
  `SERVO_TEST_NEG_IDLE_A`, a reversed sensor) is not timed. A move under way
  is void when the pulses end (STOP, a trip, a cut, BACK's first pass), when
  another command changes them, and when the sensor leaves `Online` or no
  reading comes for `SENSE_STALE_MS`, 250 ms, counted from the command at
  the earliest; a hold under way is then not shown. A bind voids a move
  under way; a profile's rebind and a new screen session drop the result.
  With CURRENT SENSE at `NONE` no step is timed, a move or hold under way
  ends, and the result stays as it is.
- **The screen.** SERVO SET's note band, second line, in `C_DIM`, when the
  line is free (`moveNote()`): `MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`,
  `MOVE LATE  ...`, `MOVE UNSEEN  ...`, and `--` for each figure not shown,
  so a step not timed or void reads `MOVE -- S  PEAK -- A  HOLD -- A`
  (`moveTimerText()`). The time and the currents are rounded to 10 ms and
  10 mA. At most 37 characters, 221 px at scale 1 from x 8, ending at x 228;
  a `static_assert` in `src/ui_servo.cpp` holds it inside the 8 px margin.
  Before the first step since the pulses started the line shows the
  start-up time, `SUPPLY UP IN 25 MS`; a two-line note takes the line from
  both. The result stays while SAFE, and goes at the pulses' start, at a
  profile's rebind and with a new screen session.
- **Choices the note does not settle.**
  - "The SET screen's last line" is the note band's second line: SET has no
    other free line, and menu-b.md's layout table fixes the rest.
  - The holding level "of that end's last hold" is the level before the
    last timed step that left the position: the readings of the 500 ms
    before that step, taken while the servo held there. The HOLD shown is a
    separate figure, the 500 ms after the arrival. "The idle level" is
    CENTRE's, as rcbench's idle is the servo at rest at its centre.
  - A first move to an end where the servo pushes on a stop, harder than at
    CENTRE, is judged against CENTRE's level and reads LATE; rcbench's
    automatic test places both ends first for this reason. The next move to
    that end is timed.
  - The rest after an untimed change is the window, 3061 ms: the bench
    cannot see when an untimed move ends, and servo_move times no move
    longer than that.
  - The time is when the current shows the arrival, not when the horn
    stops: a servo that eases its drive near the target can fall to its
    holding current while the horn still creeps.
  - A move ends only at a reading, never by the clock: core0 can take a
    reading in the millisecond after core1 read its clock, and a tick at
    that time could end a move whose arriving reading is still to come.
    Without readings the 250 ms rule ends it.
  - Two servos: the position is the pair of pulses, and the current their
    sum, so a step of A with B held is timed from the total.
  - A negative PEAK or HOLD reads `--`: it would be a reversed sensor or an
    offset, and `-0.05 A` would not fit the line's 37 characters.
- **Tests.** `test/test_move_timer.cpp` (`runMoveTimerTests`): the rest at
  3560 and 3561 ms after a change, a step during a move, a step at a lower
  SPEED; 9 and 10 readings, a reading 500 and 501 ms old; -21 and -20 mA;
  the threshold from the noise; the ring's newest 32; AUX's pulse; arrival,
  the peak, the hold closing at the reading 500 ms after the arrival, 9 and
  10 hold readings, a clipped reading and a negative hold; a first visit to a
  pushed end late, the level of the step that left it, the settled rule;
  the idle level, the levels' slots and their eviction, START forgetting
  them; late and unseen at 3060 and 3061 ms; void by a change, STOP, the
  sensor offline, 249 and 250 ms without a reading from the last reading and
  from the command; CURRENT SENSE at `NONE`; clipped readings high and low;
  a negative peak; readings seen twice, stale, not online, and taken 8 ms
  before core1's pass, which count at their time taken; a bind; the line's
  texts, rounding and width.
  `test/test_servo.cpp`: `testJobMoveTiming()` through the job table on the
  fake INA219 on its conversion clock, each reading the mean of rcbench's
  servo model over the 17.02 ms shunt conversion, 0.95 A moving at 1.2 µs a
  ms and 0.12 A at rest, driven by SIG's pulse from the core1 pass that
  changes it (the PWM frame's wait is not modelled): a step 475 ms after
  START not timed, CENTRE from MAX and MIN from CENTRE timed between 433 ms
  and 478 ms, 16 ms to `MOVE_LAG_MS` after the model's 417 ms, since the
  arrival reads near the holding level only in a conversion of at most 6 %
  moving current (450 ms in the run), the peak and the hold within the
  model's noise, a step at a lower SPEED, unseen at the first reading
  3061 ms or more after the command, the stall cut at its 17th reading of
  1.2 A during a timed move, as without one, START dropping the result, the
  release, a step of AUX alone, a profile's rebind dropping the result;
  `testMoveScreen()`, on the same model: SET's line through the screen,
  the start-up time before the first step, the line while running and kept
  while SAFE, gone at a profile's rebind, under a two-line note, gone at
  START, dashes for a step too soon; the
  frame `shot_servo_set_move.ppm`. rcbench's `test_servo_move.c` runs as a
  C11 program. Mutation check: 101 mutants of `src/move_timer.cpp`, the
  timing's lines of `src/servo_job.cpp` and of `src/ui_servo.cpp`, among
  them each use of a reading's `takenMs` replaced by the pass's time; 99
  fail the suite, 2 do not, both equivalent: `< 0` for `<= 0` where the hold
  skips a reading taken in the arrival's millisecond, which core0's one
  read a frame never gives; `>= 0` for `> 0` where the stale rule takes the
  later of the command and the newest reading, the same time either way
  when they are equal.
- **Not on hardware.** No INA219 and no converter has been on the board:
  how far the time lies from the horn's arrival, against a scope on SIG and
  on the current beside rcbench's record of an MG90S, is bench test B10;
  the owner's servos' moving and holding currents, and whether a hold at an
  end reads above its threshold, B8. `MOVE_MIN_READINGS`, the 500 ms of rest
  and hold and the 20 mA floor and 50 mA band are rcbench's or this slice's
  choices, not measured on this meter.

## Open questions

Q2 to Q5 and the input of Q1 are answered by the owner's decisions of
2026-11-14 (decisions 10 to 14). One point is open.

### Q1. The converter module

The owner chose the input and the topology: one LiPo cell, 3.0 V to 4.2 V,
into a synchronous boost with load disconnect, 5.0 V, at least 3 A out
([decision 10](#decisions)). The module is not named. The owner's
mechanical model
[boost_5v_en.step](../../hardware/mechanical/models/boost_5v_en.step) is a
5 V boost board, 13.7 × 21.7 mm, with one pad besides its 4 corner pads,
drawn as its enable. Its chip is not known: the owner does not know it
(2026-11-15), and the board's print names none. So its input range, its
output current, its current limit, whether it disconnects the load with EN
low, and its EN threshold are unknown. Whether that board is the module of
decision 10 is open. Whichever module is used is checked on the bench
before it is relied on: B6, its
disconnect with EN low at the lowest and the highest VIN, its EN from
reset, in BOOTSEL and with the board off, its start-up time and its decay;
B7, VOUT, its current limit and its temperature at 1 A, 2 A and 3 A. Its EN
must also meet the rule of
[enable and its pull-down](#enable-and-its-pull-down).

## Decisions

Decisions of the owner, 2026-11-13:

1. **The sensor.** An INA219 is added, on J3, the shared I2C1 bus.
2. **What it measures.** The servo supply, after the boost converter: high
   side, on the converter's output to the servo.
3. **The converter.** One boost converter for the whole bench, its enable
   switched by a GPIO, GP28 or GP29 as today. It replaces the converter of
   wiring part C.

Choices made in this design; the owner has not decided them:

4. **Core0 reads the sensor,** in `uiTick()` after `touchPoll()`. One core
   owns I2C1 and no lock is needed; the price is 40 readings a second.
5. **A SETTINGS row, not detection alone,** so that a declared sensor that
   is missing is a fault and not a silent loss of the cuts. Settings
   version 5.
6. **One configuration for every use,** 19EFh: /8, 16 V, bus 12 bit,
   shunt 32 samples, continuous. No calibration; current is computed from
   the shunt code.
7. **A stuck bus and a silent touch controller switch the supply off,**
   since touch is the only way to STOP.
8. **J3 is the sensor's connector,** and J4 pins 7 and 8 stay unused.
9. **The servo current through the screw terminal only;** the module's
   header VIN pins stay unconnected, away from SDA.

Decisions of the owner, 2026-11-14, answering Q1 to Q5:

10. **The converter's input** (Q1). One LiPo cell, 3.0 V to 4.2 V, into a
    synchronous boost with load disconnect, 5.0 V, at least 3 A out. The
    module is not named yet: [Q1](#q1-the-converter-module).
11. **The shunt** (Q2). 0.1 Ω as sold; `INA219_SHUNT_MOHM` stays 100.
12. **The servo tester switches the supply** (Q3). Always, with `SERVO
    SUPPLY` at a pin, `GP28` or `GP29`, as decision 20 of
    [servo-tester.md](servo-tester.md#decisions) reads.
13. **Protection on the servo tester** (Q4). rcbench's limit run: readings above
    1.0 A for 400 ms, and a hard cut at 3.0 A (`SERVO_LIMRUN_STALL_MA` 1000,
    `SERVO_LIMRUN_STALL_MS` 400 and `SERVO_LIMRUN_HARD_MA` 3000 in rcbench's
    `servo_limrun.h` at 5ef42efd). The cut on a clipped reading is this note's
    addition, as in its hard-cut row. In the KST and Hitec claims the hard cut
    acts at once, during an operation too; the collapse rule's place there is
    this note's choice, not decided by the owner. This is not the
    recommendation, which was the automatic test's stall rule, 2.0 A for
    1000 ms: the limit run trips on many servos that reach MIN or MAX
    against a stop. That is a known consequence, checked on the bench in B8.
    The [protection](#protection) section carries it, built in slice 4.
14. **Settings carried over** (Q5). The firmware carries the settings over
    from the older layouts it knows, versions 2, 3 and 4, into version 5,
    with the new fields at their defaults. A block of a version it does not
    know is discarded and announced as now (`settingsResetByUpdate()`).
    This is the first migration in `settingsLoad()`. Built: each older
    layout is a table of byte offsets in `src/settings.cpp`, recovered from
    `src/settings.h` at 8a5eebb (version 2), 5af4ddf (3) and 3cc8798 (4);
    version 3's SERVO SUPPLY of 1, an enable on GP0, loads as `NONE`. Flash
    keeps the old block until a save lands. Host-tested, not tested on
    hardware.
