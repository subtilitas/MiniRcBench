# ESC commands and the kV estimate

Status: designed, not built; waits on the owner's answers to the eight
[open questions](#open-questions). The owner put the feature first on the
build list of 2026-11-14: an ESC commands page and an RPM-per-volt and kV
estimate on the tester, a design note first, then slices with reviews. No
command of this design has gone to an ESC from this board. The firmware
facts below are read from the sources under [sources](#sources), with the
file and line; the ones no readable source settles are marked "not
verified" and listed again under [not verified](#not-verified).

## Terms

ESC is electronic speed controller, FC flight controller, UI user
interface, MCU microcontroller unit, EEPROM the ESC's settings store, PIO
the RP2350's programmable input/output block, CRC cyclic redundancy check.
DShot is the digital ESC protocol the bench sends on SIG: a 16-bit frame of
an 11-bit value, a telemetry bit and a 4-bit checksum, 1000 frames a second
here. Values 48 to 2047 are throttle, 0 is motor stop, and 1 to 47 are
commands. A command is acted on only when the ESC has seen it in several
frames in a row; each such frame is a repeat. Bidirectional DShot inverts
the line and the ESC answers each frame with the motor's electrical period,
read here as eRPM (electrical revolutions per minute); RPM is eRPM divided
by half the motor's pole count. EDT is Extended DShot Telemetry, voltage,
current, temperature, stress and status frames between the eRPM ones. KISS
telemetry is a separate serial wire on AUX, 115,200 baud. SIG and AUX are
the claim's two pins ([pin-claim.md](pin-claim.md#the-two-pins)).

3D mode is a motor that runs both ways from one throttle range: AM32 calls
it `bi_direction`, Bluejay and BLHeli_S call it bidirectional (`PGM_BIDIR`),
which is not bidirectional DShot. A beacon is the ESC's tone, one of
commands 1 to 5. kV is a motor's speed constant, RPM per volt with no load.
RPM/V here is the measured ratio of RPM to the ESC's supply voltage. Duty
is the fraction of each PWM (pulse-width modulation) period the ESC
switches the motor on.

BLHeli_32, BLHeli_S, Bluejay, AM32, KISS and Betaflight are names of their
owners' firmware or products, used here only to say which ESCs and which
references a statement is about.

## Sources

Read for this note; commit and file give the place, line numbers are at
that commit.

| Source | Commit | Files | Licence, access |
|---|---|---|---|
| AM32 (`am32-firmware/AM32`), version 2.22 (`Inc/version.h`) | 2a695a8 | `Src/dshot.c` lines 104 to 232; `Src/main.c` lines 704, 1231 to 1262, 1322 to 1384, 1530 to 1533, 2279 to 2282 | GPL-3.0; read, nothing copied |
| Bluejay (`bird-sanctuary/bluejay`), revision 0.21 (`src/Bluejay.asm` line 331) | 0368d11 | `src/Modules/DShot.asm` lines 60 to 215; `src/Modules/Isrs.asm` lines 211 to 232; `src/Modules/Enums.asm` lines 53 to 62; `src/Bluejay.asm` line 683 | GPL-3.0; read, nothing copied |
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 (`BLHeli_S.asm` lines 534, 535) | a5bcf99 | `BLHeli_S SiLabs/BLHeli_S.asm` lines 771 to 794, 3934, 4010 to 4250 | GPL-3.0; read, nothing copied |
| BLHeli_32 (same repository) | a5bcf99 | `BLHeli_32 ARM/BLHeli_32 Firmware specs/Digital_Cmd_Spec.txt` | a text file of 46 lines; BLHeli_32 itself is closed source |
| Betaflight (`betaflight/betaflight`) | 134ce0f | `src/main/drivers/dshot_command.h` lines 35 to 62; `dshot_command.c` lines 38 to 41, 191 to 212; `src/main/cli/cli.c` lines 4561 to 4744; `src/main/sensors/esc_sensor.c` lines 113, 138 | GPL-3.0; read as the FC side's reference |
| rcbench (`subtilitas/rcbench`) | b88593d3 | `docs/DShot.md`, `docs/BLHeli32.md`, `shared/ui/motor_draw.c` lines 140 to 212 | MIT, same holder |
| This repository | `main` at d4606a6 | `src/dshot/dshot.h`, `src/esc_task.{h,cpp}`, `src/config.h`, `src/ui.cpp`, `src/ui_menu.cpp`, `src/ui_am32.{h,cpp}`, `src/am32_eeprom.cpp`, `src/kiss_telem.cpp`, [kiss-telemetry.md](kiss-telemetry.md), [menu-b.md](menu-b.md) | |

PR #66, the chooser of menu structure B (branch `ccr-mb9-chooser` at
f0f746b), was read for the MENU rows it lands; it is not merged.

## What it is for

- **Direction.** Reverse a motor on the bench without an FC or a desktop
  configurator, and save it in the ESC.
- **3D mode.** Turn 3D on or off and save it, with the tester warning of the
  throttle scale it changes.
- **Beacons 1 to 5.** Hear that an ESC takes commands at all, and compare
  the tones with the one AM32 plays on a save.
- **kV.** Read an estimate of an unmarked motor's kV, propeller off, and the
  plain RPM/V while a motor runs.

It does not read or write an ESC's other settings: AM32 CONFIG does that
for AM32 over the bootloader, and Bluejay and BLHeli_S settings are item (6)
of the same build list. It does not drive 3D throttle on the tester: in 3D
the tester's throttle keeps its meaning of 0 to 2000 into DShot 48 to 2047.

## What the firmwares do with a command

### Values

| Value | BLHeli_32 (specification text only) | Bluejay 0.21 | BLHeli_S 16.7 | AM32 2.22 |
|---|---|---|---|---|
| 1 to 5 | beacon; wait 260, 260, 280, 280 and 1020 ms before the next command | beacon tone, acted on at the first check, then 200 ms with interrupts off (`DShot.asm` 84 to 96) | beep, then 100 ms (`BLHeli_S.asm` 4010 to 4093) | `play_tone_flag` 1 to 5 at once (`dshot.c` 162 to 184) |
| 6 | ESC information on the telemetry wire, 64 bytes; wait 12 ms | named (`Enums.asm` 62), not handled | not handled | `send_esc_info_flag` (`dshot.c` 185 to 187); 49 bytes on the serial telemetry wire (`main.c` 2279 to 2282) |
| 7, 8 | spin direction 1, 2 | direction normal, reversed, in RAM (`DShot.asm` 105 to 119) | direction 1, 2, in RAM (`BLHeli_S.asm` 4095 to 4135) | `dir_reversed` 0, 1 in the RAM copy of the settings (`dshot.c` 188 to 197) |
| 9, 10 | 3D mode off, on | 3D off, on, in RAM (`DShot.asm` 121 to 136) | 3D off, on, in RAM (`BLHeli_S.asm` 4137 to 4178) | `bi_direction` 0, 1 in RAM (`dshot.c` 198 to 203) |
| 12 | save settings; wait at least 35 ms | stores direction and 3D in flash (`DShot.asm` 191 to 209) | `erase_and_store_all_in_eeprom` (`BLHeli_S.asm` 4244) | `saveEEpromSettings()`, then the tone of beacon 1 if the direction is normal, of beacon 2 if reversed (`dshot.c` 204 to 208) |
| 13, 14 | EDT on, off | EDT on, off | not handled | EDT on, off |
| 20, 21 | spin direction normal, reversed | the saved direction, or its reverse, "used in turtle mode" (`DShot.asm` 160 to 189); a following 12 saves the reverse | the same from flash (`BLHeli_S.asm` 4180 to 4242) | the running direction only, `forward`; a 12 does not save it (`dshot.c` 220 to 225) |
| 22 to 29 | LED 0 to 3 on, off | not handled | not handled | not handled |
| 36, 37 | none | none | none | AM32's DShot programming mode (`dshot.c` 110 to 127, 226 to 229) |

Betaflight names the same values and marks 22 to 29 "BLHeli32 only"
(`dshot_command.h` 35 to 62).

### Rules

| Rule | BLHeli_32 | Bluejay 0.21 | BLHeli_S 16.7 | AM32 2.22 |
|---|---|---|---|---|
| Repeats for 7 to 14, 20, 21 | "Need 6x"; whether the sixth or the seventh frame acts: not verified | the first frame sets the command and a count of 0, each equal frame after it adds 1, and the command acts at a count of 6: the seventh frame (`Isrs.asm` 219 to 229, `DShot.asm` 98 to 103) | the same: the seventh frame (`BLHeli_S.asm` 779 to 794, 4097 to 4104) | the count is 1 on the first frame and the command acts at 6: the sixth frame (`dshot.c` 157 to 167) |
| Beacons | not stated | acted on at the first check | acted on at the first check | acted on at the first frame (`dshot.c` 162 to 164) |
| Telemetry bit | "must be set in the command frames" | a command frame with the bit clear clears the command and its count (`Isrs.asm` 216, 217, 226 to 229) | the same (`BLHeli_S.asm` 779) | not read: acts with or without it (rcbench `docs/DShot.md`) |
| Motor | "only executed when motors are stopped", throttle zero | checked only in the wait-for-start loop (`Bluejay.asm` 683) | checked only in the wait-for-power-on loop (`BLHeli_S.asm` 3934 to 4010) | only while `running == 0` and AM32's own `armed` (`dshot.c` 157), which AM32 sets after more than a second of zero input, counted in its loop (`main.c` 1530 to 1533) |
| A value 0 between commands | not stated | clears the command and its count (`Isrs.asm` 213, 226 to 229) | not read | clears the command and its count (`dshot.c` 142 to 155) |
| Gaps | "a significant delay (several hundred ms) may be needed between commands" | none needed | none needed | none needed |

Betaflight sends 10 repeats for 7 to 14, 20 and 21, one frame a
millisecond, and waits 100 ms after a beacon (`dshot_command.c` 38 to 41,
191 to 212).

What follows for this bench:

- **10 repeats** reach every count above. The bench sends 10
  (`DSHOT_CMD_REPEATS`, `src/dshot/dshot.h` line 71; `EDT_ENABLE_REPEATS`,
  `src/config.h` line 274). Six would act on AM32 and on none of Bluejay
  and BLHeli_S.
- **The telemetry bit is set** on every command frame already
  (`out_dshot_send(s_pumpPin, f.command, true)`, `src/esc_task.cpp`
  line 471).
- **Direction is 7 and 8, not 20 and 21.** 20 and 21 differ by firmware:
  Bluejay and BLHeli_S save a reversal that a 12 follows, AM32 does not.
  7 and 8 then 12 store the direction on all three open firmwares.
- **A change needs 12** to outlast a power cycle on the three open
  firmwares: 7 to 10 change RAM only. What BLHeli_32 does without 12 is not
  verified.
- **BLHeli_S 16.7 sends no eRPM.** Its source has no reply encoder and no
  inverted-frame flag (`grep` for "invert", "telemetry" and "gcr" in
  `BLHeli_S.asm` finds none of them), and this bench sends only
  bidirectional, inverted DShot (`out_dshot_bind(pin, kbaud, true)`,
  `src/esc_task.cpp` line 387). Whether a BLHeli_S 16.7 ESC takes the
  inverted frames at all is not verified.

### 3D and the tester's throttle

AM32 2.22 in 3D with DShot maps 48 to 1047 onto one direction and 1048 to
2047 onto the other, each doubled: `(value − 48) × 2 + 47` and
`(value − 1048) × 2 + 47` (`main.c` 1231 to 1262). The tester sends throttle
t of 0 to 2000 as t + 47, so in 3D an AM32 ESC runs at about twice the
throttle up to t = 1000, 50 %, and above that turns the other way from low.
At the default ceiling, `MAX 20%` (`DEFAULT_MAX_THROTTLE` 400,
`src/config.h` line 431), it runs as at 40 %. Bluejay and BLHeli_S state
the same split in a comment, "0=stop, 96-2095->fwd, 2096-4095->rev", in
their doubled units (`Isrs.asm` 232, `BLHeli_S.asm` 797); their scaling is
not read. BLHeli_32's is not verified.

### ESC information, command 6

BLHeli_32's specification gives a 64-byte layout: serial number, a version
byte of 254, firmware revision, and at byte 17 "rotation direction reversed
by dshot command or not", at byte 18 "3D mode active or not", ending in a
CRC. Betaflight reads the reply on its ESC sensor port, the KISS telemetry
wire, at 115,200 baud (`cli.c` 4731 to 4744, `esc_sensor.c` 113 and 138),
and tells BLHeli_32's 64 bytes from KISS's 15 and 21 by byte 12
(`cli.c` 4561 to 4590). AM32 sends 49 bytes on the same wire; their layout
is not read. On this bench the KISS receiver assembles 10-byte frames
(`src/kiss_telem.cpp`), so reading the reply is new work:
[question 8](#open-questions).

## The command path today

As built on `main` at d4606a6:

- **Frames.** Core1 sends one frame every 1000 µs whatever the arm state;
  disarmed, the throttle is 0 (`escFrameAction()`, `src/esc_task.h` line
  491). An ESC therefore sees a steady stream of value 0 while the tester
  is disarmed, which is the stopped, armed state its commands need.
- **Commands only disarmed.** A queued command goes out only while
  disarmed; armed frames carry the throttle (`escFrameAction()`).
- **The EDT enable.** 10 frames of 13 at every pump start, then again 300 ms
  (`EDT_SETTLE_MS`) after the ESC starts answering and every 1000 ms
  (`EDT_RETRY_MS`) while no EDT frame arrives, only while disarmed and with
  eRPM 0 (`escEdtStep()`, line 398; `canCommand`, line 408).
- **The beacon.** `escRequestBeep(n)`, `src/esc_task.cpp` line 232,
  refuses while armed; core1 latches the request only when no enable and no
  beacon is going out, and sends `BEEP_REPEATS`, 6 frames
  (`src/config.h` line 277; `escPumpSlot()`, `src/esc_task.h` lines 643 to
  650). An arm drops it. It does not wait for eRPM 0, for the ESC to have
  answered for any time, or for a gap after the beacon. MENU's BEEP THE ESC
  sends beacon 1 (`beepAction()`, `src/ui_menu.cpp` line 74).
- **KISS.** No KISS request goes out while a command is owed (`escPumpSlot()`).
- **Not there:** a general command, a save after it, a wait after a
  command, and the time the ESC has been answering.

## The design

### One command slot on core1

The beacon's slot becomes a command slot. `EscPump` keeps one sequence: a
command, its repeats, and an optional save after it.

| Item | Value | Why |
|---|---|---|
| Commands accepted | 1 to 5, 7, 8, 9, 10, 12 | the table above; 6 waits on question 8; 13 and 14 stay the enable's own; 20, 21 and 22 to 29 are not offered |
| Repeats, 7 to 10 and 12 | 10, `DSHOT_CMD_REPEATS` | the seventh frame acts on Bluejay and BLHeli_S |
| Repeats, beacons | 6, `BEEP_REPEATS`, as today | every open firmware acts on the first |
| Gap after 7 to 10 and 12, `ESC_CMD_GAP_MS` | 300 ms of value 0 | BLHeli_32's "several hundred ms"; chosen, not measured; the 0 frames also end the count, so a second equal command counts from 1 |
| Gap after beacon n, `ESC_CMD_BEEP_GAP_MS` | 260, 260, 280, 280, 1020 ms | BLHeli_32's beep lengths |
| Link up before a command, `ESC_CMD_SETTLE_MS` | 1500 ms of eRPM answers | AM32 takes commands only after more than 1 s of zero input; 500 ms of margin, chosen |
| Motor | eRPM 0 in the last reply | the enable's own rule (`ready`, `src/esc_task.h` line 637) |
| Order | the EDT enable first, then the sequence | as the beacon today: a broken enable reaches no count |
| Arm | drops the sequence | the rule of `escFrameAction()`; unreachable from the page, which cannot arm |
| Link lost before the last frame | the sequence is dropped and reported | the ESC restarted or went; a save it did not see is no save |
| Link lost after the last frame | reported as sent | Bluejay enables its interrupts again only after its flash write (`setb IE_EA`, `DShot.asm` line 209); a gap in the answers then is expected, not measured |

A hold for DIRECTION or 3D sends the command, 300 ms of 0, then 12, then
300 ms of 0: 620 ms in all.

Core0's side, beside `escRequestBeep()`:

```c
// esc_task.h
enum class EscCmdState : uint8_t { Idle, Waiting, Sending, Sent, Dropped };
bool        escRequestCommand(uint8_t cmd, bool save);  // false while armed
EscCmdState escCommandState(uint8_t *cmd);              // last sequence
```

`Waiting` covers the settle time, a turning motor and an enable going out.
The rule stays pure in `src/esc_task.h`, as `escPumpSlot()` is, so the host
suite drives it. `escRequestBeep(n)` becomes `escRequestCommand(n, false)`,
so MENU's beacon gains the settle time and the eRPM 0 rule.

### The page

ESC COMMANDS is a screen with `PinUser::None` and `freePins` true
(`src/ui_screen.h` line 154): it claims no pin and needs DShot running, so
it opens only while the claim is Free. It opens through `uiOpenTool()`
(`src/ui.cpp` line 956), which runs the tester's disarm first
(`testerDisarm()`, line 930). It has no arm control. Core0 sends its
heartbeat on every frame of every screen (`uiTick()`, line 1293), so the
250 ms heartbeat rule holds here as on the tester.

- **BEEP 1 to 5:** a tap each; the screen shows `SENT 3 (BEEP 3).`, and
  every control is drawn disabled for the beep's gap.
- **DIRECTION NORMAL, REVERSED:** a 1000 ms hold each (`ui_hold`,
  `UI_HOLD_MS`); the hold sends 7 or 8, then 12.
- **3D OFF, 3D ON:** a 1000 ms hold each; 9 or 10, then 12.
- While a sequence goes out every control and BACK are drawn disabled, as
  BACK is during a write ([menu-b.md](menu-b.md#decisions), decision 5).
- A control is live only while `escCommandState()` could start a
  sequence: disarmed, eRPM 0, the link up for 1500 ms. Otherwise the
  message lines say why, in `C_AMBER`.

The holds write the ESC. Menu structure B puts writes on a write page that
a tap opens ([menu-b.md](menu-b.md#decisions), decision 6). That rule was
made for lists of changes; here one button is one change and names it:
[question 3](#open-questions).

### Safety and interlocks

| Rule | Where | What it guards |
|---|---|---|
| Disarmed only | `escFrameAction()`; `escRequestCommand()` refuses while armed; `uiOpenTool()` disarms on the way in; the page has no arm control | a command frame in place of a throttle; an ESC ignores commands while it turns |
| eRPM 0 | the slot's `ready` | a prop still coasting after the disarm |
| 1500 ms of answers | the slot | AM32's own arming, about 1 s |
| A 1000 ms hold for each write | `ui_hold` | a brush of the glass writing the ESC |
| Save in the same hold | the slot's sequence | a direction that reverts on the next power cycle, unseen ([question 2](#open-questions)) |
| BACK disabled while sending | the page | leaving mid-sequence, as during a write; even with BACK live, the tester's 1000 ms arm hold could not end before the 620 ms sequence |
| The 3D note | `uiAm32Esc3d()` today | a tester throttle that runs at twice its scale |
| No answer, no command | the slot | a command to nothing, read as done ([question 5](#open-questions)) |

**The 3D note.** Today `AM32_UI_TESTER_3D`, `3D MODE ESC: THROTTLE SCALE
DIFFERS` (35 characters, 209 px, `src/ui_am32.h` line 181), stands after an
AM32 list read or an accepted AM32 write with 3D MODE on, and only a list
read or a verified write with 3D MODE off clears it (`s_esc3d`,
`src/ui_am32.cpp` lines 258, 879, 965, 995; the owner's answer to the
review of PR #55). The flag moves out of `ui_am32.cpp` into one owner both
screens write, `src/esc_3d.{h,cpp}`, and the tester reads it as before. A
completed 3D ON hold sets it. A 3D OFF hold has no read-back, like an
unverified write; by the PR #55 rule it leaves the note standing until the
bench restarts or an AM32 list read shows 3D MODE off
([question 4](#open-questions)).

**Direction.** The bench cannot see it: an eRPM reply is a period with no
sign (`src/dshot/dshot.h`, "carrying an electrical period rather than a
speed"). The pilot checks it:

1. Propeller off, a strip of tape on the bell.
2. On the tester, arm and run at 5 % (`MAX_THROTTLE_STEP` 100 is 5 %) for a
   second; watch the tape; disarm.
3. On AM32, the save plays beacon 1's tone for normal and beacon 2's for
   reversed (`dshot.c` 204 to 208); BEEP 1 and BEEP 2 on the page play the
   same tones for comparison. AM32 CONFIG then reads `MOTOR DIRECTION` and
   `3D MODE` back (`src/am32_eeprom.cpp` lines 90 to 94).
4. After a power cycle of the ESC, step 2 again: a direction that holds
   was saved.

On BLHeli_32, byte 17 of the ESC information would say it, if question 8
is built.

### MENU entry

Menu structure B lands seven MENU rows with PR #66: BEEP THE ESC, AM32
CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS, at a
42 px pitch in a 258 px viewport, so MENU scrolls 36 px
([menu-b.md](menu-b.md#menu), decision 33). The page needs a way in, and
each way changes something menu-b.md decides:

| Option | MENU | Taps to a direction change | Changes |
|---|---|---|---|
| (a) ESC COMMANDS in place of BEEP THE ESC | 7 rows; `ESC COMMANDS`, 12 characters, 142 px, x 16 to 157, the width of `BEEP THE ESC`; no status | MENU, ESC COMMANDS, hold: 2 taps and a hold | BEEP moves onto the page and takes a tap more; [menu.md](menu.md#decisions) decision 2 and menu-b decisions 16 and 23 name BEEP THE ESC as MENU's first row |
| (b) an eighth row after BEEP THE ESC | 8 rows: 8 × 42 − 258 = 78 px of scroll; SETTINGS starts at y 356 at scroll 0 | 2 taps and a hold | decision 33's seven rows |
| (c) an ESC chooser in AM32 CONFIG's row, like PROGRAMMER, with AM32 CONFIG and ESC COMMANDS | 7 rows | 3 taps and a hold; AM32 CONFIG one tap deeper | decision 16's row, and the chooser claims nothing while AM32 CONFIG claims SIG |
| (d) a fourth button on the tester | 7 rows | 1 tap and a hold | the tester's button row, 6/104, 116/54, 176/58 (menu-b "The tester"), has no room; the commands sit beside HOLD TO ARM |

This note recommends (a): every BEEP is a DShot command, the page carries
all five, and MENU keeps seven rows. [Question 1](#open-questions).

### Screen

Header `ESC COMMANDS` at scale 2, 142 px, x 8 to 149, 20 px clear of BACK
at x 170. Other text is scale 1, a string of n characters (6n − 1) px
wide; buttons follow decision 14's layout table (x 6 to 233, `UI_EDGE`).

```
y   0 | ESC COMMANDS                    [  BACK  ]|  header 36 px, BACK 170..233
   40 | SIG GP0  DSHOT600                [EDT ON] |  strip 40..55; MENU's chip
   62 | BEEP                                      |  C_DIM
   72 | [ 1 ]  [ 2 ]  [ 3 ]  [ 4 ]  [ 5 ]         |  taps, 40 x 40, x 6 + 47i
  120 | DIRECTION                                 |  C_DIM
  130 | [   NORMAL    ] [   REVERSED    ]         |  holds, 111 x 44: x 6, x 123
  174 | ===============                           |  hold bar 3 px, y 174..176
  182 | 3D MODE                                   |  C_DIM
  192 | [   3D OFF    ] [    3D ON      ]         |  holds, 111 x 44
  236 | ===============                           |  hold bar 3 px, y 236..238
  244 | HOLD 1 S: SETS IT, THEN SAVES IT.         |  6 message lines,
  256 | THE ESC SENDS NO ANSWER TO A COMMAND:     |  12 px pitch,
  268 | CHECK THE DIRECTION ON THE TESTER.        |  y 244 to 304,
  280 |                                           |  the last ending
  292 | AM32 BEEPS AS BEEP 1 ON SAVE (NORMAL)     |  at y 310
  304 | OR AS BEEP 2 (REVERSED).                  |
  320 +-------------------------------------------+
```

| Element | Rectangle (x, y, w, h) or place | Text | Width |
|---|---|---|---|
| Strip caption | x 8, y 44 | `SIG GP0  DSHOT600`; widest `SIG GP1  DSHOT1200`, 18 characters | 101 px; 107 px, to x 114 |
| Strip chip | `uiChip()` as on MENU, right edge x 232 | `EDT ON`, `EDT OFF`, `NO ESC` | 35, 41, 35 px plus 12 px: from x 179 at the widest, 64 px clear of the caption |
| BEEP caption | x 8, y 62 | `BEEP` | 23 px |
| BEEP n, n = 1 to 5 | 6 + 47(n − 1), 72, 40, 40 | `1` to `5`, scale 2, centred | 10 px |
| DIRECTION caption | x 8, y 120 | `DIRECTION` | 53 px |
| NORMAL | 6, 130, 111, 44 | `NORMAL`, scale 2, centred | 70 px |
| REVERSED | 123, 130, 111, 44 | `REVERSED`, scale 2, centred | 94 px, 8 px each side |
| 3D caption | x 8, y 182 | `3D MODE` | 41 px |
| 3D OFF | 6, 192, 111, 44 | `3D OFF`, scale 2 | 70 px |
| 3D ON | 123, 192, 111, 44 | `3D ON`, scale 2 | 58 px |
| Message line i, i = 0 to 5 | x 8, y 244 + 12i | at most 37 characters | at most 221 px, to x 228 |

Message lines, each with its width; every one is at most 37 characters:

| State | Lines | Colour |
|---|---|---|
| Idle | `HOLD 1 S: SETS IT, THEN SAVES IT.` (197 px); `THE ESC SENDS NO ANSWER TO A COMMAND:` (221 px); `CHECK THE DIRECTION ON THE TESTER.` (203 px); blank; `AM32 BEEPS AS BEEP 1 ON SAVE (NORMAL)` (221 px); `OR AS BEEP 2 (REVERSED).` (143 px) | `C_DIM` |
| Sending a direction | `SENDING 8 (REVERSED), THEN 12 (SAVE).` (221 px) | `C_TEXT` |
| Sent a direction | `SENT 8 (REVERSED), THEN 12 (SAVE).` (203 px) or `SENT 7 (NORMAL), THEN 12 (SAVE).` (191 px); `NOTHING CONFIRMS THE ESC TOOK THEM.` (209 px); `CHECK THE DIRECTION ON THE TESTER.` (203 px) | `C_TEXT` |
| Sending 3D | `SENDING 10 (3D ON), THEN 12 (SAVE).` (209 px) or `SENDING 9 (3D OFF), THEN 12 (SAVE).` (209 px) | `C_TEXT` |
| Sent 3D ON | `SENT 10 (3D ON), THEN 12 (SAVE).` (191 px); `THE TESTER SHOWS THE 3D NOTE NOW.` (197 px); `IN 3D AN AM32 ESC RUNS AT 2X THROTTLE` (221 px); `UP TO 50%, THEN TURNS THE OTHER WAY.` (215 px) | `C_TEXT`, the last two `C_AMBER` |
| Sent 3D OFF | `SENT 9 (3D OFF), THEN 12 (SAVE).` (191 px); `THE 3D NOTE STAYS UNTIL A RESTART.` (203 px), by question 4's recommendation | `C_TEXT` |
| A beacon | `SENDING 3 (BEEP 3).` (113 px), then `SENT 3 (BEEP 3).` (95 px) | `C_TEXT` |
| No answer | `NO ESC ANSWERS ON SIG GP0.` (155 px) | `C_AMBER` |
| Motor turning | `MOTOR TURNING: WAIT UNTIL IT STOPS.` (209 px) | `C_AMBER` |
| Settling | `ESC STARTING: WAIT 1.5 S.` (149 px) | `C_AMBER` |
| Dropped | `NOT SENT: THE ESC STOPPED ANSWERING.` (215 px) | `C_AMBER` |

The pin in the texts is SIG's, GP0 or GP1. Layout constants get
`static_assert`s for every width and gap, as the other screens have.

## The kV estimate

### Formula

With RPM the mechanical speed, V the ESC's supply voltage and t the
throttle the tester sends, 0 to 2000:

- RPM/V = RPM / V, the measured ratio, as rcbench's `rpm/V`
  (`motor_draw.c` line 179).
- kV estimate = RPM / (V × t / 2000) = RPM × 2000 / (V × t).

The estimate assumes the ESC's duty is t / 2000 and that the motor, with no
load, turns at kV times the voltage the duty gives it. Both are
approximations. AM32 2.22 maps its input linearly onto duty above its
sine-start changeover, from a minimum duty to 100 %:
`map(input, 47, 2047, minimum_duty_cycle, 2000)` (`main.c` line 1384), with
a start-up remap below the changeover, which its settings hold to 5 % to
25 % (`main.c` lines 704, 1322 to 1331). How Bluejay, BLHeli_S and
BLHeli_32 map throttle onto duty is not verified. In 3D the assumption is
false (above).

In integers, as the tester holds them: `rpm` from the filtered eRPM over
half the pole count, the figure the big readout shows (`drawRpm()`,
`src/ui.cpp` line 664); `volts100`, hundredths of a volt, from the merged
reading the VOLTAGE tile shows, KISS where fresh, EDT otherwise
(`escMerge()`); `t` = `s_throttle`.

```c
// kv_est.h, pure
uint32_t rpmPerVolt = (rpm * 100 + volts100 / 2) / volts100;
uint64_t den = (uint64_t)volts100 * t;
uint64_t kvEst = ((uint64_t)rpm * 2000 * 100 + den / 2) / den;
```

Both are clamped to 99999, the width the field has.

### Resolution

| Source | Step | Share of the estimate |
|---|---|---|
| eRPM, a normalised frame (mantissa 256 to 511), a period of 256 µs or more, eRPM up to 234,375 | 1 / mantissa of the period | 0.20 % to 0.39 %; at 5,600 RPM on 14 poles 14.6 RPM, 0.26 % (`erpm_resolution.py`'s rule) |
| Voltage, EDT | 0.25 V, so ±0.125 V | 0.78 % at 16.0 V (4S), 1.56 % at 8.0 V (2S), 3.13 % at 4.0 V (1S) |
| Voltage, KISS | 0.01 V, so ±0.005 V | 0.03 % at 16.0 V |
| Throttle | the value sent, exact | none |
| Display | 1 kV | 0.06 % at 1,750 kV |
| The display filter | `RPM_FILTER_SHIFT` 3 at one UI frame in 25 ms: a step's residue after 1000 ms is (7/8)^40, 0.48 % | none once settled |
| The duty assumption | unknown | unknown, not measured |

Example: a 1,750 kV motor on 14 poles at 16.0 V and t = 1000 (50 %) turns
at about 14,000 RPM in the ideal case; RPM/V reads 875 and the estimate
1,750. The eRPM step there is 0.33 %; with EDT voltage the estimate's
quantisation is about ±1.1 %, with KISS about ±0.4 %. The motor's own
losses lower both, by an amount not known here.

### When it is shown

All of these, else `KV EST --`:

1. Armed, the link alive, eRPM above 0, the pole count 2 or more.
2. A voltage fresh from KISS or EDT, above 0.5 V (rcbench's floor,
   `motor_draw.c` line 178).
3. t at least `KV_EST_MIN_THROTTLE`, 600, 30 %: above AM32's highest
   sine-start changeover, 25 %. Below it the field reads `KV EST 30%+`.
   At the default ceiling, 20 %, the estimate does not show; the pilot
   raises `MAX` in SETTINGS ([question 7](#open-questions)).
4. t unchanged for `KV_EST_SETTLE_MS`, 1000 ms: the display filter's
   residue is then 0.48 % of the step; the motor's spin-up time is not
   measured.
5. The 3D note not standing.

RPM/V needs 1 and 2 only. Neither is held after the disarm (question 7).

### What it cannot know

- **Load.** The estimate is for a motor with no propeller; with one, it
  reads low by the load's share. The bench cannot tell.
- **Duty.** The firmware's throttle-to-duty map, its minimum duty, a
  start-up or low-speed power limit, a current limit, or a speed-control
  mode that targets RPM rather than duty.
- **Voltage at the motor.** V is the ESC's supply as the ESC reports it;
  the switches', wiring's and winding's drops are inside the figure.
- **Losses.** Friction, iron losses and the no-load current lower the
  speed below kV × V; by how much, for a given motor, is unknown.
- **Timing.** Whether the ESC's commutation timing changes the no-load
  speed, and by how much, is not verified.
- **Poles.** A wrong pole count scales both figures by the ratio of the
  true count to the set one: 12 set on a 14-pole motor reads 14/12, 16.7 %
  high. The pole readout opens SETTINGS while disarmed (slice 7 of
  [menu-b.md](menu-b.md#slice-7-as-built)).
- **Direction and 3D.** The figures are unsigned, and 3D breaks the duty
  assumption.

It is labelled `KV EST`, never `KV`; the font has no `~` (glyphs 0x20 to
0x60, `src/gfx.cpp` line 251).

### On the tester

Recommended layout, [question 6](#open-questions); the rest of the tester
is as [menu-b.md](menu-b.md#the-tester) draws it.

```
y 102 | ERPM 39200  RPM/V 875                 RPM |  x 10, C_DIM; RPM 192..225
  114 | KV EST 1750              SWIPE = THROTTLE |  armed; disarmed: 14 POLES
```

| Text | Characters | Width | Place | Room |
|---|---|---|---|---|
| `ERPM 39200  RPM/V 875` | 21 | 125 px | x 10 to 134, y 102 | |
| widest, `ERPM 1234567  RPM/V 99999` | 25 | 149 px | x 10 to 158 | 33 px to `RPM` at x 192 |
| `KV EST 1750` | 11 | 65 px | x 10 to 74, y 114, `C_TEXT` | |
| widest, `KV EST 99999` | 12 | 71 px | x 10 to 80 | 50 px to `SWIPE = THROTTLE` at x 131, 74 px to `SWIPE ACTIVE` at x 155 |
| `KV EST 30%+`, `KV EST --` | 11, 9 | 65, 53 px | x 10 | |

While armed the pole readout is inert and drawn without its underline
(slice 7); this layout puts the estimate in its place until the disarm.
The 3D note takes the whole row as today, and the estimate gives way with
the pole count. The blackbox log needs nothing new: it carries `motor[0]`,
`eRPM[0]`, `vbatLatest` and `vbatEdt` (`src/blackbox_encode.cpp` lines 14
to 19), from which both figures follow.

## Tests

### Host

- **The slot,** through `escPumpSlot()` and `escPumpFrameSent()`: a
  direction hold's frames, 10 of 8, 300 frames of 0, 10 of 12, 300 of 0;
  every command frame with the telemetry bit; the EDT enable first and the
  sequence whole after it; refused while armed; held while eRPM is not 0;
  held until the link has answered 1500 ms and not at 1499 ms; dropped by
  an arm and by a link loss before the last frame, sent after it; a
  beacon's gap of 260 ms and 1020 ms; no KISS request while a command is
  owed; across a `millis()` wrap.
- **Model ESCs,** in the manner of the EDT tests: an AM32 rule (acts on the
  sixth equal frame, only after 1 s of zero input, a 0 frame clears) and a
  Bluejay and BLHeli_S rule (acts on the seventh, a frame without the
  telemetry bit clears); each takes the bench's sequence, and a sequence of
  6 repeats fails the second, so a mutation to 6 is caught.
- **The page,** by control id: opens from its MENU entry with the claim
  Free and not otherwise; disarms on the way in; the holds at 1000 ms and
  not at 999 ms; every control and BACK disabled while sending and for a
  beep's gap; each state's lines; every string within its rectangle; the
  3D note set by 3D ON and not cleared by 3D OFF (question 4); the preview
  `docs/esccmd-preview.png`.
- **The estimate,** `kv_est.h`: both formulas at the clamp, at 0.5 V and
  t = 600 and their neighbours; the settle at 1000 ms and not at 999 ms;
  hidden with the 3D note, disarmed, with no voltage, with fewer than 2
  poles; the tester's two lines and their fits.
- Every new rule mutation-checked: break it, see a test fail, restore.

### Bench

Only an ESC settles these, one per firmware at hand:

- B1. Direction: REVERSED, tape on the bell, 5 % on the tester; power-cycle
  the ESC; still reversed; NORMAL back.
- B2. AM32: the save's tone against BEEP 1 and BEEP 2; AM32 CONFIG reading
  `MOTOR DIRECTION` and `3D MODE` as sent.
- B3. 3D ON: the note on the tester; at 10 % the motor runs as at about
  20 % on AM32; 3D OFF; a power cycle.
- B4. BEEP 1 to 5 on each firmware; whether BLHeli_32 takes a command
  within its beep lengths.
- B5. On a logic analyser at SIG: 10 frames of 8, 300 ms of 0, 10 of 12;
  the EDT enable never broken by a sequence.
- B6. An ESC plugged in with the page open: the first command waits
  1500 ms and AM32 acts on it.
- B7. A BLHeli_S 16.7 ESC, if one is at hand: whether it answers or
  starts at all on inverted DShot.
- B8. kV: a motor of known rated kV, propeller off, at 30 %, 50 %, 75 % and
  100 %, with EDT and with KISS voltage; `KV EST` and `RPM/V` against the
  rating, and the RPM against an optical tachometer if one is at hand.

## Work breakdown

Each slice builds, passes the host suite and merges alone. Sizes are new
and changed lines, rough.

| Slice | Content | Code | Tests | Docs | Depends on |
|---|---|---|---|---|---|
| 1. Command slot | `EscPump`'s sequence, `escRequestCommand()`, `escCommandState()`, the settle and gap constants in `src/config.h`, `escRequestBeep()` through it; the model ESCs | about 200 | about 450 | README's MENU table row on BEEP THE ESC and its EDT paragraph, STATUS, CHANGELOG | |
| 2. ESC COMMANDS page | `src/ui_esc_cmd.{h,cpp}`, `src/esc_3d.{h,cpp}` and `ui_am32.cpp` on it, the MENU entry by question 1, `test_ids.cpp`, the preview, `tools/make_previews.py`, `tools/check_wiki.py`, README section, wiki pages in English and German, STATUS, CHANGELOG, this note's status | about 450 | about 500 | about 250 | 1, and PR #66 for options (a) to (c) |
| 3. kV estimate | `src/kv_est.h`, `drawRpm()` in `src/ui.cpp`, the tester previews, README and wiki on the tester in both languages | about 120 | about 250 | about 120 | none |
| 4. ESC information | not designed: question 8 | | | | 1 |
| 5. Bring-up | B1 to B8 | none | none | the record in STATUS | 1 to 3 |

Slices 1 and 3 do not touch each other's files beyond STATUS and
CHANGELOG and can merge in either order.

## Open questions

1. **Where does the page live?** (a) `ESC COMMANDS` in place of `BEEP THE
   ESC`, MENU's first row, with BEEP 1 to 5 on the page; MENU keeps seven
   rows (recommended). (b) An eighth MENU row after BEEP THE ESC; MENU
   scrolls 78 px. (c) An ESC chooser in AM32 CONFIG's row, with AM32 CONFIG
   and ESC COMMANDS. (d) A fourth button on the tester (not recommended:
   no room, and beside HOLD TO ARM).
2. **Save with each change?** (a) Each hold sends the change, then 12
   (recommended: a change that reverts on the next power cycle is not seen
   until it does). (b) Changes without a save, and a separate HOLD TO SAVE.
3. **A hold on the page, or a write page?** Menu-b decision 6 puts writes
   on a write page that a tap opens. (a) The 1000 ms hold on the page,
   since one button is one named change (recommended). (b) A tap opens a
   write page naming the change, whose hold sends it.
4. **The 3D note after 3D OFF.** (a) It stays until the bench restarts or
   an AM32 list read shows 3D MODE off, as PR #55's rule for a write that
   is not verified (recommended). (b) A completed 3D OFF hold clears it.
5. **Commands with no ESC answering.** (a) Refused, with `NO ESC ANSWERS
   ON SIG GP0.` (recommended; a BLHeli_S 16.7 ESC, which sends no eRPM, is
   then not served). (b) Sent with that line as a warning, 1500 ms after
   the last throttle frame, without the eRPM 0 check.
6. **Where do RPM/V and the estimate go on the tester?** (a) `RPM/V` on
   the ERPM line and `KV EST` in place of `14 POLES` while armed
   (recommended). (b) The STRESS tile becomes `KV EST`, with `RPM/V` on its
   label row, and stress shows in the log only. (c) `KV EST` alone on the
   ERPM line, and no RPM/V.
7. **The estimate's minimum throttle, and after the disarm.** 30 %, above
   AM32's highest sine-start changeover (recommended), or another value;
   and whether the estimate keeps its last value after the disarm (not
   recommended: a held figure reads as a live one).
8. **ESC information, command 6.** Build a slice that reads its reply on
   the KISS wire (BLHeli_32 64 bytes, AM32 49 bytes), which would confirm
   direction and 3D on BLHeli_32, or leave it out.

## Not verified

- BLHeli_32's behaviour beyond its 46-line specification text: whether
  "6x" means the sixth or the seventh frame, what 7 to 10 do without 12,
  how 20 and 21 differ from 7 and 8, its 3D scaling and its duty map.
- Whether a BLHeli_S 16.7 ESC takes inverted DShot frames.
- AM32 versions other than 2.22 at 2a695a8, among them the 2.21 that
  rcbench records on hardware; whether AM32 acts on a command frame
  without the telemetry bit is rcbench's statement, not read here.
- Bluejay's and BLHeli_S's 3D scaling and duty maps; whether Bluejay's
  answers stop while it writes flash.
- AM32's 49-byte information layout.
- That 300 ms between commands suffices for BLHeli_32, and that 1500 ms of
  answers suffices for every AM32 build.
- A motor's spin-up time after a throttle step, and the size of the
  no-load losses and the timing effect on the estimate.
- Everything on hardware: no command of this design, no direction change
  and no estimate has run on an ESC or a motor from this board.
