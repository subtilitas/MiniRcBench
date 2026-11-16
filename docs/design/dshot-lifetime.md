# DShot lifetime

Status: proposed, not built. The developer settled its six questions on
2026-11-16, pending the owner's review ([Decisions](#decisions)). It builds
on [menu-c.md](menu-c.md), whose slice 3 makes MENU the root, and answers
[What the structure requires of
DShot](menu-c.md#what-the-structure-requires-of-dshot).

The owner's decisions of 2026-11-15 that this note takes as fixed:

1. **MENU is the start screen.** No DShot frame leaves the board at
   power-up.
2. **DShot sends frames only while ESC TESTER or ESC COMMANDS is open.**
   Back on MENU, or in a hub, the frames stop, and the ESC sees signal loss
   and disarms. SERVO TESTER, KST SERVO, HITEC SERVO, S.BUS MONITOR, S.BUS
   OUTPUT and the ESC PROGRAM tools (today's ESC CONFIG: ESC SETTINGS and
   USB BRIDGE) get the pin at once, with no drain.
3. **The menu structure is menu-c.md**, with the owner's answers: six MENU
   rows, ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS and SETUP; a tool
   returns to its hub; RX SIGNALS, SETTINGS, SD LOG and ESC CONFIG are
   renamed RECEIVER, SETUP, LOGS and ESC PROGRAM. This note uses the new
   names for the structure and today's names for code and screen texts.
   Where ESC COMMANDS sits is menu-c.md's; its proposal is the first entry
   of the hub ESC PROGRAM.
4. **0.1.0 waits for this work.**

Pull request #49, the touch-loss stop (branch `ccr-ux-pass2`, head
cbbedf5), is being merged. It disarms the tester, and stops SERVO TESTER
and S.BUS OUTPUT, while the touch controller does not answer
(`uiTouchLost()`). [#49 and the lifetime](#49-and-the-lifetime) says how it
fits.

The code is read at 43531d9, the base branch `ccr-menu-rcb`, whose `src/`
and `test/` equal `main` at cff09ae (`git diff --stat cff09ae 43531d9 --
src test` is empty). Line numbers are at that commit.

Terms: ESC is electronic speed controller, DShot the digital ESC protocol
the tester drives, EDT Extended DShot Telemetry, eRPM electrical
revolutions per minute, KISS the serial telemetry of the KISS ESC line on
its own wire, PIO the RP2350's programmable input/output block, SM one PIO
state machine, DMA direct memory access, SIO the RP2350's single-cycle
input/output block, which drives a pad as a plain output, GPIO
general-purpose input/output, PWM pulse-width modulation, EFM8 the Silicon
Labs chip of Bluejay and BLHeli_S ESCs, AM32 an open ESC firmware, S.BUS a
serial receiver protocol, UI user interface. SIG and AUX are the two signal
pins of [pin-claim.md](pin-claim.md#the-two-pins), GP0 and GP1 by default.

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
  the block simply receives nothing until ESC TESTER or ESC COMMANDS opens.
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
([texts](#texts-and-documents-that-assume-dshot-from-boot)).

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
of ESC TESTER. Everywhere else it is parked.

The UI decides it once a frame: `uiDshotWanted()` is a pure function of the
stack of menu-c.md's [navigation](menu-c.md#navigation), and `uiTick()`
hands its result to `pinClaimDshot(bool)` before the screen's tick. Asking
every frame, rather than on each transition, means no route can leave
DShot in the wrong mode: a screen added later is parked unless the rule
names it.

`pinClaimDshot(true)` acts only while the claim is Free and DShot is
parked; `pinClaimDshot(false)` only while Free and DShot runs. ESC TESTER
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
   | tester (Q2)      |      v                                     |
   +------------------+   Parking: core1 parks SIG and AUX --------+
     leave: disarm, the drain if owed, Park
```

### Each case

| Case | What happens | Why |
|---|---|---|
| ESC TESTER opens | the tap that opens it asks for Run; core1 binds on its next pass; the first 10 frames are the EDT enable, then zero-throttle frames | `escPumpBind()` sets disarmed and throttle 0 at every bind (`src/esc_task.h` 910 to 917) |
| The ESC's zero-throttle frames before it arms | AM32 takes commands only after more than 1 s of zero input, and arms on its own rule; Bluejay's and BLHeli_S's times are not recorded here | [esc-commands.md](esc-commands.md#safety-and-interlocks), "1500 ms of answers"; not measured |
| HOLD TO ARM | its 1000 ms hold (`ARM_HOLD_MS`, `src/config.h` 540) can complete 1 s after the tap at the earliest, with at most one UI frame, 25 ms, between the tap and the first frame. Decision 10 adds a gate: arming refused until 1500 ms after the first frame, `ESC_CMD_SETTLE_MS`'s margin over AM32's 1 s | today the tester appears at the same moment DShot starts, after the splash, and the same race exists once per power-up; the design repeats it at every open |
| Leaving while armed | the MENU tap runs `testerDisarm()` (`src/ui.cpp` 1005 to 1012, 1015 to 1020): throttle 0, LATCH off; then Park. Core1 sends zero frames until more than 250 ms after the last throttle frame (`escDrainDone()`, `src/esc_task.h` 1135 to 1138), then stops | the ESC hears zero before silence, as before every handover today |
| Leaving disarmed | Park at once when no throttle frame went out within 250 ms | `escHandoverStep()`: a pump with no recent throttle frame has nothing to drain |
| SETUP from the readouts | they act only disarmed (`readoutsAct()`, `src/ui.cpp` 456). Decision 6: DShot stays up under SETUP | see decision 6 |
| SIG PIN, AUX PIN, the rate or KISS TELEM changed in SETUP | over the tester, DShot running: `applyLive()` → `escTaskConfigure()` rebuilds as today, drain included, and the pin note warns of the new pin. From MENU, parked: the change re-parks on the new wiring and binds on the next ESC TESTER | `src/ui_settings.cpp` 857 to 863; `src/esc_task.cpp` 333 to 349 |
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
| The run cue | stays MENU's second strip line; cleared as today by a LEAVE sent, by the leave page's last button, whose steps power-cycle the ESC, or by an eRPM frame newer than the answer, which only ESC TESTER and ESC COMMANDS now receive. Decision 8: a start step while it stands | |
| A claiming tool opens from a hub | Release from Park: no drain, the pins free on core1's next pass. Within 250 ms of leaving a running tester the claim waits for the rest of that drain | |
| A claiming tool leaves | the leave page as today; its last button parks instead of restarting DShot, and returns to the hub | menu-c.md, requirement 13 and decision 7 |

## The pin claim

The smallest change keeps every state and every user, and changes what
"Free" and the last state mean. The claim stays the one owner of SIG and
AUX: it alone asks core1 for Run, Park and Release.

### States

| State | Today | With the lifetime |
|---|---|---|
| Free | DShot owns both pins | no claim; DShot parked, or running for ESC TESTER and ESC COMMANDS (`pinClaimDshotRunning()`) |
| Stopping | the pump drains and releases | Release: from Park at once; from Run after the drain owed, which only a claim opened within 250 ms of a running tester meets |
| Held, Running, Leaving | as today | as today |
| Restoring | the pump rebuilds; Free once `escTaskSuspended()` is false | renamed **Parking**: core1 parks SIG and AUX; Free once `escTaskParked()` |

"Not Free" on MENU and in the hubs then lasts only for the USB bridge's
release, at most 250 ms (`BRIDGE_RELEASE_MAX_MS`), and for Parking, one
core1 pass. MENU's busy caption says `RELEASING SIG GP0` meanwhile.

### API

| Today | With the lifetime |
|---|---|
| `escTaskResume()` in `restore()` (`src/pin_claim.cpp` 60 to 63) | `escTaskPark()`; the state is Parking |
| none | `pinClaimDshot(bool on)`: Free only; Run or Park |
| none | `pinClaimDshotRunning()`: Free and the pump running |
| `pinClaimRestart()`, RESTART DSHOT | `pinClaimRelease()`: the same gate (`pinClaimUnplugDue()`), then Parking |
| `pinClaimBegin()` → `escTaskSuspend()` | unchanged; with DShot running it is refused as Busy, a backstop: no route opens a claim from ESC TESTER or ESC COMMANDS |

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
tester was disarmed less than 250 ms before. A claim no longer drains: the pump
it releases is parked. This is the owner's "no drain" for the tools.

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
| The last step | `RESTART DSHOT.` (`UI_LEAVE_RESTART`, line 112) | decision 9: `BACK TO SERVO.`, `BACK TO RECEIVER.` or `BACK TO ESC PROGRAM.`, the hub the tool opened from, 20 characters at most, recommended; or `DONE.` |
| The wide button | `RESTART DSHOT` (`src/ui_leave.cpp` 120) | decision 9: the hub's title, `SERVO`, `RECEIVER` or `ESC PROGRAM` (11 characters at scale 2, 130 px), recommended; or `DONE` |
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
   and Hitec entries send frames at once. Decision 7.
3. **Each open is a fresh start.** Every open of ESC TESTER is a new bind
   with the ESC detecting DShot again; HOLD TO ARM can complete before an
   AM32 has seen 1 s of zero input. The ESC then does not arm and ignores
   the throttle until it reads zero; not measured. Decision 10.
4. **DShot into a waiting bootloader on the next open.** After an EFM8
   answered and no LEAVE went out, an ESC that was not power-cycled waits
   in its bootloader; the next ESC TESTER sends DShot into it. Today
   RESTART DSHOT does the same at once. The run cue on MENU names it.
   Decision 8.
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

#49 adds `uiTouchLost()`, read once a frame after the touch read
(`touchLostNow()`; `src/ui.cpp` in `ccr-ux-pass2`). It fits without a
change on either side:

- **ESC TESTER**: a lost touch disarms by the idle interlock's path and
  refuses arming. DShot keeps sending zero frames: the tester is still
  open, and with no touch no screen change can happen. A lost touch never
  starts or stops DShot. Decision 10's gate and #49's refusal both refuse
  arming, each on its own condition.
- **ESC COMMANDS**: #49 adds nothing; the page's sequences end by
  themselves and it sends no throttle.
- **SERVO TESTER, S.BUS OUTPUT**: they stop as STOP does and keep the
  claim; SIG stays the job's.
- **MENU, hubs, LOGS, SETUP from MENU**: nothing is driven, so nothing has
  to stop.
- #49's README text, "with DShot off there is no motor to stop" for the
  USB bridge, stays true.

The slices start from `main` after #49 is merged, and keep its host cases.

## Texts and documents that assume DShot from boot

### Screen texts

Widths are at scale 1, 6 px a character less 1, unless named.

| Text | Where | Change |
|---|---|---|
| `UNPLUG ANY SERVO FROM GP0 NOW` on the splash | `src/ui.cpp` 1291 to 1296; `UI_SPLASH_PIN_FMT`, `src/ui.h` 166 to 171 | the splash draws it no more; the macro stays for SETUP's pin note, documented as SETUP's |
| SETUP's pin note, the same text | `pinNote()`, `src/ui_settings.cpp` 707 to 714 | shown only while DShot runs under SETUP (decision 6), against the pin the pump runs on, not the pin of the last load |
| `LINK  998/S  0% ERR` | `drawLink()`, `src/ui_settings.cpp` 783 to 801 | while DShot is parked the value reads `OFF` in `C_DIM` |
| `DSHOT RESTARTING ON SIG GP0` | `UI_MENU_RESTARTING_FMT`, `src/ui_menu.h` 48 | `RELEASING SIG GP0`, 17 characters |
| `WAIT: DSHOT RESTARTING` | `UI_WAIT_RESTART_TEXT`, `src/ui_widgets.h` 121 | `WAIT: RELEASING THE PINS`, 24 characters, inside the 37 of `src/ui_bridge.cpp` 78 |
| `EDT ON`, `EDT OFF`, `NO ESC` on MENU | `src/ui_menu.cpp` 198 to 206 | the chip goes (menu-c.md, decision 12); ESC COMMANDS keeps it |
| `DSHOT RESTARTS ON SIG GP0 AND READS` | `src/ui_leave.h` 108 | `ESC TESTER'S DSHOT ON SIG GP0 READS`, 35 |
| `RESTART DSHOT.`, `RESTART DSHOT` | `src/ui_leave.h` 112; `src/ui_leave.cpp` 120 | the hub's step and label, decision 9 |
| `DSHOT RESTARTS ON SIG GP0.` | `SBUS_UI_RESTART_FMT`, `src/ui_sbus.h` 96; `SBOUT_UI_RESTART_FMT`, `src/ui_sbus_out.h` 137 | `ESC TESTER SENDS DSHOT ON SIG GP0.`, 34 |
| `DSHOT RESTARTS ON SIG GP1, WHERE` | `SBUS_UI_RESTART_GP1_1`, `src/ui_sbus.h` 98 | `SIG GP1 IS HELD LOW, WHERE`, 26, over the unchanged `THE RECEIVER DRIVES THE LINE.`; with decision 5's (b) `ESC TESTER SENDS DSHOT ON GP1, WHERE`, 35 |
| `LEAVE, THEN DSHOT RETURNS.` | `AM32_UI_EXIT_2`, `src/ui_am32.h` 230 | `LEAVE. ESC TESTER THEN RUNS IT.`, 31, inside `RESULT_CHARS` 37 |
| `PLUG IT IN AFTER RESTART DSHOT.` | `AM32_UI_LEAVE_2`, `src/ui_am32.h` 253 | `PLUG IT IN BACK ON ESC PROGRAM.`, 31 |
| `DSHOT STAYS ON THE PIN UNTIL THEN.` | `HITEC_UI_ESC_2`, `src/ui_hitec.h` 91 | `NOTHING IS SENT UNTIL THEN.`, 27 |
| `DSHOT RUNS ON SIG GP0` | `UI_TOOLS_STRIP_FMT`, `src/ui_tools.h` 33 | goes with TOOLS (menu-c.md, slice 4) |
| `NO ESC ANSWERS ON SIG GP0.` in the first 1500 ms | `UI_ESC_CMD_NO_ANSWER_FMT`, `src/ui_esc_cmd.h` 95 | `DSHOT STARTING ON SIG GP0.`, 26, until 1500 ms after the start; then as today |
| new: the tester's track before arming may complete | beside `UI_TESTER_CARD_TEXT`, `src/ui.h` 160 to 164 | decision 10: `DSHOT STARTING: WAIT 1.5 S`, 26 characters, the card text's length, inside its `static_assert` (`src/ui.cpp` 965) |
| `SIG GP0 IS THE SERVO'S: DSHOT IS OFF.`, `PROPELLER OFF. DSHOT IS OFF.`, `THE TESTER SENDS DSHOT ONLY:`, `AND MAY READ DSHOT AS ONE.` | `src/ui_hitec.h` 85; `src/ui_bridge.h` 115; `src/ui_am32.h` 151, 251 | unchanged: still true |
| `AN ESC ANSWERS ON SIG GP0.` and its lines on SERVO TESTER, KST SERVO, HITEC SERVO, S.BUS MONITOR, S.BUS OUTPUT | `src/ui_servo.cpp` 509, 510; `src/ui_kst.cpp` 604 to 606; `src/ui_hitec.h` 89; `src/ui_sbus.h` 86; `src/ui_sbus_out.h` 107 to 111 | unchanged; reachable within 500 ms of the last answer. Decision 7 may add a line |

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
| README.md | "Safety", lines 3478, 3494 to 3496, 3534 to 3538 | "Boots disarmed and sends MOTOR_STOP continuously" becomes "Boots with DShot off; SIG held low"; the claim's ESC check as decision 7 decides |
| wiki, English and German | The-Screens and Die-Bildschirme (29 and 30 lines matching, case-insensitive, restart dshot, dshot restart, dshot returns, dshot starts, dshot runs, dshot is off, dshot comes back, splash and unplug any servo, or their German forms), First-Run and Erste-Schritte (12, 6), Servo-Tester and Servotester (8, 6), KST-Programming and KST-Programmierung (10, 10), Hitec-Programming and Hitec-Programmierung (13, 13), RX-Signals and RX-Signale (9, 9), USB-Bridge and USB-Bruecke (6, 4), AM32-Configuration and AM32-Konfiguration (5, 6), Troubleshooting and Fehlersuche (8, 4), Setup and Einrichtung (5, 2), Home (2) | as README.md |
| [pin-claim.md](pin-claim.md) | "The two pins", "The claim", "Sequence" steps 2 and 6, "Interlocks", "When the pin is unavailable", "Restoring DShot" | Free and Parking; the drain only after a running tester; the ESC check's source |
| [kiss-telemetry.md](kiss-telemetry.md) | "Receiving", "Where the code runs" | the receiver runs only with the pump |
| [blackbox-logging.md](blackbox-logging.md) | "Lifecycle" | a hand log across a parked pump; eRPM only while fresh |
| [esc-commands.md](esc-commands.md) | "The page" | DShot starts with the page |
| [bluejay-blheli-s.md](bluejay-blheli-s.md), [usb-bridge.md](usb-bridge.md), [sbus-output.md](sbus-output.md), [rx-signals.md](rx-signals.md), [servo-tester.md](servo-tester.md), [kst-programming.md](kst-programming.md), [hitec-programming.md](hitec-programming.md), [current-sense.md](current-sense.md), [menu.md](menu.md), [menu-b.md](menu-b.md) | their descriptions of RESTART DSHOT and of DShot returning | as built, in each slice; their decisions get revision lines |
| `docs/bench-plan.md` | its checks of RESTART DSHOT and of DShot after a claim | the parked line and the next ESC TESTER |
| STATUS.md, CHANGELOG.md | | per slice |

The decisions this revises carry a "Revised by" line now:
[pin-claim.md](pin-claim.md#decisions) decisions 3 and 4,
[menu-b.md](menu-b.md#not-changed) "The tester is the root",
[esc-commands.md](esc-commands.md#decisions) decision 5,
[kiss-telemetry.md](kiss-telemetry.md#receiving) and
[blackbox-logging.md](blackbox-logging.md#lifecycle). menu-b.md owner
decisions 3, 21 and 30, which name RESTART DSHOT, get theirs from the line of
[decision 9](#decisions).

## Slices

For builders on Opus. Each slice is host-tested and mutation-checked: each
mutation listed must make a named test fail, and is restored. Each slice
updates its own README, wiki pages in both languages, previews, CHANGELOG
and STATUS lines (menu-b.md owner decision 27). The order against
[menu-c.md](menu-c.md#slices):

| Slice | After | What |
|---|---|---|
| D1 | anything; before or with menu-c.md slice 1 | the pump's three modes, no caller changes |
| D2 | menu-c.md slices 2 and 3 (the stack, MENU as root) and D1 | the lifetime: boot parked, the rule, the claim's Parking |
| D3 | D2 | the tester and ESC COMMANDS on a starting pump; the log's eRPM |
| D4 | D2; the hub names after menu-c.md slices 4 and 5 (hubs, rename) | the texts |
| D5 | D4, with menu-c.md slice 6 (the records) | the design notes' descriptions, bench plan |

menu-c.md says that until the lifetime's slices merge DShot runs on MENU as
today, so no slice leaves `main` less safe than cff09ae. D2 is the slice
that stops it; 0.1.0 follows D5.

### D1: the pump's three modes (Opus)

`src/esc_task.{h,cpp}`: `EscPinMode` Run, Park, Release; the pure step
extended from `escHandoverStep()` to the wanted mode, the mode reached, the
rebuild request, the pump, the last throttle stamp and the clock; a pure
`escParkPlan()` naming each GPIO's state for a wiring change while parked;
`escTaskStart()`, `escTaskPark()`, `escTaskParked()`,
`escTaskRunning()`; `escTaskSuspended()` true only in Release; the park on
core1 in `escTaskPoll()`; `initError` cleared at a teardown.
`test/fakes.cpp` follows the pure step. Boot still asks for Run, and the
claim still calls `escTaskResume()` under its new name, so no behaviour
changes.

Host tests (`test/test_claim.cpp` or a new `test/test_dshot_life.cpp`):

- Park after a throttle frame sends zero frames until more than 250 ms
  after it, then parks; Park with no throttle frame parks on the first pass.
- Release from Park releases on the first pass with no frame.
- `escTaskSuspended()` is false while parked, and `core1JobStart()` is
  refused then.
- A wiring change while parked: the old SIG in the input, pull-down, buffer
  off state, the new SIG low, AUX on its pull-down; while running: a
  rebuild, as `testHandoverRules()` checks today.
- Run from Park binds once, with the EDT enable first.

Mutations that must fail them: Park without the drain; `escTaskSuspended()`
true while parked; the old SIG left driven low after a swap; a teardown
that keeps `initError`.

### D2: the lifetime (Opus)

`escTaskInit()` asks for Park and parks SIG on core0. `src/pin_claim.*`:
`pinClaimDshot()`, `pinClaimDshotRunning()`, `restore()` parks,
`ClaimState::Restoring` renamed `Parking`, `pinClaimRestart()` renamed
`pinClaimRelease()`, Busy while DShot runs. `src/ui.cpp`:
`uiDshotWanted()`, pure, and the call to `pinClaimDshot()` once a frame
before the screen's tick. The leave page's last button returns to the hub
(menu-c.md slice 2 already routes it).

Host tests, by control id:

- After `uiInit()`: the screen is MENU, the pump is parked, and no bind
  happened (`fakeRebuildCount()` 0).
- The ESC TESTER row binds once; the tester's MENU button parks: with the
  tester armed and throttled, zero frames for more than 250 ms first.
- ESC COMMANDS: open binds, BACK parks with no drain.
- For every screen of menu-c.md's table but ESC TESTER, ESC COMMANDS and
  SETUP over the tester: `uiDshotWanted()` is false, and the pump parked
  after one frame.
- SETUP from the readouts: DShot stays up, as decision 6 decides.
- Each claiming tool from its hub reaches Held with no drain, on the first
  pump pass; opened within 250 ms of leaving a running tester it waits for
  the rest of the drain.
- Each tool's leave, by the leave page's last button and by BACK where it
  has none: Parking, then Free, no bind, the screen its hub.
- `pinClaimDshot(true)` while a claim holds does nothing; `pinClaimBegin()`
  while DShot runs is Busy.

Mutations: `restore()` calls `escTaskStart()` (the leave cases fail);
`uiDshotWanted()` true for MENU (the boot case fails); Parking skipped,
Free before `escTaskParked()` (the Parking case fails); `pinClaimDshot()`
acting while not Free (its case fails); `escTaskInit()` asking for Run (the
boot case fails).

### D3: the tester and ESC COMMANDS on a starting pump (Opus)

The arm gate of decision 10; `DSHOT STARTING ON SIG GP0.` on ESC COMMANDS
for the first 1500 ms with no answer; the start step of decision 8; the
log's eRPM column only while fresh (`src/sd_log.cpp` 421).

Host tests:

- With decision 10: a HOLD TO ARM started at the open does not arm at
  1000 ms and arms once 1500 ms of DShot have passed; one started later
  arms after its 1000 ms as today; the track text reads `DSHOT STARTING:
  WAIT 1.5 S` until then.
- ESC COMMANDS: the first text for 1499 ms with no answer, then `NO ESC
  ANSWERS ON SIG GP0.`; an answer gives `ESC STARTING: WAIT 1.5 S.` as
  today.
- With decision 8: while the run cue stands, ESC TESTER and ESC
  COMMANDS open with the pump parked and the start step; START DSHOT acts
  1000 ms after the step opens, binds and clears the cue.
- The log: a frame with an eRPM older than 500 ms logs 0; a fresh one logs
  its value (`make -C test lognum` and the blackbox checker).
- #49's cases pass unchanged.

Mutations: the gate counting from the press, not from the start; the start
text kept past 1500 ms; START DSHOT live at once; the eRPM logged without
the freshness check.

### D4: the texts (Opus)

The screen texts of [the table above](#screen-texts), with their fit
`static_assert`s; decision 9's labels; decision 7's line, if any. The
removed names go into rule 9 of `test/check_docs.py`: `RESTART DSHOT`,
`DSHOT RESTARTING`, `DSHOT RESTARTS ON`, `LEAVE, THEN DSHOT RETURNS`.

Host tests: each leave page's step lines and its wide button by control id
and text (`test_servo.cpp`, `test_kst_ui.cpp`, `test_hitec_ui.cpp`,
`test_sbus_ui.cpp`, `test_sbus_out_ui.cpp`, `test_bridge_ui.cpp`,
`test_am32.cpp`); the splash has no pin line; MENU's busy caption; LINK
reads `OFF` while parked.

Mutations: a text one character over its limit fails the build; the splash pin
line drawn again fails its case; the busy caption back to the old text fails
MENU's caption case.

### D5: the documents

The design notes' descriptions of the table above, as built; the bench
plan's checks; previews; `python3 test/check_docs.py` and
`tools/check_wiki.py`. No host test; the checks above are its tests.

## Decisions

The owner's decisions of 2026-11-15 are numbers 1 to 4 at the top of this
note. Numbers 5 to 10 answer its six questions in order. The owner is away.
The developer made each call on 2026-11-16 and took the note's own
recommendation each time. Each waits for the owner's review.

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
   following a new rate or pin. ESC TESTER is still open under SETUP, so
   the call reads owner decision 2 as holding. The line the build adds to
   [menu-c.md](menu-c.md#what-the-structure-requires-of-dshot)
   requirement 4: "Revised by dshot-lifetime.md decision 6, pending the
   owner's review: DShot keeps running while SETUP is open over ESC
   TESTER."
7. Developer's call of 2026-11-16, pending the owner's review: **The ESC
   check without DShot.** Option (a): SERVO TESTER, KST SERVO, HITEC SERVO
   and S.BUS OUTPUT show an amber line before their first output in place
   of the lost ESC check, for example `NO ESC CHECK: UNPLUG ANY ESC
   FIRST.` (34 characters). A bench measurement of a passive line check,
   option (c), goes to the bench plan in slice D5.
8. Developer's call of 2026-11-16, pending the owner's review: **DShot
   while the run cue stands.** Option (a): while the cue stands, ESC TESTER
   and ESC COMMANDS open with DShot held off and one step on the leave page
   widget: `1 POWER-CYCLE THE ESC.` over `ITS BOOTLOADER MAY STILL WAIT` and
   `AND READ DSHOT AS A COMMAND.`. A wide `START DSHOT` acts 1000 ms after
   the step opens; the tap clears the cue. The cue stands only after a
   bootloader answered, so the step costs a tap on that path alone.
9. Developer's call of 2026-11-16, pending the owner's review: **The leave
   page's last button.** Option (a): it reads the hub's title, `SERVO`,
   `RECEIVER` or `ESC PROGRAM`, with the last step `BACK TO SERVO.` and so
   on. SERVO TESTER's left button reads `NEXT SERVO` in place of `BACK TO
   SERVO`, which would name the hub. A label names where the tap leads, as
   the tester's `MENU` button does. This revises owner decision 3 of
   [menu-b.md](menu-b.md#decisions), whose button reads RESTART DSHOT. The
   line the build adds to that decision: "Revised by dshot-lifetime.md
   decision 9, pending the owner's review: the leave page's last button
   reads the title of the hub the tool returns to, SERVO, RECEIVER or ESC
   PROGRAM, in place of RESTART DSHOT, and starts no DShot. The last step
   reads BACK TO SERVO, BACK TO RECEIVER or BACK TO ESC PROGRAM. SERVO
   TESTER's left button reads NEXT SERVO." Decisions 21 and 30 of
   menu-b.md, which name RESTART DSHOT's wait, get a line that points to
   decision 3's.
10. Developer's call of 2026-11-16, pending the owner's review: **Arming
    right after DShot starts.** Option (a): HOLD TO ARM completes no earlier
    than 1500 ms after DShot's first frame, with `DSHOT STARTING: WAIT 1.5
    S` in the track meanwhile. 1500 ms is `ESC_CMD_SETTLE_MS`, 500 ms over
    AM32's 1 s of zero input. It costs at most 500 ms after an open and
    keeps the tester from arming before the ESC can.

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
  low.
- Nothing of this note runs on hardware.
