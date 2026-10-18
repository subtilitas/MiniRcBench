# KST and Chaservo servo programming

Status: planned. Nothing here is built. It depends on the pin claim in
[pin-claim.md](pin-claim.md), slice 1 there, which is built: the KST button on
CFG is drawn disabled, `src/ui_kst.cpp` is a stub, and `core1_job.cpp` has no
KST job. This document is slice 3. The
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
run, voltage selection and supply measurement. Switching the servo's supply
on and off is in this slice, through a DC-DC (direct current to direct
current) converter whose enable input the bench drives.

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
only. The servo's supply is the user's, or the switched converter's.

## Switched servo supply

The owner's servos enter programming mode only directly after power-on. With
the supply in the bench's hand, the bench powers the servo up itself, at a
known time before the entry sequence, instead of asking the user to plug it
in while the search repeats. This is rcbench's REBOOT with a plain switch in
place of the PD mini.

### Hardware

A DC-DC converter between the user's supply and the servo's supply wire. The
bench drives the converter's enable input; it does not feed the converter.

| Item | Value |
|---|---|
| Enable pin | GP0, J5 pin 2 (also J4 pin 9); GP1, J5 pin 1, as the alternative |
| Level | 3.3 V push-pull from IOVDD |
| Polarity | set in SETUP: `GP0 HIGH` or `GP0 LOW` (decision 8) |
| Off-state resistor | external, 10 kΩ to 100 kΩ, on the enable input: a pull-down for `GP0 HIGH`, a pull-up to 3.3 V for `GP0 LOW` |
| Converter input | the user's supply. J4 pin 2 is USB VBUS and does not feed a servo: a servo can draw more than 1 A at stall |
| Converter output | the servo's red wire; its rating is the converter's |
| Ground | converter, servo and board grounds joined once, at the board |

GP0 and GP1 are UART0 TX and RX on J4 and J5. The firmware leaves them
unused (`pico_enable_stdio_uart(MiniRcBench 0)`; USB serial is
`usb_dev.cpp`), and SETUP does not offer them for the ESC wires. They are on
PWM slice 0 with the backlight, GP16, so the enable is a plain GPIO output
and never a PWM channel.

The external resistor holds the converter off from reset until the firmware
drives the pin, while the board is unpowered, and while the board is in
BOOTSEL. The internal pulls are not relied on: RP2350 erratum E9 affects the
pad pull-down of an input, and the firmware leaves GP0 undriven until the
settings say which level is off.

```
  user supply  +  ----------------- VIN   DC-DC   VOUT  ------  servo +  (red)
               -  ----------+------ GND           EN  --+-----  GP0 (J5 pin 2)
                            |                           |
                            |                         10 k to 100 k
                            |                           |
  board GND (J5 pin 4)  ----+---------------------------+-----  servo -  (brown or black)
  board SIG (GP29, J4 pin 12)  ---------------------------------  servo signal
```

Drawn for `GP0 HIGH`. For `GP0 LOW` the resistor goes from EN to 3.3 V,
J5 pin 3.

### Firmware

`src/servo_supply.{h,cpp}` owns the pin. `servoSupplyInit()` runs in
`setup()` directly after `settingsLoad()` and drives GP0 to the off level of
the configured polarity, or leaves it an input for `NONE`. A SETUP change of
the row applies at once, with the supply off. Two calls
change it, `servoSupplyOn()` and `servoSupplyOff()`, and `servoSupplyIsOn()`
reports the level the bench drives. There is no read-back: the bench knows
the level it drives, not the voltage on the servo. The level is a fact core1
writes and core0 reads, under the KST job's status lock.

With `SERVO SUPPLY` at `NONE` the search stays as described above: the user
powers the servo up. Every screen text that names
the supply has both forms.

### Search with the switch

One attempt, from `kstSeekStep()`:

1. Supply off, line driven low, for `KST_SUPPLY_OFF_MS`, 2000 ms
   (rcbench `KST_PORT_RAIL_OFF_MS`, the owner's value).
2. Supply on. The line stays low.
3. The entry sequence from `kst_session_enter()`, unchanged: its 100 ms low
   starts at power-on, so the first sync reaches the servo about 106 ms after
   the supply switched on, plus the converter's start-up time.
4. `KST_SES_OK` is `Found`, and the supply stays on. Otherwise the next attempt
   starts at step 1.

One attempt is about 2.12 s, so the 30 s search is 14 attempts. Each attempt
is one known power-up, so a servo that does not answer in 14 power cycles
does not answer at this delay, and the timed-out text says so.

The power-on window question (decision 1) changes form: the delay from power-on
to the first sync is the bench's to choose. Bring-up step 7 measures the
window by stepping that delay; if the servo needs a longer start, the wrapper
waits that long after the supply switches on before the entry starts.

### Leave, loss and heartbeat with the switch

- **Leave** switches the supply off after the 100 ms low, and keeps it off
  for `KST_SUPPLY_OFF_MS` before the unplug page. A servo without power is in
  no mode, so the leave page does not ask the user to power it off. It
  reads `SUPPLY OFF. THE SERVO LEFT PROGRAMMING MODE.`
- **REBOOT**, a new button on the field list, is rcbench's REBOOT: supply off
  for `KST_SUPPLY_OFF_MS`, on, `KST_SUPPLY_WAIT_MS`, 1000 ms (rcbench
  `KST_PORT_RAIL_WAIT_MS`), then the entry and a read. It is the way out of a
  locked session; LEAVE from a locked session still works and switches off.
- **Loss** of a found servo with the supply on is not a power cycle the bench
  made. The wrapper calls `kst_session_power_cycled()` as before and starts a
  new search, which begins with supply off.
- **Heartbeat lapse:** the running operation finishes (pin-claim decision 1).
  After it, the supply goes off, unless a write is in doubt: a locked session
  keeps the supply on, so the next RETRY can read and restore. No new
  attempt starts.
- **Claim end, any path:** the supply is off before the pins return to DShot.
  An ESC never sees the converter on.
- **Boot:** off, from the external resistor until `servoSupplyInit()`, then
  from the firmware.

### Not in this section

- Voltage selection. A converter with I2C (inter-integrated circuit) control,
  or a digital potentiometer on its feedback, on I2C1 (GP6, GP7, on J3 and
  J4). The bus carries the touch controller (0x1A), the IMU (0x6B) and the RTC
  (0x51); a new device avoids those addresses, and a fault on its lead takes
  the touch screen down with it.
- Supply measurement: an INA226 current and voltage monitor at 0x40 to 0x4F on
  the same bus would give the read-back rcbench has from its PD mini.
- The servo tester. A switched supply could make SAFE supply-off and HOLD TO
  START supply-on. Slice 2 does not use it.

### Tests for the switch

`test_kst_seek.cpp` gains a switched-supply model in `kst_sim.h`'s servo: a
servo that listens only within a window after power-on is found on the first
attempt when the window covers the first sync, and on none when it does not;
supply off for 2000 ms before every attempt; off after a leave, after a
heartbeat lapse outside a write, and at every claim end; on through a locked
session. A mutant table as for slice 1.

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

rcbench assumes a powered servo. Its CONNECT runs one entry, and with a PD
mini answering it can cycle the servo's supply itself (REBOOT: off for
2000 ms, on, then 1000 ms more). The owner's servos enter programming mode
only directly after power-on. Without a switched supply, MiniRcBench repeats
the entry while the user powers the servo up. With one, the bench powers the
servo up before each attempt ([switched servo supply](#switched-servo-supply)).

The repetition is a wrapper in MiniRcBench, `src/kst_seek.cpp`, around the
unchanged session. The ported `kst_session.c` stays byte for byte rcbench's.

```c
enum class KstSeek : uint8_t { Searching, Found, Stopped, TimedOut, NoUi };

void    kstSeekBegin(KstSeekState *s, kst_session_t *ses, uint32_t nowMs);
KstSeek kstSeekStep(KstSeekState *s, kst_session_t *ses, bool uiAlive, uint32_t nowMs);
void    kstSeekStop(KstSeekState *s, kst_session_t *ses);   // STOP SEARCH, BACK
```

- One attempt is one `kst_session_enter()` stepped to its end, 116.6 ms to
  118.8 ms. `KST_SES_ERR_NO_SERVO` starts the next attempt at once; its own
  100 ms low is the spacing. That is about 8.4 attempts a second. Frames take
  about 6 ms of each attempt (two reads of 1.37 ms and a sync of 3.23 ms);
  the rest is low line and listening.
- `KST_SES_OK` is `Found`. Any other result ends the search with that result
  shown.
- The search ends after `KST_SEEK_TIMEOUT_MS`, 30 s, about 250 attempts, as
  `TimedOut`, which offers RETRY (decision 2), or at STOP SEARCH or BACK.
  `kstSeekStop()` calls `kst_session_abort()`, which finishes a frame on the
  wire before it stops.
- A heartbeat lapse ([pin-claim.md](pin-claim.md#heartbeat-and-watchdog)) lets
  the running attempt end and starts no new one: `NoUi`.
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
| Searching | `GP29  TRY 12  1.4 S` | `POWER UP THE SERVO NOW`, then `PLUG IT IN OR SWITCH ITS SUPPLY ON. IT ANSWERS ONLY RIGHT AFTER POWER-ON.` and `A POWERED SERVO ON THIS PIN GETS NO PULSES AND MAY GO TO ITS FAILSAFE POSITION.`, a spinner | STOP SEARCH, BACK |
| Edges, no valid reply | | adds, in amber, `A REPLY CAME, NOT VALID: LATE` (the `kst_rx_t` code as a word) | as above |
| Searching, supply switched | `GP29  TRY 3  SUPPLY ON` or `SUPPLY OFF` | `THE BENCH SWITCHES THE SERVO ON AND OFF.` and the failsafe line, a spinner | STOP SEARCH, BACK |
| Timed out | `NO SERVO IN 30 S` | `CHECK SIGNAL AND GROUND, THEN POWER THE SERVO UP AGAIN.` | RETRY, BACK |
| Found | `IN PROGRAMMING MODE` | `READING 32 REGISTERS` | BACK |
| Read failed | | `READ FAILED: NO 3 EQUAL READS` and the registers in `bad_regs` | READ AGAIN, BACK |
| Read | `V6  GP29  BACKUP KST004` | the field list | see below |

The layout of the searching state:

```
y   0 | KST SERVO                        [ BACK ]|
   34 | GP29  TRY 12  1.4 S                      |
   70 |         POWER UP THE SERVO NOW           |  scale 2, C_LIME
  100 |   PLUG IT IN OR SWITCH ITS SUPPLY ON.    |  scale 1, C_DIM
  114 |   IT ANSWERS ONLY RIGHT AFTER POWER-ON.  |
  132 |   A POWERED SERVO ON THIS PIN GETS NO    |
  146 |   PULSES AND MAY GO TO ITS FAILSAFE.     |
  176 |   A REPLY CAME, NOT VALID: LATE          |  C_AMBER, only with edges
  236 |              (spinner)                   |  AM32's twelve-dot spinner
  272 | [          STOP SEARCH           ]       |  46 px; RETRY after a stop
  320 +------------------------------------------+
```

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

```c
enum class KstOp : uint8_t { Search, StopSearch, ReadAll, Verify, Edit, Restore, Abort, Leave };

struct KstCmd {
    uint16_t        seq;
    KstOp           op;
    kst_image_t     target;            // Edit
    kst_field_set_t unlocked;          // Edit; always 0 in this slice
    bool            confirmUnchecked;  // Restore
};

struct KstStatus {
    uint16_t    ackSeq;
    uint8_t     job;          // Opening, Searching, SearchEnded, InMode, Busy, Leaving, Closed
    uint8_t     lineError;    // kst_line_err_t of the open
    uint8_t     seek;         // KstSeek
    uint32_t    attempts, searchMs;
    uint8_t     result;       // kst_ses_t of the last operation
    uint8_t     lastRx;       // kst_rx_t of the last transaction
    uint8_t     inMode, hasImage, hasBackup, fingerprintOk, locked;
    uint8_t     planning, planResult, stepIndex;
    uint32_t    badRegs, diffRegs, wroteRegs;
    uint32_t    frames;       // frames sent by the running operation
    kst_image_t image, backup;
    kst_stats_t stats;
};
```

Every pass of `loop1()` the job checks the heartbeat, takes a new command,
steps the search or calls `kst_session_step()`, and publishes the status under
its lock when it changed. Each step is short: the session never waits, and a
planning step does one slice of `KST_PLAN_SLICE_UNITS`, 100 units, estimated
at 892 µs or less on a Cortex-M33 at 150 MHz (rcbench README, emulator figure,
not measured on hardware).

Memory and stack: the session is 856 bytes and lives in core1's static memory
with the `kst_line_t`. `kst_session_step()` took 528 bytes of stack in
rcbench's coprocessor image; core1's stack is `PICO_CORE1_STACK_SIZE`, 2 KB,
and the DShot pump does not run beside it. The job never calls
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
| LOW, 100 ms at least, and the supply set | `kst_line_open()`, which drives low; with a switched supply, supply off, then on before each attempt |
| PROGRAMMING, the entry holds low 100 ms more | the search |
| LEAVE: low for 100 ms by default, then PWM | low for 100 ms, `kst_line_close()` (input, pull-down on), then the unplug page and DShot |
| POWER_CYCLED, REBOOT | the loss rule above; REBOOT with a switched supply only |

The servo stays in programming mode until its supply is off
(`kst_session.h`: "The mode lasts until the servo's supply is removed").
Whether it follows PWM before that is not measured. So leaving tells the user
to power it off, and the shared unplug page follows before DShot restarts on
the pin.

## Screens

Entry: CFG → SETTINGS → KST, in the destination row
([pin-claim.md](pin-claim.md), decision 2). The AM32 screen is the model:
a state machine that touches nothing until the handover completes, a list with
an editor bar, and a write that needs a 1000 ms hold.

### Field list

```
y   0 | KST SERVO                        [ BACK ]|
   34 | V6  GP29  BACKUP KST004            (MODE)|  strip, chip C_GREEN
   56 | TUNE      MAX. DUTY                 96 % |
      | TUNE      BOOST                300.0 US  |
      | TUNE      DEAD BAND              2.0 US  |  26 px rows, 56..215,
      | PULSE     NEUTRAL               1500.2 US|  pixel scroll as AM32
      | PULSE     REVERSION                  OFF |
      | FAILSAFE  UNCONTROLLED POS     1500.0 US |
      | LOCKED    LOCK                         1 |  C_GRID, not selectable
  218 | [ - ]     DEAD BAND 2.0 US  ->      [ + ]|  editor bar 44 px
  268 | [ HOLD TO WRITE ] [REVERT] [ FILES ]     |  110, 52, 54 px; 46 px tall
  320 +------------------------------------------+
```

- Groups follow rcbench's pages: TUNE (MAX. DUTY, BOOST, STRETCH, SOFT START,
  SPEED LIMIT, SPEED, DEAD BAND), PULSE (PULSE LOWER, PULSE UPPER, NEUTRAL,
  REVERSION, LEFT ANGLE, RIGHT ANGLE), FAILSAFE (ALLOW UNCONT, UNCONTROLLED
  POS, UNCONTROLLED TIME), then LOCKED: every field whose access class is
  `KST_ACCESS_LOCKED` or `KST_ACCESS_NEVER`, read-only.
- Values come from `kst_field_display()` with their unit. The screens write
  `US` for µs; the font has no micro sign, as in rcbench.
- − and + step the raw value with `kst_field_edit()`, hold to repeat. A value
  that breaks a hard rule (`kst_limits_edit()` against the backup) is skipped,
  so the stepper reaches legal values only. A value that breaks a soft rule is
  taken; the row gets an amber mark and the editor bar shows the rule's text,
  for example `WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.`
- An image that fails the layout fingerprint is read and shown, and the
  footer reads `READ ONLY: UNKNOWN LAYOUT`; the session refuses its writes.
- The strip names the type, `V6` or `V8` (`kst_type()`), the pin and the
  backup file, or `BACKUP IN MEMORY` without a card.

### Write

HOLD TO WRITE opens the write page rather than writing at once.

```
y   0 | KST WRITE                        [ BACK ]|
   34 | 2 FIELDS                                 |
   56 | DEAD BAND        2.0 US  ->  3.0 US      |
   76 | SOFT START            1  ->  0           |
  120 | * WITHOUT SOFT START THE SERVO JUMPS     |  C_AMBER, one line a rule
  134 |   AT POWER-ON.                           |
  220 | BACKUP FIRST: KST/KST005.TXT             |  or NO CARD: BACKUP IN MEMORY
  268 | [          HOLD TO WRITE           ]     |  1000 ms, progress bar
  320 +------------------------------------------+
```

The hold is the confirmation of every soft rule shown on the page. While the
write runs the page shows `WRITING: DO NOT UNPLUG.`, then
`PLANNING THE ORDER OF THE WRITES` while `planning` is set, then
`FRAMES SENT: 94`. The result is one line per `kst_ses_t`, from a table in
`ui_kst.cpp` that the tests hold complete, with OK to return to the list:
`WRITE VERIFIED`, `NOTHING WAS WRITTEN`, `ROLLED BACK: THE SERVO HOLDS ITS
OLD VALUES`, `IN DOUBT: RESTORE FROM FILES`, and the others.

### Files

```
y   0 | KST FILES                        [ BACK ]|
   34 | CARD  12 FILES               PAGE 1/3    |
   56 | KST001                                   |  5 rows, 30 px
   86 | KST002                                   |
  116 | KST003                                   |
  146 | KST004                        (BACKUP)   |
  176 | KST005                                   |
  208 | [  <  ]                         [  >  ]  |
  242 | [          SAVE AS KST013          ]     |
  278 | [  LOAD  ]       [ RESTORE BACKUP ]      |
  320 +------------------------------------------+
```

- SAVE AS writes the servo as it stands under the next free name from
  `kst_store_preset()`, KST001 to KST999. There is no keyboard, so there is no
  other name; a file renamed on a computer still lists and loads.
- LOAD parses the selected file (`kst_file_parse()`), holds it against the
  servo (`kst_file_load()`: type, access classes, hard limits) and puts the
  result into the list as pending edits. A refusal names its code and line,
  for example `LINE 14: NOT A VALUE OF DEAD BAND`. The write then goes through
  HOLD TO WRITE as any edit.
- RESTORE BACKUP runs `kst_session_restore()` to the first image read this
  session. A plan the planner marks unchecked asks for a second hold.
- With no card mounted, or the card held by a USB host, the page shows
  `NO CARD` or `CARD IN USE BY USB` and RETRY MOUNT (`sdLogRemount()`);
  reading, editing and writing still work.

### Leave

BACK from the list, or from the search after a servo was found, opens:

```
  POWER THE SERVO OFF.
  IT STAYS IN PROGRAMMING MODE UNTIL ITS SUPPLY IS OFF.
  2 CHANGES NOT WRITTEN.                       (only with pending edits)
  THE SERVO IS IN DOUBT: RESTORE FIRST.        (only while locked)
  [ LEAVE ]
```

With a switched supply the first two lines read `SUPPLY OFF. THE SERVO LEFT
PROGRAMMING MODE.`, and REBOOT sits beside LEAVE.

LEAVE is a tap. It leaves even a locked session: without a supply to switch
there is nothing else the screen can do, and the record file below keeps what
the servo was. Then the unplug page from [pin-claim.md](pin-claim.md), then
DShot. BACK during a write is queued: the page shows
`LEAVING AFTER THE WRITE ENDS.` and the leave follows the result. BACK during
a search, a read or a verify aborts it.

## Storage

The files live in the `KST` directory of the SD card, `KST_STORE_DIR`, apart
from the blackbox logs in the root. A file is `<name>.TXT` in rcbench's text
format: 35 lines, under 1024 bytes, CRC-32 (cyclic redundancy check) checked,
editable by hand on a computer once its checksum line is removed. FatFs here
is built with long file names (`FF_USE_LFN` 3, `FF_MAX_LFN` 255, in the
library's `ffconf.h`), which covers `KST_STORE_BASE_MAX`, 32 characters.

`kst_store.c` reaches the card through 5 functions (`kst_store_io_t`:
`mounted`, `walk`, `size`, `read`, `create`). `src/kst_files.cpp` implements
them over FatFs on core0 and answers "not mounted" while `mscHoldsCard()` is
true.

| Situation | Effect |
|---|---|
| First write of a session, card mounted | the backup is saved first as the next KSTnnn.TXT, and `KST/KST.TXT` holds the image before the write. A card that takes neither stops the write: `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.` |
| Write verified | `KST/KST.TXT` is removed |
| `KST/KST.TXT` found when the screen opens | `A WRITE WAS INTERRUPTED. KST/KST.TXT HOLDS THE SERVO AS IT WAS.` FILES lists it as RECORD and loads it like a file |
| No card | read, edit and write work; the write page says `NO CARD: THE BACKUP STAYS IN MEMORY AND ENDS AT LEAVE.` |
| Card held by a USB host | as no card |

`KST.TXT` is the base name `kst_store.c` keeps free for this record
(`KST_STORE_RECORD`); `kst_store` refuses it for a save and a load, so
`kst_files.cpp` reads and removes it itself. rcbench's full journal
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
| `shared/outputs/kst_port.c`, `prog_port.c` | 1,616 | `src/kst_job.cpp`, `src/kst_seek.cpp` | not ported: they are the coprocessor's link page, the PD mini supply sequence and the port bank. The rules MiniRcBench keeps (out of PWM, low, entry, in the mode until power-off, leave with 100 ms low) are in the job and the wrapper |

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
`greatest.h` (its own MIT file, about 100 lines, no framework), the servo and
line model `kst_sim.h`, `kst_fake_card.h` and `fixtures/kst_line_words.txt`,
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
  state; leave with 100 ms low before the close; the job never calls
  `kst_session_write()`.
- `test_kst_files.cpp`: the FatFs io over a fake volume: no card, card held by
  USB, card full, write-protected, the record written before the first write
  frame and removed after a verify, the record found at entry.
- `test_ui.cpp`: every state of the screen renders, BACK in every state, HOLD
  TO WRITE fires after 1000 ms and not on a slide off, a hard-limit value is
  unreachable by stepping, the result table covers every `kst_ses_t`.
  `make previews` adds `docs/kst-preview.png`.

## Work breakdown

1. Vendor the core, the line driver and the store; `test/rcbench/` with the
   ported tests in `make` and CI.
2. `kst_seek.*` and `kst_job.*` with their tests.
3. `kst_files.cpp` and its tests.
4. `ui_kst.cpp`: search, list, write, files, leave; the CFG entry enabled; UI
   tests; preview.
5. `servo_supply.*`, the SETUP row `SERVO SUPPLY` and `SETTINGS_VERSION` 3;
   the switched search, REBOOT and the supply rules in `kst_seek.*` and
   `kst_job.*`, with the tests in [tests for the switch](#tests-for-the-switch).
6. README: the KST section, the wiring, the legal notice. Wiki pages
   `KST-Programming.md` and its German counterpart, and their entries in
   `_Sidebar.md` and the screens pages.
7. Bring-up, in rcbench's order and before any write is relied on: the reply
   level at the connector; the power-on window of the owner's
   servo, measured as the shortest delay from power-on to a sync that still
   enters; frames and replies on a scope; read reliability; one write with
   read-back, then a power cycle and a read.

About 10 to 12 days for 1 to 6. Bring-up depends on the servo and is not
estimated.

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

Decisions of the owner, 2026-10-18, on the switched supply:

7. **Enable pin.** GP0, J5 pin 2.
8. **Configuration.** A SETUP row, `SERVO SUPPLY`, with three values:
   `NONE` (no switch fitted; GP0 stays an input with no pull), `GP0 HIGH`
   (active-high enable) and `GP0 LOW` (active-low enable). `NONE` is the
   default. The row adds a field to the settings block, so
   `SETTINGS_VERSION` goes from 2 to 3. A block of another version is
   discarded whole (`settingsLoad()`), so the first boot after the update
   starts from defaults once, and a block DshotDisplay saved does not load.
   Between reset and the settings load the firmware does not drive GP0; the
   external resistor holds the converter off: a pull-down for `GP0 HIGH`, a
   pull-up for `GP0 LOW`.
9. **Timing.** rcbench's values: 2000 ms off before each search attempt and
   for REBOOT, 1000 ms start for REBOOT, and the entry directly after the
   switch-on in the search, until bring-up measures the servo.
10. **Servo tester.** It uses the switched supply in a change after slice 2:
    SAFE is supply off, HOLD TO START switches it on, STOP and a heartbeat
    trip switch it off.
