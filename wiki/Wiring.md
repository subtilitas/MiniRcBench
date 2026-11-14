**English** | [Deutsch](Verdrahtung)

# Wiring

One sheet with every connection the bench uses: the board's connectors pin
by pin, and three ways to wire a device to them. Only one device sits on
SIG at a time. The grounds are joined once, at the board.

![Wiring: the board's connectors, an ESC, a servo on a hand-switched supply and a servo on the switched supply](img/wiring.svg)

The board has JST-SH 1.0 mm connectors and no 2.54 mm headers, so every lead
needs a pigtail with a JST-SH plug. SIG is GP0 (J5 pin 2) and AUX is GP1
(J5 pin 1) on a board with blank flash; both can be changed under
**MENU → SETTINGS**.

| Part | What it shows | Details |
|---|---|---|
| Board connectors | J5, J4 and J3, each pin with its role. J4 pins 7 and 8 and J3 pins 1 and 2 are the I2C (Inter-Integrated Circuit) bus the touch controller (0x1A), the IMU (inertial measurement unit, 0x6B) and the RTC (real-time clock, 0x51) share. The optional INA219 current sensor, at 0x40, on J3, is the only part to connect there: [Servo Tester](Servo-Tester#current) | |
| A | An ESC (electronic speed controller): signal on SIG, ground on GND, the optional KISS telemetry wire on AUX. The middle wire of the ESC lead, the BEC (battery eliminator circuit) +5 V, is cut | [First Run](First-Run#wiring-the-esc), [Telemetry](Telemetry#the-optional-third-wire) |
| B | A servo on its own supply, with a switch in the positive lead. The servo tester's second servo goes on AUX | [Servo Tester](Servo-Tester#wiring), [KST Programming](KST-Programming#wiring), [Hitec Programming](Hitec-Programming#wiring) |
| C | A servo on a DC-DC (direct current to direct current) converter that the bench switches: its enable input (EN) on GP28 (J4 pin 11) or GP29 (J4 pin 12), with a pull-down from EN to GND at the converter. SIG, EN and GND all come from J4. Set **MENU → SETTINGS → SERVO SUPPLY** to the pin used | [KST Programming](KST-Programming#switched-supply), [Hitec Programming](Hitec-Programming#switched-supply) |

> **Never connect a servo's or an ESC's middle wire to a connector pin.** J4
> pin 2 is the board's USB (Universal Serial Bus) 5 V and feeds no servo or
> ESC. J3's 3.3 V powers the optional INA219 current sensor
> ([Servo Tester](Servo-Tester#current)); the other 3.3 V pins
> are not used.

> **Warning: never connect a servo or ESC lead straight across to J5.**
> J5 pins 2, 3 and 4 are SIG, 3.3 V and GND: the same order as a servo plug,
> signal, plus and minus. Pin 3 is the board's 3.3 V supply. Joined straight
> across, it gets the ESC's BEC (battery eliminator circuit) output, +5 V, or
> the servo's supply, 4.8 V to 6.0 V or 8.4 V for a high-voltage servo, and
> that can destroy the board. Leave J5 pin 3 unconnected.

**The pull-down.** Fit it at the converter, from EN to GND, so EN stays low
when the EN wire comes off at a connector.

- An EN with no internal pull-up: 10 kΩ to 100 kΩ.
- An EN with an internal pull-up to VIN, the converter's input voltage,
  such as the Pololu S13V30F5's 475 kΩ: the two form a divider. At the
  highest VIN used, the pull-down must hold EN below the converter's off
  threshold and at or below 3.6 V, and at or below 0.5 V while the board is
  off. For the Pololu module that is 10 kΩ:
  EN reads 0.45 V at 22 V. 100 kΩ puts 2.09 V on EN at 12 V, and the
  converter stays on outside the KST and Hitec screens.

The rule is in the README section
[KST servo programming](https://github.com/subtilitas/MiniRcBench#kst-servo-programming),
the arithmetic in the design note `docs/design/kst-programming.md`, "Example
modules".
