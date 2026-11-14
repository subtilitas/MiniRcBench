**English** | [Deutsch](Einrichtung)

# Setup

From an empty bench to the splash screen: what you need, the adapter cable,
and installing the firmware. [First Run](First-Run) continues from there.

---

## What you need

| Part | For | Notes |
|---|---|---|
| Waveshare RP2350-Touch-LCD-2.8 | everything | The firmware runs on this board only |
| USB-C cable (USB: Universal Serial Bus) | power, installing the firmware | A cable with data wires |
| JST-SH 1.0 mm cable, 4 pins | connector J5: signal and ground | Open wire ends on one side. Whether the board's box holds one is not checked |
| Servo extension lead, or a 3-pin header | the plug of the ESC (electronic speed controller) or servo | See [The adapter cable](#the-adapter-cable) |
| JST-SH 1.0 mm cable, 12 pins | connector J4, for the switched servo supply only | See [Wiring](Wiring), part C |
| microSD card, FAT32 (File Allocation Table) | logs, backups, servo files | Optional. See [SD Logging](SD-Logging) |

For each tool you also need:

| Tool | You need |
|---|---|
| ESC tester | An ESC that takes **bidirectional DShot**, a digital ESC protocol, with its motor and its own battery. BLHeli_32, Bluejay and AM32 firmware do. The ESC tester sends DShot only: an ESC that takes only PWM (pulse-width modulation) pulses cannot be tested on it. BLHeli_S without Bluejay is not tested |
| AM32 Configuration | An ESC running AM32 |
| Servo Tester | A servo and its own supply, 4.8 V to 6.0 V, or 8.4 V for a high-voltage servo |
| KST Programming | A KST or Chaservo servo and a 5 V supply you can switch: a plug, or a switch in the plus lead |
| Hitec Programming, D series | A Hitec D series servo and a 5 V supply |
| Hitec Programming, classic | A Hitec classic servo (HS-5xxx, HS-7xxx) and the switched supply of [Wiring](Wiring), part C: a 5 V DC-DC (direct current to direct current) converter with an enable input (EN), a buck converter or a type with output disconnect, rated above 1 A, its input supply, and a 10 kΩ pull-down resistor from EN to ground |

---

## The adapter cable

The board has JST-SH 1.0 mm connectors and no 2.54 mm headers. One cable
joins J5 to a servo plug:

| J5 pin | Signal | Goes to |
|---|---|---|
| 2 | SIG (GP0) | the signal wire: orange or white |
| 4 | GND (ground) | the minus wire: brown or black |
| 1 | AUX (GP1) | optional: the telemetry wire of an ESC with KISS telemetry, a serial ESC telemetry protocol, or the signal of a second servo |
| 3 | 3.3 V | nothing, ever. Cut it short and insulate it |

> **Warning: never connect a servo or ESC lead straight across to J5.**
> J5 pins 2, 3 and 4 are SIG, 3.3 V and GND: the same order as a servo plug,
> signal, plus and minus. Pin 3 is the board's 3.3 V supply. Joined straight
> across, it gets the ESC's BEC (battery eliminator circuit) output, +5 V, or
> the servo's supply, 4.8 V to 6.0 V or 8.4 V for a high-voltage servo, and
> that can destroy the board. Leave J5 pin 3 unconnected.

GP0 and GP1 are the board's GPIO 0 and 1 (general-purpose input/output
pins). Joining the wires takes a soldering iron and heat-shrink tube, or
crimp contacts.

> **The middle wire of a servo plug goes to no pin of the board.**
> - On an ESC it carries the ESC's +5 V output. Cut it or pull its contact
>   out of the plug.
> - On a servo it is the servo's supply. Connect it, and the minus wire, to
>   the servo's own supply as in [Wiring](Wiring), part B, or part C for
>   the switched supply. The minus wire also goes to a GND pin of the
>   board: J5 pin 4, or J4 pin 1 or 5.

> **No servo on SIG while the board starts.** Outside the servo screens SIG
> carries DShot from the end of the 3 s splash on. A servo reads it as a pulse
> and drives towards one end of its travel, at a frame rate that overheats an
> analogue servo. Plug the servo in after its screen is open. On HITEC SERVO,
> SIG carries DShot until a family is tapped: keep the time on the family view
> short. Power the board before the servo supply, and switch the servo supply
> off before the board: [Servo Tester](Servo-Tester#wiring).

The wire colours of a JST-SH cable follow no standard. Find pin 1 by the
connector, not by colour: [Wiring](Wiring) lists every pin. Where pin 1 sits
on the board's connector is in the board maker's schematic; it is not drawn
in this manual.

---

## Installing the firmware

1. Download `MiniRcBench-<version>-touch-lcd-2.8.uf2` from
   [Releases](https://github.com/subtilitas/MiniRcBench/releases), for
   example `MiniRcBench-v0.1.0-touch-lcd-2.8.uf2`. UF2 is the USB Flashing
   Format.
2. Connect the board to a computer with the USB-C cable.
3. Hold **BOOT**, tap **RESET**, release **BOOT**. A USB drive appears. Its
   name and where the two buttons sit on the board are not checked here.
4. Copy the `.uf2` file onto that drive. The board restarts.
5. The splash screen shows for 3 s and names the board,
   `RP2350-TOUCH-LCD-2.8`. Then the tester screen appears.

<img src="img/splash.png" width="240" alt="The splash screen">

To install a newer version, repeat steps 1 to 5. The drive appears whatever
firmware the board holds. A version with another settings layout returns
every saved setting to its default once.

---

## Power

- **USB-C** powers the board, from a 5 V USB supply or a computer.
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
