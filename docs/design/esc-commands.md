# ESC commands and the kV estimate

Status: designed; the owner answered its questions on 2026-11-15, see
[decisions](#decisions). Slice 1, the command slot on core1, is built and
tested on the host only ([slice 1 as built](#slice-1-as-built)); slices 2
and 3 wait on being built. The feature is item (1) of the owner's
feature choice of 2026-11-14, recorded in [STATUS.md](../../STATUS.md)
open item 28: an ESC commands page and an RPM-per-volt and kV estimate on
the tester, a design note first, then slices with reviews. No command of
this design has gone to an ESC from this board. The firmware
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
| AM32 (`am32-firmware/AM32`), version 2.22 (`Inc/version.h`) | 2a695a8 | `Src/dshot.c` lines 36 to 42, 104 to 232, 242 to 269; `Src/signal.c` line 118; `Inc/targets.h` lines 992, 1451, 1558, 1601, 1614, 2038; `Src/main.c` lines 662 to 721, 1231 to 1303, 1322 to 1384, 1530 to 1533, 2081 to 2085, 2272 to 2283; `Inc/eeprom.h` line 55 | GPL-3.0; read, nothing copied |
| Bluejay (`bird-sanctuary/bluejay`), revision 0.21 (`src/Bluejay.asm` line 331) | 0368d11 | `src/Modules/DShot.asm` lines 60 to 215; `src/Modules/Isrs.asm` lines 200 to 256; `src/Modules/Enums.asm` lines 53 to 63; `src/Bluejay.asm` line 683 | GPL-3.0; read, nothing copied |
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 (`BLHeli_S.asm` lines 534, 535) | a5bcf99 | `BLHeli_S SiLabs/BLHeli_S.asm` lines 759 to 835, 3934, 4010 to 4250 | GPL-3.0; read, nothing copied |
| BLHeli_32 (same repository) | a5bcf99 | `BLHeli_32 ARM/BLHeli_32 Firmware specs/Digital_Cmd_Spec.txt` | a text file of 46 lines; BLHeli_32 itself is closed source |
| Betaflight (`betaflight/betaflight`) | 134ce0f | `src/main/drivers/dshot_command.h` lines 35 to 62; `dshot_command.c` lines 38 to 41, 186 to 275; `src/main/cli/cli.c` lines 4561 to 4744; `src/main/sensors/esc_sensor.c` lines 113, 138 | GPL-3.0; read as the FC side's reference |
| rcbench (`subtilitas/rcbench`) | b88593d3 | `docs/DShot.md`, `docs/BLHeli32.md`, `shared/ui/motor_draw.c` lines 140 to 212 | MIT, same holder |
| This repository | `main` at 51f10e0 | `src/dshot/dshot.h`, `src/esc_task.{h,cpp}`, `src/config.h`, `src/ui.cpp`, `src/ui_menu.cpp`, `src/ui_am32.{h,cpp}`, `src/am32_eeprom.cpp`, `src/kiss_telem.cpp`, [kiss-telemetry.md](kiss-telemetry.md), [menu-b.md](menu-b.md) | |

Pull request (PR) #66, slice 9 of menu structure B, is on `main` since
33079ee: the SERVO PROGRAMMER chooser and seven MENU rows
(`src/ui_registry.cpp` lines 22 to 30).

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
of the same feature choice. It does not drive 3D throttle on the tester: in 3D
the tester's throttle keeps its meaning of 0 to 2000 into DShot 48 to 2047.

## What the firmwares do with a command

### Values

| Value | BLHeli_32 (specification text only) | Bluejay 0.21 | BLHeli_S 16.7 | AM32 2.22 |
|---|---|---|---|---|
| 1 to 5 | beacon; wait 260, 260, 280, 280 and 1020 ms before the next command | beacon tone, acted on at the first check, then 200 ms with interrupts off (`DShot.asm` 84 to 96) | beep, then 100 ms (`BLHeli_S.asm` 4010 to 4093) | `play_tone_flag` 1 to 5 at once (`dshot.c` 162 to 184) |
| 6 | ESC information on the telemetry wire, 64 bytes; wait 12 ms | named (`Enums.asm` 63), not handled | not handled | `send_esc_info_flag` (`dshot.c` 185 to 187); 49 bytes on the serial telemetry wire (`main.c` 2279 to 2282) |
| 7, 8 | spin direction 1, 2 | direction normal, reversed, in RAM (`DShot.asm` 105 to 119) | direction 1, 2, in RAM (`BLHeli_S.asm` 4095 to 4135) | `dir_reversed` 0, 1 in the RAM copy of the settings (`dshot.c` 188 to 197) |
| 9, 10 | 3D mode off, on | 3D off, on, in RAM (`DShot.asm` 121 to 136) | 3D off, on, in RAM (`BLHeli_S.asm` 4137 to 4178) | `bi_direction` 0, 1 in RAM (`dshot.c` 198 to 203) |
| 12 | save settings; wait at least 35 ms | stores direction and 3D in flash (`DShot.asm` 191 to 209) | `erase_and_store_all_in_eeprom` (`BLHeli_S.asm` 4244) | `saveEEpromSettings()`, then the tone of beacon 1 if the direction is normal, of beacon 2 if reversed (`dshot.c` 204 to 208) |
| 13, 14 | EDT on, off | EDT on, off | not handled | EDT on, off |
| 20, 21 | spin direction normal, reversed | the saved direction, or its reverse, "used in turtle mode" (`DShot.asm` 160 to 189); a following 12 saves the reverse | the same from flash (`BLHeli_S.asm` 4180 to 4242) | the running direction only, `forward`; a 12 does not save it (`dshot.c` 220 to 225) |
| 22 to 29 | LED (light-emitting diode) 0 to 3 on, off | not handled | not handled | not handled |
| 36, 37 | none | none | none | AM32's DShot programming mode (`dshot.c` 110 to 127, 226 to 229) |

Betaflight names the same values and marks 22 to 29 "BLHeli32 only"
(`dshot_command.h` 35 to 62).

### Rules

| Rule | BLHeli_32 | Bluejay 0.21 | BLHeli_S 16.7 | AM32 2.22 |
|---|---|---|---|---|
| Repeats for 7 to 14, 20, 21 | "Need 6x"; whether the sixth or the seventh frame acts: not verified | the first frame sets the command and a count of 0, each equal frame after it adds 1, and the command acts at a count of 6: the seventh frame (`Isrs.asm` 219 to 229, `DShot.asm` 98 to 103) | the same: the seventh frame (`BLHeli_S.asm` 779 to 794, 4097 to 4104) | the count is 1 on the first frame and the command acts at 6: the sixth frame (`dshot.c` 157 to 167) |
| Beacons | not stated | acted on at the first check | acted on at the first check | acted on at the first frame (`dshot.c` 162 to 164) |
| Telemetry bit | "must be set in the command frames" | a command frame with the bit clear clears the command and its count (`Isrs.asm` 216, 217, 226 to 229) | the same (`BLHeli_S.asm` 779) | not checked by the command path: the bit only sets `send_telemetry` (`dshot.c` 107 to 109), and the command path (138 to 167) acts with or without it |
| Motor | "only executed when motors are stopped", throttle zero | checked only in the wait-for-start loop (`Bluejay.asm` 683) | checked only in the wait-for-power-on loop (`BLHeli_S.asm` 3934 to 4010) | only while `running == 0` and AM32's own `armed` (`dshot.c` 157), which AM32 sets after more than a second of zero input, counted in its loop (`main.c` 1530 to 1533) |
| A value 0 between commands | not stated | clears the command and its count (`Isrs.asm` 213, 226 to 229) | leaves the command and its count as they were: a 0 skips the command bookkeeping (`BLHeli_S.asm` 770 to 775); the handler clears the command once it acts | clears the command and its count (`dshot.c` 142 to 155) |
| Gaps | "a significant delay (several hundred ms) may be needed between commands" | none needed | none needed | none needed |

Betaflight sends 10 repeats for 7 to 10, 12 to 14, 20 and 21, a beacon
once (`repeats = 1`, `dshot_command.c` 191 to 213), one frame a
millisecond. It waits 100 ms after a beacon only on its blocking path, the
one its command line uses (line 262); its inline path waits 1 ms after
every command (line 272).

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

The three open firmwares split the DShot range the same way in 3D. Values
48 to 1047 run the motor the reverse of its set direction, at twice the
step: AM32 `(value − 48) × 2 + 47` with `forward = dir_reversed`, which
is the reverse of the set direction (`main.c` 1286 to 1299). Values 1048
to 2047 run it in the set direction from low: `(value − 1048) × 2 + 47`
(`main.c` 1271 to 1285).
AM32 changes direction only once the motor is slow (`commutation_interval`
above its threshold and duty below 200 of 2000), and sends 0 until then.
Bluejay subtracts 2000 in its doubled units, sets the reverse direction
when the result is negative, and doubles the remainder (`Isrs.asm` 235 to
256); BLHeli_S does the same (`BLHeli_S.asm` 800 to 835). The `main.c`
lines 1231 to 1269 are AM32's `rc_car_reverse` branch, a separate setting.
BLHeli_32's 3D mapping is not verified.

The tester sends throttle t of 0 to 2000 as t + 47. In 3D, its 0 to 50 %
runs the motor reversed at twice the throttle; above 50 % it runs in the
set direction, from low. At the default ceiling, `MAX 20%`
(`DEFAULT_MAX_THROTTLE` 400, `src/config.h` line 431), a 3D ESC runs
reversed as at 40 %. A direction check on the tester is therefore only
meaningful with 3D off.

### ESC information, command 6

BLHeli_32's specification gives a 64-byte layout, its bytes numbered from 1:
serial number in 1 to 12, a version byte of 254 at 13, firmware revision, and at
byte 17 "rotation direction reversed by dshot command or not", at byte 18 "3D
mode active or not", ending in a CRC at 64. Betaflight reads the reply on its
ESC sensor port, the KISS telemetry wire, at 115,200 baud (`cli.c` 4731 to 4744,
`esc_sensor.c` 113 and 138), and tells BLHeli_32's 64 bytes from KISS's 15 and
21 by `escInfoBuffer[12]`, numbered from 0, the specification's byte 13 (`cli.c`
4561 to 4590). AM32 sends 49 bytes on the same wire; their layout is not read.
AM32 sends a 10-byte KISS packet in their place while `send_telemetry` is set,
and a command frame with the telemetry bit sets it (`dshot.c` 107 to 109;
`main.c` 2272 to 2283): the information goes out on the first pass after the
last such frame. On this bench the KISS receiver assembles 10-byte frames
(`src/kiss_telem.cpp`), so reading the reply is new work. The owner left it out
(decision 8): no slice reads command 6.

## The command path today

As built on `main` at 51f10e0:

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
- **The heartbeat.** A command frame leaves before the heartbeat check
  (`escFrameAction()`, lines 495 to 498): a command already owed goes out
  even if core0 has stopped. Only the throttle obeys the 250 ms rule.
- **Link-up time.** `EscEdtState::linkUpSinceMs` (line 336) holds when the
  ESC started answering, 0 while it does not (`escEdtStep()`, lines 404 to
  407); only the EDT enable reads it today.
- **Not there:** a general command, a save after it, a wait after a
  command, and the time the ESC has been answering.

## The design

### One command slot on core1

The beacon's slot becomes a command slot. `EscPump` keeps one sequence: a
command, its repeats, and an optional save after it.

| Item | Value | Why |
|---|---|---|
| Commands accepted | 1 to 5, 7, 8, 9, 10, 12 | the table above; 6 is left out (decision 8); 13 and 14 stay the enable's own; 20, 21 and 22 to 29 are not offered |
| Repeats, 7 to 10 and 12 | 10, `DSHOT_CMD_REPEATS` | the seventh frame acts on Bluejay and BLHeli_S |
| Repeats, beacons | 6, `BEEP_REPEATS`, as today | every open firmware acts on the first |
| Gap after 7 to 10 and 12, `ESC_CMD_GAP_MS` | 300 ms of value 0 | BLHeli_32's "several hundred ms"; chosen, not measured. On AM32 and Bluejay the 0 frames also end the count; on BLHeli_S a 0 leaves it, and a different command, or the handler once it acts, resets it |
| Gap after beacon n, `ESC_CMD_BEEP_GAP_MS` | 260, 260, 280, 280, 1020 ms | BLHeli_32's beep lengths |
| Link up before a command, `ESC_CMD_SETTLE_MS` | 1500 ms since `linkUpSinceMs` | AM32 takes commands only after more than 1 s of zero input; 500 ms of margin, chosen |
| Motor | eRPM 0 in the last reply | the enable's own rule (`ready`, `src/esc_task.h` line 637) |
| Order | the EDT enable first, then the sequence | as the beacon today: a broken enable reaches no count. `ready` stays false from the sequence's first frame to the end of its last gap, so a scheduled enable cannot land between 8 and 12 |
| Arm | drops the sequence | the rule of `escFrameAction()`; unreachable from the page, which cannot arm |
| Replies within a block | the result `out_dshot_poll()` returns for the previous frame (`src/esc_task.cpp` line 429), passed into the pure slot: `DSHOT_REPLY_GOOD`, eRPM or EDT alike, is that frame's reply. Frames 1 to 6 of every block of 7 to 10 and 12 need one each, read before the next frame; one reply to frames 1 to 5 may be missing or bad, frame 6's may not | no ESC acts before frame 6 (AM32) or 7 (Bluejay, BLHeli_S). The 500 ms link rule is too slow for a 620 ms sequence, and `lastRpmMs` cannot serve: only eRPM replies stamp it (`escFoldReply()`, `src/esc_task.h` line 851), in whole ms, and with EDT on AM32 answers 7 frames in 207, about one in 30, with an EDT frame (current every 40 counts, voltage and temperature every 200, `dshot.c` lines 40 to 42, 246 to 268), so about one hold in three would meet an EDT reply among its 12 checked frames and fail |
| A missing reply beyond that | the block stops at once; 0 frames follow; the sequence ends | see the outcomes below |
| Replies after frame 6 | not read | Bluejay may act from frame 7 on and writes flash with interrupts off, enabling them again only after the write (`DShot.asm` 203 to 209): a gap in the answers then is expected, not measured; AM32 writes flash while it decodes the save's frame 6 (`saveEEpromSettings()`, `dshot.c` 204 to 208), so its replies to the next frames can stop too |
| A request that cannot start | expires after `ESC_CMD_WAIT_MS`, 2000 ms; `escCancelCommand()` ends it at once | a request left waiting would fire later, on another screen |
| Core0 hangs | the sequence finishes | command frames leave before the heartbeat check (above); a sequence carries no throttle and lasts at most 620 ms, or 1020 ms for beacon 5 |

Outcomes, as `escCommandState()` reports them:

| Where the reply went missing | State | Page |
|---|---|---|
| frames 1 to 5 of the first block, the second miss there | `NotSent` | `NOT SENT: THE ESC STOPPED ANSWERING.`: no ESC acted |
| frame 6 of the first block, or later before the save block's frame 6 | `SaveLost` | `SAVE NOT SENT: POWER-CYCLE THE ESC.`: the change may sit in the ESC's RAM; a power cycle drops it. A missing reply to the save block's frame 6 says nothing of an AM32 save: AM32 sends its reply to a frame before it decodes that frame (`transfercomplete()` in `Src/signal.c`, then `processDshot()`), so it may have saved at frame 6 all the same; the power cycle then keeps the change, and the direction check below says which |
| none | `Sent` | the sent lines |
| the request never started | `Expired` | `NOT SENT: THE ESC WAS NOT READY.` |

"NOT SENT" is never shown once frame 6 of the first block has gone out. A
beacon's block is not checked: every open firmware acts on its first
frame, so a beacon reports `Sent` once that frame has gone out.

A hold for DIRECTION or 3D sends the command, 300 ms of 0, then 12, then
300 ms of 0: 620 ms in all.

Core0's side, beside `escRequestBeep()`:

```c
// esc_task.h
enum class EscCmdReady : uint8_t { NoAnswer, Turning, Settling, Busy, Ready };
enum class EscCmdState : uint8_t {
	Idle, Waiting, Sending, Sent, NotSent, SaveLost, Expired };
EscCmdReady escCommandReady();                          // published by core1
bool        escRequestCommand(uint8_t cmd, bool save);  // Ready or Busy only
void        escCancelCommand();                         // a waiting request
EscCmdState escCommandState(uint8_t *cmd);              // last sequence
```

Core1 publishes `escCommandReady()` once per frame slot, from the same
inputs the slot decides on, in this order: `NoAnswer` while no eRPM answer
is younger than `ESC_LINK_STALE_MS`, 500 ms; `Turning` while eRPM is not 0;
`Settling` until 1500 ms after `linkUpSinceMs`; `Busy` while an EDT enable,
a sequence or its gap goes out; `Ready` otherwise. The page draws its
controls live only at `Ready`, and its amber line from the other values.
`escRequestCommand()` refuses while armed and at `NoAnswer`, `Turning` and
`Settling`; at `Busy` the request waits, at most 2000 ms. The rules stay
pure in `src/esc_task.h`, as `escPumpSlot()` is, so the host suite drives
them.

`escRequestBeep(n)` becomes `escRequestCommand(n, false)`. After slice 1,
MENU's BEEP THE ESC therefore shows `REFUSED` while no ESC answers, while
the motor turns and for 1500 ms after an ESC starts answering, where today
it sends the beacon with no check of the link. Beacons are not exempt
from the answer rule (decision 5).

### The page

ESC COMMANDS is a screen with `PinUser::None` and `freePins` true
(`src/ui_screen.h` line 157): it claims no pin and needs DShot running, so
it opens only while the claim is Free. It opens through `uiOpenTool()`
(`src/ui.cpp` line 969), which runs the tester's disarm first
(`testerDisarm()`, line 938). It has no arm control. Core0 sends its
heartbeat on every frame of every screen (`uiTick()`, line 1315), so the
250 ms heartbeat rule holds here for the throttle, as on the tester.

- **BEEP 1 to 5:** a tap each; the screen shows `SENT 3 (BEEP 3).`, and
  every control is drawn disabled for the beep's gap.
- **DIRECTION NORMAL, REVERSED:** a 1000 ms hold each (`ui_hold`,
  `UI_HOLD_MS`); the hold sends 7 or 8, then 12.
- **3D OFF, 3D ON:** a 1000 ms hold each; 9 or 10, then 12.
- While a request waits or a sequence goes out, every control and BACK
  are drawn disabled, as BACK is during a write
  ([menu-b.md](menu-b.md#decisions), decision 5). A wait ends within
  2000 ms; leaving the page by any path calls `escCancelCommand()`.
- A control is live only while `escCommandReady()` reads `Ready`.
  Otherwise the message lines say why, in `C_AMBER`.

The holds write the ESC. Menu structure B puts writes on a write page that
a tap opens ([menu-b.md](menu-b.md#decisions), decision 6). That rule is
written for lists of changes; here one button is one change and names it,
so the hold stays on the page (decision 3).

### Safety and interlocks

| Rule | Where | What it guards |
|---|---|---|
| Disarmed only | `escFrameAction()`; `escRequestCommand()` refuses while armed; `uiOpenTool()` disarms on the way in; the page has no arm control | a command frame in place of a throttle; an ESC ignores commands while it turns |
| eRPM 0 | the slot's `ready`; `Turning` | a prop still coasting after the disarm |
| 1500 ms of answers | the slot; `Settling` | AM32's own arming, about 1 s |
| A reply to frames 1 to 6 of each block, one miss allowed in 1 to 5 | the slot, from `out_dshot_poll()`'s result | a save reported as sent to an ESC that restarted or went |
| A 1000 ms hold for each write | `ui_hold` | a brush of the glass writing the ESC |
| Save in the same hold | the slot's sequence | a direction that reverts on the next power cycle, unseen (decision 2) |
| BACK disabled while waiting or sending | the page; the wait expires after 2000 ms | leaving mid-sequence, as during a write, and a request that fires later on another screen |
| The 3D note | `uiAm32Esc3d()` today | a tester throttle whose lower half runs the motor reversed at twice its scale |
| No answer, no command | the slot | a command to nothing, read as done (decision 5) |

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
(decision 4).

The flag is not stored (`s_esc3d` is a static in RAM): a restart of the
bench drops the note while the ESC stays in 3D, saved by the 12 that
followed the 10. The page says so after 3D ON (`THE NOTE ENDS AT A
RESTART.`); the flag is not stored in the settings (decision 4).

The note's text, `3D MODE ESC: THROTTLE SCALE DIFFERS`, does not say that
the lower half runs reversed. Slice 2 changes it to `3D MODE ESC: 0-50%
REVERSED AT 2X` (33 characters, 197 px, from x 10 to 206, inside the 226
of its `static_assert`), a shipped text of PR #55 (decision 9). The tests,
the previews and the wiki that quote the old text change with it.

**Direction.** The bench cannot see it: an eRPM reply is a period with no
sign (`src/dshot/dshot.h`, "carrying an electrical period rather than a
speed"). The pilot checks it:

1. 3D off: in 3D the tester's 0 to 50 % runs the motor reversed, so the
   check would read the opposite direction. With the 3D note standing,
   3D OFF on the page first.
2. Propeller off, a strip of tape on the bell.
3. On the tester, arm and run at 5 % (`MAX_THROTTLE_STEP` 100 is 5 %) for a
   second; watch the tape; disarm.
4. On AM32, the save plays beacon 1's tone for normal and beacon 2's for
   reversed (`dshot.c` 204 to 208); BEEP 1 and BEEP 2 on the page play the
   same tones for comparison. AM32 CONFIG then reads the `DIRECTION` and
   `3D MODE` fields of its MOTOR group back (`src/am32_eeprom.cpp` lines 90
   to 94).
5. After a power cycle of the ESC, step 3 again: a direction that holds
   was saved.

The tester shows no note after a REVERSED hold (decision 10): the ESC's
direction is the pilot's setting, and a note that a restart drops says
nothing about the ESC on the pin.

On BLHeli_32, byte 17 of the ESC information would say it; the reply is
not read (decision 8).

### MENU entry

Since PR #66, MENU has seven rows: BEEP THE ESC, AM32
CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS, at a
42 px pitch in a 258 px viewport, so MENU scrolls 36 px
([menu-b.md](menu-b.md#menu), decision 33). The page needs a way in, and
each way changes something menu-b.md decides. The owner chose option (a)
(decision 1). The options:

| Option | MENU | Taps to a direction change | Changes |
|---|---|---|---|
| (a) ESC COMMANDS in place of BEEP THE ESC | 7 rows; `ESC COMMANDS`, 12 characters, 142 px, x 16 to 157, the width of `BEEP THE ESC`; no status | MENU, ESC COMMANDS, hold: 2 taps and a hold | BEEP moves onto the page and takes a tap more; [menu.md](menu.md#decisions) decision 2 and menu-b decisions 16 and 23 name BEEP THE ESC as MENU's first row |
| (b) an eighth row after BEEP THE ESC | 8 rows: 8 × 42 − 258 = 78 px of scroll; SETTINGS starts at y 356 at scroll 0 | 2 taps and a hold | decision 33's seven rows |
| (c) an ESC chooser in AM32 CONFIG's row, like PROGRAMMER, with AM32 CONFIG and ESC COMMANDS | 7 rows | 3 taps and a hold; AM32 CONFIG one tap deeper | decision 16's row, and the chooser claims nothing while AM32 CONFIG claims SIG |
| (d) a fourth button on the tester | 7 rows | 1 tap and a hold | the tester's button row, 6/104, 116/54, 176/58 (menu-b "The tester"), has no room; the commands sit beside HOLD TO ARM |
| (e) a tester readout: a tap on `DSHOT600` in the status bar (x 104 to 156, y 3) while disarmed opens the page, as `14 POLES` and `MAX 20%` open SETTINGS | 7 rows | 1 tap and a hold | the tester's status bar and slice 7's readout rule; its target rectangle is to be designed, beside the SD log state below it at y 14 |

Chosen: (a), the recommendation (decision 1). Every BEEP is a DShot
command, and the page carries all five. `ESC COMMANDS` is MENU's first
row. The row count of MENU after the decisions of 2026-11-15 on the other
notes is eight, and the resulting list is the revision line in
[menu-b.md](menu-b.md#menu).

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
  268 | CHECK IT ON THE TESTER WITH 3D OFF.       |  y 244 to 304,
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
| Idle | `HOLD 1 S: SETS IT, THEN SAVES IT.` (197 px); `THE ESC SENDS NO ANSWER TO A COMMAND:` (221 px); `CHECK IT ON THE TESTER WITH 3D OFF.` (209 px); blank; `AM32 BEEPS AS BEEP 1 ON SAVE (NORMAL)` (221 px); `OR AS BEEP 2 (REVERSED).` (143 px) | `C_DIM` |
| Sending a direction | `SENDING 8 (REVERSED), THEN 12 (SAVE).` (221 px) | `C_TEXT` |
| Sent a direction | `SENT 8 (REVERSED), THEN 12 (SAVE).` (203 px) or `SENT 7 (NORMAL), THEN 12 (SAVE).` (191 px); `NOTHING CONFIRMS THE ESC TOOK THEM.` (209 px); `CHECK IT ON THE TESTER WITH 3D OFF.` (209 px) | `C_TEXT` |
| Sending 3D | `SENDING 10 (3D ON), THEN 12 (SAVE).` (209 px) or `SENDING 9 (3D OFF), THEN 12 (SAVE).` (209 px) | `C_TEXT` |
| Sent 3D ON | `SENT 10 (3D ON), THEN 12 (SAVE).` (191 px); `THE TESTER SHOWS THE 3D NOTE NOW.` (197 px); `IN 3D, 0 TO 50% RUNS REVERSED AT 2X;` (215 px); `ABOVE 50% THE SET WAY, FROM LOW.` (191 px); `THE NOTE ENDS AT A RESTART.` (161 px), as the flag is not stored (decision 4) | `C_TEXT`, the last three `C_AMBER` |
| Sent 3D OFF | `SENT 9 (3D OFF), THEN 12 (SAVE).` (191 px); `THE 3D NOTE STAYS UNTIL A RESTART.` (203 px), by decision 4 | `C_TEXT` |
| A beacon | `SENDING 3 (BEEP 3).` (113 px), then `SENT 3 (BEEP 3).` (95 px) | `C_TEXT` |
| No answer | `NO ESC ANSWERS ON SIG GP0.` (155 px) | `C_AMBER` |
| Motor turning | `MOTOR TURNING: WAIT UNTIL IT STOPS.` (209 px) | `C_AMBER` |
| Settling | `ESC STARTING: WAIT 1.5 S.` (149 px) | `C_AMBER` |
| Not sent | `NOT SENT: THE ESC STOPPED ANSWERING.` (215 px), only while frame 6 of the first block has not gone out | `C_AMBER` |
| Save lost | `SAVE NOT SENT: POWER-CYCLE THE ESC.` (209 px); `CHECK IT ON THE TESTER WITH 3D OFF.` (209 px) | `C_AMBER` |
| Expired | `NOT SENT: THE ESC WAS NOT READY.` (191 px) | `C_AMBER` |

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
approximations, and the first is wrong by a factor on AM32 2.22, whose
duty, `duty_cycle_setpoint`, runs to 2000 at full:

- **Without sine start:** duty = `map(input, 47, 2047, minimum_duty_cycle,
  2000)` with input the DShot value (`main.c` lines 1363, 1384).
  `minimum_duty_cycle` is the MIN DUTY setting × 10, 0 to 500, that is 0 %
  to 25 % (`main.c` lines 662 to 666). Two more terms add to it: with the
  driving brake strength below 10, `dead_time_override` =
  `DEAD_TIME + (150 − 10 × strength)`, capped at 200 (lines 711 to 721),
  where `DEAD_TIME`
  is a constant of each target that the bench cannot read; and on targets
  built with `USE_STARTUP_BOOST`, 50 + PWM frequency × 50 / 24 (lines 2081
  to 2085). In AM32 2.22 six targets set it, all on the AT32F421
  (`Inc/targets.h` lines 992, 1451, 1558, 1601, 1614, 2038); the settings
  AM32 CONFIG reads do not name the target. AM32 CONFIG reads the
  strength as BRAKE RUNNING LEVEL at 0x2A (`src/am32_eeprom.cpp` line 129),
  AM32's `driving_brake_strength`, byte 42 (`Inc/eeprom.h` line 55); 0 and
  values above 9 count as 10.
- **With sine start:** below the changeover, SINE RANGE × 20, 5 % to 25 %
  (`main.c` line 704), the value is remapped onto 47 to 160 for the sine
  start; from the changeover it is remapped onto 160 to 2047 (lines 1322 to
  1333), and duty = `map(input, 137, 2047, minimum_duty_cycle + 40, 2000)`
  (line 1382). Duty starts at 40, 2 %, at input 137, and is about 64,
  3.2 %, at the changeover's input 160.

The factor the estimate is off by is the true duty over t / 2000, computed
from those lines for the tester's t (input t + 47), with RUNNING LEVEL 10
and a target without `USE_STARTUP_BOOST`; the rated kV times the
factor is what the estimate reads:

| t | Sine start, changeover 25 %, MIN DUTY 0 | No sine start, MIN DUTY 5 % | No sine start, MIN DUTY 0 |
|---|---|---|---|
| 30 % (600) | × 0.41 | × 1.12 | × 1.00 |
| 50 % (1000) | × 0.75 | × 1.05 | × 1.00 |
| 75 % (1500) | × 0.92 | × 1.02 | × 1.00 |
| 100 % (2000) | × 1.00 | × 1.00 | × 1.00 |

AM32 CONFIG reads SINE STARTUP, SINE RANGE and MIN DUTY (`src/am32_eeprom.cpp`
lines 118, 119, 178), so after a read the factor is known for AM32; the estimate
does not use that read (decision 7): it is shown from 75 % throttle on every
firmware. How Bluejay, BLHeli_S and BLHeli_32 map throttle onto duty is not
verified. In 3D the assumption is false (above).

In integers, as the tester holds them: `rpm` from the filtered eRPM over
half the pole count, the figure the big readout shows (`drawRpm()`,
`src/ui.cpp` line 672); `volts100`, hundredths of a volt, from the merged
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
| The duty assumption | AM32: the factor table above; others unknown | up to × 0.41 at 30 % on AM32 with sine start; unknown for the others |

Example: a 1,750 kV motor on 14 poles at 16.0 V and t = 1000 (50 %) turns
at about 14,000 RPM in the ideal case; RPM/V reads 875 and the estimate
1,750. The eRPM step there is 0.33 %, ±0.17 % about the true value; with
EDT voltage, ±0.78 %, the estimate's quantisation is about ±0.95 %, with
KISS about ±0.2 %. The motor's own losses lower both, by an amount not
known here. With MIN DUTY at its 25 % maximum and no sine start, AM32's map
puts the estimate at × 1.08 at 75 %, outside the table's columns.

### When it is shown

All of these, else `KV EST --`:

1. Armed, the link alive, eRPM above 0, the pole count 2 or more.
2. A voltage fresh from KISS or EDT, above 0.5 V (rcbench's floor,
   `motor_draw.c` line 178).
3. t at least `KV_EST_MIN_THROTTLE`, by decision 7:
   1500, 75 %, where the AM32 factors in the table
   are × 0.92 to × 1.02, and × 1.08 with MIN DUTY at 25 %. Below it the
   field reads `KV EST 75%+` (11 characters, 65 px). At the default
   ceiling, 20 %, the estimate does not show; the pilot raises `MAX` in
   SETTINGS for the reading, propeller off.
4. t unchanged for `KV_EST_SETTLE_MS`, 1000 ms: the display filter's
   residue is then 0.48 % of the step; the motor's spin-up time is not
   measured.
5. The 3D note not standing.

RPM/V needs 1 and 2 only. Neither is held after the disarm (decision 7).
Neither takes the duty factor into account.

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

Layout of decision 6; the rest of the tester
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
| `KV EST 75%+`, `KV EST --` | 11, 9 | 65, 53 px | x 10 | |

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
  an arm; the reply results fed in per frame: an EDT reply in each of
  frames 1 to 6 counted as a reply, one missing or bad reply in frames 1 to
  5 passed, a second, and a missing or bad reply to frame 6, ending the block,
  each frame of each block in turn, with the outcome of the table
  (`NotSent` up to frame 5 of the first
  block, `SaveLost` after it) and no answer read after frame 6; no
  scheduled EDT enable between the two blocks; a request at `Busy` that
  expires at 2000 ms and not at 1999 ms, and one cancelled; a sequence
  that finishes with the heartbeat lapsed; a beacon's gap of 260 ms and
  1020 ms; no KISS request while a command is owed; across a `millis()`
  wrap.
- **The readiness,** `escCommandReady()` at each boundary: `NoAnswer` at
  500 ms after the last answer and not at 499 ms; `Turning` at eRPM 1;
  `Settling` up to 1499 ms after `linkUpSinceMs`; `Busy` during an
  enable, a block and a gap; `Ready`; and the refusals of
  `escRequestCommand()` for each, MENU's BEEP included.
- **Model ESCs,** in the manner of the EDT tests: an AM32 rule (acts on the
  sixth equal frame, only after 1 s of zero input, a 0 frame clears, a lost
  or failed frame leaves the count, answers every frame it captures, before
  decoding it), a Bluejay rule (acts on the seventh, a 0 frame or a frame
  without the telemetry bit clears, a lost or failed frame leaves the count,
  no answers while it writes flash) and a BLHeli_S rule (acts on the
  seventh, a 0 frame leaves the count); each takes the bench's sequence, and a
  sequence of 6 repeats fails the second, so a mutation to 6 is caught; the
  one-miss test uses an ESC that missed the frame itself.
- **The page,** by control id: opens from its MENU entry with the claim
  Free and not otherwise; disarms on the way in; the holds at 1000 ms and
  not at 999 ms; every control and BACK disabled while sending and for a
  beep's gap and while a request waits; `escCancelCommand()` on leaving;
  each state's lines, `NOT SENT` never after frame 6 of the first block;
  every string within its rectangle; the 3D note set by 3D ON and not
  cleared by 3D OFF (decision 4); the preview
  `docs/esccmd-preview.png`.
- **The estimate,** `kv_est.h`: both formulas at the clamp, at 0.5 V and
  `KV_EST_MIN_THROTTLE` and their neighbours; the settle at 1000 ms and not at
  999 ms; hidden with the 3D note, disarmed, with no voltage, with fewer than 2
  poles; the tester's two lines and their fits.
- Every new rule mutation-checked: break it, see a test fail, restore.

### Bench

Only an ESC settles these, one per firmware at hand:

- B1. Direction, with 3D off: REVERSED, tape on the bell, 5 % on the
  tester; power-cycle the ESC; still reversed; NORMAL back.
- B2. AM32: the save's tone against BEEP 1 and BEEP 2; AM32 CONFIG reading
  the MOTOR group's `DIRECTION` and `3D MODE` as sent.
- B3. 3D ON: the note on the tester; at 10 % the motor runs as at about
  20 % and reversed; above 50 % in the set direction, from low; 3D OFF; a
  power cycle; the note after a bench restart (decision 4).
- B4. BEEP 1 to 5 on each firmware; whether BLHeli_32 takes a command
  within its beep lengths; on Bluejay, which turns interrupts off for the
  tone and 200 ms after it (`DShot.asm` 90 to 94), how long the answers
  stop and whether the readiness falls to `NoAnswer` and `Settling`.
- B5. On a logic analyser at SIG: 10 frames of 8, 300 ms of 0, 10 of 12;
  the EDT enable never broken by a sequence.
- B6. An ESC plugged in with the page open: the first command waits
  1500 ms and AM32 acts on it.
- B7. A BLHeli_S 16.7 ESC, if one is at hand: whether it answers or
  starts at all on inverted DShot.
- B8. kV: a motor of known rated kV, propeller off, at 30 %, 50 %, 75 % and
  100 %, with EDT and with KISS voltage, on AM32 with SINE STARTUP on and
  off; `KV EST` and `RPM/V` against the rating and against the factor
  table, and the RPM against an optical tachometer if one is at hand.
- B9. A link loss during a sequence: the ESC's supply cut during the first
  block and during the save block; the page's outcome lines.

## Work breakdown

Each slice builds, passes the host suite and merges alone. Sizes are new
and changed lines, rough.

| Slice | Content | Code | Tests | Docs | Depends on |
|---|---|---|---|---|---|
| 1. Command slot | `EscPump`'s sequence, `escCommandReady()`, `escRequestCommand()`, `escCancelCommand()`, `escCommandState()`, the answer check per block, the settle, gap and wait constants in `src/config.h`, `escRequestBeep()` through it; the model ESCs | about 200 | about 450 | README's MENU table row on BEEP THE ESC and its EDT paragraph, STATUS, CHANGELOG | |
| 2. ESC COMMANDS page | `src/ui_esc_cmd.{h,cpp}`, `src/esc_3d.{h,cpp}` and `ui_am32.cpp` on it, the MENU entry by decision 1, `test_ids.cpp`, the preview, `tools/make_previews.py`, `tools/check_wiki.py`, README section, wiki pages in English and German, STATUS, CHANGELOG, this note's status, the 3D note text change of decision 9 with its tests, previews, wiki and `src/ui_am32.h` | about 450 | about 500 | about 250 | 1 |
| 3. kV estimate | `src/kv_est.h`, `drawRpm()` in `src/ui.cpp`, the tester previews, README and wiki on the tester in both languages | about 120 | about 250 | about 120 | none |
| 4. ESC information | left out (decision 8) | | | | 1 |
| 5. Bring-up | B1 to B9 | none | none | the record in STATUS | 1 to 3 |

Slices 1 and 3 share no source file. Both edit README.md (slice 1 the
MENU table's BEEP row, slice 3 the tester's description), STATUS.md and
CHANGELOG.md; whichever merges second resolves those hunks. They can merge
in either order.

### Slice 1 as built

`src/esc_task.h` holds the rules, pure: `EscCmdReady`, `EscCmdState`,
`escCmdValid()`, `escCmdAccept()`, `EscCmdRequest`, the sequence
`EscCmdSeq` with `escCmdStart()`, `escCmdCheckReply()`,
`escCmdFrameSent()`, `escCmdInterrupt()` and `escCmdStopState()`,
`escCmdReadiness()`, `escPumpRelease()`, `escFrameWire()`, and
`escPumpSlot()` with the request and the reply as parameters in place of the
beacon request. `EscPump` keeps the sequence in `seq` and the readiness in
`ready`; the beacon's `beepCmd` and `beepReps` are gone. `escRequestBeep(n)`
is an inline in the header: it clamps `n` to 1 to 5 and calls
`escRequestCommand(n, false)`. `src/esc_task.cpp` does the I/O: the request
under the shared critical section, `escCancelCommand()`,
`escCommandReady()`, `escCommandState()`, the reply of `out_dshot_poll()`
handed to the slot, `escPumpRelease()` at a teardown, and one locked
publish of the state after each frame. `src/config.h` has
`ESC_CMD_GAP_MS` (300), `ESC_CMD_BEEP_GAP_MS` (260, 260, 280, 280, 1020),
`ESC_CMD_SETTLE_MS` (1500) and `ESC_CMD_WAIT_MS` (2000). The host suite's
`test/test_esc_cmd.cpp` drives the slot against the model ESCs; the EDT
tests of `test/test_ui.cpp`, the handover of `test/test_claim.cpp`, MENU's
BEEP in `test/test_menu.cpp` and the fakes take the new interface. No page
is built; MENU's BEEP THE ESC is the only caller.

Choices the note left open:

- **Which requests.** 1 to 5 and 12 go alone; 7 to 10 only with the save
  after them (`escCmdValid()`), so no caller can send a change that reverts
  at the ESC's next power cycle (decision 2). `escRequestCommand(7, false)`
  is refused.
- **A stopped block** is followed by its gap of 0 frames, 300 ms after 7 to
  10 or 12, and the sequence ends after it, with no save block. The outcome
  is set at the stop. The readiness reads Busy through that gap, so a retry
  or a scheduled EDT enable keeps the gap from the frames before.
- **When the outcome is Sent.** A beacon from its first frame. A sequence of
  7 to 10 or 12 once its last block's tenth frame has gone out with the
  check passed: Bluejay and BLHeli_S act on the seventh frame, so frame 6's
  reply alone does not make it sent. A stop after frame 6 of the first
  block, and before that tenth frame, is SaveLost.
- **An arm, a bind, a teardown,** and a drain frame inside a block, which
  only a resume during the drain can send there, stop the sequence at once,
  without a gap: the frames that follow are the throttle or the pin's next
  user. The outcome follows the same rule, NotSent before frame 6 of the
  first block, SaveLost after it; an outcome already set stays. An arm also
  drops a waiting request, reported as Expired.
- **The allowance is per block.** One missing or bad reply among frames 1 to
  5 of each block; the count starts again at the save block. The reply read
  on a block's first slot answers the frame before the block, a 0 frame,
  and is not counted.
- **Gaps are counted in frames,** one per slot. A slot core1 skips makes a
  gap longer in time, never shorter.
- **KISS requests in a gap.** Only a block's command frames hold the KISS
  requests back; the gap's 0 frames carry them at the usual cadence of 1 in
  20.
- **The wait** counts from core0's `millis()` stamp of the request. Core1
  reads the request before its clock and compares the age signed, so a
  request stamped after the slot's clock reads as new, not 2^32 − 1 ms
  old. A request that waits is replaced by a newer one; a generation count
  keeps core1 from clearing a request it did not read.
- **`escCancelCommand()`** removes a waiting request; a sequence already
  going out goes on. `escCommandState()` then reads the last sequence's
  outcome, or Idle before any.
- **The published readiness** is the one after the slot's decision: Busy
  from the slot a request starts, and Busy for one slot after the last gap
  frame, the slot whose frame ended the gap. It reads NoAnswer before
  core1's first slot and from a teardown until the next bind's ESC answers,
  so a BEEP while the pin is claimed is refused.
- **The telemetry bit** is set in `escFrameWire()`, pure, which
  `escTaskPoll()` sends from, so the host suite checks it.

MENU's BEEP THE ESC shows `REFUSED` while no ESC answers, while the motor
turns and for 1500 ms after an ESC starts answering, as the note says.
`SENT` means the request was taken, waiting included: a beacon taken at Busy
that then cannot start, its ESC unplugged within the wait, expires unseen
on MENU. Nothing else on existing screens changes.

The model ESCs of `test/test_esc_cmd.cpp`, from the rules table above: AM32
acts on the sixth equal frame, a beacon on the first, only after more than
1000 frames of zero input, a 0 frame clears its count, a lost or damaged
frame leaves it, and it answers a damaged frame, before decoding it; Bluejay
acts on the seventh, clears on a 0 frame and on a command frame without the
telemetry bit, and does not answer a damaged frame (not verified);
BLHeli_S acts on the seventh and keeps its count over a 0 frame. Each
clears its command after acting. BLHeli_S 16.7 sends no eRPM; the model
answers as a stand-in so that its count meets the bench's sequence, and one
case keeps it silent and sees nothing sent. The silences after a save, 20
frames, and after a beacon, 200 frames on Bluejay and 100 on BLHeli_S, are
the model's, not measured. The cases: a hold's 10 frames of 8, 300 of 0, 10
of 12 and 300 of 0, each command frame with the telemetry bit, no KISS
request in a block; every model reversed, in 3D, back, and saved, AM32 on
its sixth frame and Bluejay and BLHeli_S on their seventh; six frames act
on AM32 only; a lost or damaged frame 1 to 5 of either block on every
model; the reply check at each of frames 1 to 10 of both blocks, once and
after a first miss, missing and bad, with its outcome, its frames and its
gap; EDT replies in frames 1 to 6; no reply read after frame 6; 12 alone; a
beacon unanswered; an arm at five places and while a request waits; eRPM
not 0; the settle at 1499 and 1500 ms with an AM32 plugged in; the wait at
1999 and 2000 ms from Busy; a stamp 1 ms ahead; a cancel; the heartbeat
lapsed; the EDT enable before a sequence and after its last gap; each
beacon's gap; a sequence and a wait across the `millis()` wrap; a bind, a
teardown and a drain frame.

Not changed, found while building: `escFoldReply()` stamps `lastRpmMs` with
`millis()`, and 0 there means "never". The slot at `millis()` 0, once every
2^32 ms, about 49.7 days, reads the link as down, which resets
`linkUpSinceMs` and with it the 1500 ms settle and the EDT enable's 300 ms.
A sequence under way is not affected: within a block only the replies
count.

## Decisions

Decisions of the owner, 2026-11-15, answering the open questions of
2026-11-14:

1. **Where the page lives.** Option (a): `ESC COMMANDS` in place of `BEEP
   THE ESC`, MENU's first row, with BEEP 1 to 5 on the page. This is the
   recommendation. The other decisions of the day make MENU eight rows;
   see [menu-b.md](menu-b.md#menu).
2. **Save with each change.** Option (a): each hold sends the change, then
   12 (save). This is the recommendation.
3. **A hold on the page, or a write page.** Option (a): the 1000 ms hold
   on the page. This is the recommendation.
4. **The 3D note after 3D OFF, and across a restart.** Option (a): it
   stays until the bench restarts or an AM32 list read shows 3D MODE off.
   This is the recommendation.
5. **Commands with no ESC answering, and beacons.** Option (a): refused
   with `NO ESC ANSWERS ON SIG GP0.`, beacons included. A BLHeli_S 16.7
   ESC, which sends no eRPM, is then not served. This is the
   recommendation.
6. **Where RPM/V and the estimate go on the tester.** Option (a): `RPM/V`
   on the ERPM line and `KV EST` in place of `14 POLES` while armed. This
   is the recommendation.
7. **The estimate's minimum throttle, and after the disarm.** Option (b):
   75 %. The estimate keeps no value after the disarm. This is the
   recommendation, the second part as the note advised.
8. **ESC information, command 6.** Option (b): left out. The slice that
   reads the reply is not built. No recommendation was given.
9. **The tester's 3D note text.** Option (b): `3D MODE ESC: 0-50%
   REVERSED AT 2X`. This is the recommendation.
10. **A direction note on the tester.** Option (b): nothing. This is the
    recommendation.

## Open questions

None open. Questions 1 to 10 are answered by decisions 1 to 10.

## Not verified

- BLHeli_32's behaviour beyond its 46-line specification text: whether
  "6x" means the sixth or the seventh frame, what 7 to 10 do without 12,
  how 20 and 21 differ from 7 and 8, its 3D scaling and its duty map.
- Whether a BLHeli_S 16.7 ESC takes inverted DShot frames.
- AM32 versions other than 2.22 at 2a695a8, among them the 2.21 that
  rcbench records on hardware.
- Bluejay's and BLHeli_S's duty maps; whether Bluejay's answers stop while
  it writes flash or plays a beacon, and for how long.
- AM32's 49-byte information layout.
- That 300 ms between commands suffices for BLHeli_32, and that 1500 ms of
  answers suffices for every AM32 build.
- A motor's spin-up time after a throttle step, and the size of the
  no-load losses and the timing effect on the estimate.
- Everything on hardware: no command of this design, no direction change
  and no estimate has run on an ESC or a motor from this board.
