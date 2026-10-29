# Hitec servo programming

Status: work breakdown steps 1 to 5 built, host tests only: the rcbench
files and their tests, the core1 job on them, the files on the card, the
HITEC SERVO screen on MENU, and its README section and wiki pages. Step 6,
the bring-up on hardware, is open; CLASSIC stays disabled until it. The
decisions of the owner of 2026-10-23 and 2026-10-28 set its questions and
its scope
([decisions of the owner](#decisions-of-the-owner)): port rcbench's Hitec
module at commit 3e29dc1b, and write. The design rests on rcbench's session
(`hitec_session`) and register model (`hitec_reg`), not on single
transactions. Writes have KST's safeguards: a backup into the ring of
`BACKUP/` before a session's first write, a record of each write under way,
read-back verification, and holds to write. No write plan of rcbench's
session or panel has run on a servo
([what has run upstream](#what-has-run-upstream)), so bring-up runs one write
with read-back per family before any write is relied on. The classic family
depends on the switched servo supply, step 5 of
[kst-programming.md](kst-programming.md#work-breakdown), which is built. The
design is a user of the pin claim of [pin-claim.md](pin-claim.md), and is
written for the system the KST work of
[kst-programming.md](kst-programming.md) leaves after its step 5: MENU with a
KST SERVO row, the switched servo supply with its enable on GP28 or GP29,
the card phase, the shared
list `ui_list`, the shared hold `ui_hold`, the 36 px header, the ring of
[backups.md](backups.md) and the write cache of
[sd-write-cache.md](sd-write-cache.md).

## Terms

Hitec is a servo brand; the name is used only to say which servos this
works with. A classic servo is one of Hitec's HS-5xxx and HS-7xxx series, a
D series servo one of its D series. Both program over the servo's signal
wire, half duplex, as UART-style characters: a start bit, 8 data bits least
significant first, a stop bit (8N1). UART is a universal asynchronous
receiver-transmitter. PIO is the RP2350's programmable input/output block,
SM a PIO state machine, DMA direct memory access, FIFO first in, first out,
RAM random-access memory, ESC electronic speed controller, DShot the digital
ESC protocol, BEC battery eliminator circuit (an ESC's regulated output), PWM
pulse-width modulation, SD Secure Digital, SDIO Secure Digital input/output,
FAT File Allocation Table, USB Universal Serial Bus, UI user interface, CI
continuous integration, EEPROM electrically erasable programmable read-only
memory, GPIO general-purpose input/output, ADC analogue-to-digital converter,
ID identifier, CRC cyclic redundancy check, LF line feed, ASCII the American
Standard Code for Information Interchange, CW clockwise and CCW
counterclockwise. DC-DC is direct current to direct current, EN a
converter's enable input. IOVDD is the RP2350's GPIO supply, 3.3 V on this
board. SIG is the ESC signal pin as [pin-claim.md](pin-claim.md) defines it.
Open drain means the pin drives low or lets go and never drives high;
push-pull means it drives both levels. PULL-UP and PRECHARGE are rcbench's
two ways to receive a D series answer: a resistor that lifts the released
line, or the pin itself lifting it before each sample
([electrical](#electrical)). The IOmcu is rcbench's input/output
microcontroller, which runs its Hitec transactions; the PD mini is
rcbench's USB PD (Power Delivery) supply module. The HPP-21 Plus and the
HFP-30 are Hitec's programmers for the classic and the D series. SDHC and
SDXC are the SD High Capacity and SD eXtended Capacity card classes, exFAT
the Extended File Allocation Table file system, KiB 1,024 bytes and GiB
1,073,741,824 bytes.

Source: rcbench, commit 3e29dc1b, under the MIT (Massachusetts Institute of
Technology) licence, same holder: the core in
`protocols/hitec` (5 C files and 5 headers, 5,800 lines: wire, PIO
arithmetic, precharge receiver, register model, session), the RP2350 driver
`protocols/hitec/rp2350/hitec_line.{h,c,pio}` (1,165 lines), the module's
`README.md`, and, as patterns only, the HITEC link page
`shared/outputs/hitec_port.c`, the panel model `shared/bench/hitec_prog.c`,
the panel `shared/ui/hitec_panel*.c`, `docs/HitecProgrammer.md` and
`docs/LinkHitec.md`. The servo file and record texts come from
`shared/bench/hitec_card.c` ([decision 10](#decisions-of-the-owner)). The KST
files in `lib/rcbench/` stay at commit 453cf334.

## What it does

On SIG, with DShot released, for the family the user chose before the
claim:

1. enters programming mode through the session: classic by the switched
   servo supply, with the entry rcbench records; D series with a power cycle
   when the switched supply is set, else on the supply as it is
   ([entry](#entry));
2. reads the family's full set: 100 classic cells or 21 D series registers,
   and keeps the first full read of the session as its backup;
3. identifies the model and shows the named parameters of rcbench's
   register model, 20 D series and 14 classic, decoded with their stored
   value beside them;
4. edits the writable ones within their encodings, in a draft on core0;
5. writes the draft through a 1,000 ms hold, as a plan of `hitec_plan()` run
   by `hitec_session_write()`, and verifies it by read-back. Before the
   session's first write the backup goes into the ring of `BACKUP/`; before
   every write a record of it goes to `HITEC/HITEC.TXT`;
6. saves and loads servo files on the SD card, restores the session's
   backup, and for a classic servo loads the factory values and repairs the
   checksum, each through the same write.

### Not in this design

- An address dump, a range read (`hitec_session_read_set()`) and the
  classic position read (`0x65`). rcbench's panel has none of them.
- The data memory command `0x64`; rcbench's panel sends none.
- The D series factory reset (`hitec_d_factory_plan()`); rcbench's panel
  does not offer it.
- PULL-UP receive and AUX ([decision 4](#decisions-of-the-owner),
  [choice 1](#choices-made-in-this-design)).
- Voltage selection: the converter of KST decision 16 gives 5 V nominal;
  rcbench programs at 4.8 V to 6.0 V.

### What has run upstream

rcbench's records at 3e29dc1b, quoted from the files:

- The module README: "On a wire this code has run D series reads on a
  D485HW, answered with the pull-up under Line levels; nothing else of it
  has." It also reports 153 D series reads by PRECHARGE on GP13 of the
  IOmcu at the module's precharge values (2026-10-20), and 133 of 133 reads
  at other values.
- The module README: "No plan of this module has run against a servo." and
  "No operation of this module has run against a servo."
- `hitec_line.h`: "Not measured: no byte of this driver has been on a pin."
- `docs/HitecProgrammer.md`: "Classic writes through the IOmcu are run on
  one servo, an HS-7955TG (station, 2026-10-25): cell 0x02 and the balance
  0x1C, kept over a power cycle." Every other cell, a write to a servo with
  a wrong checksum and REPAIR CHECKSUM are not tested on a servo.
- `docs/HitecProgrammer.md`: "writes from rcbench to a D series servo are
  tested in the host model only." The register model marks D series ID and
  smart sensor as "Written through the rcbench IOmcu and read back"; no date
  or log of such a write is found, so the two records disagree.
- The classic entry follows IOmcu console runs of 2026-10-20 (POWER, the `+`
  answered, `67`); the session's own entry has not run.
- An HS-7955TG, an HS-5955TG and an HS-5645MG "enter serial mode and answer
  every read on an open-drain port" at a 5.0 V supply.

In short: one classic write, through the IOmcu console, on one HS-7955TG.
No write plan of rcbench's session or panel has run on a servo. D series
writes from rcbench are host-model only. None of it ran on SIG or on this
board.

## Wire

From rcbench's module README. The character formats, frames and command
bytes are measured on an HS-5645MG, an HS-7955TG and a D485HW, with the
HPP-21 Plus and the HFP-30, at a 1.4 V threshold.

| | Classic | D series |
|---|---|---|
| Bit rate | 19,200 bit/s, 52.08 µs a bit | 115,200 bit/s, 8.68 µs a bit |
| Polarity | the line idles high, a start bit is low | inverted: inside a frame the line rests low, a start bit is high |
| High level | made by the servo, through about 1 kΩ | made by a pull-up, or by the master's precharge ([electrical](#electrical)) |
| Request | 4 bytes `cmd p1 p2 sum`, summing to 0 modulo 256: `61` read a cell, `62` write a cell, `67` the first command | read 5 bytes `96 ID addr 00 sum`; write 7 bytes `96 ID addr 02 lo hi sum`; `sum` is the low byte of the sum of every byte after `96` |
| Answer | 2 bytes: value and `+` to a read, `+ +` to a write; starting 508 µs to 517 µs after the start of the master's last byte after reads, up to 614 µs after writes and after the first read of a session | to a read, 7 bytes `69 ID addr 02 lo hi sum`, starting 5.9 ms to 6.5 ms after the request with the HFP-30, 5.93 ms to 6.52 ms over 153 reads by PRECHARGE; a write is not answered |
| Memory | 256 EEPROM cells; a value of 2 cells is high byte first; cells 0x00 to 0x1C sum to 0 modulo 256, 0x1C the balance | 16-bit registers at even addresses, low byte first; a write goes to RAM, `0x70 = 0xFFFF` saves, `0x46 = 1` reboots |

The session sends every D series frame with ID 0x00 and takes no ID
([decision 5](#decisions-of-the-owner)). The answer's ID byte is reported
and not checked. The module README lists a D series ID other than 0 as not
measured, and "ID 0 reaches every servo on the line: two D series servos on
one port both take a plan". One servo on the lead is the wiring rule.

Session timing, from `hitec_session.h` and `hitec_reg.h`:

| | Value | Source |
|---|---|---|
| Tries of a transaction with an answer | 3 | `HITEC_SES_TRIES` |
| Gap after a classic transaction | 2 ms | `HITEC_SES_C_GAP_MS`, not measured |
| Gap after a D series transaction | 20 ms | `HITEC_SES_D_GAP_MS`, the HFP-30's write spacing |
| D series save to reboot | 100 ms | the HFP-30's |
| D series reboot | 1,000 ms | third-party observation; the HFP-30 reads again 834.6 ms after it |
| Classic entry: first `+` | a window of 2,000 ms from the switch-on | `HITEC_SES_C_ENTRY_WINDOW_US` |
| Classic entry: second `+` | a window of 1,000 ms; measured 250 ms to 267 ms after the first | `HITEC_SES_C_NEXT_WINDOW_US` |
| Supply off time of a POWER step | 500 ms by default, 0 ms to 10,000 ms | `HITEC_SES_OFF_MS`, `HITEC_SES_OFF_MS_MAX` |

Every result starts the gap of the step that ended, a cut or failed one
included, and the gap carries into the next operation.

Times computed from the tables, not measured:

- A classic read of one cell is about 3.6 ms, plus a gap of 2 ms to 3 ms on
  a millisecond clock. The full read of 100 cells takes about 0.6 s. A cell
  that does not answer waits out the 1.5 ms window 3 times.
- A D series read is 7.2 ms to 7.8 ms, plus the 20 ms gap. The full read of
  21 registers takes about 0.6 s. A register that does not answer waits out
  the 25 ms window 3 times.
- A D series write of one setting: 1 to 5 register writes 20 ms apart, the
  save, 100 ms, the reboot, 1,000 ms, and a read-back of the 21 registers:
  about 1.7 s.
- A classic write of one setting: its cells and a balance, each about
  3.6 ms plus the gap, then a read-back of cells 0x00 to 0x23, about 0.2 s.
  A full classic restore of 54 writes takes about 0.55 s. The classic EEPROM
  write time is not measured; the HPP-21 Plus spaces two settings 121 ms to
  128 ms apart, with a read of 0x00 to 0x23 between them.

## Electrical

| | Classic | D series |
|---|---|---|
| Pin | SIG only, GP0 by default | SIG only |
| Drive | open drain; the driver refuses push-pull | push-pull, which PRECHARGE needs (below) |
| Receive | the line released; the servo drives the high level | PRECHARGE: the pin drives high for 100.0 ns of every 1,086.7 ns and samples 113.3 ns after the release; PULL-UP is not offered ([decision 4](#decisions-of-the-owner)) |
| Pull at open and after close | the pad's pull-down, 50 kΩ to 80 kΩ | the pad's pull-up, its input inverted while open |
| Servo supply | the converter, its enable on GP28 or GP29, 5 V nominal (KST decision 16); 5.0 V in rcbench's runs, other supplies not tested | the converter with `SERVO SUPPLY` at `GP28` or `GP29`, else the user's |

**Classic high level.** A classic servo drives the high level itself,
through about 1 kΩ, from a supply of 5.0 V in rcbench's runs. The line
voltage is not measured. If the servo drives its signal wire to its supply,
a 5 V servo puts 5 V on SIG, GP0. GP0 is a fault tolerant pin: it
tolerates 5.5 V while the board is powered and 3.63 V while it is off
([pin-claim.md](pin-claim.md#signal-level)). When the master drives low
against the servo's 1 kΩ, the pad sinks about 5 mA at 5 V. On a classic line
the driver sets no drive strength, so the pad keeps its reset value, 4 mA;
the low level at 5 mA is not specified. This is
[decision 3](#decisions-of-the-owner): until the level is measured, the
classic family does not open on a bench pin.

**D series high level.** The servo pulls the line low and leaves it
released for a high level; it has a 3 kΩ pull-down of its own, reported with
another programmer and not measured. The pad's pull-up alone leaves the line
below the 1.4 V threshold, and the servo answers no read. rcbench offers two
ways to receive an answer:

- PULL-UP: a resistor from the signal wire to a supply. 2.2 kΩ to the servo
  supply is tested on a D485HW. Against the 3 kΩ pull-down that is 58 % of
  the servo supply: 2.9 V at 5.0 V, above 3.6 V from a supply of 6.24 V. Not
  offered here.
- PRECHARGE: no resistor. A second SM drives the pin high for 100.0 ns,
  releases it, waits, and samples it, every 1,086.7 ns at 150 MHz. A servo
  pulling low has pulled the line down by the sample. Not measured: the pad
  current while the precharge meets a servo pulling low, a precharge into a
  servo whose supply is off, a long run at the module's values, and any
  clock but 150 MHz. This board runs at 150 MHz.

**D series writes.** A write has no window and takes no sampler: on a
PRECHARGE line it goes out push-pull, its high bits from the pin, about
1.1 mA into the servo's 3 kΩ. The module README lists writes through a
PRECHARGE line as not measured.

**Ground and supply.** As for the servo tester
([pin-claim.md](pin-claim.md#wiring)): the servo on its own supply or on the
converter, the grounds joined once at the board, the board powered before
the servo supply. The converter, its pull-down and its ground are those of
[kst-programming.md](kst-programming.md#hardware).

**After the claim.** The claim's unplug page applies as for every servo
screen: DShot restarts only on RESTART DSHOT. A D series line closes with
the pad's pull-up on, not the pull-down the other jobs leave
([pin-claim.md](pin-claim.md#restoring-dshot)). The DShot pump puts its own
pull-up on SIG when it binds again, so the difference lasts only until
RESTART DSHOT.

## Resources

During a claim the DShot pump and the KISS (the KISS ESC line's serial
telemetry) receiver have released their
SMs, so pio2 is empty, pio0 holds the backlight's 6 words and 1 SM, and
pio1 holds the SDIO driver's 32 words ([pin-claim.md](pin-claim.md#pio-and-dma-budget)).

| | Words | SMs | DMA channels | Block |
|---|---|---|---|---|
| Hitec line | 30 of 32, one copy per open line | 1 | 2 | pio2; pio0 has 26 words free beside the backlight |
| PRECHARGE sampler | 7 at 150 MHz, 12 at most | 1 | 1 | pio0, beside the backlight |
| SDIO, from the first card initialisation | 32 | 2 | 2 | pio1 |
| Panel | 0 | 0 | 1 | |
| Backlight | 6 | 1 | 0 | pio0 |

With PRECHARGE the line takes 2 SMs and 3 DMA channels. With the SDIO
driver initialised, the backlight and the panel's channel that is 5 of 12
SMs and 6 of 16 DMA channels. No other core1 job runs during the claim. No interrupt and
no PIO interrupt flag is used. `clk_sys` stays at 150 MHz while a line is
open, as rcbench requires: the classic divider is exact (488 + 72/256), the
D series one 0.002 % short (81 + 97/256), and rcbench accepts 0.05 %.

RAM, sizes from the module README with arm-none-eabi-gcc 14.2, statics of
the job on core1:

| Item | Bytes |
|---|---:|
| `hitec_line_t`, 3,712 of it the sampler's buffer | 4,100 |
| `hitec_session_t`, with its image, target and plan | 1,060 |
| the session's backup, a `hitec_image_t` | 291 |
| a capture, `hitec_capture_t` | 140 |

The status block holds the image, the backup and the write's difference set
([the job](#the-job)); core0 holds the draft, one more image.
`hitec_param_set()` holds one image on its stack; it runs on core0.

## Where the session runs

rcbench runs the session on its panel and the transactions on the IOmcu's
HITEC link page, one at a time, over the link. Here core1 is free while
DShot is released, so the session and its transactions run together on
core1, in a core1 job, and core0 talks to it through a command block and a
status block, as the KST job does.

Why core1: the line driver wants `hitec_line_poll()` every 1 ms or faster.
A PRECHARGE receive decodes the sampler's words in each poll, about
29 words a millisecond. Core0 draws every 25 ms and blocks on the card.
Core1 has nothing else to do during the claim.

### The job

`src/hitec_job.{h,cpp}`: `PinUser::Hitec`, a row of `PIN_USERS` with
`Core1Job::Hitec`, the unplug page and the ESC check on, and a row of
`s_ops[]` in `src/core1_job.cpp`.

```c
enum class HitecOp : uint8_t { Enter, ReadAll, Write, Abort, Leave,
                               CardBegin, CardEnd };
enum class HitecJobState : uint8_t { Opening, Entering, EntryEnded, InMode, Busy,
                                     Leaving, Closed };
enum class HitecRefusal : uint8_t { None, State, NoUi, Unknown };
enum class HitecPower : uint8_t { None, Off, Listen, Wait };   // where an entry's POWER is

struct HitecCmd {
    uint16_t      seq;
    HitecOp       op;
    hitec_image_t target;          // Write: the draft, a restore's backup, a loaded file
};

struct HitecStatus {
    uint16_t      ackSeq;
    HitecJobState job;
    HitecRefusal  refused;          // of the last command taken
    uint8_t       family;           // hitec_family_t, set before the claim
    uint8_t       lineError;        // hitec_line_err_t of the open
    uint8_t       result;           // hitec_ses_t of the last operation
    uint8_t       lastRx;           // hitec_rx_t of the last decoded step
    uint8_t       failAddr, tries;
    uint16_t      done, total;      // the running read's progress
    uint8_t       stepIndex, planSteps, planResult;
    bool          inMode, hasImage, hasBackup, unknown;
    HitecPower    power;
    bool          supplyFitted, supplyOn, supplyUsed;
    uint16_t      supplyLeftDs;     // time left of the wait under way, 0.1 s
    bool          stalled, stallWrote, cardPhase;
    uint8_t       diff[HITEC_SES_SET_BYTES];
    hitec_image_t image, backup;
};
```

The fields mirror `hitec_session_t` where it has them. `unknown` is set
while `hitec_image_known()` is false for a writable address of the model or,
on a classic servo, cell 0x1C, so while the image lacks an address a write
needs: a failed write made the
session forget it (`hitec_image_forget()`), and `hitec_plan()` refuses with
`HITEC_PLAN_ERR_UNKNOWN` until a read. The supply fields are those the KST
job's status carries as built ([kst-programming.md](kst-programming.md#work-breakdown),
step 5).

The family is chosen before the claim. `hitecJobSetFamily(family)` writes it
under the job's critical section, on core0, only while no core1 job runs;
the screen calls it, then begins the claim, and the bind reads it and opens
the line for that family. RETRY after a bind error (`pinClaimRetry()`) and
BACK on the unplug page (`pinClaimKeep()`) bind again with the family it
holds. An open failure is a bind error, reported by `pinClaimJobError()` as
`GP0 COULD NOT BE TAKEN (PIO)`, `(DMA)` or `(CLOCK)` with RETRY, as on the
other servo screens. Another family means leaving through the unplug page
and choosing again.

`src/hitec_line_port.{h,cpp}` opens and closes the line on the claimed GPIO:
`hitec_line_open()` with `HITEC_DRIVE_OPEN_DRAIN` for classic,
`hitec_line_open_rx()` with `HITEC_DRIVE_PUSH_PULL` and
`HITEC_RX_PRECHARGE` for the D series. It maps `hitec_line_err_t` to
`CORE1_JOB_PIO`, `_DMA` and `_CLOCK`; `HITEC_LINE_ERR_ARG`, which the port's
fixed arguments never cause, maps to `CORE1_JOB_PIO`. The file is the one the host suite replaces with
a fake.

### Running a step

The session neither touches the wire nor reads a clock. Every pass of
`loop1()` the job checks the heartbeat, takes a new command, and then
drives the session:

1. `hitec_session_next(&ses, nowMs, &step)`, the clock
   `HITEC_SES_CLOCK_MS` from `millis()`.
2. By the step's kind:
   - **XFER.** With no transaction on the line, `hitec_line_start()` with
     `step.xfer`; refused, the result is FAILED. Then
     `hitec_line_poll()` on every pass until it hands over the capture.
   - **POWER.** `servoSupplyOff()` unless the supply is off; the off phase
     ends `step.off_ms` after the last switch-off command, a supply off since
     the bind counting from the bind. Then, only while the heartbeat is
     trusted, `servoSupplyOn()` and `hitec_line_start()` with the step's
     LISTEN in the same pass; with the heartbeat lapsed, the result is
     FAILED and the supply stays off ([heartbeat](#heartbeat-and-lapses)).
     With `SERVO SUPPLY` at `NONE` no POWER step comes
     ([entry](#entry)); one that did would be fed FAILED.
   - **WAIT.** Nothing.
   - **DONE.** The operation has ended; its result is
     `hitec_session_result()`.
3. A capture is decoded by the step's decoder, as rcbench's HITEC page does
   it (`shared/outputs/hitec_port.c`): `hitec_c_decode()` with
   `hitec_c_expect_of()` of the argument, `hitec_d_decode()` with the
   register, `hitec_listen_decode()` with the byte, or none. The result,
   RAN with the decoder's code and value, goes to `hitec_session_feed()`.
4. The status is published under the job's lock when it changed.

Each pass is short: the session never waits, and the line driver returns at
once. One call is not short: `hitec_session_write()` runs `hitec_plan()` to
its end, at most 56 steps (`HITEC_PLAN_MAX`), before any transaction. Its
time and its stack on core1 are not measured; core1's stack is
`PICO_CORE1_STACK_SIZE`, 2 KB ([kst-programming.md](kst-programming.md#where-the-session-runs)).
Step 2 of the [work breakdown](#work-breakdown) takes the frames from
GCC's (the GNU Compiler Collection's) `-fstack-usage` in the Release build.

**Abort.** `hitec_session_abort()`. With no step out the operation ends at
once. With a step out it ends at that step's result:

- a SEND, a request on the wire, runs to its end, at most its 25 ms window,
  and its result is fed RAN, as KST finishes a frame on the wire;
- a LISTEN is cut with `hitec_line_abort()` and fed CUT; the job polls
  `hitec_line_poll()` until the cut capture comes before it starts the next
  transaction, and a capture marked aborted is fed CUT, as rcbench's
  `shared/outputs/hitec_port.c` does;
- a POWER in its off phase is fed FAILED, with the supply off.

The session ends the operation with `HITEC_SES_ERR_ABORTED`.

### Commands and states

| Command | Taken in | What it does |
|---|---|---|
| Enter | Opening, EntryEnded (RETRY) | `hitec_session_enter()` ([entry](#entry)); OK goes to InMode and starts the full read |
| ReadAll | InMode | `hitec_session_read_all()`; READ AGAIN |
| Write | InMode, with a full read and the image known | `hitec_session_write()` to `target`; refused as `Unknown` while `unknown` is set |
| Abort | Entering, Busy with a read | the abort above; a write is not aborted ([choice 6](#choices-made-in-this-design)) |
| Leave | every state but Leaving and Closed | the leave of [leave and claim end](#leave-and-claim-end); a running write finishes first, a read or an entry is aborted |
| CardBegin, CardEnd | Opening, EntryEnded and InMode | the card phase ([card calls](#card-calls-and-the-heartbeat)) |

A command is refused as `State` in the other states. With the heartbeat
lapsed, Enter, ReadAll and Write are refused as `NoUi`. A refusal by the
session itself, `HITEC_SES_ERR_PLAN` for example, is the result, and the job
stays InMode. The command block holds one command; a new one replaces a
command core1 has not taken, except that an Abort or Leave not yet taken
stays and a later command is dropped; a Leave replaces a held Abort. The
release, at BACK, leaves as Leave does from any state.

The full read that follows a successful entry is the job's own, not a
command, so no command can come between them. The first full read after an
entry becomes the backup, and `hasBackup` is set; a re-entry drops it, and
the next full read becomes the new backup, as `kst_session_power_cycled()`
drops KST's.

### Comparison with the KST job

| | KST job (`src/kst_job.cpp`) | Hitec job |
|---|---|---|
| Driving | starts an operation, then calls `kst_session_step()` every pass; the session runs the transactions on its `kst_driver_t` | loops `hitec_session_next()`, runs the step itself, feeds the result with `hitec_session_feed()` |
| Clock | read by the session through its driver | passed by the job on every call, milliseconds |
| Supply | the job and `kst_seek.cpp` switch it around the session; `kst_session_power_cycled()` reports a cycle | POWER is a session step the job runs on the enable pin |
| Abort | `kst_session_abort()` finishes the frame on the wire | the job finishes a SEND, cuts a LISTEN, feeds the result |
| Planning | sliced, 100 units a step (`kst_session_edit()`) | one call inside `hitec_session_write()` |
| Backup | kept by the session | kept by the job: the first full read after an entry |
| Search | repeated entries until a servo answers | none: the entry follows the supply's switch-on |

### Opening and leaving the line

| rcbench page step (`docs/LinkHitec.md`) | Here |
|---|---|
| OPEN: the port out of PWM, 50 ms with no pulse, the line released | the claim's drain: DShot stops, then silence; `hitec_line_open()`, a released input with the family's pull. With `GP28` or `GP29`, `servoSupplyBind()` drives the enable pin low at the bind |
| The page holds the SUPPLY page from OPEN to CLOSE | the job owns the enable pin from the bind to the release; SETTINGS is unreachable during the claim |
| POWER: off, wait for off up to 3,000 ms, off 0 ms to 10,000 ms, on at 4.8 V to 6.0 V, the LISTEN | off, `off_ms` from the switch-off command (no read-back), on at the converter's 5 V, the LISTEN in the same pass |
| CLOSE | the leave and the claim end below; `hitec_line_close()`, `servoSupplyRelease()`, the unplug page |

## Switched servo supply

The supply is the converter of
[kst-programming.md](kst-programming.md#switched-servo-supply), its enable
on GP28 or GP29, driven through `servo_supply.h` under the rules KST's job
follows there. "With a switch" below means `SERVO SUPPLY` at `GP28` or
`GP29`, and "the enable pin" the pin chosen. With `SERVO SUPPLY` at `NONE`
the job touches neither pin.

- **Classic needs it.** A classic servo enters serial mode at a power-up
  ([decision 2](#decisions-of-the-owner)). CLASSIC is drawn disabled, with
  `NEEDS SERVO SUPPLY GP28 OR GP29` under it, while `SERVO SUPPLY` is
  `NONE`.
- **D series uses it when it is set.** With a switch the converter is the
  servo's only supply, so every D series entry runs a POWER step. With
  `NONE` the entry runs on the user's supply as it is.
- **Bind.** `servoSupplyBind()` after the claim's ESC check, as for KST: the
  enable pin driven low, the supply off. The pin is not driven before the
  bind.

### Entry

`hitec_session_enter(&ses, power, KST_SUPPLY_OFF_MS)`, 2,000 ms off, as the
KST search switches the same converter off
([choice 5](#choices-made-in-this-design)):

- **Classic**, with a switch only. POWER: the supply off for 2,000 ms from the
  switch-off command, then on, and a LISTEN of 2,000 ms from the switch-on
  for the servo's `+`, which the line driver answers with `+` (520.8 µs to
  527.3 µs after the start of the servo's byte). Then a LISTEN of 1,000 ms
  for the servo's next `+`, measured 250 ms to 267 ms after the first. Then
  the first command `67 00 00 99`, 3 tries; its answer's value is the
  version. A LISTEN that gets no `+` ends the entry.
- **D series with a switch**, `power` true. POWER: the supply off for 2,000 ms,
  on, a LISTEN of 1 ms that waits for nothing, then 1,000 ms
  (`HITEC_SES_D_ENTRY_WAIT_MS`), then a read of register 0x00, the model,
  3 tries.
- **D series with `NONE`**, `power` false: the read of 0x00 on the supply
  as it is.

The converter's start-up time is unknown until bring-up. The classic
LISTEN's 2,000 ms window and the D series wait of 1,000 ms run from the
switch-on command, so they contain it. When the servo's first `+` comes
after its power-up is not recorded upstream; rcbench's test rig answers at
160 ms.

An entry that fails switches the supply off: no `+`, a `+` followed by
something else, no valid answer to `67`, no answer to the read of 0x00. The
view shows the result and RETRY ([family and entry](#family-and-entry)).

### Leave and claim end

- **With a switch, both families.** LEAVE switches the supply off at once; the
  line stays released. After `KST_SUPPLY_OFF_MS`, 2,000 ms from the last
  switch-off command, the job closes the line and releases the enable pin, and the
  unplug page follows, reading `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` while it
  waits, as KST's close does ([kst-programming.md](kst-programming.md#close-with-the-switch),
  KST decision 18). The session's own leave does not run: for a classic
  servo it would switch the supply on again, and a D series reboot is
  redundant once the supply goes off ([choice 4](#choices-made-in-this-design)).
- **D series with `NONE`.** LEAVE runs `hitec_session_leave()`: the reboot,
  `0x46 = 1`, a write with no answer. Then the line closes and the unplug
  page follows. The servo restarts on the user's supply from its saved
  values.
- **Every claim end** after the supply was on at least once closes the line
  and releases the enable pin only 2,000 ms after the last switch-off command: LEAVE,
  BACK on the stalled view and BACK during an entry alike. BACK switches the
  supply off first if it is on; time spent off before BACK counts.

### Heartbeat and lapses

The supply is on only while the heartbeat is trusted (`core1UiAlive()`),
rcbench's rule and KST's ([kst-programming.md](kst-programming.md#leave-reboot-loss-and-heartbeat-with-the-switch)).
A lapse with the supply on is latched until what runs has ended, as KST's
job does it.

- **During an entry.** The entry is aborted: a POWER's off phase ends with
  the supply never switched on; a LISTEN is cut; the D series wait ends; a
  SEND of `67` or of the read of 0x00 finishes. The supply goes off at once.
- **During a read, a write or a D series leave.** The operation finishes,
  as [pin-claim.md](pin-claim.md#decisions) decision 1 requires. A write plan
  that stopped part way could leave a classic servo with a wrong checksum;
  an HS-7955TG with a wrong checksum draws 0.88 A to 0.89 A under sweep
  (rcbench, one servo, measured once). Then the supply goes off.
- **After the supply went off** the job re-initialises the session
  (`hitec_session_init()`): not in mode, the image empty. The backup in
  memory drops with it; with a card, the ring holds it if the session wrote.
  The job is EntryEnded with `stalled` set, and `stallWrote` when the
  session had written. No operation starts until the heartbeat returns.
  The view shows the stall and RETRY, which enters again and reads the servo
  again.
- **With `NONE`** a lapse lets the running operation end and starts no new
  one. The session stays in mode, and commands are taken again once the
  heartbeat returns.

**A core1 hang** with the supply on leaves it on, as for KST: the firmware
enables no watchdog timer, and a reset or a power-off of the board bounds
it ([kst-programming.md](kst-programming.md#firmware)).

### Card calls and the heartbeat

Core0 calls the card while the Hitec claim holds: the backup and the record
before a write, the record's removal after a verified write, SAVE AS, LOAD,
the FILES listing and RETRY MOUNT. A card call can block core0 past the
250 ms heartbeat. With a switch such a stall counts as a lapse: the supply goes
off and the session's backup in memory drops, the one a first write's card
calls were saving among them.

The Hitec job reuses KST's card phase as built: the mechanism, not its
code ([choice 9](#choices-made-in-this-design)).

- `HitecCardPhase` in `ui_hitec.cpp`, around each scope that calls the
  card, with `SERVO SUPPLY` at `GP28` or `GP29` only. It waits up to 50 ms for a
  command still held to be taken, and gets no phase while one is, so
  `CardBegin` never replaces it. Then it sends `CardBegin`, waits up to
  50 ms for the acknowledge, and sets `sdh_wait_hook` to
  `hitecCardStamp()`. Its destructor sends `CardEnd` and sets the hook back
  to null. Nested scopes share the outermost phase. Any other command closes
  the phase too.
- In a phase the job counts the heartbeat as trusted while
  `core1UiAlive()` holds, or while `escUiAlive()` finds the card stamp within
  250 ms and the phase has been open less than `HITEC_CARD_PHASE_MAX_MS`,
  15,000 ms, KST's `KST_CARD_PHASE_MAX_MS`. The job starts no operation in a
  phase: any other command closes the phase as the job takes it, though the
  heartbeat that command is judged by still counts the card stamp.
- `sdh_wait_hook` is the SDIO driver's one generic hook. Only one core1 job
  runs at a time, so the KST and the Hitec phases never meet.

The write cache of [sd-write-cache.md](sd-write-cache.md) takes the card's
writes off core0's path during the claim
([decision 11](#decisions-of-the-owner)); the reads still block, so the phase
stays.

**The count.** `test/kst_card_count.cpp` runs `src/hitec_files.cpp`,
`src/backup_ring.cpp` and rcbench's `hitec_card.c` over FatFs R0.15 on the
volumes and at the card times of
[kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat), "The
cap", on a full card: a ring of `BACKUP/` that has turned, KST, AM32 and
Hitec backups by turns, and 49 SAVE AS files in `HITEC/`. The counts are
without the write cache, which takes the writes off core0's path.

<!-- hitec-card-count:start -->
| Volume | Blackbox logs | Card phase | Block reads | Block writes | Typical card | SDHC | SDXC |
| --- | ---: | --- | ---: | ---: | ---: | ---: | ---: |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec first write, no old record | 30 | 11 | 0.4 s | 6.0 s | 8.8 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec record of a later write | 15 | 2 | 0.2 s | 2.1 s | 2.6 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec record removed after the verify | 15 | 4 | 0.2 s | 2.6 s | 3.6 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec first write, an old record kept | 43 | 17 | 0.6 s | 9.0 s | 13.2 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec SAVE AS, the 50th file of HITEC/ | 15 | 6 | 0.2 s | 3.1 s | 4.6 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec FILES page | 21 | 0 | 0.2 s | 2.2 s | 2.2 s |
| FAT32, 32 GiB, 32 KiB clusters | none | Hitec LOAD of a file | 10 | 0 | 0.1 s | 1.1 s | 1.1 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec first write, no old record | 30 | 11 | 0.4 s | 6.0 s | 8.8 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec record of a later write | 15 | 2 | 0.2 s | 2.1 s | 2.6 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec record removed after the verify | 15 | 4 | 0.2 s | 2.6 s | 3.6 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec first write, an old record kept | 43 | 17 | 0.6 s | 9.0 s | 13.2 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec SAVE AS, the 50th file of HITEC/ | 15 | 6 | 0.2 s | 3.1 s | 4.6 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec FILES page | 21 | 0 | 0.2 s | 2.2 s | 2.2 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in LOGS/ | Hitec LOAD of a file | 10 | 0 | 0.1 s | 1.1 s | 1.1 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec first write, no old record | 342 | 11 | 3.5 s | 38.8 s | 41.5 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec record of a later write | 77 | 2 | 0.8 s | 8.6 s | 9.1 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec record removed after the verify | 77 | 4 | 0.8 s | 9.1 s | 10.1 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec first write, an old record kept | 417 | 17 | 4.3 s | 48.2 s | 52.5 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec SAVE AS, the 50th file of HITEC/ | 77 | 6 | 0.8 s | 9.7 s | 11.2 s |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec FILES page | 207 | 0 | 2.1 s | 21.7 s | 21.7 s, over the cap |
| FAT32, 32 GiB, 32 KiB clusters | 1000 in the root, an earlier firmware's | Hitec LOAD of a file | 72 | 0 | 0.7 s | 7.6 s | 7.6 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec first write, no old record | 82 | 11 | 0.9 s | 11.5 s | 14.2 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec record of a later write | 35 | 2 | 0.4 s | 4.2 s | 4.7 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec record removed after the verify | 35 | 2 | 0.4 s | 4.2 s | 4.7 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec first write, an old record kept | 107 | 13 | 1.2 s | 14.6 s | 17.9 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec SAVE AS, the 50th file of HITEC/ | 34 | 4 | 0.4 s | 4.6 s | 5.6 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec FILES page | 47 | 0 | 0.5 s | 4.9 s | 4.9 s |
| exFAT, 64 GiB, 128 KiB clusters | none | Hitec LOAD of a file | 23 | 0 | 0.2 s | 2.4 s | 2.4 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec first write, no old record | 82 | 11 | 0.9 s | 11.5 s | 14.2 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec record of a later write | 35 | 2 | 0.4 s | 4.2 s | 4.7 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec record removed after the verify | 35 | 2 | 0.4 s | 4.2 s | 4.7 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec first write, an old record kept | 107 | 13 | 1.2 s | 14.6 s | 17.9 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec SAVE AS, the 50th file of HITEC/ | 34 | 4 | 0.4 s | 4.6 s | 5.6 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec FILES page | 47 | 0 | 0.5 s | 4.9 s | 4.9 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in LOGS/ | Hitec LOAD of a file | 23 | 0 | 0.2 s | 2.4 s | 2.4 s |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec first write, no old record | 1023 | 11 | 10.3 s | 110.3 s | 113.0 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec record of a later write | 227 | 2 | 2.3 s | 24.4 s | 24.9 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec record removed after the verify | 227 | 2 | 2.3 s | 24.4 s | 24.9 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec first write, an old record kept | 1236 | 13 | 12.5 s | 133.2 s | 136.4 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec SAVE AS, the 50th file of HITEC/ | 225 | 4 | 2.3 s | 24.7 s | 25.7 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec FILES page | 613 | 0 | 6.1 s | 64.4 s | 64.4 s, over the cap |
| exFAT, 64 GiB, 128 KiB clusters | 1000 in the root, an earlier firmware's | Hitec LOAD of a file | 214 | 0 | 2.1 s | 22.5 s | 22.5 s, over the cap |

Generated by `make -C test kstcount-write`. CI runs `make -C test kstcount` and fails on drift.
<!-- hitec-card-count:end -->

Against the cap of 15,000 ms at the SDXC times, with no log in the root:
every phase on FAT32 stays under it, the longest the first write that keeps
an old record, 13.2 s; on exFAT that write takes 17.9 s, over it, and the
first write without one 14.2 s. A later write's record takes at most 4.7 s.
With an earlier firmware's 1000 logs in the root, the first writes and the
FILES page pass the cap on FAT32, and every phase passes it on exFAT, up to
136.4 s. As for
KST, a phase past the cap switches the supply off and drops the backup in
memory; the card call goes on to its end on core0.

## Screens

Entry: MENU → HITEC SERVO, one line in `ui_registry.cpp`
([menu.md](menu.md#code)); BACK returns to MENU through the unplug page.
`HITEC SERVO` is 11 characters, 130 px at scale 2, and ends at x 145, before
`PINS BUSY` at x 169. The row follows KST SERVO
([decision 6](#decisions-of-the-owner)). As the fifth row it makes MENU seven
rows, which scroll 0 px to 36 px, as [menu.md](menu.md#menu) designs, so
SETTINGS, the last row, shows 6 px at scroll 0 and takes no tap until MENU
is scrolled.

The KST screen is the model: lists on `ui_list` with 40 px rows at a 42 px
pitch, holds on `ui_hold`, `UI_HOLD_MS`, 1,000 ms, and the 36 px header with
the strip at y 40 and the body from y 62. The screen is `src/ui_hitec.cpp`,
its rows, values, stepper and texts in `src/hitec_view.*`, as
`src/kst_view.*`.

### Family and entry

| View | What it shows | Controls |
|---|---|---|
| Family | CLASSIC (HS-5XXX, HS-7XXX) and D SERIES, with the wiring note of each; no pin is claimed yet. CLASSIC disabled with `LINE LEVEL NOT MEASURED` until bring-up ([decision 3](#decisions-of-the-owner)), and with `NEEDS SERVO SUPPLY GP28 OR GP29` under `NONE` | CLASSIC, D SERIES, BACK |
| Claim | the KST screen's: `CHECKING GP0 FOR AN ESC`, `RELEASING GP0`, the refusals and bind errors | BACK |
| Entry, classic | `SUPPLY OFF 0.5 S`, then `LISTENING FOR THE SERVO` with the window's progress, then `ENTERING`; the amber ESC line below | STOP, BACK |
| Entry, D series | with a switch, `SUPPLY OFF 0.5 S`, then `SERVO STARTING 1.0 S` and the amber ESC line; then `READING THE MODEL` | STOP, BACK |
| Entry failed | `NO SERVO IN SERIAL MODE` or `NO SERVO ANSWERS`, the decoder's word (`SILENT`, `LATE`, `SUM` and the rest of rcbench's 14 codes); `THE SUPPLY IS OFF.` with a switch; with a switch and the classic family, `CHECK THE CONVERTER ON GP28, THE SIGNAL AND GROUND.`, naming the enable pin | RETRY, BACK |
| Reading | `READING 100 CELLS` or `READING 21 REGISTERS`, `37 OF 100` | STOP, BACK |
| Read failed | `READ FAILED AT 0x32`, the decoder's word | READ AGAIN, BACK |
| Stalled, with a switch | `THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF. RETRY POWERS IT UP AND READS IT AGAIN.` Without a card, after a write, in amber, `NO CARD: THE BACKUP IN MEMORY IS LOST.` | RETRY, BACK |

The amber ESC line, in every entry view with a switch:
`AN ESC ON THIS LEAD GETS THE CONVERTER'S OUTPUT ON ITS BEC WIRE.` The
claim's ESC check misses an ESC without bidirectional DShot
([kst-programming.md](kst-programming.md#hardware)).

### Field list

```
y   0 | HITEC SERVO                      [ BACK ]|
   40 | D485HW  GP0  BACKUP HT004          (MODE)|  strip, chip C_GREEN
   62 | MOTION   SPEED            60 % (12)      |  40 px rows, 42 px pitch,
  104 | MOTION   DEAD BAND         STEP 2 (4)    |  viewport 62..215,
  146 | MOTION   SOFT START       20 % (1)       |  on ui_list
  188 | TRAVEL   NEUTRAL          8192           |  cut: 28 px, takes no press
  218 | [ - ]    SPEED 60 % -> 30 %         [ + ]|  editor bar 44 px
  268 | [ HOLD TO WRITE ] [REVERT] [ FILES ]     |  110, 52, 54 px; 46 px tall
  320 +------------------------------------------+
```

- Groups follow rcbench's tabs, rows in rcbench's order:

  | Group | D series | Classic |
  |---|---|---|
  | MOTION | SPEED, DEAD BAND, SOFT START | SPEED, DEAD BAND, RESOLUTION (HS-7xxx) |
  | TRAVEL | NEUTRAL, LEFT END, RIGHT END in counts, DIRECTION | CENTRE in µs, LEFT END, RIGHT END raw, DIRECTION |
  | PROTECT | FAILSAFE, OVERLOAD, SMART SENSOR, SENSITIVITY | FAILSAFE, FAILSAFE POS., OVERLOAD |
  | INFO | MODEL, ID, POSITION, R04, R22, R8A, R8C, RD4, RD6 | MODEL, VERSION, DATE, SERIAL, CHECKSUM |

- Values come from `hitec_param_get()`: the decoded value with its unit and
  the stored value in brackets, `CW (0)`. A stored value that fits no
  encoding shows `RAW 1234` in `C_AMBER`. Units: `%`, `STEP n`, counts as a
  number, `US` for µs (the font has no micro sign), `LIMP` and `OFF` for the
  D series fail-safe, dates as `2019-05-14`.
- Read-only rows (`HITEC_RO`) are drawn in `C_GRID` and are not
  selectable, except CHECKSUM when it reads `BAD`
  ([classic checksum](#classic-checksum)).
- A value the draft holds other than the servo's is marked `*` before the
  name.
- The strip names the model (`hitec_identify()`, `hitec_model_name()`), the
  pin, and the backup file once the first write has saved it,
  `BACKUP IN MEMORY` before. A classic strip adds the version, `V16`.
- A model rcbench has not measured is read and written in its series'
  layout (`hitec_reg.h`), as rcbench's panel does
  ([decision 12](#decisions-of-the-owner)). The write page adds, in amber,
  `MODEL NOT MEASURED: WRITTEN IN THE D LAYOUT.` or `... CLASSIC LAYOUT.`
  above the hold. The measured models are the D485HW, the HS-5645MG and the
  HS-7955TG.
- After a failed write the write button reads `READ AGAIN`; a tap runs
  ReadAll, and HOLD TO WRITE returns once the read has filled the image
  ([write](#write)).

### Editing

The draft is a `hitec_image_t` on core0, a copy of the last full read.
`hitec_param_set()` changes it with the side effects the vendor
programmer writes: a D series DIRECTION mirrors NEUTRAL and both ends, a
classic CENTRE sets both end points to 0x10, a classic DIRECTION swaps them
and takes the fail-safe position from the factory cells 0x49 and 0x4A, and
every classic edit balances cell 0x1C in the draft. A raw value is replaced
by a table value on its first edit; REVERT brings it back.

- − and + step the selected value to the next value its encoding takes,
  in the direction of the value shown, hold to repeat: `hitec_param_ok()`
  says which values exist, so the stepper reaches legal values only. A
  sideways swipe steps by one coarse step per 16 px, an eleventh of the
  range, at least one step, as KST's (`kstCoarse()`).
- Lists (DIRECTION, FAILSAFE, SMART SENSOR, SOFT START, DEAD BAND, OVERLOAD)
  step through their choices. D series FAILSAFE steps LIMP, OFF (`HITEC_FS_LIMP` 0,
  `HITEC_FS_OFF` 1), then positions of 900 µs to 2,100 µs.
- Hard rules, skipped by the stepper: the encoding's range and steps
  (`hitec_reg.c`), and the D series order LEFT END below NEUTRAL below
  RIGHT END.
- Soft rules, rcbench's seven with its texts in upper case, compared with
  the last full read: a value
  that breaks one is taken; the row gets an amber mark and the editor bar
  shows the rule's text. The write page lists them, and its hold confirms
  them.

  | Rule | Text |
  |---|---|
  | SPEED below 30 % | `A SLOW SERVO CAN CAUSE LOSS OF CONTROL OF THE MODEL.` |
  | DEAD BAND below the servo's | `A NARROW DEAD BAND CAN CAUSE BACK-AND-FORTH MOVEMENT AND HEAT.` |
  | OVERLOAD switched off | `WITHOUT OVERLOAD PROTECTION A STALLED SERVO HEATS UP.` |
  | an end point other than the servo's | `END POINT CHANGED. THE SERVO TRAVEL PER STICK MOVEMENT CHANGES TOO.` |
  | NEUTRAL or CENTRE other than the servo's | `THE CENTRE SHIFTS. ONE SIDE MOVES CLOSER TO THE MECHANICAL STOP.` |
  | DIRECTION reversed | `DIRECTION REVERSED. CHECK ALL CONTROL SURFACES BEFORE FLIGHT.` |
  | a fail-safe position more than 300 µs from 1,500 µs | `FAILSAFE POSITION IS FAR FROM THE CENTRE.` |

- D series ID edits the servo's own ID, register 0x32, 0 to 255. The
  session addresses the servo with ID 0x00 whatever the register holds
  ([decision 5](#decisions-of-the-owner)); whether a servo with another ID
  still answers ID 0x00 is not measured, so the row carries the amber text
  `THE BENCH ADDRESSES ID 0. ANOTHER ID IS NOT TESTED.` while the draft
  holds another value.

#### Classic checksum

A classic image whose cells 0x00 to 0x1C do not sum to 0 modulo 256 shows
CHECKSUM `BAD` in `C_AMBER`, selectable, and the editor bar reads
`CHECKSUM BAD. THE SERVO MAY IGNORE PWM AND DRAW HIGH CURRENT.` with a
REPAIR button in place of − and +. REPAIR rebalances the draft
(`hitec_c_rebalance()`); `hitec_plan()` then writes the balance, cell 0x1C,
through the write page. Not tested on a servo upstream.

### Write

HOLD TO WRITE opens the write page rather than writing at once.

```
y   0 | HITEC WRITE                      [ BACK ]|
   40 | 2 VALUES                                 |
   62 | SPEED            60 %  ->  30 %          |
   82 | DIRECTION          CW  ->  CCW           |
  102 | * DIRECTION REVERSED. CHECK ALL CONTROL  |  C_AMBER, 12 px a line,
  114 |   SURFACES BEFORE FLIGHT.                |  6 lines, none below 192
  126 | KEEP THE SERVO HORN CLEAR.               |
  196 | WRITING: DO NOT UNPLUG. / the result     |  3 lines at most
  236 | BACKUP FIRST: BACKUP/HT005.TXT           |  or NO CARD: BACKUP IN MEMORY
  268 | [          HOLD TO WRITE           ]     |  1,000 ms, progress bar
  320 +------------------------------------------+
```

- The page lists the changed parameters from where the servo stood when the
  page opened, in the field list's order, 6 at most and `AND n MORE`.
  A value whose stored value changes and whose text does not carries its
  stored value, `60 % (12) -> 60 % (13)`, as KST's `kstValuePair()`.
- `KEEP THE SERVO HORN CLEAR.` shows on the session's first write and on
  every write that changes DIRECTION, NEUTRAL, CENTRE or an end point, as
  rcbench's panel does.
- The hold confirms every soft rule shown, and, for a loaded file of another
  model, the amber line `FILE OF ANOTHER MODEL: D645MW.`
- The write's steps, in order:
  1. Before the session's first write, with a card mounted: the backup into
     the ring of `BACKUP/`; a record left on the card by an interrupted
     write is moved into the ring first ([storage](#storage)).
  2. The record of this write as `HITEC/HITEC.TXT`.
  3. With the write cache on, the flush: `SAVING THE BACKUP TO THE CARD.`
     and `BLOCKS LEFT: n`; `Write` goes to the job only once the cache reads
     Idle ([sd-write-cache.md](sd-write-cache.md#the-flush-barrier)). The
     page follows KST's as built (`flushStep()`, `recoverFlush()` and
     `cardSwapped()` in `ui_kst.cpp`): a flush that ends Failed stops the
     write with `THE CARD DID NOT TAKE THE BACKUP. NOTHING WAS WRITTEN TO THE
     SERVO.`, and a card whose volume serial changed since the backup gets
     the backup and the record again before the write.
  4. `Write` with the draft: `WRITING: DO NOT UNPLUG.`, then
     `WRITES SENT: 3 OF 5` from `stepIndex` and `planSteps`, then
     `READING BACK`.
  5. The result. `HITEC_SES_OK` with read-back: `2 VALUES WRITTEN AND
     VERIFIED.` in `C_GREEN`; the record is removed.
- The card steps run in a card phase with a switch
  ([card calls](#card-calls-and-the-heartbeat)). A mounted card that takes
  neither the backup nor the record stops the write:
  `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.` No card, or a
  card held by USB, writes with the backup and the record in memory.
- Results, one text per `hitec_ses_t` a write can end with, from a table in
  `hitec_view.cpp` that the tests hold complete, with OK to return to the
  list. A write that ends in error with `stepIndex` equal to `planSteps`
  sent every write: its text is `ALL WRITES SENT; READ-BACK FAILED. READ
  THE SERVO BEFORE A WRITE.` in red, whatever the code but ERR_VERIFY.

  | Result | Text | Colour |
  |---|---|---|
  | OK | `n VALUES WRITTEN AND VERIFIED.` | green |
  | ERR_VERIFY | `THE SERVO DOES NOT HOLD THE VALUES: 0x06 0x1C. READ AND CHECK.` (from `diff`) | red |
  | ERR_WRITE | `CELL 0x06 GOT NO ANSWER. READ THE SERVO BEFORE A WRITE.` | red |
  | ERR_NO_SERVO, ERR_CUT, ERR_LINK, ERR_STEP | `THE WRITE STOPPED AT STEP 3 OF 5. READ THE SERVO BEFORE A WRITE.` | red |
  | ERR_READ | `ALL WRITES SENT; READ-BACK FAILED. READ THE SERVO BEFORE A WRITE.` | red |
  | ERR_ABORTED | not reached by a write, which is never aborted; `NOTHING WAS WRITTEN.` with the code | amber |
  | ERR_ENTRY | not reached by a write; `NOTHING WAS WRITTEN.` with the code | amber |
  | ERR_PLAN | `NOTHING WAS WRITTEN: READ THE SERVO FIRST.` for `HITEC_PLAN_ERR_UNKNOWN` | amber |
  | ERR_NOT_IN_MODE, ERR_BUSY, ERR_ARG | `NOTHING WAS WRITTEN.` with the code | amber |

- A write that ends in error after a plan step went out leaves the session
  with the addresses the servo may have changed forgotten, so the next write
  needs a read. The result's OK returns to the list with READ AGAIN in
  place of HOLD TO WRITE ([field list](#field-list)). The record stays on the card.
- BACK during a write is queued: the page shows
  `LEAVING AFTER THE WRITE ENDS.` and the leave follows the result. The write
  is never aborted from the screen.

### Files

```
y   0 | HITEC FILES                      [ BACK ]|
   40 | CARD  12 FILES                           |
   62 | RECORD (HITEC.TXT)                       |  40 px rows, 42 px pitch,
  104 | HT017                         (BACKUP)   |  viewport 62..229,
  146 | FACTORY VALUES                (SERVO)    |  classic only
  188 | HT011                                    |  on ui_list
  234 | [          SAVE AS HT013           ]     |  228 x 40 px
  278 | [  LOAD  ]       [ HOLD TO RESTORE ]     |  40 px tall
  320 +------------------------------------------+
```

- The order: the record on the card, `HITEC/HITEC.TXT`, first; then the
  Hitec backups of `BACKUP/`, the latest first, each tagged `(BACKUP)`; for a
  classic servo, FACTORY VALUES; then the files of `HITEC/` by name without
  case, the first 64. Every `.TXT` other than the record is listed; a name
  longer than 8.3 (12 characters) is not.
- SAVE AS writes the servo as last read into `HITEC/` under the next free
  `HTnnn`, never over a file; it refuses once `HITEC/` holds 50 `.TXT`
  files, the record left out:
  `HITEC/ HOLDS 50 FILES. REMOVE SOME.`
- LOAD parses the selected file and puts its writable values, raw values
  included, into the draft as pending edits. A file of the other series or
  of another layout is refused: `NOT A FILE FOR THIS SERVO: D SERIES`. A file
  of another model of the same layout loads, and the write page names it. A
  record loads its `before` image. FACTORY VALUES loads the servo's own
  factory block through `hitec_c_factory()`: cells 0x30 to 0x53 copied to
  0x00 to 0x23 and rebalanced. The write then goes through HOLD TO WRITE as
  any edit.
- HOLD TO RESTORE writes the session's backup back through the write page,
  while the servo differs from it.
- With no card mounted, or the card held by a USB host, the page shows
  `NO CARD` or `CARD IN USE BY USB` and RETRY MOUNT (`sdLogRemount()`);
  reading, editing and writing still work.

### Leave

BACK from the field list opens the leave page.

```
y   0 | HITEC SERVO                      [ BACK ]|
   40 | D485HW  GP0  SUPPLY ON                   |
   66 | LEAVE SWITCHES THE SERVO'S SUPPLY OFF.   |  with a switch
   86 | 2 CHANGES NOT WRITTEN.                   |  only with pending edits
  100 | A WRITE DID NOT END VERIFIED.            |  C_AMBER, only then
  214 | [            READ AGAIN             ]    |  228 x 46 px
  268 | [               LEAVE               ]    |  228 x 46 px, a tap
  320 +------------------------------------------+
```

- With `NONE` the first line reads `LEAVE RESTARTS THE SERVO.`
- LEAVE is a tap and runs the leave of
  [leave and claim end](#leave-and-claim-end); the unplug page follows.
- READ AGAIN runs ReadAll and returns to the field list.
- BACK on the page returns to the field list.

## Storage

### Card layout

| Path | What |
|---|---|
| `BACKUP/HTnnn.TXT` | a Hitec backup in the shared ring, `BackupDevice::Hitec`, prefix `HT` |
| `HITEC/HTnnn.TXT` | SAVE AS files; at most 50 `.TXT` files there, the record left out |
| `HITEC/HITEC.TXT` | the record of a write under way |

`HITEC/` sits at the card's root beside `KST/`, `BACKUP/` and `LOGS/`.
rcbench keeps its files in `BENCH/HITEC/CONF`; this board's card holds
no BENCH tree. A file from rcbench's card loads once copied into `HITEC/`.

The files go through FatFs on core0 in `src/hitec_files.{h,cpp}`, under the
mount rule of `src/card_dir.cpp` (`cardMounted()`), in a `CardDirScope`, as
`src/kst_files.cpp` does. `kst_files.cpp`'s current limitations on a failing
card apply unchanged ([kst-programming.md](kst-programming.md#storage)).

### Servo files

The text is rcbench's servo file, written by `hitec_card_file_text()` and
read by `hitec_card_file_parse()` of `hitec_card.c`
([decision 10](#decisions-of-the-owner)):

```text
RCBENCH HITEC 1
series=D
model=D485HW
image=0000...                 21 registers of 4 hexadecimal digits; classic 100 cells of 2
check=1A2B                    CRC-16 (polynomial 0x1021, seed 0xFFFF) of the bytes before
```

A classic file adds `version=XX` before the check line. Lines end with LF
alone, digits are upper case, and the text is at most 320 bytes
(`HITEC_CARD_FILE_MAX`): a D485HW file is 140 bytes, an HS-5645MG file
270 bytes. A text with any other line, a lower-case digit, a carriage
return or a byte after the check line is refused whole, so a file edited by
hand does not load. The image lines hold the addresses of a full read
(`hitec_card_file_addrs()`).

### The backup

Before the session's first write, with a card mounted, the backup goes into
the ring of [backups.md](backups.md#the-ring) as `BACKUP/HTnnn.TXT`, in the
servo file text, with `BackupDevice::Hitec`: `backupClaim()` claims its
number and that of a record to keep in one marker write, `backupPut()`
writes it. The ring
is one sequence of 50 numbers for every device, `BACKUP/LAST` naming the
latest, the oldest written over in place. Later writes of the session save
no backup. A session is an entry and what follows it; a re-entry after a
stall starts a new one ([heartbeat](#heartbeat-and-lapses)).

### The record of a write

Before every write, the record of `hitec_card_text()` goes to
`HITEC/HITEC.TXT`: the series, `port=0`, `pin=00` (GP0), the image before
the write and the target, classic cells 0x00 to 0x23 or the 21 D series
registers, and a CRC-16 check line. A D series record is 234 bytes.

| Situation | Effect |
|---|---|
| First write of a session, card mounted | a record already on the card, from an earlier session, is moved into the ring under the next number (`backupMoveIn()`), then the backup is saved, then this write's record is written; in a card phase with a switch |
| Later writes of the session | this write's record is written over the session's own record; an overwrite that fails removes it |
| Write verified | `HITEC/HITEC.TXT` is removed, when this session wrote it and the file holds the bytes written |
| Write in error | the record stays |
| `HITEC/HITEC.TXT` found when the screen opens | `A WRITE WAS INTERRUPTED. HITEC/HITEC.TXT HOLDS THE SERVO AS IT WAS.` FILES lists it as RECORD; LOAD puts its `before` into the draft |
| No card, or a card held by USB | the write runs; the record and the backup stay in memory and end at LEAVE |

A record moved into the ring keeps its text, so a ring entry with the `HT`
prefix holds a servo file or a record; LOAD reads either
(`hitec_card_file_parse()`, then `hitec_card_parse()`). rcbench's panel
refuses a write while a record stands; here the record is moved aside, as
KST's is, so no record is lost.

## Porting

### Files copied

Copied unchanged from rcbench commit 3e29dc1b into `lib/rcbench/`, at
rcbench's paths:

| rcbench file | Lines |
|---|---:|
| `protocols/hitec/hitec_wire.c` | 449 |
| `protocols/hitec/hitec_pio.c` | 338 |
| `protocols/hitec/hitec_pre.c` | 400 |
| `protocols/hitec/hitec_reg.c` | 1,261 |
| `protocols/hitec/hitec_session.c` | 939 |
| `protocols/hitec/include/hitec_wire.h` | 586 |
| `protocols/hitec/include/hitec_pio.h` | 286 |
| `protocols/hitec/include/hitec_pre.h` | 427 |
| `protocols/hitec/include/hitec_reg.h` | 671 |
| `protocols/hitec/include/hitec_session.h` | 443 |
| `protocols/hitec/rp2350/hitec_line.c` | 790 |
| `protocols/hitec/rp2350/hitec_line.h` | 283 |
| `protocols/hitec/rp2350/hitec_line.pio` | 92 |
| `shared/bench/hitec_card.c`, [decision 10](#decisions-of-the-owner) | 522 |
| `shared/bench/include/hitec_card.h` | 266 |
| `shared/link/link_crc.c` | 39 |
| `shared/link/include/link_crc.h` | 77 |

The core is 5,800 lines, the driver 1,165. The core includes only C
standard headers and files of its folder; the driver adds the Pico SDK's
(software development kit's) `hardware/*`, `pico.h` and the generated
`hitec_line.pio.h`.
`hitec_card.c` includes `hitec_card.h`, `link_crc.h` and `<string.h>`. The
module's `README.md`, whose links point into rcbench, and its two
`CMakeLists.txt` stay out, as for KST. `lib/rcbench/README.md` gains a table
of the Hitec files at 3e29dc1b, and its opening sentence names both commits;
the existing rows keep 453cf334.

Not ported: `shared/outputs/hitec_port.c` (the IOmcu's link page),
`shared/bench/hitec_link.c`, `hitec_prog*.c`, `hitec_card_run.c`, and
`shared/ui/hitec_panel*.c`. Their rules that MiniRcBench keeps are in the
job and the screen: the decode mapping, the order backup, record, plan,
read-back, the soft rules and their texts, the result texts.

### Tests copied

Into `test/rcbench/`, with rcbench's `greatest.h`, which is byte for byte
the copy already there:

| File | Lines | Cases | With |
|---|---:|---:|---|
| `test_hitec_wire.c` | 571 | 11 | |
| `test_hitec_pre.c` | 738 | 18 | |
| `test_hitec_pio.c` | 1,025 | 23 | `hitec_pio_model.h` (718 lines), `fixtures/hitec_line_words.txt` (30 lines), the path defines `HITEC_LINE_PIO` and `HITEC_LINE_WORDS` |
| `test_hitec_reg.c` | 968 | 14 | `hitec_reg_fix.h` (193 lines, bench reads) |
| `test_hitec_reg_plan.c` | 543 | 9 | `hitec_reg_fix.h` |
| `test_hitec_session.c` | 772 | 15 | `hitec_session_rig.h` (352 lines), `link_pages.h`, see below |
| `test_hitec_session_write.c` | 544 | 13 | `hitec_session_rig.h` |
| `test_hitec_card.c` | 518 | 12 | `hitec_reg_fix.h`; tests `hitec_card.c` and `link_crc.c` ([decision 10](#decisions-of-the-owner)) |
| `hitec_headers.cpp` | 55 | 0 | the 5 core headers compiled and linked as C++11 |

That is 115 cases. Counts are `TEST_CASE(` entries at 3e29dc1b.

`test_hitec_session.c` includes rcbench's `shared/link/include/link_pages.h`
for 5 static asserts that hold the session's values to the HITEC link
page's: the 4 decoder numbers (`LINK_HITEC_DEC_NONE` 0, `_C` 1, `_D` 2,
`_LISTEN` 3) and `LINK_HITEC_OFF_MS_MAX`, 10,000. That header is 2,734 lines
of rcbench's link and is not ported. `test/rcbench/link_pages.h` is a
minimal stub of MiniRcBench's own with those 5 constants at their 3e29dc1b
values, starting with
`Adapted from rcbench shared/link/include/link_pages.h, commit 3e29dc1b, MIT.`
The test file stays byte for byte rcbench's, so a directory diff against a
later rcbench shows the stub as the one adapted file. The test's other two
asserts need only the module's headers.

Not copied: the port, link, prog, card-run and panel suites
(`test_hitec_port*.c`, `test_hitec_link*.c`, `test_hitec_rx_link.c`,
`test_hitec_prog*.c`, `test_hitec_card_run.c`, `test_hitec_panel*.c`).
Their code is not ported.

### Licence headers

Every copied file but the fixture carries `SPDX-License-Identifier: MIT`,
the licence tag of SPDX (Software Package Data Exchange).
Six headers carry rcbench's sentence on the Hitec name, "Hitec is a name or
trademark of its owner; it is used here only to say which servos this
module works with": `hitec_wire.h`, `hitec_pio.h`, `hitec_pre.h`,
`hitec_reg.h`, `hitec_session.h` and `hitec_line.h`; `hitec_card.c` and
`hitec_card.h` carry it too. The other C files, the PIO file, the tests,
`hitec_pio_model.h` and `link_crc.*` do not. New and adapted files that hold
Hitec code (`src/hitec_*`, `test/hitec_servo_sim.h`,
`test/rcbench/link_pages.h`) carry that sentence, and adapted ones also
start with `Adapted from rcbench <path>, commit 3e29dc1b, MIT.`, as
[kst-programming.md](kst-programming.md#licence-headers) requires of the
KST files. README.md's "Legal notice and disclaimer" names Hitec beside KST
and Chaservo and states the origin ([decision 7](#decisions-of-the-owner)).

### Build

- `src/hitec.cmake` and `test/hitec.mk`, as `src/kst.cmake` and
  `test/kst.mk`: the sources, the include folders, the PIO header, the C
  tests, the two path defines `test_hitec_pio.c` reads, and the C++ rule for
  `hitec_headers.cpp`, the suite's first rcbench test in C++.
- `hitec_line.c` is not in rcbench's host build either. Here it is compiled
  by the firmware build only, under `-Werror`, as KST's driver is; `make
  syntax` typechecks `hitec_line_port.cpp` with the driver's header.
- CI: the include folders join the config-permutation job's include lists,
  in both places that list them.

## Testing

- rcbench's tests above, run by `make` and CI.
- `fixtures/hitec_line_words.txt` against pioasm's output: the firmware
  build assembles `hitec_line.pio`, and a CI step compares pioasm's hex
  output (`pioasm -o hex`) with the fixture, as rcbench's firmware build
  holds it (`test_hitec_pio.c`, lines 18 to 19).
- `test/hitec_servo_sim.h`: a servo behind the fake line, adapted from the
  servo model of rcbench's `hitec_session_rig.h`, which is written for
  `greatest.h`. It takes each transaction the job starts, answers from a
  memory of 256 bytes with captures built from the wire tables (`+` at the
  entry, value and `+`, `+ +`, the D series 7-byte answer), takes writes,
  saves and reboots, and follows the fake level of the enable pin with a start-up time:
  without supply it answers nothing and leaves the mode. It starts from the
  bench reads of `hitec_reg_fix.h`.
- `test_hitec_job.cpp`: the job from the job table, one `core1JobPoll()` a
  millisecond, on `test/fake_hitec_line.cpp` and the simulated servo, on the
  suite's virtual clock. Cases:
  - the bind with each family and its three refusals; each command in each
    state; commands held at the bind not taken;
  - the classic entry: off 2,000 ms from the switch-off command, on, the `+`
    answered with `+`, the second `+`, `67`, the version; no `+` in
    2,000 ms, then the supply off and EntryEnded; a byte other than `+`;
  - the D series entry with a switch: off 2,000 ms, on, 1,000 ms, the model; with
    `NONE`: no POWER, the read of 0x00 at once, the enable pin untouched through a
    whole claim;
  - the full read after the entry, its progress, the backup set once and
    dropped at a re-entry;
  - a write of one value of each family, with read-back, the image holding
    the servo's values after it; a write with a read-back that differs
    (`diff`); a D series write cut by a lost line after a plan step, then
    Write refused as `Unknown` until ReadAll;
  - Abort: a SEND finished and fed RAN, a LISTEN cut and fed CUT, a POWER's
    off phase never switched on; Abort refused during a write;
  - the heartbeat: a lapse in an entry's off phase, its LISTEN and its D
    series wait each switching the supply off at once; a lapse during a
    write letting it finish, then the supply off, the session not in mode,
    `stallWrote` set; the supply switched on only while the heartbeat is
    trusted; with `NONE`, a lapse leaving the session in mode;
  - the leave: with a switch off at once and closed 2,000 ms after the
    switch-off command, BACK doing nothing in the wait; with `NONE` the
    D series reboot frame, then the close; every claim end after the supply
    was on waiting 2,000 ms off before the line closes and the enable pin is released;
  - the card phase: `CardBegin` taken and refused in each state; stamps
    for 900 ms keep the supply on and the backup; stamps stopped switch it
    off at 250 ms; stamps for 14,900 ms keep it on and for 15,100 ms switch
    it off; with `NONE` the hook stays null.
- `test_hitec_files.cpp`: the servo file and the record over the in-memory
  volume (`test/fake_fatfs.cpp`): SAVE AS up to 50 files, the backup into
  the ring as `HTnnn.TXT`, an old record moved into the ring, a ring entry
  that holds a record loaded as one, the record removed after a verify and
  kept after an error, a file of rcbench's text loaded, files refused for
  each parse error, no card and a card held by USB with no FatFs call.
- `test_hitec_view.cpp`: the rows of each layout in their groups on the
  bench images, each value with its unit and its stored value, `RAW` values,
  the stepper against the encodings and the D series order, the coarse
  step, each soft rule and its text, a text for every `hitec_ses_t` and
  `hitec_rx_t`.
- `test_hitec_ui.cpp`, by control id ([menu.md](menu.md#tests-by-control-id)):
  MENU opens the screen; the family choice, CLASSIC disabled under `NONE`
  and until the line is measured; the claim; the entry and the read; an edit
  and REVERT; HOLD TO WRITE not at 750 ms and not on a slide off; the write
  page, the backup and the record before the first write frame, the flush
  barrier, the result; a write in error and READ AGAIN; FILES with SAVE AS,
  LOAD, a refused file, FACTORY VALUES, HOLD TO RESTORE; REPAIR on a bad
  classic checksum; a model not measured written with the amber layout
  line; BACK in every view and during a write; LEAVE and RESTART DSHOT; a stall and RETRY; no card, a
  card held by USB, a card that takes no file; a record found at entry;
  `checkControlTable()` in each view; a preview, `docs/hitec-preview.png`.
- The changed MENU tests: the row count in `test_menu.cpp` and
  `test_ui.cpp`, `openSettings()`, which scrolls MENU before it taps
  SETTINGS, and `docs/menu-preview.png`.
- Coverage holds `tools/coverage.py`'s floors, 93 % in total and 85 % per
  file, with the README sentence and the STATUS.md table written by it.

## Work breakdown

1. Copy the module, the card text and the CRC, the tests, the
   `link_pages.h` stub; `src/hitec.cmake` with its `include()` in
   `CMakeLists.txt`; `test/hitec.mk` with its `include`, `FEATURE_*` and
   `INCLUDES +=` lines in `test/Makefile`, its include folders in
   the two C test rules and the coverage rule, the C++ test rule; CI include
   lists; `lib/rcbench/README.md`. The firmware links the driver; nothing
   opens it. Built, with a CI step that holds the KST and Hitec line
   fixtures to pioasm's words.
2. `PinUser::Hitec`, `Core1Job::Hitec`, `src/hitec_job.*`,
   `src/hitec_line_port.*` with `$(HITEC_DEVICE_SRCS)` on the `DEVICE_SRCS`
   line of `test/Makefile`, the supply rules, the card phase on the job's
   side, the fake line, `test/hitec_servo_sim.h` and `test_hitec_job.cpp`;
   the stack frames of `hitec_session_write()` on core1 from
   `-fstack-usage`. [pin-claim.md](pin-claim.md) gains Hitec in the enums of
   its application programming interface (API) section, its job and core1
   tables, its heartbeat table, its PIO and DMA budget and every sentence that names the servo and KST screens as
   the claim's users; its Restoring text names the D series pull-up.
   [kst-programming.md](kst-programming.md)'s supply sections and the
   comments of `src/servo_supply.h` name the Hitec job as a user of the enable pin beside
   KST's. Built. As built, and differing from the sections above:
   - The simulated servo is in `test/fake_hitec_line.{h,cpp}`, not in a
     `test/hitec_servo_sim.h` of its own. It takes its memory from
     `hitec_reg_fix.h` unchanged: an HS-5645MG or a D485HW.
   - `hitec_job.cpp` includes `hitec_line.h` for the line's error codes; the
     host suite compiles it with that header and links the fake line.
   - The stack is measured in CI, not here. `src/hitec.cmake` builds the
     job, its line port and driver, the session, the register model, the
     wire layer and `servo_supply.cpp` with `-fcallgraph-info=su` at the
     Release flags. `tools/stack_depth.py` finds the deepest call chain from
     `hitecJobStep()`, `hitecJobRelease()` and `hitecJobBind()` and fails
     above 1,792 bytes, which leaves 256 bytes of core1's 2,048 for
     `loop1()` and `core1JobPoll()` above them; those are not measured. A
     frame of no fixed size or a recursion fails as well. Calls into the C
     library and the SDK count 0 bytes and are listed. On the Cortex-M33 the
     deepest chain is `hitecJobStep()` into `publish()`, 1,080 bytes.
   - `HitecPower` has no `Wait`: the D series entry's 1,000 ms wait is the
     session's WAIT step, reported as `power` None.
   - The first full read that completes before a write is the backup, also
     after a first read that failed or was aborted.
   - With `NONE` a lapse during an entry ends it with RETRY and does not set
     `stalled`; the servo's supply is the user's.
   - `inMode` reads false once a leave with a switch has switched the supply
     off: the servo leaves the mode with its supply.
   - `HitecStatus` carries `lastWrite`, so the screen can tell a write's
     result from a read's.
   - Tested in `test_hitec_job.cpp`, 93 checks, against a servo that is
     powered a start-up time after the enable pin goes high and, for the D series,
     answers 800 ms after power-up: the bind with each family
     and its three refusals, a command held at the bind not taken; the
     D series entry with `NONE` and with a switch, the classic entry with a switch
     and refused without it, a classic entry with no `+` and RETRY; the full
     read, the backup and READ AGAIN, the backup after an aborted first
     read; the classic LISTEN starting at the switch-on with a 300 ms
     converter start-up, the D series read of 0x00 1,000 ms after it; a write
     of each family, a read-back
     that differs, a write cut by a lost servo, `Unknown` until a read;
     Abort in the off phase, in a LISTEN and in a read, refused in a write;
     the heartbeat in the off phase, in a LISTEN, in the D series wait and
     its read of 0x00, in a write of each family, in the mode, and with
     `NONE`; a 300 ms lapse that ends before a read does, then the supply
     off once it ends; LEAVE with a switch and with `NONE`, LEAVE in the off
     phase, in a LISTEN, in a read with `NONE`, from Opening and from the
     stalled view, LEAVE and the release during a write, each close
     2,000 ms after the switch-off; the command block's held Abort; the
     card phase in four states, 900 ms of stamps, the 15,000 ms cap. Not
     tested: an entry answered by a byte other than `+`; a lapse during a
     D series entry with `NONE`, which ends within about 150 ms, before the
     250 ms timeout can pass.

3. `src/hitec_files.*` on `card_dir` and `backup_ring`, `test_hitec_files.cpp`;
   the Hitec card phases added to `test/kst_card_count.cpp` and their table
   to this note; the `kstcount` target and its CI step extended to check
   this note as well as kst-programming.md;
   `src/backup_ring.h`'s Hitec row no longer reserved; [backups.md](backups.md)
   and [sd-write-cache.md](sd-write-cache.md) name the Hitec claim. Built.
   As built, and differing from the sections above:
   - `hitecFilesRecordWrite()` writes a later write's record over the
     session's own in place; with no record on the card it writes a new one,
     and a record of another session stays and stops it (`Exists`).
   - `hitecFilesLoad()` and `hitecFilesBackupLoad()` take a servo file or a
     record and give the image; a file of the other family is refused
     (`OtherSeries`).
   - SAVE AS names run HT001 to HT999, one above the highest `HTnnn` in
     `HITEC/`, a directory of that form included.
   - The FILES list holds every `.TXT` of `HITEC/` other than the record,
     the first 64 by name without case; a name longer than 8.3 (12
     characters) counts toward the 50 and is not listed.
   - The backup goes into the ring through `backupClaim()` and `backupPut()`,
     not `backupSave()`: one marker write claims the kept record's number and
     the backup's.
   - A later write's record whose overwrite fails is removed, so a verified
     write leaves no record of an earlier one; the next write of the session
     creates a new record. A card that refuses the removal too keeps the
     file.
   - Tested in `test_hitec_files.cpp`: no card, both as a card held by USB
     and as the logger not mounted, every call with no FatFs call; SAVE AS,
     its text, LOAD of each family, of an rcbench file name and of the
     record; the list by name without case with names of other forms, a
     full list, the next name, a directory in its way, an edited file, a
     missing file, a file too large, 49 files and the record, 50 files and
     HT999; the first write with and without an old record, the names in the
     ring, the record's images, a later write's record over the session's
     own, of another length, and with none, another session's record kept,
     the removal and a record with other bytes kept, an old record that does
     not move, a record that does not stat or create, an overwrite that
     does not open or fails part way, a removal refused after it, a record that does not read or is too
     long, a removal the card refuses, the backups list, the write allowed
     and stopped.
4. `src/ui_hitec.cpp`, `src/hitec_view.*`, `HitecCardPhase`, the registry
   line, `test_hitec_view.cpp`, `test_hitec_ui.cpp`, the MENU test changes,
   the preview and its entry in `tools/make_previews.py`. Built.
   `HITEC_CLASSIC_MEASURED` in `src/config.h` is 0, so CLASSIC stays
   disabled until bring-up. As built, and differing from the sections above:
   - The classic INFO group has no MODEL row: the register model has no
     classic model parameter, so the strip names the model.
   - The entry views show no progress: `LISTENING` without the window's
     progress, `SERVO STARTING` without its 1.0 s count. The off phase
     counts its 2.0 s in the strip.
   - A change of the classic CHECKSUM counts as a value to write
     (`hitecWritten()`), so a REPAIR alone opens the write page; the write
     page lists the parameters that go to the servo and not the cell 0x1C
     balance of other edits.
   - The strip drops the pin, then the backup, when the line would reach
     the MODE chip at x 197: a classic strip reads
     `HS-5645MG V16 BACKUP IN MEMORY`.
   - The write page shows the ID note in amber when the write changes the
     D series ID to another value than 0, as a LOAD can.
   - A read keeps the pending edits: the writable addresses they changed
     go onto the servo as read, so DIRECTION keeps its mirrored end points;
     a classic draft is rebalanced after.
     A REPAIR alone does not carry over: cell 0x1C is not a writable
     address, so the read shows CHECKSUM `BAD` again and REPAIR is tapped
     again.
   - BACK during a read, the entry's own or READ AGAIN, sends Abort; the
     leave page follows.
   - A stall drops a leave queued by BACK during a write; the main view
     with RETRY follows.
   - A later write whose record the card does not take still runs on the
     backup of the first write, as `hitec_files.h` states; the write page
     does not say so.
   - The MENU tests reach SETTINGS through `menuRow()` in `test/ui_ids.h`,
     which drags MENU until the row is whole (`revealRow()`), not through a
     scroll in `openSettings()`.
   - Tested in `test_hitec_view.cpp`: the rows of the three layouts in their
     groups; each unit's text with its stored value, `RAW` and `?`; the
     stepper at the ends, through FAILSAFE's LIMP and OFF, past a RAW value
     and in the D series order; the coarse step; each soft rule, its row
     filter and its text; a text for every `hitec_ses_t` and `hitec_rx_t`;
     the model texts, layouts and the measured models. In
     `test_hitec_ui.cpp`: MENU opens the screen; the family, CLASSIC
     disabled under `NONE` and until measured; the claim, the entry and the
     read; an edit and REVERT; HOLD TO WRITE not at 750 ms and not on a
     slide off; the write page, the record on the card before the first
     write frame, the backup, the result, a second write without a second
     backup; the flush barrier, a flush that fails, a later write while an
     earlier batch flushes with its record on the card before its first
     frame; BACK during a write; a stall during a write; the ID note;
     STOP and BACK in the entry, BACK in the entry's read, BACK on the
     unplug page; an edit kept through READ AGAIN with FILES opened during
     the read, and a DIRECTION reversal written after it with its ends
     mirrored; a sideways swipe on NEUTRAL; a write in error and READ AGAIN; FILES
     with SAVE AS, LOAD, a file of the other family refused, HOLD TO
     RESTORE; no card, a card held by USB, a card that takes no file; a
     record found at entry and moved into the ring; a classic servo on the switched supply
     with REPAIR written and FACTORY VALUES loaded; an HS-5955TG, a model
     not measured; a stall and RETRY; BACK in each view; LEAVE and RESTART
     DSHOT; another card in the slot since the backup, before a write and
     during a flush wait; `checkControlTable()` in each view. Not tested on
     the screen: a volume serial that never reads.
5. README section, wiring with the warning that an ESC on the lead gets the
   converter's output on its BEC wire and that one servo goes on the lead,
   legal notice and MENU table row; wiki pages `Hitec-Programming.md` and its
   German counterpart `Hitec-Programmierung.md`, the pair in
   `tools/check_wiki.py`, their entries in `_Sidebar.md`, `Home.md` and the
   screens pages; STATUS.md and CHANGELOG.md. Built. The README's trademark
   notice for Hitec does not say "no vendor documentation": some register
   meanings in rcbench's register model rest on published documentation or
   third-party observation (`hitec_src_t`).
6. Bring-up, before a write is relied on:
   - the classic line's high level on a scope, the servo's signal lead on a
     68 kΩ resistor to ground and not on the board, while Hitec's
     programmer or the converter makes the servo drive the line; CLASSIC
     stays disabled until it is measured ([decision 3](#decisions-of-the-owner));
   - the converter's checks of KST's bring-up, which they share
     ([kst-programming.md](kst-programming.md#work-breakdown), step 7);
   - the classic entry on the owner's servos: the `+` after the switch-on,
     the second `+`, the answer to `67`, after the 2,000 ms off time; the
     converter's output decay to 0 V on a scope, against that off time;
   - the D series entry with a switch: the model read after 1,000 ms; a read by
     PRECHARGE, the answer delay against 5.93 ms to 6.52 ms;
   - the full read of each family against what Hitec's programmer shows;
   - one write with read-back per family: one value written, the read-back
     verified, the servo power-cycled and read again, and the value checked
     with Hitec's programmer; then a restore of the backup that changes at
     least three settings in one plan, so the 2 ms spacing of classic
     writes and the balance after each setting run once, read back,
     power-cycled and read again.

About 11.5 days for 1 to 5, not measured. Bring-up depends on the servos
and is not estimated.

## Decisions of the owner

Decisions of the owner, 2026-10-23, questions 1 to 7, on the note's first
version, written against rcbench 4a437488. Decision 1 is replaced by
decision 9. Decisions 2 and 5 are updated to rcbench 3e29dc1b on 2026-10-28,
as the owner's decision 8 requires where rcbench's recorded behaviour
contradicts them; the others stand as decided.

1. **Writes.** None: reads, position and dumps only. Replaced by decision 9.
2. **Classic entry.** By the switched servo supply of
   [kst-programming.md](kst-programming.md#switched-servo-supply): off, on,
   and a listen for `+` from the switch-on. Not chosen: `0x67` alone with
   the supply on, and the user powering the servo up by hand during the
   listen. Updated to rcbench 3e29dc1b: the supply off, on, a LISTEN
   of 2,000 ms whose `+` is answered with `+`, a second LISTEN of 1,000 ms
   for the servo's next `+`, then `67 00 00 99` with 3 tries
   ([entry](#entry)). At 4a437488 the entry was not recorded upstream, and
   whether the servo needed an answer byte was not known.
3. **Classic line level.** Measured at bring-up before CLASSIC is enabled;
   a protecting lead, if the measurement asks for one, is designed from that
   measurement. A lead has limits: a series resistor R makes the master's
   low reach the servo as V × R / (R + 1 kΩ), 2.5 V at R = 1 kΩ and 5 V,
   which may read as high; a clamp to 3.3 V feeds an unpowered board
   through the clamp. rcbench at 3e29dc1b still records the classic line
   voltages as not measured.
4. **D series receive.** PRECHARGE only. PULL-UP is not offered: 2.2 kΩ to
   the servo supply puts more than 3.6 V on the line from a 6.24 V supply,
   and a value to 3.3 V is not tested.
5. **D series ID.** Decided as a stepper, 0x00 to 0xFF, 0x00 by default,
   not stored. Updated to rcbench 3e29dc1b: `hitec_session.c` sends every
   D series read and write with ID 0x00 and has no ID argument, and the
   copied session stays unchanged. The request ID is 0x00 and there is no
   stepper. The servo's own ID, register 0x32, is an edited parameter
   ([editing](#editing)).
6. **MENU row.** HITEC SERVO follows KST SERVO; SETTINGS becomes the
   seventh row, which takes a tap after a scroll.
7. **Origin wording.** The legal notice states rcbench's origin claim for
   the Hitec code: wire captures of the project's own hardware and Hitec's
   published D series manual; no vendor code and no vendor document is
   included. Confirmed by the owner. rcbench's module README states the same
   at 3e29dc1b.

Decisions of the owner, 2026-10-28:

8. **The port.** rcbench's Hitec module at commit 3e29dc1b: wire, pio,
   pre, session, reg, and the RP2350 line driver. The design rests on
   rcbench's session (`hitec_session`) and register model (`hitec_reg`),
   not on single transactions.
9. **Writes.** As rcbench's panel has them: the named parameters edited, and
   write plans with read-back, with KST's safeguards: a backup into the
   shared ring of `BACKUP/` before a session's first write
   ([backups.md](backups.md)); a record of the write under way; read-back
   verification; holds to write, as KST. This replaces decision 1 of
   2026-10-23. No write plan of rcbench's session or panel has run on a
   servo: one classic write through the IOmcu console on one HS-7955TG is
   recorded, and D series writes from rcbench are host-model only.
   Bring-up therefore runs one write with read-back per family before any
   write is relied on ([work breakdown](#work-breakdown), step 6).
10. **Servo files.** rcbench's text format, `RCBENCH HITEC 1`, through
    `hitec_card.c` and `link_crc.c` copied unchanged: one format for both
    benches.
11. **The write cache.** It runs during the Hitec claim as during the KST
    SERVO and AM32 CONFIG claims, with the flush barrier before a write.
12. **Models not measured.** Writable in their series' layout, as rcbench's
    panel allows, with an amber line on the write page; the 1,000 ms hold is
    the confirmation.

## Choices made in this design

1. **SIG only.** As KST: the wire a servo is plugged into for the servo
   tester. AUX is not offered.
2. **One family per claim.** The family is chosen before the claim, so a
   bind failure is reported as a bind failure, and the line opens with the
   family's drive and receive mode. Another family leaves through the unplug
   page.
3. **Clock in milliseconds.** `HITEC_SES_CLOCK_MS` from `millis()`, as
   rcbench's panel uses it. Every wait of the job is in milliseconds, and
   the clock wraps after 49.7 days, not 71.6 minutes; a session left open
   past a microsecond wrap would wait once more, up to 1,000 ms. A 2 ms gap
   becomes 2 ms to 3 ms.
4. **No session leave with the switched supply.** The classic leave is a
   POWER step that ends with the supply on, and the claim end switches it
   off for 2,000 ms anyway. The D series reboot adds nothing once the supply
   is off. With `NONE` the D series leave reboots the servo, the one way it
   restarts from its saved values.
5. **2,000 ms off for both families.** `KST_SUPPLY_OFF_MS`, within
   `HITEC_SES_OFF_MS_MAX`, 10,000 ms. rcbench's session defaults to 500 ms,
   and its panel waits first until its PD mini reads its output off, up to
   3,000 ms. The converter here has no read-back, so the off time is the
   only wait, and KST's off time for the same converter applies. The
   output's decay is measured at bring-up. A supply off since the bind
   counts.
6. **A write is never aborted from the screen.** A plan cut part way leaves
   a classic checksum unbalanced or a D series value unsaved. BACK during a
   write is queued; a lapse lets it finish. Reads and the entry take an
   abort.
7. **Models not measured are marked, not refused.** The parameters'
   encodings are measured on the D485HW, the HS-5645MG and the HS-7955TG.
   Another model takes its series' layout and is written in it
   ([decision 12](#decisions-of-the-owner)); the write page names the
   layout in amber.
8. **The backup belongs to the job, and a re-entry drops it.** The session
   keeps no backup. A re-entry after a stall may meet another servo of the
   same model, which rcbench's panel cannot tell apart either; a new backup
   avoids restoring one servo's values into another. The ring keeps the
   earlier session's backup when it wrote.
9. **KST's card phase reused as a pattern.** The Hitec claim powers the
   servo through the enable pin while core0 calls the card, as KST's does, so the same
   lapse would switch the servo off during the first write's card calls.
   `sdh_wait_hook` is generic already. The command pair, the stamp and the
   15 s cap are copied into the Hitec job rather than shared: `KstCardPhase`
   is bound to the KST job's command block, and changing built, tested KST
   code for a second user is a larger change than the copy of about
   50 lines.
10. **A record before every write.** rcbench's panel records every write,
    with the image before it and the target. KST records only the first;
    here each write's record is the one that names what a cut write may have
    left, since a failed Hitec write forgets addresses and needs a read.
11. **No ENABLE WRITE switch.** rcbench's panel has a write switch with a 2 s
    hold; the write page's 1,000 ms hold is the confirmation here, as on
    KST's screen ([decision 9](#decisions-of-the-owner)).
12. **No address dump.** The named parameters and the INFO group's raw
    registers show what a dump would; rcbench's panel has none.
13. **The servo model for the job tests is MiniRcBench's.** rcbench's rig
    is written for `greatest.h` and steps the session by hand; the job test
    needs a servo behind a fake line and the fake enable pin, as KST's
    `kst_supply_sim.h` is.
