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
| SDK (software development kit) fakes | `test/stubs/hardware/structs/powman.h`, `test/stubs/hardware/watchdog.h`, `test/fakes.cpp` |

## Inputs

Three values, read once, from the Pico SDK 2.1.1:

- `powman_hw->chip_reset`, the power manager's CHIP_RESET register at offset
  0x2c (`hardware/regs/powman.h`, `hardware/structs/powman.h`). Its HAD_*
  bits, 16 to 28, say which reset the power manager last saw. The header
  marks them read-only.
- `watchdog_caused_reboot()`: true when the watchdog's REASON register is
  non-zero and the boot ROM (read-only memory) reports a normal boot
  (`rom_get_last_boot_type() == BOOT_TYPE_NORMAL`).
- `watchdog_enable_caused_reboot()`: true when REASON is non-zero and
  watchdog scratch register 4 holds the value `watchdog_enable()` writes,
  0x6ab73121.

`diagDecodeRestart()` takes the three as arguments and reads no register,
so the host suite runs it on every flag. In the firmware build, a
`static_assert` per bit holds `DIAG_CR_*` in `diag.h` to the SDK's
`POWMAN_CHIP_RESET_*_BITS`.

## Decoding

The first match decides.

| Order | Input | Text | Colour |
|---:|---|---|---|
| 1 | `watchdog_enable_caused_reboot()` | `WATCHDOG` | amber |
| 2 | `watchdog_caused_reboot()` | `REBOOT` | grid |
| 3 | HAD_POR, bit 16 | `POWER-ON` | grid |
| 4 | HAD_BOR, bit 17 | `BROWN-OUT` | amber |
| 5 | HAD_RUN_LOW, bit 18 | `RUN PIN` | grid |
| 6 | HAD_DP_RESET_REQ, bit 19; HAD_RESCUE, bit 21; HAD_HZD_SYS_RESET_REQ, bit 27 | `DEBUGGER` | grid |
| 7 | HAD_GLITCH_DETECT, bit 26 | `GLITCH` | amber |
| 8 | HAD_WATCHDOG_RESET_POWMAN_ASYNC, bit 22; `_POWMAN`, bit 23; `_SWCORE`, bit 24; `_RSM`, bit 28 | `WATCHDOG` | amber |
| 9 | HAD_SWCORE_PD, bit 25 | `POWER-DOWN` | grid |
| 10 | none | `UNKNOWN` | grid |

The watchdog functions come first. `watchdog_enable()` in the SDK selects the
reset through the power-on state machine (PSM) only (`psm_hw->wdsel`), not
through the power manager's own WDSEL register. A reset of that kind need
not change CHIP_RESET, which then still reads the reset before it. Whether
it does is not known: the SDK headers do not say, and it is not measured.

The order among the CHIP_RESET flags only matters when more than one is
set. Whether the chip ever sets more than one is not known.

RESCUE_FLAG (bit 4) and DOUBLE_TAP (bit 0) are not reasons and are ignored.

## Watchdog

The firmware arms no watchdog: nothing calls `watchdog_enable()`.
`WATCHDOG` from order 1 therefore cannot appear with this firmware. The
flags of order 8 follow the power manager's WDSEL register (RESET_RSM,
RESET_SWCORE, RESET_POWMAN, RESET_POWMAN_ASYNC), which this firmware does
not set either. The watchdog reason can only appear once a watchdog is
armed. `platReboot()` in `src/plat.h` calls `watchdog_reboot()` and has no
caller, so `REBOOT` shows only after a reboot through the watchdog that
another program requested; which programs do so is not known. Which reason
the board shows after a UF2 (USB Flashing Format) copy is not known.

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
- a UF2 copy in BOOTSEL mode: the result is not known.

## Open questions for a fault log

A fault log would keep records across restarts. Before one is built, the
owner decides:

1. **What counts as a fault.** Candidates: a restart other than POWER-ON;
   the core1 heartbeat trip (`UI_HEARTBEAT_TIMEOUT_MS`, 250 ms); a failed
   settings save; `DSERR`; a card error or a failed flush of the SD write
   cache; a lost servo or a write result in red on KST SERVO or HITEC SERVO;
   a failed AM32 write. Which of these, and whether a warning counts too.
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
   over the USB (Universal Serial Bus) serial port.
7. **How it is cleared.** A hold on the page, as HOLD TO SAVE works, or
   RESET on SETTINGS, or never; whether a clear is itself recorded.
