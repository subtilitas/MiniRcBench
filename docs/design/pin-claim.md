# Pin claim and core1 jobs

Status: slices 1 and 2 are built. Slice 1: the claim (`src/pin_claim.h`),
the core1 job dispatch (`src/core1_job.h`), the drain, the AM32 screen on the
claim, the rows for its tools and the stub screens. Slice 2: the servo tester and its
core1 job ([servo-tester.md](servo-tester.md)). Host tests only; nothing of
either is tested on hardware. Slice 3, KST programming, is built through its
step 5 ([kst-programming.md](kst-programming.md)). Slice 4, Hitec programming
([hitec-programming.md](hitec-programming.md)), adds the Hitec job; its
steps 1 and 2 are built. Slices A to C of [menu.md](menu.md), the menu
restructure, are built before slice 3.
Since slice C, the tester's third button reads MENU, the list of tools is
MENU, and the screen for the persisted settings is SETTINGS. This document
uses those names; its recorded decisions keep the names they were made
under. The design uses the DShot
driver in `src/dshot/` (`out_dshot_bind()`, `out_dshot_stop()`,
`out_dshot_release()`).

Terms: ESC is electronic speed controller. DShot is the digital ESC protocol
the tester drives; bidirectional DShot inverts the line and the ESC answers on
the same wire. EDT is Extended DShot Telemetry. KISS telemetry is the serial
protocol of the KISS ESC line, sent on its own wire. AM32 is an open ESC
firmware with a serial bootloader. KST and Chaservo are servo brands that
share one programming protocol. PWM is pulse-width modulation, here the
RP2350's hardware PWM slices that make servo pulses. PIO is the RP2350's
programmable input/output block, SM is one PIO state machine, DMA is direct
memory access, GPIO is general-purpose input/output, ADC is analogue-to-digital
converter, SD is Secure Digital, SDIO is Secure Digital input/output, UI is
user interface, USB is Universal Serial Bus.

The two features that use the claim have their own documents:
[servo-tester.md](servo-tester.md) and
[kst-programming.md](kst-programming.md). This document holds what they share:
the claim, the pins, the hardware budget and the wiring. The slice plan for all
three is at the end.

## Why

MiniRcBench runs on the Waveshare RP2350-Touch-LCD-2.8, which brings out two
signal pins, its only free GPIOs. Outside a claim both belong to the ESC: the
DShot pump on one, the KISS receiver on the other. The servo tester
and the KST programmer need the same two wires. A second set of pins does not
exist, so the pins change owner by screen.

## The two pins

The claim names pins by role, not by number, because the number is a setting.

| Role | Outside a claim | Claimed by |
|---|---|---|
| SIG | the ESC signal wire, `settings()->dshotPin` | AM32 config, servo A, KST programming, Hitec programming |
| AUX | the KISS telemetry wire, `settings()->kissPin` | servo B (two-servo mode) |

AUX is the KISS pin whether or not KISS is switched on in SETTINGS. KISS is off
by default, and GP1 is still the AUX pin.

| Role | Default | Connector |
|---|---|---|
| SIG | GP0 | J4 pin 9, J5 pin 2 |
| AUX | GP1 | J4 pin 10, J5 pin 1 |
| GND | | J4 pins 1 and 5, J5 pin 4 |

SETTINGS offers GP0 and GP1 only, for either role (`BOARD_FREE_GPIO_MASK`
0x00000003). They are UART0's pins, and the firmware does not use UART0
(universal asynchronous receiver-transmitter). GP28 (J4 pin 11) and GP29 (J4
pin 12) carry only the optional switched servo supply's enable; see [Signal
level](#signal-level). With KISS off, SETTINGS lets the ESC pin take the KISS
pin's GPIO, so SIG and AUX can name the same pin.

## The claim

A claim takes the whole ESC task down, not one pin of it. With the SIG pin
gone the ESC hears no frames and disarms, and with no DShot frames there is no
KISS request, so the KISS receiver has nothing to receive. A claim of SIG alone
would leave a receiver running on AUX for no reason, and a claim of AUX alone
has no user: every screen that wants AUX also wants SIG. So every claim is the
existing `escTaskSuspend()` and both pins are free while it holds. The roles a
screen asks for decide only which pins its job binds.

| Screen | Roles bound | Core1 job | Who drives the pin |
|---|---|---|---|
| AM32 CONFIG | SIG | none, core1 idle | core0, bit-banged bootloader transport (`am32_bl.cpp`) |
| SERVO, one servo | SIG | servo | core1, hardware PWM |
| SERVO, two servos | SIG and AUX | servo | core1, hardware PWM |
| KST SERVO | SIG | KST | core1, `kst_line` PIO driver and session |
| HITEC SERVO | SIG | Hitec | core1, `hitec_line` PIO driver and session |

A claim lasts while its screen is open, its sub-screens included (SERVO SET,
KST FILES). Leaving the screen with BACK ends it. No other screen is reachable
during a claim: SETTINGS, SD LOG and the main screen sit behind BACK.

### Sequence

Core0 runs the claim as a state machine in `src/pin_claim.cpp`, one step per UI
frame (`UI_PERIOD_MS`, 25 ms). Core1 makes every PIO change on the two signal
pins, because the core that claims a PIO state machine is the core that frees
it, as `esc_task.h` requires; the AM32 transport, which binds SIG on core0, is
the exception.

1. **Check.** Core0 refuses the claim while armed, and, for SERVO, KST and
   HITEC,
   while an ESC answers on SIG. See [interlocks](#interlocks). DShot keeps
   running during the check.
2. **Stopping.** Core0 calls `escTaskSuspend()`, which forces a disarm and
   sets the request. Once core1 sees the request, it sends throttle-zero
   frames until more than `ARM_ZERO_THROTTLE_MS`, 250 ms, have passed since
   the last frame that carried a throttle, so the ESC sees at least 250 ms of
   them (`escHandoverStep()`, `escDrainDone()`). Then, in this order:
   `out_dshot_stop(sig)` cuts any frame in flight and leaves no edges,
   `out_dshot_release(sig)` puts the pad back as a plain input with the pulls
   off, `esc_task.cpp` puts the pull-up back on SIG, so the line holds the
   bidirectional idle level until the claim's user takes it, and
   `kissUartEnd()` ends the PIO receiver on AUX. Core1 then sets
   `escTaskSuspended()`. The drain frames carry no command and no KISS
   request. A pump that sent no throttle frame since its bind needs no drain.
   Coming from MENU, the 250 ms have usually passed: opening MENU disarms. A
   user who taps MENU and AM32 CONFIG in quick succession can open AM32 about
   100 ms after the last throttle frame, and its handover then waits out the
   rest. A rebuild after a SETTINGS pin change drains the same way.
3. **Held.** Core0 reads the GPIO numbers once: SIG from `escTaskDshotPin()`,
   AUX from `escTaskKissPin()`. `pinClaimGpio()` returns them from here on, for
   the roles the claim asked for. For SERVO, KST and HITEC it starts the core1 job
   with those numbers, and with AUX as 0xFF when the claim did not ask for it.
   AM32 has no core1 job: it calls `am32BlBegin(pinClaimGpio(PIN_SIG))`, and
   its claim stays Held until BACK.
4. **Running.** The job binds its pins and reports `Running`, or reports a bind
   failure and binds nothing. A failed bind leaves the claim Held, with the
   reason in `pinClaimJobError()`. RETRY (`pinClaimRetry()`) starts the job
   again and keeps the claim; BACK goes to step 5.
5. **Leaving.** BACK (`pinClaimEnd()`) asks the job to stop. The job lets its
   pins go once nothing it drives is under way, which for the servo job is
   the last pulse (`servoQuietMs()`, 41 ms at 50 Hz) and for the KST job the
   frame on the wire and then 100 ms of low line (`KST_LEAVE_LOW_MS`); for
   the Hitec job a running write, then 2000 ms of switched supply off, or
   the D series reboot with `SERVO SUPPLY` at `NONE`. The servo and KST jobs
   leave their pins inputs with the pull-down on, the Hitec job leaves SIG an
   input with its family's pull: the pull-down for classic, the pull-up for
   the D series. Then the job reports `Idle`. The claim stays Leaving until
   then. Before DShot restarts, SERVO, KST and HITEC show the
   unplug page ([restore](#restoring-dshot)):
   `pinClaimUnplugDue()` is true once the job is idle, RESTART DSHOT calls
   `pinClaimRestart()`, and BACK on the page calls `pinClaimKeep()`, which
   returns the claim to Held and starts the job again. AM32 has neither a job
   nor the page, so its BACK restores DShot within the `pinClaimEnd()` call.
6. **Restoring.** Core0 calls `escTaskResume()`. Core1 rebuilds the driver from
   the wiring latched in `esc_task.cpp`, and the claim is `Free` from the
   first look after `escTaskSuspended()` turns false. Nothing is left for
   core0 to do in this step, so it ends without a poll once the screen has
   closed.

BACK during Stopping resumes DShot at once. No job has run and nothing has
been driven, so no unplug page is shown.

### API

Core0, in `src/pin_claim.h`:

```c
enum class PinUser : uint8_t { None, Am32, Servo, Kst, Hitec };
enum class ClaimState : uint8_t { Free, Stopping, Held, Running, Leaving, Restoring };
enum class ClaimRefusal : uint8_t {
    None,
    Armed,         // the tester is armed
    Busy,          // another screen holds the pins
    EscAnswering,  // eRPM within ESC_LINK_STALE_MS (500 ms); SERVO, KST, HITEC only
    KissLive,      // a KISS frame within KISS_STALE_MS (500 ms), AUX asked for
    SamePin,       // AUX asked for, and SIG and AUX are the same GPIO
};

ClaimRefusal pinClaimBegin(PinUser who, uint8_t roles);  // PIN_SIG | PIN_AUX
ClaimState   pinClaimPoll();               // once per UI frame, by the holder
ClaimState   pinClaimState();              // no step
PinUser      pinClaimHolder();             // None while Free
uint8_t      pinClaimGpio(uint8_t role);   // latched at Held; 0xFF before
uint8_t      pinClaimJobError();           // why a bind failed, 0 for none
void         pinClaimEnd(PinUser who);     // BACK; the poll finishes it
void         pinClaimRetry(PinUser who);   // RETRY after a failed bind
bool         pinClaimUnplugDue();          // SERVO, KST, HITEC: Leaving, job idle
void         pinClaimRestart(PinUser who); // RESTART DSHOT on the unplug page
void         pinClaimKeep(PinUser who);    // BACK on the unplug page
```

What differs per user is a table, `PIN_USERS` in `pin_claim.h`, one row per
`PinUser`: the core1 job (None: the screen drives the pin from core0, as AM32
does), whether the unplug page comes before DShot restarts, and whether the
claim is refused while an ESC answers on SIG. `pinUserInfo()` reads it. A new
pin user is one enum value and one row.

The refusal check is a pure function, `pinClaimCheck()`, beside it, so the
host suite tests every rule without the SDK. Its order is Armed, Busy,
EscAnswering (SERVO, KST and HITEC only; AM32 is exempt), KissLive, SamePin;
freshness is `escFieldFresh()`, the rule the readouts use.

Core1, in `src/core1_job.h`:

```c
enum class Core1Job : uint8_t { None, Servo, Kst, Hitec };
enum class Core1JobPhase : uint8_t { Idle, Starting, Running, Stopping };
struct Core1JobOps {                       // one per job, defined by its slice
    uint8_t (*bind)(uint8_t sigGpio, uint8_t auxGpio);  // 0, or CORE1_JOB_PWM ...
    void    (*step)(uint32_t nowMs, bool uiAlive);
    bool    (*release)(uint32_t nowMs);  // each pass while Stopping, until true
    void    (*init)();                     // core0, from core1JobInit(); may be null
};

void core1JobInit();          // core0, before core1 is launched; calls each init
bool core1JobStart(Core1Job job, uint8_t sigGpio, uint8_t auxGpio);  // core0
void core1JobStop();                                                   // core0
Core1Job core1JobRunning();   // core0: which job runs; None once idle
Core1JobPhase core1JobPhase();// core0
uint8_t core1JobError();      // core0: CORE1_JOB_PWM, _PIO, _DMA, _CLOCK;
                              // REFUSED comes from pinClaimJobError()
void core1JobPoll();          // core1, from loop1() after escTaskPoll()
bool core1UiAlive();          // core1: escHeartbeat() within 250 ms
```

`loop1()` is `escTaskPoll(); core1JobPoll();`. `escTaskPoll()` returns at
once while suspended, and `core1JobPoll()` returns at once, without a lock,
while no job runs. `core1JobStart()` is refused unless `escTaskSuspended()` is
true, so the DShot pump and a job never drive a pin together. It is refused
too for `Core1Job::None`, for a job with no `Core1JobOps` in the table in
`core1_job.cpp`, and while a job is not idle. Slice 2 adds the servo job's
entry, `SERVO_JOB_OPS` from `src/servo_job.h`; slice 3 adds KST's, and
slice 4 `HITEC_JOB_OPS` from `src/hitec_job.h`. A stop
that arrives while core1 binds is kept, and the next pass releases what the
bind took. A job's `release` never blocks: it returns false while it still
needs time, and the job stays Stopping, with the next pass asking again.

Each job has its own command block and status block, written and read under
its own critical section, in the same pattern as `escSnapshot()`. They are in
the two feature documents. `core1UiAlive()` is `escUiAlive()` against
`escHeartbeatMs()`, the rule `escTaskPoll()` uses, so one rule decides "the UI
is alive" for the pump and every job.

### What core1 does while a pin is claimed

| Claim | Core1 | Why there |
|---|---|---|
| AM32 | nothing; `escTaskPoll()` and `core1JobPoll()` both return at once | the bootloader transport runs on core0 and blocks it for about 40 ms per attempt; nothing in it needs core1 |
| SERVO | the servo job: holds the PWM bindings, slews the pulse width at SPEED, runs the sweep curve, checks the heartbeat, every pass of `loop1()` | core0 draws at 40 Hz and stalls for tens of milliseconds on SD writes; a 5 Hz sine sampled at 40 Hz is 8 steps per cycle. Core1 samples the curve every millisecond and PWM latches it at each frame |
| Hitec | the Hitec job: `hitec_session_next()`, the step on the `hitec_line` driver or the switched supply, `hitec_session_feed()` | the line driver wants a poll every 1 ms or faster; a PRECHARGE receive decodes about 29 sampler words a millisecond ([hitec-programming.md](hitec-programming.md#where-the-session-runs)) |
| KST | the KST job: the entry loop wrapper, `kst_session_step()` and the `kst_line` driver | the session needs a step every 1 ms or faster, and its driver slack is 5 ms (`KST_DRIVER_SLACK_US`); core0 cannot promise either |

### Interlocks

**Armed: the claim is refused.** `pinClaimBegin()` returns `Armed` and
changes nothing. It does not disarm on the caller's behalf. Every entry point
is a MENU row, and `uiOpenMenu()`, the one route from the tester to MENU,
disarms, so through the UI the refusal is unreachable. A route added later
that reaches a claim while armed then shows up as a refused claim in a test,
instead of a motor stopped as a side effect of navigation.
`escTaskSuspend()` keeps its own forced disarm as the backstop under that
check.

An armed ESC therefore never has its pin taken. What it sees is: throttle zero
from the disarm, at least 250 ms of zero frames, then silence. On silence the
ESC disarms after its own signal-loss timeout, which is ESC firmware specific
and not recorded here.

**An ESC answers on SIG: SERVO, KST and HITEC wait.** Servo pulses of 1000 to
2000 µs are a valid PWM throttle signal, and AM32 and BLHeli_32 accept PWM
input. A KST entry sequence holds the line low for 100 ms and sends frames
into whatever is there, and a Hitec entry sends its first command. So while an eRPM frame has arrived within
`ESC_LINK_STALE_MS`, 500 ms, the screen does not claim. It shows the reason
and keeps checking every frame with DShot still running; 500 ms after the ESC
goes quiet it claims by itself. The pulses still need HOLD TO START
([servo-tester.md](servo-tester.md)). The check misses an ESC that does not
speak bidirectional DShot, and an ESC plugged in after the claim. The screen
text says so.

AM32 is exempt: it needs the ESC on the pin.

**KISS frames on AUX: two-servo mode waits.** A live KISS frame within
`KISS_STALE_MS`, 500 ms, means an ESC telemetry output drives AUX. Servo PWM
on that pin would drive against it. The two-servo mode is refused with the
reason; one-servo mode is not affected.

**SIG and AUX on one GPIO: two-servo mode is refused.** With KISS off, SETTINGS
can set the ESC pin to the KISS pin's GPIO. Two servos on one pin would be
one compare register driven twice. `pinClaimCheck()` returns `SamePin` when
AUX is asked for and the two roles name the same GPIO; one-servo mode is not
affected.

### Heartbeat and watchdog

Core0 calls `escHeartbeat()` at the top of every `uiTick()`, on every screen.
`UI_HEARTBEAT_TIMEOUT_MS` is 250 ms.

| Owner | Heartbeat lost for 250 ms | Heartbeat back |
|---|---|---|
| DShot | throttle forced to zero (`escFrameAction()`, unchanged) | throttle follows the UI again |
| AM32 | nothing; the transport runs on core0, which is what stopped | nothing |
| Servo job | both pins to level 0, no edges; the sweep ends; the job latches `Tripped` | nothing restarts. The screen shows `STOPPED: SCREEN STALLED` and needs a new HOLD TO START |
| Hitec job | an entry is cut: an off phase ends with the supply never on, a LISTEN is cut, a SEND finishes; a read or a write finishes; then, with a switched supply, the supply goes off and the session starts again; with `NONE` the session stays in the mode, and a cut entry ends with RETRY. In a card phase the card heartbeat counts for at most 15 s ([hitec-programming.md](hitec-programming.md#heartbeat-and-lapses)) | entries and operations are taken again; after a stall the entry needs RETRY |
| KST job | the running operation finishes; no new operation and no new entry attempt starts; the line stays driven low. With a switched supply ([kst-programming.md](kst-programming.md#leave-reboot-loss-and-heartbeat-with-the-switch)), every wait in the search or in REBOOT ends at once with the supply off; a session operation finishes, then the supply goes off; the supply is switched on only while the heartbeat is trusted. In a card phase around a card call, a card heartbeat from the SDIO driver's waits counts as the UI's for at most 15 s ([card calls](kst-programming.md#card-calls-and-the-heartbeat)) | operations are taken again; the search needs RETRY. With a switched supply the screen says the supply went off |

Why KST does not abort: `kst_session_abort()` after the first write frame of a
plan locks the session and can leave the servo between two images. The
heartbeat exists to stop motion that the UI can no longer control; a write
plan commands no motion and ends by itself. Whether the motor moves in
programming mode is not measured (rcbench `docs/ServoProgrammer.md`,
measurement 2); decision 1 below sets the rule without that measurement.

No flash write happens during a claim. `settingsSave()` parks core1 for tens
of milliseconds, which would stall a KST transaction past its driver slack.
SETTINGS, the only caller, is unreachable while a claim holds, and
`settingsSave()` checks it: during a claim it refuses and writes nothing.

### Settings

The claim reads the pins SETTINGS chose, as latched by `escTaskConfigure()`, not
the compiled defaults. They cannot change during a claim, because SETTINGS is
unreachable. A pin change in SETTINGS applies to the next claim.

Nothing about the servo tester is stored in the settings block; see
decision 2 in [servo-tester.md](servo-tester.md). The programmer stores one
field, `SERVO SUPPLY`, whether a switched servo supply is fitted and whether
its enable is on GP28 or GP29
([kst-programming.md](kst-programming.md#decisions), decision 8). Slices 1
and 2, and slices A to C of [menu.md](menu.md#storage), leave
`SETTINGS_VERSION` at 2; slice 3 makes it 3 for the field. The layout is
version 4, with the signal pins on GP0 and GP1 and the enable on GP28 or
GP29. A stored block of an earlier version, its ESC pin on GP29 or GP28 or
its enable on GP0, is discarded whole and the compiled defaults load, so
every saved setting returns to its default once after the update.

### When the pin is unavailable

The screen draws its header and BACK, a message in place of its body, and its
primary control disabled (drawn and gated per `ui_widgets.h`). BACK always
works.

| Cause | Screen text | Way out |
|---|---|---|
| An ESC answers on SIG | `AN ESC ANSWERS ON GP0.` `UNPLUG IT: SERVO PULSES READ AS THROTTLE.` | unplug the ESC; the claim follows 500 ms later |
| KISS frames on AUX, two servos asked | `KISS WIRE ON GP1 IS LIVE.` `TWO SERVOS NEED IT UNPLUGGED.` | unplug it, or choose one servo |
| SIG and AUX on one GPIO, two servos asked | `ESC AND KISS PIN ARE THE SAME GPIO.` `MOVE THE ESC PIN IN SETTINGS.` | choose one servo, or change a pin in SETTINGS |
| The job could not bind | `GP0 COULD NOT BE TAKEN (PWM)` or `(PIO)`, `(DMA)`, `(CLOCK)` | RETRY keeps the claim; BACK shows the unplug page before DShot restarts |
| Armed, or another screen holds the pins | `DISARM FIRST` or `PINS BUSY` | BACK; unreachable through the UI |

The GPIO numbers in the text are the live ones from SETTINGS.

### Restoring DShot

`escTaskResume()` rebuilds the driver from the wiring `esc_task.cpp` latched:
the same SIG pin, bitrate, bidirectional mode, KISS enable and KISS pin. Pole
count and throttle ceiling live in the settings block and are untouched. The
pump starts disarmed with HOLD off, as after a SETTINGS change. Its first frames
are the EDT enable, and it decodes every reply with EDT on (`escEdtBind()`,
`ESC_EDT_DECODE`), so an ESC that kept EDT on through the session reads
correctly from its first reply. The repeated enable starts again for the ESC
it finds, because a rebuild clears its state. Telemetry counters in
`EscTelemetry` carry on.

`out_dshot_bind()` sets the pull-up and the output inversion on SIG.
`kissUartBegin()` sets the pull-up on AUX. Both override what the job left:
servo PWM and `kst_line_close()` each leave an input with the pull-down on.

**Unplug page.** Bidirectional DShot idles the line high and pulls it low for
about 27 µs per frame at 600 kbit/s, once per `DSHOT_PERIOD_US`, 1000 µs. A
PWM servo left on SIG measures that as a pulse of about 973 µs at 1 kHz: it
drives towards one end of its travel, at a frame rate that overheats an
analogue servo. So before `escTaskResume()` the SERVO, KST and HITEC screens
show:

```
  UNPLUG THE SERVO.
  DSHOT RESTARTS ON GP0 AND READS AS
  A 970 US PULSE AT 1000 HZ.
  [ RESTART DSHOT ]
```

RESTART DSHOT is a tap. BACK on that page returns to the screen and keeps the
claim. The same hazard exists at boot for a servo left on SIG; the README
section "Servo tester" and the wiki page Servo-Tester carry that warning.

The page itself is drawn by the SERVO, KST and HITEC screens. The rule is in
the claim: for those three users DShot does not restart until
`pinClaimRestart()`, so the screens of slices 2 to 4 cannot skip the page.

### Reuse of the AM32 handover

The claim is the AM32 screen's pin handover moved into `pin_claim.cpp` and
given a state, a refusal and a user. `ui_am32.cpp` calls
`pinClaimBegin(PinUser::Am32, PIN_SIG)` on entry and keeps its own `S_HANDOVER`
state, which reads `pinClaimPoll()` until Held and then calls
`am32BlBegin(pinClaimGpio(PIN_SIG))`. On exit it calls `am32BlEnd()` and then
`pinClaimEnd(PinUser::Am32)`, which resumes DShot within the call. The one step
the handover has beyond release and rebuild is the zero-frame drain of at least
250 ms. A refused claim, which the UI cannot reach, shows `DISARM FIRST` or
`PINS BUSY` and is asked again every frame.

## Hardware

### PWM slices

The RP2350 has 12 PWM slices with two channels, A and B. A slice has one
counter and one wrap register, so both channels run at one frame rate. A
channel is one compare register, so two pins on the same channel carry the
same pulse width. For GP0 to GP31 the slice is `(gpio >> 1) & 7` and the
channel is `gpio & 1` (`PWM_GPIO_SLICE_NUM` in Pico SDK 2.1.1,
`out_pwm_map.c` in rcbench).

| GPIO | Slice | Channel | Use |
|---|---|---|---|
| GP0 | 0 | A | SIG |
| GP1 | 0 | B | AUX |
| GP16 | 0 | A | backlight; driven by PIO (`backlight.pio`), not by the slice |

The two servos are on one slice, channels A and B: two pulse widths, one
frame rate. When SIG and AUX are different GPIOs they are on different
channels, and the servo tester's one frame rate for both servos never needs
a choice. When SETTINGS has put them on the same GPIO, the claim refuses
two-servo mode (`SamePin`).

The backlight on GP16 is slice 0, channel A, as GP0 is, and runs from a
PIO state machine on pio0 at 20 kHz (`src/backlight.pio`), so slice 0 is
free for the servo outputs. `board_desc.cpp` asserts that every free GPIO is
on one slice, so two servos share one frame
rate, and that at most one free GPIO is on each channel of that slice, so two
different free pins are its two channels and never one compare register.

The PWM counter runs at 1 MHz: `clk_sys` 150 MHz divided by 150, an integer
divider. One count is 1 µs. The wrap is 19,999 at 50 Hz and 1,784 at 560 Hz.

### PIO and DMA budget

Each PIO block has 32 instruction words and 4 state machines. The RP2350 has 3
blocks and 16 DMA channels. Program sizes are counted from the `.pio` sources
and, for SDIO, from the generated header in no-OS-FatFS-SD-SDIO-SPI-RPi-Pico
v3.6.2.

| User | Program words | SMs | DMA channels | When |
|---|---|---|---|---|
| DShot, bidirectional (rcbench `dshot_bidir_tx` + `dshot_rx`) | 11 + 5 = 16 | 2, adjacent, one block | 0 | ESC running |
| KISS receiver (`pio_uart_rx`) | 9 | 1 | 0 | ESC running, KISS on |
| SDIO card (`sdio_cmd_clk` + `sdio_data_rx` + `sdio_data_tx`), fixed to pio1 | 18 + 5 + 9 = 32 | 2 | 2 | always |
| Panel (`st7789.cpp`) | 0 | 0 | 1 | always |
| Backlight (`backlight`), fixed to pio0 | 6 | 1 | 0 | always |
| KST line (`kst_line`) | 24 | 1 | 2 | during a KST claim |
| Hitec line (`hitec_line`), and its PRECHARGE sampler for the D series | 30, and 7 at 150 MHz | 1, and 1 | 2, and 1 | during a Hitec claim |
| Servo PWM | 0 | 0 | 0 | PWM slices only |

Placement, with DShot built before KISS as `escTaskPoll()` does. The SDK's
free-block search starts at pio2, and `src/esc_task.cpp` asserts at build
time that DShot and KISS fit one block (3 state machines, 25 of 32 words):

| Block | ESC running | KST claim | Hitec claim, D series |
|---|---|---|---|
| pio0 | backlight, 6 words, 1 SM | backlight | backlight, and the sampler, 7 words |
| pio1 | SDIO, 32 of 32 words, 2 SMs | SDIO, full | SDIO, full |
| pio2 | DShot 16 + KISS 9 = 25 words, SM0 to SM2 | KST 24 words, SM0 | Hitec 30 words, SM0 |
| DMA | 3 of 16 | 5 of 16 | 6 of 16 |

Nothing needs the claim for resources. If the KST line opened beside a
running DShot and KISS, it would not fit in pio2 (7 words left) and would land
in pio0. The claim exists for the pin, not for the PIO block.

### System clock

Nothing in MiniRcBench sets `clk_sys`. Pico SDK 2.1.1 defaults it to
150 MHz (`SYS_CLK_HZ` in `platform_defs.h`), and neither the FatFs driver nor
the panel driver changes it. The KST driver needs a 25.40 µs half-cell within
0.2 %: 3810 cycles at 150 MHz, exact. The clock may move by ±0.3 MHz before
`kst_line_open()` refuses it.

### Signal level

The pins drive 3.3 V, from IOVDD, the RP2350's GPIO supply, 3.3 V on this
board. SIG and AUX are GP0 and GP1. On the RP2350A of this board both are
"Digital IO (FT)" pins, fault tolerant (RP2350 datasheet, Table 1674): they
tolerate voltages up to 5.5 V while IOVDD is at 3.3 V, and with IOVDD at
0 V very little current flows below 3.63 V. So SIG and AUX take up to 5.5 V
while the board is powered and up to 3.63 V while it is off. That matters
for KST, where the servo drives the line in its reply
([kst-programming.md](kst-programming.md)), and for KISS telemetry, which
the ESC drives.

GP28 and GP29 are "Digital IO / Analogue" pins, the ADC inputs GP26 to GP29
of the RP2350A, and not fault tolerant. They are not offered for SIG or
AUX. They carry only the switched servo supply's enable, a 3.3 V output
([kst-programming.md](kst-programming.md#switched-servo-supply)).

Whether a given servo reads 3.3 V as high is not recorded per servo. Standard
servo inputs are commonly specified for 3.3 V logic.

### Wiring

The servo's power comes from its own supply. The board supplies signal and
ground only. For KST and Hitec programming the supply can be a converter
whose enable the board switches on GP28 or GP29
([kst-programming.md](kst-programming.md#switched-servo-supply)); the board
still feeds the servo nothing.

```
  servo supply, 4.8 V to 6.0 V (8.4 V for an HV servo)
     +  ----------------------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo A signal (orange or white)
  board AUX  ------------------------------------------  servo B signal, two-servo mode only

  SIG GP0 = J4 pin 9 or J5 pin 2, AUX GP1 = J4 pin 10 or J5 pin 1,
  GND = J4 pin 1 or 5, or J5 pin 4
```

- The servo's middle wire goes to its supply, never to a connector pin.
- The board's 5 V pin is its USB supply. A servo can draw more than 1 A at
  stall; do not feed it from the board.
- Power the board before the servo supply, and switch the servo supply off
  before the board. A servo fed while the board is off can drive its signal
  wire into an unpowered pin, rated 3.63 V.
- The two grounds are joined once, at the board.

## Testing

`test/test_claim.cpp`, built. `escTaskPoll()` makes its decisions with pure
functions in `esc_task.h`: `escHandoverStep()` (run, drain or teardown),
`escDrainSlot()`, `escPumpSlot()`, `escPumpFrameSent()`, which stamps the
last throttle frame, and `escPumpBind()`, which disarms. The suite runs those
functions one 1 ms slot at a time, as `escTaskPoll()` calls them, and the fake
pump in `test/fakes.cpp` is built on them too. The claim runs against that
fake pump and against fake jobs that the test puts in the job table of
`core1_job.cpp`, which it includes for that. The test plays core1's side by
calling `fakePumpPoll()` and `core1JobPoll()` itself, so it controls the order
of the two cores' steps:

- refusal per rule: armed, busy, ESC answering within 500 ms and quiet at
  500 ms, KISS live with AUX asked and ignored without, SIG and AUX on one
  GPIO with AUX asked and ignored without;
- the drain: the teardown 251 ms after the last throttle frame, after 250
  zero frames with no command and no KISS request, for a suspend and for a
  rebuild; none needed after a long disarm or with no throttle since the
  bind; the bind starts disarmed at throttle zero;
- order: DShot released before any job bind; job idle before
  `escTaskResume()`; the unplug page before restore for SERVO and KST and not
  for AM32;
- BACK in every state, including `Stopping`, `Restoring` and a failed bind;
  RETRY after a failed bind; Busy while `Restoring`;
- restore: the pins, bitrate and KISS settings handed to the rebuild equal
  those before the claim; the pump starts disarmed;
- the heartbeat rule per job, with the clock across a `millis()` rollover.

The AM32 tests in `test/test_ui.cpp` pass with the AM32 screen on the
claim, and `test_claim.cpp` drives that screen through the UI as well: the
latched GPIO reaches the transport, BACK restores DShot with no unplug page,
and opening AM32 100 ms after a throttle frame waits out the drain. The
unplug page is tested as the claim's rule; the SERVO screen's page is tested
in `test/test_servo.cpp`, with the servo job as it ships. `test_claim.cpp`
puts fake jobs in the job table, and removes them to test a job with no entry,
as KST ships. What `escTaskPoll()` keeps is the
I/O around those functions: the PIO calls, the KISS receiver and the shared
state. It is compiled only.

## Slices

Three slices here, and between slices 2 and 3 the slices A to C of
[menu.md](menu.md#slices). Each merges on its own and leaves the firmware
working. Slices 1 and 2 are built.

| Slice | Content | Depends on | Effort |
|---|---|---|---|
| 1. Claim | `pin_claim.*`, `core1_job.*` with no jobs yet, the drain and `escTaskKissPin()` in `esc_task.*`, `loop1()` in `main.cpp`, AM32 onto the claim, the CFG rows with SERVO and KST drawn disabled, stub `ui_servo.cpp` and `ui_kst.cpp`, per-feature source lists `src/servo.cmake`, `src/kst.cmake`, `test/servo.mk`, `test/kst.mk`, board `static_assert`s, `test_claim.cpp` | the DShot driver port | 2 to 3 days |
| 2. Servo tester | everything in [servo-tester.md](servo-tester.md) | slice 1 | 5 days |
| A. Widgets | [menu.md](menu.md#slices): `ui_list` and `ui_hold`; the AM32 list and SERVO SET on them | slice 2 | 2 days |
| B. Control ids | menu.md: control tables; the UI tests by control id | slice 2 | 2 days |
| C. MENU and SETTINGS | menu.md: the MENU list, the sectioned SETTINGS list, the 36 px header, SD LOG BACK to MENU, the `PinUser` table | slices A and B | 4 to 5 days |
| 3. KST programming | everything in [kst-programming.md](kst-programming.md), the switched servo supply and its SETTINGS row included | slices 1 and C | 10 to 12 days, hardware bring-up not counted |

Slice 1 cannot run beside the DShot port: both edit `esc_task.cpp`, `main.cpp`
and `CMakeLists.txt`. Slices 2 and 3 can run in parallel after slice 1; by
menu.md decision 4, slice 3 follows menu.md slice C. Slice 1
creates the files they would otherwise both edit: the stub screens
`src/ui_servo.cpp` and `src/ui_kst.cpp` with their headers, which each slice
replaces whole; one source list per feature, `src/servo.cmake` and
`src/kst.cmake`, which `CMakeLists.txt` includes, and `test/servo.mk` and
`test/kst.mk`, which `test/Makefile` includes; and one stub suite per feature,
`test/test_servo.cpp` with `runServoTests()` and `test/test_kst.cpp` with
`runKstTests()`, which `test/test_main.cpp` runs. `ui.cpp` opens a tool from
its MENU row, and a row whose claim is refused reads `PINS BUSY`. Each slice adds one line to the job table of
`core1_job.cpp`. Slice 3 otherwise edits its own files, except for these
shared files ([work breakdown](kst-programming.md#work-breakdown)): for the
screen, step 4, `src/ui_registry.cpp`, one line; for the switched supply,
step 5, `src/settings.*`, `src/main.cpp`, `src/sdio/sd_host.{h,c}` and
`src/sdio/sd_pio.c` (the wait hook), `docs/design/sdio-driver.md`,
`test/test_settings.cpp`, `test/test_ui.cpp`, `test/test_sdio.cpp`,
`docs/settings-preview.png` and the wiki's SETTINGS sections; and for the
documentation, steps 5 and 6, `README.md`, `STATUS.md`, `CHANGELOG.md` and
the wiki. Slice 2 also changes these shared files:

- `src/core1_job.h` and `.cpp`: the `init` member of `Core1JobOps`,
  `release` taking the time and returning whether the pins are let go, and
  `core1UiAlive()` reading the heartbeat stamp before `millis()`;
- `src/esc_task.h` and `.cpp`: `escUiAlive()` counting a stamp less than
  250 ms newer than the clock as alive, and the pump reading the stamp first;
- `src/gfx.cpp` and `gfx.h`: the line and arc primitives;
- `src/main.cpp`: the servo job and headers in its overview;
- `test/Makefile`: a generic rule for each feature's C sources and C test
  programs (`SERVO_CSRCS`, `SERVO_CTESTS`; `KST_CSRCS` and `KST_CTESTS` when
  slice 3 needs them);
- `test/fakes.*` and `test/stubs/hardware/`: the PWM register model and the
  pull-down;
- `test/test_claim.cpp` (the release contract and the heartbeat rule),
  `test/test_gfx.cpp` (the primitives) and `test/test_ui.cpp` (the MENU row);
- `tools/notices.py` (the `lib/rcbench/` rule), `tools/coverage.py` (its
  scope text), `tools/make_previews.py` and `tools/check_wiki.py` (the servo
  images and page pair), and the copied headers' include paths in the config
  permutations of `ci.yml`;
- `docs/design/kst-programming.md`: the owner's KST decisions 1 to 4;
- `docs/settings-preview.png` and `docs/setup-preview.png`: regenerated,
  since MENU shows SERVO TESTER live.

The slices share three documentation files, each adding its own section:
`README.md` (slice 3 also adds the legal notice), `wiki/_Sidebar.md` and
`wiki/The-Screens.md` with its German counterpart. Each also updates its
entries in `STATUS.md`, `CHANGELOG.md`, `docs/design/README.md` and
`wiki/Home.md`; slice 2 also names its STOP buttons in `wiki/First-Run.md` and
`wiki/Erste-Schritte.md`. Each slice's user pages are new wiki files of its
own. The second slice to merge resolves those sections by hand.

## Decisions

Decisions of the owner, 2026-10-17: each recommendation is accepted. The
numbers stay, so the references above keep pointing at them.

1. **Heartbeat loss during a KST write.** The operation finishes, and nothing
   new starts until the heartbeat returns. An abort after the first write
   frame locks the session and can leave the servo between images; a write
   commands no motion that the heartbeat could stop.
2. **CFG layout.** Superseded by [menu.md](menu.md#decisions) decisions 1
   and 2. The BEEP row splits into BEEP and SD LOG, 104 px each. The
   destination row holds AM32, SERVO, KST and SETUP at 50 px each: 4 × 50 px
   plus 3 × 4 px gaps fill the 212 px between the margins.
3. **ESC-answering wait.** The claim follows automatically 500 ms after the
   ESC goes quiet. On SERVO the claim sends nothing: pulses need HOLD TO
   START. On KST the search starts at once, as the AM32 search does.
4. **Unplug page.** A tap dismisses it. The page guards against forgetting,
   not against a slip of the finger, and BACK beside it undoes nothing
   dangerous.

Revision of 2026-10-18, after the owner's menu decisions
([menu.md](menu.md#decisions)): decision 2 is superseded. The MENU list of
menu.md decision 1 replaces the CFG screen and its destination row, and BEEP
becomes a MENU row (menu.md decision 2). Slice C of menu.md is built: the
CFG screen and its destination row are gone.
