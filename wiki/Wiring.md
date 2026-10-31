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
| Board connectors | J5, J4 and J3, each pin with its role. J3 and J4 pins 7 and 8 are the shared I2C (Inter-Integrated Circuit) bus of the touch controller: do not use them | |
| A | An ESC (electronic speed controller): signal on SIG, ground on GND, the optional KISS telemetry wire on AUX. The middle wire of the ESC lead, the BEC (battery eliminator circuit) +5 V, is cut | [First Run](First-Run#wiring-the-esc), [Telemetry](Telemetry#the-optional-third-wire) |
| B | A servo on its own supply, with a switch in the positive lead. The servo tester's second servo goes on AUX | [Servo Tester](Servo-Tester#wiring), [KST Programming](KST-Programming#wiring), [Hitec Programming](Hitec-Programming#wiring) |
| C | A servo on a DC-DC converter that the bench switches: its enable input (EN) on GP28 (J4 pin 11) or GP29 (J4 pin 12), with a 10 kΩ pull-down to the supply's minus. Set **MENU → SETTINGS → SERVO SUPPLY** to the pin used | [KST Programming](KST-Programming#wiring), [Hitec Programming](Hitec-Programming#wiring) |

> **Never connect a servo's or an ESC's middle wire to a connector pin.** J4
> pin 2 is the board's USB (Universal Serial Bus) 5 V and feeds no servo or
> ESC; the 3.3 V pins are not used.

The 10 kΩ pull-down fits the Pololu S13V30F5 converter. A converter with
another enable input needs the value its own page names: see
[KST Programming](KST-Programming#wiring).
