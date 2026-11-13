**English** | [Deutsch](Einrichtung)

# Setup

From an empty bench to the splash screen: what you need, the adapter cable,
and installing the firmware. [First Run](First-Run) continues from there.

---

## What you need

| Part | For | Notes |
|---|---|---|
| Waveshare RP2350-Touch-LCD-2.8 | everything | The firmware runs on this board only |
| USB-C cable (USB: Universal Serial Bus) | power, installing the firmware | A cable with data wires; a charge-only cable shows no drive |
| JST-SH 1.0 mm cable, 4 pins | connector J5: signal and ground | Open wire ends on one side. Whether the board's box holds one is not checked |
| Servo extension lead, or a 3-pin header | the plug of the ESC or servo | See [The adapter cable](#the-adapter-cable) |
| JST-SH 1.0 mm cable, 12 pins | connector J4, for the switched servo supply only | See [Wiring](Wiring), part C |
| microSD card, FAT32 (File Allocation Table) | logs, backups, servo files | Optional. See [SD Logging](SD-Logging) |

For each tool you also need:

| Tool | You need |
|---|---|
| ESC tester | An ESC (electronic speed controller) that takes **bidirectional DShot**, with its motor and its own battery. BLHeli_32, Bluejay and AM32 firmware do. An ESC that takes only PWM (pulse-width modulation) pulses is not driven. BLHeli_S without Bluejay is not tested |
| AM32 Configuration | An ESC running AM32 |
| Servo Tester | A servo and its own supply, 4.8 V to 6.0 V, or 8.4 V for a high-voltage servo |
| KST Programming | A KST or Chaservo servo and a 5 V supply you can switch: a plug, or a switch in the plus lead |
| Hitec Programming, D series | A Hitec D series servo and a 5 V supply |
| Hitec Programming, classic | The switched supply of [Wiring](Wiring), part C: a 5 V DC-DC (direct current to direct current) converter with an enable input |

---

## The adapter cable

The board has JST-SH 1.0 mm connectors and no 2.54 mm headers. One cable
joins J5 to a servo plug:

| J5 pin | Signal | Goes to |
|---|---|---|
| 2 | SIG (GP0) | the signal wire: orange, yellow or white |
| 4 | GND (ground) | the minus wire: brown or black |
| 1 | AUX (GP1) | optional: KISS telemetry, or a second servo |
| 3 | 3.3 V | nothing. Cut it short and insulate it |

> **The middle wire of a servo plug goes to no pin.** On an ESC it carries
> the ESC's +5 V output; on a servo it is the servo's supply. Cut it or pull
> its contact out of the plug on the board's side.

The wire colours of a JST-SH cable follow no standard. Find pin 1 by the
connector, not by colour: [Wiring](Wiring) lists every pin. Where pin 1 sits
on the board's connector is in the board maker's schematic; it is not drawn
in this manual.

---

## Installing the firmware

1. Download `MiniRcBench-<version>-touch-lcd-2.8.uf2` from
   [Releases](https://github.com/subtilitas/MiniRcBench/releases). UF2 is
   the USB Flashing Format.
2. Connect the board to a computer with the USB-C cable.
3. Hold **BOOT**, tap **RESET**, release **BOOT**. A USB drive named
   `RP2350` appears.
4. Copy the `.uf2` file onto that drive. The drive disappears and the board
   restarts.
5. The splash screen shows for 3 s and names the board,
   `RP2350-TOUCH-LCD-2.8`. Then the tester screen appears.

<img src="img/splash.png" width="240" alt="The splash screen">

To install a newer version, repeat steps 1 to 5. The board enters this mode
whatever firmware it holds. A version with another settings layout returns
every saved setting to its default once.

---

## Power

- **USB-C** powers the board. Any 5 V USB supply works; no computer is needed
  after the firmware is installed.
- **A lithium-polymer cell** on the board's battery connector is optional.
  The board charges it. With a cell fitted, press the power button to switch
  on. The connector type and the cell's polarity are in the board maker's
  documentation; they are not checked here.
- The ESC and every servo run from **their own supply**, never from the
  board.

---

## Next

- [First Run](First-Run): wiring an ESC and the first arm
- [Wiring](Wiring): every connection on one sheet
- [Troubleshooting](Troubleshooting): symptom first
