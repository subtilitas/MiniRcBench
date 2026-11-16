**English** | [Deutsch](Erste-Schritte)

# First Run

This page covers wiring an ESC (electronic speed controller), powering up, and
the first arm. You need two wires and a battery for the ESC. For the parts,
the adapter cable and installing the firmware, see [Setup](Setup) first.

---

## Safety first

The board makes a motor spin. Do these three things before anything else:

- **Remove the propeller.**
- **Secure the motor.** An unsecured motor can move or jump when it spins up.
- **Power the ESC from its own battery.** Do not power a motor from the
  board's 5 V pin.

The firmware has these interlocks. They do not replace the steps above:

- The throttle ceiling defaults to **20 %** of the DShot range.
- Arming needs a **1 s hold**, with the throttle at zero for at least 250 ms.
- `DISARM` acts **on touch**, as does `STOP` on the servo tester. Every other
  control acts on release.
- While armed, **30 s** without a touch disarms the tester. The last 5 s count
  down on screen.
- If the display firmware stops, the second core sets the throttle to zero
  after **250 ms** without a heartbeat.

---

## Wiring the ESC

Two wires: signal and ground. [Wiring](Wiring) shows every connection on one
sheet.

| | RP2350-Touch-LCD-2.8 |
|---|---|
| **Signal** | GP0, J4 pin 9 or J5 pin 2 |
| **Ground** | GND, J4 pin 1 or 5, or J5 pin 4 |

The board has JST-SH 1.0 mm connectors and no 2.54 mm headers, so the ESC
lead needs a pigtail with a JST-SH plug.

GP means GPIO (general-purpose input/output pin). The ESC pin is SIG; the
KISS telemetry wire, if you fit one, goes on AUX, GP1. Both can be changed
later under **MENU → SETUP → SIG PIN** and **AUX PIN**, where a step
swaps the two, and the setting is stored in flash.

> **Cut or de-pin the middle wire of the ESC lead first.**
> The middle conductor of a 3-wire servo plug is the ESC's **+5 V BEC
> (battery eliminator circuit) output**, and it goes to no pin. GP0 and GP1,
> the signal pins, tolerate 5.5 V while the board is powered and 3.63 V
> while it is off. The tester needs signal and ground only.

> **Warning: never connect a servo or ESC lead straight across to J5.**
> J5 pins 2, 3 and 4 are SIG, 3.3 V and GND: the same order as a servo plug,
> signal, plus and minus. Pin 3 is the board's 3.3 V supply. Joined straight
> across, it gets the ESC's BEC (battery eliminator circuit) output, +5 V, or
> the servo's supply, 4.8 V to 6.0 V or 8.4 V for a high-voltage servo, and
> that can destroy the board. Leave J5 pin 3 unconnected.

Keep the signal wire short. The maximum working length is not measured. If the
`LINK` error rate rises, set **DSHOT** to 300 under **MENU → SETUP**.

---

## Powering up

Power the board over USB-C (USB: Universal Serial Bus) and the ESC from its
own battery. The order does not matter. The firmware sends the telemetry
enable to every ESC that starts answering, so the ESC can be connected or
power-cycled at any time.

The splash screen shows for 3 s, then `MENU` appears, the list of tools. The
splash names the board the firmware is built for, RP2350-TOUCH-LCD-2.8, and the
firmware version, for example `VERSION 0.1.0`. No DShot frame leaves the
board at power-up: the ESC pin is driven low from `escTaskInit()` on, before
the splash, and nothing is sent, through the splash and on `MENU`. DShot
starts only when you open `ESC TESTER` or `ESC COMMANDS`.

<img src="img/splash.png" width="240" alt="Splash screen">

After a firmware update that changed the layout of the stored settings from
version 2, 3, 4 or 5, the saved settings are carried over: the splash shows
`SETTINGS CARRIED OVER FROM AN UPDATE` and
`CHECK MENU > SETUP, THEN SAVE` in amber above the name. Check them in
SETUP and hold `HOLD TO SAVE`
([Troubleshooting](Troubleshooting#my-settings-reset-themselves)).

After an update from a layout the firmware does not know, the saved
settings are discarded and every setting starts at its compiled default.
The splash then shows two amber lines above the name:
`SETTINGS RESET BY UPDATE` and `CHECK MENU > SETUP`. Check at least the
pole count, `MAX THROTTLE`, `SIG PIN`, KISS, `SERVO SUPPLY` and
`CURRENT SENSE`, then hold `HOLD TO SAVE`. Until a save lands, MENU's
SETUP row reads `UPDATED`, after a carry-over as after a reset. A board
that never had saved settings shows none of this.

<img src="img/splash-reset.png" width="240" alt="Splash screen after an update discarded the saved settings">

If the touch controller did not answer at power-up, the splash shows
`TOUCH CONTROLLER NOT FOUND` in amber above the name. The line goes as soon
as the controller answers. If it is still there when `MENU` appears,
touch does not work; see
[Troubleshooting](Troubleshooting#the-screen-does-not-react-to-touch).

<img src="img/splash-no-touch.png" width="240" alt="The splash with TOUCH CONTROLLER NOT FOUND in amber above the name">

`MENU` is where every tool starts. Its header has no `BACK`: nothing lies
behind it. Tap its first row, `ESC TESTER`, to open the tester. The tester
opens disarmed, at throttle 0, with `LATCH` off; its `MENU` button at the
bottom right returns to `MENU`. DShot starts when the tester opens: the ESC
first hears the telemetry enable, then zero-throttle frames. `MENU` stops
DShot again, after zero-throttle frames for at least 250 ms, and holds the
ESC pin low. An ESC that loses the frames does what its firmware does on
signal loss; that is not measured on this board.

> **Unplug any servo from the ESC pin before you open the tester.** A servo
> there reads DShot as a pulse of about 970 µs at 1000 Hz and moves to one
> end of its travel.

<img src="img/menu.png" width="240" alt="MENU after the splash: ESC TESTER is the first row">

---

## Before an ESC is connected

<img src="img/tester-noesc.png" width="240" alt="The tester screen with no ESC connected">

With no ESC answering, the screen shows `NO TELEMETRY` under the RPM
(revolutions per minute) readout, `--` in the five ESC tiles, and a dark red
zero for RPM. The large voltage at the top right, captioned `BOARD`, is the
board's own battery, not the ESC's; the ESC's voltage is the `VOLTAGE` tile.

If the ESC is powered and wired and this does not change, see
[Troubleshooting](Troubleshooting#no-telemetry).

---

## Your first arm

<img src="img/tester-idle.png" width="240" alt="The tester disarmed: SAFE in green, ARM TO ENABLE in the throttle bar">

Disarmed, the badge reads `SAFE` and the throttle bar reads `ARM TO ENABLE`.
For the first 1.5 s after the tester opens it reads
`DSHOT STARTING: WAIT 1.5 S` in amber instead, and `HOLD TO ARM` is drawn as
an outline: the ESC needs to see DShot before it can arm. A press made then
does nothing; lift and press again once the button is live.

1. Check that the propeller is off.
2. Press and **hold** `HOLD TO ARM` for 1 s. A green bar fills along the bottom of
   the badge while you hold.
3. The badge turns red and reads `ARMED`. The backlight dims to a quarter for
   120 ms, so the event is visible while you watch the motor.

<img src="img/tester-armed.png" width="240" alt="Armed, with telemetry arriving">

To stop, press `DISARM`. It acts on touch. The press that disarms, kept down,
does not arm again: arming needs a new press that starts on the button.

**Lift the throttle finger before `DISARM`.** The firmware reads only the
touch controller's first contact slot. Which contact the controller puts
there while two fingers touch the glass is not measured, so a press made
while the throttle finger rests on the glass may not register. A wet
screen, or a lead lying across the glass, can count as a finger and block
every tap.

A finger already on the glass when `MENU` appears opens nothing when it
lifts: a row takes a tap only from a press that began on it.

If the touch controller stops answering while the tester is armed, the
tester disarms once four reads in a row have failed over 100 ms or more,
sets the throttle to zero, turns `LATCH` off and shows
`TOUCH NOT ANSWERING`.

---

## Giving it throttle

Two surfaces set the same throttle value.

**The bar at the bottom.** With `LATCH` off, the finger position along the
track is the throttle, and the throttle returns to zero on release. The full
track width is the *ceiling*: at the default 20 % ceiling, the right end
commands 20 %.

**The number area** above the bar is a relative throttle pad. Swipe **up** for
more, **down** for less. A tap does nothing, and swipes add up, so the
throttle can be raised over several short swipes without looking. After 6 px
of travel the hint under the RPM readout changes to `SWIPE ACTIVE` and the
swipe moves the throttle. While the tester shows the 3D note of
[The Screens](The-Screens) in the hint's place, `SWIPE ACTIVE` still replaces
it while a swipe is engaged, and the note returns on release.

`LATCH` makes the bar relative and latching: the throttle stays where you leave
it. The bar shows a grab handle and its caption reads `LATCHED`. Turning
`LATCH` off sets the throttle to zero.

---

## Raising the ceiling

`MENU` on the tester returns to the list of tools, and always disarms on
the way. `SETUP` is the last row, on the screen with no drag: tap it. Or,
while
disarmed, tap `MAX 20%` or `14 POLES` on the
tester, underlined while a tap on them works: the tap disarms the same way
and opens SETUP, and `BACK` there returns to the tester.

<img src="img/settings.png" width="240" alt="SETUP after a change: MOTOR POLES at 16 and UNSAVED - HOLD SAVE TO KEEP">

- **MOTOR POLES**: converts the reported eRPM (electrical RPM) to RPM with
  `RPM = eRPM / (poles / 2)`. Range 2–28, even values only. Default 14. A
  wrong value scales the RPM reading by a fixed factor; other readings are
  unaffected.
- **MAX THROTTLE**: the throttle ceiling, 5–100 % in 5 % steps. Hold `-` or `+`
  to repeat; the repeat speeds up the longer you hold.

Both rows are at the top of the list. Both values are stored only when saved,
with `HOLD TO SAVE` under the list. The note line reads
`UNSAVED - HOLD SAVE TO KEEP` until then, and `MENU` shows `UNSAVED` on its
`SETUP` row.

`SIG PIN`, further down, moves the ESC pin to the other GPIO at once: over
the tester DShot moves with it, from `MENU` the next tester uses it. While it
is not the pin in use at power-up, a line of its own above `LINK` reads in
amber `UNPLUG ANY SERVO FROM GP1 NOW`; the note line under it still says
whether the settings are saved.

<img src="img/settings-pin.png" width="240" alt="SETUP after SIG PIN moved to GP1: UNPLUG ANY SERVO FROM GP1 NOW above LINK, UNSAVED on the note line">

---

## Next

- [The Screens](The-Screens): the rest of the interface
- [Telemetry](Telemetry): what the tiles show
- [SD Logging](SD-Logging): recording a run and plotting it
