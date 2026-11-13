# Diagnostics

Built, host tests only, not tested on hardware. SETTINGS ends with a
DIAGNOSTICS section of two read-only rows: LAST RESTART, why the chip last
restarted, and UPTIME, the time since boot. The firmware version is the
VERSION row under BOARD and on the splash. A persistent fault log is not
built; its open questions are at the end.

## What is built

| Part | Where |
|---|---|
| Decoding, texts, uptime text | `src/diag.h`, `src/diag.cpp` |
| Capture at boot | `diagCaptureRestart()`, called in `setup()` after the power latch on GP26 (general-purpose input/output 26) |
| Rows | `src/ui_settings.cpp`, section DIAGNOSTICS, after BOARD |
| Host tests | `test/test_diag.cpp` (`runDiagTests`), `testSettingsDiagnostics` in `test/test_menu.cpp` |
| SDK (software development kit) fakes | `test/stubs/hardware/structs/powman.h`, `test/stubs/hardware/structs/watchdog.h`, `test/stubs/hardware/watchdog.h`, `test/stubs/pico/bootrom.h`, `test/fakes.cpp` |

## Inputs

Four values, read once, from the Pico SDK 2.1.1:

- `powman_hw->chip_reset`, the power manager's CHIP_RESET register at offset
  0x2c (`hardware/regs/powman.h`, `hardware/structs/powman.h`). Its HAD_*
  bits, 16 to 28, say which reset the power manager saw. The header marks
  them read-only.
- `watchdog_hw->reason`, the watchdog's REASON register
  (`hardware/regs/watchdog.h`, `hardware/structs/watchdog.h`): bit 0 TIMER,
  bit 1 FORCE, both zero after a hardware reset.
- `watchdog_enable_caused_reboot()`: true when REASON is non-zero and
  watchdog scratch register 4 holds the value `watchdog_enable()` writes,
  0x6ab73121.
- `rom_get_last_boot_type()` (`pico/bootrom.h`): the boot ROM's (read-only
  memory's) type of the last boot, `BOOT_TYPE_NORMAL` 0, `BOOT_TYPE_BOOTSEL`
  2, `BOOT_TYPE_RAM_IMAGE` 3, `BOOT_TYPE_FLASH_UPDATE` 4, `BOOT_TYPE_PC_SP`
  0xd, or a negative error code.

`diagDecodeRestart()` takes the four as arguments and reads no register,
so the host suite runs it on every branch. In the firmware build,
`static_assert`s hold `DIAG_CR_*` and `DIAG_BOOT_TYPE_FLASH_UPDATE` in
`diag.h` to the SDK's `POWMAN_CHIP_RESET_*_BITS` and
`BOOT_TYPE_FLASH_UPDATE`. The SDK's own `watchdog_caused_reboot()` is not
used: it is true only after a boot of type `BOOT_TYPE_NORMAL`, so it would
hide the reboot after a flash update.

## Decoding

The first match decides.

| Order | Input | Text | Colour |
|---:|---|---|---|
| 1 | `watchdog_enable_caused_reboot()` | `WATCHDOG` | amber |
| 2 | REASON non-zero and boot type 4, `BOOT_TYPE_FLASH_UPDATE` | `UPDATE` | grid |
| 3 | REASON non-zero, any other boot type or an error | `REBOOT` | grid |
| 4 | HAD_GLITCH_DETECT, bit 26 | `GLITCH` | amber |
| 5 | HAD_WATCHDOG_RESET_RSM, bit 28; HAD_WATCHDOG_RESET_SWCORE, bit 24 | `WATCHDOG` | amber |
| 6 | HAD_HZD_SYS_RESET_REQ, bit 27 | `DEBUGGER` | grid |
| 7 | HAD_SWCORE_PD, bit 25 | `POWER-DOWN` | grid |
| 8 | HAD_POR, bit 16 | `POWER-ON` | grid |
| 9 | HAD_BOR, bit 17 | `BROWN-OUT` | amber |
| 10 | HAD_RUN_LOW, bit 18 | `RUN PIN` | grid |
| 11 | HAD_DP_RESET_REQ, bit 19; HAD_RESCUE, bit 21 | `DEBUGGER` | grid |
| 12 | HAD_WATCHDOG_RESET_POWMAN, bit 23; HAD_WATCHDOG_RESET_POWMAN_ASYNC, bit 22 | `WATCHDOG` | amber |
| 13 | none | `UNKNOWN` | grid |

The watchdog comes first. `watchdog_enable()` and `watchdog_reboot()` in
the SDK select the reset through the power-on state machine (PSM) only
(`psm_hw->wdsel`), not through the power manager's own WDSEL register. A
reset of that kind need not change CHIP_RESET, which then still reads the
reset before it. Whether it does is not known: the SDK headers do not say,
and it is not measured. It is also not known whether a reset that leaves
the watchdog block alone (GLITCH, possibly HZD_SYS_RESET_REQ) clears the
watchdog's REASON register. If it does not, a UF2 copy followed by a glitch
reset reads `UPDATE`, or `REBOOT`, instead of `GLITCH`.

Orders 4 to 7 and 8 to 12 split the flags by what `hardware/regs/powman.h`
says each reset does to the power manager ("powman no" or "yes"). GLITCH,
WATCHDOG_RESET_RSM, HZD_SYS_RESET_REQ, SWCORE_PD and WATCHDOG_RESET_SWCORE
leave it alone; POR, BOR, RUN_LOW, DP_RESET_REQ, RESCUE and the two
WATCHDOG_RESET_POWMAN flags reset it. Whether the chip clears older HAD_*
flags when it records a new one is not known. If the flags add up, a flag of
the first group is newer than any of the second, so it decides: POR and
GLITCH together read `GLITCH`, not `POWER-ON`. Within each group the order
is a choice, not a fact of the chip.

RESCUE_FLAG (bit 4) and DOUBLE_TAP (bit 0) are not reasons and are ignored.

## What each text means

- `POWER-ON`, `BROWN-OUT`, `RUN PIN`, `GLITCH`, `POWER-DOWN`: the reset the
  CHIP_RESET flag names: power applied, the supply below the brown-out
  level, the RUN pin pulled low, the glitch detector, the switched core
  powered down.
- `DEBUGGER`: a reset from an attached debugger, Arm or RISC-V.
- `WATCHDOG`: an armed watchdog timed out.
- `UPDATE`: the reboot after a UF2 copy. UF2 is the Universal Serial Bus
  (USB) Flashing Format: a firmware file copied onto the drive the boot ROM
  shows in BOOTSEL mode, entered by holding the BOOT button while the board
  powers up. According to the RP2350 boot ROM source, not in this tree and
  not verified on hardware, the boot ROM reboots through the watchdog with
  the reboot type `REBOOT2_FLAG_REBOOT_TYPE_FLASH_UPDATE`, clears scratch
  register 4 and selects only the power-on state machine. The boot type is
  then `BOOT_TYPE_FLASH_UPDATE`.
- `REBOOT`: any other reboot through the watchdog, which another program
  asked for, for example picotool.
- `UNKNOWN`: none of the inputs set.

## Watchdog

The firmware arms no watchdog: nothing calls `watchdog_enable()`.
`WATCHDOG` from order 1 therefore cannot appear with this firmware. The
flags of orders 5 and 12 follow the power manager's WDSEL register
(RESET_RSM, RESET_SWCORE, RESET_POWMAN, RESET_POWMAN_ASYNC), which this
firmware does not set either. The watchdog reason can only appear once a
watchdog is armed. Nothing in the firmware calls `watchdog_reboot()`, so
`REBOOT` shows only after a reboot that another program requested.

## Uptime

`diagUptimeSeconds()` divides the SDK's 64-bit microsecond timer by
1,000,000, so it does not wrap with `millis()` after 49.7 days. The text is
`H:MM:SS` with the hours not capped, at most 13 characters
(`1193046:28:15`). SETTINGS repaints once a second while any of the UPTIME
row shows, and not while it is scrolled out.

## Layout

DIAGNOSTICS adds a 20 px caption and two 50 px rows. The list's content is
820 px tall; it scrolls from 0 px to 620 px. At 620 px VERSION is at y 70,
LAST RESTART at y 140 and UPTIME at y 190. Both values are right-aligned to
x 232 at scale 1, as BUILT FOR and VERSION are.

## Not tested on hardware

Nothing of this has run on a chip. Bench checks needed, one restart each,
reading LAST RESTART after it:

- power applied from off: `POWER-ON` expected;
- the RUN pin pulled low: `RUN PIN` expected;
- a supply dip below the brown-out level: `BROWN-OUT` expected;
- a reset from an attached debugger: `DEBUGGER` expected; an Arm SYSRESETREQ
  may not reach the power manager, and what it shows is not known;
- a UF2 copy in BOOTSEL mode: `UPDATE` expected;
- `picotool reboot`: `REBOOT` expected;
- a power-on, then a glitch or a debugger reset: whether CHIP_RESET then
  holds one flag or two;
- a UF2 copy, then a glitch reset: `GLITCH` if the glitch reset clears
  REASON, `UPDATE` or `REBOOT` if it does not.

## Open questions for a fault log

A fault log would keep records across restarts. Before one is built, the
owner decides:

1. **What counts as a fault.** Candidates: a restart other than POWER-ON; the
   core1 heartbeat trip (`UI_HEARTBEAT_TIMEOUT_MS`, 250 ms); a failed settings
   save; `DSERR`; a card error or a failed flush of the SD (Secure Digital)
   write cache; a lost servo or a write result in red on KST SERVO or HITEC
   SERVO; a failed AM32 write. Which of these, and whether a warning counts too.
2. **What one record holds.** The reason or event code, the uptime at the
   time, the firmware version, a restart counter, and per event a value
   such as a pin or a result code. Whether the time of day is needed: the
   board's PCF85063 RTC (real-time clock) is not used, so the uptime is the
   only clock.
3. **Where it is stored.** Options: a flash sector of its own beside the
   settings sector; the SD card, in a file under its own directory; the
   watchdog scratch registers or the power manager's scratch registers,
   which survive some resets but not a power-on. Flash writes park core1
   and so are refused while armed or during a pin claim, as a settings save
   is; a record raised at such a time needs a place to wait.
4. **Size.** How many records, and what happens when it is full: a ring
   that overwrites the oldest, or a stop.
5. **Wear.** A flash sector takes a rated number of erase cycles, from the
   flash part's datasheet, not looked up here. A restart loop could erase
   on every boot. A rule is needed, for example append within the sector and
   erase only when it is full, or at most one record per boot.
6. **How it is shown.** A DIAGNOSTICS page of its own with a list, or a
   count on the LAST RESTART row that opens a list; whether it is also sent
   over the USB serial port.
7. **How it is cleared.** A hold on the page, as HOLD TO SAVE works, or
   RESET on SETTINGS, or never; whether a clear is itself recorded.
