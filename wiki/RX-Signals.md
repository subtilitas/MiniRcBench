**English** | [Deutsch](RX-Signale)

# RX Signals

`MENU → RX SIGNALS` holds the tools for a receiver's signals. **MONITOR**
shows what a receiver sends on its S.BUS output: the 16 channels, the two
digital channels, whether the link is up, the frame rate and the errors on
the line. It sends nothing. **OUTPUT** sends S.BUS to a flight
controller's receiver input: 16 channels on sliders, the two digital
channels and the failsafe and frame-lost flags. S.BUS is Futaba's serial bus
for receiver channels.

Not tested on hardware: no receiver and no flight controller has been on a
pin of this board yet.

<img src="img/rx-chooser.png" width="240" alt="RX SIGNALS: MONITOR with S.BUS FROM A RECEIVER, ON GP1., OUTPUT with S.BUS TO A FLIGHT CONTROLLER. and ON GP0, NEVER TO AN ESC."> <img src="img/sbus-live.png" width="240" alt="S.BUS MONITOR with frames at 14 ms: the OK chip, 16 lime bars with their values in microseconds, 14.0 MS 71.4 HZ">

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

## The hub

`MENU → RX SIGNALS` opens a hub, a page with two buttons. The strip names both
signal pins, `SIG GP0  AUX GP1`, so you see which role GP1 has before you
tap. The page takes no pin, and DShot is off while it shows.

- **MONITOR** opens S.BUS MONITOR.
- **OUTPUT** opens S.BUS OUTPUT. The two lines under it are its warning:
  `S.BUS TO A FLIGHT CONTROLLER.` `ON GP0, NEVER TO AN ESC.`

Under the strip's pins a second line reads
`NO ESC CHECK: UNPLUG ANY ESC FIRST.` in amber. DShot is off on RX SIGNALS,
so the bench cannot see an ESC on the pins: unplug any ESC before you tap an
entry. With SIG on GP1, MONITOR carries `GP1 IS SIG: PLUG IN AFTER THE TAP.`
in amber: GP1 is held low until the tap, so plug the receiver in after it.

`BACK` returns to `MENU`. While the pins are still in use after another
tool, RX SIGNALS opens as always, MONITOR and OUTPUT are outlines and do
nothing, and the strip's second line reads `SETTING SIG GP0 LOW` or
`RELEASING SIG GP0` in amber.

---

## S.BUS MONITOR

The screen takes GP1 when it opens. While it does, the body
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
| `AN ESC ANSWERS ON SIG GP1.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | SIG is GP1, and an ESC answers there. The receiver would drive the ESC's signal input | Unplug the ESC. The screen takes the pin 500 ms after the ESC goes quiet; plug the receiver in once the link chip shows. The chip appears only when the claim holds and the job runs; until then GP1 is held low |
| `KISS WIRE ON AUX GP1 IS LIVE.` `UNPLUG IT. PLUG THE RECEIVER IN` `ONCE THE LINK CHIP SHOWS.` | KISS telemetry arrives on GP1 | Unplug the telemetry wire. Plug the receiver in once the link chip shows |
| `S.BUS NEEDS GP1.` `SET A PIN TO GP1 IN SETTINGS.` | neither SIG nor AUX is GP1 | Set SIG PIN or AUX PIN to GP1. SETTINGS swaps the two, so this does not happen on this board |
| `AUX GP1 COULD NOT BE TAKEN (CLOCK)` | the board's serial receiver could not reach 100,000 baud | `RETRY` |

The ESC check misses an ESC that does not answer in bidirectional DShot. With
DShot off outside ESC TESTER and ESC COMMANDS, an ESC answers only within
500 ms of leaving one of them: the hub's `NO ESC CHECK` line is the check
that stands.

<img src="img/sbus-esc.png" width="240" alt="S.BUS MONITOR waiting: AN ESC ANSWERS ON SIG GP1. and UNPLUG IT. PLUG THE RECEIVER IN ONCE THE LINK CHIP SHOWS. in amber"> <img src="img/sbus-no-gp1.png" width="240" alt="S.BUS MONITOR with no role on GP1: S.BUS NEEDS GP1. and SET A PIN TO GP1 IN SETTINGS. in amber">

---

## Leaving

`BACK` opens the leave page, as on the [servo tester](Servo-Tester). The
board lets GP1 go at once. Step 1 reads `UNPLUG THE RECEIVER.` and says
what follows on SIG: `ESC TESTER SENDS DSHOT ON SIG GP0.`, or, when SIG is
GP1, `SIG GP1 IS HELD LOW, WHERE THE RECEIVER DRIVES THE LINE.` There the
board's low output and the receiver's would drive one wire against each
other.

- `BACK TO MONITOR` keeps the pin and returns to the monitor with every
  count at 0.
- `RX SIGNALS`, the hub's title, returns to RX SIGNALS and holds the pins
  low; no DShot starts. It waits 1 s after the pin is let go.

`BACK` on the page is an outline and does nothing.

<img src="img/sbus-leave.png" width="240" alt="The S.BUS leave page: 1 UNPLUG THE RECEIVER., ESC TESTER SENDS DSHOT ON SIG GP0., BACK TO MONITOR, RX SIGNALS still grey"> <img src="img/sbus-leave-sig.png" width="240" alt="The S.BUS leave page with SIG on GP1: SIG GP1 IS HELD LOW, WHERE THE RECEIVER DRIVES THE LINE.">

---

## S.BUS OUTPUT

**Read this first.** GP0 goes to a flight controller (FC) only, never to an
ESC (electronic speed controller). Take the propellers off. The output is a
throttle to whatever listens: an armed FC passes the throttle channel to its
motors, and an arm switch is one of the channels. Before START the board
checks GP0 for an ESC's bootloader, but it finds a BLHeli_S ESC only while
that ESC is powered and in its bootloader during the check. An ESC that is
unpowered during the check, or plugged in after it, is not found. By its
source, read and not run on an ESC, a BLHeli_S ESC can take the S.BUS line
as a throttle signal, calibrate its throttle range, arm and start its motor
with no input from you.

What it is for: check an FC's channel map, endpoints and mode switches in its
configurator without a receiver or a transmitter; see what the FC does with
the failsafe and frame-lost flags and when the frames stop; send fixed,
repeatable values.

<img src="img/sbus-out-safe.png" width="240" alt="S.BUS OUTPUT SAFE: the SAFE chip, GP0 GOES TO A FLIGHT CONTROLLER. and NEVER TO AN ESC. PROPS OFF. in amber, eight grey sliders, CH3 THR with its ceiling mark, HOLD TO START"> <img src="img/sbus-out-running.png" width="240" alt="S.BUS OUTPUT running: the RUNNING chip, THROTTLE CH3: 1000 TO 1200 US (20%)., the flags line, sliders with lime values, STOP">

### Wiring

```
  board GP0  --------------------------------------  FC S.BUS input
  board GND  --------------------------------------  FC ground

  GP0 = J5 pin 2 or J4 pin 9, GND = J5 pin 4 or J4 pin 1 or 5
```

- GP0 is SIG, the ESC pin, unless SETTINGS → SIG PIN is GP1. In the default
  wiring the FC lead goes where the ESC was: unplug the ESC first.
- The board sends signal and ground only. The FC takes power from its own
  USB port or its own supply; on its battery its ESCs and motors are live.
- Power the board before the FC, and switch the FC off before the board.
  What an FC's S.BUS input does to GP0 while the board is off, and whether
  every FC reads the board's 3.3 V signal, has not been checked.

### Before START

The screen takes GP0, then checks it for an ESC's bootloader for
about 1.1 s: a quiet second, then three greetings, as ESC SETTINGS does. It
repeats the check until one round gets no answer. Only then does the chip
read `SAFE` and **HOLD TO START** become available.

| Text | Why | What to do |
|---|---|---|
| `CHECKING SIG GP0 FOR AN ESC.` | the check runs | wait about a second |
| `AN ESC BOOTLOADER ANSWERS ON SIG GP0.` `UNPLUG THE ESC BATTERY AND LEAD.` `START WAITS UNTIL NONE ANSWERS.` | a Bluejay, BLHeli_S or AM32 bootloader answered: an ESC is on GP0 | Unplug the ESC's battery and its lead. START stays off until a round of the check gets no answer. `MENU` shows `POWER-CYCLE THE ESC TO RUN IT` |
| `AN ESC ANSWERS ON SIG GP0.` `UNPLUG IT: NO ESC STAYS ON` `THE BENCH WHILE S.BUS RUNS.` | an ESC answers DShot on SIG | Unplug it. The screen takes the pin 500 ms after it goes quiet |
| `KISS WIRE ON AUX GP0 IS LIVE.` `UNPLUG IT. PLUG THE FLIGHT` `CONTROLLER IN ONCE SAFE SHOWS.` | SIG is GP1, GP0 is AUX, and KISS telemetry arrives on it | Unplug the telemetry wire. Plug the FC in once the `SAFE` chip shows |
| `S.BUS NEEDS GP0.` `SET A PIN TO GP0 IN SETTINGS.` | neither SIG nor AUX is GP0 | Set SIG PIN or AUX PIN to GP0. SETTINGS swaps the two, so this does not happen on this board |
| `SIG GP0 COULD NOT BE TAKEN (CLOCK)` | the serial port could not reach 100,000 baud, or the check's line could not get its clock | `RETRY` |
| `SIG GP0 COULD NOT BE TAKEN (PIO)`, `(DMA)` or `(REFUSED)` | the check's line found no free PIO state machine or DMA channel, or its driver did not take the pin | `RETRY` |

`SIG GP0` reads `AUX GP0` when SIG is GP1.

<img src="img/sbus-out-esc.png" width="240" alt="S.BUS OUTPUT waiting: AN ESC ANSWERS ON SIG GP0. UNPLUG IT: NO ESC STAYS ON THE BENCH WHILE S.BUS RUNS. in amber"> <img src="img/sbus-out-kiss.png" width="240" alt="S.BUS OUTPUT with GP0 as AUX: KISS WIRE ON AUX GP0 IS LIVE. UNPLUG IT. PLUG THE FLIGHT CONTROLLER IN ONCE SAFE SHOWS. in amber">

<img src="img/sbus-out-checking.png" width="240" alt="S.BUS OUTPUT checking: CHECKING SIG GP0 FOR AN ESC., HOLD TO START as an outline"> <img src="img/sbus-out-bootloader.png" width="240" alt="S.BUS OUTPUT: AN ESC BOOTLOADER ANSWERS ON SIG GP0. UNPLUG THE ESC BATTERY AND LEAD. START WAITS UNTIL NONE ANSWERS. in amber">

### The main view

| Part | What it shows |
|---|---|
| Strip | `SIG GP0  100 KBAUD  8E2`: the pin, the line's speed and format; the chip `SAFE` (green), `RUNNING` (red) or `STALLED` (magenta) |
| Notes | while SAFE, the two warnings in amber, `GP0 GOES TO A FLIGHT CONTROLLER.` `NEVER TO AN ESC. PROPS OFF.` While running, `THROTTLE CH3: 1000 TO 1200 US (20%).`: the throttle channel and its ceiling; and `CH17 OFF  CH18 OFF  FS OFF  LOST OFF`, amber while one of them is on. After a stop, why it stopped, and the warning `NEVER TO AN ESC. PROPS OFF.` again: START works at once, with no new check for an ESC |
| 8 rows | a channel, its slider from 1000 to 2000 µs and the value it sends, lime while running. The throttle channel reads `CH3 THR` in amber, with an amber mark at its ceiling |
| Buttons | `HOLD TO START`, or `STOP` while running; `CH 9-16` or `CH 1-8` for the other eight channels; `SET` |

- **HOLD TO START**, 1 s, sends a fixed first frame whatever the sliders
  showed: the throttle channel at 1000 µs, the other three sticks at
  1500 µs, channels 5 to 16 at 1000 µs, everything else off. The sliders
  jump to it.
- **The sliders** move only while frames go out, by dragging, in 5 µs
  steps. A tap alone changes nothing.
- **The throttle channel** stops at the ceiling of SETTINGS → MAX THROTTLE:
  1200 µs at the default 20 %. The ceiling limits the channel, not the
  motors of an armed FC, which add their idle and their stabilisation.
- **STOP** stops the frames at once, on the touch. The FC then goes into its
  own failsafe.
- **The screen stalls** for a quarter of a second: the frames stop, the chip
  reads `STALLED` and the note `STOPPED: SCREEN STALLED`.
- **No touch for 30 s** while the throttle is above 1000 µs, or channels 5 to
  16, CH17 or CH18 are off their first values: the frames stop with
  `STOPPED: NO TOUCH FOR 30 S.` An FC armed by a switch spins its motors at
  idle even at zero throttle. With every value at its first frame the
  frames go on, so the FC's receiver page can stay open.
- **The touch controller does not answer**: four reads in a row fail over
  100 ms or more, so `STOP` cannot be pressed. The frames stop as on
  `STOP`, on the main view and on SET, the note reads
  `STOPPED: TOUCH DID NOT ANSWER` until the next START, and `HOLD TO START`
  is drawn as an outline until the controller answers again. The tester
  and the servo tester stop on the same rule (see
  [The Screens](The-Screens#the-tester)).

After any stop, hold START again; the first frame is the fixed one again.

<img src="img/sbus-out-touch-lost.png" width="240" alt="S.BUS OUTPUT after the touch controller stopped answering: the SAFE chip, STOPPED: TOUCH DID NOT ANSWER and NEVER TO AN ESC. PROPS OFF. in amber, HOLD TO START as an outline">

<img src="img/sbus-out-stalled.png" width="240" alt="S.BUS OUTPUT after the screen stalled: the STALLED chip, STOPPED: SCREEN STALLED in magenta, NEVER TO AN ESC. PROPS OFF. in amber">

### SET

| Row | Values | When it changes |
|---|---|---|
| THROTTLE CHANNEL | `CH1` to `CH4`, `CH3` by default | only while stopped: `STOP TO CHANGE THE CHANNEL.` |
| DIGITAL CH17, CH18 | `OFF`, `ON` | only while frames go out: `START TO SET THE FLAGS.` |
| FLAGS FAILSAFE BIT, FRAME LOST BIT | `OFF`, `ON` | as CH17 |

Tap a row, then `-` or `+`; a change applies at once. While frames go out,
`STOP` is in the bottom right corner; while stopped, the bottom reads
`NEVER TO AN ESC. PROPS OFF.` `BACK` returns to the main view.

<img src="img/sbus-out-set.png" width="240" alt="OUTPUT SET while stopped: START TO SET THE FLAGS., the rows CHANNEL CH3, CH17 OFF, CH18 OFF, FAILSAFE BIT OFF, and NEVER TO AN ESC. PROPS OFF. at the bottom">

### Leaving the output

`BACK` stops the frames and opens the leave page. Step 1 reads
`UNPLUG THE FLIGHT CONTROLLER.` and `ESC TESTER SENDS DSHOT ON SIG GP0.`:
the next ESC TESTER or ESC COMMANDS drives that pin.

- `BACK TO OUTPUT` keeps the pin and returns to the screen; the bootloader
  check runs again before START.
- `RX SIGNALS`, the hub's title, returns to RX SIGNALS and holds the pin
  low; no DShot starts. It waits 1 s after the pin is let go.

After a Bluejay or BLHeli_S bootloader answered the check, `BACK` first
shows ESC SETTINGS' step: `UNPLUG THE ESC BATTERY, THEN LEAVE.` The ESC waits
in its bootloader and might read DShot as a command. `STAY` returns to the
screen, `LEAVE` lets the pin go; the last step reminds you to plug the ESC
back in once RX SIGNALS shows.

<img src="img/sbus-out-leave.png" width="240" alt="The S.BUS OUTPUT leave page: 1 UNPLUG THE FLIGHT CONTROLLER., ESC TESTER SENDS DSHOT ON SIG GP0., BACK TO OUTPUT, RX SIGNALS"> <img src="img/sbus-out-efm8-leave.png" width="240" alt="The leave page after a Bluejay or BLHeli_S bootloader answered: 1 UNPLUG THE ESC BATTERY, THEN LEAVE., STAY and LEAVE">

---

## Not checked yet

- The output level of any receiver at GP1, and that the board reads it
  with margin.
- Frames from a real receiver, its frame rate, and a line with no errors
  over time.
- What GP1 reads after a receiver is unplugged: by the chip's erratum
  RP2350-E9 the pin can stay at about 2.1 V. The screen then reads
  `NO SIGNAL`, with `BRK` one higher.
- The output: its signal on GP0, the 14 ms frame period on a scope, and
  any flight controller reading its frames, flags and stops.
- That an ESC's bootloader answers the board's check, and what Bluejay,
  BLHeli_S and AM32 do with S.BUS on their signal wire: read from their
  sources only.
