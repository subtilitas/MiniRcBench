# KST and Chaservo servo programming

Status: work breakdown steps 1 to 4 built, host tests only. The KST core,
its RP2350 line driver and the KST file store are copied into
`lib/rcbench/`, built by the firmware and the host suite, and rcbench's seven
tests of them run in `make` and CI. The search (`src/kst_seek.*`) and the
core1 job (`src/kst_job.*`) run the session on the line, and the host suite
runs both against rcbench's simulated servo. `src/kst_files.*` puts the file
store and the record of a write on the card through FatFs, tested against an
in-memory volume. `src/ui_kst.cpp` is the screen, MENU's KST SERVO row, and
`src/kst_view.*` the rows, values, stepper and texts it draws; the host suite
runs the screen through MENU against the simulated servo and the in-memory
volume. The switched supply, step 5, is not built: LEAVE asks the user to
power the servo off. It depends on the pin claim in
[pin-claim.md](pin-claim.md), slice 1 there, which is built. This
document is slice 3. It comes after slices A to C of [menu.md](menu.md), the
menu restructure, which are built, and is written for the system they leave:
MENU, to which slice 3 adds the KST SERVO row, the SETTINGS list, the shared
list `ui_list`, the shared hold `ui_hold` and the 36 px header. The
rcbench code it ports has not sent a frame to a servo either: rcbench's
`protocols/kst/README.md` says "no frame of this code has been sent to a
servo, and the pin driver has not run on a chip".

Terms: KST and Chaservo are servo brands that share one programming protocol
and one register set; the names are used only to say which servos this works
with. The servo's signal wire carries the protocol, half duplex, Manchester
coded. A half-cell is half of one Manchester bit. PIO is the RP2350's
programmable input/output block, SM a PIO state machine, DMA direct memory
access, PWM pulse-width modulation, ESC electronic speed controller, DShot the
digital ESC protocol, SD Secure Digital, FAT File Allocation Table, USB
Universal Serial Bus, UI user interface, ADC analogue-to-digital converter.
GPIO is general-purpose input/output, SIO the RP2350's single-cycle I/O
block that drives a GPIO as a plain output, UART a universal asynchronous
receiver-transmitter, TX its transmit pin and RX its receive pin. IOVDD is
the RP2350's GPIO supply, 3.3 V on this board, VBUS the USB 5 V supply line,
and BOOTSEL the RP2350's USB boot mode for flashing. IMU is inertial
measurement unit, RTC real-time clock, GND ground, and BEC battery
eliminator circuit, an ESC's regulated output for a receiver and servos.
SIG is the ESC signal pin as [pin-claim.md](pin-claim.md) defines it.

Source: rcbench, commit 453cf334, MIT, same holder: the core in
`protocols/kst` (7 C files and 7 headers, 5,598 lines), the RP2350 driver in
`protocols/kst/rp2350/kst_line.*`, the file store `shared/logfile/kst_store.c`,
the card glue `shared/bench/kst_card.c`, the port glue
`shared/outputs/kst_port.c` and `prog_port.c`, the panel
`shared/ui/kst_panel.c`, and `docs/ServoProgrammer.md`.

## What it does

On SIG, with DShot released:

1. searches for a servo by repeating the entry sequence while the user powers
   the servo up, or, with a switched supply, powers it up itself before each
   attempt ([switched servo supply](#switched-servo-supply));
2. reads the 32 registers, three passes that must agree, and keeps the first
   image read as the session's backup;
3. shows the named fields with their units, and edits the editable ones within
   the hard limits of `kst_limits.h`;
4. writes a change as a plan of single-register writes with read-back and
   roll-back (`kst_session_edit()`), then verifies all 32 registers;
5. saves and loads servo files on the SD card, and restores the backup.

Not in this slice: unlocking locked fields, the pairing release, the limit
run, voltage selection and supply measurement. An optional switch of the
servo's supply is in this slice: a DC-DC (direct current to direct current)
converter whose enable input the bench drives.

## Electrical

| Need | Value | On MiniRcBench |
|---|---|---|
| Pin | bidirectional: push-pull for the frame, driven low for 50 µs after its last edge, then an input for the reply window, then driven low again | SIG, through `kst_line` |
| Idle | driven low by the master between transactions | the driver holds it low from `kst_line_open()` to `kst_line_close()` |
| Pull-down | pad pull-down on from the open, 50 kΩ to 80 kΩ; rcbench expects one on the board too | internal only; see below |
| Master half-cell | 25.40 µs, required to 50 ns, edge jitter under 100 ns | 3810 cycles of the 150 MHz `clk_sys`, exact |
| Clock tolerance | the open is refused beyond 0.2 % | 149.7 MHz to 150.3 MHz; nothing in MiniRcBench changes `clk_sys` |
| Servo half-cell | 25.575 µs, accepted 24.3 µs to 26.9 µs | decoder in `kst_wire.c` |
| Edge stamp | 4 cycles, 26.7 ns at 150 MHz | |
| PIO | 1 SM, 24 of 32 words | pio2 once DShot and KISS are released; [budget](pin-claim.md#pio-and-dma-budget) |
| DMA | 2 channels | 5 of 16 in use during a KST claim |
| Reply level | at most 5 V, typically 3.3 V, not measured (rcbench README); 3.3 V on the owner's servo, by the owner's statement | SIG is held to 3.6 V; see below |

**Pull-down.** SIG is also the bidirectional DShot pin, which idles high on
the pad pull-up. A fixed 10 kΩ pull-down on that pin holds the idle line at
0.34 V to 0.79 V, below the 0.8 V input-low limit, and the first falling edge
of each ESC reply is lost (rcbench `hardware/docs/IOBoard.md`, idle bias). So
the board pin carries no external pull-down. The driver holds the line low
outside transactions, so the pull-down matters only inside a reply window,
1.5 ms after a read and 8 ms after a write, and after the close. If a bench
test shows the internal 50 kΩ to 80 kΩ is not enough, the pull-down goes into
a servo-only adapter lead, never onto an ESC lead (decision 3).

**Reply level.** The servo drives the line during its reply. SIG is GP29, an
ADC pin of the RP2350A, for which a 5 V rating is not checked
([signal level](pin-claim.md#signal-level)); the MiniRcBench README treats
every pin as 3.6 V maximum. A servo that replies at 5 V is out of range.
The owner states that the servo replies at 3.3 V, so no level shifter is
needed for it (decision 4); that is the owner's statement, not a recorded
measurement. The 3.6 V rule stands for every servo: one that replies above
3.6 V needs a level shifter in its lead.

Wiring is the servo diagram in [pin-claim.md](pin-claim.md#wiring), servo A
only. The servo's supply is the user's, or, with a switched supply, the
converter's ([switched servo supply](#switched-servo-supply)).

## Switched servo supply

The owner reports that the servos enter programming mode only directly after
power-on; that is not measured
([timing against the power-on window](#timing-against-the-power-on-window)).
With the supply in the bench's hand, the bench powers the servo up itself, at
a known time before the entry sequence, instead of asking the user to plug it
in while the search repeats. rcbench switches the servo's supply through its
PD mini, a USB PD (Power Delivery) supply module that it sets and reads back.
MiniRcBench has an on and off switch and no read-back.

The switch is optional. `SERVO SUPPLY` in SETTINGS says whether one is
fitted: `NONE`, the default, or `GP0` (decision 8). Every rule in this section
holds with `GP0` only. With `NONE` the bench never drives GP0, the search is
the one in [entering programming mode](#entering-programming-mode), and
every screen text that names the supply has its form without a switch.

### Hardware

A DC-DC converter between the user's supply and the servo's supply wire. The
bench drives the converter's enable input, EN; it does not feed the
converter. VIN and VOUT are the converter's input and output.

| Item | Value |
|---|---|
| Enable pin | GP0, J5 pin 2 (also J4 pin 9) |
| Level | 3.3 V push-pull from IOVDD; high is on, low is off (active-high) |
| Off-state resistor | external pull-down from EN to the supply's minus: 10 kΩ to 100 kΩ for an EN with no internal pull-up; with an internal pull-up, the divider sets the value ([example modules](#example-modules)) |
| Converter type | buck (step-down), or another type with output disconnect, such as a four-switch buck-boost or a synchronous boost with load disconnect. A disabled boost converter without one passes VIN minus a diode drop to the servo. A plain non-synchronous boost module, for example a 3.7 V to 5 V module with only VIN, GND and VOUT, does not qualify: it has no EN, and its input passes to the output |
| EN input | takes 3.3 V logic as high. With an internal pull-up from EN to VIN, the pull-up and the external pull-down form a divider while GP0 is undriven or unpowered, and at the highest VIN used it must meet two limits. First, EN below the converter's falling threshold; otherwise the converter is on outside a claim, while DShot runs. With the Pololu module below, a 100 kΩ pull-down puts 2.09 V on EN at 12 V, above its highest rising threshold of 1.2 V. Second, EN at most 3.6 V, the README's limit for every pin and below the 3.63 V an unpowered pin is rated for ([signal level](pin-claim.md#signal-level)); the same 100 kΩ gives 3.83 V at 22 V. No lower limit applies to an unpowered GP0: the first limit holds EN below the falling threshold, 0.84 V or more on the Pololu, and no source here rates an unpowered pin below 3.63 V. A module whose divider fails either limit needs a smaller pull-down, or its pull-up removed |
| Converter output | 5 V nominal, for every servo the owner programs, HV (high-voltage) servos included (decision 16); rated for the servo's stall current, more than 1 A. The servo tester's range in the [pin-claim.md wiring](pin-claim.md#wiring) is not this requirement |
| Start-up time | from EN high to VOUT in range: unknown until bring-up |
| Converter input | the user's supply. J4 pin 2 is USB VBUS and does not feed a servo |
| Ground | servo minus to the supply's minus; one tap from the supply's minus to board GND |

GP0 is UART0's TX pin on J4 and J5. The firmware leaves UART0 unused
(`pico_enable_stdio_uart(MiniRcBench 0)`; USB serial is `usb_dev.cpp`), and
SETTINGS does not offer GP0 for the ESC wires. GP0 is on PWM slice 0 with the
backlight, GP16, so the enable is a plain GPIO output and never a PWM
channel.

**Off state.** The external pull-down holds EN low whenever nothing drives
GP0:

- Board unpowered: GP0's pad has no supply, and the pull-down alone holds EN
  low.
- From reset: GPIO0's pad comes up with its pull-down enabled, its input
  disabled and its isolation on (`PADS_BANK0_GPIO0_RESET`, 0x116, in the Pico
  SDK, software development kit, 2.1.1,
  `src/rp2350/hardware_regs/include/hardware/regs/pads_bank0.h`). The
  pad pull-down and the external one act together until `servoSupplyInit()`.
  Erratum RP2350-E9 affects a pad whose input is enabled: its leakage can hold
  the pin high against the internal pull-down. GP0's input is disabled from
  reset, so E9 cannot latch it.
- BOOTSEL: what the boot ROM (read-only memory) does with GP0 in BOOTSEL is
  not checked. A boot ROM that drove GP0 high there would switch the
  converter on. The design relies on the external pull-down; bring-up step 7
  checks it.
- Outside a claim that uses the supply: GP0 is an input with both pulls off
  and its input disabled, so the external pull-down alone holds EN low. A
  missing or too-weak pull-down leaves the converter on outside the claim,
  while DShot runs, with its output on the lead's middle wire. Bring-up
  step 7 checks EN and VOUT on the main screen.

**Ground.** The servo's minus goes to the supply's minus, and one tap joins the
supply's minus to board GND, as in the [pin-claim.md
wiring](pin-claim.md#wiring). The servo's stall current then returns to the
supply and never passes through J5 pin 4, a JST-SH 1.0 mm contact. The tap may
be J5 pin 4, or J4 pin 1 or 5.

**An ESC on the lead.** The converter's output is the lead's middle wire. On
an ESC that wire is the BEC output, the ESC's own regulated supply, and the
converter drives against it. The claim's ESC check
([interlocks](pin-claim.md#interlocks)) does not see an ESC that does not
speak bidirectional DShot, nor an ESC plugged in after the claim. GP0 is not
driven during that check, so the converter is off while DShot runs. The
README's wiring section and the search screen warn of it.

```
  user supply +  ------- VIN    DC-DC    VOUT  ---------  servo +  (red)
  user supply -  ---+--- GND             EN  ----+------  GP0, J5 pin 2
                    |                            |
                    |                       10 k pull-down
                    |                            |
                    +----------------------------+------  servo -  (brown)
                    |
  board GND  -------+    the one tap: J5 pin 4, or J4 pin 1 or 5
  board SIG  -------------------------------------------  servo signal
                                                          (GP29, J4 pin 12)
```

The drawing shows the 10 kΩ pull-down the Pololu example below needs. For an
EN with no internal pull-up, any value from 10 kΩ to 100 kΩ holds it off.

#### Example modules

Examples, not owner decisions. The Pololu module meets the requirements
above with the 10 kΩ pull-down below. The PowerBoost meets them only once its
pull-up's value is known, since that value sets the pull-down it needs and
the level GP0 sees.

| Module | Type | VIN | Output | EN | Source |
|---|---|---|---|---|---|
| Pololu S13V30F5 | step-up/step-down (buck-boost) | 2.8 V to 22 V; reverse-voltage protection to 20 V | 5 V, 3 % accuracy; typical maximum continuous output 2 A to 4 A depending on VIN, limited by thermal dissipation; 22.9 mm × 22.9 mm × 9.7 mm | on by default through a 475 kΩ pull-up to VIN; rising threshold 1 V to 1.2 V, falling at most 160 mV lower, so at least 0.84 V; sleep draw 2 µA to 10 µA per volt of VIN | Pololu product page [4082](https://pololu.com/product-info-merged/4082) |
| Adafruit PowerBoost 1000 Basic, the alternative from one lithium cell | synchronous boost, TI TPS61030 | one cell | 5.2 V | pulled up to the battery, resistor value not stated, so it meets the requirements only once the value is known; 20 µA disabled. Adafruit: "connecting the ENable pin to ground ... will completely turn off the output" | [Adafruit guide](https://learn.adafruit.com/adafruit-powerboost-1000-basic) |

The Pololu divider, with a 10 kΩ pull-down from EN to GND and GP0 undriven
or unpowered: EN = VIN × 10 kΩ / (475 kΩ + 10 kΩ) = VIN × 10 / 485. That is
0.25 V at 12 V and 0.45 V at 22 V, below the lowest falling threshold of
0.84 V, so the converter is off, and far below 3.6 V. For EN to stay below
0.84 V at 22 V the pull-down must be below 18.9 kΩ
(475 kΩ × 0.84 V / 21.16 V); 18 kΩ is the largest standard value under it.
At 100 kΩ, EN reads 2.09 V at 12 V, so
the converter is on, and 3.83 V at 22 V, above GP0's 3.6 V. GP0 driving
3.3 V is above the 1.2 V highest rising threshold, so the converter is on,
and the pull-up then sources at most (22 V − 3.3 V) / 475 kΩ, about 39 µA,
into the GP0 output.

Not stated by Pololu: what VOUT does while EN is low, and whether it is
disconnected from VIN. A four-switch buck-boost is expected to disconnect, so
this is a bring-up check: VOUT with EN low, at the lowest and the highest VIN
used.

For the PowerBoost the pull-up's value is not stated, so the pull-down it
needs and the level an unpowered GP0 sees are not known, and the module meets
the requirements only once they are; its schematic is not checked here. Its
battery reaches 4.2 V, above GP0's 3.6 V limit, so the divider matters there
too.

### Firmware

`src/servo_supply.{h,cpp}` owns GP0.

| Call | Core, when | What it does to GP0 |
|---|---|---|
| `servoSupplyInit()` | core0, in `setup()` directly after `settingsLoad()`, before core1 starts | with `GP0`: SIO function, output disabled, both pulls off, input disabled. Selecting SIO with `gpio_set_function()` clears the pad's isolation and its output disable and turns its input on (Pico SDK 2.1.1, `src/rp2_common/hardware_gpio/gpio.c`), so the input is disabled after it. The SIO output enable stays off, so the pin is not driven. With `NONE`: nothing; GP0 keeps its reset state |
| `servoSupplyConfigure()` | core0, from SETTINGS' `applyLive()`, outside every claim | as `servoSupplyInit()`, for the new value. A change to `NONE` leaves GP0 an input with both pulls off |
| `servoSupplyBind()` | core1, from the KST job's bind, with `GP0` | drives GP0 low: off |
| `servoSupplyOn()`, `servoSupplyOff()` | core1, the KST job, between bind and release, with `GP0` | drives GP0 high or low |
| `servoSupplyRelease()` | core1, from the KST job's release, at every claim end, with `GP0` | drives GP0 low, then makes it an input with both pulls off and its input disabled |
| `servoSupplyIsOn()` | core1 | the level the bench drives; there is no read-back |

GP0 is driven only while a claim that uses the supply holds: the KST claim,
and the servo tester's once decision 10 is built. Two cores write GP0's pad,
never at once: core0 at boot and at a SETTINGS change, core1 only inside a
KST claim, and SETTINGS is unreachable while a claim holds. The level is a fact
core1 writes and core0 reads, as `KstStatus.supply`, under the KST job's
status lock. The bench knows the level it drives, not the voltage on the
servo.

**A core1 hang.** The supply rules below run on core1, the heartbeat rule
among them, and the firmware enables no watchdog timer. A core1 that hangs
with the supply on leaves it on: nothing in the firmware switches it off.
This is a known limitation. What bounds it lies outside the firmware: a reset
or a power-off of the board returns GP0 to the off state above, and the user
can switch off the supply. A core0 that hangs is covered: core1 sees the
heartbeat lapse and switches the supply off.

### Search with the switch

One attempt, from `kstSeekStep()`:

1. Supply off, line driven low, for `KST_SUPPLY_OFF_MS`, 2000 ms, counted
   from the switch-off command (rcbench `KST_PORT_RAIL_OFF_MS`, the owner's
   value). Every search starts here, so a servo whose supply was on is off
   for 2000 ms first.
2. Supply on, only while the heartbeat is trusted (`core1UiAlive()`). The
   line stays low.
3. `kst_session_enter()`, unchanged, `KST_SUPPLY_ENTRY_DELAY_MS` after the
   switch-on: 0 ms by decision 9 ([timing](#timing-of-the-switch-on)).
4. `KST_SES_OK` is `Found`, and the supply stays on. Otherwise the next
   attempt starts at step 1.

One attempt is 2000 ms, plus `KST_SUPPLY_ENTRY_DELAY_MS`, plus the entry's
116.6 ms to 118.8 ms: about 2.12 s at a delay of 0 ms. The timeout is
checked outside the entry too: a search that reaches `KST_SEEK_TIMEOUT_MS`,
30 s, in an off phase or in a positive delay's wait ends there as
`TimedOut`, with the supply off, and an attempt is counted at its switch-on.
At a delay of 0 ms the 30 s search then holds 14 power-ups: 14 attempts end
by 29.66 s, the 15th is in its off phase at 30 s, and `KstStatus.attempts`
reads 14. Another delay gives another count: 13 at 40 ms.

STOP SEARCH, BACK and the timeout end the search with the supply off. In an
off phase the supply is not switched on. In an on phase it goes off once
`kst_session_abort()` has finished the frame on the wire.

A switched search with no switch behind it: with `SERVO SUPPLY` at `GP0` and
no converter fitted, or with the servo on a supply of its own, the switch
does nothing to the servo. The search then makes 14 attempts with 2000 ms of
silence before each, and frames go out in about 118 ms of each 2.12 s. A
servo powered up by hand most likely powers up in an off phase, so a
timed-out search says nothing about the servo's window. The switched
timed-out text therefore names SETTINGS > SERVO SUPPLY and the wiring
([screens](#screens-with-the-switch)).

### Timing of the switch-on

The servo's supply comes up one converter start-up time after the switch-on
command, and that time is unknown until bring-up. With
`KST_SUPPLY_ENTRY_DELAY_MS` at 0 ms the entry starts at the switch-on: its
first read goes out about 100 ms later, its sync about 106 ms later. The
servo therefore sees the first sync about 106 ms minus the start-up time
after its own power-on, and the first read about 6 ms before that. A
power-on window that closes earlier is unreachable at 0 ms.

A negative delay moves the switch-on inside the entry's 100 ms low: the
wrapper starts `kst_session_enter()` first and switches the supply on that
many milliseconds later. At −40 ms the sync reaches the servo about 66 ms
minus the start-up time after its power-on. The delay stays above −100 ms
plus the start-up time, so no frame goes out before VOUT is in range. A
positive delay waits that long after the switch-on before the entry starts.

The delay is a build-time constant; nothing in SETTINGS sets it. Bring-up
step 7 steps it with the owner's servo on the converter: from 0 ms down in
10 ms steps to that lower limit, then from 0 ms up in 50 ms steps to 1000 ms,
with 10 power-ups at each value, recording how many enter. The delays at
which all 10 enter are the window, and the constant takes a value inside it.

### Leave, REBOOT, loss and heartbeat with the switch

- **Leave.** LEAVE switches the supply off at once; the line stays low. After
  `KST_SUPPLY_OFF_MS`, 2000 ms from the switch-off command, the job closes the
  line and releases GP0, and the unplug page follows. A servo without power is
  in no mode (rcbench `shared/outputs/include/kst_port.h`), so the leave page
  does not ask the user to power it off. How long a KST servo needs without
  power to leave programming mode is not measured; 2000 ms is the owner's
  value. Before the tap the page reads
  `LEAVE SWITCHES THE SERVO'S SUPPLY OFF.`; during the wait,
  `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` A locked session leaves the same way.
  BACK during the wait does nothing: the supply is off, and the wait ends at
  the unplug page. HOLD TO REBOOT is hidden during the wait, and the job
  refuses `Reboot` while Leaving.
- **REBOOT.** A 1000 ms hold on the leave page ([leave](#leave)), offered with
  `SERVO SUPPLY` at `GP0` only. The line stays low throughout. The supply goes
  off and the wrapper calls `kst_session_power_cycled()`. After
  `KST_SUPPLY_OFF_MS` the supply goes on, only while the heartbeat is trusted.
  After `KST_SUPPLY_WAIT_MS`, 1000 ms (rcbench `KST_PORT_RAIL_WAIT_MS`),
  `kst_session_enter()` and `kst_session_read_all()` follow. Both times count
  from the switch commands. This is not rcbench's REBOOT: rcbench counts from
  its PD mini's read-back of off and on and then returns the port to PWM. Here
  there is no read-back, and an entry and a read follow.
  - `kst_session_power_cycled()` drops the backup, `wrote_regs` and the lock
    (rcbench `protocols/kst/include/kst_session.h`), and the read after the
    reboot becomes the new backup. Without a card the old backup exists
    only in memory, so REBOOT loses it; the hold is the confirmation, and
    the leave page says so above it.
  - In a locked session REBOOT is how the servo is read as it is; the
    restore then goes through FILES, LOAD and HOLD TO WRITE.
  - Refused while an operation runs or plans, and while Leaving: the job
    refuses `Reboot`, and the button is drawn disabled and gated per
    `ui_widgets.h`, or hidden during LEAVE's wait.
  - BACK during REBOOT switches the supply off if it is on and closes as
    LEAVE does ([close](#close-with-the-switch)): the unplug page follows
    2000 ms after the switch-off command.
  - An entry with no answer starts the switched search, as RETRY does.
    Whether the owner's servo still enters about 1100 ms after power-on is
    unknown; if it does not, that search is what finds it.
- **Loss** of a found servo with the supply on is not a power cycle the bench
  made. The wrapper calls `kst_session_power_cycled()`, as [without a
  switch](#what-changes-here), and starts a new search, which begins with the
  supply off.
- **Heartbeat lapse.** The supply is on only while the heartbeat is trusted.
  That is rcbench's rule (`kst_port.h`: "the supply is switched on again
  only while the heartbeat is trusted"; "the supply ends off whenever nobody
  is there to switch it off").
  - An off phase, search step 1 or REBOOT's off wait, ends at once, and the
    supply is not switched on.
  - REBOOT's on wait, and the time between switch-on and entry, end at once
    with the supply off.
  - A running session operation, an entry, a read, a verify, a write or a
    restore, finishes, as [pin-claim.md](pin-claim.md#decisions) decision 1
    requires. Then the supply goes off.
  - With the supply off, a session that was in the mode gets
    `kst_session_power_cycled()`: `in_mode` clears, and the backup and the
    lock drop.
  - No new operation starts until the heartbeat returns. Then the screen
    shows the stall and RETRY ([screens](#screens-with-the-switch)).
  - A locked session is no exception (decision 14).
  - In a card phase, around every card call core0 makes in the claim, a
    card heartbeat stamped from the waits of the SDIO (Secure Digital
    input/output) card driver counts as the UI's, for at most 15 s ([card
    calls](#card-calls-and-the-heartbeat)).
- **Claim end, any path:** the supply is off, for 2000 ms from the switch-off
  command ([close](#close-with-the-switch)), and GP0 an input before the
  pins return to DShot, so the DShot pump never runs with the converter on.
  That covers the pump, not an ESC the claim's check misses ([an ESC on the
  lead](#hardware)).
- **Boot:** off. The reset state and the external pull-down hold EN low until
  `servoSupplyInit()`, then the external pull-down alone.

### Close with the switch

Every claim end after the supply was on at least once in the claim closes
the line and releases GP0 only once `KST_SUPPLY_OFF_MS`, 2000 ms, have passed
since the last switch-off command: LEAVE, BACK during REBOOT, BACK on the
stalled screen and BACK during a switched search alike (decision 18). BACK
switches the supply off first if it is on; time spent off before BACK counts.
Until the close the page reads `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`, BACK
does nothing, and the unplug page follows. Without the wait, a servo that
entered programming mode in an on phase could reach the unplug page and
DShot after less than the 2000 ms without power that LEAVE gives it.

### Card calls and the heartbeat

Core0 calls the card several times while the KST claim holds: before the
session's first write it saves the backup under the ring's next name and the image
before the write as `KST/KST.TXT` ([storage](#storage)); SAVE AS writes a file;
LOAD and the FILES listing walk the `KST` directory and read a file; RETRY MOUNT
initialises and mounts the card; a verified write, and LEAVE from a servo in
programming mode that is not in doubt, remove `KST/KST.TXT`; a card mounted
while the write page is up gets the ring's next name looked up. A card call can
block core0 past the 250 ms heartbeat. The SDIO driver waits up to 1000 ms for
busy after each written block and tries a chunk up to 3 times
(`SDH_BUSY_TIMEOUT_US` and `SDH_ATTEMPTS` in `src/sdio/sd_host.h`,
[sdio-driver.md](sdio-driver.md)); the SD specification lets a card take 250 ms
per written block on an SDHC (high capacity) card, 500 ms on an SDXC (extended
capacity) card (4.6.2.2), and 100 ms per read block. With the switch such a
stall would count as a lapse: the supply goes off, and
`kst_session_power_cycled()` drops the backup in memory, the one the first
write's card call was saving among them.

**The card phase** (decisions 17 and 19). With `SERVO SUPPLY` at `GP0`,
every card call core0 makes while the KST claim holds runs inside one:

1. `ui_kst.cpp` sends `CardBegin` and waits for the job's acknowledge. The
   job takes it only while no operation runs or plans, so a card call waits
   for a running operation to end. The line stays low and the supply as it
   is.
2. Core0 makes the card calls. The SDIO driver offers one generic wait hook,
   `sdh_wait_hook`, a function pointer declared in `src/sdio/sd_host.h` and
   null by default, which it calls on every poll of its waits: `busyWait()`
   (`src/sdio/sd_host.c:29-37`), and the command, receive and status waits on
   `time_us_32()` in `src/sdio/sd_pio.c` (`:111-131`, `:228-245` and
   `:273-281`). The driver knows nothing of KST. While the card phase is
   open, `kst_files.cpp` sets the hook to `kstCardStamp()`, which stamps the
   time on core0, and sets it back to null when the phase closes.
3. Core0 closes the phase: with `Edit` or `Restore` after the first write's
   card calls, and with `CardEnd` after every other card call, or when the
   card took neither file; the write then stops with
   `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`

In the card phase the KST job counts the heartbeat as trusted while
`core1UiAlive()` holds, or while `kstCardStampFresh()`, core1's reader of the
stamp, finds it within 250 ms and the phase has been open for less than
`KST_CARD_PHASE_MAX_MS`, 15,000 ms, from the job's acknowledge of
`CardBegin`. A core0 that hangs outside the driver's waits stamps nothing,
and the lapse rule applies 250 ms later. A core0 stuck in an outer loop that
keeps calling the driver stamps without end; the cap ends that after 15 s,
and the lapse rule applies. The job starts no operation in the card phase.

**The cap.** The card phases are counted, not estimated. `make -C test
kstcount` runs `src/kst_files.cpp`, `src/backup_ring.cpp`,
`src/am32_backup.cpp` and rcbench's `kst_store.c` over FatFs R0.15, from the
SD library at the commit `CMakeLists.txt` pins and with the library's
`ffconf.h`, on a RAM disk that counts every block it reads and writes
(`test/kst_card_count.cpp`). Each phase runs on a full card: a ring of
`BACKUP/` that has turned, KST and AM32 backups by turns, `BACKUP/LAST`
([backups.md](backups.md)), and 49 SAVE AS files in `KST/`
([storage](#storage)). The volumes are FAT32 of 32 GiB with 32 KiB clusters,
as the SD Association formats an SDHC card, and exFAT of 64 GiB with 128 KiB
clusters, as it formats an SDXC card. Each holds no blackbox log, 1000 logs in
`LOGS/`, or 1000 logs in the root as an earlier firmware wrote them, made
before `KST` and `BACKUP`, so a lookup of either passes over all of them.
Each total is the reads times one read plus the writes times one write:

- a typical card: 10 ms a read, 10 ms a write;
- SDHC within the specification: 105 ms a read (5 ms command, 100 ms
  access), 261 ms a write (5 ms command, 1.2 ms transfer, 250 ms busy, 5 ms
  status);
- SDXC within the specification: 105 ms a read, 511 ms a write (500 ms
  busy).

<!-- kst-card-count:start -->
| Volume | Blackbox logs | Card phase | Block reads | Block writes | Typical card | SDHC | SDXC |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| FAT32, 32 GiB, 32 KiB clusters | none | KST first write, no old record | 26 | 12 | 0.4 s | 5.9 s | 8.9 s |
| FAT32, 32 GiB, 32 KiB clusters | none | KST record removed after the verify | 16 | 4 | 0.2 s | 2.7 s | 3.7 s |
| FAT32, 32 GiB, 32 KiB clusters | none | KST first write, an old record kept | 39 | 18 | 0.6 s | 8.8 s | 13.3 s |
| FAT32, 32 GiB, 32 KiB clusters | none | KST SAVE AS, the 50th file of KST/ | 23 | 7 | 0.3 s | 4.2 s | 6.0 s |
| FAT32, 32 GiB, 32 KiB clusters | none | KST FILES page | 21 | 0 | 0.2 s | 2.2 s | 2.2 s |
| FAT32, 32 GiB, 32 KiB clusters | none | AM32 backup | 9 | 6 | 0.1 s | 2.5 s | 4.0 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | KST first write, no old record | 26 | 12 | 0.4 s | 5.9 s | 8.9 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | KST record removed after the verify | 16 | 4 | 0.2 s | 2.7 s | 3.7 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | KST first write, an old record kept | 39 | 18 | 0.6 s | 8.8 s | 13.3 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | KST SAVE AS, the 50th file of KST/ | 23 | 7 | 0.3 s | 4.2 s | 6.0 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | KST FILES page | 21 | 0 | 0.2 s | 2.2 s | 2.2 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | AM32 backup | 9 | 6 | 0.1 s | 2.5 s | 4.0 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | KST first write, no old record | 338 | 12 | 3.5 s | 38.6 s | 41.6 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | KST record removed after the verify | 78 | 4 | 0.8 s | 9.2 s | 10.2 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | KST first write, an old record kept | 413 | 18 | 4.3 s | 48.1 s | 52.6 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | KST SAVE AS, the 50th file of KST/ | 85 | 7 | 0.9 s | 10.8 s | 12.5 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | KST FILES page | 207 | 0 | 2.1 s | 21.7 s | 21.7 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | AM32 backup | 71 | 6 | 0.8 s | 9.0 s | 10.5 s |
| exFAT, 64 GiB, 128 KiB clusters | none | KST first write, no old record | 58 | 10 | 0.7 s | 8.7 s | 11.2 s |
| exFAT, 64 GiB, 128 KiB clusters | none | KST record removed after the verify | 36 | 2 | 0.4 s | 4.3 s | 4.8 s |
| exFAT, 64 GiB, 128 KiB clusters | none | KST first write, an old record kept | 89 | 14 | 1.0 s | 13.0 s | 16.5 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | none | KST SAVE AS, the 50th file of KST/ | 56 | 5 | 0.6 s | 7.2 s | 8.4 s |
| exFAT, 64 GiB, 128 KiB clusters | none | KST FILES page | 47 | 0 | 0.5 s | 4.9 s | 4.9 s |
| exFAT, 64 GiB, 128 KiB clusters | none | AM32 backup | 23 | 6 | 0.3 s | 4.0 s | 5.5 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | KST first write, no old record | 58 | 10 | 0.7 s | 8.7 s | 11.2 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | KST record removed after the verify | 36 | 2 | 0.4 s | 4.3 s | 4.8 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | KST first write, an old record kept | 89 | 14 | 1.0 s | 13.0 s | 16.5 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | KST SAVE AS, the 50th file of KST/ | 56 | 5 | 0.6 s | 7.2 s | 8.4 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | KST FILES page | 47 | 0 | 0.5 s | 4.9 s | 4.9 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | AM32 backup | 23 | 6 | 0.3 s | 4.0 s | 5.5 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | KST first write, no old record | 999 | 10 | 10.1 s | 107.5 s | 110.0 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | KST record removed after the verify | 228 | 2 | 2.3 s | 24.5 s | 25.0 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | KST first write, an old record kept | 1218 | 14 | 12.3 s | 131.5 s | 135.0 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | KST SAVE AS, the 50th file of KST/ | 249 | 5 | 2.5 s | 27.4 s | 28.7 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | KST FILES page | 613 | 0 | 6.1 s | 64.4 s | 64.4 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | AM32 backup | 210 | 6 | 2.2 s | 23.6 s | 25.1 s, over the cap |

Generated by `make -C test kstcount-write`. CI runs `make -C test kstcount` and fails on drift.
<!-- kst-card-count:end -->

What bounds the counts:

- `BACKUP/` holds at most 50 backups and the marker, and SAVE AS refuses once
  `KST/` holds 50 files. On FAT32 a name of 8.3 form is one entry of 32 bytes,
  so each directory with `.` and `..` is 53 or 54 entries, 4 sectors. On exFAT
  a file is a set of at least 3 entries, 96 bytes, about 10 sectors for the
  same files. Deleted entries stay in a directory and count: a directory that
  once held more files keeps its length.
- The root holds `LOGS`, `KST` and `BACKUP`: a lookup in it reads one block,
  however many logs `LOGS/` holds. Logs an earlier firmware wrote to the root
  stay there, and each adds 32 bytes to every lookup in the root on FAT32 and
  96 on exFAT. A KST first write works in two directories, so it looks one of
  them up in the root several times: with 1000 such logs, 338 block reads on
  FAT32 and 999 on exFAT. Moving those logs into `LOGS/` on a computer takes
  the root back to three entries.
- Every card phase makes one directory FatFs's current one
  (`CardDirScope`, `src/card_dir.h`), so the paths inside it name the file
  alone.
- The ring's oldest backup is written over in place, which keeps its
  cluster: its data block and its directory entry, and no FAT sector.
- `BACKUP/LAST` is rewritten in place once per phase, also for a first write
  that keeps an old record and saves a backup.

Against the cap of 15 s at the SDXC times, with no log in the root: every
phase on FAT32 stays under it, the longest the first write that keeps an old
record, 13.3 s; on exFAT that same write takes 16.5 s, over it. A first write
keeps an old record only after a write was interrupted. With an earlier
firmware's 1000 logs in the root, first writes pass the cap on both volumes,
up to 135.0 s. The write cache of [sd-write-cache.md](sd-write-cache.md),
planned, takes the writes off core0's path, which brings the exFAT first
write under the cap.

The cap, 15 s, bounds how long a core0 stuck in the driver's loops keeps the
servo powered in programming mode, with no frame on the wire. A card
past the cap: the supply goes off, the session is power-cycled as for any
lapse, and the card call goes on to its end on core0; a write that waited for
it then finds the session out of the mode and does not run.

What remains:

- The screen does not repaint and touch is not read while the card works,
  up to the cap. The servo sits powered in programming mode at its unchanged
  image, and no frame goes out.
- A card wait that times out keeps the supply on until it does. The driver
  then returns an error, and the write stops as above, with the supply on
  and the backup in memory.
- The card heartbeat counts only for the KST job in a card phase. The servo
  job and the DShot pump keep the 250 ms rule unchanged.
- A blackbox log started by hand on SD LOG is not stopped by the claim. Its
  card writes are outside the card phase, so a stall there counts as a
  lapse. Whether such a log stalls core0 past 250 ms during a KST claim is
  not measured.

### Screens with the switch

| Step | Strip | Body | Controls |
|---|---|---|---|
| Searching, supply switched | `GP29  TRY 3  SUPPLY OFF 1.4 S`, or `SUPPLY ON`; TRY is the attempt under way, `KstStatus.attempts` + 1 in an off phase | `SWITCHED SUPPLY`, then `THE BENCH SWITCHES THE SERVO OFF FOR 2 S, THEN ON, BEFORE EACH TRY.`, the failsafe line, and in amber `AN ESC ON THIS LEAD GETS THE CONVERTER'S OUTPUT ON ITS BEC WIRE.`; a spinner | STOP SEARCH, BACK |
| Timed out, supply switched | `NO ANSWER IN 14 POWER-UPS`, the number from `KstStatus.attempts` | `CHECK THE CONVERTER ON GP0, ITS WIRING AND THE SIGNAL. A SERVO ON ITS OWN SUPPLY NEEDS SETTINGS > SERVO SUPPLY AT NONE.` | RETRY, BACK |
| Stalled, supply switched | `GP29  SUPPLY OFF` | `THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF. RETRY POWERS IT UP AND READS IT AGAIN.` Without a card, after a write, in amber, `NO CARD: THE BACKUP IN MEMORY IS LOST.` and nothing more. With a card, for a locked session, in amber, `THE LAST WRITE IS IN DOUBT. AFTER THE READ, LOAD KST005 IN FILES AND HOLD TO WRITE.` | RETRY, BACK |
| REBOOT | `GP29  SUPPLY OFF 1.4 S`, then `SUPPLY ON 0.6 S` | `REBOOT: DO NOT UNPLUG THE SERVO.`, then `READING 32 REGISTERS` | BACK |

BACK on the stalled screen goes to the unplug page once the supply has been
off for 2000 ms; the session is not in the mode. BACK during REBOOT switches
the supply off if it is on, waits as LEAVE does, 2000 ms from the switch-off
command, and then shows the unplug page ([close](#close-with-the-switch)).

The layout of the switched search, where the lines are longer than without a
switch:

```
y   0 | KST SERVO                        [ BACK ]|  header 36 px (menu.md)
   40 | GP29  TRY 3  SUPPLY OFF 1.4 S            |
   70 |           SWITCHED SUPPLY                |  scale 2, C_LIME, 178 px
  100 |   THE BENCH SWITCHES THE SERVO OFF FOR   |  scale 1, C_DIM
  114 |   2 S, THEN ON, BEFORE EACH TRY.         |
  132 |   A POWERED SERVO ON THIS PIN GETS NO    |
  146 |   PULSES AND MAY GO TO ITS FAILSAFE.     |
  164 |   AN ESC ON THIS LEAD GETS THE           |  C_AMBER
  178 |   CONVERTER'S OUTPUT ON ITS BEC WIRE.    |
  196 |   REPLY NOT VALID: LATE                  |  C_AMBER, only with edges
  236 |              (spinner)                   |
  272 | [          STOP SEARCH           ]       |  46 px; RETRY after a stop
  320 +------------------------------------------+
```

### SETTINGS row

Slice 3 adds the `SERVO SUPPLY` row to the SETTINGS list of
[menu.md](menu.md#settings). It is a section of its own, SERVO SUPPLY,
between WIRING and DISPLAY, contributed by KST's registry entry (menu.md
decisions 8 and 13). The list's geometry, gestures, fixed footer and asserts
are menu.md's, built in its slice C.

- Values: `NONE`, 0, the default, for no switch fitted, and `GP0`, 1, an
  active-high enable on GP0 (decision 8).
- The row is a toggle, as KISS TELEM is: a tap switches `NONE` and `GP0`. A
  change applies at once through `servoSupplyConfigure()`; no claim holds
  while SETTINGS is open, so nothing is driven. HOLD TO SAVE stores it with
  every other row, and RESET sets it to `NONE` with the other compiled
  defaults.
- Version: the row adds the `servoSupply` field to the settings block, so
  `SETTINGS_VERSION` goes from 2 to 3. The first boot after slice 3 discards
  the stored block and starts from the compiled defaults, every setting and
  the ESC pin included. A block DshotDisplay saved does not load, and
  DshotDisplay discards a version 3 block.
- Validation: `settingsValidate()` sets any value but 0 or 1 to 0.
- Fit: the label, 71 px at scale 1, ends at x 78; the 94 × 44 px toggle
  starts at x 138. The list grows by a 20 px caption and a 50 px row, from
  530 px to 600 px, and its scroll range from 0 px to 330 px to 0 px to
  400 px.

At 230 px:

```
   40 | DSHOT KBAUD      600  [  -  ] [  +  ]     |  cut: 4 px, takes no press
   50 | KISS TELEM          [    OFF     ]        |
  100 | KISS PIN          --  [  -  ] [  +  ]     |  disabled while KISS is off
  150 | SERVO SUPPLY                              |  section caption
  170 | SERVO SUPPLY        [    NONE    ]        |  toggle, scale 2
  220 | DISPLAY                                   |  caption, ends at y 239
```

### Not in this section

- Voltage selection. A converter with I2C (inter-integrated circuit) control,
  or a digital potentiometer on its feedback, on I2C1 (GP6, GP7, on J3 and
  J4). The bus carries the touch controller (0x1A), the IMU (0x6B) and the
  RTC (0x51); a new device avoids those addresses, and a fault on its lead
  takes the touch screen down with it.
- Supply measurement: an INA226 current and voltage monitor at 0x40 to 0x4F
  on the same bus would give the read-back rcbench has from its PD mini.
- The servo tester's use of the supply: decided (decision 10), and built in a
  change after KST programming. Until then the servo tester never drives
  GP0, so with a converter fitted its servo is unpowered unless it has a
  supply of its own.

### Tests for the switch

`kst_sim.h` is copied unchanged. The switched-supply servo is a model in a
MiniRcBench test file, `test/kst_supply_sim.h`, around `kst_sim.h`'s servo.
It reads the fake GP0 level, applies a start-up time, and drives the
servo's `powered`: on while the supply is up, with `in_mode` cleared when it
goes off. For a servo with a power-on window it holds `powered` off outside
the window until the servo is in the mode, so a sync outside the window goes
unanswered. `test_kst_seek.cpp` and `test_kst_job.cpp` run against it:

- a window that covers the first sync: found on the first attempt; a window
  that closes before it: 14 attempts and the switched timed-out state;
- a negative `KST_SUPPLY_ENTRY_DELAY_MS`: the switch-on inside the low, and
  no frame before the switch-on plus the start-up time;
- supply off for 2000 ms before every attempt, counted from the switch-off
  command;
- leave: off at the command, closed 2000 ms later; `Reboot` refused while
  Leaving, and HOLD TO REBOOT hidden;
- the close: BACK during REBOOT's on wait, on the stalled screen and during
  a switched search each close 2000 ms after the switch-off command, not
  before; BACK in an off phase counts the time spent off before it;
- heartbeat lapse in an off phase (search step 1, REBOOT's off wait): the
  operation ends with the supply off, never switched on; a lapse during a
  write: the write finishes, then off, then `kst_session_power_cycled()`; a
  locked session goes off as well;
- the supply switched on only while the heartbeat is trusted;
- REBOOT: off for 2000 ms and on for 1000 ms, counted from the switch
  commands, then an entry and a read; `kst_session_power_cycled()` called;
  refused while an operation runs or plans, and with `NONE`; the UI sends it
  only after a 1000 ms hold, not on a slide off;
- the card phase: a fake card whose write blocks for 900 ms and calls
  `sdh_wait_hook` every 1 ms keeps the supply on and the backup, and the
  write follows; the same block without the calls, a core0 hung outside the
  driver, switches the supply off 250 ms after the last stamp and calls
  `kst_session_power_cycled()`; a phase that stamps every 1 ms for 20 s
  keeps the supply on until 15,000 ms after the acknowledge of `CardBegin`
  and then switches it off, and one of 14,900 ms keeps it on throughout; a
  card that takes neither file leaves the supply on, the backup kept and
  nothing written; `CardBegin` refused while an operation runs; a card phase
  around each of SAVE AS, LOAD, the FILES listing, RETRY MOUNT and the
  record's removal after a verify, each closed by `CardEnd`, and none with
  `NONE`; the hook null outside a phase; the servo job and the pump ignore
  the card heartbeat;
- GP0 not driven outside a claim: output disabled from boot through SETTINGS
  changes until the job's bind; after every claim end an input with both
  pulls off and its input disabled; with `NONE`, untouched through a whole
  KST claim;
- the supply off at every claim end, and off after BACK during an off phase;
- settings: the version 3 layout, the `servoSupply` clamp, a version 2 block
  discarded whole;
- SETTINGS, by control id: `SERVO SUPPLY` scrolled into view, toggled,
  saved and reset; the list's 400 px scroll range; every value the row shows
  against its row table entry. The list's gestures, partial-visibility rule
  and fixed footer are tested in [menu.md](menu.md#slices) slice C.

## Entering programming mode

### The sequence rcbench sends

`kst_session_enter()`: the line low for `KST_ENTRY_LOW_US`, 100 ms; a read of
register 0x01; if no reply, the sync burst of 64 high pulses of one half-cell
each; if no acknowledge, one more read of 0x01. The gap from the end of a
reply window to the next frame is `KST_GAP_US`, 3 ms, inside the entry
sequence. A valid reply to either read, or an acknowledge of the sync, ends
the operation with `KST_SES_OK` and `in_mode` set. Three silent windows end it
with `KST_SES_ERR_NO_SERVO`. A servo already in the mode answers the first
read.

| Step | Duration |
|---|---|
| Line low | 100 ms |
| Read 0x01: start latency up to 31 half-cells (787 µs), 54 half-cells (1.372 ms), window 1.5 ms | up to 3.66 ms |
| Gap | 3 ms |
| Sync burst: 127 half-cells (3.226 ms), window 1.5 ms | up to 5.51 ms |
| Gap | 3 ms |
| Read 0x01 | up to 3.66 ms |
| One attempt | 116.6 ms to 118.8 ms |

### What changes here

rcbench assumes a powered servo. Its CONNECT runs one entry, and with a PD mini
answering it can cycle the servo's supply itself (REBOOT: off for 2000 ms, on,
then 1000 ms more). The owner reports that the servos enter programming mode
only directly after power-on. Without a switched supply, MiniRcBench repeats
the entry while the user powers the servo up. With one, the bench powers the
servo up before each attempt ([switched servo supply](#switched-servo-supply)).

The repetition is a wrapper in MiniRcBench, `src/kst_seek.cpp`, around the
unchanged session. The ported `kst_session.c` stays byte for byte rcbench's.

```c
enum class KstSeek : uint8_t { Idle, Searching, Found, Stopped, TimedOut, NoUi, Failed };

void     kstSeekBegin(KstSeekState *s, kst_session_t *ses, uint32_t nowMs);
KstSeek  kstSeekStep(KstSeekState *s, kst_session_t *ses, bool uiAlive, uint32_t nowMs);
void     kstSeekStop(KstSeekState *s, kst_session_t *ses);   // STOP SEARCH, BACK
bool     kstSeekLost(KstSeekState *s, kst_session_t *ses, kst_ses_t r, bool uiAlive,
                     uint32_t nowMs);                          // the loss rule below
uint32_t kstSeekMs(const KstSeekState *s, uint32_t nowMs);
```

- One attempt is one `kst_session_enter()` stepped to its end, 116.6 ms to
  118.8 ms. Without a switched supply, `KST_SES_ERR_NO_SERVO` starts the next
  attempt at once; its own 100 ms low is the spacing. That is 8.4 to 8.6
  attempts a second, 8.6 in the simulation. With one, an attempt starts with
  2000 ms off ([search with the switch](#search-with-the-switch)). Frames take
  about 6 ms of each attempt (two reads of 1.37 ms and a sync of 3.23 ms); the
  rest is low line and listening.
- `KST_SES_OK` is `Found`. Any result but `KST_SES_ERR_NO_SERVO` and a
  stop's `KST_SES_ERR_ABORTED` ends the search as `Failed`, with that result
  shown, a stopped attempt's `KST_SES_ERR_DRIVER` too; a session that refuses
  the entry, because another operation runs, ends it at once.
- The session takes an abort before it reads the window. A STOP SEARCH while
  the sync is on the wire to a servo that acknowledges it ends the search as
  `Stopped` with the servo in programming mode; the first read of a RETRY
  finds it, as it finds any servo already in the mode.
- Without a switched supply the search ends as `TimedOut`, which offers
  RETRY (decision 2), when an attempt ends `KST_SEEK_TIMEOUT_MS`, 30 s, or
  later after the search began: at 30.0 s to 30.12 s, after 258 attempts in
  the simulation. With one, at a delay of 0 ms, it ends at 30 s in an off
  phase, after 14 power-ups
  ([search with the switch](#search-with-the-switch)). It ends as
  `Stopped` at STOP SEARCH or BACK once the attempt has ended aborted.
  `kstSeekStop()` calls `kst_session_abort()`, which finishes a frame on the
  wire before it stops.
- A heartbeat lapse ([pin-claim.md](pin-claim.md#heartbeat-and-watchdog))
  lets the running attempt end and starts no new one: `NoUi`. With a
  switched supply the search also ends with the supply off
  ([heartbeat](#leave-reboot-loss-and-heartbeat-with-the-switch)).
- Between attempts and after the search ends the line stays driven low by the
  open driver, with the pad pull-down on. It is never released to float while
  the screen is open.
- A reply is recognised by the session's own decoder, nothing else: first
  rising edge 350 µs to 600 µs after the frame's last edge, 11 cells, mean
  half-cell 24.3 µs to 26.9 µs, levels under 1.5 µs removed as glitches, the
  line low again within 100 µs of the nominal end. A window with edges that
  fail one of these is not a servo found; the screen shows the
  `last_reply` result code (`kst_rx_t`), so "a servo that answers wrongly" is
  told apart from silence, as the AM32 screen's `REPLY NOT UNDERSTOOD` does.

After a found servo is lost (a read ends with `KST_SES_ERR_NO_SERVO` in a
session that was in the mode), the wrapper calls
`kst_session_power_cycled()` and searches again. That is rcbench's
POWER_CYCLED, "the user's word that the servo was without power", said by the
loss itself: the backup and the lock drop, and the next servo read becomes the
new backup.

### Timing against the power-on window

The line is low continuously except for frames, so at the moment of power-on
the servo sees a low line. Within one attempt the reads go out at about
100 ms and 114 ms, and the sync at about 106 ms. A sync burst reaches the
servo within about 119 ms of its power-on and a read within about 105 ms, plus
however long the servo takes to boot.

How long after power-on the servo listens is unknown. rcbench records no
window: `docs/ServoProgrammer.md` lists "how long without power ends
programming mode" under measurements still open (item 7), and its session
assumes a powered servo can be entered. Whether a powered servo can be entered
is therefore unknown too: the owner reports that it cannot, rcbench assumes it
can, and neither is a recorded measurement. The search covers both: a servo
that takes entry while powered answers the first attempt. If the window proves
shorter than about 120 ms, some power-ups are missed and the user powers up
again (decision 1).

### A servo already running on PWM

A servo that is powered and was following pulses before the screen opened
sees the search like this:

- No pulse in a servo's range. After the 100 ms low the line is high only for
  one or two half-cells at a time, 25.4 µs or 50.8 µs, and the sync pulses are
  25.4 µs. The shortest pulse the servo tester sends is 400 µs. To a PWM
  decoder this is a lost signal.
- Electrically it is the same 3.3 V push-pull drive the PWM uses. The pad
  drives only during a frame and 50 µs after it; it listens in the window. A
  servo outside programming mode is not expected to drive the line; which
  side holds the line at idle is rcbench measurement 1, not done.
- Duty: about 6 ms of frames in each 117 ms to 119 ms attempt.

Not known: what a servo does with a frame outside programming mode (rcbench
measurement 3). What is known is what it does on a lost signal: whatever its
own failsafe says. On a KST servo that is ALLOW UNCONT, UNCONTROLLED POS and
UNCONTROLLED TIME: with ALLOW UNCONT on, the horn moves to UNCONTROLLED POS
after UNCONTROLLED TIME, 2.0 s on the one servo rcbench read. The search
screen says so.

### What the user sees

| Step | Strip | Body | Controls |
|---|---|---|---|
| Checking | `KST SERVO` | `CHECKING GP29 FOR AN ESC` | BACK |
| An ESC answers | | the message from [pin-claim.md](pin-claim.md#when-the-pin-is-unavailable) | BACK |
| Releasing | `RELEASING GP29` | | BACK |
| Searching, no switch | `GP29  TRY 12  1.4 S` | `POWER UP` and `THE SERVO NOW` on two lines at scale 2, then `PLUG IT IN OR SWITCH ITS SUPPLY ON. IT ANSWERS ONLY RIGHT AFTER POWER-ON.` and `A POWERED SERVO ON THIS PIN GETS NO PULSES AND MAY GO TO ITS FAILSAFE.`, a spinner | STOP SEARCH, BACK |
| Edges, no valid reply | | adds, in amber, `REPLY NOT VALID: LATE` (the `kst_rx_t` code as a word) | as above |
| Searching, supply switched | see [screens with the switch](#screens-with-the-switch) | | |
| Timed out, no switch | `NO SERVO IN 30 S` | `CHECK SIGNAL AND GROUND, THEN POWER THE SERVO UP AGAIN.` | RETRY, BACK |
| Found | `IN PROGRAMMING MODE` | `READING 32 REGISTERS` | BACK |
| Read failed | | `READ FAILED: NO 3 EQUAL READS` and the registers in `bad_regs`, at most 9 on the line and `AND n MORE` below for the rest; `ABORTED. READ THE SERVO AGAIN.` after BACK; `THE SCREEN STALLED. READ THE SERVO AGAIN.` when the job refused the read; `THE READ WAS STOPPED. READ THE SERVO AGAIN.` for a read that ended before its abort was taken | READ AGAIN, BACK |
| Read | `V6  GP29  BACKUP KST004` | the field list | see below |

The layout of the searching state without a switch:

```
y   0 | KST SERVO                        [ BACK ]|  header 36 px (menu.md)
   40 | GP29  TRY 12  1.4 S                      |
   70 |                POWER UP                  |  scale 2, C_LIME, 94 px
   88 |             THE SERVO NOW                |  scale 2, C_LIME, 154 px
  112 |   PLUG IT IN OR SWITCH ITS SUPPLY ON.    |  scale 1, C_DIM
  126 |   IT ANSWERS ONLY RIGHT AFTER POWER-ON.  |
  144 |   A POWERED SERVO ON THIS PIN GETS NO    |
  158 |   PULSES AND MAY GO TO ITS FAILSAFE.     |
  202 |   REPLY NOT VALID: LATE                  |  C_AMBER, only with edges
  236 |              (spinner)                   |  AM32's twelve-dot spinner
  272 | [          STOP SEARCH           ]       |  46 px; RETRY after a stop
  320 +------------------------------------------+
```

`POWER UP THE SERVO NOW` on one line at scale 2 would be 262 px, wider than
the 240 px panel, so it takes two lines.

## Where the session runs

rcbench runs the session on its coprocessor, called every 1 ms or faster,
beside the PWM bank, and the panel reaches it over a link page. Here core1 is
free while DShot is released, so the session runs on core1 and core0 talks to
it through a command block and a status block, as the servo job does.

Why core1: `kst_session_step()` should be called every 1 ms or faster to keep
the pacing near its minimum, and the driver must hand over a capture within
`KST_DRIVER_SLACK_US`, 5 ms, of the window's end or the operation ends with
`KST_SES_ERR_DRIVER`. Core0 draws every 25 ms and can stall for tens of
milliseconds on an SD write. Core1 has nothing else to do.

`src/kst_job.h` as built in step 2; the lines marked step 5 come with the
switched supply.

```c
enum class KstOp : uint8_t { Search, StopSearch, ReadAll, Verify, Edit, Restore, Abort, Leave,
                             Reboot,                 // step 5: SERVO SUPPLY at GP0 only
                             CardBegin, CardEnd };   // step 5: the card phase
enum class KstJobState : uint8_t { Opening, Searching, SearchEnded, InMode, Busy,
                                   Rebooting,        // step 5
                                   Leaving, Closed };
enum class KstRefusal : uint8_t { None, State, NoUi };   // of the last command taken
enum class KstSupply : uint8_t { NotFitted, Off, On };   // step 5; NotFitted: SERVO SUPPLY at NONE

struct KstCmd {
    uint16_t        seq;
    KstOp           op;
    kst_image_t     target;            // Edit's target, Verify's expected image
    kst_field_set_t unlocked;          // Edit; always 0 in this slice
    bool            confirmUnchecked;  // Restore
};

struct KstStatus {
    uint16_t    ackSeq;
    KstJobState job;
    KstRefusal  refused;
    uint8_t     lineError;    // kst_line_err_t of the open
    KstSeek     seek;
    uint32_t    attempts, searchMs;
    uint8_t     seekRx;       // kst_rx_t of the search's last refused reply
    uint8_t     result;       // kst_ses_t of the last operation or search
    uint8_t     lastRx;       // kst_rx_t of the last transaction
    bool        inMode, hasImage, hasBackup, fingerprintOk, locked, planning;
    uint8_t     planResult, stepIndex;
    uint32_t    badRegs, diffRegs, wroteRegs;
    uint32_t    frames;       // frames sent by the running or last operation or search
    uint8_t     supply;       // step 5; KstSupply: the level the bench drives, servoSupplyIsOn()
    uint32_t    supplyMs;     // step 5; since the last switch command
    bool        stalled;      // a heartbeat lapse ended the last search
    uint8_t     cardPhase;    // step 5; CardBegin taken, not yet closed
    kst_image_t image, backup;
    kst_stats_t stats;
};
```

A command is taken in these states, and refused as `State` in the others;
with the heartbeat lapsed, Search and the four operations are refused as
`NoUi`:

| Command | Taken in |
|---|---|
| Search | Opening, SearchEnded (RETRY) |
| StopSearch | Searching |
| ReadAll, Verify, Edit, Restore | InMode; a refusal by the session itself is the result, and the job stays InMode |
| Abort | Searching, Busy |
| Leave | every state but Leaving and Closed |

The command block holds one command. A new one replaces a command core1 has
not taken, except that a StopSearch, Abort or Leave not yet taken stays and a
later command is dropped; a Leave replaces a held StopSearch or Abort.
Commands held at the bind are not taken. The release, at BACK, leaves as Leave
does from any state.

Every pass of `loop1()` the job checks the heartbeat, takes a new command,
steps the search or calls `kst_session_step()`, and publishes the status under
its lock when it changed. Each step is short: the session never waits, and a
planning step does one slice of `KST_PLAN_SLICE_UNITS`, 100 units, estimated
at 892 µs or less on a Cortex-M33 at 150 MHz (rcbench README, emulator figure,
not measured on hardware).

Memory and stack: the session is 856 bytes and lives in core1's static memory
with the `kst_line_t`. `kst_session_step()` took 528 bytes of stack in
rcbench's coprocessor image; core1's stack is `PICO_CORE1_STACK_SIZE`, 2 KB,
and the DShot pump does not run beside it. GCC's (the GNU Compiler
Collection's) `-fstack-usage` in the Release build gives the largest frames
on that path as `kst_reply_decode()` 272 bytes, `kst_session_step()` 264,
`kst_limits_edit()` 248 and the job's `publish()` 152, each a single
function's frame; the deepest chain is not measured. The job never calls
`kst_session_write()`, `kst_plan_edit()` or `kst_plan_restore()`: they run the
planner to its end in one call and their stack is not measured. Edits go
through `kst_session_edit()` and restores through `kst_session_restore()`,
which plan in slices.

Core0 owns the files: `kst_file.c` turns images into text and back on core0,
and the card is only ever touched from core0, as `sd_log.cpp` already does.

### Opening and leaving the line

| rcbench port step (`kst_port.h`) | Here |
|---|---|
| STOPPING, 50 ms with no pulse | the claim's drain: DShot stops, then silence |
| LOW, 100 ms at least, and the supply set | `kst_line_open()`, which drives low. With a switched supply, `servoSupplyBind()` drives GP0 low at the job's bind, and the search switches the supply off for 2000 ms and on before each attempt |
| PROGRAMMING, the entry holds low 100 ms more | the search |
| LEAVE: low for 100 ms by default, then PWM | without a switched supply: low for 100 ms, `kst_line_close()` (input, pull-down on), then the unplug page and DShot. With one: supply off and the line low for 2000 ms, `kst_line_close()`, `servoSupplyRelease()`, then the unplug page and DShot |
| POWER_CYCLED, REBOOT | the loss rule above. REBOOT with a switched supply only, on the leave page; not rcbench's REBOOT, which counts from its PD mini's read-back and ends in PWM: here the times count from the switch commands, and an entry and a read follow |

The servo stays in programming mode until its supply is off
(`kst_session.h`: "The mode lasts until the servo's supply is removed").
Whether it follows PWM before that is not measured. So, without a switched
supply, leaving tells the user to power it off; with one, the bench switches
the supply off itself. The shared unplug page follows before DShot restarts
on the pin.

## Screens

Entry: MENU → KST SERVO, one line in `ui_registry.cpp`
([menu.md](menu.md#code)); BACK returns to MENU through the unplug page. The
AM32 screen is the model: a state machine that touches nothing until the
handover completes, a list with an editor bar, and a write that needs a
1000 ms hold. The lists are on `ui_list` and the holds on `ui_hold`, and the
drawings below use menu.md's 36 px header: the strip at y 40, the body from
y 62. List rows are 40 px tall at a 42 px pitch, the floor of menu.md
decision 5.

### Field list

```
y   0 | KST SERVO                        [ BACK ]|
   40 | V6  GP29  BACKUP KST004            (MODE)|  strip, chip C_GREEN
   62 | TUNE      MAX. DUTY                 96 % |  40 px rows, 42 px pitch,
  104 | TUNE      BOOST                300.0 US  |  viewport 62..215,
  146 | TUNE      DEAD BAND              2.0 US  |  on ui_list
  188 | PULSE     NEUTRAL               1500.2 US|  cut: 28 px, takes no press
  218 | [ - ]     DEAD BAND 2.0 US  ->      [ + ]|  editor bar 44 px
  268 | [ HOLD TO WRITE ] [REVERT] [ FILES ]     |  110, 52, 54 px; 46 px tall
  320 +------------------------------------------+
```

- The list scrolls in pixels with menu.md's gesture: a tap selects, a
  vertical drag scrolls, and a sideways swipe steps the row it began on by
  one coarse step per 16 px. A coarse step is an eleventh of the field's
  raw range, rounded up, at least 1 (`kstCoarse()`): 24 raw for MAX. DUTY.
  Three whole rows are visible. LOCKED rows are drawn in `C_GRID` and are
  not selectable.
- Groups follow rcbench's pages: TUNE (MAX. DUTY, BOOST, STRETCH, SOFT START,
  SPEED LIMIT, SPEED, DEAD BAND), PULSE (PULSE LOWER, PULSE UPPER, NEUTRAL,
  REVERSION, LEFT ANGLE, RIGHT ANGLE), FAILSAFE (ALLOW UNCONT, UNCONTROLLED
  POS, UNCONTROLLED TIME), then LOCKED: every field whose access class is
  `KST_ACCESS_LOCKED` or `KST_ACCESS_NEVER`, read-only.
- Values come from `kst_field_display()` with their unit. The screens write
  `US` for µs; the font has no micro sign, as in rcbench.
- − and + step the raw value with `kst_field_edit()`, hold to repeat, in
  the direction of the value shown: + raises it whichever way the raw value
  goes, so + widens DEAD BAND, 0.2 µs × (65 − raw) (`kstStep()`). A value
  that breaks a hard rule (`kst_limits_edit()` against the backup) is skipped,
  so the stepper reaches legal values only. A value that breaks a soft rule is
  taken; the row gets an amber mark and the editor bar shows the rule's text,
  for example `WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.`
- An image that fails the layout fingerprint is read and shown. With no row
  selected the editor bar reads `READ ONLY: UNKNOWN LAYOUT` and
  `NOTHING IS WRITTEN TO THIS SERVO`, and the footer's write button reads
  `READ ONLY`; the session refuses its writes.
  A locked session's write button reads `IN DOUBT`.
- The strip names the type, `V6` or `V8` (`kst_type()`), the pin and the
  backup file once the first write has saved it, `BACKUP IN MEMORY` before.
- A value whose raw step does not change its text, MAX. DUTY's 245 and 244
  both reading `96 %`, carries its raw value in the editor bar and on the
  write page: `96 % (245) -> 96 % (244)` (`kstValuePair()`).

### Write

HOLD TO WRITE opens the write page rather than writing at once.

```
y   0 | KST WRITE                        [ BACK ]|
   40 | 2 FIELDS                                 |
   62 | DEAD BAND        2.0 US  ->  3.0 US      |
   82 | SOFT START            1  ->  0           |
  102 | * WITHOUT SOFT START THE SERVO JUMPS     |  C_AMBER, 12 px a line,
  114 |   AT POWER-ON.                           |  6 lines, none below 192
  196 | WRITING: DO NOT UNPLUG. / the result     |  3 lines at most
  236 | BACKUP FIRST: KST/KST005.TXT             |  or NO CARD: BACKUP IN MEMORY
  268 | [          HOLD TO WRITE           ]     |  1000 ms, progress bar
  320 +------------------------------------------+
```

The hold is the confirmation of every soft rule shown on the page. While the
write runs the page shows `WRITING: DO NOT UNPLUG.`, then
`PLANNING THE ORDER OF THE WRITES` while `planning` is set, then
`FRAMES SENT: 94`. The result is one text per `kst_ses_t`, from a table in
`kst_view.cpp` that the tests hold complete, with OK to return to the list:
`WRITE VERIFIED`, `A WRITE DID NOT TAKE. NOTHING WAS WRITTEN.`, `ROLLED
BACK: THE SERVO HOLDS ITS OLD VALUES`, `IN DOUBT: THE SERVO DOES NOT HOLD
THE VALUES. RESTORE FROM FILES.`, and the others. A result in doubt is red,
a write that left nothing changed amber, a verified one green.

- The page lists the fields from where the servo stood when the page
  opened, so the result still names what was written.
- OK takes a press that lands while it shows. The finger that ends the
  hold lifts on the same rectangle and does not dismiss the result.
- When the card takes neither the backup nor the record, the page reads
  `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.` and no frame
  goes out. No card, or a card held by USB, writes with the backup in
  memory.

### Files

```
y   0 | KST FILES                        [ BACK ]|
   40 | CARD  12 FILES                           |
   62 | RECORD (KST.TXT)                         |  40 px rows, 42 px pitch,
  104 | KST017                        (BACKUP)   |  viewport 62..229, 168 px,
  146 | KST011                                   |  on ui_list
  188 | KST010                                   |
  234 | [          SAVE AS KST013          ]     |  228 x 40 px
  278 | [  LOAD  ]       [ HOLD TO RESTORE ]     |  40 px tall
  320 +------------------------------------------+
```

- The list scrolls instead of paging (menu.md decision 9): four whole rows
  in the viewport, a drag scrolls, a tap selects. Twelve files are 504 px of
  content, a scroll range of 0 px to 336 px. There is no sideways mode.
- The order: a record on the card, `KST/KST.TXT`, first; then the KST
  backups of `BACKUP/`, the latest first (`kstFilesBackups()`), each tagged
  `(BACKUP)`; then the files of `KST/` in `kst_store_list()`'s order, by name
  without case, the KSTnnn files in one run by falling number. The page reads
  the first 64 files of `KST/` (`KST_UI_FILES_MAX`); the strip then reads
  `CARD  FIRST 64 OF n FILES`. The record and the backups load as files.

- SAVE AS writes the servo as it stands into `KST/` under the next free name
  from `kst_store_preset()`, one above the highest KSTnnn, through
  `kstFilesSaveAs()`, and never over a file. Once `KST/` holds 50 files the
  button does not act and the strip reads `KST/ HOLDS 50 FILES. REMOVE SOME.`
  ([storage](#storage)). There is no keyboard, so there is no other name; a
  file renamed on a computer still lists and loads.
- LOAD parses the selected file (`kst_file_parse()`), holds it against the
  servo (`kst_file_load()`: type, access classes, hard limits) and puts the
  result into the list as pending edits. A refusal names its code and line,
  for example `LINE 14: NOT A VALUE OF DEAD BAND`. The write then goes through
  HOLD TO WRITE as any edit.
- HOLD TO RESTORE runs `kst_session_restore()` to the first image read this
  session, through the write page. A plan the planner marks unchecked asks
  for a second hold there.
- With no card mounted, or the card held by a USB host, the page shows
  `NO CARD` or `CARD IN USE BY USB` and RETRY MOUNT (`sdLogRemount()`);
  reading, editing and writing still work.

### Leave

BACK from the list, or from the search after a servo was found, opens the
leave page. Without a switched supply:

```
  POWER THE SERVO OFF.
  IT STAYS IN PROGRAMMING MODE UNTIL ITS SUPPLY IS OFF.
  2 CHANGES NOT WRITTEN.                       (only with pending edits)
  THE SERVO IS IN DOUBT: RESTORE FIRST.        (only while locked)
  [ LEAVE ]
```

LEAVE is a tap. Without a switched supply it leaves even a locked session:
there is nothing else the screen can do, and the record file below keeps what
the servo was. Then the unplug page from [pin-claim.md](pin-claim.md), then
DShot. BACK during a write is queued: the page shows
`LEAVING AFTER THE WRITE ENDS.` and the leave follows the result. BACK during
a search, a read or a verify aborts it.

With a switched supply:

```
y   0 | KST SERVO                        [ BACK ]|
   40 | V6  GP29  SUPPLY ON                      |  strip
   66 | LEAVE SWITCHES THE SERVO'S SUPPLY OFF.   |  scale 1, centred, 227 px
   86 | 2 CHANGES NOT WRITTEN.                   |  only with pending edits
  100 | THE SERVO IS IN DOUBT: RESTORE FIRST.    |  only while locked, C_AMBER
  136 | REBOOT: SUPPLY OFF FOR 2 S, ON,          |  scale 1, C_DIM
  150 | THEN A NEW ENTRY AND READ. THE READ      |
  164 | BECOMES THE BACKUP.                      |
  182 | NO CARD: REBOOT DROPS THE BACKUP IN      |  C_AMBER, no card only
  196 | MEMORY.                                  |
  214 | [          HOLD TO REBOOT           ]    |  228 x 46 px, 1000 ms hold
  268 | [              LEAVE                ]    |  228 x 46 px, a tap
  320 +------------------------------------------+
```

- LEAVE is a tap. The supply goes off, the page's text gives way to
  `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`, and after 2000 ms the unplug page
  follows ([leave](#leave-reboot-loss-and-heartbeat-with-the-switch)). A
  locked session leaves the same way. HOLD TO REBOOT is hidden during the
  wait, and the job refuses `Reboot` while Leaving.
- HOLD TO REBOOT runs REBOOT after a 1000 ms hold on `ui_hold`, as HOLD TO
  WRITE does: progress across the button, and a slide off cancels. It is
  drawn disabled and takes no press while an operation runs or plans.
- The backup line: without a card, the amber line above. With a card, after
  the session's first write, `THE BACKUP STAYS ON THE CARD: KST005.` With a
  card and no write, none: the servo still holds the backup's values.
- BACK on the page returns to the field list, or to the search after a
  lost servo. The leave page is exempt from the loss rule, so BACK from
  the search after a loss stays on it; that BACK also stops the search.

REBOOT is on this page and not on the field list, whose footer is full
(110, 52 and 54 px). The leave page carries the locked session's warning
(decision 13).

## Storage

SAVE AS files and the record of a write under way live in the `KST`
directory of the SD card, `KST_STORE_DIR`; the session's backups live in the
ring of `BACKUP/` that every device shares ([backups.md](backups.md)); the
blackbox logs live in `LOGS/`. A file is `<name>.TXT` in rcbench's text
format: 35 lines, under 1024 bytes, CRC-32 (cyclic redundancy check) checked,
editable by hand on a computer once its checksum line is removed. FatFs here
is built with long file names (`FF_USE_LFN` 3, `FF_MAX_LFN` 255, in the
library's `ffconf.h`), which covers `KST_STORE_BASE_MAX`, 32 characters.

`kst_store.c` reaches the card through 5 functions (`kst_store_io_t`:
`mounted`, `walk`, `size`, `read`, `create`). `src/kst_files.cpp` implements
them over FatFs on core0. The card counts as mounted while the blackbox
logger has it mounted (`SdLogState` Idle or Logging) and the USB card reader
is off (`MscState::Idle`): while `mscHoldsCard()` is true, and while the
reader takes the card back, every call answers "not mounted" and touches no
FatFs function. Every card call runs in a `CardDirScope` (`src/card_dir.h`),
which makes its directory FatFs's current one and the root again at its end,
so the paths inside it name the file alone; `KstFilesScope` is that scope for
`KST`.

**SAVE AS** takes the name `kst_store_preset()` proposes, one above the
highest KSTnnn of `KST/`, through `kstFilesSaveAs()`, and its files are never
written over. It refuses once `KST/` holds `KST_FILES_SAVE_MAX`, 50, files the
store lists, the record not among them: the FILES strip reads
`KST/ HOLDS 50 FILES. REMOVE SOME.` and the button does not act. That bounds
the directory a SAVE AS walks ([the cap](#card-calls-and-the-heartbeat)).

**The backups.** The session's backup and a record moved out of the way take
numbers in the ring of `BACKUP/`, `KSTnnn.TXT`, as the ring's rules of
[backups.md](backups.md) and `src/backup_ring.h` set them: one sequence of 50
numbers for every device, `BACKUP/LAST` naming the latest, the oldest written
over in place. FILES lists the KST backups of `BACKUP/` first, the latest
first, each with `(BACKUP)`, then the files of `KST/`; a backup loads as a
file.

FatFs results map to the store's:

- `FR_DENIED` on a create is CARD_FULL for a full volume or directory, and
  DENIED when a directory or a read-only file has the name; a short write is
  CARD_FULL.
- A file named `KST` or `BACKUP` where the directory goes is DENIED.
- `FR_WRITE_PROTECTED` is DENIED. FatFs reports it only when the disk layer
  sets STA_PROTECT, which this board's card driver never does: a
  write-protected card fails as `FR_DISK_ERR`, a write error.
- Any other error is a write or read error.

A create that fails once removes the file it made, and the `KST` or `BACKUP`
directory when it made that too. Current limitations, found by running FatFs R0.15 on
a host:

- A write error that persists, as a write-protected or failing card gives:
  FatFs keeps the sector it could not write in its window, the remove fails
  with `FR_DISK_ERR`, and every later call that moves the window fails the
  same way until the card is mounted again: SD LOG's RETRY, the USB card
  reader's hand-back or a power cycle. The card still counts as mounted, so
  the KST screen's RETRY MOUNT, offered without a card, is not one of them.
  The list, LOAD and the record lookup fail until then. The new file's 0-byte
  entry stays in that sector and reaches the card if writes work again before
  a remount.
- A rename that stops between registering the new name and removing the old
  one, a write that fails between the two, leaves both names on the card:
  two entries that share one cluster chain. The first write reports the
  record as not kept and writes no record. Removing one of the two on a
  computer frees the other's data, and later sessions keep the second name
  as one more KSTnnn.TXT.
- A create whose close fails with a clean window, a data sector that does
  not take its write: FatFs keeps the file open, refuses to remove it or to
  open it for reading (`FR_LOCKED`), and holds one of its 16 lock entries
  (`FF_FS_LOCK`) until the card is mounted again.

| Situation | Effect |
|---|---|
| First write of a session, card mounted | the backup is saved first into the ring of `BACKUP/`, and `KST/KST.TXT` holds the image before the write, both in a card phase ([card calls](#card-calls-and-the-heartbeat)). A card that takes neither stops the write: `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.` |
| Write verified | `KST/KST.TXT` is removed when this session wrote it, the card stayed mounted since, and the file holds the bytes written (FNV-1a, Fowler-Noll-Vo, hash `recordSum`), in a card phase |
| Later writes of the session | no backup is saved and no record is written |
| LEAVE, the servo in programming mode and not in doubt | `KST/KST.TXT` is removed on the same terms: no write is open |
| A card swapped during the session | the board has no card-detect line: the first card access after the swap fails, and FatFs mounts the new card at the call after it. The logger reports no unmount. Its record stays unless it holds the bytes of the session's own record; the strip, the backup line and the `(BACKUP)` tag still name the session's backup file on the first card |
| A servo found, the first or after a loss | the session holds no record of its own: a record on the card stays until the first write renames it |
| `KST/KST.TXT` found when the screen opens | `A WRITE WAS INTERRUPTED. KST/KST.TXT HOLDS THE SERVO AS IT WAS.` FILES lists it as RECORD and loads it like a file |
| No card | read, edit and write work; the write page says `NO CARD: BACKUP IN MEMORY`, and the backup ends at LEAVE |
| Card held by a USB host | as no card |

`KST.TXT` is the base name `kst_store.c` keeps free for this record
(`KST_STORE_RECORD`); `kst_store` refuses it for a save and a load, so
`kst_files.cpp` reads, writes and removes it itself.

The first write's card calls, `kstFilesFirstWrite()`, in order:

1. One walk of `BACKUP` and a read of `BACKUP/LAST` name both files: a record
   to keep takes the ring's next number and the backup the one after it, or
   the backup the next one alone. `BACKUP/LAST` is written once, with the
   backup's number.
2. A record already on the card, left by an interrupted write, is moved into
   `BACKUP/` under its number, after the files there are removed, so no
   record is lost. A record that cannot be renamed
   stays: no new record is written over it, and the verified write does not
   remove it. A rename that FatFs reports as failed after it took effect, its
   last sync failing with the change still in FatFs's window, counts as kept:
   the record is looked up under both names. A card that does not answer
   whether a record is there gets no record either.
3. The backup is saved under its number, over the file there or as a new
   file.
4. The image before the write is written as `KST/KST.TXT`.

The write runs with no card, or when the card took the backup or the record
(`kstFilesWriteAllowed()`). rcbench's full journal
(`kst_prog.c`, the port and the step of a write under way) is not ported; the
record is the image before the write and nothing else.

## Porting

| rcbench file | Lines | Here | How |
|---|---|---|---|
| `protocols/kst/kst_wire.c`, `kst_reg.c`, `kst_limits.c`, `kst_plan.c`, `kst_session.c`, `kst_pio.c`, `kst_file.c` and the 7 headers in `include/` | 5,598 | `lib/rcbench/protocols/kst/` | copied as they are |
| `protocols/kst/rp2350/kst_line.c`, `.h`, `.pio` | 461 | `lib/rcbench/protocols/kst/rp2350/` | copied as they are; `pico_generate_pio_header()` assembles the `.pio` |
| `shared/logfile/kst_store.c`, `include/kst_store.h` | 404 + header | `lib/rcbench/shared/logfile/` | copied as they are |
| `shared/bench/kst_card.c` | 232 | `src/kst_files.cpp` | adapted: the save, list and load glue between `kst_file`, `kst_store` and the card, without the `kst_prog.h` request and answer types; FatFs `kst_store_io_t` written new |
| `shared/ui/kst_panel.c` | 3,134 | `src/ui_kst.cpp` | rewritten for 240 × 320 with the AM32 list idiom and `ui_widgets.h`; the page grouping, the soft-rule texts and the result texts are taken |
| `shared/bench/kst_prog.c` | 3,078 | `src/ui_kst.cpp`, `src/kst_files.cpp` | not ported; the draft and the message table are rewritten small, the journal becomes the record file |
| `shared/outputs/kst_port.c`, `prog_port.c` | 1,616 | `src/kst_job.cpp`, `src/kst_seek.cpp` | not ported: they are the coprocessor's link page, the PD mini supply sequence and the port bank. The rules MiniRcBench keeps are in the job and the wrapper: out of PWM, low, entry, in the mode until power-off, and leave with 100 ms low without a switched supply. With one, MiniRcBench's own switch sequence takes rcbench's supply rules: 2000 ms off, 1000 ms on for REBOOT, on only while the heartbeat is trusted, and off whenever nobody is there to switch it off |

The vendored tree mirrors rcbench's paths, so a diff against a later rcbench
commit is a directory diff. `lib/rcbench/README.md` lists every file, its
rcbench path and the commit. The core builds as C11 with its own
`CMakeLists.txt` unused; `src/kst.cmake` names the sources.

### Licence headers

Copied files keep their text whole: the `SPDX-License-Identifier: MIT` line,
and the trademark sentence they carry ("KST and Chaservo are names or
trademarks of their respective owners..."). Adapted and new files that hold
KST code start with `Adapted from rcbench <path>, commit 453cf334, MIT.` where
they are adapted, and the same trademark sentence. Both projects are MIT under
one holder, so the licence does not change. The copied headers point at a
README section "Legal notice and disclaimer"; MiniRcBench's README gains that
section in this slice, with the trademark notice, no affiliation, and the
statement that the protocol comes from wire captures and no vendor code or
documentation.

## Testing

**Ported**, copied as they are into `test/rcbench/` with rcbench's harness
`greatest.h` (its own MIT file, 127 lines, no framework), the servo and
line model `kst_sim.h` and `fixtures/kst_line_words.txt`,
built as C11 and run by `make` and CI:

| File | Covers |
|---|---|
| `test_kst_wire.c` | frames, the decoder, the 13 reply codes |
| `test_kst_reg.c` | the register model, conversions, fingerprint |
| `test_kst_plan.c` | write plans, slices, the work cap |
| `test_kst_session.c` | entry, reads, writes with read-back and roll-back, verify, abort, power cycle |
| `test_kst_pio.c` | the PIO counts, and `kst_line.pio` assembled and run in a model, held to the fixture |
| `test_kst_file.c` | the file format, the 15 refusals, loading |
| `test_kst_store.c` | names, presets, save, list, load against a fake card |

Not ported: `test_kst_card.c`, `test_kst_link.c`, `test_kst_panel.c`,
`test_kst_port.c`, `test_kst_prog.c`. Their code is not ported.

**New**, in MiniRcBench's `check.h` harness. `kst_sim.h` compiles as C++17,
so these run against the same simulated servo:

- `test_kst_seek.cpp`: a servo powered at 0 s, 3.0 s and 29.9 s is found on
  the next attempt; attempt length 116.6 ms to 118.8 ms; no frame closer than
  3 ms to the last window (the simulator counts it); timeout at 30 s; STOP
  SEARCH mid-frame lets the frame finish; a heartbeat lapse lets the attempt
  end and starts none; a loss calls `kst_session_power_cycled()`; a reply with
  a bad rate is shown and is not `Found`.
- `test_kst_job.cpp`: the command and status blocks; each `KstOp` in each
  state; leave with 100 ms low before the close without a switched supply; the
  supply rules in [tests for the switch](#tests-for-the-switch), in step 5;
  the job never calls `kst_session_write()`, `kst_plan_edit()` or
  `kst_plan_restore()`: the test includes `kst_job.cpp` with the three renamed
  to counters, which stay at 0. The job runs from the job table, one
  `core1JobPoll()` a millisecond, on `test/fake_kst_line.cpp`: the simulator
  behind `src/kst_line_port.h`, on the suite's virtual clock.
- `test_kst_files.cpp`, built in step 3: the FatFs io over an in-memory volume
  (`test/fake_fatfs.cpp`): the mount rule, no card and a card held by USB with
  no FatFs call, the first write's backup and record, an old record kept and
  never removed when it stays, a rename reported failed that took effect, a
  rename that left both names, a failed lookup after a rename, a full
  directory that refuses a rename, card full, a write-protected card as this
  board reports it (a disk error) and as `FR_WRITE_PROTECTED`, a full
  directory, a file or a directory in a file's way, a read-only record, a
  one-time write or sync error with no file or new directory left behind, a
  close error that leaves its file locked, one of the two files taken, the
  record found and loaded at entry, the store's save, list and load over
  FatFs. With the screen, in `test_kst_ui.cpp`: the record written before
  the first write frame and removed after a verify. With `GP0`, step 5: a
  card phase around
  every card call, the first write's opened before the backup and closed by
  the write command, the others closed by `CardEnd`; `sdh_wait_hook` set only
  inside a phase.
- `test_sdio.cpp`: `sdh_wait_hook` called on every poll of `busyWait()`, and
  never while null.
- `test_kst_view.cpp`, built in step 4: the 27 rows in their groups, each
  value with its unit on the bench image, the stepper against the hard rules
  (MAX. DUTY stops at 98 %, raw 250), the coarse step, a text for every rule,
  every `kst_ses_t` and every file error, the line breaks.
- `test_kst_ui.cpp`, built in step 4, by control id
  ([menu.md](menu.md#tests-by-control-id)): MENU opens the screen, the claim,
  the search, STOP SEARCH and RETRY, the read, the field list, an edit,
  REVERT, HOLD TO WRITE not at 750 ms and not on a slide off, the write
  page, the backup and the record before the first write frame, the verify,
  a hard-limit value written as its limit, FILES with SAVE AS, LOAD and a
  refused file, a write left in doubt and HOLD TO RESTORE, BACK in every
  view and during a write, LEAVE and RESTART DSHOT, a lost servo, no card,
  a card held by USB, a card that takes no file, an image that fails the
  fingerprint, a record found at entry; `checkControlTable()` in each view.
  `python3 tools/make_previews.py` adds `docs/kst-preview.png`.

## Work breakdown

1. Vendor the core, the line driver and the store; `test/rcbench/` with the
   ported tests in `make` and CI. Built.
2. `kst_seek.*` and `kst_job.*` with their tests. Built, without the
   switched supply, which is step 5.
3. `kst_files.cpp` and its tests. Built, without the card phase, which is
   step 5.
4. `ui_kst.cpp`: search, field list and FILES on `ui_list`, write, leave,
   the holds on `ui_hold`; its `UiScreen` and its line in `ui_registry.cpp`,
   so MENU shows KST SERVO ([menu.md](menu.md#code)); UI tests by control id;
   preview. Built, without the switched supply, which is step 5.
5. The switched supply, its settings field and its SETTINGS row:
   - `servo_supply.*`, and `servoSupplyInit()` in `main.cpp`'s `setup()`; the
     switched search, REBOOT, the close and the supply rules in `kst_seek.*`
     and `kst_job.*`, `Reboot`, `CardBegin` and `CardEnd` in `KstOp` and the
     supply fields in `KstStatus`; the card phase around every card call,
     `kstCardStamp()` and `kstCardStampFresh()`, `KST_CARD_PHASE_MAX_MS`;
     `sdh_wait_hook` in `src/sdio/sd_host.h`, called from the waits of
     `sd_host.c` and `sd_pio.c`, and in the layers list of
     [sdio-driver.md](sdio-driver.md#layers) (`sdio-driver.md:309-313`);
     `test/kst_supply_sim.h` and the tests in [tests for the
     switch](#tests-for-the-switch).
   - `settings.h` and `settings.cpp`: the `servoSupply` field at offset 12
     (payload 14 bytes, block 28 bytes on the host compiler's layout),
     `SETTINGS_VERSION` 3, and a `settingsValidate()` clamp: any value but 0
     (`NONE`) or 1 (`GP0`) becomes 0. The comments that say a DshotDisplay
     block loads unchanged go: on `SETTINGS_VERSION`, on `boardId`, and at
     the board check in `settingsValidate()`.
   - `test_settings.cpp`: `testDshotDisplayLayout` becomes the version 3
     layout test, and a DshotDisplay 2.x block is discarded whole and loads
     the compiled defaults.
   - The SERVO SUPPLY section: one `SettingsRow` in KST's header, named by
     KST's line in `ui_registry.cpp` ([SETTINGS row](#settings-row));
     `test_ui.cpp`: the row by control id. The scrolling list, its asserts
     and its gesture tests are built (menu.md slice C).
   - `docs/settings-preview.png`, the SETTINGS preview of menu.md slice C,
     regenerated with `make previews`; the SETTINGS sections of the wiki
     pages `The-Screens.md` and `Die-Bildschirme.md`, and their images,
     which slice C named.
   - README.md: the SERVO SUPPLY row in the SETTINGS row table of its
     SETTINGS screen section, and the servo supply in the SETTINGS row of the
     MENU table.
   - `STATUS.md`: the settings feature row and the `runSettingsTests` row,
     which name the block layout DshotDisplay 2.x writes.
   - `CHANGELOG.md` and the README: the first boot after the update discards
     the stored block and starts from the compiled defaults, every setting
     and the ESC pin included. DshotDisplay, flashed back, discards a
     version 3 block the same way.
6. README: the KST section, the wiring, the legal notice; the converter,
   its requirements and the warning on an ESC's BEC wire; GP0 in the pin
   table as the supply enable with `SERVO SUPPLY` at `GP0`; the KST SERVO
   row in the MENU table of README.md, which has no KST row until then. Wiki
   pages `KST-Programming.md` and its German counterpart, and their entries
   in `_Sidebar.md` and the screens pages.
7. Bring-up, in rcbench's order and before any write is relied on:
   - the converter, with the user's supply on: with no servo, EN low and
     VOUT at 0 V with the board unpowered, held in reset, in BOOTSEL, and on
     the main screen with `SERVO SUPPLY` at `GP0`; the
     start-up time from EN high to VOUT in range, on a scope; with the
     servo as the load, VOUT at the end of the 2000 ms off time. The level
     at which a KST servo resets is not known, so the record is the voltage
     reached, not a pass or fail; VOUT with EN low at the lowest and the
     highest VIN used, for a converter whose output disconnect its maker
     does not state;
   - the reply level at the connector;
   - the power-on window of the owner's servo: without a switch, the
     shortest delay from power-on to a sync that still enters; with the
     switch, by stepping `KST_SUPPLY_ENTRY_DELAY_MS`
     ([timing](#timing-of-the-switch-on)); and that the owner's HV servo
     enters programming mode at 5 V (decision 16);
   - frames and replies on a scope; read reliability; one write with
     read-back, then a power cycle and a read.

About 10 to 12 days for 1 to 6, not measured. The scrolling SETTINGS list
and its coordinate tests are built in menu.md slice C, and the lists and holds are
the shared widgets of menu.md slice A; the card phase and the close rule are
added here. Bring-up depends on the servo and is not estimated.

## Decisions

Decisions of the owner, 2026-10-17: recommendations 5 and 6 are accepted.
Decisions of the owner, 2026-10-18: questions 1 to 4, recorded here for
slice 3, which implements them.

1. **Power-on window shorter than an attempt.** Keep rcbench's sequence
   unchanged until the window is measured, which is bring-up step 7. If it
   proves shorter than about 120 ms, the wrapper starts attempts closer
   together; the 100 ms low is part of the measured entry and stays in the
   core. As recommended.
2. **Search timeout.** 30 s, then RETRY; not the 60 s recommended. The search
   does no harm to a servo, but a screen that holds a pin low for ever without
   saying so is one the user forgets.
3. **Pull-down.** The internal one only, and a 10 kΩ pull-down in a
   servo-only lead if bring-up shows reply edges lost at the window start.
   Never on the board pin, where it would break bidirectional DShot. As
   recommended.
4. **Reply level.** The owner states that the servo replies at 3.3 V, so no
   level shifter is needed for it. The rule stays: no level above 3.6 V on
   SIG, so a servo that replies higher needs a level shifter in its lead.

5. **KST on AUX.** Programming runs on SIG only. It is the wire a servo or
   ESC is plugged into, and one pin keeps one rule for the ESC interlock.
6. **Unlocking locked fields.** Not in this slice. rcbench can lift
   `KST_ACCESS_LOCKED` for a field with a warning held 2 s; the locked fields
   are the ones whose function is not measured.

Decisions of the owner, 2026-10-18, on the switched supply. Decision 8 is
the owner's revision of the same day, after review round 1 of PR #6, and
decisions 11 and 12 come from that revision:

7. **Enable pin.** GP0, J5 pin 2.
8. **Configuration.** A SETUP row, `SERVO SUPPLY`, with two values: `NONE`, the
   default, for no switch fitted, and `GP0`, an active-high enable on GP0.
   There is no active-low form, and GP1 is not offered. An external pull-down
   on EN, 10 kΩ to 100 kΩ, holds the converter off while the board is
   unpowered, from reset and in BOOTSEL; for an EN with an internal pull-up the
   divider sets the value ([example modules](#example-modules)). The row adds a
   field to the settings block, so `SETTINGS_VERSION` goes from 2 to 3. A block
   of another version is discarded whole (`settingsLoad()`), so the first boot
   after the update starts from the compiled defaults once, every setting and
   the ESC pin included. A block DshotDisplay saved does not load, and
   DshotDisplay discards a version 3 block.
9. **Timing.** rcbench's values: 2000 ms off before each search attempt and
   for REBOOT, 1000 ms start for REBOOT, and the entry directly after the
   switch-on in the search, `KST_SUPPLY_ENTRY_DELAY_MS` 0 ms, until
   bring-up measures the servo.
10. **Servo tester.** It uses the switched supply in a change after KST
    programming: SAFE is supply off, HOLD TO START switches it on, STOP and
    a heartbeat trip switch it off. Until that change the servo tester never
    drives GP0, so with a converter fitted the servo there is unpowered
    unless it has a supply of its own
    ([servo-tester.md](servo-tester.md#decisions), decision 20).
11. **SETUP scrolls.** Superseded by [menu.md](menu.md#decisions)
    decision 13. SETUP becomes a list scrolled in pixels, as the AM32 list,
    and `SERVO SUPPLY` is its eighth row, after KISS PIN. HOLD TO
    SAVE, RESET, LINK and the note line with `UNSAVED` stay fixed below the
    list.
12. **Larger SETUP rows.** Superseded by [menu.md](menu.md#decisions)
    decision 5. Scrolling removes the height limit, so the rows grow: 44 px
    rows at a 50 px pitch, − and + 44 × 44 px, toggles 94 × 44 px, values and
    toggle labels at scale 2 and labels at scale 1; four rows visible in a
    200 px viewport, a scroll range of 0 px to 200 px; the footer buttons
    48 px tall. [menu.md](menu.md#settings) has the layout.

Choices made in this design where review round 1 of PR #6 left the choice
open; the owner has not decided them:

13. **REBOOT on the leave page,** behind a 1000 ms hold. The field-list footer
    has no free slot (110, 52 and 54 px). The leave page is where the session's
    end is decided, it shows a locked session's warning, and a hold there is
    apart from LEAVE's tap. The hold confirms the loss of an in-memory backup
    without a card.
14. **No locked-session exception on a heartbeat lapse.** The supply goes
    off after the running operation ends, locked session or not, as
    rcbench's rule that the supply ends off whenever nobody is there to
    switch it off. A stalled UI cannot show the servo, take a restore or
    switch the supply off. Keeping the supply on would not save the restore,
    which needs the UI, and would hold a servo in programming mode at an
    unchecked image for as long as the stall lasts. The cost: the wrapper
    calls `kst_session_power_cycled()`, which drops the backup in memory.
    With a card the backup file and `KST/KST.TXT` keep it; without one it is
    lost, and the stalled screen says so. A card write that stalls core0
    before the first write does not count as a lapse (decision 17).
15. **The SETUP scroll gesture.** Superseded by
    [menu.md](menu.md#decisions) decision 14. A drag scrolls only when it
    starts off the controls, and a press on a control never scrolls. The
    steppers keep their first step on touch-down, which they would lose if a
    drag could start on them.

Decision of the owner, 2026-10-18, on the supply voltage:

16. **5 V programming supply.** Every servo the owner programs, HV servos
    included, takes programming at 5 V, so the switched supply is 5 V
    nominal. That is the owner's statement, not a measurement; the 5 V
    programming level of an HV servo is a bring-up check (step 7). The
    servo tester's wiring range in [pin-claim.md](pin-claim.md#wiring),
    4.8 V to 6.0 V and 8.4 V for an HV servo, is unchanged.

Choices made in this design where review round 2 of PR #6 left the choice
open; the owner has not decided them:

17. **Card calls keep the heartbeat, for at most 15 s.** In a card phase the
    SDIO driver's waits call a generic hook that stamps a card heartbeat the
    KST job counts as the UI's ([card calls](#card-calls-and-the-heartbeat)).
    Ordering alone, the card write before the session's write, does not
    help: the job idles in the mode with the supply on while core0 writes,
    and the lapse rule would switch it off. A lease of fixed length alone
    would keep the supply on through any core0 hang for the whole lease. The
    stamp from inside the driver's waits keeps the supply on only while
    core0 waits on the card, and `KST_CARD_PHASE_MAX_MS`, 15 s, caps it. With
    no log in the root, the longest phase counted at the SD specification's
    times, the first write that keeps an old record, takes 12.8 s on FAT32
    and 16.3 s on exFAT ([the cap](#card-calls-and-the-heartbeat)); the
    driver's own limits, 765 ms a read and 3033 ms a write after 3 tries,
    allow about 109 s for that write on exFAT, and a core0 stuck in an outer
    loop would stamp for ever.
18. **Every close waits 2000 ms off.** BACK during REBOOT, on the stalled
    screen and during a switched search closes the line only 2000 ms after
    the switch-off command, as LEAVE does ([close](#close-with-the-switch)).
    Of the two choices, going to the unplug page at once or waiting, this is
    the safer: DShot never reaches a servo that may have entered programming
    mode in an on phase before it has been off as long as LEAVE keeps it.
    The cost is up to 2000 ms on the wait text before the unplug page.
19. **A card phase around every card call.** Not only the first write's
    backup and record: SAVE AS, LOAD, the FILES listing, RETRY MOUNT and the
    record's removal after a verify each block core0 on the card too, and
    each would count as a lapse without one. One rule for every card call is
    simpler to test than a list of exceptions. With `NONE` there is no
    supply to keep on, and no card phase.

Revision of 2026-10-18, after the owner's menu decisions
([menu.md](menu.md#decisions)) and review round 2 of PR #6. Decisions 11, 12
and 15 are superseded: 11 by menu.md decision 13, one SETTINGS list in
sections, in which SERVO SUPPLY is a section of its own between WIRING and
DISPLAY; 12 by menu.md decision 5, whose 40 px floor the rows, controls and
footer of decision 12 meet, under a 36 px header that starts the viewport at
y 40; 15 by menu.md decision 14, which carries it into `ui_list` unchanged.
SETUP in decisions 8, 11, 12 and 15 is the screen menu.md decision 12 names
SETTINGS. Decision 11's count is corrected: SERVO SUPPLY was the sixth row of
eight, after KISS PIN, not the eighth. Decision 8's BOOTSEL clause holds as
long as the boot ROM leaves GP0 undriven, which bring-up step 7 checks.
Decisions 8 and 11 keep the wording the owner decided; the two corrections
stand here only.
