# MiniRcBench

[![CI](https://github.com/subtilitas/MiniRcBench/actions/workflows/ci.yml/badge.svg)](https://github.com/subtilitas/MiniRcBench/actions/workflows/ci.yml)

A bench tester for RC (radio-controlled) model parts. It runs on the
**Waveshare RP2350-Touch-LCD-2.8** and needs no flight controller, laptop or
Betaflight. MiniRcBench continues the DshotDisplay firmware, which remains at
[github.com/subtilitas/DshotDisplay](https://github.com/subtilitas/DshotDisplay)
as a separate project.

Today it is a bidirectional DShot tester for one electronic speed controller
(ESC). The board sends bidirectional DShot to the ESC from an on-screen
throttle. On the same wire it decodes the electrical RPM (eRPM) and Extended
DShot Telemetry (EDT) that the ESC returns: RPM (revolutions per minute),
voltage, current, ESC temperature, stress and status. The touch panel shows all
of it. An optional third wire carries KISS telemetry, with finer voltage and
current steps. The board records the telemetry to microSD as a Betaflight
blackbox log, offers the card to a USB (Universal Serial Bus) host as a
read-only drive, and reads and writes AM32 ESC settings over the signal wire.

It is also a servo tester: PWM (pulse-width modulation) servo output on the
same two signal pins, with manual position, sweep and two servos at once (see
[Servo tester](#servo-tester); design note `docs/design/servo-tester.md`).
It programs KST and Chaservo servos on the ESC pin (see
[KST servo programming](#kst-servo-programming); design note
`docs/design/kst-programming.md`), and Hitec D series and classic servos (see
[Hitec servo programming](#hitec-servo-programming); design note
`docs/design/hitec-programming.md`). The AM32 editor, the servo tester and
the two servo programmers take the pins through the pin claim of
`docs/design/pin-claim.md`. An optional INA219 current sensor on the
board's I2C (Inter-Integrated Circuit) bus reads the servo supply's current
and voltage (see [Current sensor](#current-sensor); design note
`docs/design/current-sense.md`). SETTINGS shows the firmware version and,
under DIAGNOSTICS, the reason for the last restart and the time since boot
(design note `docs/design/diagnostics.md`).

An S.BUS (Futaba's serial bus for receiver channels) receiver monitor, MENU →
RX SIGNALS → MONITOR, shows a receiver's 16 channels, its two digital
channels, the link state, the frame rate and the error counts (see
[S.BUS monitor](#sbus-monitor); design note `docs/design/rx-signals.md`).
During its pin claim it receives on GP1 through UART0 (universal asynchronous
receiver-transmitter 0) with the pad's input inverted and decodes the frames
with rcbench's decoder. It is tested on the host only; the inversion on the
pin is not measured. An S.BUS output to a flight controller, OUTPUT on the same
chooser, sending a frame every 14 ms on GP0 through UART0 with the pad's output
inverted, the throttle channel under MAX THROTTLE, is designed in
`docs/design/sbus-output.md` and not built: it waits on a check for an ESC
bootloader on GP0 before START, which needs the Bluejay and BLHeli_S reading of
ESC SETTINGS first. The chooser draws it disabled, `NOT BUILT`. A PPM
(pulse-position modulation) output is not built, by the owner's decision. An
angle meter on the board's QMI8658 IMU (inertial measurement unit), MENU →
TOOLS → ANGLE METER, reads incidence, decalage, dihedral and control-surface
throw from gravity, in degrees and in mm, with ZERO, MAX each side and a
three-pose calibration (see [Angle meter](#angle-meter); design note
`docs/design/angle-meter.md`). It is built and tested on the host only:
nothing of it has run on a board, and its accuracy is not measured. A run
summary per blackbox log, written beside the log and shown on SD LOG, with the
board's PCF85063 real-time clock dating the files on the card as an option, is
designed in `docs/design/run-summary.md`; the owner answered its questions
on 2026-11-15. Its slices 1 to 5 are built and host-tested: the summary
computed from the telemetry, the logger writing it as
`LOGS/LOGnnnnn.TXT` beside each log
([Blackbox logging to microSD](#blackbox-logging-to-microsd)), SD LOG
showing it after a stop, the clock read at start-up, dating the files and
the summary's clock lines while it runs, and the clock set from a computer
over the USB serial port with `tools/set_clock.py`
([Setting the clock](#setting-the-clock)). The owner's board has no cell
in the RTC socket J2, so its clock is set again after each power-up, and
files written before the set have FAT date 0. Nothing of it has run on the
board.
An ESC COMMANDS page, MENU's first row, sends DShot beacons, direction and
3D mode, each change saved in the ESC behind a 1 s hold (slices 1 and 2 of
`docs/design/esc-commands.md`, host tests only, no command of it sent to an
ESC from this board yet). The armed tester shows RPM per volt on its
`ERPM` line and an estimate of the motor's kV, its speed constant in RPM per
volt with no load, as `KV EST`, in place of the pole count (slice
3 of the same note, host tests only, not measured on a motor); see
[Telemetry and logging](#telemetry-and-logging). The owner answered the
note's questions on 2026-11-15.
A USB bridge, through which an ESC tool on a desktop computer reaches the
ESC's bootloader on SIG over the board's USB serial port, by passthrough at
19200 baud or through MSP (the MultiWii Serial Protocol) and the 4-way
interface of flight controllers, is designed in `docs/design/usb-bridge.md`;
the owner answered its questions on 2026-11-15. It
is entered from MENU's `ESC CONFIG` chooser, beside `ESC SETTINGS`. Its
slices 1 and 2 are built, tested on the host only: its pin user, its core1
job and its line, two PIO state machines on SIG, and passthrough with the
screen `USB BRIDGE`, see [USB bridge](#usb-bridge). MSP and the 4-way
interface are its slice 3, not built.
Reading and editing Bluejay and BLHeli_S ESC settings over the signal
wire, from the connect page of `ESC SETTINGS`, with rcbench's escwire
module, is designed in `docs/design/bluejay-blheli-s.md`; the owner
answered its questions on 2026-11-15. Its slice 2 is built, tested on the
host only: the `ESC CONFIG` chooser on MENU, the title `ESC SETTINGS`, and
a Bluejay or BLHeli_S answer named on the connect page with nothing read,
after which SIG returns to DShot only from a leave page. Reading and
writing their settings are not built.
Menu structure B is described in `docs/design/menu-b.md`; its 10 slices are
in the tree, tested on the host only: its building blocks, its colour roles,
its names and SETTINGS, its layout table, its write rule and BACK guard, the
one leave page of KST SERVO, HITEC SERVO and SERVO TESTER, the tester's
readouts that open SETTINGS with LATCH, SERVO TESTER's title, the MENU row's
label, MENU's rows by device with PROGRAMMER, the chooser of the servo
programmers, and the design notes and checks no single slice owned.
The uses that the design note's decision 11 names take their role's colour,
and a value changed and not yet written is cyan.

![UI preview](docs/ui-preview.png)

*The host test suite renders every screenshot here from the UI (user
interface) code.
`tools/make_previews.py` stitches them together.*

> ### [User manual — the Wiki](https://github.com/subtilitas/MiniRcBench/wiki)
>
> **[English](https://github.com/subtilitas/MiniRcBench/wiki/Setup)** ·
> **[Deutsch](https://github.com/subtilitas/MiniRcBench/wiki/Einrichtung)**
>
> Setup, wiring, first run, every screen, telemetry, logging, AM32
> configuration, the servo tester, KST and Hitec programming, and
> troubleshooting, for someone using the board. This README describes the
> firmware and the build.

Project state and open items: [STATUS.md](https://github.com/subtilitas/MiniRcBench/blob/main/STATUS.md). Changes by release: [CHANGELOG.md](https://github.com/subtilitas/MiniRcBench/blob/main/CHANGELOG.md).

---

## Core split

Bidirectional DShot is half-duplex. The ESC answers only frames it receives,
and it disarms when frames stop. A telemetry display therefore cannot listen
passively. It has to be the throttle source. This makes the device a bench
tester, not an inline monitor.

The display transfers 153,600 bytes per full frame (240 × 320 pixels in RGB565,
16 bits per pixel: 5 red, 6 green, 5 blue) over SPI (Serial Peripheral
Interface) by DMA (direct memory access). At the 62.5 MHz write maximum in
`LCD_SPI_HZ` that is at least 19.7 ms (computed, not measured). DShot frame
timing cannot share a core with that, so the work is split:

| | core0 | core1 |
|---|---|---|
| Job | touch, rendering, battery sense | DShot frame pump + telemetry decode |
| Rate | 40 Hz (`UI_PERIOD_MS` = 25 ms) | 1 kHz (`DSHOT_PERIOD_US` = 1000 µs) |
| Allowed to block | yes | never |

The two cores share one struct guarded by a `critical_section`. Core0 calls
`escHeartbeat()` every frame. If no heartbeat arrives for
`UI_HEARTBEAT_TIMEOUT_MS` (250 ms), core1 forces the throttle to zero.

A screen that needs the signal pins claims them (`src/pin_claim.h`). The claim
asks core1 to suspend the DShot pump. Once core1 sees the request, it sends
zero frames for at least 250 ms (`ARM_ZERO_THROTTLE_MS`) after the last
throttle frame, then stops the pump with no edges, releases the pin (a plain
input with the pulls off), puts the pull-up back on it and ends the KISS
receiver. Core1 then runs that screen's job instead (`src/core1_job.h`), and
`loop1()` is `escTaskPoll(); core1JobPoll();`. ESC SETTINGS' AM32
transport runs on core0; its core1 job, rcbench's escwire session
(`src/escwire_job.h`), starts only once a Bluejay or BLHeli_S bootloader
answered. The servo tester, the KST and Hitec programmers, the S.BUS monitor
and the USB bridge each have one. Leaving the screen rebuilds the pump from
the wiring it had, disarmed, once the job has let the pins go.

---

## Hardware

**Board:** [Waveshare RP2350-Touch-LCD-2.8](https://www.waveshare.com/wiki/RP2350-Touch-LCD-2.8).
An RP2350A with a 240×320 IPS (in-plane switching) portrait panel (ST7789T3
over SPI1), CST328 capacitive touch, a QMI8658 IMU (inertial measurement
unit), a PCF85063 RTC (real-time clock), a PCM5101A audio codec, a microSD
slot and a LiPo (lithium-polymer) battery charger. In the tables below, I2C is
the two-wire Inter-Integrated Circuit bus, PIO is the RP2350's programmable
I/O block, and GPIO means general-purpose input/output pin.

### Pin map (from the official schematic)

The pin map is `src/board_rp2350_touch_lcd_2_8.h`.

| Function | GPIO | Notes |
|---|---|---|
| LCD (liquid-crystal display) SCK / MOSI / CS / DC / RST | 10 / 11 / 13 / 14 / 15 | SPI1. SCK is serial clock, MOSI master out, slave in, CS chip select, DC data/command, RST reset. GP12 is the panel's MISO (master in, slave out) line, wired and not used |
| LCD backlight | 16 | NPN (negative-positive-negative) transistor as low-side switch, 20 kHz, 8-bit, from a PIO state machine on pio0 (`src/backlight.pio`), so PWM slice 0 stays free |
| Touch + IMU + RTC I2C SDA / SCL | 6 / 7 | I2C1, **shared bus** (touch 0x1A, IMU 0x6B, RTC 0x51), 400 kHz, on J3 too. SDA is serial data, SCL serial clock. An optional INA219 current sensor at 0x40 shares it, under SETTINGS → CURRENT SENSE |
| Touch INT / RESET | 18 / 17 | Interrupt, active low. The touch reset is a net of its own, separate from LCD_RST |
| microSD CLK / CMD / D0–D3 | 19 / 20 / 21–24 | 4-bit SDIO (Secure Digital Input/Output) on PIO, blackbox logging, see below. CLK is clock, CMD command, D0–D3 the data lines |
| Power latch | 26 | `PIN_BAT_EN`, driven high first at boot |
| Power button | 25 | 10 kΩ pull-up, reads low while pressed |
| Battery sense | 27 | ADC (analog-to-digital converter) input 1, 200k/100k divider → `VBAT = Vadc × 3` |
| I2S audio BCK / LRCK / DIN | 2 / 3 / 4 | PCM5101A codec, not used by the firmware. I2S is Inter-IC Sound; BCK is bit clock, LRCK left-right clock, DIN data in |
| RTC INT, IMU INT1 / INT2 | 5, 8 / 9 | Not used by the firmware |
| ESC signal (SIG) | 0 | J4 pin 9, J5 pin 2, free. The TX (transmit) pin of UART0 (universal asynchronous receiver-transmitter 0), which the firmware does not use: the S.BUS job uses UART0's receiver only, and the USB bridge's 19200 baud line is two PIO state machines on SIG. Set under SETTINGS → SIG PIN |
| KISS telemetry RX (receive; KISS is a serial ESC telemetry protocol), AUX | 1 | J4 pin 10, J5 pin 1, free. The RX pin of UART0. Optional third wire; also the servo tester's second servo, and the S.BUS job's input, through UART0 with the pad's input inverted. Set under SETTINGS → AUX PIN |
| Switched servo supply enable (EN) | 28 or 29 | J4 pin 11 or J4 pin 12. Optional, under SETTINGS → SERVO SUPPLY. An output at 3.3 V |

Only GP0 and GP1 are free (`BOARD_FREE_GPIO_MASK` = `0x00000003`). Both are
"Digital IO (FT)" pins, fault tolerant (RP2350 datasheet, Tables 1674 and
1680): they are rated 5.5 V while IOVDD, the board's I/O supply, is at
3.3 V, and 3.63 V while it is off. GP28 and GP29 are "Digital IO /
Analogue" pins, ADC inputs, and not fault tolerant: they are rated IOVDD +
0.5 V. They carry only the switched supply's enable, an output.

### Wiring diagram

![Wiring: the board's connectors, an ESC, a servo on a hand-switched supply and a servo on the switched supply](docs/wiring.svg)

One sheet, four parts:

| Part | Wiring | Details |
|---|---|---|
| Board connectors | J5, J4 and J3 pin by pin, each with its role | [Pin map](#pin-map-from-the-official-schematic) |
| A | ESC: signal on SIG, ground, KISS telemetry on AUX, the BEC (battery eliminator circuit) wire cut | [Wiring the ESC](#wiring-the-esc), [KISS telemetry](#kiss-telemetry-optional-third-wire) |
| B | Servo on its own supply with a switch in the positive lead; a second servo on AUX for the servo tester | [Servo tester wiring](#wiring), [KST wiring](#wiring-1), [Hitec wiring](#wiring-2) |
| C | Servo on a boost converter with output disconnect, from one LiPo cell, whose enable input (EN) is on GP28 or GP29, with a 10 kΩ pull-down at the converter; SIG, EN and GND (ground) on J4. The optional INA219 current sensor on the converter's output wire, its I2C bus, 3.3 V and ground from J3 | [Servo tester, switched supply](#servo-tester), [KST wiring, switched supply](#wiring-1), [Classic servos](#classic-servos), [Current sensor](#current-sensor) |

`tools/make_wiring.py` draws `docs/wiring.svg` (Scalable Vector Graphics)
from the connector table in `src/board_rp2350_touch_lcd_2_8.h`, the start
pins in `src/config.h`, the enable pins in `src/servo_supply.h`, checked
against `servoSupplyPinOf()` in `src/servo_supply.cpp`, and the INA219's
address and shunt in `src/sense.h`. In the connector table `^` marks the
I2C pins of the sensor's connector, J3, and `*` the bus's other pins, J4
pins 7 and 8; the script stops when `^` is on more than one connector, when
that connector carries more than SCL, SDA, 3V3 and GND, or when the address
is one of the board's own I2C parts. The Docs
job in `ci.yml` runs `python3 tools/make_wiring.py --check`, which fails when
the file differs from what the script draws, and when the script takes one
of nine changed sources it has to refuse (`SENSOR_MUTANTS`).

### Wiring the ESC

Two wires:

| ESC | Board |
|---|---|
| Signal | **GP0**, J4 pin 9 or J5 pin 2 |
| Ground | **GND** (ground), J4 pin 1 or 5, or J5 pin 4 |

The board has no 2.54 mm headers. All signals are on JST-SH 1.0 mm
connectors, so an ESC lead needs a pigtail:

```
J4 (12P): 1=GND  2=VBUS 3=USB_N 4=USB_P 5=GND   6=3V3
          7=GP7* 8=GP6* 9=GP0   10=GP1  11=GP28 12=GP29
J3  (4P): 1=GP7^ 2=GP6^ 3=3V3   4=GND
J5  (4P): 1=GP1  2=GP0  3=3V3   4=GND
                                (^ = on the shared I2C bus: J3 is the
                                     connector of the optional INA219, at 0x40)
                                (* = on the shared I2C bus: do not use)
                                (J5 pin 3: never connect)

ESC signal (SIG)     GP0   J4 pin 9  or J5 pin 2
KISS telemetry (AUX) GP1   J4 pin 10 or J5 pin 1
GND                        J4 pin 1 or 5, or J5 pin 4
servo supply EN      GP28  J4 pin 11, or GP29  J4 pin 12 (optional)
```

> **Cut or depin the middle wire first.** The middle conductor of an ESC lead
> is the BEC (battery eliminator circuit) **+5 V** output, and it goes to no
> pin. GP0 and GP1 tolerate 5.5 V on the signal while the board is powered
> (IOVDD at 3.3 V) and 3.63 V while it is off. The tester needs signal and
> ground only.

> **Warning: J5 pins 2, 3 and 4 are in the order of a servo plug.** They are
> SIG (GP0), 3V3 and GND: signal, plus and minus, as on a servo or ESC lead.
> **Never connect a servo or ESC lead straight across to J5.** Pin 3 is the
> board's 3.3 V supply. Joined straight across, it gets the ESC's BEC output,
> +5 V, or the servo's supply, 4.8 V to 6.0 V or 8.4 V for an HV
> (high-voltage) servo, and that can destroy the board. Leave J5 pin 3
> unconnected.

SIG, the ESC pin, and AUX, the KISS telemetry pin, are set on the board
under **MENU → SETTINGS → SIG PIN** and **AUX PIN** and stored in flash. A
step of either row swaps SIG and AUX, so the two never share a GPIO; the
J5 pin of each shows under its value. `DSHOT_PIN_LCD_2_8` in `config.h` is
the pin a board with blank flash starts on, **GP0**, J4 pin 9 or J5 pin 2.
The KISS telemetry wire starts on **GP1**. To swap them at build time,
build with `-DDSHOT_PIN_LCD_2_8=1 -DKISS_PIN_LCD_2_8=0`. The two must move
together. A build that puts both on one pin fails a `static_assert`.

An ESC on the wrong one of the two pins produces no error. **LINK** stays at
`0/S`.

**Power the ESC from its own battery.** Do not power a motor from the board's
5 V pin. Connect the ESC ground to the board ground, and keep the signal wire
short. The maximum working lead length is not measured. If the **LINK** error
rate rises, set **DSHOT** to 300 under **MENU → SETTINGS**.

The board has a power-button latch on GP26 (`PIN_BAT_EN`). The firmware drives
it high as the first action at boot. Without it, the board loses power when
the button is released while running on battery. On USB the latch has no
effect.

### Other boards

The image is for the RP2350-Touch-LCD-2.8 only. It does not check which board
it runs on: flashed onto another board, it can drive pins that are wired to
other parts there. The splash and SETTINGS → BUILT FOR name the board the
image is built for. DshotDisplay supports the RP2350-Touch-LCD-2.

---

## Building

Native [Pico SDK](https://github.com/raspberrypi/pico-sdk) (software
development kit) with CMake. No Arduino core, no IDE (integrated development
environment).

Requirements: `cmake` 3.18 or later, `ninja` and an `arm-none-eabi` toolchain.

```sh
sudo apt install cmake ninja-build gcc-arm-none-eabi   # Debian/Ubuntu
brew install cmake ninja arm-none-eabi-gcc             # macOS
```

Fetch the SDK once and set `PICO_SDK_PATH`:

```sh
git clone --depth 1 --branch 2.1.1 https://github.com/raspberrypi/pico-sdk.git ~/pico-sdk
git -C ~/pico-sdk submodule update --init --depth 1 lib/tinyusb
export PICO_SDK_PATH=~/pico-sdk
```

`lib/tinyusb` is required. Serial telemetry, and the line that sets the
clock ([Setting the clock](#setting-the-clock)), run over USB CDC
(communications device class). Without the submodule the SDK builds with no
USB support and reports no error.

Build:

```sh
cmake -B build -G Ninja -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .
ninja -C build
```

Build options:

```sh
-DDSHOT_PIN_LCD_2_8=1    # the ESC pin a blank board starts on
-DKISS_PIN_LCD_2_8=0     # ...and the telemetry pin; the two move together
-DSTRICT=ON              # warnings fatal, for this project's sources
```

The ESC pin defaults to **GP0** (`DSHOT_PIN_LCD_2_8`) and the telemetry pin
to **GP1** (`KISS_PIN_LCD_2_8`). The suffix names the board, as the image's
file name does. There is no board to select, and a `-DBOARD` is ignored.
Configuring with `-DDSHOT_PIN`, `-DKISS_PIN`, `-DDSHOT_PIN_LCD_2` or
`-DKISS_PIN_LCD_2` fails, with a message naming the options this build takes.

The output is `build/MiniRcBench-touch-lcd-2.8.uf2`. UF2 is the USB Flashing
Format.

To flash: hold **BOOT**, tap **RESET**, release **BOOT**, and copy the `.uf2`
onto the USB drive that appears.

This image's settings layout is version 6. A block of versions 2 to 5,
written by an earlier image, is carried over, each newer setting at its
default; a block of any other layout version is discarded, and the board
starts from the compiled defaults, the ESC pin GP0. See SETTINGS under
[Using it](#using-it).

`-DSTRICT=ON` adds `-Werror` for this project's own sources. CI (continuous integration) sets it. It is
off by default, so a toolchain update that adds a warning does not stop a
local build.

Only the Cortex-M33 build (`rp2350-arm-s`) is supported. This project does not
build or test for the RP2350's Hazard3 RISC-V cores.

### Prebuilt image

Every [release](https://github.com/subtilitas/MiniRcBench/releases) has the
`.uf2` attached. Flash it as above.

Every green CI run uploads the `.uf2` as an
artifact, so an untagged commit can be tested on hardware.

`tools/sdtest` is not among them. It is a bench diagnostic for the SD card
wiring, not firmware, and builds for the RP2350-Touch-LCD-2.8 only. A plain
`ninja -C build` builds it locally next to the firmware, as
`build/sdtest.uf2`.

### Reproducible builds

`CMakeLists.txt` pins each dependency and fetches it at configure time. The
tree contains no third-party source. The DShot driver in `src/dshot/` is part
of the tree: it comes from [rcbench](https://github.com/subtilitas/rcbench),
which has the same copyright holder and the same MIT licence, and each of its
files names the rcbench commit it was taken from.

| Dependency | Version | Pinned by |
|---|---|---|
| pico-sdk | 2.1.1 | commit hash |
| no-OS-FatFS-SD-SDIO-SPI-RPi-Pico | v3.6.2 | commit hash |

Each hash has its tag in a comment beside it. A tag can be moved; a hash cannot.

Two constraints on these dependencies:

- Only part of the SD library is built: FatFs, its glue, its card layer and
  its SPI card driver. The board's card is on SDIO, but the card layer's init
  path calls into the SPI driver, so the image does not link without it. The
  library's own `CMakeLists.txt` would also compile its SDIO driver, which
  its README derives from GPL-3.0 (GNU General Public License version 3)
  code. So `CMakeLists.txt` fetches the source without adding it, and defines
  the target `sd_fatfs` from a copy of the library's `src/` without the SDIO,
  ZuluSCSI-firmware and SdFat directories. The SDIO driver is `src/sdio/`, in
  this tree; its design note is `docs/design/sdio-driver.md`.
- The SD driver is the **SDIO-SPI** repository, not the older SPI-only one. The
  SPI-only driver's `rtc.c` includes `hardware/rtc.h`. The RP2040 has that RTC
  peripheral; the RP2350 does not (it has powman instead), so that driver fails
  to compile for the RP2350.

---

## Using it

**Splash**

For 3 s (`SPLASH_MS`) after power-up the board shows its name, the board the
image is built for, `RP2350-TOUCH-LCD-2.8`, and the firmware version, for
example `VERSION 0.1.0`. DShot starts on the ESC pin when the splash ends,
so the last line names that pin in amber: `UNPLUG ANY SERVO FROM GP0 NOW`. A
servo left on it reads the DShot frames as pulses and drives towards one end
of its travel. When `settingsLoad()` carried a block of settings version 2,
3 or 4 over (`settingsCarriedOver()`), two amber lines at y 40 and y 52 read
`SETTINGS CARRIED OVER FROM AN UPDATE` and
`CHECK MENU > SETTINGS, THEN SAVE`; when it discarded a whole block of a
version it has no layout for (`settingsResetByUpdate()`), they read
`SETTINGS RESET BY UPDATE` and `CHECK MENU > SETTINGS`. Blank flash and a
damaged block show neither. While the touch controller has not
answered once since `touchInit()`, the splash shows
`TOUCH CONTROLLER NOT FOUND` in amber at y 64, under those two lines and
above the name; the line goes at the controller's first answer
(`uiSplashTick()`). The splash reads the touch every 25 ms into a state of
its own, so a finger resting on the glass when the tester appears gives no
press edge: it does not arm, does not move the throttle and does not start
a hold. Lifting it still ends a tap where it rests, on a button that acts
on release such as LATCH or MENU, because the driver keeps where the press
began; on the pole count or the ceiling the lift opens SETTINGS. If you
touched the screen at power-up, lift the finger before HOLD TO ARM.

**Main screen**

- **HOLD TO ARM**: press and hold for 1 s (`ARM_HOLD_MS`). Arming is refused
  unless the throttle has been at zero for 250 ms (`ARM_ZERO_THROTTLE_MS`).
  The hold counts only for a press that starts on the button while disarmed
  and while no USB host holds the card. A host that takes the card during
  the hold cancels that press. The press that disarms, kept down, does not
  arm again.
  While a USB host holds the card the button is drawn as an outline, the
  disabled look, and the throttle bar reads `EJECT THE USB DRIVE TO ARM` in
  amber.
  Once armed the button reads **DISARM** and acts on touch-down.
- **Throttle bar**: the gesture depends on LATCH.

  | | LATCH off (default) | LATCH on |
  |---|---|---|
  | Bar label | `THROTTLE` | `LATCHED` + grab handle |
  | Gesture | **absolute**: finger position sets the throttle | **relative**: throttle moves by the finger's travel |
  | Touching the bar | jumps to that position | changes nothing |
  | On release | returns to zero | stays |

  The bar is 34 px tall (`THR_TRACK_H`). The outer 16 px at each end
  (`THR_EDGE_SAT`) count as the rail, so zero and the ceiling are reachable
  with a finger. Relative drags accumulate across several swipes and
  re-anchor at both rails. `HOLD_DRAG_SENSITIVITY_PCT` (default 100) sets how
  much of the ceiling one track width of travel covers.

  The percentage shown is of the full 0–2000 DShot range. The bar's full width
  is the configured ceiling.
- **Throttle pad**: the area above the gauge (RPM readout and telemetry tiles)
  is a relative throttle surface in both modes. Swipe **up** for more,
  **down** for less. Swipes accumulate.

  A tap on the pad never changes the throttle. A drag engages after 6 px of
  travel (`PAD_DEADZONE_PX`). One full-height swipe covers 60 % of the ceiling
  (`PAD_DRAG_SENSITIVITY_PCT`).

  The pad ends where the gauge's grab area begins, with no overlap and no gap.
  The hint under the RPM readout reads `SWIPE = THROTTLE`, grey while
  disarmed and in the text colour while armed, and changes to `SWIPE ACTIVE`
  in green while a drag is engaged. While the
  3D note shows ([Telemetry and logging](#telemetry-and-logging)), the note
  stands in the hint's place until a drag engages; `SWIPE ACTIVE` then
  replaces the note, and the note returns on release. The pad works the same.
- **LATCH**: off by default, blue while on. Turning it off sets the
  throttle to zero.
- **MENU**: the list of tools and settings. Opening it disarms, sets the
  throttle to zero and turns LATCH off.
- **Pole count and ceiling**: `14 POLES` at x 10, y 114, and `MAX 20%` at
  x 88, y 236, both in the text colour. While disarmed and while the pin
  claim is free, each is underlined in the text colour (y 122 and y 244) and
  a tap on it opens SETTINGS (`TesterCtl::Poles` at {4, 106, 72, 22},
  `TesterCtl::Max` at {82, 230, 60, 22}). The tap runs MENU's disarm first,
  through `uiOpenTool()` in `src/ui.cpp`, and `BACK` on SETTINGS opened this
  way returns to the tester. While armed they show no underline, `KV EST`
  stands in the pole count's place ([Telemetry and
  logging](#telemetry-and-logging)), and a press on them is the throttle
  pad's or the throttle bar's: the pole count lies in
  the pad, `MAX` reaches into the bar's grab area. While another screen
  holds the signal pins (DShot restarting) they show no underline and a tap
  does nothing, as MENU's SETTINGS row is refused. While the 3D note takes
  the row, the pole count and its target are gone; `MAX` still acts.

**Touch**

The CST328 driver (`src/cst328.cpp`) reads only the controller's first
contact slot, a 5-byte read of finger 1. Which contact the controller puts
there while two touch the glass is not measured, so a press made while the
throttle finger rests on the glass may not register: lift the throttle
finger before DISARM. A wet screen, or a lead lying across the glass, can
count as a contact and block every tap.

**Button behaviour**

Every button except DISARM acts on release and shows a pressed outline while
held. Sliding off before release cancels the press.

DISARM acts on touch-down.

The `-` / `+` steppers step once on press. After 400 ms held
(`REPEAT_DELAY_MS`) they repeat every 120 ms, after 1.2 s every 60 ms, and
after 2.5 s every 25 ms.

Arming and disarming dim the backlight to a quarter for 120 ms
(`BL_DIP_MS`), so the event is visible while watching the motor.

**MENU**

![MENU](docs/menu-preview.png)

*EDT live, EDT silent, no ESC answering, and PROGRAMMER, the servo
programmers' chooser.*

MENU is a list of rows. Each row is 40 px tall at a 42 px pitch, with the name
at scale 2 on the left and its state right-aligned. **BACK** returns to the
tester. Every tool's **BACK** returns to MENU, except SETTINGS opened from
the tester's pole count or ceiling, whose **BACK** returns to the tester.

- **EDT ON / EDT OFF / NO ESC**: a read-only chip on the strip under the
  title, square and 12 px tall, so it does not look like a button.
  `EDT ON` in green while EDT frames arrive; `EDT OFF` in red while
  eRPM frames arrive without EDT; `NO ESC` in dim grey while no eRPM frame
  has arrived for 500 ms (`ESC_LINK_STALE_MS`), the normal state with no ESC
  connected. The firmware sends the enable command itself (see
  [Telemetry and logging](#telemetry-and-logging)). The ESC does not
  acknowledge the command, so the chip shows received frames, not sent
  commands. The strip's caption names both signal pins by role and number
  and the DShot rate, for example `SIG GP0  AUX GP1  DSHOT600`.

| Row | State on the right | A tap |
|---|---|---|
| **ESC COMMANDS** | nothing | Opens ESC COMMANDS: BEEP 1 to 5, the direction and 3D mode, each change saved in the ESC. See [ESC commands](#esc-commands) |
| **ESC CONFIG** | nothing | Opens ESC CONFIG, the chooser of the ESC tools: ESC SETTINGS and USB BRIDGE. See [AM32 ESC configuration](#am32-esc-configuration) and [USB bridge](#usb-bridge) |
| **SERVO TESTER** | nothing | Opens the servo tester. See [Servo tester](#servo-tester) |
| **SERVO PROGRAMMER** | nothing | Opens PROGRAMMER, the chooser of the servo programmers: KST / CHASERVO, HITEC D SERIES, HITEC CLASSIC. See below |
| **RX SIGNALS** | nothing | Opens RX SIGNALS, the chooser of the receiver signal tools: MONITOR, and OUTPUT drawn disabled, `NOT BUILT`. See [S.BUS monitor](#sbus-monitor) |
| **TOOLS** | nothing | Opens TOOLS, the chooser of the bench tools that use no signal pin: ANGLE METER. See [Angle meter](#angle-meter) |
| **SD LOG** | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a USB host holds the card | Opens the logging status. See below |
| **SETTINGS** | `UNSAVED` while the live settings differ from flash; else `UPDATED` while an update's reset or carry-over is not saved over (`settingsResetByUpdate()`, `settingsCarriedOver()`); else nothing; both amber | Opens SETTINGS: the tester, the wiring, the servo supply and current sensor, the display, the board, diagnostics. See below |

The rows go by what sits on the signal pin: the ESC, a servo, then the
bench. A tool row shows no state: the strip names the pins.

- **DSHOT RESTARTING**: SERVO TESTER claims the signal pins, ESC CONFIG's
  ESC SETTINGS claims SIG, SERVO PROGRAMMER's entries open the KST and Hitec
  programmers, which claim SIG, RX SIGNALS' MONITOR claims GP1, ESC COMMANDS
  needs DShot running on SIG, SETTINGS changes the wiring a claim
  latched, and TOOLS' ANGLE METER saves to flash, which no claim may
  share. While the
  claim is not free, for example after RESTART DSHOT until the DShot pump
  runs again, these rows are drawn disabled, an outline, SETTINGS under its
  own status and the tool rows with none, a tap opens nothing, and the
  strip's caption reads `DSHOT RESTARTING ON SIG GP0` in amber. A tool
  opened some other way while another claim holds reads
  `WAIT: DSHOT RESTARTING`; no path through the screens reaches that.
- **Rows**: six rows fit the screen without scrolling. MENU holds eight, so
  it scrolls by 78 px, with a 3 px scroll bar at x 235. TOOLS is the sixth
  row, whole and tappable without a drag. SD LOG, the seventh, shows 6 px at
  y 314 and is whole after a drag of 34 px; SETTINGS, the eighth, starts
  below the screen and is whole after a drag of 76 px. A row takes a tap
  only while 40 px of it show.

**PROGRAMMER**

The SERVO PROGRAMMER row opens PROGRAMMER (`src/ui_servo_prog.cpp`), a
chooser of three entries, 228 x 46 px at y 62, 142 and 222, from
`UI_PROGRAMMERS` in `src/ui_registry.cpp`. The chooser claims nothing:
DShot runs on SIG while it shows. A tap on an entry opens its tool through
`uiOpenTool()`, and the tool claims SIG on that frame. **BACK** returns to
MENU, and so do the tools' RESTART DSHOT and their BACK before a claim
holds. The strip reads `SIG GP0  SERVO SUPPLY OWN`, or the supply's pin.

| Entry | Lines under it with SERVO SUPPLY at OWN | With GP28 or GP29 |
|---|---|---|
| **KST / CHASERVO** | `POWER THE SERVO UP AFTER THE TAP.` | `THE BENCH SWITCHES IT ON GP28.` |
| **HITEC D SERIES** | `D485HW, D645MW AND THE OTHERS`, `POWER THE SERVO UP AFTER THE TAP.` | the models, `THE BENCH SWITCHES IT ON GP28.` |
| **HITEC CLASSIC** | drawn disabled: `HS-5XXX, HS-7XXX`, then in amber `NEEDS THE CONVERTER OF WIRING PART C` and `AND SETTINGS > SERVO SUPPLY GP28/29.` | live: the models, `THE BENCH SWITCHES IT ON GP28.` |

A servo powered before the tap would take DShot's frames, about a 970 us
pulse at 1000 Hz, and move: so the tap comes first, and the servo is
powered after it, or by the bench.

**SD LOG screen**

- **STATUS**: `NO CARD`, `READY`, `RECORDING` or `CARD ERROR`. The board has
  no card-detect pin, so the firmware learns of a card only by mounting it.
- **START / STOP**: starts or ends a log. Disabled with no card.
- **FILE**, **FRAMES**, **WRITTEN**: the open file, frames logged, kB written.
  After a log ends, FILE keeps its name with `(LAST)`, for example
  `LOG00012.BFL (LAST)`, until the next start or mount; `--` before the first
  log and after a mount (`sdLogBegin()`), which also zeroes the counters: the
  card may be another one.
- **The last run**, rows 2 to 8 once a log has stopped and while the logger
  holds its run summary (`SdLogStatus::summary`): `MAX RPM`, `MAX CURRENT`,
  `MAX TEMP`, `MIN VOLTAGE`, `USED` (the KISS counter's rise) or
  `USED FROM CURRENT`, `ARMED` and `SUMMARY`, the `.TXT`'s name or
  `NOT SAVED: CARD FULL` / `NOT SAVED: WRITE FAILED`. Each extreme is the
  held value, `--` when unknown; RPM uses MOTOR POLES at the stop. A source
  tag beside a value reads `KISS`, `EDT` or `MIXED` (KISS with EDT in its
  gaps); a value from DShot eRPM has none. Red: `MAX TEMP` at 90 °C and
  above, and `NOT SAVED`. FRAMES to WORST FLUSH are then in the `.TXT`, and
  CARD and MOUNT are hidden until the next START that opens a log, or a
  mount. While a log runs, and after a mount or USB DRIVE, the rows show
  the counters, CARD and MOUNT. A failed START keeps the last run, as it keeps
  FILE. The tester shows nothing of the run.
- **Note lines** (y 217 and y 228): how the last log ended, when it ended
  on its own or a tool took the card, in amber (`SdLogEnd`); a card's fault
  until the next start or mount, a tool's stop until the next start
  (`sdLogMountForget()`), with a mount hint on its second line:
  `RECORDING STOPPED: CARD FULL` / `FREE SPACE ON A COMPUTER, RETRY MOUNT`
  (FatFs wrote fewer bytes than asked with `FR_OK`),
  `RECORDING STOPPED: CARD WRITE FAILED` / `REMOVED? REINSERT, THEN RETRY MOUNT`
  (a write, sync or close returned an error),
  `NO LOG STARTED: THE FILE DID NOT OPEN` / `CHECK THE CARD, THEN RETRY MOUNT`
  (`LOGS` not made or read, or the file not opened),
  `NO LOG STARTED: LOG99999.BFL EXISTS` / `MOVE LOGS TO A COMPUTER, RETRY MOUNT`
  (no number after the highest file),
  and `RECORDING STOPPED BY KST SERVO`, `HITEC SERVO` or `ESC SETTINGS` when
  the tool's claim stopped the log (`cardCacheBegin()`). A USB DRIVE refusal
  outranks them for its 4000 ms.
- **DROPPED FRAMES**: frames the ring buffer refused because the card was
  slower than the data. Red when non-zero. Non-zero means the buffer is too
  small for that card.
- **BUF PEAK**: buffer high-water mark against `SD_LOG_BUFFER_BYTES`. Amber
  above 1/2, red above 3/4.
- **WORST FLUSH**: the longest single card write, in ms. Amber above 50 ms.
- **CARD**: card type and size. **MOUNT**: the FatFs result code and name,
  for example `0 OK`, `3 NOT READY`, `13 NO FILESYSTEM`.
- **RETRY MOUNT**: mounts again. The card is otherwise mounted only once, at
  boot. With `NO CARD` and `3 NOT READY` and no card size, the note line
  above START reads `INSERT A CARD, THEN RETRY MOUNT` when nothing answered
  the card initialisation, and `CARD NOT USABLE: RESEAT OR REPLACE` when a
  card answered and failed.
- **USB DRIVE**: serves the card to the USB host. See
  [Blackbox logging to microSD](#blackbox-logging-to-microsd).
- **BACK**: returns to MENU, also while a host holds the card. The handover
  continues, and arming stays blocked until the card is ejected.

**SETTINGS screen**

![SETTINGS](docs/settings-preview.png)

*SETTINGS at the top, scrolled to the end, in high contrast, and the UNSAVED
marker it puts on MENU.*

SETTINGS opens from MENU, or from a tap on the tester's pole count or
ceiling while disarmed; **BACK** returns to where it was opened from.
SETTINGS is one list in sections. The list fills y 40 to 239 (to y 227 while
the ESC pin's warning shows, see below) and scrolls under a drag. A row is
44 px tall at a 50 px pitch. **MOTOR POLES** and
**MAX THROTTLE** lead it, so they need no scroll. The sections, in order:
ESC TESTER, WIRING, SERVO SUPPLY, DISPLAY, BOARD and DIAGNOSTICS. On a row
with a connector line, SIG PIN, AUX PIN and SERVO SUPPLY, the value sits at
y+8 to y+21 of the row and the line, scale 1, at y+27 to y+33, both ending at
x 132. Section captions and the values of the rows that change are in the text
colour, read-only values grey, and a toggle that is on fills blue.

| Section | Row | What it does |
|---|---|---|
| ESC TESTER | **MOTOR POLES** | eRPM to RPM is `RPM = eRPM / (poles / 2)`. Range 2–28, even values only. Default 14 |
| ESC TESTER | **MAX THROTTLE** | The throttle ceiling. Default 20 % (400 of 2000). Steps of 5 %, range 5–100 % |
| WIRING | **SIG PIN** | SIG, the GPIO of the DShot signal, with its connector pin under the value: `GP0` over `J5 PIN 2`, `GP1` over `J5 PIN 1`. Steps through the board's two free GPIOs; a step onto AUX's GPIO swaps SIG and AUX, so on this board every step is a swap |
| WIRING | **DSHOT** | The DShot bitrate in kbit/s, 150 / 300 / 600 / 1200, wrapping at the ends. Applies at once: the driver is rebuilt on the new pin and rate before the next frame |
| WIRING | **KISS TELEM** | Whether to listen for the telemetry wire, on AUX. A toggle; it moves no pin |
| WIRING | **AUX PIN** | AUX, the GPIO of the KISS telemetry wire and of the servo tester's second servo, with its J5 pin under the value. Acts whatever KISS is, and swaps with SIG as SIG PIN does. The receiver is a PIO state machine, so any free GPIO works |
| SERVO SUPPLY | **SERVO SUPPLY** | `OWN`, the default, `GP28` or `GP29`: whether the enable input of a DC-DC converter for the servo's supply is wired, and to which pin. `OWN`: the servo has a supply of its own and the bench switches nothing. With `GP28` the value has `J4 PIN 11` under it, with `GP29` `J4 PIN 12` (design note `docs/design/kst-programming.md`, "Switched servo supply"). A core section after WIRING, whichever tools MENU lists. A stepper: - and + step OWN, GP28, GP29 and stop at the ends; it applies at once. With a pin chosen, that pin is an input with both pulls off and its input disabled outside a servo tester, KST or Hitec claim, so the converter's external pull-down holds it off; during a claim the bench switches the servo's supply ([Servo tester](#servo-tester), KST servo programming, Hitec servo programming) |
| SERVO SUPPLY | **CURRENT SENSE** | `NONE`, the default, or `INA219`: whether an INA219 current sensor is on the I2C bus at 0x40 ([Current sensor](#current-sensor)). A toggle; it applies at once. With `NONE` nothing is sent to 0x40. With `INA219` the bench probes the part at once and every 1000 ms while it is not found, and a caption under the label reads `FOUND`, `NOT FOUND` or `OTHER PART`. `OTHER PART` is another chip at 0x40, or an INA219 left on another configuration, which a power-cycle of the module clears |
| DISPLAY | **CONTRAST** | `NORMAL` or `HIGH`. See below |
| DISPLAY | **BACKLIGHT** | 16–255 in steps of 32. High contrast forces 255 while on |
| BOARD | **BUILT FOR** | `RP2350-TOUCH-LCD-2.8`, the board the image is built for. Read-only |
| BOARD | **VERSION** | The firmware version from `src/version.h`, for example `0.1.0`. Read-only |
| BOARD | **CLOCK** | The board's clock to the minute, `2026-11-14 15:42`, or `NOT SET` in grey, with the caption `SET AT EACH POWER-UP`: the board has no cell on J2, so `tools/set_clock.py` sets it again after each power-up ([Setting the clock](#setting-the-clock)). Redrawn once a minute and after a set while its row shows. Read-only |
| DIAGNOSTICS | **LAST RESTART** | Why the chip last restarted, read once at boot: `POWER-ON`, `BROWN-OUT`, `RUN PIN`, `WATCHDOG`, `UPDATE`, `REBOOT`, `DEBUGGER`, `GLITCH`, `POWER-DOWN` or `UNKNOWN`. Amber for `BROWN-OUT`, `GLITCH` and `WATCHDOG`. Read-only. See below |
| DIAGNOSTICS | **UPTIME** | Time since boot as hours, minutes and seconds, `H:MM:SS`, for example `1:02:03`. Redrawn once a second while its row shows. Read-only |

The `-` and `+` buttons are 44 × 44 px and the toggles 94 × 44 px.

**LAST RESTART.** `src/diag.cpp` reads four values first thing in
`setup()`: the power manager's CHIP_RESET register
(`powman_hw->chip_reset`), the watchdog's REASON register
(`watchdog_hw->reason`), `watchdog_enable_caused_reboot()` and the boot ROM's
`rom_get_last_boot_type()`. A pure function, `diagDecodeRestart()`, turns
them into the reason; the first line that matches decides:

| Read | Shows |
|---|---|
| `watchdog_enable_caused_reboot()` | `WATCHDOG`: an armed watchdog timed out |
| REASON non-zero and boot type `BOOT_TYPE_FLASH_UPDATE` (4) | `UPDATE`: the reboot after a UF2 copy |
| REASON non-zero otherwise | `REBOOT`: a reboot through the watchdog that another program asked for, for example picotool |
| HAD_GLITCH_DETECT | `GLITCH` |
| HAD_WATCHDOG_RESET_RSM, HAD_WATCHDOG_RESET_SWCORE | `WATCHDOG` |
| HAD_HZD_SYS_RESET_REQ | `DEBUGGER` |
| HAD_SWCORE_PD | `POWER-DOWN` |
| HAD_POR | `POWER-ON` |
| HAD_BOR | `BROWN-OUT` |
| HAD_RUN_LOW | `RUN PIN` |
| HAD_DP_RESET_REQ, HAD_RESCUE | `DEBUGGER` |
| HAD_WATCHDOG_RESET_POWMAN, HAD_WATCHDOG_RESET_POWMAN_ASYNC | `WATCHDOG` |
| none of these | `UNKNOWN` |

The SDK's register header marks for each CHIP_RESET flag whether its reset
resets the power manager. The flags of resets that leave it alone come
first: CHIP_RESET may still hold an older flag beside them, and theirs is the
newer. Whether the chip clears older flags when it records a new one is not
known.

According to the boot ROM source, not verified on hardware, the reboot after
a UF2 copy goes through the watchdog with the boot type of a flash update,
so it reads `UPDATE`, not the CHIP_RESET flag of an older reset. The
firmware arms no watchdog: nothing calls `watchdog_enable()`. So `WATCHDOG`
can only appear once a watchdog is armed. Nothing in the firmware calls
`watchdog_reboot()` either. Not tested on hardware: which reason a given
restart shows on the board is not recorded. The design note is
`docs/design/diagnostics.md`.

**Scrolling.** A drag scrolls the list when it starts off a control: on a
section name, a row's label or value, or the gap between rows. After 6 px of
vertical travel the list follows the finger. A press on a control never
scrolls: `-` and `+` step on touch-down and repeat while held, and a toggle
acts on release. A control cut by the edge of the list takes a press only
while at least 40 px of it show (`UI_LIST_TAP_MIN`). Opening SETTINGS sets the
scroll to 0 px.

Below the list, fixed and not scrolled:

- **LINK**: live packets per second and checksum error rate. Green when
  packets arrive, amber above 5 % errors.
- The note line.
- **HOLD TO SAVE** (150 × 46 px, from x 6) and **HOLD DEFAULTS**
  (72 × 46 px, to x 233), at y 268. HOLD DEFAULTS' label takes two lines,
  `HOLD` over `DEFAULTS`, in amber.

A wrong ESC pin produces no error: the ESC receives no frames. **LINK** on this
screen shows the result of each pin change. A non-zero packet rate means the
pin is correct.

The settings block is version 6 (`SETTINGS_VERSION`), 44 bytes of payload:
version 5's 14 and the angle meter's ALONG AXIS, SIGN, LENGTH and
calibration after them ([Angle meter](#angle-meter)). By the owner's
decision of 2026-11-14 a whole block of version 2, 3, 4 or 5 is carried
over (`settingsCarriedOver()`): `settings.cpp` describes each older layout
as byte offsets, recovered from the commits that wrote it (version 2:
12 bytes of payload, the CRC at byte 20; versions 3 to 5: 14 bytes, the CRC
at byte 24), and the host suite builds each block byte by byte. Whole means
the magic, the version, that layout's payload size and its CRC-32 right.
Every field the old layout has keeps its value, judged by
`settingsValidate()` as a version 6 block is; every newer field takes its
default: CURRENT SENSE `NONE`, and the angle meter's `LONG`, `NORMAL`,
LENGTH `OFF` and no calibration. A version 3 SERVO SUPPLY of 1 named an
enable on GP0, the ESC wire's pin since version 4, and loads as 0, `OWN`; an
ESC pin on GP28 or GP29 from a block before version 4 is not free here and
takes the default, KISS on such a pin is switched off, and a KISS pin this
board does not free takes the default. DshotDisplay 2.x's block carries
over the same way: its `src/settings.h` at the tags v2.0 and v2.3 and at
b9e94dc, the file's last change, holds version 2 with the same layout.
Flash is not rewritten at the load: the old block stays the only copy until
a save lands, so a power loss at boot cannot lose it, and every boot until
then carries it over again. Until a save lands the splash shows its two lines,
MENU's SETTINGS row `UPDATED`, and the note line
`CARRIED OVER BY UPDATE - CHECK, SAVE` while nothing else is pending; HOLD
TO SAVE writes a version 6 block even with nothing changed.

A whole block of a version with no layout here, 1 or above 6, or of version
2 to 5 with another layout's payload size, is discarded, and the
compiled defaults load, every setting and the ESC pin included.
DshotDisplay discards this project's blocks the same way. Such a block (the
magic, another version, at most `SETTINGS_OTHER_MAX` bytes of payload and
its own CRC right) sets `settingsResetByUpdate()` until a save lands: the
splash's two lines, `UPDATED` on MENU's SETTINGS row, and the note line
`RESET BY UPDATE - CHECK AND SAVE` while nothing else is pending.

**HOLD TO SAVE** (1 s, `UI_HOLD_MS`) writes all settings to flash: every row
above, pole count and throttle ceiling included. Erasing the flash sector stops
core1, and with it the DShot pump, for the length of the erase. That duration
is not measured. Saving is refused while armed (`NOT WHILE ARMED - DISARM TO
SAVE`); opening MENU disarms, so the refusal is a backstop. **HOLD DEFAULTS**
(1 s, `UI_HOLD_MS`, the same hold widget, `src/ui_hold.*`) loads the compiled
defaults, pole count and throttle ceiling included, into the live settings;
the angle meter's setup and calibration, which SETTINGS does not show, keep
their values. A
tap loads nothing, a slide off the button cancels the hold, and a bar over
the button and a `C_RED` fill, as on every hold of `src/ui_hold.*`, show it
running. It has no armed check: SETTINGS opens only through MENU, which
disarms. Flash changes only on the next save.

Every value applies when changed. Only persistence needs the hold, so a value
can be tried and then discarded by leaving without saving. The note line reads
`UNSAVED - HOLD SAVE TO KEEP` after a change, and otherwise
`SAVED SETTINGS IN USE` or, with nothing stored,
`NOTHING SAVED: DEFAULTS IN USE`. After HOLD DEFAULTS it
reads `DEFAULTS LOADED - RESTART TO UNDO` while the live settings are what it
left and differ from what the next boot loads (the saved settings, or the
compiled defaults when none are stored). Any other change ends it, and so
does every completed HOLD TO SAVE, failed saves included: a save whose
read-back fails has already rewritten the sector. MENU shows `UNSAVED` on
the SETTINGS row while the live settings differ from flash.

SIG PIN moves DShot at once (`escTaskConfigure()`). While the live ESC pin is
not the one `settingsLoad()` put live, the pin the splash named
(`settingsBootDshotPin()`), a line of its own at y 230 (`PIN_Y`) repeats the
splash's warning for the live pin in amber, for example
`UNPLUG ANY SERVO FROM GP1 NOW`, and the list ends at y 227 (`VIEW_H_PIN`), so
it scrolls 12 px further; its top rows do not move. The rule is the boot pin
rather than the saved pin, which a save would end, or the pin at opening, which
a return to SETTINGS would end, while DShot stays on a pin the splash did not
name. With KISS on, a step of SIG PIN or AUX PIN swaps the two, so DShot runs on
the wire the ESC's KISS pad was on until the wires are swapped as well: the line
then reads `SWAP THE ESC'S SIGNAL AND KISS WIRES` (`SETTINGS_SWAP_NOTE`, 36
characters, 215 px). The note line keeps its own order: SAVE FAILED and NOT
WHILE ARMED, until the next save or leaving; ADJUSTED and KISS OFF, until the
next change (KISS OFF follows a KISS pin the board does not free, which no
SETTINGS row sets: the steppers offer free pins only); DEFAULTS LOADED; UNSAVED;
SAVED; the reset by an update; then the set in use. SAVED is green, SAVE
FAILED and NOT WHILE ARMED red, and the other notes but the set in use amber.

**High contrast**

![High contrast](docs/contrast-preview.png)

*The tester screen in the normal and the high-contrast palette.*

High contrast is black on white. Accents are darkened to carry white text,
every glyph stroke gets one extra pixel of weight, frames are 2 px, the
seven-segment stroke is thicker, and the backlight is forced to 255. It is
meant for direct sunlight on the transmissive IPS panel.

The palette has fewer distinct greys than the dark one. Label greys are
merged so that labels stay legible outdoors.

The extra glyph weight is a 1 px vertical smear. The 5×7 font sits in a 6 px
cell, so a horizontal smear would close the gap between glyphs. A vertical
smear adds no width, so no layout changes.

**Reading the telemetry**

`--` means no source has supplied that field within its timeout. Plain eRPM
works on any bidirectional DShot ESC. Voltage, current, temperature, stress and
status need EDT support in the ESC firmware (BLHeli_32, Bluejay, AM32). The
**LINK** tile shows good packets per second and the checksum error rate over
the last second. At 1 kHz a clean link reads close to `1000/S` and `0% ERR`.
A rising error rate points at signal integrity.

The voltage and current tiles carry a source tag: **KISS** in the text colour
or **EDT** dimmed. EDT reports voltage in 0.25 V steps and KISS in 0.01 V steps.
A tag that drops from KISS to EDT means the telemetry wire has stopped, and the
reading is 25 times coarser. Consumption in mAh appears on the temperature
tile with KISS only. EDT does not carry it.

The tester's top row shows `DSHOT` plus the bitrate the DShot driver runs
at, for example `DSHOT600`, as set under SETTINGS → DSHOT, dim, and in the
text colour once an EDT enable has been sent; `DSHOT1200`, the widest, ends
at x 156. `DSERR` in amber replaces it when the driver could not claim its
two PIO state machines; nothing is sent to the ESC then.

The large voltage at the top right, captioned `BOARD`, is the board's own
battery, read through the 200 kΩ/100 kΩ divider on GP27. It reads at most
9.90 V. The ESC's pack voltage is the **VOLTAGE** tile. It is drawn amber
below 3.50 V (`BOARD_CELL_LOW_MV`) and red below 3.30 V
(`BOARD_CELL_CRIT_MV`), a lithium-polymer cell's usual low range, not
measured on this board, and goes back only 50 mV (`BOARD_CELL_HYST_MV`)
above each threshold (`uiBoardCell()`). The band is taken from the value as
drawn, rounded to centivolts, so `3.50V` is never amber and `3.30V` never
red. What the divider reads with USB power alone is not measured, and no
GPIO senses USB power, so the firmware cannot tell USB from the cell. With
USB power alone the value can be low and then says nothing about a cell;
below 2.50 V (`BOARD_CELL_IMPLAUSIBLE_MV`), implausible for a working cell,
it is drawn in the normal text colour, and red again from 2.55 V, the same
50 mV band.

While the value is red (`BoardCell::Critical`, `uiBoardWriteRefused()`), KST
SERVO, HITEC SERVO and ESC SETTINGS refuse a write and a KST or Hitec restore.
KST SERVO and HITEC SERVO ask at the end of the write page's hold: refused
there, no file goes to the card and no frame to the servo. They ask again just
before their files and their write go out, after a wait for a card flush:
refused there, the backup and the write's record may be on the card already, and
no frame goes out. ESC SETTINGS asks once, before its backup: refused, no file
goes to the card and no byte to the ESC. A write already running is not stopped:
cut off, it would leave the servo partly written and unverified, while one that
finishes ends with its read-back. The KST and Hitec write pages read, in amber
at y 196 and y 208, `BOARD VOLTAGE IS RED: NOT WRITTEN.` and `CHARGE THE BOARD'S
CELL, HOLD AGAIN.`, their hold still live; ESC SETTINGS shows the first line as
its write's result on `ESC WRITE`, in amber at the same place, over `CHARGE THE
CELL. TAP OK, WRITE AGAIN.`, since its result stands under OK. Amber, the text
colour, and a value below 2.50 V (no cell to judge) refuse no write. The bands
are a lithium-polymer cell's usual range, not measured on this board; what the
divider reads with USB power and no cell is not measured either, so a board on
USB alone may refuse writes.

---

## Telemetry and logging

The firmware enables EDT itself. There is no button.

The first frames the DShot pump sends after it starts are the enable,
`EDT_ENABLE_REPEATS` (10) identical frames of DShot command 13. The pump starts
at power-up, after a change of the ESC pin, the bitrate, KISS or the KISS pin
under SETTINGS, and when the AM32 editor hands the pin back. Every reply is
decoded with EDT on, from the first one:

- an ESC that kept EDT on across one of those restarts is read correctly from
  its first reply, and none of its EDT frames is shown as an RPM;
- an ESC without EDT, or with EDT off, sends eRPM frames only. AM32 and
  Bluejay normalise the exponent of every eRPM frame, with EDT on or off, and
  those frames decode the same with EDT on. For other ESC firmware this is
  unknown: an eRPM frame that is not normalised, an exponent above 0 with
  bit 8 of the payload clear, decodes as an EDT reading and gives no RPM;
- the replies to the enable frames are decoded like any other. AM32 answers
  the enable with a status frame.

At the 1 kHz frame rate the enable at pump start takes 10 ms. Arming
during it sends the throttle on the next frame and drops the rest of the
enable. A command and the first KISS telemetry request wait for its end.

The enable shares the wire with one command slot on core1, which sends
DShot commands 1 to 5 (beacons), 12 (save settings), and 7 to 10
(direction, 3D mode) with 12 after them. The slot is slice 1 of the design
note `docs/design/esc-commands.md`; ESC COMMANDS is its only user.
A command starts only while all of these hold: disarmed, an eRPM answer
younger than 500 ms, the last eRPM answer 0, the ESC answering for 1500 ms
(`ESC_CMD_SETTLE_MS`; AM32 takes commands only after more than 1 s of zero
input), and no enable or other command on the wire. A beacon is 6 frames,
then 260, 260, 280, 280 or 1020 ms of zero throttle for beacons 1 to 5
(`ESC_CMD_BEEP_GAP_MS`). Commands 7 to 10 and 12 are 10 frames each
(`DSHOT_CMD_REPEATS`), each followed by 300 ms of zero throttle
(`ESC_CMD_GAP_MS`): AM32 acts on the sixth equal frame, Bluejay and BLHeli_S
on the seventh. Every command frame carries the telemetry bit, without which
Bluejay and BLHeli_S drop it. For 7 to 10 and 12, frames 1 to 6 of each
block of 10 need an answer, eRPM or EDT; one of frames 1 to 5 may go
unanswered, and a second, or none to frame 6, stops the block. An arm drops
the command, and a teardown of the pump, for a pin claim or a wiring change,
stops a command and expires a request that still waits. No scheduled enable
goes out between a command and its save, and no KISS request goes out
during a command's frames.

An ESC that is not yet ready for commands ignores that enable, and it does
not count as an attempt. While an ESC answers with eRPM and no EDT frame has
arrived for `EDT_STALE_MS` (1000 ms), the enable is sent again every
`EDT_RETRY_MS` (1000 ms), counted from the last frame of the one before.
After `ESC_LINK_STALE_MS` (500 ms) without eRPM the state resets. An ESC
connected later, power-cycled or replaced then gets its own enable.

Each repeated enable is `EDT_ENABLE_REPEATS` (10) identical frames. It is
sent only when all of these hold:

- the link has been up for `EDT_SETTLE_MS` (300 ms). An ESC answers eRPM
  within milliseconds of power-up but executes commands only after a run of
  valid zero-throttle frames;
- the tester is disarmed and eRPM is zero. DShot commands execute only with
  the motor stopped, and a motor coasts for seconds after a disarm;
- no other command is on the wire, its gap of zero throttle included. An ESC
  executes a command only after a run of identical frames, so a beacon
  inside an enable would break both.

Every reading expires. An EDT field not refreshed for `EDT_STALE_MS` (1 s)
shows `--`. RPM shows a dark red zero, and the line under it reads
`NO TELEMETRY`, after `ESC_LINK_STALE_MS` (500 ms) without eRPM. A display that
held old values would show a plausible reading from an ESC that is no longer
connected.

The RPM area's bottom row, y 114, holds the pole count, `14 POLES`, and the
swipe hint. While `esc3dNote()` (`src/esc_3d.h`) is true it holds `3D MODE ESC:
0-50% REVERSED AT 2X` (`ESC_3D_TESTER_NOTE`, 33 characters, 197 px, x 10 to x
206) in amber in their place, armed or not, while the pad is not engaged. While
a drag is engaged, `SWIPE ACTIVE` in green takes the row alone, as the only sign
the drag took hold, and the note returns on release. The `ERPM` / `NO TELEMETRY`
line above it, y 102, does not change. Arming is not refused on it. In 3D mode
the ESC runs the tester's 0 to 50 % the reverse of its set direction at twice
the throttle, and above 50 % in the set direction from low. The flag is set by
an ESC SETTINGS read that opens the list on settings with 3D MODE on, by a write
of settings with 3D MODE on that the ESC accepted (`am32BlWrite()` returned OK),
whether the read-back then verifies, fails or differs, and by ESC COMMANDS' 3D
ON once sent, or once stopped after the ESC may have acted on it: such an ESC
may run in 3D mode. Only a list read or a verified write with 3D MODE off clears
it, and a restart does, since it is not stored ([AM32 ESC
configuration](#editing) explains the throttle scale). ESC COMMANDS' 3D OFF, a
failed read, a read that does not open the list (the read-only view of a layout
not known among them: byte 0x12 may be another field there), a write the ESC
refused and a write of 3D MODE off that is not verified leave it. It describes
the last ESC those two tools reached, not the one on the pin now: a swapped ESC
is not detected.

**RPM/V and the kV estimate.** kV is a motor's speed constant, RPM per volt with
no load. While armed, the `ERPM` line, y 102, carries `RPM/V` after two spaces,
`ERPM 156800  RPM/V 1400`, and the bottom row's pole count gives its place to
`KV EST`, `C_TEXT` at x 10, y 114, until the disarm (decisions 6 and 7 of
`docs/design/esc-commands.md`, slice 3). With RPM the eRPM from the display
filter over half of MOTOR POLES, before the readout's clamp at 99999, V the
voltage the `VOLTAGE` tile shows, KISS where fresh, EDT otherwise, and t the
throttle sent, 0 to 2000, `src/kv_est.h` computes, in 64-bit integers, rounded
half up and clamped to 99999:

- RPM/V = RPM / V;
- the kV estimate = RPM × 2000 / (V × t).

The estimate assumes the ESC's duty is t / 2000 and a motor with no
propeller. Neither is checked. On AM32 2.22 the design note works out from
AM32's source that the estimate at 75 % throttle is × 0.92 to × 1.08 of the
rated kV for the SINE STARTUP and MIN DUTY settings it covers, with RUNNING
LEVEL 10 and on a target built without `USE_STARTUP_BOOST`; the duty maps of
Bluejay, BLHeli_S and BLHeli_32 are not verified. Load, losses, the drops in
the ESC and the wiring, and a wrong pole count are inside the figure. An ESC
in 3D mode that the 3D note does not know of, set by another tool or in an
earlier power-up of the bench, gives a wrong figure with no warning: on AM32
at t 1500 the motor runs in the set direction at a duty of about 0.50, and
the estimate reads about × 0.67 of the rated kV. So it is labelled `KV EST`,
never `KV`.

| Field | Shown when | Otherwise |
|---|---|---|
| `RPM/V 1400` | armed; eRPM within `ESC_LINK_STALE_MS` (500 ms); RPM above 0 with MOTOR POLES 2 or more; a voltage from KISS within `KISS_STALE_MS` (500 ms) or EDT within `EDT_STALE_MS` (1000 ms), above 0.5 V | `RPM/V --`; disarmed, no `RPM/V` |
| `KV EST 1758` | the same, and t from `KV_EST_MIN_THROTTLE` (1500, 75 %) to 2000, within `KV_EST_SETTLE_BAND` (20) of one value for `KV_EST_SETTLE_MS` (1000 ms), the filtered eRPM within `KV_EST_ERPM_SETTLE_PERMILLE` (0.5 %) of the last reply, and the 3D note not standing | `KV EST 75%+` below 1500 when the inputs are there; else `KV EST --` |

The settle counts from the UI frame on which the throttle sent left the band of
±20 around the value the settle started at (`kvSettleStep()`, stepped in
`uiTick()` after `escSetThrottle()`): a finger resting on the bar at `MAX 100%`
moves the throttle by about 11 a pixel, and a settle that restarted on every
change would never end. 1000 ms leaves 0.48 % of a step in the display filter.
The eRPM check covers a motor still spinning up. Up to 234,375 eRPM one eRPM
step is 0.20 % to 0.39 % of the value, inside the 0.5 %; above it a step is
1 / period (1 % at 600,000 eRPM). Once the check has passed it holds until the
settle restarts, another condition fails, or the filtered eRPM is more than 3 %
from the last reply (`KV_EST_ERPM_HOLD_PERMILLE`, 30) or the reply is 0, so
noisy replies do not make the field flicker between the figure and `KV EST --`
and a stall reads `KV EST --` on its first frame. The motor's own spin-up time
is not measured.
At the default `MAX 20%` the estimate never shows. The widest texts fit, held by
`static_assert`s in `src/ui.cpp`: `ERPM 60000000  RPM/V 99999`, 26 characters at
the decoder's largest eRPM, 155 px, ends 27 px before the `RPM` label at x 192;
`KV EST 99999`, 71 px, ends 50 px before `SWIPE = THROTTLE`. While the 3D note
stands it takes the row; `RPM/V` stays on the line above. The blackbox log
records nothing new: `motor[0]`, `eRPM[0]`, `vbatLatest` and `vbatEdt` give both
figures.

An expired **ESC STATUS** reads `--`, never `OK`.

Bidirectional DShot and EDT work on hardware. No defect is known as of
DshotDisplay v2.3.

### KISS telemetry (optional third wire)

EDT rides in the eRPM frame and needs no extra wire. Its resolution is 0.25 V
and 1 A. On a 6S pack a 200 mV sag is below one step, and idle current reads
0 A.

An ESC with KISS telemetry (BLHeli_32 and AM32 implement it) sends a 10-byte
frame at 115200 baud on a separate wire when the DShot frame requests it. The
frame carries voltage and current in 0.01 V and 0.01 A steps, plus consumed
mAh, which EDT does not carry.

| ESC | Board |
|---|---|
| Telemetry pad | **GP1**, J4 pin 10 or J5 pin 1 |

The line is transmit-only from the ESC. Requests go out every 20th DShot frame
(`KISS_REQUEST_EVERY_N`), which is 50 Hz at 1 kHz.

**Status:** working on hardware (owner report). It is off by default
(`KISS_ENABLE_LCD_2_8` = 0) because the board has two free pins and the ESC
uses one.

**Any free GPIO can receive it.** The receiver is a UART built from one PIO state machine. The
RP2350's two hardware UARTs receive only on GPIOs where `n % 4 == 1`: of the
two free pins, GP1 only. The PIO receiver also takes GP0, so the ESC and
telemetry pins can swap. See `src/pio_uart_rx.h`.

The tiles fall back from KISS to EDT `KISS_STALE_MS` (500 ms) after the last
KISS frame.

> **Voltage.** The KISS specification puts this line at 3.6 V. GP1 tolerates
> 5.5 V while the board is powered (IOVDD at 3.3 V) and 3.63 V while it is
> off. The level a given ESC drives is not measured by this project.

RPM is **not** taken from KISS. KISS arrives at 50 Hz and bidirectional DShot at
1 kHz, and spin-up, oscillation and desync show in the 1 kHz stream. KISS
resolves 100 eRPM. Bidirectional DShot encodes a period with a 9-bit mantissa,
so its step grows with RPM. Above about 5,500 mechanical RPM on a 14-pole motor
(about 6,500 on 12 poles, 4,800 on 16) the KISS step is finer. The log
records both. The arithmetic is in `docs/design/erpm_resolution.py`.

### Blackbox logging to microSD

Logs are written in Betaflight's blackbox format. They open in
[**logwiju**](https://subtilitas.github.io/logwiju/), the viewer written for
them, and in Blackbox Explorer, `blackbox_decode` and PIDtoolbox. Files are
named `LOGS/LOGnnnnn.BFL` on a card formatted with FAT (File Allocation
Table). Each new file takes the highest number in `LOGS` plus 1, so the
highest number is the newest log; an empty `LOGS` starts at 00001. A name
counts in any letter case, as FAT compares names without it; any other name
is ignored (`sdLogNameNumber()`). The first start after a mount reads `LOGS`
once with `f_readdir()`, every directory entry once, about one 512-byte
sector per 16 short-named files; later starts count on from memory. A
mount (`RETRY MOUNT`, the end of USB DRIVE, a tool's remount), FatFs
mounting the card again on its own after a card fault, and every logging
error forget the number, since a card swapped or edited in a card reader
looks the same to a board with no card-detect line
(`src/sd_log_next.cpp`, tested over FatFs by `make -C test lognum`). After
`LOG99999.BFL` no log starts and SD LOG says so. A file is
created new, never over an existing one. Logs an earlier firmware wrote to
the card's root stay there and are not counted. Files carry the time of the
board's real-time clock while the clock is set in that power-up, else FAT
date and time 0 (see below).

Each log that stops clean gets a run summary beside it, `LOGS/LOGnnnnn.TXT`
with the log's number (`src/run_summary.*`, `src/sd_log_summary.*`,
`docs/design/run-summary.md`). It is plain ASCII (American Standard Code for
Information Interchange) text, one value a line: the log's name, the
firmware version, the clock at the start and the stop (`2026-11-14 15:42:07`
or `not set`), how the log ended (`STOP`, `DISARM`, `KST SERVO`,
`HITEC SERVO` or `ESC SETTINGS`), its duration and armed time, the maximum
RPM, current and temperature, the minimum voltage while armed, the charge
used by the KISS counter and from the current, and the logger's counters
that SD LOG shows while a log runs (frames, dropped frames, kilobytes
written, buffer peak, worst flush). Each extreme is the value held over
three consecutive new readings, with its source and the raw extreme beside
it. After the stop SD LOG shows the held values, the charge and the armed
time in its rows 2 to 8, and whether the `.TXT` was saved (see SD LOG
screen above); a log a card error ended shows its run up to the failure,
`NOT SAVED`.

`sdLogTick()` hands every frame's telemetry and arm state to the summary,
a constant-time update on core0 (`runSumAdd()`): at most about 1,000
instructions a frame, about 10 µs at 150 MHz or 0.5 % of core0 at 500 frames
a second, estimated from the compiled code, not measured. The stop writes the
file only after the `.BFL` has closed clean, with `FA_CREATE_ALWAYS` over
any `.TXT` of that number, the text built in the drained write buffer. A
`.TXT` does not count as a log's name, so it does not move the next log's
number. A summary that cannot be saved (card full, a failed write) leaves
the `.BFL` as it is and the logger `READY`: a short or failed file is
removed when the card still allows it; after a close that fails, an empty
`.TXT` can stay. A log a card error ended gets no
`.TXT`. The `.TXT`, the `.BFL` beside it and every failure path are tested
over FatFs R0.15 on a RAM disk by `make -C test lognum`
(`test/sd_log_fatfs.cpp`).

The clock is the board's PCF85063A at 0x51 on I2C1 (`src/clock.*`). `setup()`
reads its registers 00h to 0Ah once, after `senseInit()`: 14 bytes on the wire,
129 clocks, with a 1000 µs timeout, noted to the current sensor's bus record. A
running clock, with the oscillator-stop flag OS and the bits STOP, EXT_TEST and
12_24 clear and every time register in range, is the time from then on, kept by
the 64-bit microsecond timer; no bus transfer follows. Anything else leaves the
clock not set until the next start-up. With OS set, the oscillator has stopped
since the time was written, as at every power-up of a board without a cell on
J2: the firmware sends the software reset, 58h to Control_1, and turns CLKOUT
off, 07h to Control_2. FatFs stamps files through `get_fattime()`, which returns
the kept time packed while the clock is set and 0 otherwise; the SD library's
`my_rtc.c`, with its own `get_fattime()`, is not in the firmware build. The
run summary takes the clock at the log's start and
at its stop. Tested against a register-level model of
the part in `test/fake_i2c.*` (`test/test_clock.cpp`); not tested on hardware.

![SD log screen](docs/log-preview.png)

The card runs over 4-bit PIO SDIO at 10 MHz (`SD_LOG_SDIO_HZ`), with the
driver in `src/sdio/`, which is not tested on hardware. The board cannot use
hardware SPI for the card: its SD clock is on GP19, which is SPI0 TX in the
pin mux.

Open the screen with **MENU → SD LOG**, the sixth row. By default arming
starts a log and disarming stops it (`SD_LOG_AUTO_ON_ARM`). Auto-stop ends
only a log that auto-start began. **START/STOP** is always available.

At the default 500 Hz (`SD_LOG_RATE_HZ`) a frame averages 14.5 bytes
(measured). That is 7.3 kB/s, or 26 MB per hour.

Size the buffer from **BUF PEAK** and **WORST FLUSH**. `SD_LOG_BUFFER_BYTES`
defaults to 8192 bytes, about 1.1 s at 7.3 kB/s. Whether that covers a given
card's erase stalls is not measured. A peak near the buffer size, or a flush
longer than the buffer covers, means raise it. **DROPPED FRAMES** above zero
means it was too small.

The logger drops frames rather than block. The UI never waits for the card.

**Getting the files off:** **MENU → SD LOG → USB DRIVE** presents the card
to the USB host as a read-only removable drive, using MSC (mass storage
class) on the same USB cable as the serial port. The logger unmounts before
the drive appears and touches the card again only after eject. The host and
the firmware never share the FAT volume.
While the host holds the card, SD LOG counts what it has read, for example
`3.0 MB READ`, in lime, a live reading.
Without a card the button is drawn unavailable and does nothing. With a card,
the request is refused while armed (`DISARM FIRST`) or while recording (`STOP
RECORDING FIRST`). Arming is blocked while the host holds the card. See
`src/usb_msc.h`. A host that unconfigures the device (`tud_umount_cb()`)
gives the card back as an eject does. TinyUSB 0.18.0's RP2040 port, which the
RP2350 uses, forces VBUS detection on and never reports an unplug: a cable
pulled with the board on its battery arrives as a bus suspend, as a sleeping
host does, and a suspend is not handled. EJECT on SD LOG takes the card back
then. Not tested on hardware.

### Setting the clock

The board has no cell in its RTC socket J2, so the clock is lost whenever
3.3 V goes away: set it again after each power-up. Until then files carry
FAT date 0 and a log's summary reads `not set`. There is no page on the
device to set it; a computer on the USB cable sets it over the serial port:

```text
python3 tools/set_clock.py
```

It needs Python 3 and the `pyserial` package (`python3 -m pip install
pyserial`), on Windows, Linux and macOS. It finds the board by its USB
vendor and product IDs, 2E8A:4006, or takes `--port` (`COM5`,
`/dev/ttyACM0`); `--list` names the boards it finds, `--query` reads the
clock without setting it, `--help` lists the options. It waits for the
computer's next whole second and sends it as local time: no time zone, no
daylight saving, so at a daylight-saving change the clock is set again by
hand. Its host tests are `test/test_set_clock.py`, run by `make -C test run`.

The serial port takes one line command (`src/clock_line.*`), read on core0
in `usbDevTask()`'s pass without waiting for input, one line a pass. A line
ends at a CR (carriage return) or an LF (line feed); a line that does not
start with `CLOCK` is ignored, and so is a line that starts as one of the
board's answers does (`CLOCK SET `, `CLOCK NOW `, `CLOCK NOT SET`,
`CLOCK REFUSED`), so that a terminal echoing them back, as Linux opens a
`ttyACM` port by default, sets nothing. The rest are parsed byte for byte
and answered with one line ending in CR LF:

| Sent | Answer |
|---|---|
| `CLOCK 2026-11-14 15:42:07` | `CLOCK SET 2026-11-14 15:42:07` |
| `CLOCK?` | `CLOCK NOW 2026-11-14 15:42:09` or `CLOCK NOT SET` |
| a date or time out of range, or a day the month does not have (`2027-02-29`, `24:00:00`, years outside 2000 to 2099) | `CLOCK REFUSED: BAD DATE` |
| any other line that starts with `CLOCK`, longer than 32 characters among them | `CLOCK REFUSED: BAD LINE` |
| a line that starts as an answer does | nothing |
| no answer at 0x51, a failed transfer, the bus stuck | `CLOCK REFUSED: RTC NOT ANSWERING` |
| the read-back is not the time sent | `CLOCK REFUSED: RTC READ-BACK DIFFERS` |

A set (`clockSet()`) is four transfers on the shared I2C1 bus, each noted to
the current sensor's bus record (`senseNote()`): 58h to Control_1, the
software reset; 20h and 07h from 00h, STOP with CLKOUT off; the time with OS
clear and the weekday from the date; 00h to Control_1, STOP released. A read
of 00h to 0Ah then checks the part runs at the time sent. The set is 308
clocks on the wire, 770 µs at 400 kHz, the read-back included. Only a set
that reads back changes the kept time, which runs from the moment STOP was
released, and counts one set more; the part starts its count about 0.5 s
after that, so the kept time can run up to 1 s behind it. A refused set
leaves the kept time as it was; the part can be left reset or stopped and
reads not set at the next start-up. A read-back that differs is followed by
20h to Control_1, STOP, so a restart with 3.3 V up, after a watchdog or a
crash, reads the part as not set rather than a time nothing checked.
`usbDevClockLineEnable()` turns the line off for a user that takes the
port's bytes over, the USB bridge; while off, no byte is read.

A set is accepted at any time, during a log as well: it changes no card and
no motor state, and the log's durations come from the timer. The log's
summary then reads `Clock at stop: 2026-11-14 15:44:31 (clock set during
the log)`. The set holds core0 for its 770 µs on the bus: a log frame due
in that time is taken up to 770 µs late, and the frames keep their 2 ms
schedule, which `sdLogTick()` resets only when it falls 8 ms behind; not
measured on the board. SETTINGS → BOARD → CLOCK shows the kept time to the
minute or `NOT SET`.

### Tuning

All in `config.h`. Unless a row says otherwise, a default is a starting value,
not one derived from measurement.

Six of these are only *defaults*: the ESC pin, the DShot bitrate, the KISS
wire, the pole count, the throttle ceiling and the backlight. Each can be
changed under **MENU → SETTINGS** and stored in flash. On a board with saved
settings the stored value wins, so editing `config.h` and reflashing changes
nothing for those six. The SETTINGS note line says which set is in use.

| Setting | Default | What it does |
|---|---|---|
| `DSHOT_PIN_LCD_2_8` | `0` | ESC signal GPIO the board starts on |
| `KISS_TELEM_ENABLE` | `1` | Compile the KISS path in at all |
| `KISS_ENABLE_LCD_2_8` | `0` | Whether the board listens for a KISS wire. **Off**: the board has two free pins and the ESC takes one. One tap under SETTINGS turns it on |
| `KISS_PIN_LCD_2_8` | `1` | GPIO for the telemetry wire. Receive only, changeable under SETTINGS. The other of the two free pins; `DSHOT_PIN_LCD_2_8` has GP0 |
| `KISS_REQUEST_EVERY_N` | `20` | Request every Nth DShot frame; 20 at 1 kHz is 50 Hz. Values below 2 overlap replies and fail an `#error` |
| `KISS_STALE_MS` | `500` | How long a KISS frame stays in use before the display falls back to EDT |
| `EDT_STALE_MS` | `1000` | How long an EDT field stays valid after its last frame. Then the tile shows `--` |
| `ESC_LINK_STALE_MS` | `500` | Time without eRPM before the ESC counts as gone. Resets the automatic EDT enable |
| `EDT_RETRY_MS` | `1000` | Interval between EDT enables while eRPM arrives and EDT does not |
| `EDT_SETTLE_MS` | `300` | Time the link must be up before the first repeated EDT enable. The enable at pump start does not wait |
| `ESC_CMD_SETTLE_MS` | `1500` | Time the link must be up before a DShot command starts |
| `ESC_CMD_GAP_MS` | `300` | Zero-throttle frames after each block of 10 frames of commands 7 to 10 and 12 |
| `ESC_CMD_BEEP_GAP_MS` | `260, 260, 280, 280, 1020` | Zero-throttle frames after beacons 1 to 5 |
| `ESC_CMD_WAIT_MS` | `2000` | A command request that cannot start within this time is dropped |
| `KISS_REPLY_TIMEOUT_MS` | `10` | Abandon an incomplete reply after this long; affects only the timeout counter |
| `SD_LOG_ENABLE` | `1` | Compile the logging path in at all |
| `SD_LOG_SDIO_HZ` | `10 MHz` | SDIO card clock |
| `SD_LOG_RATE_HZ` | `500` | Log frame rate. eRPM is the only 1 kHz signal |
| `SD_LOG_I_INTERVAL` | `32` | Frames between keyframes (I frames). Lower values resynchronise sooner after damage and cost size |
| `SD_LOG_BUFFER_BYTES` | `8192` | Ring buffer. Size it from BUF PEAK and WORST FLUSH |
| `SD_LOG_CHUNK_BYTES` | `512` | Flush size. Keep it a multiple of 512 to avoid read-modify-write in the card |
| `SD_LOG_PREALLOC_BYTES` | `16 MiB` | Contiguous space reserved per file, so the FAT is not rewritten mid-log. About 38 min at 7.3 kB/s. Unused space is released at close |
| `SD_LOG_SYNC_MS` | `2000` | Interval for committing the file's directory entry, the first 2000 ms after the start. A log cut before the first commit reads 0 bytes. After it, with the 16 MiB reservation, the entry says 16 MiB and data written since the last commit is readable; without it, or past 16 MiB, the size is the one at the last commit |
| `SD_LOG_AUTO_ON_ARM` | `1` | Start and stop logging with ARM, in addition to the manual button |

### Reading the logs

Files are written to the directory `LOGS` as `LOGnnnnn.BFL`, each with its
run summary `LOGnnnnn.TXT`, a text file any editor opens.

<div align="center">

[![Open your logs in logwiju](https://img.shields.io/badge/open%20your%20logs%20in-logwiju-07b0c8?style=for-the-badge)](https://subtilitas.github.io/logwiju/)

### **[logwiju](https://subtilitas.github.io/logwiju/)** — the viewer for these logs

</div>

Drop a `.BFL` file onto the page to plot it. It needs no install, upload or
account, and the file stays in the browser. Wheel zooms, drag pans,
shift+drag zooms to a box, double-click fits. The side panel selects fields.

The name, read in German (`w` as English *v*, `j` as English *y*), sounds like
"logview". The tail is **WI**ngert **JU**lian.

The logs also open in
[Blackbox Explorer](https://github.com/betaflight/blackbox-log-viewer), or on the
command line:

```sh
blackbox_decode LOG00001.BFL      # -> LOG00001.01.csv and .01.event
```

The `.event` file holds the end-of-log marker. The CSV (comma-separated values) file holds the data.

Fields per frame: `loopIteration`, `time`, `motor[0]` (throttle), `eRPM[0]`,
`eRPMkiss[0]`, `vbatLatest`, `amperageLatest`, `vbatEdt`, `amperageEdt`,
`escTemperature[0]`, `escConsumption` and `escStress`.

Both eRPM sources are logged: `eRPM[0]` (bidirectional DShot, 1 kHz) and
`eRPMkiss[0]` (KISS, 50 Hz). Both voltage and current sources are logged:
`vbatLatest` / `amperageLatest` carry KISS where it was live, `vbatEdt` /
`amperageEdt` always carry EDT. This allows the KISS decoder to be checked
against EDT from the log itself.

## ESC commands

**MENU → ESC COMMANDS** sends DShot commands to the ESC on SIG: beacons 1 to
5, the spin direction and 3D mode. A change is saved in the ESC in the same
sequence. The design is `docs/design/esc-commands.md`. Every command goes
through the command slot on core1 (see
[Telemetry and logging](#telemetry-and-logging)). No command of this page has
been sent to an ESC from this board: what an ESC does with them is read from
the AM32, Bluejay and BLHeli_S sources and the BLHeli_32 specification text,
not measured.

![ESC commands](docs/esccmd-preview.png)

*At rest, a beacon going out, 3D ON sent, and no ESC answering.*

The page claims no pin: DShot runs on SIG while it shows, and MENU draws its
row disabled while a pin claim is not free. Opening it disarms the tester. It
has no arm control.

| Control | Kind | Sends |
|---|---|---|
| **1** to **5** under BEEP, 40 x 40 px | tap | beacon 1 to 5, 6 frames (`BEEP_REPEATS`), then 260, 260, 280, 280 or 1020 ms of zero throttle |
| **NORMAL**, **REVERSED** under DIRECTION, 111 x 44 px | 1 s hold | 7 or 8, 10 frames, 300 ms of zero throttle, then 12 (save), 10 frames, 300 ms: 620 ms |
| **3D OFF**, **3D ON** under 3D MODE | 1 s hold | 9 or 10, then 12, as above |

A control is live while core1 reads an eRPM answer younger than 500 ms, eRPM
0 and the ESC answering for 1500 ms. A request made while an EDT enable,
command or gap is on the wire waits in the slot, at most 2000 ms, so an EDT
retry does not end a hold. Otherwise the controls are drawn disabled and the
lines under them say why, in amber: `NO ESC ANSWERS ON SIG GP0.`, `MOTOR
TURNING: WAIT UNTIL IT STOPS.` or `ESC STARTING: WAIT 1.5 S.`. While the page's
request waits or its sequence goes out, BACK is drawn disabled too; a request
that cannot start within 2000 ms expires. Leaving the page drops a request that
still waits (`escCancelCommand()`). A hold's fill turns red while held, and a
lime bar under it fills over the 1000 ms (`UI_HOLD_MS`).

The six lines at y 244 to 310, at most 37 characters each:

| State | Lines |
|---|---|
| At rest | `HOLD 1 S: SETS IT, THEN SAVES IT.`, `THE ESC SENDS NO ANSWER TO A COMMAND:`, `CHECK IT ON THE TESTER WITH 3D OFF.`, a blank line, `AM32 BEEPS AS BEEP 1 ON SAVE (NORMAL)`, `OR AS BEEP 2 (REVERSED).` |
| Going out | `SENDING 8 (REVERSED), THEN 12 (SAVE).`, `SENDING 3 (BEEP 3).` |
| A direction sent | `SENT 8 (REVERSED), THEN 12 (SAVE).`, `NOTHING CONFIRMS THE ESC TOOK THEM.`, `CHECK IT ON THE TESTER WITH 3D OFF.` |
| 3D ON sent | `SENT 10 (3D ON), THEN 12 (SAVE).`, `THE TESTER SHOWS THE 3D NOTE NOW.`, `A RESTART OR 3D OFF READ CLEARS IT.`, then in amber `IN 3D, 0 TO 50% RUNS REVERSED AT 2X;`, `ABOVE 50% THE SET WAY, FROM LOW.` |
| 3D OFF sent | `SENT 9 (3D OFF), THEN 12 (SAVE).`, and while the tester's 3D note stands `THE 3D NOTE STAYS ON THE TESTER.` |
| A beacon sent | `SENT 3 (BEEP 3).` |
| Stopped before frame 6 of the first block | `NOT SENT: THE ESC STOPPED ANSWERING.`, amber |
| Stopped after it | `SAVE NOT SENT: POWER-CYCLE THE ESC.`, then for a direction `CHECK IT ON THE TESTER WITH 3D OFF.`, for 3D ON `THE TESTER SHOWS THE 3D NOTE NOW.`, for 3D OFF `AFTER A POWER CYCLE IT MAY RUN IN 3D.`; amber |
| Never started | `NOT SENT: THE ESC WAS NOT READY.`, amber |

With an outcome shown, a readiness that refuses a command takes the sixth
line. The page shows only the outcome of its own last request: opened again,
it is at rest.

The ESC sends no answer to a command, so the bench cannot see the direction.
To check it: 3D off (in 3D the tester's 0 to 50 % runs the motor reversed),
propeller off, a strip of tape on the bell, then on the tester arm, run at
5 % for a second and disarm. On AM32 the save plays beacon 1's tone for
normal and beacon 2's for reversed, and ESC SETTINGS reads `DIRECTION` and
`3D MODE` back. After a power cycle of the ESC, a direction that holds was
saved.

After 3D ON is sent, or stops after the ESC may have acted on it, the tester
shows `3D MODE ESC: 0-50% REVERSED AT 2X` in amber in place of the pole
count. 3D OFF leaves the note, since nothing reads the ESC back; a restart
of the bench or an ESC SETTINGS read with 3D MODE off clears it. The note is
not stored. The tester shows no note for the direction.

## AM32 ESC configuration

**MENU → ESC CONFIG → ESC SETTINGS** reads, edits and writes the settings
of an AM32, Bluejay or BLHeli_S ESC over the signal wire. It does not flash
ESC firmware and never changes a Bluejay or BLHeli_S ESC's tags. The
transports are `src/am32_bl.h` for the greeting and AM32, and rcbench's
escwire session on core1 for Bluejay and BLHeli_S (`src/escwire_job.h`).

MENU's second row, **ESC CONFIG**, opens a chooser
(`src/ui_esc_config.h`), PROGRAMMER's pattern: `ESC SETTINGS` (142 px at
scale 2) with the lines `AM32 SETTINGS, READ AND WRITTEN.` and
`BLUEJAY, BLHELI_S: READ AND WRITTEN.`, and `USB BRIDGE` (118 px) with
`A DESKTOP ESC TOOL OVER USB.` and `PASSTHROUGH: AM32 DIRECT MODE.` under
it ([USB bridge](#usb-bridge)). The strip reads `SIG GP0`. The chooser
claims nothing; the tap on an entry claims SIG. Its BACK, and the tools'
BACK and RESTART DSHOT, return to MENU. The row is drawn disabled while
the pin claim is not free. Its titles
are one set for every firmware, `ESC SETTINGS`, `ESC HEX` and `ESC WRITE`,
and the list's strip names the firmware, for example
`NEOESC f051  F051  AM32 2.16`.

![ESC CONFIG and a Bluejay ESC](docs/esc-preview.png)

![A Bluejay or BLHeli_S page](docs/efm8-preview.png)

![Writing a Bluejay page](docs/efm8-write-preview.png)

![AM32 config](docs/am32-preview.png)

Entering the screen claims the signal pin (`src/pin_claim.h`): it disarms,
sends throttle zero for at least 250 ms after the last throttle frame,
stops the DShot driver and hands the pin to the bootloader transport; the
strip reads `RELEASING SIG GP0` meanwhile. Leaving hands it back, at once
or, after a Bluejay or BLHeli_S answer, from the leave page.

Leaving an AM32 sends the bootloader no `RUN` command. The transport has
one, `am32BlRun()` in `src/am32_bl.h`, and nothing calls it. An AM32 ESC that
answered is in its bootloader when the screen closes, and DShot frames follow
on the signal wire. What the ESC does after the screen is left, its firmware
started on its own, on those frames or not at all, is not recorded.
Power-cycle the ESC after leaving, before driving a motor. A Bluejay or
BLHeli_S ESC whose page was read is left with LEAVE, below. After a session
whose handshake succeeded, or in which an EFM8 bootloader answered, MENU's
strip shows `POWER-CYCLE THE ESC TO RUN IT` in amber on a second line under
its caption (`uiAm32RunCue()`), until an eRPM frame stamped after that answer
arrives, RESTART DSHOT on the leave page below is tapped, or the board
restarts. LEAVE from a Bluejay or BLHeli_S list leaves it: LEAVE has no
answer, so only an eRPM frame shows that the firmware started. A reply that
holds a piece of an EFM8 answer sets it too, below.

### Connecting

Open the screen, then power-cycle the ESC: the page says so in amber,
`POWER-CYCLE THE ESC`, the step to take now. By the AM32 bootloader's source,
the bootloader decides about 46 ms after power-up and then waits while the
line rests high; a greeting's low bits in those 46 ms start the ESC's
firmware, which resets itself about 2 s later with no DShot, and after that
reset only a low in the bootloader's last 5 ms starts the firmware again
(`docs/design/bluejay-blheli-s.md`). Not measured.

The page runs a connect cycle (`src/greet_cycle.h`, that note's decision
2): 1000 ms (`ESC_GREET_QUIET_MS`) with the line on its pull-up and nothing
sent, `LINE HIGH, NOTHING SENT` on the status line, then a burst of 3
(`ESC_GREET_BURST`) 21-byte greetings, one a frame, each waiting 30 ms
(`AM32_GREET_TMO_MS`) for its reply, `WAITING... (TRY n)`, then quiet again.
A greeting holds the line low for at least 6 ms; repeated without a pause it
would keep a Bluejay's bootloader check, a high line for about 150 ms, from
ever passing. By the sources a Bluejay running on DShot enters its
bootloader within the first quiet second, about 320 ms plus 150 ms after
the last frame, and BLHeli_S after its timeout plus 15 ms; an AM32
powered up within a burst or the 46 ms before it, about 15 % of the time,
starts its firmware and returns about 2 s later by its firmware's own
reset. None of it is measured. The tries count greetings only. An answer's
status stays through the quiet phase until the next answer.

`AM32_FORCE_LOW_JUMP` (default 0) instead holds the signal low for
`AM32_BOOT_LOW_MS` (200 ms) once before the first attempt. The reference
configurator does not do this, and the firmware cannot receive while it holds
the line.

The screen shows which half failed:

| Message | Meaning | What to change |
|---|---|---|
| `LINE HIGH, NOTHING SENT` | The quiet phase of the connect cycle | Nothing: a burst of 3 greetings follows |
| `WAITING... (TRY n)` | No reply yet | Power-cycle the ESC while the screen is open |
| `nB: xx xx xx @19200`, then `REPLY NOT UNDERSTOOD:` and `IS THE ESC RUNNING AM32?` | The ESC sent bytes that did not parse | Baud rate or framing, or firmware other than AM32. The first three bytes are shown |
| `EFM8 E8B2 ANSWERED: READING.` | A Bluejay or BLHeli_S bootloader answered; the signature is shown | Nothing: the escwire session reads the page, below |
| `READ FAILED (...)` | Handshake succeeded, settings read failed | Retry; check the wire |
| `NO VALID SETTINGS`, with `NOT AN AM32 ESC, OR BLANK SETTINGS.` and `NOTHING WILL BE WRITTEN.` | The settings block is implausible (`am32Plausible()`): among others a layout revision of 0 or above 8 | Nothing to do: the screen stays on the connect page, which has no `WRITE...` |

### A Bluejay or BLHeli_S ESC

The greeting is a valid greeting for the EFM8 bootloader of Bluejay and
BLHeli_S, so such an ESC can answer it. `blAnswerClassify()`
(`src/bl_answer.h`) reads the answer: 5 bytes or more ending in 0x30 whose
fifth byte from the end is an AM32 family, 0x1F, 0x2B or 0x35, are AM32;
`471` with 0xE8 four bytes after its `4`, anywhere in the reply, is an EFM8
bootloader, its signature 0xE8 and the byte after it, E8B1, E8B2 or E8B5 on
EFM8BB1, EFM8BB2 and EFM8BB51; anything else is not understood. The EFM8
rule does not check the bytes after the 0xE8, so a trailing glitch byte or
a garbled byte still counts as an EFM8 answer and keeps DShot off its
bootloader; escwire's own first contact then checks the answer in full.
From the bootloaders' sources; not seen on an ESC.

A reply not understood that holds a piece of an EFM8 answer, `471`, or `71`,
a byte and 0xE8, or 0xE8 followed by 0xB1, 0xB2 or 0xB5
(`blAnswerEfm8Fragment()`), as an answer with its `d` or its leading `4`
lost does, gives the claim the leave page and MENU the run cue too: its
bootloader may wait on the line. The connect page names the bytes, as for
any reply not understood, and the cycle greets on; BACK opens the leave
page, whose strip reads `EFM8  SIG GP0`. Bytes with none of these pieces
leave the leave page off. A false hit costs a leave page.

An EFM8 answer gives the pin claim a leave page
(`pinClaimWantLeavePage()`) and sets MENU's run cue. Then the AM32
transport lets go of SIG, leaving it floating, and the claim starts its
core1 job, `Core1Job::EscWire` (`src/escwire_job.h`), on core1's next pass.
A bind the driver refuses puts the pull-up back on SIG and names the
resource on the connect page, `SIG GP0 COULD NOT BE TAKEN (DMA)` and the
like, or `(PIN)` for a pin the driver does not take; BACK opens the leave
page.
The job opens rcbench's line driver, `ew_line` on one PIO state machine of
25 words and 2 DMA channels, and drives the line high in the same call. It
holds it high 500 ms more, makes its own first contact, 3 tries of 100 ms,
and identifies the page from its first 112 bytes at 0x1A00, or 0x3000 on
EFM8BB51; then it reads the whole page, 112 bytes for BLHeli_S and 255 for
Bluejay. The read erases and programs nothing. The connect page reads
`EFM8 E8B2 ANSWERED: READING.` (28 characters, 167 px) meanwhile, and in
place of `POWER-CYCLE THE ESC` and its three lines, which do not apply to a
bootloader that waits, `EFM8 BOOTLOADER` over `THE ESC WAITS IN ITS
BOOTLOADER` / `FOR COMMANDS. ITS FIRMWARE DOES` / `NOT RUN UNTIL IT LEAVES.`
A second first contact to a bootloader that already answered the greeting
reads to it as a restart, by its source; not verified on an ESC.

The page decides what opens, by rcbench's `ew_id_efm8()`
(`src/efm8_view.h`):

| Page | Screen | Strip |
|---|---|---|
| Bluejay layout 208, BLHeli_S layout 33, marker 0x55 0xAA | the list | `BLUEJAY 0.21  LAYOUT 208  E8B2` (30 characters, 179 px), grey |
| Bluejay layouts 33 to 207, BLHeli_S layout 32 | the list, read-only | `BLUEJAY 0.19, LAYOUT 206: READ-ONLY.` over `SHOWN ONLY. BACKUP BJ018`, amber |
| Name tag 0xFF with BLHeli_S layout 33 or Bluejay layout 208: the ESC lost its tags | the list, read-only | `ESC LOST ITS TAGS: READ-ONLY.` over `REFLASH ON A COMPUTER. BACKUP BH018` |
| Settings marker not 0x55 0xAA | the list, read-only | `SETTINGS MARKER WRONG: READ-ONLY.` over `SHOWN ONLY. BACKUP BJ018` |
| The first 112 bytes 0xFF | the connect page | status `EFM8 E8B2: SETTINGS PAGE EMPTY`; `PAGE EMPTY, AS AFTER A CUT WRITE.` / `KEEP THE ESC POWERED: SEE MANUAL.` |
| Any other page rcbench does not take | the connect page | status `EFM8 E8B2: PAGE NOT KNOWN`, `BLUEJAY ON EFM8BB1: NOT TAKEN`, `BLUEJAY LAYOUT 205: NOT TAKEN`, `BLHELI_S 16.9: NOT TAKEN` or `EFM8 E8B3: PART NOT KNOWN`; `NOT BLUEJAY OR BLHELI_S AS KNOWN:` / `NOTHING IS WRITTEN.` |

A read-only list saves its backup as it opens, as AM32's read-only view
does, since no write follows: the second strip line names it,
`BACKUP BJ018`, or reads `SHOWN ONLY. NO CARD: NO BACKUP.`,
`SHOWN ONLY. USB DRIVE: NO BACKUP.` while a USB host holds the card, or
`SHOWN ONLY. BACKUP FAILED.`; for lost tags `REFLASH ON A COMPUTER.
NO CARD.`, `REFLASH ON A COMPUTER. USB DRIVE.` or `REFLASH ON A COMPUTER.
BACKUP FAILED.` A list opened again on a page whose text is the ring's
latest backup saves no second copy and names that one.

The list shows rcbench's fields for the page's firmware and layout
(`ew_map_s.c`, `ew_map_bj.c`): its group above its label, MOTOR, STARTUP,
BRAKE, LIMITS, SOUND, INPUT and OTHER, and the value as rcbench writes it,
in capitals; DIRECTION's 3 and 4 read `3D` and `3D REVERSED`, and a value
none of the field's reads `RAW 17` in amber. The revision bytes 0x00 to
0x02 are on the strip. On Bluejay 208 and BLHeli_S 33 the fields rcbench's
map lets a write change are edited as on AM32, below: 15 on Bluejay, 12 on
BLHeli_S. The rest, BLHeli_S' three throttle bytes, Bluejay's PWM INDICATOR
and SAFETY ARM, are grey and shown only. On a read-only list every row is
grey, a tap selects a row for its note, and `WRITE...` and `REVERT` are
drawn disabled. The bar under the list reads, in grey,
`TAP A SETTING TO EDIT`, or on a read-only list
`SHOWN ONLY: NOTHING IS WRITTEN.`, over
`BACK SENDS LEAVE: THE ESC STARTS.` HEX opens `ESC HEX`: the 112 or 255
bytes, 8 to a line, in a list that scrolls; its BACK returns to the list.

The strip's two amber lines, in this order: the BACK guard's, the selected
row's note, why a list is read-only, a half-written page's, then the 3D
note, else the caption.

| When | Lines |
|---|---|
| DIRECTION at 3D or 3D REVERSED, selected | `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| TEMPERATURE or LOW-SPEED PROTECTION selected at OFF | `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| POWER RATING selected | `SETS HOW THE ESC READS ITS TEMP:` / `A WRONG VALUE MISREADS THE LIMIT.` |
| DIRECTION at 3D or 3D REVERSED | the 3D lines |

A read that opens a list with DIRECTION at 3D or 3D REVERSED sets the
tester's 3D note, `3D MODE ESC: 0-50% REVERSED AT 2X`, and one at NORMAL
or REVERSED clears it; a page whose marker is wrong leaves it, since its
firmware loads its defaults at its next start.

#### Writing a Bluejay or BLHeli_S page

`WRITE...` opens `ESC WRITE`, as on AM32: the strip counts the changes,
each change is its label and `old -> new` (`MEDIUM HIGH -> MEDIUM LOW` is the
widest, 25 characters), and the backup line reads
`BACKUP FIRST: BACKUP/BJ001.TXT`, `BACKUP: BACKUP/BJ001.TXT` once saved,
or `NO CARD: OLD VALUES LOST ON LEAVING` in amber. Only HOLD TO WRITE,
1000 ms, writes. First the page as first read in the session goes to the
card, `BACKUP/BJnnn.TXT` for Bluejay or `BACKUP/BHnnn.TXT` for BLHeli_S, in
the ring every device shares (`src/backup_ring.h`), as rcbench's ESC
settings text (`src/efm8_backup.h`): `ESC settings backup, format 1`,
`Firmware:`, `Layout:`, `Processor: EFM8`, `Signature: E8B2`, one line per
field, the whole page's bytes, the tags at 0x40 to 0x6F among them, and a
CRC-32 line. A card that refuses it stops the write,
`NO BACKUP ON THE CARD: NOT WRITTEN`; with no card the write runs and the
backup stays in memory, as on AM32. The BOARD voltage in its red band
refuses the write as on AM32. Then the job runs rcbench's write: the guard
lets only editable fields change, each to one of its values, the page is
read again and compared with the first read, then erased, sent, programmed
and read back; a failed step or a read-back that differs runs it again from
the erase, 3 times at most. The guard refuses an erase or program anywhere
but the settings page, the bootloader's area and the firmware among it.
`WRITING: DO NOT UNPLUG.` stands meanwhile, BACK drawn disabled. The result,
at y 196 and 208:

| Result | First line | Second line |
|---|---|---|
| verified | `WRITE VERIFIED. BACKUP BJ018`, green | `LEAVING STARTS THE ESC WITH THEM.` |
| the re-read failed | `READ FAILED: NOTHING ERASED.` | `TAP OK, WRITE AGAIN.` |
| the page changed since the read | `ESC PAGE CHANGED: NOT WRITTEN.` | `TAP OK, READ AGAIN.` |
| 3 procedures, the page read back as read | `WRITE FAILED: PAGE AS READ.` | `TAP OK, WRITE AGAIN.` |
| the page read back as neither | `HALF WRITTEN: KEEP THE ESC POWERED.` | `TAP OK, WRITE AGAIN NOW.` |
| no answer after the erase | `NO ANSWER IN THE WRITE: KEEP POWER.` | `TAP OK, THEN READ AGAIN.` |

After a changed page or a lost answer `READ AGAIN` takes `WRITE...`'s place
until a read succeeds; a read of a page not half written takes it as the
page as read and keeps the edits, and a page that differs from the one
backed up gets a backup of its own before the next write. While a page is half
written the list edits nothing, LEAVE is refused, and the strip reads
`HALF WRITTEN: KEEP THE ESC POWERED.` over `READ AGAIN, THEN WRITE.` or,
once read, `WRITE AGAIN NOW. REVERT: AS READ.`: `WRITE...` writes the draft
again, or after REVERT the page as first read, its tags included, on a page
that reads `THE PAGE AS READ, TAGS INCLUDED.` with its hold live. BACK on a
half-written page shows `HALF WRITTEN. BACK AGAIN LEAVES IT;` over
`A POWER CYCLE THEN LOSES ITS TAGS.`; the second BACK aborts the session
with no LEAVE and opens the leave page. Edits not written arm the same
guard as on AM32. A write that ended the session, a heartbeat stop or a
driver failure, says `THE SESSION ENDED. TAP OK TO SEARCH.`: OK greets the
ESC again and BACK opens the leave page. A verified write with DIRECTION at
3D or 3D REVERSED sets the tester's 3D note, one at NORMAL or REVERSED
clears it, and a half-written page with 3D in its draft sets it.

A session that ends with the page half written, by a driver failure, a
heartbeat stop, a frame the guard refuses after the erase, a READ AGAIN cut
that way, or the guard's second BACK, leaves a page the next session reads as
it is: most likely empty or with lost tags, which it does not write, or, with
marker and tags intact, as a writable page that is not the old one. The
result's second line then reads `SESSION ENDED: RESTORE ON A COMPUTER.`, and
with no backup on a card the backup line reads `NO BACKUP: OLD PAGE ON THIS
SCREEN.` in amber. The session's first read is kept for the rest of the visit,
the first such page only: `OLD PAGE` (`Am32Ctl::OldPage`) on the connect page
opens its bytes as ESC HEX, in amber under `OLD PAGE, BEFORE THE CUT WRITE:`
over `ON THIS SCREEN ONLY: COPY IT.` or `ALSO ON THE CARD: BACKUP BJ018`, and
an EFM8 list's ESC HEX shows them under the page's own.

Why a cut write loses the tags, and the way back, are in
`docs/design/bluejay-blheli-s.md`: a power cut between the erase and the
end of the program leaves the firmware to store its defaults with the tags
0xFF at its next start; the next session reads `ESC LOST ITS TAGS:
READ-ONLY.`, and the tags return with a firmware flash on a computer.
Keep the ESC powered after a cut write. None of this is verified on an ESC.

**BACK from the list sends LEAVE** (that note's decision 3): the frame
`00 01`, which starts the ESC's firmware, 500 µs of idle line, 300 ms of low
line, then the line parked. The page reads `LEAVING` over `THE ESC STARTS
ITS FIRMWARE ON` / `LEAVE, THEN DSHOT RETURNS.` and, in amber,
`KEEP THE PROPELLER OFF.`; BACK is drawn disabled. Once LEAVE went out the
job lets SIG go and DShot returns, with no leave page, and the screen
returns to MENU. LEAVE has no answer: whether the firmware started is not
seen, and not verified on an ESC, so MENU's run cue stays until an eRPM
frame after it. A LEAVE that did not go out opens the leave page below.

An entry or a read that ends without the page, the ESC silent, another
answer or a stop, closes the job; the claim keeps its leave page, the AM32
transport takes SIG again, and the connect cycle greets again under
`THE ESC ANSWERED, THEN STOPPED.` / `SEARCHING AGAIN.` in amber.

Every other BACK after an EFM8 answer, from the connect page while the job
reads, after a page not taken, or after the cycle greets again, opens the
leave page of `docs/design/bluejay-blheli-s.md` decision 8, the leave page
of SERVO TESTER, KST SERVO and HITEC SERVO, before LEAVE:

| Step | Text | Width |
|---|---|---|
| 1, current until LEAVE | `UNPLUG THE ESC BATTERY, THEN LEAVE.` / `ITS BOOTLOADER WAITS FOR COMMANDS` / `AND MAY READ DSHOT AS ONE.` | 35, 33 and 26 characters: 209, 197 and 155 px from x 20 |
| 2, current after LEAVE | `PLUG IT IN AFTER RESTART DSHOT.` | 31, 185 px |

The strip reads `EFM8 E8B2  SIG GP0`, and BACK is drawn disabled.
**STAY** returns to the page BACK left, the claim and the job kept: a read
under way goes on. **LEAVE** cuts the job's entry or read, sends nothing and
lets SIG go, or lets the AM32 transport go of it; DShot stays stopped, and
**RESTART DSHOT** takes the wide button, live 1000 ms later. Its tap
restarts DShot, clears MENU's run cue, since the steps power-cycled the ESC,
and returns to MENU. There is no left button after LEAVE. An AM32 session in
which no EFM8 bootloader answered has no leave page: BACK gives DShot the
pin at once.

The job (`src/escwire_job.h`) stops a running entry, read or write with
`ew_session_stop()` when core0's heartbeat lapses for 250 ms, and lets a
running LEAVE finish: a write that has sent its erase runs to its read-back,
starts no other procedure, and then parks the line. A board restart while an
EFM8 bootloader waits sends DShot into it: the bench cannot know.

### Layouts outside 2 to 4

The field table is checked against layout revisions 2 to 4
(`AM32_LAYOUT_MIN_KNOWN`, `AM32_LAYOUT_MAX_KNOWN`): layout 1 is AlkaMotors'
firmware before 1.78, which reads nothing above byte 0x29, and 4 is the
newest AM32 v2.22 writes. A plausible block in layout 1 or 5 to 8 opens a
read-only view (`S_VIEW` in `src/ui_am32.cpp`): the 48 bytes in hex, 8 to a
line, and no field, since a layout the table does not know may have moved
any of them (`am32FieldShown()`). The strip reads, in amber,
`LAYOUT 5 NOT KNOWN: READ-ONLY.` over `NOT VERIFIED. BACKUP AM001`, or
`NOT VERIFIED. NO CARD: NO BACKUP.` without a card or with the card held by
USB, or `NOT VERIFIED. BACKUP FAILED.` when the card refuses it. Under the
bytes, in grey: `THE BYTES AS READ. THE FIELD TABLE` /
`IS NOT CHECKED FOR THIS LAYOUT.`

The backup goes to the card as the view opens, since no write follows. The
view's one control is BACK: no row, no `WRITE...`, REVERT or HEX, and the
screen has no transition from the view to the write. The view keeps the title
`ESC SETTINGS`, and its BACK goes to MENU. The write itself checks the
layout of the bytes as read before anything else (`writeRefused()`), so a
way to it would still write nothing: `LAYOUT NOT KNOWN: NOT WRITTEN`.

### Editing

| Gesture | Effect |
|---|---|
| Tap a row | Selects it; the editor bar at the bottom shows its name and value |
| **Swipe the row sideways** | **Coarse**: one coarse step per 16 px of travel; 11 steps, 176 px, cover any field's range |
| `-` / `+` buttons | **Fine**: one step per press; hold to repeat, accelerating |
| Drag up/down | Scrolls the list 1:1 with the finger |

A press selects nothing on its own. It becomes a selection on release if the
finger did not move, or when a sideways swipe is recognised, so a swipe adjusts
the row it started on. A vertical drag never selects.

The axis locks after 6 px of travel (`UI_AXIS_LOCK_PX`), so a gesture is
either a scroll or an edit. Scrolling is measured from the touch point, so no
travel is lost to the lock. A coarse step (`UI_SWIPE_STEP_PX`, 16 px) is
⌈range ÷ (11 × step)⌉ × step: 24 for a field of 0 to 255 at step 1, 4 for
motor poles at step 2. It is made of the field's own steps, so a value on the
field's step stays on it, and an even pole count stays even; a value read off
the step moves by the step from where it is. A coarse step that would pass
the end of the range stops at the end. Travel past the end is not stored, so
the first coarse step back moves the value off it.

A value that differs from the ESC as read is cyan, with a 3 px cyan bar at
its row's left edge; the editor bar's value is cyan too, and the bar's top
line names the value as read: `PWM FREQ  24KHZ ->` over the new value, or
`24KHZ ->` alone where the label and the value do not fit the 140 px between
`-` and `+`. Selecting a row fills it and lights its label, and leaves the
value's colour as it is: a value as read is in the text colour. **REVERT**,
labelled in amber, restores the values that were read. **HEX** shows the raw
48 bytes for checking a field's decoding, titled `ESC HEX`; its BACK
returns to the list.

BACK on the list with edits not written arms a guard (decision 7 of
`docs/design/menu-b.md`): the strip reads, in amber,
`2 CHANGES NOT WRITTEN.` over `BACK AGAIN DISCARDS THEM.`, or
`1 CHANGE NOT WRITTEN.` over `BACK AGAIN DISCARDS IT.` A second BACK goes
to MENU and drops the edits; a press anywhere else disarms the guard and the
strip returns. There is no timer. Without edits BACK goes to MENU at once.

**WRITE...** opens `ESC WRITE` on a tap; nothing on the list is held. The
page lists each field that changes, from the ESC as read to the edit: the
label, then `old -> new`, the old value grey and the new one cyan, at most 4
lines and then `AND n MORE`; the strip counts them, `2 CHANGES`. A line at
the bottom names the backup: `BACKUP FIRST: BACKUP/AM001.TXT` before the
session's first write with a card in, `BACKUP: BACKUP/AM001.TXT` once it is
on the card, and in amber `NO CARD: OLD VALUES LOST ON LEAVING`, or
`USB DRIVE: OLD VALUES LOST ON LEAVING` while USB holds the card: without a
card the settings as read stay in RAM until a write with a card saves them,
and leaving ESC SETTINGS loses them. Writing needs a 1 s hold on the page's
`HOLD TO WRITE`, the wide button; the tap that opened the page is not timed.
While the write runs the page reads `WRITING: DO NOT UNPLUG.` in amber and
BACK is drawn disabled. BACK before the hold, or under the result, returns to
the list. While the BOARD voltage is red the hold writes nothing: the result
reads `BOARD VOLTAGE IS RED: NOT WRITTEN.` with `CHARGE THE CELL. TAP OK,
WRITE AGAIN.` under it, both amber, and OK returns to the list with the edits
kept ([Using it](#using-it), the BOARD voltage). OK takes HOLD TO WRITE's
place in the wide button; it takes a fresh tap, and lifting the finger that
held HOLD TO WRITE leaves the result up. After the write the firmware waits
`AM32_WRITE_SETTLE_MS` (300 ms), reads the block back, and compares it byte
for byte. It shows `WRITE VERIFIED`, in green, only on a full match, with
`POWER-CYCLE THE ESC TO APPLY.` under it: the bench sends no command that
starts the ESC's firmware, and whether the ESC runs the new settings without
a power cycle is not tested. On a mismatch it shows the first differing
byte: `DIFF @xx: GOT xx WANT xx`. The result is drawn at scale 1 from x 8,
y 196, under the page's change lines; every result fits one 37-character
line. A result that is not a verified write is amber: under `WRITE FAILED:
...`, `VERIFY READ: ...` or `DIFF ...` a second amber line reads `NOT
VERIFIED. TAP OK, WRITE AGAIN.`: OK returns to the list with the edits kept,
and `WRITE...` and the hold write again. Under `NO BACKUP ON THE CARD: NOT
WRITTEN` it reads `CHECK THE CARD, THEN WRITE AGAIN.`. The page writes what
it shows: settings that differ from those it opened on are refused,
`CHANGED SINCE THE PAGE: NOT WRITTEN`, and a page that lists no change draws
HOLD TO WRITE disabled; no path through the screen reaches either.

Some values put two amber lines on the strip in place of the ESC name
(`listNote()` in `src/ui_am32.cpp`): the selected row's first, then 3D
MODE, then PROTOCOL, then firmware below 1.80. The facts behind them are
from the AM32 source, not from the bench: am32-firmware/AM32 at the tags
v2.05, v2.16 and v2.18 to v2.22 and `main` at 077b6a4, the AM32
configurator at 06955fe, and for firmware below 2.00 the tags v1.77 to v2.01
of AlkaMotors/AM32-MultiRotor-ESC-firmware and its commits 9909ea7, f917c01
and aa590e8. The
firmware version is bytes 0x03 and 0x04 (`am32Firmware()`); a major outside
1 to 9 or a minor above 99 counts as 0, the oldest.

| Value | Strip | AM32 source |
|---|---|---|
| `3D MODE` on, byte 0x12 | `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` | `bi_direction`; `setInput()` runs 48..1047 one way and 1048..2047 the other. The tester sends 47 + throttle (1..2000) |
| `TEMP LIMIT` selected, `OFF` (141) | `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` | `loadEEpromSettings()` sets a value outside 70..140 to 255, no limit |
| `CURRENT LIMIT` selected, `OFF` (0, 101, 102; 100 below 2.21) | as TEMP LIMIT | the limit acts for 1..100 from v2.21 (`<= 100`), for 1..99 in v2.05 to v2.20 (`< 100`), in 2 A steps |
| `LOW VOLT CUTOFF` selected, `OFF` or `? (2)` | as TEMP LIMIT | 0 is off; 1 per cell; 2 absolute, which trips from v2.21 (`absolute_voltage_cutoff * 50`). v2.19 and v2.20 compare the raw byte with the voltage in 10 mV, so it never trips; v2.18 and older act on 1 alone |
| `PROTOCOL` at 2 to 9 | `THE TESTER SENDS DSHOT ONLY:` / `THE ESC WILL NOT RUN HERE.` | `enum inputType`, zero-based: 0 AUTO, 1 DSHOT, 2 SERVO, 3 SERIAL, 4 EDT ARM, 5 DRONECAN. SERVO detects servo pulses only. SERIAL, 5 and 6 to 9 leave `EDT_ARMED` at 0, and `dshot.c` takes a throttle above 47 only with it set; 10 and above are AUTO. EDT ARM sets it on the EDT enable (command 13, after 6 frames, armed and stopped) and clears it on every zero frame; the tester sends the enable only while disarmed (`escFrameAction()`) and zero frames after it, so the throttle is dropped too. The editor offers 0 to 4, as the configurator does |
| Firmware below 1.80, or a version byte AM32 did not write | `FIRMWARE BELOW 1.80 OR UNKNOWN:` / `FIELDS IT MAY NOT READ ARE HIDDEN.` | A field the firmware does not read is not listed (`am32FieldShown()`, `Am32Field::readFw`): 0x2A..0x2D from 1.80 (`AM32_FW_READS_2A`; AlkaMotors commit 9909ea7 adds the reads and raises VERSION_MINOR 79 to 80), 0x2E from 1.94 (`AM32_FW_READS_2E`; aa590e8, 93 to 94), 0x2F from 2.16 (`AM32_FW_READS_2F`; the tags v2.05 to v2.12 do not read it). A tag may store a lower version than its code (v1.99 stores 1.95), never a higher one. At 1.80 CURRENT LIMIT acts on 2..99, from 1.81 (f917c01) on 1..99. The backup file lists the fields the firmware reads; its hex lines hold every byte |

After a list read with 3D MODE on, or a write of 3D MODE on that the ESC
accepted, verified or not, the tester shows
`3D MODE ESC: 0-50% REVERSED AT 2X` (`esc3dNote()`, set and cleared from
`tryConnect()` and `doWrite()`;
[Telemetry and logging](#telemetry-and-logging) says where it is drawn).
Arming is not refused on it.

### Backup

Before the first write of a session, the settings block as read at entry goes
to the card as `BACKUP/AMnnn.TXT` ([Device backups](#device-backups)). The
read-only view saves it as it opens. A block in a layout outside 2 to 4 is
saved with no field line and the line
`Layout not known: fields not read, not verified`. The
result line then reads `WRITE VERIFIED. BACKUP AM018`, for example. A card
that refuses the backup stops the write: `NO BACKUP ON THE CARD: NOT WRITTEN`.
The melody and the CAN block are not saved; nothing reads the file back yet.

### Protocol

| | |
|---|---|
| Link | one-wire half-duplex serial, 19200 8N1, on the ESC signal pin |
| Init string | 21 bytes: twelve `0x00`, `0x0D`, `"BLHeli"`, `0xF4 0x7D` |
| Commands | `SET_ADDRESS 0xFF 00 hi lo`, `SET_BUFFER 0xFE 00 00 len`, `PROG_FLASH 0x01 0x01`, `READ_FLASH 0x03 len`, `ERASE 0x02`, `RUN 0x00` |
| Checksum | CRC-16/ARC (cyclic redundancy check): polynomial `0xA001`, init 0, appended low byte first |
| ACK (acknowledge) | `0x30`, always the last byte received |
| Settings | 48 bytes, at an address that depends on the MCU (microcontroller) |

The settings address depends on the MCU family. The handshake reply gives the
family in its fifth-from-last byte:

| Type byte | Family | Settings at |
|---|---|---|
| `0x2B` | STM32G071, 2 KB pages | `0x7E00` |
| `0x1F` | STM32F051, 1 KB pages | `0x7C00` |
| `0x35` | STM32F3, 2 KB pages | `0xF800` |

The reply is indexed from the end. A host with a hardware UART on the shared
wire also captures its own transmission, so only the tail is at a fixed
position.

Bytes `0x05..0x0C` hold the device name before layout revision 3, and ramp and
current-PID (proportional-integral-derivative controller) settings from revision 3 on. Fields are gated by revision, so only
the set that applies is shown.

Host and ESC share one conductor, so every transmitted byte is echoed back and
discarded before the reply is read. Interrupts are masked per byte, not per
frame. One byte at 19200 8N1 is 0.52 ms; masking for a whole frame would block
the system for several milliseconds.

### Verification

The host test harness runs two cross-checks:

- Every protocol constant is asserted against a known-working Python
  configurator: init string, all six command encodings, CRC polynomial and
  byte order, ACK position, and all three MCU-to-address mappings.
- The field table is decoded against that tool's default settings blob and
  compared to the meanings in its byte comments: 14 poles, 24 kHz, 15°
  advance, 3.0 V cell cutoff, 1502 µs servo neutral, a 40 A current limit,
  and others. Where the tool's comments and AM32's source disagree, the
  source wins: the tool numbers the protocol from 1, AM32's
  `enum inputType` from 0; its "55 = 2200kv" is 2220 KV (motor velocity
  constant, RPM per volt) in AM32, raw x 40 + 20; and its 141 and 102 are
  the values at which AM32 applies no limit, shown as `OFF`.
- The field table is checked against AM32's `Inc/eeprom.h`, its
  `loadEEpromSettings()` and the configurator's field definitions, at the
  versions named above. Where the accepted range depends on the firmware,
  the field says from which version (`Am32Field::topFw`, `onFw`, `scaleFw`;
  `am32FieldMax()`): ABSOLUTE low-voltage cutoff from 2.21, BY RPM PWM from
  2.18, ACTIVE brake from 2.19, PWM FREQ above 48 kHz from 2.19, a 200 A
  current limit from 2.21, ABS VOLT CUT in volts from 2.21, ADVANCE in the
  10..42 encoding from 2.19 (`AM32_ADVANCE_NEW_FW`; before it 0..3 in 7.5°
  steps, v2.05 main.c:629-633, v2.18 main.c:591). On older
  firmware the editor stops at that firmware's top, and a stored value
  outside the range offered on that firmware, a number included, reads
  `? (n)`, as does an on/off byte of 2 or more. A host test runs every
  field over all 256 byte values on eight firmware versions: a press changes
  no byte of the 48 but its own, and lands inside a literal table of ranges
  per firmware written from AM32's source, on the end of it when it starts
  outside. A screen test writes eight blocks of arbitrary bytes, layouts 2
  to 4, on those firmware versions back through the UI after one press on
  POLES: the ESC holds every other byte as read.

---

## Servo tester

**MENU → SERVO TESTER** drives one servo on SIG, the ESC pin, or two servos on
SIG and AUX, the KISS pin, with servo pulses from the RP2350's hardware PWM. It
does not measure the servo: there is no position feedback, so the horn on the
screen is drawn where the pulse puts it. Not tested on hardware. Design note:
`docs/design/servo-tester.md`; the rules are pure functions in
`src/servo_model.h`, and core1 makes the pulses (`src/servo_job.h`).

![Servo tester](docs/servo-preview.png)

### Wiring

The servo takes power from its own supply. The board supplies signal and
ground only.

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

- The servo's middle wire goes to its supply, never to a connector pin. The
  board's 5 V pin is its USB supply, and a servo can draw more than 1 A at
  stall.
- Power the board before the servo supply, and switch the servo supply off
  before the board. A servo fed while the board is off can drive its signal
  wire into an unpowered pin, rated 3.63 V.
- The two grounds are joined once, at the board.
- SIG and AUX are the pins set under MENU → SETTINGS → SIG PIN and AUX PIN,
  both whatever KISS is. A step of either swaps the two, so SETTINGS never
  puts both on one GPIO, and `settingsValidate()` moves AUX off SIG in a
  stored block. Pins on one GPIO set some other way leave one servo only,
  with `SIG AND AUX ARE THE SAME GPIO.` and
  `SET SIG PIN OR AUX PIN IN SETTINGS.` on the SET screen.

> **Unplug the servo before DShot runs.** Outside the servo screen the ESC pin
> carries bidirectional DShot, which idles high and pulls low for about 27 µs
> per frame at 600 kbit/s, once every 1000 µs. A servo on that pin reads it as
> a pulse of about 970 µs at 1 kHz: it drives towards one end of its travel,
> at a frame rate that overheats an analogue servo. That happens at **boot**
> for a servo left on SIG, which the splash warns of with
> `UNPLUG ANY SERVO FROM GP0 NOW`, and after the servo screen, which therefore
> opens the leave page at `1 UNPLUG THE SERVO.` and restarts DShot only on
> `RESTART DSHOT`, 1 s after the pins are let go.

### Using it

The screen is titled `SERVO TESTER`, its MENU row's label, on the main view and
on the leave page (`SERVO_UI_TITLE` in `src/ui_servo.h`). It claims the pins
(`src/pin_claim.h`). It does not claim while an ESC answers on SIG within
500 ms, because servo pulses of 1000 µs to 2000 µs are a valid PWM throttle
signal; it claims by itself 500 ms after the ESC goes quiet. That check misses
an ESC that does not answer in bidirectional DShot, so the screen says
`PULSES ON GP0 ARE THROTTLE TO AN ESC.` Two servos also need AUX free: a KISS
frame within 500 ms, or SIG and AUX on one GPIO, leaves one servo only, with the
reason on the SET screen. While the screen waits for the pins, for example while
an ESC answers, it cannot know whether AUX is free, and TWO can be chosen. If
the claim then finds AUX refused, the screen takes SIG alone and goes back to
one servo, and TWO stays off until the screen is opened again. A TWO still
waiting for `HOLD TO APPLY` goes back to one servo too (`claimPins()`); the wait
is decided again, so one that held only for TWO ends and the rest of the change
applies.

| Control | What it does |
|---|---|
| `SAFE` chip | The pins are bound to PWM at level 0: no pulses, the line low |
| `HOLD TO START` | Hold 1 s (`ARM_HOLD_MS`). Every bound pin goes to CENTRE at once, whatever was commanded before. The chip reads `RUNNING` |
| `STOP` | Acts on touch. The pulses end from the next PWM frame; the last one ends within one frame, 20 ms at 50 Hz |
| The horn | Press on the dial and drag: the servo goes to the angle under the finger, −90° MIN, +90° MAX. Release holds. Refused while SAFE; a press that starts off the dial never drags |
| `MIN` `CENTRE` `MAX` | Act on release, while RUNNING |
| `SWEEP` / `STOP SWEEP` | A square, sine or triangle about the centre, run on core1; a stopped sweep leaves the servo where the curve was |
| `SPEED` | A slew, 10 % to 100 %: at 10 % a full MIN to MAX travel takes 5 s; 100 % is no slew |
| `A` `B` `AB` | Two servos: what the horn and the buttons drive. With `AB`, REVERSE B mirrors B. A's horn and letter are lime, B's blue |
| `SET` | Profile, rate, range, servo count, REVERSE B and the sweep |

If the screen stalls for 250 ms (`UI_HEARTBEAT_TIMEOUT_MS`), core1 ends the
pulses and the sweep, the chip reads `STALLED`, and nothing restarts until the
next HOLD TO START.

The strip's caption names the pin by role and number, the frame rate and
the travel, `SIG GP0  50 HZ  1000-2000 US`, and `SIG+AUX  50 HZ  1000-2000 US`
with two servos. Where that would run within 4 px of the widest chip,
`RUNNING` at x 179, at a three-digit rate with a four-digit MIN, the pin goes
by its role alone, `SIG  333 HZ  1000-2000 US`, and with two servos the
travel goes, `SIG+AUX  333 HZ`. While the screen takes the pins the dial band
reads `RELEASING SIG GP0`, then `TAKING SIG GP0`; a refused bind reads, for
example, `SIG GP0 COULD NOT BE TAKEN (PWM)`.

While SAFE with one servo, the readout row reads `TWO SERVOS:` and
`SET > SERVOS` on the right: SET's SERVOS row switches the second servo on.
The hint is not shown while the claim refuses AUX.

**Switched supply.** With SETTINGS → SERVO SUPPLY at `GP28` or `GP29` the
servo tester switches the servo's supply, the boost converter of part C of
the [wiring diagram](#wiring-diagram) (decision 20 of
`docs/design/servo-tester.md`, built as slice 3 of
`docs/design/current-sense.md`). Not tested on hardware.

| State | Enable pin | Pulses | Strip, second line, amber |
|---|---|---|---|
| SAFE | low from the claim on | level 0 | `GP28 SUPPLY ON AT START` |
| HOLD TO START | high at once | level 0 until the supply is up | `GP28 SUPPLY COMING UP` |
| Supply up | high | CENTRE, then what the controls command | `GP28 SUPPLY ON` |
| STOP, on SERVO or SET | low in the same core1 pass as level 0 on the pins | level 0 | `GP28 SUPPLY ON AT START` |
| A heartbeat lapse of 250 ms | low | level 0, `STALLED` | as SAFE |
| A cut of the current sensor (below) | low in the same core1 pass as level 0 | level 0, the cut's chip | as SAFE |
| BACK | low at once; an undriven input after the last pulse, before the pins return to DShot | level 0 | |

- The supply counts as up at the first reading of the current sensor taken
  more than 18 ms after the switch-on, one conversion cycle of the sensor
  (`SUPPLY_READ_LAG_MS`), at 4.5 V or more (`SUPPLY_ON_MIN_MV`), with CURRENT
  SENSE at `INA219`, or 500 ms after the switch-on with `NONE`
  (`SUPPLY_START_MAX_MS`). With the sensor and no such reading within
  500 ms the supply goes off, the pulses never start, and the dial reads
  `SUPPLY NOT UP: NO 4.5 V IN 500 MS.` over `CHECK THE CONVERTER ON GP28.`
  The 500 ms is a value of the design note until bench test B6 measures a
  converter's start-up time. SET shows the time of the last switch-on to
  that reading, `SUPPLY UP IN 25 MS`, an upper bound by one read and one
  conversion cycle, until the first timed step takes the line (Move timing,
  below).
- While the supply comes up the pulses wait at level 0 and MIN, CENTRE,
  MAX, SWEEP and the dial are drawn disabled; STOP acts.
- The touch controller is the only way to STOP. A touch controller that has
  not answered for 250 ms (`touchAnsweredMs()`), or the shared I2C bus marked
  stuck by the sensor, switches the supply off while RUNNING: the chip reads
  `STALLED`, the dial `STOPPED: TOUCH DID NOT ANSWER` or
  `STOPPED: TOUCH BUS STUCK`. Either refuses HOLD TO START while it lasts;
  with no trip the dial reads `START WAITS: THE I2C BUS IS STUCK.` or
  `START WAITS: TOUCH DOES NOT ANSWER.`
- With CURRENT SENSE at `INA219` and the part not found, HOLD TO START is
  drawn disabled and the dial reads `START WAITS FOR THE CURRENT SENSOR.`
  over `NONE IN CURRENT SENSE SKIPS IT.`
- After a switch-off, with the sensor, a reading above 2.5 V 2000 ms or more
  later turns the strip's line to `GP28 SUPPLY NOT OFF`: a converter without
  output disconnect, or EN held high. It is a warning: EN is already low.
- While SAFE the dial reads `PULSES ON GP0 ARE THROTTLE TO AN ESC,` over
  `AND ITS BEC WIRE GETS THE SUPPLY.` in amber.
- With SERVO SUPPLY at `OWN` the screen drives neither GP28 nor GP29, and the
  supply's rules and texts above do not apply.

**Protection.** With CURRENT SENSE at `INA219` the servo tester stops on
the sensor's readings (slice 4 of `docs/design/current-sense.md`,
`src/supply_guard.*`). Each new reading is judged once; a sample already
seen, or one 250 ms old or older, never counts. Not tested on hardware.

| Cut | Condition | Applies | Chip, magenta | Dial |
|---|---|---|---|---|
| Hard | one reading at 3.0 A or more, either sign, or clipped (`GUARD_HARD_MA`) | always | `OVERCURRENT` | `STOPPED: CURRENT 3.0 A OR MORE` |
| Stall | readings above 1.0 A for 400 ms, none at or below it between (`GUARD_STALL_MA`, `GUARD_STALL_MS`) | always | `STALL` | `STOPPED: OVER 1.0 A FOR 400 MS` |
| Collapse | after the start-up reading, readings under 4.0 V for 1000 ms, none at or above it between (`GUARD_LOW_MV`, `GUARD_LOW_MS`) | GP28 or GP29 | `LOW SUPPLY` | `STOPPED: UNDER 4.0 V FOR 1000 MS` |
| Sensor lost | the part not online, or no reading for 250 ms since START (`SENSE_STALE_MS`) | GP28 or GP29 | `NO SENSOR` | `CURRENT SENSOR LOST: SUPPLY OFF.` |

- A cut ends the pulses and the sweep and, with the switch, takes the enable
  pin low in the same core1 pass, before level 0 on the pins. The chip and
  the dial's line stay until the next HOLD TO START; the dial's second line
  reads `HOLD TO START AGAIN.`, or `START WAITS FOR THE CURRENT SENSOR.` while
  a lost sensor refuses START.
- A run of the stall or the collapse rule starts again after a gap of
  250 ms between two readings. A transient fault of the bus, one or two
  failed reads, cuts nothing; three take the part offline, which is the
  sensor-lost cut.
- With `OWN` only the hard and the stall cut apply: the bench knows neither
  the user's supply voltage nor whether it runs through the sensor.
  While RUNNING with `OWN` and the sensor not online, the dial's second line
  reads `NO SENSOR READING: NO CURRENT CUTS.`, dim.
- The stall rule, decision 13's, trips on many servos held at MIN or MAX
  against their end stop; bench test B8 checks it on the owner's servos.
- Beside a chip wider than RUNNING, `OVERCURRENT`, `LOW SUPPLY` or
  `NO SENSOR`, the caption reads by role alone, `SIG  50 HZ  1000-2000 US`,
  and at 333 Hz with one servo `SIG  333 HZ`.

**Move timing.** With CURRENT SENSE at `INA219` the servo job times each
step from the sensor's readings (slice 5 of `docs/design/current-sense.md`,
`src/move_timer.*`, rcbench's `servo_move.c`). A step is `MIN`, `CENTRE` or
`MAX` that changes the pulse. The timing only reads: it changes no pulse
and no cut. Not tested on hardware.

- SET's note band shows the last step on its second line when that line is
  free, in `C_DIM`: `MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`, at most 37
  characters. MOVE is the time from the command to the reading that shows
  the arrival, to 10 ms; PEAK the highest reading from the command to the
  arrival; HOLD the mean of the readings of the 500 ms after the arrival
  (`MOVE_HOLD_MS`), at least 10 of them (`MOVE_MIN_READINGS`). `LATE` in
  MOVE's place is movement and no arrival within 3061 ms; `UNSEEN` no
  movement in that time. A figure whose conditions do not hold reads `--`:
  a step not timed, a move under way, a move ended by STOP, a cut, another
  command or the sensor lost, a hold not yet 500 ms long or with a clipped
  reading in it, a peak or hold below 0. Before the first step after START
  the line shows the supply's start-up time; a two-line note takes the
  line. With CURRENT SENSE at `NONE` no step is timed and the line stays as
  it is.
- A step is timed when the servo has been at rest for 500 ms
  (`MOVE_REST_MS`): from the reading that showed the last timed move's
  arrival or the end of a late or unseen move's window, or 3061 ms after
  any other change of the pulse, START, a drag, a
  sweep, a step at SPEED below 100 % or one not timed. A step at SPEED below
  100 % is never timed: its pulse slews. The level before is the mean of
  the readings of the 500 ms before the command, at least 10 of them, and
  not below -20 mA, which is a reversed sensor; the threshold is the larger
  of 20 mA and 3 times their standard deviation, rcbench's rule.
- rcbench's rules, one reading at a time: movement is the first reading
  more than the threshold from the level before; the arrival is the first
  reading back within 50 mA of the destination's holding level after one
  above it by the threshold, or at an end held harder than the servo moves,
  two readings in a row within 50 mA of that level and of each other. The
  destination's holding level is its level before the last timed step that
  left it this run, else CENTRE's, else the level before. A first visit to an
  end where the servo pushes on a stop therefore reads `LATE`; the next one
  is timed.
- The time is an upper bound on when the current shows the arrival. A
  shunt conversion that straddles the arrival averages moving and holding
  current; the first one wholly after it ends up to one cycle and one
  conversion, 17.55 ms and 17.02 ms, later, and core0 reads it up to 25 ms
  after that: 61 ms in all, one read and two cycles rounded up
  (`MOVE_LAG_MS`). The pulse leaves at the next PWM frame, up to 20 ms at
  50 Hz. The current can show an arrival before the horn stops: a servo
  that eases its drive near the target can fall to its holding current
  while the horn still creeps. How far the time lies from the horn's
  arrival is not measured (bench test B10).

**SET.** The PULSE rows change only while SAFE. TYPE loads one of rcbench's
profiles:

| TYPE | MIN | CENTRE | MAX | Rates | Default |
|---|---|---|---|---|---|
| STANDARD PWM | 1000 µs | 1500 µs | 2000 µs | 50 to 333 Hz | 50 Hz |
| NARROW 760 | 660 µs | 760 µs | 860 µs | 50 to 333 Hz | 50 Hz |
| WIDE | 800 µs | 1500 µs | 2200 µs | 50 to 333 Hz | 50 Hz |
| HELI CYCLIC | 820 µs | 1520 µs | 2220 µs | 50, 120, 200, 333 Hz | 333 Hz |
| HELI TAIL 760 | 410 µs | 760 µs | 1110 µs | 200, 333, 560 Hz | 560 Hz |

MIN, CENTRE and MAX move in 10 µs steps between 400 µs and 2500 µs and keep a
pause of 1000 µs after the longest pulse (500 µs for the heli profiles), so the
rates offered depend on MAX: 333 Hz at most for 2000 µs. A heli profile, or a
rate above 60 Hz, at which an analogue servo overheats, is held behind a warning
until `HOLD TO APPLY` (1 s). So is a TYPE, MIN or MAX step that leaves the
profile past its travel (`servoPastTravel()`: TYPE WIDE, MIN below 900 µs or MAX
above 2100 µs, NARROW 760 and HELI TAIL 760 more than 100 µs outside their own
range) and either changes TYPE or takes MIN below or MAX above the applied
profile's (`servoTravelNeedsHold()`). Its warning is the amber end-stop note,
`PAST ITS TRAVEL A SERVO STALLS AND` / `HEATS. TRY MIN AND MAX HORN-FREE.`; a
heli or 60 Hz warning comes first when both apply. Each step decides again
against the applied profile: a step back inside the margin, or back to the
applied MIN or MAX, ends the wait and applies when nothing else waits. A step
toward the centre, SERVOS TWO to ONE, a CENTRE or REVERSE B change, and a RATE
change up to 60 Hz on a type that is not a heli type apply at once while nothing
waits and join a waiting change otherwise. SERVOS ONE to TWO waits for the hold
when the profile is past its travel, since it drives a second servo there, and
applies at once on a profile within it. HELI CYCLIC's own 820 µs
to 2220 µs are past the travel; its one hold covers both warnings. A value that
waits for the hold is cyan, with a 3 px cyan bar at its row's left edge, until
HOLD TO APPLY; while RUNNING the PULSE rows are grey. BACK from SET with a
change waiting arms a guard (decision 7 of `docs/design/menu-b.md`): the
strip reads, in amber, `1 CHANGE NOT APPLIED.` over
`BACK AGAIN DISCARDS IT.`, or `n CHANGES NOT APPLIED.` over
`BACK AGAIN DISCARDS THEM.`, counting the rows in cyan. A second BACK
returns to the servo screen and drops the change, and SET opens again on the
applied profile; a press anywhere else disarms the guard. With nothing
waiting BACK returns at once. The SWEEP
rows (shape, 0.05 Hz to 5.00 Hz, amplitude 0 % to 100 %, dwell 0 ms to 5000 ms,
endless or 1 to 9999 movements) change at any time and apply to the next SWEEP.
`DEFAULTS`, offered only while SAFE, returns to STANDARD PWM 50 Hz, one servo, a
sine at 0.50 Hz. While RUNNING the footer shows `STOP` in its place, which acts
on touch as `STOP` on the servo screen does. Nothing of the servo tester is
stored: every entry starts at STANDARD PWM 50 Hz.

**BACK.** `BACK` ends the pulses and opens the leave page (`src/ui_leave.h`,
the note `docs/design/menu-b.md`, "The leave page"). It has no step before
LEAVE, so it opens releasing: `1 UNPLUG THE SERVO.` in amber with a 3 px bar,
`DSHOT RESTARTS ON SIG GP0 AND READS` and `AS A 970 US PULSE AT 1000 HZ.`
under it, and `2 RESTART DSHOT.` in grey; the strip names the pins the
screen holds, `SIG GP0  AUX GP1`. Core1 lets the pins go only after the last
pulse has completed: it writes level 0 and waits two frames plus 1 ms, 41 ms
at 50 Hz, before the pins leave PWM, so no pulse is cut short. A profile
change waits the same way before the rebind. The released pins are inputs
with the pull-down on until DShot takes SIG back and, with KISS on, the KISS
receiver takes AUX. Until the pins are let go the page reads
`RELEASING THE PINS` (`RELEASING THE PIN` with SIG alone), and BACK TO SERVO
and RESTART DSHOT are drawn disabled. Then BACK TO SERVO keeps the claim and
returns to the servo screen, SAFE. RESTART DSHOT acts on a press that lands
1000 ms or more after the pins are let go (`UI_HOLD_MS`), so a second tap
right after BACK restarts nothing. BACK on the page is drawn disabled in
every phase and does nothing.

## KST servo programming

**MENU → SERVO PROGRAMMER → KST / CHASERVO** opens KST SERVO, which reads
and writes the settings of a KST or Chaservo servo
over its signal wire, on SIG (GP0 or GP1). Both brands share one programming
protocol and one set of 32 registers. The protocol code is rcbench's
(`lib/rcbench/protocols/kst/`): Manchester-coded frames, half duplex, from
the RP2350's PIO (programmable I/O). Not tested on hardware: no frame of the
line driver has been on a pin. Design note: `docs/design/kst-programming.md`.

![KST servo programming](docs/kst-preview.png)

### Wiring

As for the servo tester: the servo on its own supply, signal on SIG, the
grounds joined once at the board. The servo answers only in the first
moments after its supply comes on, so the supply has to be switchable by
hand: a plug, or a switch in its positive lead.

```
  servo supply, 5 V
     +  ----/ switch /--------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo signal (orange or white)

  SIG GP0 = J4 pin 9 or J5 pin 2, GND = J4 pin 1 or 5, or J5 pin 4
```

- The owner programs every KST servo, HV (high-voltage) servos included, at
  5 V. Other voltages are not tested.
- The power-on order and the unplug rule of the [servo tester
  wiring](#wiring) apply here too.

**Switched supply (optional).** With SETTINGS → SERVO SUPPLY at `GP28` or
`GP29`, the bench powers the servo itself through a boost converter whose
enable input (EN) is on that pin, GP28 at J4 pin 11 or GP29 at J4 pin 12:

```
  LiPo + --fuse-- VIN  BOOST  VOUT -- IN+ [INA219] IN- --  servo +  (red)
  LiPo - ---+---- GND         EN  --+-------------------  GP28, J4 pin 11
            |                       |                     or GP29, J4 pin 12
            |                 10 k pull-down
            |                       |
            +-----------------------+-------------------  servo -  (brown)
            |
  board GND +   one tap: J5 pin 4, or J4 pin 1 or 5
  board SIG --------------------------------------------  servo signal

  SIG: GP0, J4 pin 9 or J5 pin 2
  INA219, optional: SCL J3 pin 1, SDA pin 2, VCC pin 3 (3.3 V), GND pin 4
```

- The converter: a synchronous boost with output disconnect, 5 V out, at
  least 3 A, from one LiPo cell, 3.0 V to 4.2 V (design note
  `docs/design/current-sense.md`, decision 10). The cell is one of its own,
  charged off the bench, with a fuse at the cell. A boost without the
  disconnect passes the cell's voltage, less a diode drop, to the servo
  while EN is low, and does not qualify; nor does a module with no EN. A
  boost needs its input below 5 V: from a pack of two cells or a bench
  supply, a buck or a buck-boost does the same, such as the Pololu
  S13V30F5. The module is not named. The 5 V boost board of
  `hardware/mechanical/models/boost_5v_en.step` names no chip, so its
  disconnect, its EN threshold, its current limit and its start-up time are
  not known until bench tests B6 and B7 of the design note.
- EN takes 3.3 V as high: the enable pin is a 3.3 V output, on GP28 or
  GP29, ADC pins that are not fault tolerant. An external pull-down from EN
  to the supply's minus holds the converter off while the enable pin is not
  driven: 10 kΩ to 100 kΩ for an EN with no internal pull-up. Fitted at the
  converter, it also holds EN low when the EN wire comes off at a
  connector. An EN with an internal pull-up needs a pull-down small enough
  to keep EN below its off threshold and at most 3.6 V at the highest VIN
  used, and at most 0.5 V while the board is off. On one cell, 4.2 V, that
  is at most 0.135 times the pull-up; 10 kΩ for the Pololu S13V30F5's
  475 kΩ (design notes, "Enable and its pull-down" and "Example modules").
- The optional INA219 current sensor sits on the converter's output wire,
  IN+ from VOUT, IN- to the servo's plus, and takes J3
  ([Current sensor](#current-sensor)). Without it VOUT goes straight to the
  servo's plus.
- The converter's output is the lead's middle wire. **An ESC on this lead
  gets the converter's output on its BEC wire**, its own regulated supply.
  The claim's ESC check sees only an ESC that answers bidirectional DShot.
- The enable pin is driven only during a servo tester, KST or Hitec claim;
  outside it the pull-down alone holds EN low. A reset or a power-off of the
  board returns the pin to that state; nothing in the firmware switches the
  supply off if core1 hangs.

### Using it

1. Tap KST / CHASERVO on PROGRAMMER, with the servo unpowered. The tap
   opens KST SERVO, which claims SIG on that frame and, once DShot has
   drained and stopped, searches: it repeats rcbench's
   entry sequence, one attempt every 116.6 ms to 118.8 ms, for 30 s. The
   strip reads `SIG GP0  TRY 12  1.4 S`. With the switched supply, each
   attempt is the supply off for 2 s, then on and the entry at once: 14
   power-ups in 30 s, the strip `SIG GP0  TRY 3  SUPPLY OFF 1.4 S` or
   `SUPPLY ON`. A timed-out switched search reads `NO ANSWER IN 14 POWER-UPS`
   and names the converter, and that a servo on its own supply needs
   `SETTINGS > SERVO SUPPLY AT OWN`. While an ESC answers on SIG the screen
   claims nothing and reads `AN ESC ANSWERS ON SIG GP0.`, the strip
   `CHECKING SIG GP0 FOR AN ESC`.
2. Power the servo up during the search: the screen says `POWER UP` /
   `THE SERVO NOW` in amber, the step to take now (`SWITCHED SUPPLY` with the
   switched supply). The servo enters programming mode,
   and the screen reads its 32 registers, each until 3 of 5 reads agree.
   A powered servo on SIG gets no pulses while the screen is open and may go
   to its failsafe position.
3. The field list shows 27 named fields in four groups: TUNE, PULSE,
   FAILSAFE and LOCKED. The strip's chip reads `PROG MODE` on blue while the
   servo is in programming mode, and `IN DOUBT` on red after a write whose
   outcome is not known. LOCKED fields are read-only and grey. A tap
   selects a field; − and + step it, and a sideways swipe steps it by an
   eleventh of its range. The stepper skips values that break a hard limit,
   for example MAX. DUTY above 98 %. A value that differs from the servo as
   read is cyan, with a 3 px cyan bar at its row's left edge, and the editor
   bar's value is cyan too; selection fills the row and leaves the value's
   colour as it is. A value that breaks a soft rule is taken, with the bar
   in amber and the rule's text in the editor bar. REVERT is labelled in
   amber.
4. A tap on `WRITE...` opens the write page, `KST WRITE` (decision 6 of
   `docs/design/menu-b.md`): the changes, counted on the strip
   (`1 CHANGE`, `2 CHANGES`) and listed 4 at most, then `AND n MORE`, each old
   value dim and each new value cyan, their soft rules, and where the backup
   goes. Only the page holds: its HOLD TO WRITE, 1 s, writes, and the tap
   that opened it is not timed. While the write runs, or waits for the card,
   BACK is drawn disabled and does nothing, so no BACK leaves a servo half
   written; BACK before the hold or under the result returns to the list.
   Before the first frame of the session's first
   write with a card mounted, the image read at entry goes to the card as the
   backup `BACKUP/KSTnnn.TXT`, and the servo as it stands as the record
   `KST/KST.TXT`. Later writes of the session save no backup and write no
   record. The write runs as single-register writes with read-back, then a full
   verify. A verified write removes the record, and so does LEAVE from a servo
   in programming mode that is not in doubt: only the record this session wrote,
   while its card stays mounted and the file holds the bytes the session wrote.
   With a card mounted at the screen's opening, the card's writes queue in
   memory and go to the card while the screen keeps drawing: the write page
   reads `SAVING THE BACKUP TO THE CARD.` and `BLOCKS LEFT: n` until the card
   holds the backup and the record, and the servo is written only then. A flush
   the card does not take stops the write: `THE CARD DID NOT TAKE THE BACKUP
   (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. TAP OK, WRITE AGAIN.` A
   flush fails on a block write that fails or a card swapped since
   (`src/sdio/sd_cache.h`), not on a full card, which FatFs refuses before
   anything is queued. Nothing is sent to the servo; the card is mounted again
   and the first write forgotten, so the next write, `WRITE...` and its hold,
   saves the backup again and then writes.
5. BACK from the list opens the leave page (`src/ui_leave.h`), three
   numbered steps, the current one amber with a 3 px bar:
   `1 POWER THE SERVO OFF, THEN LEAVE.` with `IT STAYS IN PROGRAMMING MODE
   UNTIL` and `ITS SUPPLY IS OFF.`, then `2 UNPLUG THE SERVO.` with
   `DSHOT RESTARTS ON SIG GP0 AND READS` and `AS A 970 US PULSE AT 1000 HZ.`,
   then `3 RESTART DSHOT.` Amber notes under them name what LEAVE loses:
   `2 CHANGES NOT WRITTEN.`, `THE SERVO IS IN DOUBT: RESTORE FIRST.` STAY
   returns to the list, or to the search after a lost servo. LEAVE holds the
   line low for 100 ms and lets the pin go; step 2 is current from the tap
   on, and NEXT SERVO and RESTART DSHOT, in LEAVE's rectangle, are drawn
   disabled until the pin is let go. Then NEXT SERVO keeps the claim and
   starts a new search (`NEXT SERVO: POWER IT AFTER THE TAP.`), and RESTART
   DSHOT acts on a press that lands 1000 ms or more later, so a double tap
   on LEAVE restarts nothing; it returns to MENU. BACK on the page is drawn
   disabled in every phase. With the switched supply step 1 reads
   `LEAVE SWITCHES THE SUPPLY OFF.`: LEAVE switches it off, and the page
   reads `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` until the line closes 2 s after
   the switch-off. Every claim end does the same once the supply was on.
   **HOLD TO REBOOT** (1 s), right of STAY before LEAVE, switches the supply
   off for 2 s and on for 1 s, then enters programming mode and reads the
   servo as it is: the new backup. The pending changes go; without a card
   the backup in memory is lost, and the page says so above the button:
   `NO CARD: REBOOT LOSES THE BACKUP` and `IN RAM.` BACK during REBOOT
   closes as LEAVE does and opens the page at `1 UNPLUG THE SERVO.`; so does
   BACK on a search that has found no servo.
6. With the switched supply, the supply is on only while the screen runs. A
   screen that stalls for 250 ms switches it off once a running operation
   has ended; the servo leaves programming mode and its backup in memory goes.
   The page then reads `THE SCREEN STALLED, SO THE BENCH SWITCHED THE
   SERVO'S SUPPLY OFF.`, and RETRY powers it up and reads it again.
   With CURRENT SENSE at `INA219`, while the supply is on, one reading at
   3.0 A or more, or clipped, or readings under 4.0 V for 1000 ms after the
   start-up reading, switch it off in the same core1 pass, during an
   operation too; the frame on the wire finishes, since the line driver
   cannot cut one. The session is power-cycled as at a stall and the page
   reads `THE SERVO DREW 3.0 A OR MORE, SO THE BENCH SWITCHED ITS SUPPLY
   OFF.` or `THE SUPPLY WAS UNDER 4.0 V FOR 1 S, SO THE BENCH SWITCHED IT
   OFF.`; the supply stays off until RETRY. With the sensor not answering,
   `-- A` on the strip, nothing is cut and the session goes on. Not tested
   on hardware.

| Result | Colour | Meaning |
|---|---|---|
| `WRITE VERIFIED` | green | The servo holds the new values |
| `... NOTHING WAS WRITTEN.` | amber | No register changed |
| `ROLLED BACK: THE SERVO HOLDS ITS OLD VALUES` | amber | A write did not take and was undone |
| `IN DOUBT: ... RESTORE FROM FILES.` | red | The servo state is unknown; only a restore is taken |

**FILES** lists the record first when one is there, then the KST backups of
`BACKUP/`, the latest first, then `KST/*.TXT` by name, the KSTnnn files
together. SAVE AS writes the servo as it stands into `KST/` under the next free
KSTnnn, never over a file, and refuses once `KST/` holds 50 files:
`KST/ HOLDS 50 FILES. REMOVE SOME.` LOAD holds the file against the
servo (type, access classes, hard limits) and puts its values into the list as
pending changes; a refusal names its line, for example
`LINE 14: NOT A VALUE OF DEAD BAND`. A tap on `RESTORE...` opens the restore
page, `KST RESTORE`, whose HOLD TO RESTORE (1 s) writes the image read at
entry back.

The backups go to the ring of `BACKUP/` that every device shares: see
[Device backups](#device-backups).

Without a card, or with the card held by a USB host, the backup stays in
memory, and reading, editing and writing still work. The write page's backup
line then reads, in amber, `NO CARD: OLD VALUES LOST AT LEAVE` or
`CARD ON USB: OLD VALUES LOST AT LEAVE`, and
`NO BACKUP ON THE CARD: LOST AT LEAVE` when the card took the record and
refused the backup: until a later write with a card mounted saves it, the
session's backup is the only copy of the
servo's old values, and LEAVE, REBOOT or a power loss ends it. Such a write
stays allowed, by the owner's decision of 2026-11-14 (STATUS.md, open item
20). A card that is
mounted but takes neither the backup nor the record stops the write before
any frame:
`THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, TAP OK, WRITE AGAIN.`

While the BOARD voltage is red, the write page's HOLD TO WRITE and HOLD TO
RESTORE write nothing, no file and no frame, and the page reads
`BOARD VOLTAGE IS RED: NOT WRITTEN.` and `CHARGE THE BOARD'S CELL, HOLD
AGAIN.` in amber ([Using it](#using-it), the BOARD voltage).

An image that fails rcbench's layout fingerprint is shown and not written:
the write button reads `READ ONLY`, and with no row selected the editor bar
reads `READ ONLY: UNKNOWN LAYOUT`.

**Current limitations**

- With the switched supply, the screen's card calls run in a card phase:
  the card driver's waits keep the supply on for at most 15 s
  (`KST_CARD_PHASE_MAX_MS`) while the screen does not redraw. A card call
  that blocks longer, or blocks outside the driver's waits for 250 ms or
  more, counts as a stall: the supply goes off and the backup in memory is
  lost. A card call while a read, write, search or REBOOT runs gets no
  phase. The card phase is tested on the host only, not on a card.
- The converter's start-up time, the servo's power-on window and the entry
  delay (`KST_SUPPLY_ENTRY_DELAY_MS`, 0 ms) are not measured.
- The first write's card reads run on core0, and the screen does not redraw
  until they end; its writes queue and flush while the screen keeps drawing.
  With no card at the screen's opening the cache stays off, and a card
  mounted later blocks for the writes too. Without the cache the first
  write's card calls take up to 1.0 s on a full card with access and busy
  times of 10 ms, and at the SD specification's full times up to 13.3 s on a
  FAT32 SDHC (Secure Digital High Capacity) card and 16.5 s on an exFAT SDXC
  (Secure Digital eXtended Capacity) card, however many logs `LOGS/` holds.
  Logs an earlier firmware wrote to the card's root slow every lookup in it:
  with 1000 of them, up to 135.0 s. These are FatFs block counts
  times card times (`make -C test kstcount`), not measured on a card.
- The board has no card-detect line, so a card swapped during a session
  is not seen as an unmount. A record on the new card with the same bytes
  as the session's own record, the same servo image, is removed as the
  session's. The strip, the write page's backup line and the `(BACKUP)`
  tag in FILES name the session's backup file, which is on the first card,
  until the next write: a file of that name on the new card carries the tag
  meanwhile. The next write sees the swap and saves the backup to the new
  card first.

---

## Hitec servo programming

**MENU → SERVO PROGRAMMER → HITEC D SERIES** or **HITEC CLASSIC** opens
HITEC SERVO, which reads and writes the settings of a Hitec D series or
classic servo over its signal wire, on SIG (GP0 or GP1). A classic servo needs
SERVO SUPPLY at `GP28` or `GP29`. The protocol code is rcbench's
(`lib/rcbench/protocols/hitec/`): its session, register model, wire and PIO
(programmable I/O) line driver. D series frames run at 115,200 bit/s, half
duplex, and the bench receives the servo's answer without a pull-up
resistor (PRECHARGE: the pin lifts the line for 100 ns before each sample).
The classic family (HS-5xxx, HS-7xxx) needs the switched servo supply:
see [Classic servos](#classic-servos). Not tested on hardware: no byte of
the line driver has been on a pin of this board, and no write plan of
rcbench's has run on a servo. Design note:
`docs/design/hitec-programming.md`.

![Hitec servo programming](docs/hitec-preview.png)

### Wiring

As for KST: the servo on its own supply, signal on SIG, the grounds joined
once at the board. **One servo on the lead**: every frame goes to ID 0, and
the frame carries no other address. rcbench records two D series servos on
one port both taking a frame sent to ID 0. Whether a servo whose own ID is
not 0 answers ID 0 is not measured.

```
  servo supply, 5 V
     +  ----/ switch /--------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo signal (orange or white)

  SIG GP0 = J4 pin 9 or J5 pin 2, GND = J4 pin 1 or 5, or J5 pin 4
```

- No pull-up resistor on the signal wire: the bench's PRECHARGE receive
  needs none, and rcbench's PULL-UP receive is not offered.
- With SETTINGS → SERVO SUPPLY at `GP28` or `GP29`, the bench powers the servo through
  the converter of the [KST switched supply](#kst-servo-programming) and
  power-cycles it for the entry. **An ESC on this lead gets the converter's
  output on its BEC wire**, its own regulated supply; the claim's ESC check
  sees only an ESC that answers bidirectional DShot.
- rcbench's runs used a 5.0 V servo supply. Other supplies are not tested,
  and nothing has run on this board.

### Using it

1. Tap HITEC D SERIES or HITEC CLASSIC on PROGRAMMER. The tap opens HITEC
   SERVO for that family, which claims SIG on that frame: DShot sends zero
   frames for at least 250 ms after the last throttle frame, stops and
   leaves the pin an input with the pull-up on. The strip reads
   `RELEASING SIG GP0 FROM DSHOT` meanwhile. While an ESC answers on SIG
   the screen claims nothing and reads `AN ESC ANSWERS ON SIG GP0: UNPLUG
   IT.` over `DSHOT STAYS ON THE PIN UNTIL THEN.` in amber, the strip
   `CHECKING SIG GP0 FOR AN ESC`; the claim follows once the ESC has been
   quiet for 500 ms, and BACK meanwhile returns to MENU, as it does while
   DShot drains. With `GP28` or `GP29`, and for CLASSIC, the Hitec job and
   the entry start once DShot has stopped: the entry switches the supply
   off for 2.0 s and on again, the strip counting `SUPPLY OFF 1.4 S`, then
   waits for the servo's start, about 1 s. With `OWN`, D SERIES reads the
   servo at once on the supply it has, so the screen stops first: the strip
   reads `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.` in green over
   `POWER THE SERVO UP, THEN TAP READ.` in amber, and READ, the wide
   button, starts the job. No frame goes to SIG between DShot's stop and
   READ, so a servo powered once the screen asks for it never sees DShot.
   The headline of each step, `SUPPLY OFF`, `SERVO STARTING`,
   `READING THE MODEL` or, for a classic servo, `LISTENING`, is amber.
   Once the pin is held, BACK before the servo is in programming mode, the
   wait for READ included, opens the leave page at `1 UNPLUG THE SERVO.`,
   as after LEAVE, and NEXT SERVO there starts a new session in the same
   family. The ESC check runs only before the claim holds: an ESC connected
   after that is not detected.
2. The screen reads the 21 registers, `READING 21 REGISTERS`, `7 OF 21`, and
   keeps this first full read as the session's backup. A servo that does
   not answer gives `NO SERVO ANSWERS` with the decoder's word, for example
   `REPLY: SILENT`, and RETRY.
3. The field list shows rcbench's 20 parameters in four groups: MOTION, TRAVEL,
   PROTECT and INFO. Each value shows its unit and the stored value in brackets,
   `90 % (18)`; the counts of NEUTRAL, the ends and POSITION, ID, SENSITIVITY
   and the INFO numbers show no bracket. A stored value no encoding takes reads
   `RAW 1234` in amber. Read-only rows are grey, a `RAW` value in them amber,
   and cannot be selected. The strip's chip reads `PROG MODE` on blue. A tap
   selects a row; - and + step it to the next value the encoding takes, and a
   sideways swipe steps it by an eleventh of its range. LEFT END stays below
   NEUTRAL, below RIGHT END. A value that differs from the servo as read is
   cyan, with a 3 px cyan bar at its row's left edge; the label carries no mark.
   The editor bar's value is cyan too, and selection fills the row and leaves
   the value's colour as it is. A value that breaks one of rcbench's seven soft
   rules, for example SPEED below 30 %, is taken with the bar in amber and the
   rule's text. REVERT is labelled in amber. Reversing DIRECTION mirrors NEUTRAL
   and both ends, as Hitec's programmer does.
4. ID edits the servo's own ID. The bench addresses ID 0 whatever the
   servo holds, and whether a servo with another ID still answers ID 0 is
   not measured, so one write may leave a servo the bench cannot read or
   restore. The editor bar says
   `BENCH ADDRESSES ID 0. A WRITE MAY LOCK OUT READ AND RESTORE.` and the
   write page `THE BENCH ADDRESSES ID 0 AND MAY NOT READ OR RESTORE THIS
   SERVO AGAIN.`
5. A tap on `WRITE...` opens the write page, `HITEC WRITE` (decision 6 of
   `docs/design/menu-b.md`): the changes, their soft rules
   and where the backup goes, and `KEEP THE SERVO HORN CLEAR.` on the
   session's first write and on a write that moves DIRECTION, NEUTRAL or an
   end point. The horn line, the ID note, `FILE OF ANOTHER MODEL` and
   `MODEL NOT MEASURED` always show when they apply; the changed values,
   counted on the strip (`1 CHANGE`, `2 CHANGES`), a changed ID first, take
   the room left, 4 rows at most, then `AND n MORE`, and soft rules that do
   not fit are counted on the last line, `AND n MORE RULES: SEE EACH ROW.`
   Each change reads `old -> new`, the old value and the arrow dim, the new
   value cyan.
   Only the page holds: its HOLD TO WRITE, 1 s, writes, and the tap that
   opened it is not timed. While the write runs, or waits for the card, BACK
   is drawn disabled and does nothing; BACK before the hold or under the
   result returns to the list. Before the first frame of the session's first
   write with a card mounted, the backup goes to the card as
   `BACKUP/HTnnn.TXT`; before every write, the servo as it is and the
   target go to `HITEC/HITEC.TXT` as the record of the write. The write
   sends the changed registers 20 ms apart, saves, reboots the servo
   (1.0 s) and reads all 21 registers back: about
   1.7 s for one setting, computed from rcbench's timings and not measured.
   The page reads `WRITES SENT: 2 OF 4`, then `READING BACK`. A verified
   write removes its record. While the BOARD voltage is red the hold writes
   nothing, no file and no frame, and the page reads
   `BOARD VOLTAGE IS RED: NOT WRITTEN.` and `CHARGE THE BOARD'S CELL, HOLD
   AGAIN.` in amber; HOLD TO RESTORE is refused the same way
   ([Using it](#using-it), the BOARD voltage).
6. BACK from the list opens the leave page, as KST SERVO's: step 1 reads
   `LEAVE SWITCHES THE SUPPLY OFF.` with `GP28` or `GP29` and
   `LEAVE RESTARTS THE SERVO.` with `OWN`; the notes name
   `2 CHANGES NOT WRITTEN.` and `A WRITE DID NOT END VERIFIED.` STAY returns
   to the list, and READ AGAIN, right of STAY, reads the servo again and
   keeps the pending changes. LEAVE with `GP28` or `GP29` switches the
   supply off, and the page reads `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` until
   the line closes; with `OWN` LEAVE restarts the servo from its saved
   values, and the page reads `RELEASING THE PIN`. Once the pin is let go
   NEXT SERVO starts a session for the next servo as for the first: with
   `OWN` a D series servo waits for READ
   (`NEXT SERVO: POWER IT AFTER THE TAP.`); with the switch the entry starts
   at once. RESTART DSHOT acts 1000 ms later and returns to MENU.
   BACK on the page is drawn disabled in every phase; BACK during the entry
   opens it at `1 UNPLUG THE SERVO.`
7. With `GP28` or `GP29` the supply is on only while the screen runs. A screen that
   stalls for 250 ms switches it off: at once during the entry, else once
   a running read or write has ended. The page then reads
   `THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF.` with
   RETRY. With CURRENT SENSE at `INA219` the hard and the collapse cut of
   KST SERVO apply too: the supply off and the transaction on the line cut
   in the same core1 pass, a write's too, and the page names the cut. Not
   tested on hardware.

| Result | Colour | Meaning |
|---|---|---|
| `n VALUES WRITTEN AND VERIFIED.` | green | The servo holds the new values |
| `NOTHING WAS WRITTEN ...` | amber | No frame reached the servo |
| `THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.` | red | The read-back differs at the named registers |
| `THE WRITE STOPPED AT STEP 2 OF 4. READ THE SERVO BEFORE A WRITE.` | red | Part of the plan ran; the servo state is not known |
| `ALL WRITES SENT; READ-BACK FAILED. ...` | red | Every write went out and the read-back did not end |

After a write that failed once frames had gone out, the image no longer
holds the registers the write touched: the write button reads READ AGAIN
until a read fills them.

**FILES** lists the record `HITEC/HITEC.TXT` first when one is there, then
the Hitec backups of `BACKUP/`, the latest first, then the `.TXT` files of
`HITEC/` by name without case, at most 64. SAVE AS writes the servo as last
read into `HITEC/` under the next free `HTnnn`, never over a file, and
refuses once `HITEC/` holds 50 `.TXT` files, the record left out. LOAD puts
the file's writable values into the list as pending changes; a file of the
other family or of another layout is refused:
`NOT A FILE FOR THIS SERVO: CLASSIC`. A file of another model of the same
layout loads, and the write page names it. The files are rcbench's servo
file text with a CRC-16 (cyclic redundancy check) line, so a file from
rcbench's card loads once copied into `HITEC/`, and a file edited by hand
does not. A tap on `RESTORE...` opens the restore page, `HITEC RESTORE`,
whose HOLD TO RESTORE (1 s) writes the session's backup back.

Without a card, or with the card held by a USB host, the backup stays in
memory, and reading, editing and writing still work. The write page's backup
line then reads, in amber, `NO CARD: OLD VALUES LOST AT LEAVE` or
`CARD ON USB: OLD VALUES LOST AT LEAVE`, and
`NO BACKUP ON THE CARD: LOST AT LEAVE` when the card took the record and
refused the backup: until a later write with a card mounted saves it, the
session's backup is the only copy of the
servo's old values, and LEAVE or a power loss ends it. Such a write stays
allowed, by the owner's decision of 2026-11-14 (STATUS.md, open item 20). A
card that is
mounted but takes neither the backup nor the record stops the write before
any frame:
`THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, TAP OK, WRITE AGAIN.`
With a card mounted at the screen's opening, the card's writes queue in
memory, as for KST, and the servo is written only once the card holds the
files. A flush that fails stops the write with
`THE CARD DID NOT TAKE THE BACKUP (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. TAP OK, WRITE AGAIN.`,
or `A FILE` in place of `THE BACKUP` when the batch held a later write's
files.

A record left by an interrupted write is named while the screen enters and
reads the servo: `A WRITE WAS INTERRUPTED. HITEC/HITEC.TXT HOLDS THE SERVO AS IT WAS.`
LOAD of the record puts that servo back into the list. The session's first
write moves the record into the ring of `BACKUP/` before it writes its own.

A model rcbench has not measured is read and written in the D series
layout, and the write page says so in amber:
`MODEL NOT MEASURED: WRITTEN IN THE D LAYOUT.` The measured D series model
is the D485HW.

### Classic servos

PROGRAMMER offers HITEC CLASSIC (HS-5xxx, HS-7xxx) with SERVO SUPPLY at
`GP28` or `GP29`, since a classic servo enters programming mode only at its
power-up. Under `OWN` it is drawn disabled, with
`NEEDS THE CONVERTER OF WIRING PART C` and
`AND SETTINGS > SERVO SUPPLY GP28/29.`: the switched supply is the DC-DC
converter of part C of the [wiring diagram](#wiring-diagram), and the
setting only names its enable pin. A classic servo drives the signal line
high itself, through about 1 kΩ, to its supply: 5 V nominal from the
converter. SIG is GP0 or GP1, both fault tolerant, rated 5.5 V while the
board is powered and 3.63 V while it is off. The line voltage is not
measured. The classic screens are built and tested on the host:
REPAIR of a checksum that does not sum, `FACTORY VALUES` from the servo's
own factory cells, and the HS-5xxx and HS-7xxx layouts.

**Current limitations**

- The bench sends every frame to ID 0. Whether a D series servo with
  another ID answers it is not measured.
- With `GP28` or `GP29`, the screen's card calls run in a card phase that
  keeps the supply on for at most 15 s while the screen does not redraw, as for KST.
  At the SD specification's full times the first write that keeps an old
  record takes 17.9 s on an exFAT SDXC (Secure Digital eXtended Capacity)
  card, over that cap, and the supply goes off; on FAT32 every Hitec card
  phase takes at most 13.2 s with no logs in the card's root. These are FatFs
  block counts times card times (`make -C test kstcount`), not measured on a
  card.
- A later write whose record the card does not take still runs, on the
  first write's backup; the write page does not say so.
- READ AGAIN drops a pending REPAIR, which READ AGAIN shows as `BAD` again.
- The entry views show no countdown for the servo's 1 s start.
- No classic entry, read or write has run on a servo on this board. The
  classic line voltage is not measured. The pad sinks about 5 mA against the
  servo's 1 kΩ at 5 V, above its 4 mA drive setting; the low level at that
  current is not specified.

---

## S.BUS monitor

**MENU → RX SIGNALS → MONITOR** opens S.BUS MONITOR, which reads the S.BUS
(Futaba's serial bus for receiver channels) output of a receiver on GP1: the
16 proportional channels, the two digital channels CH17 and CH18, the link
state, the frame rate and the error counts. It sends nothing. The receive
path is UART0's receiver on GP1 with the pad's input inverted, at 100,000
baud, 8 data bits, even parity and 2 stop bits (8E2); the decoder is
rcbench's (`lib/rcbench/protocols/sbus/`). Not tested on hardware: no
receiver has been on a pin of this board, and the inversion on the pin is
not measured. Design note: `docs/design/rx-signals.md`; the screen is
`src/ui_sbus.h`, what it shows `src/sbus_view.h`, the core1 job
`src/sbus_job.h`.

![S.BUS monitor](docs/sbus-preview.png)

### Wiring

```
  receiver supply, the receiver's rated input
     +  ----------------------------------------------  receiver +
     -  ----------------+-----------------------------  receiver -
                        |
  board GND  -----------+
  board GP1  ------------------------------------------  receiver S.BUS out

  GP1 = J5 pin 1 or J4 pin 10, GND = J5 pin 4 or J4 pin 1 or 5
```

- GP1 is AUX, the KISS pin, in the default wiring. With SETTINGS → SIG PIN
  at GP1 it is SIG; the monitor reads GP1 in either role, because GP1 is
  UART0's receive pin.
- The receiver takes power from its own supply, never from the board's
  3.3 V or USB 5 V pins.
- Power the board before the receiver, and the receiver off before the
  board. The pin is rated 5.5 V while the board is powered and 3.63 V while
  it is not. The receiver's output level is not measured; S.BUS outputs are
  commonly 3.3 V logic.
- The receiver's servo and ESC outputs stay unplugged: a receiver bound to a
  transmitter drives them whatever this screen does.

### Using it

The RX SIGNALS row opens RX SIGNALS (`src/ui_rx_signals.cpp`), a chooser
with two entries, 228 x 46 px at y 62 and 142, as PROGRAMMER's. It claims
nothing. Its strip names both pins, `SIG GP0  AUX GP1`. **MONITOR** opens
S.BUS MONITOR. **OUTPUT**, an S.BUS output to a flight controller, is drawn
disabled with `NOT BUILT` under it and opens nothing: it is designed in
`docs/design/sbus-output.md` and not built; it waits on a check for an ESC
bootloader on GP0, which needs ESC SETTINGS' Bluejay and BLHeli_S reading
first.
The row is drawn disabled while the pin claim is not free, as the other rows
that lead to a claim.

S.BUS MONITOR claims the role that is GP1 (`src/pin_claim.h`): SIG and AUX
in the default wiring, SIG alone when SIG is GP1. Before the claim it
checks:

- **GP1 is SIG.** The receiver goes onto the ESC's wire. While an ESC
  answers there within 500 ms, the screen does not claim and shows
  `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT. PLUG THE RECEIVER IN`
  `ONCE THE LINK CHIP SHOWS.`; it
  claims by itself 500 ms after the ESC goes quiet. Like the other screens'
  check, it misses an ESC that does not answer in bidirectional DShot. With
  GP1 as AUX an ESC on SIG only loses its frames, and there is no such wait.
  The chip shows once the claim holds and the job runs; until then DShot
  frames go out on GP1, so the receiver waits for it.
- **KISS frames on GP1**, GP1 as AUX: `KISS WIRE ON AUX GP1 IS LIVE.` and
  the same second and third lines; the claim follows 500 ms after the last
  frame.
- **No role on GP1**: `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` On
  this board SETTINGS' two steppers swap GP0 and GP1, so this does not
  happen through the screens.

While the claim drains DShot the body reads `RELEASING AUX GP1`, then
`TAKING AUX GP1` until the job binds. A UART clock the job refuses reads
`AUX GP1 COULD NOT BE TAKEN (CLOCK)` with **RETRY**.

| Part | What it shows |
|---|---|
| Strip | `AUX GP1  100 KBAUD  8E2`, or `SIG GP1` when SIG is GP1, and the link chip |
| Chip | `NO SIGNAL` (amber): no frame and no byte for 100 ms, or none yet. `NO FRAMES` (amber): bytes arrive, no frame the decoder takes. `FAILSAFE` (red): the last frame carries the failsafe bit. `FRAME LOST` (amber): a frame with the frame-lost bit within 250 ms. `OK` (green) |
| 16 rows | the channel number; a bar over the raw 11-bit value, 0 to 2047, with ticks at 1000, 1500 and 2000 µs; the value in µs |
| Line 1 | `CH17 OFF  CH18 OFF`; the mean interval of the last 16 frames and the frame rate, `14.0 MS  71.4 HZ` |
| Line 2 | `FRAMES`, frames since the claim; `LOST` and `FS`, frames with the frame-lost and the failsafe bit |
| Line 3 | `PAR`, `FRM`, `BRK`, `OVR`: bytes with a parity, framing or break error, and overruns. A byte with an error is not decoded |
| Line 4 | `RESYNC`: bytes the decoder threw away looking for a frame; `FOOTER`: frames of the right length with a footer it refuses |

The bars are lime while frames arrive, red while the receiver reports
failsafe, so its failsafe values read at a glance, and dim once no frame has
arrived for 100 ms, keeping the last frame readable. Before the first frame
the bars are empty and the values read `----`. Counts above 9999 show 9999,
frames above 999999 show 999999.

The value is rcbench's `sbus_to_us()`: raw 172 is 1000 µs, 992 is 1500 µs,
1811 is 2000 µs, linear, raw 0 is 895 µs and raw 2047 2144 µs. A flight
controller can read the same raw value as a few µs more or less at the
ends: Betaflight's S.BUS input is widely quoted as 5/8 of raw plus 880 µs,
not checked against its source. The bar is in raw units and does not depend
on that.

**BACK** opens the leave page, as on SERVO TESTER, at
`1 UNPLUG THE RECEIVER.`, with where DShot restarts: `DSHOT RESTARTS ON
SIG GP0.` in the default wiring; `DSHOT RESTARTS ON SIG GP1, WHERE` `THE
RECEIVER DRIVES THE LINE.` when SIG is GP1. The job lets GP1 go at once, with
UART0 off and no inversion left on the pad. **BACK TO MONITOR** keeps the
claim and starts again with every count at 0; **RESTART DSHOT** acts 1 s
after the pin is let go and returns to MENU. **BACK** before the claim
holds returns to MENU with DShot running.

Not verified:

- The receiver's output level and idle level at GP1, and that the RP2350
  reads a 3.3 V output with margin.
- Frames from any receiver, the frame period it sends, and zero parity and
  framing errors over time.
- That the PL011 checks only the first of the two stop bits.
- Core1's pass time with the job: the drain must come within 2000 µs to keep
  a frame whole.
- GP1's level after a receiver is unplugged (RP2350-E9): a latched pad reads
  as `NO SIGNAL` with the break count one higher.

---

## USB bridge

**MENU → ESC CONFIG → USB BRIDGE** lets a desktop ESC tool on the board's
USB serial port reach the ESC's bootloader on SIG at 19200 baud 8N1
(eight data bits, no parity, one stop bit). The board is then a USB-serial
adapter whose TX and RX are joined on the ESC's signal wire: every byte
the PC sends goes to the wire, and every byte on the wire, the echo of the
PC's own included, goes back to the PC. It ports DshotDisplay's bridge.
Slice 2 of `docs/design/usb-bridge.md`; host tests only. No byte of it has
gone to an ESC from this board, and no desktop tool has been run against
it.

![USB bridge](docs/bridge-preview.png)

**Which tools.** Passthrough serves a tool that speaks the bootloader's
frames itself and expects the echo: the AM32 configurator with Direct
ticked, and the Python AM32 configurator DshotDisplay's bridge was written
for. ESC-Configurator and the AM32 configurator's default speak MSP (the
MultiWii Serial Protocol) and the 4-way interface of flight controllers;
they need slice 3, not built. Slice 2 passes their bytes to the wire too,
as a USB-serial adapter would, and they get their own bytes back and no
answer. Which of BLHeliSuite's interfaces work is not verified.

**Wiring.** As for DShot: the ESC on SIG and GND, on its own battery. Then
the board's USB cable to the PC. The PC sees the board's one serial port,
the one the console uses, and the card reader beside it; the bridge adds
no USB interface.

**What happens.**

- The tap on USB BRIDGE disarms and claims SIG for `PinUser::Bridge`: DShot
  drains, stops and lets the pin go, the strip reading `RELEASING SIG GP0`.
  The claim has no check for an ESC: the bridge needs it on the pin. While
  USB DRIVE holds the card there is no claim, and the screen reads
  `EJECT THE USB DRIVE FIRST.`.
- Once the line is bound (`src/bridge_wire.h`), `src/usb_bridge.h` opens:
  it takes the serial port from the console (`usbDevConsole()` in
  `src/usb_dev.h`), so neither the telemetry line nor any other `printf()`
  reaches the port, and drops what the PC queued before, the 64 bytes
  TinyUSB holds at most. The mode reads `WAITING` until the PC's first
  byte, then `PASSTHROUGH`. 2000 ms with no byte either way, or the port
  closed by the PC (DTR, data terminal ready, falling after it was set),
  return it to `WAITING`; the bridge forwards whether DTR is set or not.
- Each PC frame is gathered until 2 ms pass with no byte, or 320 bytes are
  in, and goes to the wire back to back; the bridge never waits for the
  ESC before the next. The ring from the wire, 1024 bytes, absorbs a
  repaint of core0. Bytes the ring could not take count as `LOST`.
- Each frame's first byte is named, for the screen only: `SET ADDRESS`,
  `SET BUFFER`, `READ`, `PROGRAM`, `ERASE`, `RUN`, `KEEP ALIVE`, `INIT` for
  the greeting, `PAYLOAD` for the frame after a SET BUFFER, `FRAME` for any
  other. The frame is forwarded unchanged.
- An echo that differs from the byte sent counts as a collision: the ESC
  drove the line while the bench did, as when the PC sends its next frame
  before the ESC has answered the last. The bytes reach the PC as the wire
  read them, and the session goes on.

**The screen.** The strip reads `SIG GP0  19200 8N1` and a chip,
`PORT OPEN` on lime while DTR is set, `PORT CLOSED` on grey otherwise.
Under it: the mode; the bytes `PC > ESC` and `ESC > PC`, the echo
included; `FRAMES` and `LOST`, `LOST` amber above 0; the last command,
such as `LAST SET ADDRESS 7E00`; and six lines: `OPEN THIS PORT IN A
DESKTOP ESC TOOL.`, `THE ESC RUNS ON ITS OWN BATTERY.`, `PROPELLER OFF.
DSHOT IS OFF.`, `POWER-CYCLE THE ESC IF THE TOOL` `FINDS NO ESC, AND AFTER
A FLASH.`. The counts repaint at most every 250 ms. `THE PC CLOSED THE
PORT.` replaces the first line after a close, and `BYTES LOST: THE BRIDGE
FELL BEHIND.`, amber, the second once a byte is lost. A failed bind reads
`SIG GP0 COULD NOT BE TAKEN (PIO)` with RETRY under it.

**Writes and BACK.** PROGRAM, ERASE and a SET BUFFER payload are
write-class.

| When | BACK |
|---|---|
| a write-class frame waits for its answer, at most 3000 ms | drawn disabled; the first line reads `WRITING: DO NOT UNPLUG ESC OR USB.`, amber |
| before the line is bound: Stopping, or a failed bind | returns to MENU; DShot comes back at once, or once the job has let the line go |
| the bridge running, whatever was sent | the leave page: `POWER THE ESC OFF.`, `RESTART DSHOT.`, after a write with no RUN behind it also the note `FLASH THE ESC AGAIN BEFORE USE.`; `BRIDGE` returns to the bridge, RESTART DSHOT, live 1000 ms after the line is let go, to MENU |

Once the line is bound the ESC may wait in its bootloader whatever the
tool sent: one powered up while SIG is held high enters it, AM32 2.22
enters it after about 2 s without a signal, a tool may send MSP rather
than the greeting, and BACK may come before a greeting's answer. DShot
frames could then pass as its commands. So every BACK of a running bridge
gives the leave page, as the servo tools' BACK does. A RUN counts only as
4 bytes whose last two are the CRC of the first two, and only for the
note. MENU shows `POWER-CYCLE THE ESC TO RUN IT` from the claim's begin,
after a failed bind and a BACK before the bind included, until RESTART DSHOT
or an eRPM frame; a RUN does not clear it, since a garbled frame could read
as one.

BACK sends nothing, no RUN. From BACK on, the bridge reads nothing more
from the PC and drops a frame it was gathering; a frame already on the
wire is finished. The console gets the serial port back once the claim is
free, so a tool still connected then reads the telemetry lines; the
clock's `CLOCK` line is read again from then on, and while the bridge holds
the port a `CLOCK` line goes to the ESC as data. A USB cable pulled from a
board on its own cell leaves the claim as it is: SIG stays at the pad's
pull-up and nothing is sent. On USB power alone the board stops with the
cable, and plugging it in again restarts it with DShot on SIG: power the
ESC off before the cable goes back in.

**The log.** `USB_BRIDGE_LOG` in `src/config.h`, 0 by default, is
DshotDisplay's traffic log as a build option: at 1 every frame and every
run of bytes to the PC is kept with its time in a 4 KB RAM ring and printed
on the serial port as a hex dump after BACK, before the console has the
port again. `src/bridge_log.h` describes the dump.

## Current sensor

An optional INA219, Texas Instruments' current and power monitor, measures
the servo supply's current and voltage. It sits on the high side of the
supply: the current flows through the shunt of the module, 0.1 Ω, between
the supply's output and the servo's plus wire. Not tested on hardware: no
INA219 has been on the board's bus. Design note:
`docs/design/current-sense.md`.

**What it measures.**

- The current through the shunt, from the shunt voltage, 10 µV a step:
  100 µA a step on 0.1 Ω, ±3.2 A at the gain the bench sets (/8,
  ±320 mV). A reading at the end of that range is clipped and reads
  `>3.2 A`; past it the part gives no value.
- The voltage at the shunt's servo side, IN-, against the board's ground,
  4 mV a step, in the 16 V range.
- The bench writes one configuration, 19EFh: the shunt averaged over
  32 samples, 17.02 ms, the bus voltage at 12 bit, 532 µs, both
  continuously. Each reading represents 17.02 ms of the 25 ms between two
  reads.

It does not measure the horn's angle and does not set the supply's
voltage.

**Wiring.** The module's VCC, GND, SDA and SCL go to J3, the connector of
the board's I2C1 bus: SCL to J3 pin 1, SDA to pin 2, VCC to pin 3, 3.3 V,
GND to pin 4, and the module's GND to nothing else. A pigtail goes to the
module's header pins; a Qwiic or STEMMA QT cable puts SCL on the module's
GND. The module is never fed from 5 V: its pull-ups would pull SDA and SCL,
and with them the touch controller, to 5 V. The servo's current goes through
the module's screw terminal only: IN+ from the supply's output, IN- to the
servo's plus wire. The VIN- and VIN+ pins of the module's header stay
unconnected. The address is 0x40, both address pins at GND, the common
module's default (`INA219_ADDR`). Part C of the
[wiring diagram](#wiring-diagram) draws it on the boost converter's output.
The design note's "Electrical" section has the circuit, the ratings and the
bus's pull-ups.

**SETTINGS → CURRENT SENSE.** `NONE`, the default, or `INA219`, in the
SERVO SUPPLY section below SERVO SUPPLY. With `INA219` the bench probes the
part at boot and every 1000 ms while it is not online, and the row's caption
reads `FOUND`, `NOT FOUND`, or `OTHER PART` for a part at 0x40 that is no
INA219. An INA219 left on another configuration by other firmware also reads
`OTHER PART`, which a power-cycle of the module clears. With `NONE` nothing is
sent to 0x40.

**Readout.** With CURRENT SENSE at `INA219`:

- SERVO TESTER: the current at scale 2 right of the pulse, `0.42 A`, and
  under it the bus voltage, `5.02 V`. With two servos on the one supply the
  row reads their sum, `A+B 0.84 A`. A clipped reading reads `>3.2 A` in
  amber. The sweep's count moves to the first line of the dial.
- KST SERVO and HITEC SERVO: on the strip, after the supply's state,
  `SUPPLY ON 0.08 A`.
- SERVO SET: the last step's move timing,
  `MOVE 0.42 S  PEAK 1.21 A  HOLD 0.12 A`
  ([Servo tester](#servo-tester), "Move timing").
- A reading 250 ms old or older, or none, reads `-- A`.

**How it is read.** Core0 reads the shunt and bus registers once per UI
frame, 25 ms, directly after the touch read, so the touch read is never
delayed by the sensor; one core owns the bus and no lock is needed. A
frame's sensor read is 96 clocks of the bus, 240 µs at 400 kHz; the
configuration is read back once a second, and a part found on its power-on
configuration twice in a row is probed and configured again. rcbench's
`sense_bus.c`, copied into `lib/rcbench/shared/sense/`, decides on faults:
three failed transactions take the part offline, two timeouts of 1000 µs in
a row or a line held low mark the bus stuck, and a recovery, 9 clocks on SCL
and a STOP, runs at once and every 100 ms while it is stuck, with CURRENT
SENSE at `NONE` too, since touch and the IMU share the bus. The touch
driver, the IMU driver and the clock's read at start-up (`src/clock.cpp`)
report their own transfers to the same record; an abort with a line still
low 10 µs after it counts as a stuck bus, not as a missing part. The probe
reads the configuration register first and writes a register only when it
reads as an INA219's, 399Fh or 19EFh; another chip
at 0x40 is read once a second, and nothing but the register pointer is
written to it. Each reading is published as a `SenseSample`
(`src/sense.h`), under a lock of its own, for core1, where the servo
tester's job reads the bus voltage back after each switch-on and switch-off
([Servo tester](#servo-tester)) and times the servo's steps from it
(`src/move_timer.*`), and the servo, KST and Hitec jobs cut the supply on its
readings (`src/supply_guard.*`).

## Angle meter

The angle meter reads angles from gravity with the accelerometer of the
board's QMI8658 IMU (inertial measurement unit): a wing's incidence and the
decalage between wing and stabiliser, dihedral, and a control surface's throw
in degrees and as the trailing edge's travel in mm. MENU → TOOLS → ANGLE
METER. Design note: `docs/design/angle-meter.md`. Built and tested on the
host only, against a register model of the part: nothing of it has run on a
board. Which QMI8658 variant is fitted, its address on the bus, its byte
order and its axes on the board are not verified, and the accuracy of every
reading is not measured; the design note's bench tests B1 to B9 are open.

![Angle meter](docs/angle-preview.png)

*TOOLS, the main view after a zero with LENGTH set and SIGN at INVERT, SETUP,
and FLIP CAL with a result.*

**How it reads.** Opening the meter opens the IMU driver (`src/qmi8658.*`):
a probe of 0x6B, a soft reset, the accelerometer at ±2 g and 31.25 Hz with
its low-pass filter, the gyroscope off. Each UI frame, 25 ms, core0 reads one
burst and its time stamp after the touch read and the current sensor, 465 µs
of the bus at 400 kHz. The model (`src/angle_model.*`) averages the samples
in fours, about 7.8 Hz, applies the calibration, maps the chip's axes to the
board's by `BOARD_IMU_AXES`, a placeholder until bench test B3, and takes
`theta`, the turn about the board edge that lies along the axis of the
measurement, and that edge's tilt from the horizontal, into rcbench's IMU
model (`imu.c`) for its filter and still time. The reading is `SETTLED` once
`theta` has stayed within 0.5° for 1000 ms. BACK on the main view writes
CTRL7 00h, which turns the accelerometer off; nothing is sent to 0x6B while
the meter is closed.

**The main view.**

| Line | Shows |
|---|---|
| Strip | The edge along the axis and its tilt in whole degrees, `LONG EDGE  AXIS TILT 3°`, amber from 45°; the chip `SETTLED` (green), `MOVING` (grey), `TOO STEEP`, `STARTING` or `NO IMU` (amber) |
| Angle | Scale 5, in tenths of a degree, from the zero or, before a zero, from level: `NOT ZEROED: FROM LEVEL` or `FROM ZERO` under it. `---` while the model has no reading, at the start and 300 ms after the last frame, and while the axis is tilted 70° or more, with `TURN THE AXIS NEARER LEVEL.` under it. With no part answering at 0x6B, `NO ANSWER FROM THE IMU AT 0X6B.`; with another part there, `NOT A QMI8658: ID 0X3C.` with its WHO_AM_I |
| Travel | With LENGTH set, the trailing edge's travel, LENGTH × sin(angle), at scale 3, and `L 40 MM, HINGE TO TRAILING EDGE`; with LENGTH `OFF`, `SET LENGTH IN SETUP FOR MM` |
| MAX | The largest settled angle each side since ZERO or RESET MAX, negative first, `--` for a side with none, and under it their travel |
| Temperature | The IMU's temperature, and after a zero its change since then, amber from 2.0 °C |
| Footer | ZERO, RESET MAX, SETUP |

ZERO arms on its tap, `ZERO WHEN SETTLED: HOLD STILL`, and takes the zero
from the first settled reading at least 1000 ms after the finger lifts, so
the tap's own movement has passed; it clears MAX. With none within 10 s it
reads `NOT SETTLED IN 10 S: NO ZERO` and the old zero stays. ZERO is drawn
disabled while the IMU is not online. MAX takes settled readings only, so a
knock while the surface moves does not set it. The zero and MAX last for the
visit: BACK drops them, and the next visit starts from level. The screen
shows 0.1° and 0.1 mm; that is the display's resolution, not an accuracy.

**SETUP.** Rows as SETTINGS draws them. A row applies at once; BACK returns
to the main view and saves the four fields while they differ from flash
(`settingsAngleDirty()`).

| Row | Values | What it does |
|---|---|---|
| ALONG AXIS | `LONG`, the default, or `SHORT` | Which board edge lies along the axis the angle turns about: the long edge along a hinge line for a throw, the short one for incidence, dihedral and a hinge line shorter than the board. A change drops the zero and MAX |
| LENGTH, MM | `OFF`, the default, or 10 to 500 | The surface's length from the hinge line to the trailing edge, for the travel. `-` and `+` step on the press and repeat while held |
| SIGN | `NORMAL`, the default, or `INVERT` | Turns the reading's sign and swaps the MAX sides; the zero and MAX stay |
| CALIBRATION | `FLIP` | Opens FLIP CAL. The caption reads `CALIBRATED` or `NOT CALIBRATED` |

The save, `settingsSaveAngle()`, writes the meter's four fields over the
settings as last saved, so a change made on SETTINGS and not saved stays
unsaved. While the fields differ from flash, after a save that failed, the
main view reads `SETUP NOT SAVED TO FLASH.`, on later visits too; the values
stay live until a restart, and SETUP's next BACK tries the save again, with
or without an edit. SETTINGS' HOLD TO SAVE stores them too. The IMU keeps
running on SETUP and FLIP CAL.

**FLIP CAL.** Three poses on a flat, steady surface, level or not, each held
still for 2 s: screen up; turned end for end on the same spot, lifted off and
put back; screen down, then screen up again. The strip names the step,
`STEP 1 OF 3`, beside the main view's chip, two lines say what to do, and the
angle shows under them. While the board lies screen down, touches are
ignored and BACK and AGAIN are drawn disabled, so the glass on the table taps
nothing. The result is the offset of each chip axis in mg and the gain of
the axis along the normal, `OFFSET X 30  Y -20  Z 40 MG` and `GAIN Z 1.020`;
an offset past ±0.2 g or a gain outside 0.88 to 1.12 gives no result. The
1 s HOLD TO KEEP applies it and saves it: `KEPT AND SAVED.`, or
`APPLIED, NOT SAVED TO FLASH.` when the save fails. A new calibration drops
the zero and MAX. AGAIN starts over; BACK returns to SETUP without keeping a
result. The gains of the two in-plane axes stay 1. Whether the flip's limits
and the calibration are right for the fitted part is not measured (bench
tests B5 and B6).

**Settings.** ALONG AXIS, SIGN, LENGTH and the calibration are kept in the
settings block, version 6 ([Using it](#using-it), SETTINGS). SETTINGS does
not show them, and its HOLD DEFAULTS keeps them.

**Pins.** The meter claims no pin: DShot keeps running on SIG, and the ESC
is disarmed as MENU opens. TOOLS and the meter open only while the pin claim
is free, since the meter's save writes flash.

## Device backups

A tool that writes settings into a device saves the device's settings, as
read before the session's first write, to the card first. KST SERVO, AM32
CONFIG and HITEC SERVO do. All backups share one directory and one ring of
50 numbers:

| File | What |
|---|---|
| `BACKUP/KSTnnn.TXT` | a KST or Chaservo servo's image, rcbench's KST text format |
| `BACKUP/HTnnn.TXT` | a Hitec servo's first full read as rcbench's servo file text, or a record of an interrupted write moved into the ring |
| `BACKUP/AMnnn.TXT` | an AM32 ESC's 48-byte settings block as text: one line per field its firmware reads, the bytes in hex, a checksum line |
| `BACKUP/LAST` | the latest number, 3 digits and a line feed |

- The numbers 001 to 050 are one sequence for every device. The next backup
  takes the number after the latest, 001 after 050.
- Once all 50 are taken, the next backup writes over the oldest, whatever
  device it belongs to. 50 servo sessions in a row push out every ESC backup
  before them.
- A file with another name, KST051 and up included, is never written over.
- Without `BACKUP/LAST` the latest number is guessed from the names; a ring
  with all 50 numbers taken then writes over number 001.
- A card that is mounted and refuses the backup stops the write. Without a
  card the write runs, and the backup stays in memory until a write with a
  card saves it, or the tool closes.
- While one of these tools' screens is open, card writes queue in memory and go to
  the card in the order they were made, one block per pass of the main loop,
  and the device is written only once the card holds the backup. A running
  blackbox log stops when the screen opens. Before each batch the card's
  volume serial number is checked, so a card swapped in the meantime gets no
  block of the old card. Before each later write to the device the number is
  read again: a card swapped since the backup is mounted again and gets the
  backup before the device is written.

Blackbox logs and their run summaries go to `LOGS/`, KST's SAVE AS files and
its record to `KST/`, Hitec's to `HITEC/`.

---

## Safety

A motor on a bench can injure, and more so with a propeller fitted.

In the firmware:

- Boots disarmed and sends `MOTOR_STOP` (throttle 0) continuously.
- Arming needs a 1 s hold with the throttle at zero for at least 250 ms.
  Only a press that lands on the button while arming is allowed counts: the
  press that disarms, or one made while a USB host holds the card, does not
  arm however long it stays.
- With LATCH off, the throttle returns to zero on finger lift.
- Throttle ceiling, default 20 %.
- Auto-disarm after 30 s with no touch input (`IDLE_DISARM_MS`). For the last
  5 s, `IDLE ns` replaces the board's battery voltage and its `BOARD`
  caption in the top row. The time counts from the last contact the touch
  controller reports, so a wet screen or something lying on the glass keeps
  the tester armed.
- Core1 sets the throttle to zero if core0 sends no heartbeat for 250 ms.
- Opening MENU disarms. Every tool but SETTINGS opens only from MENU;
  SETTINGS also opens from a tap on the tester's pole count or ceiling, only
  while disarmed, and that tap runs the same disarm first. While a pin claim is
  not free, after RESTART DSHOT until the DShot pump runs again, MENU draws
  the tools that claim the pins and SETTINGS disabled, its strip reads
  `DSHOT RESTARTING ON SIG GP0`, and the tester's two readouts do not act.
- Before DShot gives up the signal pin, to the AM32 editor or for a pin
  change, it sends throttle zero for at least 250 ms after the last throttle
  frame.
- A settings save is refused while armed, because erasing flash stops the
  DShot pump.
- A stored settings block that fails its CRC, version or size check is
  discarded whole and the compiled defaults are used. Fields are never merged
  one by one, so a corrupted byte cannot raise the throttle ceiling. A whole
  block of an older layout, version 2 to 4, is read field by field from
  that layout and validated as a current block.
- Arming is blocked while a USB host holds the SD card.
- After ESC SETTINGS read 3D MODE on, or wrote it and the ESC accepted the
  write, verified or not, and after ESC COMMANDS' 3D ON, the tester shows
  `3D MODE ESC: 0-50% REVERSED AT 2X`; arming is not refused on it.
- ESC COMMANDS sends a command only disarmed, with the motor stopped and
  an ESC that has answered for 1500 ms; a direction or 3D change
  needs a 1 s hold and is saved in the ESC with command 12 in the same
  sequence. BACK is drawn disabled while a request waits or a sequence goes
  out.
- ESC SETTINGS, KST SERVO and HITEC SERVO write a device only from their
  write page: a tap on `WRITE...` or `RESTORE...` opens it, it names every
  change, and its 1 s hold writes. While a write runs, or waits for the
  card to hold the backup, BACK is drawn disabled and does nothing. While
  the BOARD voltage is red the hold writes nothing.
- BACK on ESC SETTINGS' list with edits not written, and on SERVO TESTER →
  SET with a change waiting, names the changes in amber first; a second
  BACK drops them.
- On SERVO TESTER → SET a heli profile, a rate above 60 Hz, a profile taken
  further past its travel and a second servo on such a profile wait for
  HOLD TO APPLY (1 s).
- SETTINGS loads the compiled defaults only after a 1 s hold of HOLD
  DEFAULTS.
- The servo tester starts SAFE, with no pulses; HOLD TO START is a 1 s hold
  that puts every pin at CENTRE, and STOP acts on touch. A hold counts only
  a press that lands on the button while it is live, so a press made while
  the button is drawn disabled starts nothing. Core1 ends the
  pulses if core0 sends no heartbeat for 250 ms. The servo screen does not
  claim the pins while an ESC answers on them, and DShot restarts only on
  the leave page's RESTART DSHOT, 1 s after the pins are let go. With the
  switched supply it is off while SAFE and off with every end of the
  pulses, a silent touch controller and a stuck I2C bus included, and the
  enable pin is an undriven input again before DShot restarts.

Not in the firmware:

- Remove the propeller unless the test needs it.
- Clamp the motor down.
- Wear eye protection and stay out of the plane of rotation.
- Keep a way to cut battery power that does not depend on the touchscreen.

The software interlocks back up these habits. They do not replace them.

---

## Layout

```
CMakeLists.txt          build + pinned dependency versions
pico_sdk_import.cmake   locates the SDK via PICO_SDK_PATH
src/
  main.cpp              core0 main(), core1 launch, @page architecture
  plat.h                millis/micros/delay over the SDK timebase
  config.h              defaults for every tunable setting
  version.h             the firmware version, read by CMake and release.yml
  settings.{h,cpp}      the persisted subset: rules, validation, fallback (pure)
  settings_flash.cpp    the one flash sector those live in
  theme.{h,cpp}         the two palettes and the stroke weights
  board_pins.h          includes the board's pin map
  board_rp2350_touch_lcd_2_8.h  the pin map itself, from the schematic
  board_desc.{h,cpp}    the pins and defaults the drivers and settings read, as data
  esc_task.{h,cpp}      core1 DShot pump, command slot, EDT decode, cross-core state
  pin_claim.{h,cpp}     one screen at a time takes the two signal pins from DShot; the PIN_USERS table
  core1_job.{h,cpp}     the core1 job on the claimed pins: start, bind, step, stop
  dshot/                bidirectional DShot, from rcbench
    dshot.h, dshot_{frame,telem,edt}.c  frame, reply decode, EDT schedule (pure C)
    out_dshot.{h,c}, dshot.pio          RP2350 pin driver: 2 state machines, 16 instructions
  ui.{h,cpp}            the tester (arm/throttle state machine, touch handling) and the navigation
  ui_screen.h           UiScreen, SettingsRow, SettingsSection, UiMenuEntry, UiProgEntry and the control ids
  ui_registry.{h,cpp}   one line per MENU row and per PROGRAMMER, RX SIGNALS and ESC CONFIG entry, and the checks that their texts fit
  ui_menu.{h,cpp}       MENU
  ui_esc_cmd.{h,cpp}    ESC COMMANDS: BEEP 1 to 5, direction and 3D mode, through the command slot
  esc_3d.{h,cpp}        the tester's 3D note, which ESC SETTINGS and ESC COMMANDS set
  ui_servo_prog.{h,cpp} PROGRAMMER: the chooser of the servo programmers
  ui_rx_signals.{h,cpp} RX SIGNALS: the chooser of MONITOR and OUTPUT
  ui_esc_config.{h,cpp} ESC CONFIG: the chooser of ESC SETTINGS and USB BRIDGE
  ui_settings.{h,cpp}   SETTINGS: sections, pins, bitrate, contrast, hold-to-save
  diag.{h,cpp}          the restart reason, decoded from the reset registers, and the uptime
  ui_sdlog.{h,cpp}      SD LOG screen
  ui_widgets.{h,cpp}    shared header, buttons, rows and chips
  ui_list.{h,cpp}       the scrolling list: pixel scroll, axis lock, tap, swipe
  ui_hold.{h,cpp}       the hold-to-act control: 1 s timer, one firing, its bar
  ui_leave.{h,cpp}      the leave page: phases, numbered steps, RESTART DSHOT's wait
  ui_am32.{h,cpp}       ESC SETTINGS: connect, edit, verified write; an EFM8 answer and its leave page
  ui_servo.{h,cpp}      SERVO TESTER: the main view, SERVO SET, the leave page
  ui_slider.{h,cpp}     slider with delta drag, tap-to-set and presets, from rcbench
  servo_model.{h,cpp}   servo profiles, limits, steppers and mappings (pure)
  servo_pwm.{h,cpp}     hardware PWM servo outputs, from rcbench
  servo_job.{h,cpp}     the servo tester's core1 job: pulses, trip, supply
  move_timer.{h,cpp}    the servo tester's move timing from the current: rcbench's servo_move fed per reading (pure)
  ui_kst.{h,cpp}        KST SERVO: the search, the field list, the write, FILES, the leave
  kst_view.{h,cpp}      what KST SERVO shows: rows, values with units, the stepper, the texts
  kst_seek.{h,cpp}      the KST search: the entry sequence repeated until a servo answers
  kst_job.{h,cpp}       the KST programmer's core1 job: search, read, write, leave
  kst_line_port.{h,cpp} the job's KST line, rcbench's RP2350 line driver
  kst_files.{h,cpp}     KST's SAVE AS files, its record of a write and its backups
  ui_hitec.{h,cpp}      HITEC SERVO: the claim, READ, the entry, the field list, the write, FILES
  hitec_view.{h,cpp}    what HITEC SERVO shows: rows, values with units, the stepper, the texts
  hitec_job.{h,cpp}     the Hitec programmer's core1 job: rcbench's session, entry, read, write
  hitec_line_port.{h,cpp} the job's Hitec line, rcbench's RP2350 line driver
  hitec_files.{h,cpp}   Hitec's SAVE AS files, its record of a write and its backups
  sbus_job.{h,cpp}      the S.BUS monitor's core1 job: UART0 on GP1 inverted, rcbench's decoder, the status
  ui_sbus.{h,cpp}       S.BUS MONITOR: the claim of GP1, the channels and counts, the leave page
  sbus_view.{h,cpp}     what S.BUS MONITOR shows: the link state, the bars' ink, the texts (pure)
  bridge_wire.{h,cpp}   the USB bridge's core1 job: the frame slot, the ring, the echo, the counts
  bridge_line_port.{h,cpp} the bridge's line: two PIO state machines on SIG
  bridge_line.pio       ...its transmitter, seven instructions; the receiver is pio_uart_rx.pio
  usb_bridge.{h,cpp}    the bridge's USB side: the port to the frame slot and the ring back
  bridge_log.{h,cpp}    the bridge's traffic log, a build option off by default
  ui_bridge.{h,cpp}     USB BRIDGE: the claim, the counts, BACK's rules, the leave page
  backup_ring.{h,cpp}   the ring of device backups in BACKUP/
  card_dir.{h,cpp}      the card's mount rule, directory scope and file calls
  sense.{h,cpp}         the INA219 current sensor on the shared I2C bus: codec, probe, readings, recovery
  qmi8658.{h,cpp}       the board's QMI8658 IMU on the shared I2C bus: probe, reset, set-up, burst, stop
  angle_model.{h,cpp}   the angle meter's model: frames, calibration, theta and tilt, ZERO, MAX, the flip (pure)
  ui_angle.{h,cpp}      ANGLE METER: the reading, ZERO, MAX, SETUP and FLIP CAL; opens and closes the IMU driver
  ui_tools.{h,cpp}      TOOLS, the chooser behind MENU's TOOLS row
  servo.cmake, kst.cmake, hitec.cmake, rx.cmake, sense.cmake, escwire.cmake, angle.cmake, bridge.cmake  each feature's firmware sources (test/*.mk: its tests)
  ui_input.{h,cpp}      tap-on-release and held-repeat rules, shared (pure)
  rpm_filter.h          display smoothing of the RPM readout
  kv_est.h              the tester's RPM/V and kV estimate, their settle and texts (pure)
  am32_bl.{h,cpp}       one-wire bootloader transport
  bl_answer.{h,cpp}     the greeting's answer classified: AM32, EFM8 or neither (pure)
  am32_eeprom.{h,cpp}   AM32 settings layout, decoding and presentation
  am32_backup.{h,cpp}   the AM32 settings block as a backup file
  gfx.{h,cpp}           RGB565 framebuffer, dirty bands, 5x7 font, 7-seg digits, lines, arcs
  st7789.{h,cpp}        panel init + DMA blitter
  touch.h               capacitive touch interface
  cst328.cpp            CST328 touch controller driver
  kiss_telem.{h,cpp}    KISS ESC telemetry decode (pure)
  pio_uart_rx.{h,cpp}   receive-only UART on a PIO state machine, any GPIO
  pio_uart_rx.pio       ...and its nine instructions
  esc_merge.{h,cpp}     per-field preference between KISS and EDT (pure)
  blackbox_encode.{h,cpp} Betaflight blackbox log writer (pure)
  log_ring.{h,cpp}      ring buffer between encoder and card (pure)
  run_summary.{h,cpp}   a log's run summary: extremes, charge, file text (pure)
  clock.{h,cpp}         the PCF85063A real-time clock: start-up read, the set, kept time, get_fattime()
  clock_line.{h,cpp}    the serial port's CLOCK line command: parser, answers, line reader
  sd_log.{h,cpp}        FatFs writer, lifecycle, drop accounting
  sd_log_summary.{h,cpp} the run summary's .TXT beside its log, over FatFs
  sd_hw_config.c        SD wiring for the FatFs driver
  sdio/                 SDIO card driver, MIT, written from the SD specification
    sd_proto.{h,c}, sd_host.{h,c}  wire formats and card protocol (pure C)
    sd_pio.{h,c}, sd_pio.pio       RP2350 bus: 2 state machines, 32 instructions, 2 DMA channels
    sd_card_glue.c, compat/SDIO/   the SD library's callbacks and the two headers it includes
  usb_dev.{h,cpp}       USB device: CDC serial plus MSC; hands serial input to the CLOCK line
  usb_descriptors.c     USB descriptors
  usb_msc.{h,cpp}       the SD card as a read-only USB drive
  tusb_config.h         TinyUSB configuration
lib/rcbench/            rcbench files copied unchanged: servo sweep, GPIO-to-PWM fold,
                        KST core and line driver, KST file store, Hitec core,
                        S.BUS decoder, sensor bus, escwire core and line driver,
                        control-surface arithmetic, IMU model, WitMotion decoder
tools/
  sdtest/               standalone SD bring-up test, 2.8" only; built locally, never released
  make_previews.py      restitches docs/*.png and wiki/img/ from the test suite's frames
  make_wiring.py        draws docs/wiring.svg from the pin sources; --check fails on drift
  check_wiki.py         wiki link, anchor and image checks
  coverage.py           host-suite line coverage: writes and checks the README and STATUS.md figures
  set_clock.py          sets the board's clock to the computer's local time over USB serial
Doxyfile                API doc config -> docs/html/
docs/                   generated docs + preview images
docs/design/            design notes, kept in step with the code
test/                   host test suites + Pico SDK stubs; rcbench/: rcbench's own C tests
wiki/                   the user manual, mirrored to the GitHub wiki
.github/workflows/      ci.yml (CI), release.yml (Release), docs.yml (Docs: wiki publishing)
```

All firmware source is under `src/`, `main.cpp` included, except the rcbench
files copied unchanged into `lib/rcbench/` with rcbench's own layout
(`lib/rcbench/README.md`): the sweep curve, the GPIO-to-PWM map, the KST
programming core with its RP2350 line driver, the KST file store, and the
Hitec programming core with its RP2350 line driver, servo file text and CRC,
the S.BUS (Futaba's serial bus for receiver channels) decoder, the
current sensor's bus record (`sense_bus.c`), the escwire core for ESC
settings over the signal wire with its RP2350 line driver, which nothing
calls yet, and for the angle meter the control-surface arithmetic
(`servo_rudder.c`), the IMU (inertial measurement unit) model (`imu.c`) and
the WitMotion decoder `imu.h` includes, which the angle model
(`src/angle_model.cpp`) calls. Files include
each other as plain siblings, except two drivers with folders of their own: the
DShot driver in `src/dshot/`, so that it can be compared with its rcbench
original, and the SDIO card driver in `src/sdio/`, whose `compat/` headers have
to keep the paths the SD library includes.

Modules marked *pure* have no hardware dependency. They are compiled into the
host test binary and tested directly, without a board.

### Tests

The firmware's own logic runs on a host computer against stubs in `test/stubs/`
that mirror the Pico SDK signatures:

```sh
cd test && make
```

Needs nothing but a C++17 compiler and a C11 compiler: the DShot core in
`src/dshot/`, the SDIO protocol layers in `src/sdio/` and the rcbench files in
`lib/rcbench/` are compiled as C, as the firmware compiles them, and linked into
the C++ suite. `make run` also builds and runs rcbench's own tests of those
files as C11 programs: `test/rcbench/test_servo_sweep.c` and
`test_out_pwm_map.c`, and the seven KST tests `test_kst_wire.c`,
`test_kst_reg.c`, `test_kst_plan.c`, `test_kst_session.c`, `test_kst_pio.c`,
`test_kst_file.c` and `test_kst_store.c`, and the eight Hitec tests
`test_hitec_wire.c`, `test_hitec_pre.c`, `test_hitec_pio.c`, `test_hitec_reg.c`,
`test_hitec_reg_plan.c`, `test_hitec_session.c`, `test_hitec_session_write.c`
and `test_hitec_card.c`, with `hitec_headers.cpp` built as C++11, `test_sbus.c`
(12 cases), and the twelve escwire tests `test_ew_a.c`, `test_ew_id.c`,
`test_ew_map.c`, `test_ew_guard.c`, `test_ew_session.c`,
`test_ew_session_write.c`, `test_ew_file.c`, `test_ew_file_load.c`,
`test_ew_pio.c`, `test_ew_text.c`, `test_ew_session_text.c` and
`test_ew_file_e32.c` (210 cases), with `escwire_headers.cpp` built as C++11,
the move timing's `test_servo_move.c` (18 cases), and the angle meter's
`test_servo_rudder.c`, `test_imu_angles.c` and `test_witmotion.c`
(25 cases), copied unchanged, and `test_imu.c` (9 cases) and
`test_imu_filter.c` (10), adapted from rcbench's to the cases that need `imu.h`
alone. The SDIO tests run the card protocol against a simulated card. Time is
virtual and counted in microseconds — `millis()` advances only when a test
says so — which makes hold-to-arm, hold-to-write and gesture repeat
deterministic rather than dependent on machine speed. The fake ESC serves a real
settings blob recovered from the reference configurator, so the UI tests operate
on values a real ESC would report. The suite renders 169 screens to PPM
(portable pixmap) images as it runs, one file per `fakeDumpFrame()` name, so a
layout change is visible in the frames:

- five of the splash: as it boots, with no answer from the touch
  controller, after an update reset the settings, the same with no touch
  answer, after an update carried the settings over;
- twelve of the tester: disarmed, armed, live, stale, no ESC, an ESC
  answering at rest, with KISS values, the kV estimate at 79 %, the 3D mode
  note, `SDERR` while armed, the card held by a USB host, high contrast;
- eight of MENU: on arrival, the EDT chip on, off and reading NO ESC,
  SETTINGS' `UNSAVED` and `UPDATED`, DSHOT RESTARTING, AM32's power-cycle
  cue;
- four of MENU → ESC COMMANDS: at rest, a beacon going out, 3D ON sent, and
  no ESC answering;
- two of MENU → SERVO PROGRAMMER: SERVO SUPPLY at `OWN` and at a pin;
- one of MENU → RX SIGNALS, the chooser;
- one of MENU → ESC CONFIG, the chooser;
- eight of S.BUS MONITOR: waiting for the first frame, frames, failsafe,
  the receiver gone, an ESC answering on GP1 as SIG, no role on GP1, and the
  leave page with GP1 as AUX and as SIG;
- seven of MENU → SETTINGS: the top, scrolled to the end, high contrast,
  SERVO SUPPLY at a pin, the CURRENT SENSE row with a sensor found, the
  warning for SIG moved off the boot pin, settings carried over by an update;
- eight of MENU → SD LOG: ready, recording, no card, a card found by RETRY
  MOUNT, the last log's run after STOP, a full card with its run not saved,
  USB DRIVE refused, USB DRIVE serving;
- nineteen of ESC SETTINGS: the connect page, a reply it cannot parse, a
  Bluejay or BLHeli_S bootloader's answer, the leave page after it, the
  list, a field edit, scrolled, ESC HEX, the read-only view, firmware 2.22,
  an older firmware, the 3D mode note, a protection off, the BACK guard,
  ESC WRITE before its hold, written, written with a backup, a write not
  verified, a write refused on a red BOARD voltage;
- twenty-one of SERVO TESTER: SAFE, running, sweeping, two servos, stalled,
  an ESC answering, a refused bind, one GPIO for both pins, the switched
  supply's line, the current sensor's readout, a declared sensor not found,
  the overcurrent and the sensor-lost cut, SERVO SET, SET with a warning, SET
  while running, SET with the supply's start-up time, SET with a move timed,
  SET past the travel, the supply not off, the leave page;
- twenty of KST SERVO: the search, the switched search, the field list, an
  edit, KST WRITE, KST WRITE with five changes, KST WRITE without a card,
  the result, a failed flush, a write refused on a red BOARD voltage, a
  failed read, the servo in doubt, the stalled screen, KST FILES, the
  restore page, and the leave page before LEAVE, with changes not written,
  with the switched supply, counting the supply off, and released;
- sixteen of HITEC SERVO: the wait for READ, an ESC answering, the field
  list, an edit, HITEC WRITE, HITEC WRITE for a loaded file, the ID note, a
  model not measured, a write error, a write refused on a red BOARD
  voltage, HITEC FILES, the repair view, the stall, and the leave page
  before LEAVE, after a failed write, and released.

Failing tests dump their frame too.

The fake clock in `test/fakes.cpp` advances only when a test says so, and
it is not reset between tests, so a test added or removed earlier in the
run shifts it. A screen that draws from the clock is therefore captured
after the test sets the clock: `fakeClockAlign()` moves it to a whole spinner
turn (1800 ms) before the KST search spinner and the ESC SETTINGS connect
spinner, and `fakeClockSetMs()` sets the time since boot to 10000 h before
SETTINGS' UPTIME. The images in `docs/` and `wiki/img/` do not change when a
test is added or removed.

`make previews` restitches the images in `docs/` from those frames. It is a separate
target so a test run does not leave the working tree dirty.

`make coverage` builds an instrumented copy of the suite in `test/cov/` and runs it, and
`python3 tools/coverage.py` writes the figure below from its gcov counters. The
figure counts `src/` only: the copied rcbench files in `lib/rcbench/` are
compiled into the instrumented suite but are not in it.

<!-- coverage:start -->
Host-suite line coverage of `src/` is **98.4%**: 14,370 of 14,598 lines in 91 files. CI fails below 93% in total or below 85% in any file.
<!-- coverage:end -->

`sd_log.cpp` is not linked into the host suite (FatFs and the card driver
come with it), so the logging UI is tested against a fake logger in
`fakes.cpp`. `make lognum` links `sd_log.cpp` alone, over FatFs on a RAM
disk with the card driver faked (`test/sd_log_fatfs.cpp`). The fake is the
observable:
tapping START has to actually reach `sdLogStart()` for its state to change, which covers
navigation, hit-testing and dispatch without production code carrying test-only
accessors.

`test_ui.cpp` `#include`s `ui_am32.cpp` rather than linking it, so tests can read its
file-static state without production code carrying test-only accessors. For that reason the
Makefile does not list `ui_am32.cpp` in `SRCS`. `test_claim.cpp` includes `core1_job.cpp`
the same way, to put fake jobs in its job table, and the Makefile does not list it either;
`test_servo.cpp` includes `ui_servo.cpp`, and `test/servo.mk` does not list it. The servo
PWM driver runs against a register fake in `fakes.cpp` that records each pin's function,
level, pull and input override and each slice's wrap, divider and enable.
The S.BUS job runs against a UART0 fake there: a line of UARTDR words each due
at a time, the 32-entry receive FIFO with its overrun, the divisors of SDK
2.1.1 from a settable `clk_peri`, and a receiver that hears the line only with
GP1 on the UART function and its input inverted, reads each character in the
format set, 8N1 after `uart_init()`, and loads a character caught in its middle
as a framing error. `test/sbus_line.h` puts a receiver's frames on that line,
120 µs a byte, with parity, framing and break faults, cut frames and any
footer. `test_sbus_ui.cpp` runs S.BUS MONITOR on it through MENU and RX
SIGNALS, with core1's job passes every 500 µs between UI frames.

`esc_task.cpp` cannot be linked either, so core0's side of the command slot is
a copy in `fakes.cpp` on the shipped `escCmdAccept()`. A test sets the
readiness core1 would publish and the state of the last sequence; a request
the copy takes reads `Waiting` until then. `test_esc_cmd_ui.cpp` runs ESC
COMMANDS on it through MENU: every control at each readiness, the holds at
999 and 1000 ms, each outcome's lines and the 3D note. `test_esc_cmd.cpp`
runs the slot's own rules, pure in `src/esc_task.h`, against model AM32,
Bluejay and BLHeli_S ESCs.

The I2C1 bus is `test/fake_i2c.cpp`: the parts on it by address, the clocks of
each transaction (9 per byte, 2 for a start and a stop, 1 for a repeated
start), and faults on demand: a part absent, the next transfers failing with a
NACK (not acknowledged) or a timeout, SDA or SCL held low, SDA held until the
recovery's clocks. With no part added every address answers with a NACK. Its
INA219 model holds the six registers and their power-on values, RST, the
pointer, the PG (gain) range and the 16 V or 32 V range, the shunt code from a
current and a shunt, the bus code at IN-, and on request the conversion time
the configuration sets, with the shunt code the mean of a current function
over the cycle's shunt conversion. `test_sense.cpp` runs the sensor against it:
the codec at the range ends, the probe, the readings, the faults rcbench's
`sense_bus.c` decides on, the read-back, the sample's 250 ms age across a
`millis()` wrap, the readout's text, and the touch driver, `cst328.cpp`, reading
every frame beside the sensor present, absent and failing. It includes
`cst328.cpp` in a namespace of its own, beside the scripted touch of `fakes.cpp`
the screen tests use. rcbench has no test of `sense_bus.c` of its own; its tests
reach it through drivers that are not copied. `test_supply_guard.cpp` holds the
cuts' rules at each threshold, pure; `test_servo.cpp`, `test_kst_job.cpp`,
`test_hitec_job.cpp` and the screens' suites run them on the INA219 model.
`test_move_timer.cpp` holds the move timing's rules at each boundary, pure;
`test_servo.cpp` runs it through the job and SET with rcbench's servo model,
`servo_sim.c`, behind the INA219 model on its conversion clock, each reading the
mean of the model's current over the shunt conversion.

The I2C fake also holds a QMI8658 model: a register file with the power-on
values, WHO_AM_I and REVISION_ID, the pointer, address auto-increment and the
data registers' byte order by CTRL1, one byte taken from a write of several, the
soft reset with dQY_L after it, no answer for a boot time after a reset, and
with the accelerometer on a sample every 32 ms with a time stamp one higher, one
of them on request in the middle of a burst, and the latched sample's latch
time. On request a failed transfer reaches the part before it fails. It logs
every register access with its time. `test_qmi8658.cpp` runs the IMU driver
against it: the probe's three outcomes, the reset's wait of 15 ms and of 1750
ms, the configuration written a byte at a time and read back, no second reset
within 2000 ms, a probe at once at millis() 0 before any reset and 30 days after
the last, the first sample taken latched after the warm-up, the samples'
decoding, a time stamp repeated or changed during the burst, the faults through
the shared bus record, BACK's CTRL7 00h also after a CTRL7 write that timed out,
nothing to 0x6B while the driver is closed, touch and the INA219 on the same
bus, and the bus time of a frame in each failure mode, at most 1730 µs.

`test_angle_model.cpp` runs the angle meter's model on synthetic gravity
vectors, a board turned about a tilted line and fed as the driver's samples:
`theta` and the tilt for each ALONG AXIS, SIGN and axis map, TOO STEEP from 70°
and amber from 45°, the frames of four samples, stale at 300 ms, SETTLED at
1000 ms, ZERO 1000 ms after the lift and its 10 s limit, MAX from settled
frames, the wrap at ±180°, the travel, the temperature warning at 2.0 °C, the
calibration and the errors it removes, the flip's arithmetic against values
worked by hand and its steps, NaN and degenerate inputs, and the texts in
tenths.

`test_angle_ui.cpp` runs TOOLS and ANGLE METER through the real UI, the IMU
driver and the QMI8658 model on the I2C fake: MENU's TOOLS row and the
chooser; the driver opened by the meter and CTRL7 00h on BACK, nothing to
0x6B before or after; the angle, the strip's tilt, amber from 45°, `---`
and TOO STEEP from 70°, SETTLED and MOVING, a stale reading, no part, another
part and a part lost; ZERO a second after the lift and its 10 s limit, MAX
from settled frames, RESET MAX, the temperature warning; SETUP's rows,
LENGTH's steps and hold-to-repeat, one save on BACK and none without a
change, a failed save; FLIP CAL's steps, touches ignored screen down, HOLD
TO KEEP at 1000 ms and not 975 ms, its save and a failed one; and the widest
texts. `test_settings.cpp` adds the version 6 layout, the carry-over of a
version 5 block, the angle fields' validation at their boundaries and
`settingsSaveAngle()`.

`make pioemu`, which `make` runs too, runs `test/pio_sdio_emu.py`. It assembles
the two SDIO PIO programs from `src/sdio/sd_pio.pio` itself and runs them one
system clock at a time at 150 MHz against a model of an SD card, through a
host model that makes the calls `sd_pio.c` makes. The FIFO (first in, first
out) words come from `sd_proto.c`, compiled for the run, and its checks judge
the results. The scenarios:

- CMD0, which has no response;
- an R1 and an R2 response;
- CMD17, a single-block read whose data start during the response;
- CMD18, a multiple-block read over 3 blocks;
- CMD12, which stops it, and its busy on DAT0;
- CMD24, a single-block write, and its CRC status token.

Each runs at 400 kHz, 10 MHz and 25 MHz, with card output delays of 0, 1 and
2 system clocks, against a fast card and a slow one: 18 runs. A run fails on a
wrong response or block, on the host and the card driving one line at once,
and on a line that changes within 1 system clock of the point where the card
or a state machine samples it. A control run with a card delay of 3 system
clocks at 25 MHz has to fail.

What the model covers, from the PIO chapter of the RP2350 datasheet: the
instructions the programs use, side-set, delays, wrap, the fractional clock
divider, autopull and autopush, 4-word FIFOs, the 2-clock input synchroniser
and the forced instructions `sd_pio.c` issues. What it does not: it is a
model, not proof of hardware timing. It has no analogue behaviour and no trace
delay, its DMA is ideal, it has one card model and no card initialisation, and
its host side follows `sd_pio.c` by hand. The Firmware job in CI holds its
assembler to the SDK's `pioasm`.

`make pioemu` also runs `test/pio_backlight_emu.py`. It assembles
`src/backlight.pio` with the same assembler and runs it one state machine
cycle at a time for every level from 0 to 255, set up as `st7789Init()` sets
it up: the period is 516 cycles (515 at level 0, 20 kHz at a divider of
14.5), the pin is high for 2 × level cycles, and a level written into the
running machine, at a period's start or at 5 points inside one, takes effect
at the next period. It also runs the init's forced `pull noblock` and
`out isr, 32`. The Firmware job holds its words to `pioasm`.

`make pioemu` also runs `test/pio_bridge_emu.py`. It assembles the USB
bridge's transmitter, `src/bridge_line.pio`, and `src/pio_uart_rx.pio` and
runs both on one pad, one state machine cycle at a time, set up as
`src/bridge_line_port.cpp` sets them up: 42 runs. Each byte's start bit,
data bits and stop bit are 8 cycles; the pad is driven from the first start
bit to the end of the last stop bit and let go then, with the next byte late
or not; the bytes of a frame are 82 cycles apart; the receiver hears every
byte sent, its echo, and has it in its FIFO before the transmitter waits for
the next; an ESC's reply 0.3 ms after the release is received at 8 phases
of a cycle with its bits 2 % and 3 % longer and 2 % shorter, with the pad
never driven by both. A control run with bits 3 % shorter has to lose
bytes. The Firmware job holds its words to `pioasm`. `test/test_bridge.cpp`
runs the bridge's job on `test/fake_bridge_line.h`, a wire on the virtual
clock with those cycle counts and a model ESC, and through the pin claim.
`test/test_usb_bridge.cpp` runs the USB side on `test/fake_cdc.h`, the
serial port with 64-byte packets and TinyUSB's buffers: DshotDisplay's
bridge tests ported, models of the Python tool's 25 ms reads and of the
AM32 configurator's Direct rule, the gather, the modes, the write rules
and the console. `test/test_bridge_ui.cpp` runs USB BRIDGE through the UI.
`test/test_bridge_log.cpp` is a program of its own, built with the log on.

### Continuous integration

`.github/workflows/ci.yml` runs one job per row:

| Job | What it catches |
|---|---|
| **Firmware** | Real Pico SDK build of the RP2350-Touch-LCD-2.8 image, pinned by `CMakeLists.txt`, plus UF2 validation, a check that no file of the SD library's SDIO driver is in the image, the third-party notices written and checked for the image, and the PIO emulator's assembler compared with `pioasm` |
| **Host tests** | Protocol, codec and UI regressions a compile cannot see, the SDIO PIO programs run in a cycle-level model against a card model, and the backlight and USB bridge PIO programs in models of their own |
| **Coverage** | Line coverage below 93% in total or 85% in any file, or a README or STATUS.md coverage figure that no longer matches `tools/coverage.py` |
| **Blackbox round-trip** | Encodes a log and decodes it with Betaflight's own `blackbox_decode`, comparing every value |
| **Config permutations** | All 32 combinations of `AM32_PUSH_PULL_TX`, `AM32_FORCE_LOW_JUMP`, `LCD_ROTATION`, `KISS_TELEM_ENABLE` and `SD_LOG_ENABLE`, warnings fatal |
| **Docs** | Undocumented additions, through Doxygen and `test/check_docs.py`; a screen text, view or image that menu structure B removed, named again in README.md, a wiki page or a shot name (`check_docs.py`); and a `docs/wiring.svg` that differs from what `tools/make_wiring.py` draws |
| **Workflow lint** | `actionlint`, plus a duplicate-key check that PyYAML would otherwise let through and GitHub would not |

The firmware job also checks the size against ceilings of 1 MiB flash and 400,000 bytes of RAM (random-access memory), that
the two pin options reach the configure step, and that the options this build
does not take are refused. The check that a pin value reaches the firmware is
in the host job, which rebuilds the suite with the macros overridden.

The permutation job exists because those options are exactly the ones nobody
compiles by hand. `check_docs.py` enforces documentation precisely — public API
(application programming interface) documented, `@param` names matching
signatures, no dangling `@ref` — where Doxygen's `WARN_IF_UNDOCUMENTED` would
also demand a comment on every file-static `s_scroll`.

`docs.yml` publishes `wiki/` to the GitHub wiki after every successful Release run of a
tag without a pre-release suffix, from a tag push or started by hand, so the wiki
describes firmware that can be downloaded. A pre-release such as `v0.1.0-rc1` publishes no
manual. It
renders the screenshots, runs `tools/check_wiki.py`, and can be run by hand for any `v*`
tag; without one it takes the newest tag that has no pre-release suffix.

### Releases

```sh
git tag -a v0.1.0 -m "MiniRcBench 0.1.0"
git push origin v0.1.0
```

The tag names the firmware version in `src/version.h`, 0.1.0 at present, with
an optional pre-release suffix such as `-rc1`. CMake reads the same file, and
the image reports the version to `picotool info` and over USB.

`release.yml` runs the host and documentation checks, builds the `.uf2`,
writes `THIRD_PARTY_NOTICES.txt` for it (see [License](#license)), checksums
both, and publishes a GitHub Release with the image, the notices and the
checksums attached.

Assets per release:

```
MiniRcBench-v0.1.0-touch-lcd-2.8.uf2
THIRD_PARTY_NOTICES.txt
SHA256SUMS.txt
```

The workflow refuses six things:

- **A tag that names another version**: the tag is `v`, the version in
  `src/version.h`, and an optional pre-release suffix. A tag with a suffix
  is published as a GitHub pre-release.
- **Building from failing code**: host tests and documentation checks run first.
- **Publishing an implausible image**: each `.uf2` is checked for the `UF2\n`
  magic and a size that is a multiple of 512 bytes.
- **Publishing without the licences**: `tools/notices.py` fails on code in
  the image that no component covers, and its check fails on a missing or
  empty licence section, in the build job and again before publishing.
- **Inventing a tag**: `--verify-tag` aborts if the tag was never pushed.
- **Leaving a half-populated release**: creating a release and attaching assets
  are separate API calls, so the workflow reads the release back and confirms
  that the image, `THIRD_PARTY_NOTICES.txt` and `SHA256SUMS.txt` are present,
  and nothing else. It downloads them, checks them against `SHA256SUMS.txt`,
  and checks the notices again.

### API documentation

The source is annotated in Doxygen style, and a `Doxyfile` is checked in:

```sh
doxygen            # writes docs/html/index.html
```

`EXTRACT_ALL` is `NO`, so an undocumented addition produces a warning instead
of an empty page:

```sh
doxygen 2>&1 | grep -i warning
```

`EXTRACT_STATIC` is on, because most of this firmware is file-static. The
`@page architecture` block in `src/main.cpp` covers the two-core design. This
README is the front page.

### Rendering

The whole 240×320 RGB565 frame lives in SRAM (static random-access memory):
150 KB of the RP2350's 520 KB. Draw calls record the rows they touched. Only those
full-width bands are sent by DMA, each as one contiguous transfer. Each UI zone
caches the values it last drew and skips drawing when nothing changed, so a
static screen sends almost no SPI traffic.

**Pixel format**: the SPI peripheral uses 16-bit frames for pixel data, so
RGB565 words go out MSB-first (most significant byte first) with no software
byte swap. Commands use 8-bit frames. The panel clock is set to 62.5 MHz
(`LCD_SPI_HZ`); the SPI divider rounds down to the nearest achievable rate.

### The user manual

The [wiki](https://github.com/subtilitas/MiniRcBench/wiki) source is `wiki/`
in this repository, in English and German. `docs.yml` mirrors it to GitHub
after each successful Release run and overwrites edits made in the browser.
Every page says so in its footer.

Its screenshots are not committed. The host suite renders them, one PNG
(Portable Network Graphics) image per captured screen, with the same tool that
builds the previews above. The same tool copies `docs/wiring.svg` into
`wiki/img/`:

```bash
cd test && make wiki      # render wiki/img/, then check every link and image
```

The Host tests job in `ci.yml` runs the same render and check on every push to
`main` or `master` and on every pull request. A renamed page or a dropped
`fakeDumpFrame()` call fails that run. The images are named after the shots,
`shot_prog_chooser.ppm` → `img/prog-chooser.png`, and show the screens as
the suite draws them, so the manual's pictures follow menu structure B.
`test/check_docs.py` fails when README.md or a wiki page names a screen
text, a view or an image that menu structure B removed; the script holds
the list.

---

## Credits

- The DShot implementation in `src/dshot/` comes from
  [rcbench](https://github.com/subtilitas/rcbench), `protocols/dshot` at commit 453cf334,
  by the same copyright holder under the same MIT licence. It is written from the
  published protocol descriptions.
- The SDIO card driver in `src/sdio/` is MIT and written for this project from the
  SD Association's
  [Physical Layer Simplified Specification](https://www.sdcard.org/downloads/pls/)
  v9.10 and the RP2350 datasheet. No other SD host driver was consulted; its design
  note, `docs/design/sdio-driver.md`, lists the sources with section numbers.
- FatFs is by ChaN. The SPI card driver and the FatFs glue are from carlk3's
  [no-OS-FatFS-SD-SDIO-SPI-RPi-Pico](https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico),
  whose SPI driver builds on Arm Mbed OS's SDBlockDevice.
- The KST programming core, its RP2350 line driver and the KST file store in
  `lib/rcbench/` come from [rcbench](https://github.com/subtilitas/rcbench) at
  commit 453cf334, by the same copyright holder under the same MIT licence.
- The Hitec programming core, its RP2350 line driver, the Hitec servo file
  text and its CRC in `lib/rcbench/` come from rcbench at commit 3e29dc1b,
  by the same copyright holder under the same MIT licence.
- The S.BUS decoder in `lib/rcbench/` comes from rcbench at commit 5ef42efd,
  by the same copyright holder under the same MIT licence.
- The escwire core, its RP2350 line driver and the servo file store in
  `lib/rcbench/` come from rcbench at commit 2b4f934a, by the same copyright
  holder under the same MIT licence.
- The control-surface arithmetic, the IMU model and the WitMotion decoder in
  `lib/rcbench/` come from rcbench at commit e98345aa, by the same copyright
  holder under the same MIT licence.
- [logwiju](https://subtilitas.github.io/logwiju/): the browser-based blackbox viewer
  for these logs.
- [DShot — the missing handbook](https://brushlesswhoop.com/dshot-and-bidirectional-dshot/)
- [Extended DShot Telemetry](https://github.com/bird-sanctuary/extended-dshot-telemetry)
- [Waveshare RP2350-Touch-LCD-2.8 wiki](https://www.waveshare.com/wiki/RP2350-Touch-LCD-2.8)
  and [schematic](https://files.waveshare.com/wiki/RP2350-Touch-LCD-2.8/RP2350-Touch-LCD-2.8-Schematic.pdf)

## Legal notice and disclaimer

### Trademark notice

KST and Chaservo are names or trademarks of their respective owners. They
are used in this repository only to say which servos its KST programming
code works with. MiniRcBench is not affiliated with
them and is neither supported nor sponsored by them. The protocol is
implemented from the author's own wire captures of a programming card and a
servo; no vendor code and no vendor documentation is included.

Hitec is a name or trademark of its owner. It is used in this repository
only to say which servos its Hitec programming code works with. MiniRcBench
is not affiliated with Hitec and is neither supported nor sponsored by it.
The Hitec protocol code is rcbench's, written from wire captures of Hitec's
programmers, the HPP-21 Plus and the HFP-30, and of servos. Some register
meanings rest on published documentation or third-party observation;
rcbench's register model marks the source of each (`hitec_src_t`). No
vendor code is included.

Futaba and S.BUS are names or trademarks of their owners. They are used in
this repository only to say which receivers its S.BUS decoder works with.
MiniRcBench is not affiliated with Futaba and is neither supported nor
sponsored by it. The S.BUS decoder is rcbench's; no vendor code is included.

Every other name of a maker, product, protocol or firmware in this
repository is a name or trademark of its owner, used only to describe
compatibility, identify a part or name a reference. MiniRcBench is not
connected with any of them.

### Disclaimer

MiniRcBench is open-source software for self-build. Use, replication and
the programming of servos are at one's own risk. To the extent permitted by
law, the author accepts no liability for:

- damage to servos, for example from a wrong voltage, timing errors or
  corrupted settings data in the servo;
- damage to computers, microcontrollers, ESCs or other connected hardware;
- injury or damage that results from operating the hardware.

The licence terms are in [LICENSE](LICENSE).

## License

This repository's code is MIT throughout (see `LICENSE`), the DShot driver in
`src/dshot/` and the SDIO card driver in `src/sdio/` included.

A firmware image also contains the libraries it links, each under its own
licence:

| Code in the image | Licence |
|---|---|
| MiniRcBench, `src/` | MIT |
| Pico SDK 2.1.1 | BSD-3-Clause |
| `printf` of the Pico SDK (`pico_printf`), by Marco Paland | MIT |
| CMSIS (Common Microcontroller Software Interface Standard) of the Pico SDK, by Arm | Apache-2.0 |
| TinyUSB 0.18.0, from the Pico SDK | MIT |
| `no-OS-FatFS-SD-SDIO-SPI-RPi-Pico` v3.6.2: SPI card driver, FatFs glue, utilities | Apache-2.0 |
| FatFs R0.15, from that library | FatFs licence, a one-clause BSD-style licence |
| newlib, the C library of the Arm GNU Toolchain 13.3.Rel1 | a collection of permissive licences, most of them BSD-style |
| libgcc and the GCC start-up files, from the same toolchain | GPL-3.0 with the GCC (GNU Compiler Collection) Runtime Library Exception |

The SD library's SPI card driver borrows from Arm Mbed OS's SDBlockDevice,
which is Apache-2.0, and its CRC code is partly derived from crcany, which is
under the zlib licence.

The build compiles nothing from that library's SDIO driver, which its README
derives from ZuluSCSI-firmware (GPL-3.0), nor from its ZuluSCSI-firmware and
SdFat directories, and CI checks the image for it. The only GPL-licensed code
in the image is the GCC runtime. Its Runtime Library Exception lets a program
compiled by GCC carry that code under the program's own terms. libstdc++'s
C++ name demangler, `cp-demangle.o`, is libiberty code under GPL-2.0-or-later
with its own link exception. The image does not contain it: the firmware has
no guarded function-level static, so nothing pulls in libstdc++'s exception
code and the terminate handler that calls the demangler.
`tools/notices.py` names the file and its exception if a build links it.

The image as a whole carries no licence of its own, and whoever distributes it
meets each component's conditions. For a binary those are a copy of the
Apache License 2.0 (section 4(a); neither the SD library nor the CMSIS files
of the Pico SDK have a NOTICE file), the copyright notices, conditions and
disclaimers of the BSD-style licences of the Pico SDK and of newlib in the
accompanying documentation, and the MIT notices of TinyUSB, `printf` and this
repository. The FatFs and zlib licences ask nothing of a binary.

Every release carries `THIRD_PARTY_NOTICES.txt`, which holds those texts.
`tools/notices.py` writes it from the image itself: the debug information
(DWARF compile units) of the ELF (Executable and Linkable Format) file names
each compiled source, and the linker map each member of the toolchain's
libraries. Each component's section gives its version or commit as
`CMakeLists.txt`, `release.yml` or the Pico SDK pins it, its files in the
image, the copyright lines of those files, and its licence text, copied from
the licence files that ship with the code: the SDK checkout, the SD library in
`build/_deps`, and the `license.txt` of the Arm GNU Toolchain. The script fails
on code in the image that no component covers, on a linker map entry with a
size that names no input, and on an SDK, TinyUSB or SD library checkout that
is not the pinned commit. After a firmware build:

```sh
python3 tools/notices.py --sdk "$PICO_SDK_PATH" --out build/THIRD_PARTY_NOTICES.txt
python3 tools/notices.py --check build/THIRD_PARTY_NOTICES.txt
```

The Arm GNU Toolchain keeps `license.txt` beside its `bin/`; for a toolchain
packaged without it, `--toolchain-licence` names the file. `--check` fails on
a section with an empty or unclosed licence text and on a section the index
lists and the file lacks; with `--sdk` it also fails on a component of the
image the file has no section for. The Firmware job of CI writes and checks
the file on every push, and the Release workflow checks it before publishing
and again after.

---

In collaboration with Claude Code.
