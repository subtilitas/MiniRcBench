# DShot lifetime

Status: slices D1, D2, D3 and D4 built, host tests only ([D1 as
built](#d1-as-built), [D2 as built](#d2-as-built), [D3 as
built](#d3-as-built), [D4 as built](#d4-as-built)); D5 not built. The
developer settled its six questions on
2026-11-16, and made four further calls the reviews of the note raised, each
pending the owner's review ([Decisions](#decisions)). It builds on
[menu-c.md](menu-c.md), whose slice 3 makes MENU the root, and answers
[What the structure requires of
DShot](menu-c.md#what-the-structure-requires-of-dshot).

The owner's decisions of 2026-11-15 that this note takes as fixed:

1. **MENU is the start screen,** in place of the ESC tester
   ([menu-c.md](menu-c.md#decisions) owner decision 1).
2. **No DShot at power-up.** DShot sends frames only while ESC TESTER or ESC
   COMMANDS is open ([menu-c.md](menu-c.md#decisions) owner decision 2).
3. **The menu structure is menu-c.md**, with the owner's answers: six MENU
   rows, ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS and SETUP; a tool
   returns to its hub; RX SIGNALS, SETTINGS, SD LOG and ESC CONFIG are
   renamed RECEIVER, SETUP, LOGS and ESC PROGRAM. This note uses the new
   names for the structure and today's names for code and screen texts.
   Owner decision 4 of menu-c.md puts ESC COMMANDS in a hub; menu-c.md's
   choice 8, which the owner has not decided, makes it the first entry of
   the hub ESC PROGRAM.
4. **0.1.0 waits for this work.**

This note reads decision 2 so: back on MENU, in a hub, on LOGS or on SETUP
opened from MENU the frames stop, and SERVO TESTER, KST SERVO, HITEC SERVO,
S.BUS MONITOR, S.BUS OUTPUT and the ESC PROGRAM tools (today's ESC CONFIG:
ESC SETTINGS and USB BRIDGE) get the pin with no drain, since the pump they
release is parked. What the ESC does when the frames stop is its firmware's
signal-loss behaviour, read from source and not measured
([new risk 1](#what-new-risk-appears)). SETUP opened over ESC TESTER is
[decision 6](#decisions)'s reading of decision 2, which waits for the
owner's confirmation.

Pull request #49, the touch-loss stop, is merged into `main` as 3b36588.
While the touch controller does not answer (`uiTouchLost()`), it disarms the
tester and refuses arming, and it stops SERVO TESTER and S.BUS OUTPUT and
refuses their HOLD TO START. Core1's own cut in the servo job stays: it
switches the servo's supply off once the touch controller's last answer is
250 ms old (`touchOk`, `src/servo_job.cpp` 405, as at cff09ae). [#49 and the
lifetime](#49-and-the-lifetime) says how it fits.

The code is read at 43531d9, whose `src/` and `test/` equal `main` at
cff09ae (`git diff --stat cff09ae 43531d9 -- src test` is empty). Line
numbers are at that commit. `main` 3b36588 is cff09ae with #49 added. #49
moves lines in 12 files of `src/`, `src/ui.cpp` and `src/ui.h` among them
(`git diff --stat cff09ae 3b36588 -- src`); the references to its own code
below are at 3b36588.

Terms: ESC is electronic speed controller, DShot the digital ESC protocol the
tester drives, EDT Extended DShot Telemetry, eRPM electrical revolutions per
minute, KISS the serial telemetry of the KISS ESC line on its own wire, PIO the
RP2350's programmable input/output block, SM one PIO state machine, DMA direct
memory access, SIO the RP2350's single-cycle input/output block, which drives a
pad as a plain output, GPIO general-purpose input/output, PWM pulse-width
modulation, EFM8 the Silicon Labs chip of Bluejay and BLHeli_S ESCs, AM32 an
open ESC firmware, S.BUS a serial receiver protocol, UI user interface, ADC
analog-to-digital converter, RAM random-access memory, USB Universal Serial Bus.
SIG and AUX are the two signal pins of
[pin-claim.md](pin-claim.md#the-two-pins), GP0 and GP1 by default.

## Boot today

| Step | What happens | Where |
|---|---|---|
| Power latch | GP26 high first | `src/main.cpp` 106 to 108 |
| Settings | loaded before the pump is seeded | `src/main.cpp` 126 |
| Pump seeded | `escTaskInit()` copies SIG, the rate, KISS on and AUX from the settings; `s_suspendReq` stays false | `src/main.cpp` 133; `src/esc_task.cpp` 316 to 331, 295 |
| Splash | 3000 ms (`SPLASH_MS`), core1 not running; SIG and AUX keep the pad's reset state | `src/main.cpp` 148 to 161; `src/config.h` 661 to 668 |
| Splash text | `UNPLUG ANY SERVO FROM GP0 NOW` in amber at y 288, because DShot starts when the splash ends | `src/ui.cpp` 1291 to 1296; `src/ui.h` 166 to 171 |
| First screen | the tester, `Level::Tester` | `src/ui.cpp` 1346 |
| Core1 | launched after `setup()`; `escTaskBegin()` claims nothing | `src/main.cpp` 309, 260 to 263; `src/esc_task.cpp` 357 to 363 |
| DShot start | the first `escTaskPoll()` sees no suspend and no pump, and binds: `out_dshot_bind(pin, kbaud, true)`, bidirectional | `src/esc_task.cpp` 429 to 456 |
| PIO | the bind claims a transmitter SM and an adjacent receiver SM in one block, puts the pull-up on SIG, inverts SIG's output at the pad, and reads the inversion back; it enables nothing: "nothing is on the wire until a frame is sent" | `src/dshot/out_dshot.c` 134 to 221 |
| DMA | none: the DShot driver and the KISS receiver use no DMA channel (`grep -i dma` over `src/dshot/`, `src/esc_task.cpp` and `src/pio_uart_rx.cpp` finds no call) | |
| EDT enable | the first 10 frames (`EDT_ENABLE_REPEATS`) are command 13, then zero-throttle frames, one every 1000 µs (`DSHOT_PERIOD_US`); a later enable every 1000 ms while the ESC answers without EDT | `escPumpBind()`, `src/esc_task.h` 910 to 917; `src/config.h` 96, 296, 329 |
| KISS | with KISS on, the PIO receiver starts on AUX with the bind, with its pull-up | `src/esc_task.cpp` 449 to 451; `src/pio_uart_rx.cpp` 60 to 65 |
| Telemetry | each slot reads the reply to the previous frame and folds it into `EscTelemetry` | `src/esc_task.cpp` 474 to 483 |
| Status bar | `DSHOT600` from `escTaskDshotKbaud()`, in the text colour once an EDT enable went out, dim before; `DSERR` in amber while `initError`, a refused bind | `src/ui.cpp` 600 to 640 |
| MENU's chip | `EDT ON`, `EDT OFF` or `NO ESC` from the telemetry | `src/ui_menu.cpp` 198 to 206, 241 to 248 |
| MENU's run cue | `POWER-CYCLE THE ESC TO RUN IT` while `uiAm32RunCue()`, `usbBridgeRunCue()` or `sbusOutRunCue()` holds; each clears on an eRPM frame newer than its bootloader's answer | `src/ui_menu.cpp` 249 to 253; `src/ui_am32.cpp` 2487 to 2492; `src/usb_bridge.cpp` 616 to 618; `src/sbus_out_job.cpp` 521 to 526 |
| SETTINGS' LINK | `LINK  998/S  0% ERR` from `packetRate` and `errPercent` | `src/ui_settings.cpp` 783 to 801 |
| SETTINGS' pin note | `UNPLUG ANY SERVO FROM GP1 NOW` while SIG differs from the pin of the last load, since `applyLive()` moves DShot at once | `src/ui_settings.cpp` 707 to 714, 857 to 863 |

From then on DShot runs on every screen, MENU included, except while a pin
claim holds the pins ([pin-claim.md](pin-claim.md#the-claim)).

## What changes at boot

Core1 still starts after the splash, but it starts with the pump down and
the pins parked ([the pump's three modes](#the-pumps-three-modes)):

- `escTaskInit()` asks for Park instead of leaving the pump free to bind,
  and parks SIG itself on core0, before core1 is launched, as
  `servoSupplyInit()` sets its enable pin before core1 starts
  (`src/main.cpp` 127 to 130). The parking is a GPIO setting, not a PIO
  claim, so core0 may make it while core1 does not run.
- The first `escTaskPoll()` finds Park wanted and the pump down: it binds
  nothing, claims no SM and starts no KISS receiver.
- The status bar, the LINK line and the telemetry block stay as they are;
  the block receives nothing until ESC TESTER or ESC COMMANDS opens.
- MENU is the first screen (menu-c.md, slice 3).

So no DShot frame leaves the board at power-up, and SIG is held low from
`escTaskInit()` on. Before it, from reset to `escTaskInit()`, SIG and AUX
are in the pad's reset state.

## SIG and AUX while idle

"Idle" is MENU, the hubs, LOGS, SETUP opened from MENU, and every claiming
tool before its claim and after its release.

### The pad's states today

| State | Pad | Where |
|---|---|---|
| Reset | function NULL (`IO_BANK0_GPIO0_CTRL_FUNCSEL_RESET` 0x1f), input buffer off (`PADS_BANK0_GPIO0_IE_RESET` 0), pull-down on (`PDE_RESET` 1), pull-up off, isolated (`ISO_RESET` 1) | pico-sdk 2.1.1, `hardware/regs/pads_bank0.h` lines 37 to 86, `io_bank0.h` line 134 |
| DShot bound | PIO, output inverted, pull-up; between frames the transmitter releases the line and the pull-up holds it high | `src/dshot/out_dshot.c` 183 to 186 |
| DShot stopped | the SMs off, the pin direction input, so the pull-up holds it high | `out_dshot_stop()`, 233 to 266 |
| DShot released | SIO, no inversion, pulls off, input: floating | `release_pad()`, 104 to 115 |
| After the pump's teardown | the pull-up put back: high | `src/esc_task.cpp` 403 to 405 |
| After the KISS receiver | function NULL, pulls off: floating | `src/pio_uart_rx.cpp` 77 to 85 |
| After the servo or KST job | input, pull-down | `src/servo_pwm.cpp` 115, 116; [pin-claim.md](pin-claim.md#sequence) step 5 |
| After the S.BUS jobs | input, pull-down | `src/sbus_job.cpp` 131, 132; `src/sbus_out_job.cpp` 262 to 265 |
| After the escwire job, the bridge, the AM32 transport | input, pull-up | `src/escwire_job.cpp` 184; `src/bridge_line_port.cpp` 104, 105; `src/am32_bl.cpp` 87, 88 |

`gpio_set_function()` turns the input buffer on and clears the isolation
(pico-sdk `hardware_gpio/gpio.c` lines 38 to 52), so every state after the
first bind has the input buffer on.

### The choice: SIG driven low, AUX on its pull-down

**SIG is an SIO output driving low, at the pad's lowest drive setting,
2 mA.** **AUX is an input with the pull-down on and the input buffer off**,
the reset state. [Decision 5](#decisions) settles it, because "nothing
drives SIG" also reads as "SIG is not driven".

Why low, and why driven:

1. **A line resting high sends an EFM8 ESC into its bootloader.** Bluejay
   enters it about 320 ms plus 150 ms after its last frame once the line
   rests high, BLHeli_S after its timeout plus 15 ms
   ([bluejay-blheli-s.md](bluejay-blheli-s.md#entering-the-bootloader)).
   The bootloader then waits for commands with no timeout, and DShot frames
   can pass as one ([bluejay-blheli-s.md](bluejay-blheli-s.md#safety),
   "No DShot into a waiting bootloader"). Today's teardown leaves SIG high
   (`src/esc_task.cpp` 405); with DShot off on MENU for minutes, every
   visit to MENU would put a powered EFM8 ESC into its bootloader, and the
   next ESC TESTER would send DShot into it.
2. **A low line starts an AM32's firmware.** AM32's bootloader starts the
   firmware on more than 450 low samples in its pulled-down check, on one
   low sample in its last floating 5 ms, and leaves its wait on a line low
   for more than 20 ms
   ([bluejay-blheli-s.md](bluejay-blheli-s.md#am32s-bootloader), from
   source, not measured).
3. **A pull-down alone is not known to hold the line low against the
   ESC.** Whether an EFM8's or an AM32's own pull-up on its signal pin
   beats the RP2350's pull-down is not measured; the opposite case, the
   bench's pull-up against the AM32's pull-down, is open in
   [bluejay-blheli-s.md](bluejay-blheli-s.md#am32s-bootloader) too. A
   driven low settles both.
4. **A pull-down with the input buffer on can latch high.** RP2350-E9, as
   `src/board_pins.h` lines 33 to 38 record: a pad configured as an input
   with the pull-down engaged can latch at about 2.1 V. Driving the pad
   avoids it.
5. **Low is the level of no signal.** A servo, a receiver input or a flight
   controller's input on SIG sees no pulse. An unpowered ESC on SIG gets no
   current from a pin at 0 V, where a pin driven high could feed it through
   its signal pin's protection diode
   ([bluejay-blheli-s.md](bluejay-blheli-s.md#the-connect-cycle)).

What it costs: a device that drives SIG high while idle drives against the
pad. Of today's uses that is an S.BUS receiver plugged into SIG when SIG
is GP1, the monitor's other wiring (`PinUser::Sbus` in `src/pin_claim.h`
line 97), and a KISS telemetry wire put on SIG by mistake. The current then
is the two drivers' short-circuit current, which the 2 mA setting does not
limit to 2 mA and which is not measured. The S.BUS MONITOR's leave page
already asks for the receiver to be unplugged; its text changes to say why
([texts](#texts-and-documents-that-assume-dshot-from-boot)). Before the
claim, while SIG is GP1, the hub RECEIVER draws a second line under its
entry MONITOR in `C_AMBER`: `GP1 IS SIG: PLUG IN AFTER THE TAP.`, 34
characters, 203 px, within the entry's 37. The tap claims SIG and releases
the pad to the monitor's input; until then the pad is driven low. Slice D2
builds it.

Why AUX is not driven: on AUX an ESC's KISS telemetry output and an S.BUS
receiver are outputs by design, so a driven AUX would fight them. Nothing
on AUX decides an ESC's bootloader. The pull-down with the input buffer off
is the reset state; by the RP2350 datasheet's description of E9 the latch
needs the input buffer on (not verified on this board).

Whether the board carries a resistor on GP0 or GP1 is not recorded in this
repository.

### What is known about a floating line

Not measured: whether an ESC arms on noise picked up by a floating signal
lead. Arming needs a valid protocol: a DShot frame carries a 4-bit
checksum, and PWM input wants 1000 to 2000 µs pulses at a rate the ESC
accepts. What a long unterminated lead picks up near mains wiring is not
recorded here. With SIG driven low, SIG does not float while idle; it
floats only between a job's release and the park, one core1 pass, and
during the pad's reset state, before `escTaskInit()`.

## The pump's three modes

Core1 keeps the one construction site of `escTaskPoll()`
(`src/esc_task.cpp` 379 to 456). Core0 asks for one of three modes:

| Mode | Asked by | Core1 does | Reported |
|---|---|---|---|
| Run | `escTaskStart()`, from the claim only | binds as today: EDT enable first, KISS receiver with KISS on | running: `escTaskRunning()` |
| Park | `escTaskPark()`, from the claim only, and `escTaskInit()` | the drain if a throttle frame went out within 250 ms, then today's teardown, then SIG driven low and AUX on its pull-down | parked: `escTaskParked()` |
| Release | `escTaskSuspend()`, from the claim only, as today | the drain if one is owed, the teardown, or from Park: SIG and AUX back to today's handover state, both inputs, SIG with the pull-up | released: `escTaskSuspended()`, as today |

- `escTaskSuspended()` stays the guard of `core1JobStart()`
  (`src/core1_job.cpp` 63) and is true only in Release. A job never binds
  on a parked pin.
- From Park to Release no drain is due: nothing was sent.
- `escTaskConfigure()` (a SETUP change) in Park re-parks on the new wiring:
  the old SIG goes to the reset-like state, input with the pull-down and the
  buffer off, then the new SIG is driven low. In Run it rebuilds as today,
  drain included.
- A teardown clears `initError`, so `DSERR` names the current bind only.
- The decision is a pure function in `src/esc_task.h`, as
  `escHandoverStep()` is (`src/esc_task.h` 1164 to 1171), so the host suite
  tests every transition with the fake pump of `test/fakes.cpp`
  (`fakePumpPoll()`, lines 952 to 977).

Only `src/pin_claim.cpp` calls the three: it already makes the only calls
of `escTaskSuspend()` and `escTaskResume()` outside `src/esc_task.cpp`
(`grep` over `src/`). `escTaskResume()` is renamed `escTaskStart()`.

## The lifetime

### The rule

DShot runs while the screen on top of the navigation stack is ESC TESTER
or ESC COMMANDS, and, by [decision 6](#decisions), while SETUP is on top
of ESC TESTER. Everywhere else it is parked. While MENU's run cue stands,
ESC TESTER and ESC COMMANDS open on the start step of
[decision 8](#decisions) with DShot parked, until START DSHOT. The screen's
own way out stays live on the step: the tester's `MENU` and ESC COMMANDS'
BACK return with DShot parked and the cue kept. The step's text, START
DSHOT and that way out are all the step draws and takes touches on; the
screen's other controls and readouts are hidden and inert
([decision 14](#decisions)).

The UI decides it once a frame: `uiDshotWanted()` is a pure function of the
stack of menu-c.md's [navigation](menu-c.md#navigation), of the run cue
(`uiAm32RunCue()`, `usbBridgeRunCue()`, `sbusOutRunCue()`, ORed as MENU
reads them) and of whether START DSHOT was tapped since the screen opened.
`uiTick()` hands its result to `pinClaimDshot(bool)` before the screen's
tick, so a tap that changes the stack takes effect at the next `uiTick()`,
one frame later. Asking
every frame, rather than on each transition, means no route can leave
DShot in the wrong mode: a screen added later is parked unless the rule
names it.

`pinClaimDshot(true)` acts only while the claim is Free: it asks core1 for
Run, and from then on Run is wanted. `pinClaimDshot(false)` acts only while
Free and Run is wanted: it asks for Park, and from then on Run is not
wanted, whether core1 still drains or has parked. "Run is wanted" is the
claim's own record of its last call, not the pump's state. ESC TESTER
and ESC COMMANDS keep `freePins` (menu-c.md, requirement 3), so through the
UI they open only while the claim is Free.

```
                 power-up
                    |
                    v
   +-----------------------------+
   | Free, DShot parked          |  MENU, hubs, LOGS, SETUP from MENU
   | SIG low, AUX on pull-down   |<--------------------------------+
   +-----------------------------+                                 |
     |                  ^      |                                   |
     | ESC TESTER or    |      | a claiming tool opens             |
     | ESC COMMANDS     |      v                                   |
     | opens            |   Stopping: Release, no drain, or the    |
     v                  |   rest of a running tester's drain       |
   +------------------+ |      |                                   |
   | Free, DShot runs | |      v                                   |
   | ESC TESTER,      |-+   Held -> Running -> Leaving             |
   | ESC COMMANDS,    |     (the leave page, as today)             |
   | SETUP over the   |      |                                     |
   | tester (dec. 6)  |      v                                     |
   +------------------+   Parking: core1 parks SIG and AUX --------+
     leave: disarm, the drain if owed, Park
```

### Each case

| Case | What happens | Why |
|---|---|---|
| ESC TESTER opens | the tap pushes the tester; the next `uiTick()` reads the stack and asks for Run; core1 binds on its next pass; the first 10 frames are the EDT enable, then zero-throttle frames | `escPumpBind()` sets disarmed and throttle 0 at every bind (`src/esc_task.h` 910 to 917) |
| The ESC's zero-throttle frames before it arms | AM32 takes commands only after more than 1 s of zero input, and arms on its own rule; Bluejay's and BLHeli_S's times are not recorded here | [esc-commands.md](esc-commands.md#safety-and-interlocks), "1500 ms of answers"; not measured |
| HOLD TO ARM | its 1000 ms hold (`ARM_HOLD_MS`, `src/config.h` 540) could complete 1 s after the tap, with at most one UI frame, 25 ms, between the tap and the first frame. Decisions 10 and 12 add a gate: arming is refused until 1500 ms after core1's last bind, `ESC_CMD_SETTLE_MS`'s margin over AM32's 1 s. The gate holds while `escTaskBoundMs()` is 0 or less than 1500 ms old; a stamp that moves cancels a timed press. The stamp is 0 from the tap until core1's bind, so the frame between them is gated too. The gate starts again at every bind: the open, a rebuild after a SETUP change over the tester, START DSHOT. Meanwhile HOLD TO ARM is drawn disabled, a press that lands on it is not timed, and the track reads `DSHOT STARTING: WAIT 1.5 S`. The earliest arm is 2500 ms after the bind | today the tester appears at the same moment DShot starts, after the splash, and the same race exists once per power-up; the design repeats it at every open |
| Leaving while armed | the MENU tap runs `testerDisarm()` (`src/ui.cpp` 1005 to 1012, 1015 to 1020): throttle 0, LATCH off; then Park. Core1 sends zero frames until more than 250 ms after the last throttle frame (`escDrainDone()`, `src/esc_task.h` 1135 to 1138), then stops | the ESC hears zero before silence, as before every handover today |
| Leaving disarmed | Park at once when no throttle frame went out within 250 ms | `escHandoverStep()`: a pump with no recent throttle frame has nothing to drain |
| SETUP from the readouts | they act only disarmed (`readoutsAct()`, `src/ui.cpp` 456). Decision 6: DShot stays up under SETUP | see decision 6 |
| SIG PIN, AUX PIN, the rate or KISS TELEM changed in SETUP | over the tester, DShot running: `applyLive()` → `escTaskConfigure()` rebuilds as today, drain included, so a new SIG PIN binds DShot on the new pin while SETUP is on screen, and the pin note names that pin; the arm gate starts again at the rebuild. From MENU, parked: the change re-parks on the new wiring and binds on the next ESC TESTER | `src/ui_settings.cpp` 857 to 863; `src/esc_task.cpp` 333 to 349 |
| LOGS: an auto log | ends at the disarm that leaving the tester runs: `sdLogArmAction()` stops only the log arming began (`src/sd_log.h` 410 to 420) | as today |
| LOGS: a log started by hand | runs on, as today across MENU ([blackbox-logging.md](blackbox-logging.md#lifecycle)); while DShot is parked its frames carry throttle 0 and no fresh telemetry. The eRPM column is logged only while fresh, as the EDT columns are (`src/sd_log.cpp` 421 against 428 to 431); today a frozen last eRPM would be logged | a hand log is the user's; ending it on a screen change would surprise more than an empty stretch |
| The run summary | written at the log's stop, from the frames; its eRPM is taken only while fresh (`src/run_summary.cpp` 134, 135). Unchanged | |
| ESC COMMANDS opens | Run, as the tester. The page reads `NO ESC ANSWERS ON SIG GP0.` until the ESC's first eRPM answer, then `ESC STARTING: WAIT 1.5 S.` for 1500 ms (`ESC_CMD_SETTLE_MS`, `src/config.h` 368; `escCmdReadiness()`, `src/esc_task.h` 811 to 821); its controls are drawn disabled until Ready (`src/ui_esc_cmd.cpp` 184 to 188, 252 to 258). Slice D3 reads `DSHOT STARTING ON SIG GP0.` in place of the first text for the first 1500 ms | the ESC needs time to see DShot; "no ESC" in that time is not yet known |
| Beacons and the EDT enable | the EDT enable goes out at each bind; a beacon pressed before Ready waits at most 2000 ms and expires (`ESC_CMD_WAIT_MS`); BACK is drawn disabled while a request waits or a sequence goes out (`src/ui_esc_cmd.cpp` 403), so leaving never cuts a sequence | as today |
| ESC COMMANDS closes | BACK → hub; Park with no drain: the page sends no throttle | |
| The kV estimate | on the armed tester only; it keeps no value after the disarm ([esc-commands.md](esc-commands.md#decisions), decision 7); every open starts disarmed. Unchanged | |
| The 3D note | a flag in RAM (`src/esc_3d.h`), set by ESC COMMANDS and ESC SETTINGS, read by the tester. Unchanged: the flag does not depend on DShot | |
| The 30 s idle disarm | on the tester only (`src/ui.cpp` 1478 to 1487); DShot keeps sending zero frames while the tester stays open | the tester is open; the rule is the owner's |
| The touch-loss stop | see [#49 and the lifetime](#49-and-the-lifetime) | |
| The BOARD voltage | read every frame on core0 from the ADC before any screen ticks (`src/ui.cpp` 1400 to 1405) and drawn on the tester's status bar; the red band refuses KST, Hitec and AM32 writes (`uiBoardWriteRefused()`, `src/ui.h` 99 to 115). Independent of DShot. With MENU the start screen it is not on the first screen; where it shows is menu-c.md's and ui-theme.md's | |
| The run cue | stays MENU's second strip line; cleared as today by a LEAVE sent, by the leave page's last button, whose steps power-cycle the ESC, or by an eRPM frame newer than the answer, which only ESC TESTER and ESC COMMANDS now receive. ESC SETTINGS and S.BUS OUTPUT set it on a bootloader's answer; USB BRIDGE sets it on every claim (`src/ui_bridge.cpp` 150 to 153), and decision 13 clears it at a claim's end that no ESC answered. Decision 8: a start step while it stands; decision 14: `MENU` or BACK leaves the step with DShot parked and the cue kept | |
| A claiming tool opens from a hub | Run is not wanted, so the claim is not Busy and asks for Release. From Park: no drain, the pins free on core1's next pass. Within 250 ms of leaving a running tester the pump still drains: the claim stays in Stopping until the rest of that drain is sent, then Release | `escHandoverStep()`: the drain owed is taken before any release |
| A claiming tool leaves | the leave page as today; its last button parks instead of restarting DShot, and returns to the hub | menu-c.md, requirement 13 and decision 7 |

## The pin claim

The smallest change keeps every state and every user, and changes what
"Free" and the last state mean. The claim stays the one owner of SIG and
AUX: it alone asks core1 for Run, Park and Release.

### States

| State | Today | With the lifetime |
|---|---|---|
| Free | DShot owns both pins | no claim; DShot parked, or running for ESC TESTER and ESC COMMANDS (`pinClaimDshotRunning()`) |
| Stopping | the pump drains and releases | Release: from Park at once; while the pump still drains after Run, once the rest of the drain is sent, which only a claim opened within 250 ms of leaving a running tester meets |
| Held, Running, Leaving | as today | as today |
| Restoring | the pump rebuilds; Free once `escTaskSuspended()` is false | renamed **Parking**: core1 parks SIG and AUX; Free once `escTaskParked()` |

"Not Free" on MENU and in the hubs then lasts only for the USB bridge's
release, at most 250 ms (`BRIDGE_RELEASE_MAX_MS`), while the claim is
Leaving, and for Parking, one core1 pass. The busy caption, on MENU and on
the hubs, reads `RELEASING SIG GP0`, 17 characters, while the claim is
Leaving, and `SETTING SIG GP0 LOW`, 19 characters, while it is Parking.

### API

| Today | With the lifetime |
|---|---|
| `escTaskResume()` in `restore()` (`src/pin_claim.cpp` 60 to 63) | `escTaskPark()`; the state is Parking |
| none | `pinClaimDshot(bool on)`: Free only; Run or Park |
| none | `pinClaimDshotRunning()`: Free and the pump running |
| `pinClaimRestart()`, RESTART DSHOT | `pinClaimRelease()`: the same gate (`pinClaimUnplugDue()`), then Parking |
| `pinClaimBegin()` → `escTaskSuspend()` | unchanged, but refused as Busy while Run is wanted, a backstop: no route opens a claim from ESC TESTER or ESC COMMANDS. After `pinClaimDshot(false)` it is not Busy; during the rest of a drain it waits in Stopping |

`pinClaimCheck()` (`src/pin_claim.h` 222 to 235) keeps its rules. Two of
them lose their source while DShot is parked: EscAnswering reads
`lastRpmMs`, and KissLive reads `kissLastMs`, which only a running pump
updates. Each then refuses only within 500 ms of the last DShot answer,
after ESC TESTER or ESC COMMANDS closed. See [safety](#safety) and
decision 7.

### The 250 ms drain

The drain stays in `escHandoverStep()` and is taken wherever the pump stops
after a throttle frame: leaving the tester with the motor driven within 250 ms,
and a SETUP change over the running tester, which owes a drain only when the
tester was disarmed less than 250 ms before. A claim no longer starts a drain:
the pump it releases is parked, or, within 250 ms of leaving the tester,
still finishing the drain that leaving started. This is the reading of owner
decision 2 at the top: the tools get the pin with no drain of their own.

### The leave page

The page keeps its three phases, STAY, LEAVE, its BACK drawn disabled and
the 1000 ms wait of its last button (`UiLeave::waitMs`, `src/ui_leave.cpp`
33 to 35; [menu-b.md](menu-b.md#decisions) owner decisions 3, 25 and 30).
What it guards changes: the last button no longer starts DShot; a servo,
receiver or flight controller left on SIG meets DShot at the next ESC
TESTER or ESC COMMANDS instead. The unplug step keeps its place and says
that.

| Step | Today | With the lifetime |
|---|---|---|
| The unplug step, line 1 | `UNPLUG THE SERVO.` | unchanged |
| line 2 | `DSHOT RESTARTS ON SIG GP0 AND READS` (`UI_LEAVE_DSHOT_FMT`, `src/ui_leave.h` 108) | `ESC TESTER'S DSHOT ON SIG GP0 READS`, 35 characters, the step's limit (`UI_LEAVE_STEP_CHARS`) |
| line 3 | `AS A 970 US PULSE AT 1000 HZ.` | unchanged |
| The last step | `RESTART DSHOT.` (`UI_LEAVE_RESTART`, line 112) | decision 9: `BACK TO ` and the title of the hub the tool opened from, and a full stop: `BACK TO SERVO.`, `BACK TO RECEIVER.` or `BACK TO ESC PROGRAM.`, 20 characters at most; before menu-c.md slice 5 `BACK TO RX SIGNALS.` and `BACK TO ESC CONFIG.`, 19 |
| The wide button | `RESTART DSHOT` (`src/ui_leave.cpp` 120) | decision 9: the hub's title as the hub draws it, `SERVO`, `RECEIVER` or `ESC PROGRAM` (11 characters at scale 2, 130 px); a way back, so `C_CYAN` with a `C_ONACCENT` label from ui-theme.md slice 11 ([ui-theme.md](ui-theme.md#cyan)) |
| SERVO TESTER's left button | `BACK TO SERVO` | decision 9: `NEXT SERVO`, as KST SERVO and HITEC SERVO, because SERVO is the hub's name |
| NEXT SERVO, BACK TO MONITOR, BACK TO OUTPUT | `pinClaimKeep()` | unchanged |

`BACK TO ESC PROGRAM` at scale 2 would be 226 px in the 228 px button
(`UI_LEAVE_WIDE`, `src/ui_leave.h`), which is why the label is the title
alone, as the tester's `MENU` button names where it leads.

### The core1 jobs

No job changes. A job's release leaves its pad as it does today; the
Parking that follows sets SIG low and AUX on its pull-down, on core1, in
`escTaskPoll()`, which `loop1()` runs before `core1JobPoll()`
(`src/main.cpp` 273 to 276). Parking is asked only once the job is idle
(`leaveStep()`, `src/pin_claim.cpp` 78 to 82), so it never touches a pin a
job still drives. The heartbeat rules of
[pin-claim.md](pin-claim.md#heartbeat-and-watchdog) are unchanged; with the
pump parked the heartbeat has no throttle to zero.

Held keeps today's pad: from Park, Release puts SIG on the pull-up, as the
teardown does today. HITEC SERVO waiting for READ, and ESC SETTINGS between
greetings, see the line they see today
([pin-claim.md](pin-claim.md#sequence) step 3).

## Safety

### What can no longer happen

- DShot at power-up into a servo left on SIG. The splash's warning goes.
- DShot on MENU and in the hubs into a servo, a receiver or a flight
  controller plugged in there.
- The leave page's last button sending DShot into a servo nobody unplugged
  or into an EFM8 bootloader still powered: it sends nothing.
- A powered EFM8 ESC entering its bootloader because the line rests high
  after DShot stops: SIG rests low (from the firmwares' source, not
  measured).
- A KISS receiver running on AUX while no ESC is meant to be there.

### What new risk appears

1. **DShot stops mid-run on a screen change.** Only the MENU tap leaves
   the tester while armed; it disarms first, and core1 sends zero frames
   for more than 250 ms after the last throttle frame before it stops.
   What the ESC does then is its firmware's signal-loss behaviour, not
   measured here: AM32's firmware resets itself 0.5 s after its last valid
   frame while armed and 2 s while disarmed, and Bluejay returns to its
   no-signal state about 320 ms after the last frame
   ([bluejay-blheli-s.md](bluejay-blheli-s.md#entering-the-bootloader),
   from source). The motor is at throttle zero by then. A motor coasting
   with a propeller keeps turning until it stops by itself.
2. **No ESC check on the claims.** SERVO TESTER, KST SERVO, HITEC SERVO and
   S.BUS OUTPUT are refused today while an ESC answers on SIG, and two
   servos while KISS frames arrive on AUX
   ([pin-claim.md](pin-claim.md#interlocks)). With DShot parked nothing
   answers, so both checks pass. Servo pulses of 1000 to 2000 µs into an
   ESC are a valid PWM throttle. HOLD TO START still guards the servo
   tester's first pulse, the S.BUS OUTPUT probe still refuses START while
   an ESC's bootloader answers ([sbus-output.md](sbus-output.md)), and KST
   and Hitec entries send frames on their entry tap. Decisions 7 and 11: the
   hubs SERVO and RECEIVER warn before that tap, from slice D2 on.
3. **Each open is a fresh start.** Every open of ESC TESTER is a new bind
   with the ESC detecting DShot again; without a gate HOLD TO ARM could
   complete before an AM32 has seen 1 s of zero input. The ESC then would
   not arm and would ignore the throttle until it reads zero; not measured.
   Decisions 10 and 12, from slice D2 on.
4. **DShot into a waiting bootloader on the next open.** After an EFM8
   answered and no LEAVE went out, an ESC that was not power-cycled waits
   in its bootloader; the next ESC TESTER sends DShot into it. Today
   RESTART DSHOT does the same at once. The run cue on MENU names it.
   Decision 8, from slice D3 on; between D2 and D3 the next open sends
   DShot as RESTART DSHOT does at cff09ae.
5. **SIG driven low against a device that drives it.** See
   [SIG and AUX while idle](#the-choice-sig-driven-low-aux-on-its-pull-down).
6. **An AM32 left powered on MENU restarts itself** every 2 s with no
   frames, by the source reading above; whether it beeps at each start is
   not known.

### A finger slipping between screens

- Every button acts on release except DISARM, which acts on press
  (README, "Button behaviour"). The tap that leaves a screen has ended
  before the next screen reads a press, so a press does not carry over.
- The tester's MENU button (x 176 to 233, y 283 on, `BTN_MENU`,
  `src/ui.cpp` 173, 177) and MENU's first row, ESC TESTER (y 62 to 101,
  menu-c.md), do not overlap; a second tap at the same place does not
  reopen the tester.
- A quick MENU then ESC TESTER starts DShot again shortly after its stop;
  an ESC that had not yet lost the signal hears zero frames again, and the
  tester opens disarmed. HOLD TO ARM's press must land on ARM after
  the tester opens (`s_armLive`, `src/ui.cpp` 1137 to 1142).
- HOLD TO ARM's finger cannot slide onto MENU and leave: a tap needs its
  press and its release inside the button (`inputTapped()`,
  `src/ui_input.cpp` 19 to 23), and that press began on ARM.

### #49 and the lifetime

#49, merged as 3b36588, adds `uiTouchLost()`, set once a frame in
`uiTick()` right after the touch read (`touchLostNow()`, `src/ui.cpp`).
The touch counts as lost while the controller has not answered since
`touchInit()`, or after 4 failed reads in a row (`TOUCH_LOST_READS`) and
100 ms since its last answer (`TOUCH_LOST_MS`, `src/config.h`).
`touchInit()` clears the record: no answer and `s_pollFails` 0
(`src/cst328.cpp`). It fits without a change on either side:

- **ESC TESTER**: a lost touch disarms by the path of DISARM and the idle
  interlock, `testerStop()`, and refuses arming; `drawButtons()` draws
  HOLD TO ARM disabled while it lasts. DShot keeps sending zero frames:
  the tester is still open, and with no touch no screen change can happen.
  A lost touch never starts or stops DShot. Decision 10's gate and #49's
  refusal both refuse arming, each on its own condition, and HOLD TO ARM
  is drawn disabled while either refuses ([decision 12](#decisions)).
- **ESC COMMANDS**: #49 adds nothing; the page's sequences end by
  themselves and it sends no throttle.
- **SERVO TESTER, S.BUS OUTPUT**: they stop as STOP does and keep the
  claim; SIG stays the job's. HOLD TO START is refused while the touch is
  lost. SERVO TESTER's notes (`safeNotes()`, `src/ui_servo.cpp`) read
  `TOUCH NOT ANSWERING` and then `THE SERVO IS STOPPED.` or
  `HOLD TO START WORKS ONCE IT ANSWERS.`; S.BUS OUTPUT records the stop as
  `SbusOutStop::Touch` and reads `STOPPED: TOUCH DID NOT ANSWER`. Core1's
  250 ms cut of the servo's supply stays as at cff09ae.
- **MENU, hubs, LOGS, SETUP from MENU**: nothing is driven, so nothing has
  to stop.
- #49's README text, "with DShot off there is no motor to stop" for the
  USB bridge, stays true.

D2 and every slice after it keep #49's host cases.

## Texts and documents that assume DShot from boot

### Screen texts

Widths are at scale 1, 6 px a character less 1, unless named.

| Text | Where | Change | Slice |
|---|---|---|---|
| `UNPLUG ANY SERVO FROM GP0 NOW` on the splash | `src/ui.cpp` 1291 to 1296; `UI_SPLASH_PIN_FMT`, `src/ui.h` 166 to 171 | the splash draws it no more; the macro stays for SETUP's pin note, documented as SETUP's | D2 |
| SETUP's pin note, the same text | `pinNote()`, `src/ui_settings.cpp` 707 to 714 | shown only while DShot runs under SETUP (decision 6), against the pin the pump runs on, not the pin of the last load | D4 |
| `LINK  998/S  0% ERR` | `drawLink()`, `src/ui_settings.cpp` 783 to 801 | while DShot is parked the value reads `OFF` in `C_DIM` | D4 |
| `DSHOT RESTARTING ON SIG GP0` | `UI_MENU_RESTARTING_FMT`, `src/ui_menu.h` 48 | `RELEASING SIG GP0`, 17 characters, while the claim is Leaving; `SETTING SIG GP0 LOW`, 19, while it is Parking | D2 |
| `WAIT: DSHOT RESTARTING` | `UI_WAIT_RESTART_TEXT`, `src/ui_widgets.h` 121 | `WAIT: THE PINS ARE BUSY`, 23 characters, inside the 37 of `src/ui_bridge.cpp` 78 | D2 |
| `EDT ON`, `EDT OFF`, `NO ESC` on MENU | `src/ui_menu.cpp` 198 to 206 | the chip goes (menu-c.md, decision 12); ESC COMMANDS keeps it | D2 |
| `DSHOT RESTARTS ON SIG GP0 AND READS` | `UI_LEAVE_DSHOT_FMT`, `src/ui_leave.h` 108 | `ESC TESTER'S DSHOT ON SIG GP0 READS`, 35 | D2 |
| `RESTART DSHOT.`, `RESTART DSHOT` | `src/ui_leave.h` 112; `src/ui_leave.cpp` 120 | the hub's step and label, decision 9 | D2 |
| `DSHOT RESTARTS ON SIG GP0.` | `SBUS_UI_RESTART_FMT`, `src/ui_sbus.h` 96; `SBOUT_UI_RESTART_FMT`, `src/ui_sbus_out.h` 137 | `ESC TESTER SENDS DSHOT ON SIG GP0.`, 34 | D2 |
| `DSHOT RESTARTS ON SIG GP1, WHERE` | `SBUS_UI_RESTART_GP1_1`, `src/ui_sbus.h` 98 | `SIG GP1 IS HELD LOW, WHERE`, 26, over the unchanged `THE RECEIVER DRIVES THE LINE.` | D2 |
| `LEAVE, THEN DSHOT RETURNS.` | `AM32_UI_EXIT_2`, `src/ui_am32.h` 230 | `LEAVE. ESC TESTER THEN RUNS IT.`, 31, inside `RESULT_CHARS` 37 | D2 |
| `PLUG IT IN AFTER RESTART DSHOT.` | `AM32_UI_LEAVE_2`, `src/ui_am32.h` 253, the last step of the page after an EFM8 answer | `PLUG IT IN ONCE ESC PROGRAM SHOWS.`, from `PLUG IT IN ONCE %s SHOWS.` with the hub's title: 34 characters, 33 with `ESC CONFIG`, inside `UI_LEAVE_STEP_CHARS` 35. The ESC is powered again once SIG is parked low, where an EFM8 does not enter its bootloader | D2 |
| `DSHOT STAYS ON THE PIN UNTIL THEN.` | `HITEC_UI_ESC_2`, `src/ui_hitec.h` 91 | `NOTHING IS SENT UNTIL THEN.`, 27 | D2 |
| `DSHOT RUNS ON SIG GP0` | `UI_TOOLS_STRIP_FMT`, `src/ui_tools.h` 33 | goes with TOOLS | menu-c.md slice 4 |
| `NO ESC ANSWERS ON SIG GP0.` in the first 1500 ms | `UI_ESC_CMD_NO_ANSWER_FMT`, `src/ui_esc_cmd.h` 95 | `DSHOT STARTING ON SIG GP0.`, 26, until 1500 ms after the bind; then as today | D3 |
| new: the tester's track while the arm gate holds | beside `UI_TESTER_CARD_TEXT`, `src/ui.h` 160 to 164 | decisions 10 and 12: `DSHOT STARTING: WAIT 1.5 S`, 26 characters, the card text's length, inside its `static_assert` (`src/ui.cpp` 965) | D2 |
| new: the second strip line of the hubs SERVO and RECEIVER | `uiStripTwoLines()` under the hub's caption | decisions 7 and 11: `NO ESC CHECK: UNPLUG ANY ESC FIRST.`, 35 characters, 209 px, `C_AMBER`, while the claim is Free | D2 |
| new: MONITOR's second line while SIG is GP1 | the hub RECEIVER's entry `MONITOR` | `GP1 IS SIG: PLUG IN AFTER THE TAP.`, 34 characters, 203 px, `C_AMBER` ([what it costs](#the-choice-sig-driven-low-aux-on-its-pull-down)) | D2 |
| new: the start step while the run cue stands | ESC TESTER and ESC COMMANDS, on the leave page widget | decision 8: `1 POWER-CYCLE THE ESC.` over `ITS BOOTLOADER MAY STILL WAIT` and `AND READ DSHOT AS A COMMAND.`; `START DSHOT`, 228 px wide in the row of the widget's left and right buttons, y 214 ([decision 14](#decisions)) | D3 |
| `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.`, `PROPELLER OFF. DSHOT IS OFF.`, `THE TESTER SENDS DSHOT ONLY:`, `AND MAY READ DSHOT AS ONE.` | `src/ui_hitec.h` 85; `src/ui_bridge.h` 115; `src/ui_am32.h` 151, 251 | unchanged: still true | |
| `AN ESC ANSWERS ON SIG GP0.` and its lines on SERVO TESTER, KST SERVO, HITEC SERVO, S.BUS MONITOR, S.BUS OUTPUT | `src/ui_servo.cpp` 509, 510; `src/ui_kst.cpp` 604 to 606; `src/ui_hitec.h` 89; `src/ui_sbus.h` 86; `src/ui_sbus_out.h` 107 to 111 | unchanged; reachable within 500 ms of the last answer | |

The texts that state DShot's restart or return change in D2, the slice that
makes them false, so no slice leaves such a text on `main`. A slice that
removes a text adds it to rule 9 of `test/check_docs.py`.

Comments that state the boot order change with the code: `src/main.cpp`
21 to 35, 148 to 150, 251 to 276; `SPLASH_MS`, `src/config.h` 661 to 667,
whose reason, the ESC's boot, goes; the splash keeps its 3000 ms for its
notes; `src/pin_claim.h` 1 to 72; `src/ui_menu.h` 12 to 17;
`src/ui_leave.h` 1 to 40; `src/esc_task.h` 1290 to 1312.

### Documents

Each changes in the slice that changes its code (slice D5 for the rest),
by [menu-b.md](menu-b.md#decisions) owner decision 27.

| Document | Where | Change |
|---|---|---|
| README.md | "Core split", lines 137 to 173 | the pump runs only for ESC TESTER and ESC COMMANDS; a claim releases a parked pump; leaving parks |
| README.md | "Using it", "Splash", lines 429 to 452 | no DShot at the splash's end; the pin line goes |
| README.md | "Using it", MENU and the claim's busy state, lines 513, 577 to 603 | `RELEASING SIG GP0`; no `DSHOT RESTARTING` |
| README.md | "Telemetry and logging", lines 929 to 933 | the pump starts when ESC TESTER or ESC COMMANDS opens, not at power-up |
| README.md | "ESC commands", line 1387 | DShot starts when the page opens |
| README.md | "AM32 ESC configuration" and "A Bluejay or BLHeli_S ESC", lines 1455, 1485 to 1494, 1703 to 1729 | LEAVE and the leave page's last step |
| README.md | "Servo tester", "KST", "Hitec", "S.BUS monitor", "S.BUS output", "USB bridge", lines 1988 to 1996, 2319 to 2329, 2485 to 2490, 2662, 2738, 2944 to 2948, 3086 to 3088, 3205 to 3234 | the leave page's step 2 and last step |
| README.md | "Safety", lines 3478, 3494 to 3496, 3534 to 3538 | "Boots disarmed and sends MOTOR_STOP continuously" becomes "Boots with DShot off; SIG held low"; the claim's ESC check sees an ESC only within 500 ms of its last answer, and the hubs SERVO and RECEIVER warn `NO ESC CHECK: UNPLUG ANY ESC FIRST.` (decision 11) |
| wiki, English and German | The-Screens and Die-Bildschirme (29 and 30 lines matching, case-insensitive, restart dshot, dshot restart, dshot returns, dshot starts, dshot runs, dshot is off, dshot comes back, splash and unplug any servo, or their German forms), First-Run and Erste-Schritte (12, 6), Servo-Tester and Servotester (8, 6), KST-Programming and KST-Programmierung (10, 10), Hitec-Programming and Hitec-Programmierung (13, 13), RX-Signals and RX-Signale (9, 9), USB-Bridge and USB-Bruecke (6, 4), AM32-Configuration and AM32-Konfiguration (5, 6), Troubleshooting and Fehlersuche (8, 4), Setup and Einrichtung (5, 2), Home (2) | as README.md |
| [pin-claim.md](pin-claim.md) | "The two pins", "The claim", "Sequence" steps 2 and 6, "Interlocks", "When the pin is unavailable", "Restoring DShot" | Free and Parking; the drain only after a running tester; the ESC check's source |
| [kiss-telemetry.md](kiss-telemetry.md) | "Receiving", "Where the code runs" | the receiver runs only with the pump |
| [blackbox-logging.md](blackbox-logging.md) | "Lifecycle" | a hand log across a parked pump; eRPM only while fresh |
| [esc-commands.md](esc-commands.md) | "The page" | DShot starts with the page |
| [bluejay-blheli-s.md](bluejay-blheli-s.md), [usb-bridge.md](usb-bridge.md), [sbus-output.md](sbus-output.md), [rx-signals.md](rx-signals.md), [servo-tester.md](servo-tester.md), [kst-programming.md](kst-programming.md), [hitec-programming.md](hitec-programming.md), [current-sense.md](current-sense.md), [menu.md](menu.md), [menu-b.md](menu-b.md) | their descriptions of RESTART DSHOT and of DShot returning | as built, in each slice; their decisions get revision lines |
| `docs/bench-plan.md` | its checks of RESTART DSHOT and of DShot after a claim | the parked line and the next ESC TESTER |
| STATUS.md, CHANGELOG.md | | per slice |

The decisions and passages this revises carry a "Revised by" line now,
marked "Not built", with "pending the owner's review" for each part that
comes from a developer's call: [pin-claim.md](pin-claim.md#decisions)
decisions 3 and 4, [menu-b.md](menu-b.md#not-changed) "The tester is the
root", [esc-commands.md](esc-commands.md#decisions) decision 5,
[kiss-telemetry.md](kiss-telemetry.md#receiving),
[blackbox-logging.md](blackbox-logging.md#lifecycle), menu-b.md owner
decisions 3 and 30 and decision 21, which name RESTART DSHOT, by the line of
[decision 9](#decisions), and menu-c.md's
[requirement 4](menu-c.md#what-the-structure-requires-of-dshot), by the line
of decision 6. The build marks each line built when its slice merges.

## Slices

D1 to D4 suit Opus, D5 Sonnet, as [menu-c.md](menu-c.md#build-plan) names
the models. Each slice is host-tested and mutation-checked: each mutation
listed must make a named test fail, and is restored. Each slice updates its
own README, wiki pages in both languages, previews, CHANGELOG and STATUS
lines (menu-b.md owner decision 27). The order against
[menu-c.md](menu-c.md#build-plan):

| Slice | After | What | Model |
|---|---|---|---|
| D1 | anything; before or with menu-c.md slice 1. Built in pull request #119 (branch `ccr-pump-modes`), with the amendments [D1](#d1-the-pumps-three-modes-opus) lists ([D1 as built](#d1-as-built)) | the pump's three modes and core1's bind stamp, no caller changes | Opus |
| D2 | menu-c.md slices 2, 3 and 4 (the stack, MENU as root, the hubs) and D1 as amended; #49 is on `main` as 3b36588. Built on branch `ccr-dshot-d2` ([D2 as built](#d2-as-built)) | the lifetime: boot parked, the rule, the claim's Parking; the arm gate; the hubs' line in place of the ESC check; the texts that state DShot's restart; MENU's chip goes | Opus |
| D3 | D2. Built on branch `ccr-dshot-d3`, on menu-c.md slice 5 ([D3 as built](#d3-as-built)) | ESC COMMANDS on a starting pump; the start step while the run cue stands; the bridge's run cue; the log's eRPM | Opus |
| D4 | D2 and menu-c.md slice 5 (the rename). Built on branch `ccr-dshot-d4`, on D3 ([D4 as built](#d4-as-built)) | SETUP's pin note and LINK; the comments of the boot order | Opus |
| D5 | D4, with menu-c.md slice 6 (the records) | the design notes' descriptions, bench plan | Sonnet |

D2 follows menu-c.md slice 4, so its tests open every tool from its hub,
and it needs no rename: the texts it adds take the hub's title as the hub
draws it. The graph has no cycle: slices 1 to 6 are a chain, D1 joins
before slice 1 or with it, D2 hangs off slice 4 and D1, D3 off D2, D4
off D2 and slice 5, D5 off D4 and slice 6.

### What each slice leaves on main

- **From menu-c.md slice 3 to D2** MENU is the first screen, and DShot runs
  from boot behind it on every screen, as at cff09ae. The splash's
  `UNPLUG ANY SERVO FROM GP0 NOW` and MENU's EDT chip are true then. No
  slice in this stretch is less safe than cff09ae.
- **From D2 to D3** ESC COMMANDS reads `NO ESC ANSWERS ON SIG GP0.` during
  the ESC's first 1500 ms; ESC TESTER and ESC COMMANDS open with DShot
  while the run cue stands, as RESTART DSHOT starts it at cff09ae; USB
  BRIDGE's cue stands after a claim no ESC answered, as at cff09ae; a hand
  log across a parked pump logs the last eRPM, frozen. No safety text is
  false: D2 changes every text that names a DShot restart.
- **From D2 to D4** SETUP opened from MENU shows its pin note after a SIG
  PIN change, though DShot is parked: it warns of what the next ESC TESTER
  sends. LINK does not read `OFF` while parked.
- **From D4 to D5** the design notes' descriptions, not their decision
  lines, still describe RESTART DSHOT as built at cff09ae.

0.1.0 follows slice 14 of menu-c.md and D5 (owner decision 4 at the top),
as [the build plan](menu-c.md#build-plan) says.

### D1: the pump's three modes (Opus)

D1 is built in pull request #119 (branch `ccr-pump-modes`), not merged,
with the four amendments below ([D1 as built](#d1-as-built)):

1. **The bind stamp** `escTaskBoundMs()`, as described below, in
   `src/esc_task.{h,cpp}`, with the fake pump of `test/fakes.cpp` setting
   it as core1 does.
2. **Its test:** `escTaskBoundMs()` is 0 before the first bind, the fake
   clock at the bind, the new clock at a rebuild after
   `escTaskConfigure()`, and 0 once parked and once released.
3. **Its mutation:** the bind stamp left unchanged at a rebuild; the test
   of item 2 fails.
4. **The `highEnables` test:** SIG's output latch is preset high
   (`gpio_put(SIG, 1)`) before the park, because the fake's `gpio_init()`
   (`test/fake_i2c.cpp`) sets the direction and the function but does not
   clear the latch, as the SDK's does. The park then leaves SIG an output
   driving 0 with the 2 mA drive, and the fake's `highEnables` count for SIG
   stays 0 across it. Its mutation: `gpio_set_dir()` before `gpio_put(0)`
   in `escPadSet()`'s `EscPad::Low` case; the test fails.

D2 relies on the amended #119: its arm gate reads the stamp.

What D1 builds. `src/esc_task.{h,cpp}`: `EscPinMode` Run, Park, Release;
the pure step extended from `escHandoverStep()` to the wanted mode, the
mode reached, the rebuild request, the pump, the last throttle stamp and
the clock; a pure `escParkPlan()` naming each GPIO's state for a wiring
change while parked; `escTaskStart()`, `escTaskPark()`, `escTaskParked()`,
`escTaskRunning()`; `escTaskSuspended()` true only in Release; the park on
core1 in `escTaskPoll()`; `initError` cleared at a teardown.

The park drives SIG low with no high glitch: SIG's SIO output has value 0
(`gpio_put()`) and 2 mA before the direction becomes output. The pad is
never an output with the value 1.

Core1 stamps `millis()` at each bind, whatever its cause, and publishes it
to core0 as `escTaskBoundMs()`, as `escHeartbeatMs()` crosses the cores:
one aligned 32-bit word, written by core1 after the bind, read by core0. A
bind at `millis()` 0 is stamped 1, so 0 never names a bind. The stamp is 0
while the pump is parked or released: core1 writes 0 when it starts a
teardown. D2's arm gate reads it.

`test/fakes.cpp` follows the pure step. Boot still asks for Run, and the
claim still calls `escTaskResume()` under its new name, so no behaviour
changes.

Host tests (`test/test_claim.cpp` or a new `test/test_dshot_life.cpp`):

- Park after a throttle frame sends zero frames until more than 250 ms
  after it, then parks; Park with no throttle frame parks on the first pass.
- With SIG's latch preset high, the park leaves SIG an output driving 0
  with the 2 mA drive, and the fake's `highEnables` count for SIG stays 0
  across it.
- Release from Park releases on the first pass with no frame.
- `escTaskSuspended()` is false while parked, and `core1JobStart()` is
  refused then.
- A wiring change while parked: the old SIG in the input, pull-down, buffer
  off state, the new SIG low, AUX on its pull-down; while running: a
  rebuild, as `testHandoverRules()` checks today.
- Run from Park binds once, with the EDT enable first.
- `escTaskBoundMs()` is 0 before the first bind, the fake clock at the
  bind, the new clock at a rebuild after `escTaskConfigure()`, and 0 once
  parked and once released.

Mutations that must fail them: Park without the drain; `escTaskSuspended()`
true while parked; the old SIG left driven low after a swap; a teardown
that keeps `initError`; the direction set to output before the value 0 (the
`highEnables` case); the bind stamp left unchanged at a rebuild.

#### D1 as built

Pull request #119, branch `ccr-pump-modes`, not merged; host tests only,
not tested on hardware.

- **The bind stamp** is `escTaskBoundMs()` in `src/esc_task.{h,cpp}`.
  `pumpTeardown()` writes 0 before it stops the driver, and `pumpBind()`
  writes `millis()`, 0 made 1, last, after a barrier (`__dmb()`). The fake
  pump of `test/fakes.cpp` sets it in the same places.
- **Its test** is `testBoundMs()` in `test/test_claim.cpp`.
  `fakePumpPowerOn()` puts the fake pump back to its power-on state, and
  `escTaskBoundMs()` reads 0 there, before the first bind.
- **The `highEnables` case** is in `testParkDrain()`. The test presets
  SIG's latch high before the park (`sigLatchHigh()`), as a job could leave
  it. The fake's `gpio_init()` (`test/fake_i2c.cpp`) clears the latch, as
  the SDK's does.
- **`EscPad::Low`** in `escPadSet()` writes the value 0, the 2 mA drive and
  the pulls off first. A pad that is already an SIO output keeps its
  direction, so it is never undriven; any other pad gets the direction in.
  Then the SIO function, and the direction out. `testParkAlreadyLow()`
  parks a SIG a job left as an SIO output at 0 and counts the GPIO writes
  that leave it undriven: 0.
- **The wiring latch.** `escTaskConfigure()` stores the wiring, then a
  barrier, then the rebuild request. Core1 clears the request, then a
  barrier, then reads the wiring. A configure that lands during core1's
  reads can be latched half-applied for one pass, while disarmed; the
  request still set rebuilds it on the next pass (`testConfigureMidway()`).
- **The source check** `testPollSource()` reads `src/esc_task.cpp` by the
  path the test Makefile passes (`MRB_TEST_SRC_DIR`), and checks the order
  of each step the fake pump copies, each barrier among them.

### D2: the lifetime (Opus)

D2 starts once #119, amended as [D1](#d1-the-pumps-three-modes-opus)
lists, has merged; #49 is on `main` as 3b36588. The arm gate reads
`escTaskBoundMs()` and `uiTouchLost()`.

`escTaskInit()` asks for Park and parks SIG on core0. `src/pin_claim.*`:
`pinClaimDshot()`, `pinClaimDshotRunning()`, `restore()` parks,
`ClaimState::Restoring` renamed `Parking`, `pinClaimRestart()` renamed
`pinClaimRelease()`, Busy while Run is wanted. `src/ui.cpp`:
`uiDshotWanted()`, pure, and the call to `pinClaimDshot()` once a frame
before the screen's tick; until D3 its run cue input is false. The leave
page's last button parks and returns to the hub (menu-c.md slice 2 already
routes it).

With it, so that no false safety text and no unguarded path reaches
`main`:

- **The arm gate** of decisions 10 and 12, on `escTaskBoundMs()`, with
  HOLD TO ARM drawn disabled and `DSHOT STARTING: WAIT 1.5 S` in the track.
  The gate holds while `escTaskBoundMs()` is 0 or less than 1500 ms old; a
  stamp that moves cancels a timed press.
- **The hubs' line** of decisions 7 and 11 on SERVO and RECEIVER, and
  MONITOR's second line while SIG is GP1.
- **The texts** of [the table above](#screen-texts) marked D2, with their
  fit `static_assert`s and decision 9's labels. The removed names go into
  rule 9 of `test/check_docs.py`: `RESTART DSHOT`, `DSHOT RESTARTING`,
  `DSHOT RESTARTS ON`, `LEAVE, THEN DSHOT RETURNS`.
- **MENU's EDT chip goes** (menu-c.md decision 12), moved here from
  menu-c.md slice 3: with D2 it would read a parked pump.

Host tests, by control id, each tool opened from its hub:

- After `uiInit()`: the screen is MENU, the pump is parked, no bind
  happened (`fakeRebuildCount()` 0), the splash drew no pin line, and
  MENU's strip holds no chip.
- The ESC TESTER row binds once; the tester's MENU button parks: with the
  tester armed and throttled, zero frames for more than 250 ms first.
- ESC COMMANDS from its hub: open binds, BACK parks with no drain.
- For every screen of menu-c.md's table but ESC TESTER, ESC COMMANDS and
  SETUP over the tester: `uiDshotWanted()` is false, and the pump parked
  after one frame.
- SETUP from the readouts: DShot stays up; a SIG PIN change there binds on
  the new pin while SETUP shows, and the bind stamp moves.
- Busy: after `pinClaimDshot(true)` a `pinClaimBegin()` is refused as Busy.
  After `pinClaimDshot(false)` with the tester throttled 100 ms before, a
  `pinClaimBegin()` is not Busy, stays in Stopping until the drain has run
  past 250 ms, then reaches Held. With no drain owed each claiming tool
  from its hub reaches Held on the first pump pass.
- Each tool's leave, by the leave page's last button and by BACK where it
  has none: Parking, then Free, no bind, the screen its hub. The wide
  button reads the hub's title and the last step `BACK TO ` and that
  title. The hub's busy caption reads `SETTING SIG GP0 LOW` while Parking and
  `RELEASING SIG GP0` while the bridge's release runs.
- `pinClaimDshot(true)` while a claim holds does nothing.
- The arm gate: for 1499 ms after the bind HOLD TO ARM is drawn disabled
  and the track reads `DSHOT STARTING: WAIT 1.5 S`; a press that lands at
  1499 ms and is held 3000 ms neither arms nor draws badge progress; a
  press that lands at 1500 ms arms at 2500 ms, its progress growing as
  today. A SIG PIN change in SETUP over the tester starts the gate again.
  While #49's touch loss refuses arming, HOLD TO ARM is drawn disabled too.
- The frame between the tap and the bind: the ESC TESTER row is tapped, and
  the test runs `uiTick()` frames with core1's pass held back, so
  `escTaskBoundMs()` reads 0 while the tester shows; HOLD TO ARM is drawn
  disabled, the track reads `DSHOT STARTING: WAIT 1.5 S`, and a press held
  through those frames is not timed. Once core1 binds, the gate counts
  1500 ms from the stamp.
- A rebuild under SETUP during a timed press: the tester is 1500 ms past
  its bind and a press on HOLD TO ARM is timed for 500 ms; the test then
  calls `escTaskConfigure()` with a new SIG PIN, as SETUP's `applyLive()`
  does, and core1 rebinds. The stamp moves, the press is cancelled, its
  progress is gone, it does not arm at 1000 ms, and the gate holds 1500 ms
  from the new stamp. No UI path reaches SETUP with a finger on HOLD TO
  ARM; the case checks the rule, which any later rebind path meets.
- The hubs SERVO and RECEIVER draw `NO ESC CHECK: UNPLUG ANY ESC FIRST.`
  in `C_AMBER` as the strip's second line while the claim is Free; ESC
  PROGRAM draws none; the busy caption takes its place while not Free.
  MONITOR's `GP1 IS SIG: PLUG IN AFTER THE TAP.` shows with SIG on GP1
  only.
- Each D2 text of the table by control id and text; rule 9 refuses the
  removed names.
- #49's cases pass unchanged.

Mutations: `restore()` calls `escTaskStart()` (the leave cases fail);
`uiDshotWanted()` true for MENU (the boot case fails); Parking skipped,
Free before `escTaskParked()` (the Parking case fails); `pinClaimDshot()`
acting while not Free (its case fails); `escTaskInit()` asking for Run (the
boot case fails); Busy read from the pump's state, not from Run wanted (the
drain case fails); the gate stamped at the tap, not at core1's bind (the
SETUP case fails); a press timed while the gate holds (the 1499 ms case
fails); a stamp of 0 read as no gate (the tap-to-bind case fails); a timed
press kept when the stamp moves (the rebuild-under-SETUP case fails); the
hubs' line dropped on RECEIVER (its case fails).

#### D2 as built

Branch `ccr-dshot-d2`, on `main` after pull request #125 (menu-c.md slice
4); host tests only, not tested on hardware.

Files: `src/esc_task.{h,cpp}` (`escTaskInit()` parks, `s_want` Park from the
start); `src/pin_claim.{h,cpp}` (`pinClaimDshot()`, `pinClaimDshotRunning()`,
`pinClaimRelease()`, `ClaimState::Parking`, Busy while Run is wanted);
`src/ui.{h,cpp}` (`uiDshotRule()`, `uiDshotWanted()`, `uiArmGated()`,
`uiNavOpenerTitle()`, the arm gate, the call once a frame, the splash);
`src/config.h` (`ARM_GATE_MS`); `src/ui_menu.{h,cpp}` (the busy captions,
`uiBusyCaption()`, the chip gone); `src/ui_hub.{h,cpp}` and
`src/ui_registry.cpp` (`UiHub::warn`, `UiHubEntry::note` and `noteWhen`);
`src/ui_rx_signals.{h,cpp}` (MONITOR's line); `src/ui_leave.{h,cpp}`
(`uiLeaveBackStep()`, the wide button's label); the tools' leave pages and
texts in `src/ui_servo.*`, `src/ui_kst.*`, `src/ui_hitec.*`,
`src/ui_sbus.*`, `src/ui_sbus_out.*`, `src/ui_bridge.*`, `src/ui_am32.*`;
`src/ui_widgets.h`; the splash comment in `src/main.cpp`.

The state machine as built:

| From | Event | To | Core1 |
|---|---|---|---|
| power-up | `escTaskInit()` | Free, Run not wanted | parked on core0 before core1 starts |
| Free, Run not wanted | `pinClaimDshot(true)`: ESC TESTER, ESC COMMANDS or SETUP over the tester on top | Free, Run wanted | binds on its next pass, the EDT enable first; the bind stamp |
| Free, Run wanted | `pinClaimDshot(false)`: any other screen on top | Free, Run not wanted | disarmed; zero frames while a throttle frame is 250 ms old or less, then the teardown and the park |
| Free, Run wanted | `pinClaimBegin()` | refused, Busy | none |
| Free, Run not wanted | `pinClaimBegin()` | Stopping | the rest of a drain, if one is owed, then Release |
| Stopping | `escTaskSuspended()` | Held | as before |
| Held, Running, Leaving | as before | as before | as before |
| Leaving | the leave page's last button (`pinClaimRelease()`), or the job idle for a claim with no page | Parking | parks SIG low and AUX on its pull-down |
| Stopping | BACK (`pinClaimEnd()`) | Parking | parks, after the rest of a drain |
| Parking | `escTaskParked()` | Free, Run not wanted | none |
| any but Free | `pinClaimDshot()` | unchanged | none |

`uiDshotRule()` is pure: ESC TESTER or ESC COMMANDS on top, or SETUP over
ESC TESTER; its `cueHolds` input, the run cue standing with START DSHOT not
tapped, is false until D3. `uiInit()` hands it to `pinClaimDshot()` once too,
which at power-up asks for nothing.

Where the build chose, the note being silent:

- **The hubs' strip.** The hub's own line stays the first line; the second
  line is the busy caption while the claim is not Free, else `UiHub::warn`.
  ESC CONFIG, which has no warning, shows its busy caption as a second line
  too. MONITOR's GP1 line shows while SIG is GP1, whatever the claim's state.
- **The busy caption** reads `SETTING SIG GP0 LOW` in Parking and
  `RELEASING SIG GP0` in every other state that is not Free.
- **The gate's text** is amber, as the card's text in the same place.
- **The leave page of ESC SETTINGS** keeps its two steps; step 2 is
  `PLUG IT IN ONCE ESC CONFIG SHOWS.` from `AM32_UI_LEAVE_2`, and the wide
  button reads the hub's title. The hubs' titles are those before menu-c.md
  slice 5: `BACK TO RX SIGNALS.` and `BACK TO ESC CONFIG.`, 19 characters.
- **The wide button's colour** stays `C_PANEL`: its `C_CYAN` is ui-theme.md
  slice 11's.
- **The control ids** named for RESTART DSHOT are `Release` (`ServoCtl`,
  `KstCtl`, `HitecCtl`, `SbusCtl`, `SbusOutCtl`, `BridgeCtl`, `Am32Ctl`,
  `UiLeaveEv`), and `ServoCtl::BackToServo` is `NextServo`.
- **`src/main.cpp`**: the splash's comment changes with the code; the other
  comments of the boot order and `SPLASH_MS` stay for D4.
- **A quick MENU, then ESC TESTER, within the drain** finds the pump still
  bound: Run is asked again before the teardown, the pump runs on with no new
  bind, and the gate counts from the earlier bind. The ESC heard zero frames
  throughout. Arming afterwards needs the 1000 ms hold only, since the
  earlier bind is older than 1500 ms by then.
- **The track after a refused bind.** `DSHOT STARTING: WAIT 1.5 S` shows
  only while a bind is pending or younger than 1500 ms. After the driver
  refused the bind (`initError`, `DSERR` in the status bar), the stamp stays
  0 and the gate holds; the track then reads `DSHOT COULD NOT START`
  (`UI_TESTER_REFUSED_TEXT`), amber, 21 characters, 125 px of the track's
  width. The track reads `escTaskBindRefused()`, a flag core1 sets in each
  bind and clears in each teardown. No bind starts before the last pump's
  teardown, so the flag is clear while a bind is pending, and a refused bind
  leaves the stamp 0: core0 reads the stamp, then the flag, and a pending
  bind never reads as refused.

Tests: `test/test_dshot_life.cpp`, thirteen cases, and in `test/test_claim.cpp`
the drain after a running tester, Parking, the park as the claim left it,
and `testPollSource()`'s check of `escTaskInit()`. The host suites that
claimed from the tester leave it for MENU first; the job suites keep the pump
running before a claim (`freshPump()`), since their own pad writes are
under test. `fakes.cpp` gains `escTaskInit()`, `fakeParkCount()`,
`fakeParkJobIdle()`, `fakePinsReset()` and `fakeSetBindRefused()`.

Mutations, each run against the whole suite and restored:

| Mutation | Result |
|---|---|
| `restore()` calls `escTaskStart()` | the suite fails from the first leave on, "the pins parked, no DShot", and stops there |
| `uiDshotWanted()` true for MENU | 1867 checks fail, among them "MENU alone: false" of the rule case |
| Parking skipped: Free before `escTaskParked()` | 40 checks fail, among them "parking until core1 parks the pins" |
| `pinClaimDshot()` acting while not Free | 8 checks fail, "Run while parking does nothing" first |
| `escTaskInit()` asking for Run | 1 check fails: `testPollSource()`'s "escTaskInit() seeds the wiring, then asks for Park" |
| The fake's `escTaskInit()` asking for Run | 11 checks fail, among them the power-up case's "a second of MENU: parked, no bind, no frame" |
| Busy read from the pump's state, not from Run wanted | 2545 checks fail, the first "100 ms after the throttle frame: the claim is not busy" |
| The gate stamped at the tester's open, not at core1's bind | 20 checks fail, among them "back on the tester: the gate holds from the new bind" (the SETUP case) |
| A press timed while the gate holds | 1 check fails: "a press that lands at 1499 ms, held 3000 ms: no arm" |
| The gate's backstop in the timed branch dropped too | none: equivalent, since a live press needs the gate open when it lands and a stamp that moves cancels it |
| A stamp of 0 read as no gate | 162 checks fail, among them "the same press, held 3000 ms past the bind: no arm" (the tap-to-bind case) |
| A timed press kept when the stamp moves | 1 check fails: "held on 3000 ms: no arm" (the rebuild case) |
| The hubs' line dropped on RX SIGNALS | 4 checks fail, among them the RX SIGNALS case of the hubs' line |
| Run asked only once the pins are parked (`pinClaimDshot()`) | 20 checks fail, among them the drain race's "ESC TESTER again: no new bind, the stamp unchanged, never parked" |
| The fake's `escTaskStart()` asking for a rebuild | 18 checks fail, among them four of the drain race's, "zero frames throughout" with them |
| The track's refused branch dropped: the gate's text after a refused bind | 1 check fails: "the track reads the refused text in amber, not the gate's" |
| `pumpTeardown()` not clearing the refused flag | 1 check fails, the source order "the refused flag cleared, a barrier, then the stamp 0" |
| The fake's teardown not clearing the refused flag | 2 checks fail, among them "a bind pending: the gate's text, not the refused text" |

### D3: ESC COMMANDS and the run cue on a starting pump (Opus)

`DSHOT STARTING ON SIG GP0.` on ESC COMMANDS for the first 1500 ms after
the bind with no answer; the start step of decision 8, with
`uiDshotWanted()` reading the run cue, and its way out and START DSHOT's
place by decision 14; decision 13's rule for the bridge's cue, with
`bridgeWireBinds()` in `src/bridge_wire.*` and `usbBridgeRunCueSettle()`,
and the fakes' `fakeCsExitOnce()` and `fakeBridgeLineInject()`; the log's
eRPM column only while fresh (`src/sd_log.cpp` 421).

START DSHOT clears every source of the cue: `uiAm32RunCueClear()`, new,
`usbBridgeRunCueClear()`, and `sbusOutRunCueClear()`, new, which marks the
last answer as `sbusOutProbeRestart()` does.

Host tests:

- ESC COMMANDS: the first text for 1499 ms after the bind with no answer,
  then `NO ESC ANSWERS ON SIG GP0.`; an answer gives `ESC STARTING: WAIT
  1.5 S.` as today.
- Decision 8: while the cue stands, from each of its three sources, ESC
  TESTER and ESC COMMANDS open with `uiDshotWanted()` false, the pump
  parked and the start step; START DSHOT is drawn disabled for 999 ms and
  acts at 1000 ms after the step opens; the tap binds, clears the cue and
  starts the arm gate. With no cue both open running.
- Decision 14: on the start step, the tester's `MENU` returns to MENU and
  ESC COMMANDS' BACK to ESC PROGRAM, each live from the step's first
  frame and drawn live; after either the pump is still parked, no bind
  happened (`fakeRebuildCount()` unchanged), no frame went out, and the
  cue still stands, so the next open shows the step again. START DSHOT
  lies inside y 214 to 259 and does not overlap `BTN_MENU`. On the step
  the screen's list of controls holds only START DSHOT and the way out,
  and:
  - A tap at `TGT_MAX`'s centre, x 111, y 240, in the step's first 999 ms,
    while START DSHOT is disabled (later the same tap is START DSHOT's):
    the screen stays ESC TESTER on the step, SETUP does not open, no bind
    happens and the pump stays parked.
  - A 1000 ms press on HOLD TO ARM's rectangle, then a swipe along the
    track below START DSHOT, y 270 from x 30 to x 200: nothing arms, the
    throttle stays 0, no frame goes out, no bind happens, and the track's
    rectangle from y 260 to 281 holds only `C_BG`.
  - On ESC COMMANDS, a 1500 ms hold on 3D ON above START DSHOT, x 178,
    y 200: no command is queued or sent, no bind happens, START DSHOT does
    not act, and 3D ON's rectangle from y 192 to 213 holds no pixel of
    the 3D ON button's fill or label (the step's note lines may reach
    y 200).
  - `uiDshotWanted()` for the stack ESC TESTER under SETUP, with the cue
    standing and START DSHOT not tapped: false.
- Decision 13, on the host fakes (`test/fake_bridge_line.h`, core1's side
  run by `core1JobPoll()` as in `test/test_bridge_ui.cpp`). Each case
  reads `usbBridgeRunCue()` first. Then it taps BACK through `uiTick()`
  until ESC PROGRAM shows, and opens ESC TESTER: running when the cue is
  clear, on the start step when it stands. Cases (b) and (c) call
  `uiBridgeTick()` directly, which returns false and skips `src/ui.cpp`'s
  tool dispatch, so their next `uiTick()` reaches ESC PROGRAM by that
  BACK: `back()` returns false once the claim is neither Held nor Running.
  Case (a) needs a frame loop that runs the pump pass and not
  `core1JobPoll()`; `test/test_bridge_ui.cpp` gains it beside `brFrames()`
  and `framesNoPump()`.
  - (a) The claim Held, its start pending with core1's pass held back,
    BACK: no bind, no leave page, the cue is clear.
  - (b) The race: the claim Held, its start pending; the test calls
    `uiBridgeTick()` with BACK tapped and core1's bind put after the
    frame's `pinClaimPoll()`. No frame loop can put a core1 pass there, so
    `test/fakes.cpp` gains `fakeCsExitOnce()`, a one-shot hook that runs a
    function after the next `critical_section_exit()`: the first one in
    that tick is `pinClaimPoll()`'s read of the job's phase, and the hook
    runs `core1JobPoll()`, which binds. The line opened once
    (`fakeBridgeLineOpens()` 1), `bridgeWireBinds()` moved, nothing went
    on the wire (`fakeBridgeLineWireCount()` 0), `uiBridgeTick()` returned
    false, no leave page: the cue is clear.
  - (c) The race of (b), after the first claim of (e), so 500 received
    and 400 sent are left over. The hook also puts one byte on the wire
    from the model ESC unasked (`fakeBridgeLineInject()`, new) and runs
    core1's passes until the job has counted it: `received` 1, `sent` 0.
    No leave page: the cue stands.
  - (d) The line bound, the claim Running, the leave page's last button:
    the cue is clear, as today.
  - (e) Leftover counts: a first claim's line sends 400 bytes and receives
    500, the model ESC's answers, and the claim ends at the leave page's
    last button. A second claim's binds fail (`fakeBridgeLineRefuse()`
    with a PIO error): the first, then RETRY's. Each zeroes the counts,
    moves `bridgeWireBinds()` and opens nothing (`fakeBridgeLineOpens()`
    unchanged). BACK: no leave page, Parking, then Free with no bind; the
    cue is clear.
  - (f) The leftovers of (e); the second claim ends by BACK while Held,
    before its line binds: the leftovers are not read, and the cue is
    clear.
  - (g) The leftovers of (e); the second claim begins with no pump pass
    (`framesNoPump()`), so it stays Stopping. BACK: `pinClaimEnd()`
    restores at once (`src/pin_claim.cpp` 191 to 194), no job starts,
    `bridgeWireBinds()` does not move, no leave page: the cue is clear.
- The log: a frame with an eRPM 500 ms or older logs 0; a fresh one logs
  its value (`make -C test lognum` and the blackbox checker).
- #49's cases pass unchanged.

Mutations: the start text kept past 1500 ms; START DSHOT live at once; START
DSHOT leaving the bridge's cue set; the settle clearing whatever the wire
received (case c fails); the settle never clearing (cases a, b, e, f and g
fail); the settle run only after a job error or with `bridgeWireBinds()` unmoved
(case b fails); the settle taking the counts' difference against a stamp at
`usbBridgeRunCueSet()` (case c fails: 1 less 500 and 0 less 400 wrap, and the
difference reads as no more received than sent); the counts read before
`bridgeWireBinds()` moves (cases e, f and g fail: the leftovers set the flag);
`bridgeJobBind()` zeroing the counts only after a successful `bridgeLineOpen()`,
`bridgeWireBinds()` still raised at every bind (case e fails); `MENU` drawn
disabled on the start step, or its tap clearing the cue (the decision 14 case
fails); the readouts' targets left live on the step (the `TGT_MAX` case fails);
HOLD TO ARM left live or the track drawn on the step (the swipe case fails); ESC
COMMANDS' holds drawn on the step (the 3D ON case fails); `uiDshotWanted()` true
for SETUP over the tester whatever the step (its case fails); the eRPM logged
without the freshness check.

#### D3 as built

Branch `ccr-dshot-d3`, on `main` after pull request #128 (menu-c.md slice
5); host tests only, not tested on hardware.

Files: `src/ui.{h,cpp}` (`uiRunCue()`, `uiRunCueClear()`, the start step's
state and `uiStartStepEnter()`, `uiStartStepShown()`, `uiStartStepLive()`,
`uiStartStepTouch()`, `uiStartStepDraw()`, `testerStepTick()`,
`TesterCtl::Start`, `uiDshotWanted()` reading the cue); `src/ui_leave.{h,cpp}`
(the step's texts, `UI_LEAVE_START`, `UiLeaveStart`, `uiLeaveStartOpen()`,
`uiLeaveStartLive()`, `uiLeaveStartTouch()`, `uiLeaveStartDraw()`);
`src/ui_esc_cmd.{h,cpp}` (`UI_ESC_CMD_STARTING_FMT`, `dshotStarting()`,
`stepTick()`, `EscCmdCtl::Start`); `src/ui_menu.cpp` (MENU reads
`uiRunCue()`); `src/ui_am32.{h,cpp}` (`uiAm32RunCueClear()`);
`src/sbus_out_job.{h,cpp}` (`sbusOutRunCueClear()`); `src/bridge_wire.{h,cpp}`
(`bridgeWireBinds()`); `src/usb_bridge.{h,cpp}` (the watch, `cueWatch()`,
`usbBridgeRunCueSettle()`); `src/ui_bridge.cpp` (`back()` calls the settle);
`src/sd_log.{h,cpp}` (`sdLogErpmField()`).

Where the build chose, the note being silent or the safer reading taken:

- **The cue the rule reads.** `uiDshotWanted()` holds DShot back while the
  start step shows or while the cue stands now: `cueHolds` is the step's
  latch ORed with `uiRunCue()`. The step is latched at the screen's open, so
  a cue that cleared by itself under the step would not start DShot before
  START DSHOT; a cue that rose under a running screen parks DShot. Both read
  the safe way. Only START DSHOT ends the latch, and it clears every source.
- **One latch for both screens.** `ui.cpp` keeps one step state; ESC TESTER's
  and ESC COMMANDS' opens each set it from the cue (`uiStartStepEnter()`).
  Returning to the tester from SETUP does not reopen it, and SETUP cannot be
  reached from the step.
- **What the step draws.** On the tester: the status bar, which holds no
  control, the step from y 62 down, START DSHOT and MENU. On ESC COMMANDS:
  the header with BACK and the strip with its chip, the step and START
  DSHOT. Everything decision 14 lists is neither drawn nor read: the step
  ticks in place of the screen's tick, so `handleMainTouch()`, the BEEP
  buttons, the holds and the message lines never run.
- **START DSHOT's lift.** The frame START DSHOT acts on returns before the
  tester's touch handling, and the tester draws whole from the next frame:
  the same lift over MAX would otherwise open SETUP.
- **A press before 1000 ms.** START DSHOT takes only a press that lands
  while it is live, as the leave page's last button; one that lands at
  999 ms and lifts later does nothing.
- **START DSHOT's colour.** Live, it is `C_PANEL` with a `C_TEXT` label, as
  HOLD TO START today; disabled, the outline. Its `C_GREEN` is ui-theme.md
  slice 11's, as D2 left the wide button's `C_CYAN` to it. MENU is already
  `C_CYAN`.
- **The first frame.** The tap that opens a screen pushes it at the end of
  its frame; the step is drawn and read from the next frame, the screen's
  first, and START DSHOT's 1000 ms count from the tap's frame. DShot is
  asked for on no frame between.
- **ESC COMMANDS' start text.** `DSHOT STARTING ON SIG GP0.` while the bind
  stamp is 0 and no bind was refused, and for `ESC_CMD_SETTLE_MS`, 1500 ms,
  after the stamp. After a refused bind the stamp stays 0 and the page reads
  `NO ESC ANSWERS ON SIG GP0.`, which stays true; the stamp is read before
  the refused flag, as the tester's track reads them.
- **S.BUS OUTPUT's clear.** `sbusOutRunCueClear()` marks the last answer and
  keeps the EFM8 mark, which belongs to the leave page.
- **The settle ends the watch.** `usbBridgeRunCueSettle()` reads the counts
  once more, clears the cue only with the flag clear, and ends the claim's
  watch, so a second BACK of the same screen changes nothing.
  `usbBridgeRunCueClear()` ends it too.
- **A cue that stands at a claim.** Clearing the flag at each claim would
  let a second claim ended before its bind, BACK with no leave page, clear
  the cue a first claim's ESC byte kept, while the ESC may wait in its
  bootloader. `usbBridgeRunCueSet()` sets the flag to whether the cue
  already stands (`s_escSeen = s_cue;` before
  `s_cue = true;`): the settle clears only a cue this claim raised.
  Decision 13 carries a "Revised by" line. The rule on the counts is
  `usbBridgeEscByteSeen()`, a pure function the tests call.
- **The eRPM column.** `sdLogErpmField()` gives `EscTelemetry::erpm` while
  `escFieldFresh()` holds with `ESC_LINK_STALE_MS`, 500 ms, as
  `src/run_summary.cpp` takes eRPM: a frame 499 ms old logs its value, one
  500 ms old logs 0. lognum tests the function, and decodes the eRPM column
  of a log across a parked pump from the card: the last eRPM while under
  500 ms old, then 0.
- **The tests' cue sources.** The AM32 cue is set through `testAm32CueSet()`
  in `test/test_ui.cpp`, which includes `src/ui_am32.cpp`; the bridge's by
  `usbBridgeRunCueSet()`; S.BUS OUTPUT's by a real probe answer in
  `test/test_sbus_out_ui.cpp`. Suites that leave an ESC SETTINGS session's
  cue standing clear it before they open the tester again, as the ESC's
  power cycle would; the bridge case that expected the cue after failed binds
  now expects decision 13's clear.
- **Decision 13's leftovers.** Cases (c), (e), (f) and (g) each start from a
  fresh first claim whose line sends 400 bytes and receives 500 (a model ESC
  that answers each 100-byte frame with 25 bytes), ended at the leave page's
  last button.

Tests: `test/test_dshot_life.cpp` `testEscCmdStarting()`, `testStartStep()`
(decision 8 from the AM32 and bridge sources on both screens, and both screens
with no cue), `testStartStepWayOut()` (decision 14); the S.BUS OUTPUT source
in `test/test_sbus_out_ui.cpp`; decision 13's cases (a) to (i) in
`test/test_bridge_ui.cpp` `testCueAtClaimEnd()`, with `framesNoJob()`,
`fakeCsExitOnce()` (`test/fakes.cpp`) and `fakeBridgeLineInject()`
(`test/fake_bridge_line.cpp`), and `usbBridgeEscByteSeen()`'s rule, among it a
byte lost or a collision with no more received than sent; `testErpmFresh()`
and `testErpmLogged()` in `test/sd_log_fatfs.cpp` (`make -C test lognum`).
Case (h) is the review's: case (c), back to ESC PROGRAM, a second claim Held
with its start pending, BACK; the first claim's cue stands. Case (i): a byte
the line received is read by `usbBridgeTask()`, not by the settle, before
core1 binds again and zeroes the counts; the cue stands. Case (c) also settles
twice. The blackbox checker encodes through `gen_blackbox_log.cpp`, not
`sdLogTick()`, so it does not see the column's freshness; it passes unchanged.
#49's cases pass unchanged.

Mutations, each run against the whole suite (lognum for the last two) and
restored:

| Mutation | Result |
|---|---|
| The start text kept past 1500 ms (`dshotStarting()` true once bound) | 4 checks fail, the first "1500 ms: NO ESC ANSWERS ON SIG GP0." |
| START DSHOT live at once | 29 checks fail, the first "999 ms on, START DSHOT disabled and drawn so, parked, no bind, no frame" |
| START DSHOT live only past 1000 ms (`>` for `>=`) | 1 check fails: "START DSHOT: not live at 999 ms, live at exactly 1000 ms" |
| START DSHOT leaving the bridge's cue set | 8 checks fail, the first "START DSHOT ends the step and clears every cue" |
| START DSHOT leaving ESC SETTINGS' cue set | 15 checks fail, the same first |
| START DSHOT leaving S.BUS OUTPUT's cue set | 1 check fails: "START DSHOT: the cue gone, the EFM8 mark kept, DShot binds once" |
| The settle clearing whatever the wire received | 2 checks fail: case (c), "the cue stands", and its step |
| The settle never clearing | 11 checks fail, among them cases (a), (b), (e), (f) and (g) |
| The settle clearing only with `bridgeWireBinds()` unmoved | 5 checks fail, among them cases (b) and (e) |
| `back()` not calling the settle | 11 checks fail, as the settle never clearing |
| The counts' difference against a stamp at `usbBridgeRunCueSet()` | 6 checks fail, among them case (c)'s second settle |
| The counts read before `bridgeWireBinds()` moves | 6 checks fail, among them cases (e), (f) and (g) |
| `bridgeJobBind()` zeroing the counts only after a successful open, `bridgeWireBinds()` still raised at every bind | 3 checks fail, among them case (e)'s "a failed bind: the counts zeroed" |
| `bridgeWireBinds()` raised only after a successful open | 2 checks fail: case (e) |
| `usbBridgeRunCueSet()` clearing the flag, as first built | 2 checks fail: case (h), "the first claim's cue: the cue stands" |
| `usbBridgeTask()` not running `cueWatch()` | 2 checks fail: case (i), "a byte seen by usbBridgeTask(): the cue stands" |
| A lost byte not counted as the ESC's | 1 check fails: "a byte lost with no more received than sent is the ESC's" |
| A collision not counted as the ESC's | 1 check fails: "a collision with as many received as sent is the ESC's" |
| The settle not ending the watch | none: equivalent. After a settle the cue is clear, and only a new `usbBridgeRunCueSet()` raises it, which starts a new watch; or it stands with the flag set, and only a new `usbBridgeRunCueSet()` sets the flag again |
| MENU drawn disabled on the start step | 1 check fails: "MENU live from the first frame, drawn cyan" |
| MENU's tap on the step clearing the cue | 16 checks fail, among them "MENU: MENU, parked, no bind, no frame, the cue kept" |
| ESC COMMANDS' BACK on the step clearing the cue | 2 checks fail, the first "BACK: ESC PROGRAM, parked, no bind, no frame, the cue kept" |
| The readouts' targets left live on the step | 23 checks fail, among them the `TGT_MAX` case |
| HOLD TO ARM and the track left live on the step (`handleMainTouch()` on the step) | 23 checks fail, among them the swipe case |
| The track drawn on the step | 5 checks fail, among them "the track's rows, y 260 to 281, hold only the background" |
| START DSHOT's lift fed to the readouts on its frame | 18 checks fail, among them "past 1000 ms the same tap is START DSHOT's: the tester, no SETUP" |
| ESC COMMANDS' holds drawn on the step | 5 checks fail, among them the 3D ON case's rows |
| ESC COMMANDS' holds read on the step | 1 check fails: "a 1500 ms hold on 3D ON's place: no command, no bind, the step stays" |
| `uiDshotRule()` true for SETUP over the tester whatever the step | 2 checks fail, among them the rule case's "with the cue held (D3): false" |
| `uiDshotWanted()` not reading the step's latch | none at first: the cue stands throughout the step in every case. A case added: an eRPM frame clears the cue under the step; then 1 check fails, "the cue cleared under the step: the step stays, parked, no bind, no frame" |
| `uiDshotWanted()` not reading the cue, the latch alone | none at first: no tool sets a cue while the tester shows. A case added: a cue set under a running tester; then 1 check fails, "a cue that rises under the tester: not wanted, parked" |
| The tester's open not latching the step | 40 checks fail |
| ESC COMMANDS' open not latching the step | 29 checks fail |
| The eRPM logged without the freshness check (`sdLogErpmField()`) | 5 checks of lognum fail, the first "500 ms old: 0" |
| `sdLogTick()` writing the raw eRPM | 2 checks of lognum fail, one per volume: "0 from 500 ms on" |

### D4: SETUP's texts and the comments (Opus)

SETUP's pin note and LINK of [the table above](#screen-texts); the comments
that state the boot order.

Host tests: the pin note shows only while DShot runs under SETUP over the
tester, naming the pin the pump runs on; from MENU after a SIG PIN change
it does not show; LINK reads `OFF` in `C_DIM` while parked and its rate
while running.

Mutations: the pin note against the pin of the last load (the over-tester
case fails); LINK's value drawn while parked (the `OFF` case fails).

#### D4 as built

Branch `ccr-dshot-d4`, on branch `ccr-dshot-d3` (pull request #129); host
tests only, not tested on hardware.

Files: `src/ui_settings.cpp` (`pinNote()`, `drawLink()`, the fit
`static_assert`s), `src/ui_settings.h` (`UI_SETTINGS_LINK_OFF`, the file's
LINK paragraph), `src/ui.h` (`UI_SPLASH_PIN_FMT`'s comment), `src/settings.h`
(`settingsBootDshotPin()`'s comment); the comments of the boot order in
`src/config.h` (`SPLASH_MS`), `src/main.cpp` (the core table, `setup1()`,
`loop1()`) and `src/esc_task.h` (the file comment, `escTaskBegin()`).

The pin note: `pinNote()` returns `UI_SPLASH_PIN_FMT` with
`escTaskDshotPin()` while `pinClaimDshotRunning()` holds: the claim Free,
Run wanted and core1 reporting Run. Only SETUP over ESC TESTER wants Run
while SETUP shows, so the note shows there alone, at the pin of the last
load too. `UNPLUG ANY SERVO FROM GP29 NOW`, the widest, is 30 characters,
179 px, from x 8 to 186; a `static_assert` in `src/ui_settings.cpp` checks
the format's length against x 232.

LINK: while `escTaskParked()` holds the value reads `OFF` in `C_DIM`,
right-aligned at y 242, 17 px from x 215; the label `LINK`, 23 px from x 8,
stays. A `static_assert` keeps 6 px between them. The repaint cache keeps
whether the last draw read `OFF`, so the row repaints when the pump parks or
leaves Park with no other change on the screen.

Where the build chose, the note being silent:

- **The swap note keeps its rule.** With KISS on and SIG off the pin of the
  last load (`settingsBootDshotPin()`), the line reads
  `SWAP THE ESC'S SIGNAL AND KISS WIRES` as before, parked or running, and
  takes the line's place over the pin note: it describes the wiring the next
  ESC TESTER sends on, which a step from MENU changes too. The table names the
  pin note only.
- **Running is core1's report.** The note reads `pinClaimDshotRunning()`,
  not `uiDshotWanted()`: it shows while core1 reports Run, from SETUP's
  first frame over a running tester, and not during the drain after one.
  A refused bind reports Run too (`escTaskRunning()`), so the note shows
  while `DSERR` stands; nothing is sent then, and the note errs the safe way.
- **Parked is `escTaskParked()`.** During the drain after a throttled tester,
  at most 255 ms, SETUP opened from MENU reads the rate while zero frames go
  out, then `OFF` once core1 has parked.
- **The comments.** `src/pin_claim.h`, `src/ui_menu.h`, `src/ui_leave.h`,
  `src/ui.cpp` and `escTaskInit()` already stated the parked boot, from D2.
  `SPLASH_MS` keeps 3000 ms, now for the splash's notes.

Tests: `test/test_dshot_life.cpp` `testSetupTexts()`, with the telemetry
holding 998 packets a second: from MENU no note and LINK `OFF`, after a SIG
PIN change too; over the tester the note names GP0, then the new pin, and
LINK the rate, `0/S  0% ERR` in `C_DIM` with no frame back; from MENU after
that change no note and `OFF`; SETUP from MENU within the drain the rate,
then `OFF` once parked. `test/test_menu.cpp` `testEscPinNote()` runs the
note's line, the list's 188 px and its scroll over the tester, and the clamp
when the line goes through the swap note from MENU; the shot
`shot_settings_pin.ppm` is taken over the tester.

Mutations, each run against the whole suite and restored:

| Mutation | Result |
|---|---|
| The pin note against the pin of the last load: shown while SIG is off `settingsBootDshotPin()`, naming `settings()->dshotPin` | 13 checks fail, among them the over-tester case's "the pin note names GP0, the pin the pump runs on, the pin of the last load" and "from MENU after a SIG PIN change: no pin note" |
| LINK's value drawn while parked (`off` always false) | 5 checks fail, among them "LINK reads OFF in C_DIM, not the rate the telemetry holds" |
| The pin note whatever DShot does (the running check dropped) | 79 checks fail, among them "from MENU after a SIG PIN change: no pin note" |
| LINK's repaint cache not keeping `OFF` | none at first: every case parked with a change on the note's line. A case added, SETUP within the drain; then 1 check fails, "parked: LINK reads OFF, no pin note" |

### D5: the documents (Sonnet)

The design notes' descriptions of the table above, as built; the bench
plan's checks, with the measurement of decision 7's option (c); previews;
`python3 test/check_docs.py` and `tools/check_wiki.py`. No host test; the
checks above are its tests.

## Decisions

The owner's decisions of 2026-11-15 are numbers 1 to 4 at the top of this
note. Numbers 5 to 10 answer its six questions in order. The owner is away.
The developer made each call on 2026-11-16 and took the note's own
recommendation each time. Numbers 11 to 14 settle points the reviews of
the note raised, each the safest option that builds. Each waits for the owner's
review.

5. Developer's call of 2026-11-16, pending the owner's review: **SIG while
   idle: driven low, or on the pull-down.** Option (a): SIG is an SIO
   output driven low at 2 mA while idle, and AUX rests on its pull-down
   with the input buffer off. A high line sends a powered Bluejay or
   BLHeli_S ESC into its bootloader, which is the path by which DShot can
   write an ESC's flash. Not measured: whether the pull-down alone holds
   the line low against an ESC's pull-up.
6. Developer's call of 2026-11-16, pending the owner's review: **SETUP over
   the tester: DShot up, or parked.** Option (a): DShot keeps running while
   SETUP is on top of ESC TESTER, disarmed, and the LINK line shows the ESC
   following a new rate or pin. A SIG PIN change under SETUP binds DShot on
   the new pin while SETUP is on screen, through `applyLive()` and
   `escTaskConfigure()`, and the pin note names that pin. This interprets
   owner decision 2: ESC TESTER is still open under SETUP, so the call
   reads "while ESC TESTER is open" as holding there. The interpretation
   needs the owner's confirmation; menu-c.md requirement 4 read the owner's
   rule the other way, DShot stopped under SETUP. The line added to
   [menu-c.md](menu-c.md#what-the-structure-requires-of-dshot)
   requirement 4: "Revised by dshot-lifetime.md decision 6, pending the
   owner's review: DShot keeps running while SETUP is open over ESC TESTER,
   and a SIG PIN change there binds DShot on the new pin while SETUP is on
   screen. Not built."
7. Developer's call of 2026-11-16, pending the owner's review: **The ESC
   check without DShot.** Option (a): SERVO TESTER, KST SERVO, HITEC SERVO
   and S.BUS OUTPUT get an amber line before their first output in place
   of the lost ESC check: `NO ESC CHECK: UNPLUG ANY ESC FIRST.`, 35
   characters by `textW()`, 209 px at scale 1. Where it shows and the slice
   that builds it: [decision 11](#decisions). A bench measurement of a
   passive line check, option (c), goes to the bench plan in slice D5.
8. Developer's call of 2026-11-16, pending the owner's review: **DShot
   while the run cue stands.** Option (a): while the cue stands, ESC TESTER
   and ESC COMMANDS open with DShot held off and one step on the leave page
   widget: `1 POWER-CYCLE THE ESC.` over `ITS BOOTLOADER MAY STILL WAIT` and
   `AND READ DSHOT AS A COMMAND.`. `START DSHOT`, placed by decision 14,
   acts 1000 ms after the step opens; the tap clears the cue. `START
   DSHOT` starts an output, so it is `C_GREEN` from ui-theme.md slice 11
   ([ui-theme.md](ui-theme.md#green)). ESC SETTINGS and S.BUS OUTPUT set
   the cue only on a bootloader's answer. USB BRIDGE sets it on every claim
   (`src/ui_bridge.cpp` 150 to 153), before any answer, and with DShot
   parked it clears only on an eRPM frame (`src/usb_bridge.cpp` 616 to 618)
   or on the leave page's last button (`src/ui_bridge.cpp` 442);
   [decision 13](#decisions) clears it at a claim's end that no ESC
   answered. With that rule the cue stands only where a bootloader may
   still wait, and the step costs a tap on those paths alone. Slice D3
   builds it. [Decision 14](#decisions) names the step's way out and START
   DSHOT's place.
9. Developer's call of 2026-11-16, pending the owner's review: **The leave
   page's last button.** Option (a): it reads the hub's title, `SERVO`,
   `RECEIVER` or `ESC PROGRAM`, with the last step `BACK TO SERVO.` and so
   on. SERVO TESTER's left button reads `NEXT SERVO` in place of `BACK TO
   SERVO`, which would name the hub. A label names where the tap leads, as
   the tester's `MENU` button does. The wide button is a way back, so it
   is `C_CYAN` from ui-theme.md slice 11 ([ui-theme.md](ui-theme.md#cyan)).
   Slice D2 builds the labels. This revises owner decision 3 of
   [menu-b.md](menu-b.md#decisions), whose button reads RESTART DSHOT. The
   line added to that decision: "Revised by dshot-lifetime.md decision 9,
   pending the owner's review: the leave page's last button reads the
   title of the hub the tool returns to, SERVO, RECEIVER or ESC PROGRAM, in
   place of RESTART DSHOT, and starts no DShot. The last step reads BACK TO
   SERVO, BACK TO RECEIVER or BACK TO ESC PROGRAM. SERVO TESTER's left
   button reads NEXT SERVO. Not built." Decision 21 and owner decision 30
   of menu-b.md, which name RESTART DSHOT's wait, carry a line that points
   to decision 3's.
10. Developer's call of 2026-11-16, pending the owner's review: **Arming
    right after DShot starts.** Option (a): HOLD TO ARM completes no earlier
    than 1500 ms after DShot's first frame, with `DSHOT STARTING: WAIT 1.5
    S` in the track meanwhile. 1500 ms is `ESC_CMD_SETTLE_MS`, 500 ms over
    AM32's 1 s of zero input. It costs at most 500 ms after an open and
    keeps the tester from arming before the ESC can. Decision 12 sets when
    the gate starts and how it shows, and raises that cost.
11. Developer's call of 2026-11-16, pending the owner's review: **Where
    decision 7's line shows, and when it is built.** KST SERVO and HITEC
    SERVO send frames on their entry's tap: the claim starts the job at
    once, and for a Hitec D series servo on its own supply at READ. A line
    on the tool's own screen would come after the first output. The line is
    therefore the second strip line of the hubs SERVO and RECEIVER, in
    `C_AMBER` through `uiStripTwoLines()` under the hub's caption, while the
    claim is Free, so it stands before every entry tap: SERVO TESTER, KST /
    CHASERVO, HITEC D SERIES and HITEC CLASSIC on SERVO, OUTPUT on
    RECEIVER. While the claim is not Free the busy caption takes the line,
    and the claiming entries are drawn disabled. ESC PROGRAM draws no such
    line: its tools need the ESC on SIG. Slice D2 builds it with the
    lifetime, so no merge leaves `main` where servo pulses could reach an
    ESC with neither the claim's check nor the line.
12. Developer's call of 2026-11-16, pending the owner's review: **The arm
    gate's start and look.** The gate of decision 10 starts again at every
    bind, from core1's bind stamp (`escTaskBoundMs()`, slice D1) published
    to core0, not from the tap: the first open, a rebuild after a rate,
    pin or KISS TELEM change in SETUP over the tester, and START DSHOT. The
    gate holds while `escTaskBoundMs()` is 0 or less than 1500 ms old; a
    stamp that moves cancels a timed press. The stamp is 0 while the pump
    is parked or released, which covers the frame between the tap and the
    bind.
    While it holds, and while #49's touch loss refuses arming, HOLD TO ARM
    is drawn disabled, as while a USB host holds the card, and a press that
    lands on it is not timed: the finger lifts and presses again once the
    button is live. The badge's progress grows only for a timed press, 92
    px in 1000 ms as today; a press on the disabled button draws none. The
    earliest arm is then 2500 ms after the bind, at most 1500 ms over
    today's 1000 ms hold. Slice D2 builds it. Reason: a button drawn as
    refused must not arm, and a gate counted from the tap would miss a
    rebind under SETUP.
13. Developer's call of 2026-11-16, pending the owner's review: **The
    bridge's run cue at a claim's end.** The counts of `bridgeWireStats()`,
    `sent`, `received`, `lost` and `collisions`, count since the line's
    last bind: `bridgeJobBind()` zeroes them (`src/bridge_wire.cpp` 225 and
    226) before it opens the line, so a bind that fails zeroes them too,
    and a claim binds once more at each RETRY (`pinClaimRetry()`,
    `src/pin_claim.cpp` 242 to 248). `usbBridgeRunCueSet()` runs at the
    claim (`src/ui_bridge.cpp` 153), before the line's first bind, so counts
    stamped there are the previous claim's, and a difference against them
    wraps: with 500 received and 400 sent left over, a new bind that sends
    10 and receives 19 reads as "received no more than sent", and the cue
    would clear though the ESC answered. The rule takes no difference
    across a bind.

    Which claims end with no leave page:

    - A claim that reached Running ends at the leave page. `back()` asks
      for the page while `pinClaimState()` reads Running
      (`src/ui_bridge.cpp` 415 and 416), and `pinClaimKeep()`, the page's
      left button, never clears that request (`src/pin_claim.cpp` 250 to
      259), so every later BACK of the claim gives the page too.
    - A bind that fails opens nothing and takes nothing: `bridgeJobBind()`
      returns before it sets `s_open` (`src/bridge_wire.cpp` 227 to 229),
      and `pinClaimPoll()` reads the job Idle again as "the bind failed and
      took nothing" (`src/pin_claim.cpp` 148 to 151).
    - A frame goes to the wire only from `forward()` in `usbBridgeTask()`,
      which runs only while the claim is Running for the bridge
      (`claimRunning()`, `src/usb_bridge.cpp` 141 to 143 and 571 to 579).

    A claim that ends with no leave page (`pinClaimLeavePage()` false in
    `back()`) therefore ended before `pinClaimPoll()` saw it Running, and
    sent nothing: BACK while Stopping, BACK while Held before a bind, BACK
    after binds that failed, or a race. In the race core1's bind lands
    after the frame's `pinClaimPoll()` (`src/ui_bridge.cpp` 429) and before
    BACK in the same frame (432): the claim still reads Held, `back()` asks
    for no page, and the line, bound for less than a frame, sent nothing.
    A bind that core1 took up before the stop and finishes after BACK
    (`src/core1_job.cpp` 124 to 137) sends nothing either. On these ends the
    counts can hold only bytes the line received unasked, which the rule
    keeps as a guard:

    - `usbBridgeRunCueSet()` clears a per-claim flag, "ESC byte seen", and
      notes `bridgeWireBinds()`, a new count of the line's binds.
    - `bridgeWireBinds()` crosses the cores as the counts do: one aligned
      `uint32_t` in `src/bridge_wire.cpp`, written by core1 in
      `bridgeJobBind()` after it zeroes the counts and before
      `bridgeLineOpen()`, with the file's release store `WIRE_PUBLISH`
      (`src/bridge_wire.cpp` 12 to 19), so a failed bind raises it too.
      Core0 reads it with the acquire `WIRE_OBSERVE` before it reads the
      counts, so a core0 that sees the new value sees the zeroed counts.
    - Every `usbBridgeTask()` pass, every pass of core0's loop
      (`src/main.cpp` 194), reads the counts once `bridgeWireBinds()` has
      moved since the claim began, so they are this claim's, counted from 0
      at the last bind. Received more than sent, or `lost` or `collisions`
      above 0, sets the flag: the echo gives one byte back per byte sent
      (`src/bridge_wire.h`, "The echo"), so any byte more came from the ESC.
    - `bridgeWireStats()` reads `sent` before `received`
      (`src/bridge_wire.cpp` 118 to 124), and core1 counts a byte sent
      before its echo. A read that spans a byte sent and echoed shows one
      received more than sent, and sets the flag: the error keeps the cue,
      the safe direction. Reading `received` first would turn the error
      the other way: a byte sent between the two reads would hide one of
      the ESC's. The order stays.
    - The flag stays set across the claim's binds. The counts stay after a
      release until the next bind. A read that meets core1 zeroing the
      counts at a RETRY's bind mixes two binds of this claim; no read
      clears the flag, so such a read can only keep the cue.
    - When the claim ends with no leave page, a new
      `usbBridgeRunCueSettle()`, called from `back()`, reads the counts once
      more under the same condition and clears the cue only if the flag is
      clear.

    The leave page's last button clears the cue as today: its steps power
    the ESC off. The cue then stands only where a bootloader may still
    wait. One that answered waits for commands with no timeout; one that
    did not answer starts its firmware on the parked low line, an EFM8
    after `BOOT_DELAY`, about 250 ms with no edge, an AM32 after 20 ms low
    ([bluejay-blheli-s.md](bluejay-blheli-s.md#entering-the-bootloader);
    from source, not measured). Slice D3 builds it, with decision 8.

    Revised by [D3 as built](#d3-as-built), "A cue that stands at a
    claim", pending the owner's review: `usbBridgeRunCueSet()` sets the
    flag to whether the cue already stands, in place of clearing it, so a
    claim that ends with no leave page never clears a cue that an earlier
    claim's ESC byte kept.
14. Developer's call of 2026-11-16, pending the owner's review: **The start
    step's way out.** Decision 8's start step is left by START DSHOT or by
    the screen's own way out, which stays live on the step: ESC TESTER's
    `MENU` button (x 176 to 233, y 283 on, `BTN_MENU`, `src/ui.cpp` 177)
    and ESC COMMANDS' header BACK. Each returns as it does without the
    step, the tester to MENU and ESC COMMANDS to its hub, with DShot
    parked, nothing sent and the run cue kept: no step was done. The step
    takes the leave page widget's numbered steps and its 1000 ms wait, not
    the leave page's rules. Owner decision 25 of
    [menu-b.md](menu-b.md#decisions) draws BACK disabled on the leave page,
    where a claim holds SIG and only the page's steps end it. On the start
    step no claim holds and DShot is parked, so leaving loses nothing, and
    BACK and `MENU` stay live. START DSHOT does not take the widget's
    wide button (`UI_LEAVE_WIDE`, y 268 to 313), which would cover the
    tester's button row: it is 228 px wide at the row of the widget's left
    and right buttons (`UI_LEAVE_BTN_Y`, y 214, 46 px tall), x 6 to 233 and
    y 214 to 259, at scale 2.

    That rectangle lies over live controls of both screens. On ESC TESTER
    it covers `TGT_MAX` (x 82 to 141, y 230 to 251, `src/ui.cpp` 199),
    whose tap opens SETUP while disarmed, the throttle zone (y 234 to 281,
    `Z_THR_Y0` and `Z_THR_Y1`, `src/ui.cpp` 76 and 77) and the track's grab
    band (from y 240, `THR_TRACK_Y` 248 less `THR_TOUCH_PAD_TOP` 8,
    `src/ui.cpp` 89 and 112); the step's lines, from y 66
    (`UI_LEAVE_STEP_Y`), cover `TGT_POLES` (y 106 to 127). On ESC COMMANDS
    it covers the 3D OFF and 3D ON holds (y 192 to 235, `UI_ESC_CMD_3D_Y`
    and `UI_ESC_CMD_HOLD_H`, `src/ui_esc_cmd.h` 120 and 122) and the
    message lines (from y 244, `UI_ESC_CMD_LINE_Y`, 124).

    The rule: on the step only the step's text, START DSHOT and the
    screen's way out, the tester's `MENU` and ESC COMMANDS' BACK, are drawn
    and live. The rest of the screen is hidden and inert: on the tester the
    readouts with `TGT_POLES` and `TGT_MAX`, the pad, the track, HOLD TO
    ARM and LATCH; on ESC COMMANDS the BEEP buttons, the four holds and the
    message lines. A touch on them does nothing, and none is in the
    screen's list of controls.

    `uiDshotWanted()` stays false for SETUP over a tester whose step was
    not done: decision 6 keeps DShot up under SETUP only over a tester that
    runs it. SETUP cannot be reached from the step anyway: the readouts,
    its only way in from the tester, are inert there.

    The colours follow [ui-theme.md](ui-theme.md#decisions) decisions 15
    and 16. START DSHOT is a start button: disabled for its first 999 ms,
    `C_BG` with a frame and a `C_GRID` label, then `C_GREEN` with a
    `C_ONACCENT` label. The tester's `MENU` is `C_CYAN` with a `C_ONACCENT`
    label, and ESC COMMANDS' BACK the header's `C_CYAN` button, each live
    from the step's first frame. Slice D3 builds the step; slices 11 and 12
    of [menu-c.md](menu-c.md#build-plan) give START DSHOT its colours on
    ESC COMMANDS and on the tester.

## Open questions

None open. The six questions of this note as it stood are settled, each by
the developer's call of 2026-11-16, pending the owner's review:

1. ~~SIG while idle: driven low, or on the pull-down.~~ Settled by
   [decision 5](#decisions).
2. ~~SETUP over the tester: DShot up, or parked.~~ Settled by
   [decision 6](#decisions).
3. ~~The ESC check without DShot.~~ Settled by [decision 7](#decisions).
4. ~~DShot while the run cue stands.~~ Settled by [decision 8](#decisions).
5. ~~The leave page's last button.~~ Settled by [decision 9](#decisions).
6. ~~Arming right after DShot starts.~~ Settled by
   [decision 10](#decisions).

The reviews of the note raised four more points, settled by decisions 11
to 14, pending the owner's review: where decision 7's line shows, the arm
gate's start and look, the bridge's run cue, and the start step's way out.

## Not verified

- How an ESC behaves on signal loss on this bench, for each firmware.
- Whether the RP2350's pull-down holds SIG low against an ESC's own
  pull-up, and the current of the pad driving low against a receiver.
- Which ESCs arm on what a floating lead picks up.
- The times an ESC needs from DShot's first frame to arming, beyond AM32's
  1 s read from its source.
- Whether an AM32 restarting every 2 s on a parked line plays its start-up
  tones.
- Whether an EFM8 bootloader that answered leaves its wait on a line held
  low, and whether one that did not answer starts its firmware on it.
- Nothing of this note runs on hardware.
