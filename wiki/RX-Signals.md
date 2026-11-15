**English** | [Deutsch](RX-Signale)

# RX Signals

`MENU → RX SIGNALS` holds the tools for a receiver's signals. **MONITOR**
shows what a receiver sends on its S.BUS output: the 16 channels, the two
digital channels, whether the link is up, the frame rate and the errors on
the line. It sends nothing. **OUTPUT**, an S.BUS output to a flight
controller, is not built yet. S.BUS is Futaba's serial bus for receiver
channels.

Not tested on hardware: no receiver has been on a pin of this board yet.

<img src="img/rx-chooser.png" width="240" alt="RX SIGNALS: MONITOR with S.BUS FROM A RECEIVER, ON GP1., OUTPUT as an outline with S.BUS TO A FLIGHT CONTROLLER. and NOT BUILT"> <img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR with frames at 14 ms: the OK chip, 16 lime bars with their values in microseconds, 14.0 MS 71.4 HZ">

---

## Wiring

The receiver needs its own supply. The board takes the signal and ground
only.

```
  receiver supply, the receiver's rated input
     +  ----------------------------------------------  receiver +
     -  ----------------+-----------------------------  receiver -
                        |
  board GND  -----------+
  board GP1  ------------------------------------------  receiver S.BUS out

  GP1 = J5 pin 1 or J4 pin 10, GND = J5 pin 4 or J4 pin 1 or 5
```

- GP1 is AUX, the KISS pin, unless SETTINGS → SIG PIN is set to GP1. The
  monitor reads GP1 whatever its role: it is the only pin the board's
  serial receiver can read.
- Never power the receiver from the board's 3.3 V or 5 V pins.
- Power the board first and the receiver second; switch the receiver off
  before the board. The pin takes up to 5.5 V while the board is on, only
  3.63 V while it is off. S.BUS outputs are commonly 3.3 V logic; no
  receiver's level has been measured here, and the board does not check it.
- Leave the receiver's servo and ESC (electronic speed controller) outputs
  unplugged. A receiver bound to
  a transmitter drives them whatever this screen does.

See also [Wiring](Wiring).

---

## The chooser

`MENU → RX SIGNALS` opens a page with two buttons. The strip names both
signal pins, `SIG GP0  AUX GP1`, so you see which role GP1 has before you
tap. The page takes no pin: DShot keeps running.

- **MONITOR** opens S.BUS MONITOR.
- **OUTPUT** is drawn as an outline with `NOT BUILT` under it and does
  nothing.

`BACK` returns to `MENU`. While DShot restarts after another tool, the
RX SIGNALS row in `MENU` is an outline and does nothing, as the other rows
that lead to a pin.

---

## S.BUS MONITOR

The screen takes GP1 from DShot when it opens. While it does, the body
reads `RELEASING AUX GP1`, then `TAKING AUX GP1`, then the channels appear.

| Part | What it shows |
|---|---|
| Strip | `AUX GP1  100 KBAUD  8E2`: the pin, the line's speed and format. `SIG GP1` when SIG is GP1 |
| Chip | the link: `NO SIGNAL`, `NO FRAMES`, `FAILSAFE`, `FRAME LOST` or `OK` (below) |
| 16 rows | the channel number, a bar over the whole raw range with marks at 1000, 1500 and 2000 µs, and the value in µs |
| Line 1 | `CH17` and `CH18`, the two digital channels, `ON` or `OFF`; the time between frames and the frame rate, for example `14.0 MS  71.4 HZ` |
| Line 2 | `FRAMES`: frames received since the screen took the pin, or since BACK TO MONITOR; `LOST` and `FS`: frames that carried the receiver's frame-lost and failsafe flags |
| Line 3 | `PAR`, `FRM`, `BRK`, `OVR`: bytes with a parity, framing or break error, and overruns: bytes were lost because the board read too late. A byte with an error is not used |
| Line 4 | `RESYNC`: bytes thrown away while looking for the start of a frame; `FOOTER`: frames of the right length with an end byte the decoder refuses |

| Chip | Colour | Means |
|---|---|---|
| `NO SIGNAL` | amber | nothing received for 100 ms, or nothing yet |
| `NO FRAMES` | amber | bytes arrive, but no frame: a line at the right speed that is not S.BUS, or a variant whose end byte the decoder refuses. Line 4 tells which |
| `FAILSAFE` | red | the receiver says it is in failsafe: the transmitter is off or out of range |
| `FRAME LOST` | amber | a frame within the last 250 ms carried the receiver's frame-lost flag |
| `OK` | green | frames arrive |

<img src="img/sbus-failsafe.png" width="240" alt="S.BUS MONITOR in failsafe: the FAILSAFE chip, red bars and values"> <img src="img/sbus-lost.png" width="240" alt="S.BUS MONITOR after the receiver stopped: the NO SIGNAL chip, the last values in grey">

**The bars.** Lime while frames arrive; red while the receiver reports
failsafe, so you read its failsafe values directly; grey once no frame has
come for 100 ms, so the last values stay readable. Before the first frame
the bars are empty and the values read `----`.

<img src="img/sbus-waiting.png" width="240" alt="S.BUS MONITOR before the first frame: NO SIGNAL, empty bars, ---- values">

**The values.** A channel's raw value is 0 to 2047. The screen shows it in
µs the way the rcbench library converts it: 172 is 1000 µs, 992 is 1500 µs,
1811 is 2000 µs. A flight controller can show a few µs more or less near the
ends; the middle matches. The bar is drawn over the raw value and is the
same either way.

**What to check with it.**

- That a receiver bound to your transmitter sends S.BUS at all, and at which
  frame rate: about 71 Hz for 14 ms, about 143 Hz for 7 ms.
- Which stick moves which channel, before you set up a flight controller.
- The failsafe: switch the transmitter off. The receiver either keeps
  sending with `FAILSAFE` and its failsafe values, or stops sending, which
  shows as `NO SIGNAL`.
- The link on the bench: `LOST` counts frames the receiver marked as lost.

---

## When the monitor waits

| Text | Why | What to do |
|---|---|---|
| `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | SIG is GP1, and an ESC answers there. The receiver would drive the ESC's signal input | Unplug the ESC. The screen takes the pin 500 ms after the ESC goes quiet; plug the receiver in once the link chip shows. The chip appears only when the claim holds and the job runs; until then DShot keeps driving GP1 |
| `KISS WIRE ON AUX GP1 IS LIVE.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | KISS telemetry arrives on GP1 | Unplug the telemetry wire. Plug the receiver in once the link chip shows |
| `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` | neither SIG nor AUX is GP1 | Set SIG PIN or AUX PIN to GP1. SETTINGS swaps the two, so this does not happen on this board |
| `AUX GP1 COULD NOT BE TAKEN (CLOCK)` | the board's serial receiver could not reach 100,000 baud | `RETRY` |

The ESC check misses an ESC that does not answer in bidirectional DShot.

<img src="img/sbus-esc.png" width="240" alt="S.BUS MONITOR waiting: AN ESC ANSWERS ON SIG GP1. and UNPLUG IT. PLUG THE RECEIVER IN ONCE THE LINK CHIP SHOWS. in amber"> <img src="img/sbus-no-gp1.png" width="240" alt="S.BUS MONITOR with no role on GP1: S.BUS NEEDS GP1. and SET A PIN TO GP1 IN SETTINGS. in amber">

---

## Leaving

`BACK` opens the leave page, as on the [servo tester](Servo-Tester). The
board lets GP1 go at once. Step 1 reads `UNPLUG THE RECEIVER.` and says
where DShot restarts: `DSHOT RESTARTS ON SIG GP0.`, or, when SIG is GP1,
`DSHOT RESTARTS ON SIG GP1, WHERE THE RECEIVER DRIVES THE LINE.` There
DShot's output and the receiver's would drive one wire against each other.

- `BACK TO MONITOR` keeps the pin and returns to the monitor with every
  count at 0.
- `RESTART DSHOT` returns to `MENU` and gives the pins back to DShot. It
  waits 1 s after the pin is let go.

`BACK` on the page is an outline and does nothing.

<img src="img/sbus-leave.png" width="240" alt="The S.BUS leave page: 1 UNPLUG THE RECEIVER., DSHOT RESTARTS ON SIG GP0., BACK TO MONITOR, RESTART DSHOT still grey"> <img src="img/sbus-leave-sig.png" width="240" alt="The S.BUS leave page with SIG on GP1: DSHOT RESTARTS ON SIG GP1, WHERE THE RECEIVER DRIVES THE LINE.">

---

## Not checked yet

- The output level of any receiver at GP1, and that the board reads it
  with margin.
- Frames from a real receiver, its frame rate, and a line with no errors
  over time.
- What GP1 reads after a receiver is unplugged: by the chip's erratum
  RP2350-E9 the pin can stay at about 2.1 V. The screen then reads
  `NO SIGNAL`, with `BRK` one higher.
