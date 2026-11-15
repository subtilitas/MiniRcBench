# USB bridge to the ESC's signal wire

Status: designed; the owner answered its questions on 2026-11-15, see
[decisions](#decisions). Slices 1 and 2 are built, host tests only
([slice 1 as built](#slice-1-as-built), [slice 2 as
built](#slice-2-as-built)): the pin user, the core1 job, the PIO line, the
frame slot and the ring; passthrough, the console switch, the traffic log
as a build option and the screen USB BRIDGE, opened from ESC CONFIG.
Slices 3 and 4 wait on being built. The
feature is item (5) of the owner's
feature choice of 2026-11-14 in [STATUS.md](../../STATUS.md) open item 28,
"a USB one-wire bridge for desktop ESC tools"; open item 29 tracks this
note. It ports DshotDisplay's `feat/usb-bridge` at 6693084. A design note
comes first, then slices with reviews. No byte of this design has gone to
an ESC from this board. The facts below are read from the sources under
[sources](#sources), with the file and line; the ones no readable source
settles are marked "not verified" and listed again under
[not verified](#not-verified).

## Terms

ESC is electronic speed controller, PC the desktop or laptop computer the
pilot runs a tool on, USB Universal Serial Bus, CDC the USB communications
device class (the serial port), ACM its abstract control model, MSC the USB
mass storage class (the card reader), DTR the data terminal ready signal a
host sets when it opens a port, PID product identifier, VID vendor
identifier. PIO is the RP2350's programmable input/output block, SM one PIO
state machine, DMA direct memory access, FIFO first in, first out, UART
universal asynchronous receiver-transmitter, CRC cyclic redundancy check,
ACK acknowledgement, API application programming interface, CLI command
line interface, XOR exclusive or, EEPROM the ESC's settings store (named
after electrically erasable programmable read-only memory), RAM
random-access memory, UI user interface, eRPM electrical revolutions per
minute, COM port a serial port as Windows names it, GPIO general-purpose
input/output, TX and RX transmit and receive, SDK software development
kit, ID identifier.
8N1 is eight data bits, no parity, one stop bit. DShot is the digital ESC
protocol the tester sends on SIG. SIG and AUX are the claim's two pins
([pin-claim.md](pin-claim.md#the-two-pins)).

A one-wire link is a half-duplex serial line on the ESC's signal wire: one
end sends, then lets go of the line, then the other end answers on the same
wire. An ESC's bootloader is the small program that starts before its
firmware and reads and writes the ESC's flash over that link at 19200 baud
8N1. MSP is the MultiWii Serial Protocol a flight controller speaks over
its USB port. The 4-way interface ("4way-if") is the protocol in which a
flight controller, once told by MSP to pass through, speaks to the ESCs'
bootloaders on the PC's behalf; its frames start with 0x2F. Passthrough
here is the other kind: the bridge copies bytes between the USB port and
the wire, and the PC speaks the bootloader protocol itself.

BLHeliSuite, BLHeli_S, BLHeli_32, Bluejay, AM32, ESC-Configurator,
Betaflight and DshotDisplay are names of their owners' firmware, tools or
products, used here only to say which ESCs, tools and references a
statement is about.

## Sources

Read for this note; commit and file give the place, line numbers are at
that commit.

| Source | Commit | Files | Licence, access |
|---|---|---|---|
| DshotDisplay (`subtilitas/dshotdisplay`), branch `feat/usb-bridge` | 6693084 | `src/bridge_wire.h` (140 lines), `src/bridge_wire.cpp` (176), `src/usb_bridge.h` (83), `src/usb_bridge.cpp` (126), `src/bridge_log.h` (61), `src/bridge_log.cpp` (191), `src/am32_bl.cpp` lines 408 to 440, `src/am32_bl.h` lines 179 to 243, `src/config.h` lines 195 to 299, `src/ui_am32.cpp` lines 60 to 120, 172 to 270, 389 to 395, 507 to 518, 710 to 765, `DshotDisplay.ino` lines 110 to 149, `README.md` lines 291 to 359, `test/test_bridge.cpp` (421), `test/test_dump.cpp` (61), `test/Makefile` | MIT, same holder; built and run here: all 129 checks pass, 28 of them in the eight bridge sections |
| Betaflight (`betaflight/betaflight`) | 134ce0f | `src/main/io/serial_4way.c` lines 71 to 91, 137 to 182, 191 to 295, 344 to 394, 445 to 497, 597 to 625; `src/main/io/serial_4way_avrootloader.c` lines 49 to 74, 76 to 194, 196 to 229, 231 to 345; `src/main/io/serial_4way.c` lines 641 to 661; `src/main/target/common_pre.h` lines 103, 104, 258, 310, 324, 326; `src/main/msp/msp.c` lines 881 to 895; `src/main/msp/msp.c` lines 293 to 350; `src/main/msp/msp_protocol.h` lines 59 to 62, 94 to 272; `src/main/drivers/serial_escserial.c` lines 52, 920 to 1003; `src/main/cli/cli.c` lines 4834 to 4867, 8776 | GPL-3.0; read as the flight-controller side's reference, nothing copied |
| ESC-Configurator (`stylesuxx/esc-configurator`), v0.32.1 | 21b407a | `src/utils/Serial.js` lines 27, 140 to 144; `src/Containers/App/serialSlice.js` line 4; `src/Containers/App/index.jsx` lines 172, 206 to 312, 439 to 470; `src/utils/Msp.js` lines 13 to 35, 130 to 138, 404 to 557, 789; `src/utils/FourWay.js` lines 125 to 135, 1395 to 1500; `src/utils/FourWayConstants.js` lines 12 to 26; `src/utils/helpers/Flash.js` lines 154 to 160 | AGPL-3.0; read, nothing copied |
| AM32 configurator (`am32-firmware/am32-configurator`) | e11c42e | `components/SerialDevice.vue` lines 415 to 417, 505 to 540, 612 to 660, 674 to 702, 813 to 817, 1076 to 1083; `src/communication/direct.ts` lines 6 to 19, 103 to 120, 236 to 266; `src/communication/msp.ts` lines 118 to 126; `src/communication/four_way.ts` lines 87, 623 to 625; `src/communication/commands.queue.ts` lines 48 to 118 | no licence file at that commit; read, nothing copied |
| AM32 bootloader (`am32-firmware/AM32-bootloader`) | 578ff29 | `bootloader/main.c` lines 45, 52, 396 to 398, 423 to 473, 546 to 565, 576 to 579, 815 to 816, 835 to 860, 1097 to 1157, 1217 to 1281 | GPL-3.0; read, nothing copied |
| AM32 firmware 2.22 | 2a695a8 | `Src/main.c` lines 2137 to 2163 | GPL-3.0; read, as in [esc-commands.md](esc-commands.md#sources) |
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 | a5bcf99 | `BLHeli_S SiLabs/BLHeliBootLoad.inc` lines 5 to 11, 48 to 91, 176 to 180, 205 to 215; `BLHeli_S SiLabs/BLHeli_S.asm` lines 3585 to 3605 | GPL-3.0; read, nothing copied |
| Bluejay (`bird-sanctuary/bluejay`), revision 0.21 | 0368d11 | `README.md` lines 37 to 40; `src/BLHeliBootLoad.inc` line 238 | GPL-3.0; read, nothing copied |
| rcbench (`subtilitas/rcbench`) | b88593d3 | `protocols/escwire/README.md` lines 1 to 60, 232 to 256; `protocols/escwire/rp2350/ew_line.h` lines 1 to 60 | MIT, same holder |
| Pico SDK 2.1.1, with its TinyUSB | | `src/rp2350/hardware_regs/include/hardware/regs/io_bank0.h` lines 141, 277, 423, 561; `lib/tinyusb/src/class/cdc/cdc_device.c` lines 132 to 135; `lib/tinyusb/src/device/usbd.h` lines 97 to 99 | BSD-3-Clause, MIT |
| This repository | `main` at 8c7ace3 | `src/usb_dev.{h,cpp}`, `src/usb_descriptors.c`, `src/tusb_config.h`, `src/usb_msc.{h,cpp}`, `src/ui_sdlog.cpp` lines 391 to 406, `src/ui_am32.h` lines 175 to 197, `src/am32_eeprom.h` line 43, `src/ui_kst.cpp` lines 610 to 628, `src/main.cpp`, `CMakeLists.txt` lines 315 to 329, `src/config.h` lines 577 to 640, `src/am32_bl.{h,cpp}`, `src/pin_claim.h`, `src/ui_leave.h` lines 19 to 28, `src/core1_job.h`, `src/pio_uart_rx.h`, `src/ui_am32.cpp`, `src/ui_registry.cpp`, `test/stubs/tusb.h`, [pin-claim.md](pin-claim.md), [menu-b.md](menu-b.md) | |

BLHeliSuite and BLHeliSuite32 are closed source. Every statement below
about them is marked not verified.

## What it is for

- **Flashing an ESC's firmware.** The bench reads and writes AM32 settings
  itself (AM32 CONFIG) but flashes no firmware, and does not plan to: a
  desktop tool already does it, with its own image catalogue and checks.
  DshotDisplay's bridge says the same (`usb_bridge.h` lines 5 to 9).
- **Settings the bench has no screen for.** BLHeli_S and Bluejay settings
  are item (6) of the same feature choice; until it is built, and for every
  setting it does not cover, a desktop tool does the work through the
  board.
- **No separate adapter.** The ESC stays on the bench's SIG wire; the PC
  reaches it through the board's USB port, the one that already carries the
  serial console and the card reader.

While the bridge runs, no throttle from the bench reaches the ESC: DShot is
stopped. The PC is not held to bootloader commands: in passthrough every
byte it sends reaches the wire, a bootloader RUN frame included, and in the
4-way interface `cmd_DeviceReset` sends one; once the ESC's firmware runs,
it reads whatever the PC sends next ([safety](#safety)).

## What the desktop tools speak

| Tool | Port | Protocol on the port | How the tool chooses | Echo of its own bytes expected | Source |
|---|---|---|---|---|---|
| AM32 configurator, "Direct" ticked | Web Serial at 19200 | the AM32 bootloader frames, raw | the VID is in its direct list (0x1A86, 0x0403, 0x4348, 0x26BA, 0x10C4), or the user ticks Direct | yes: it strips exactly the length it sent from the front of each reply | `SerialDevice.vue` 416, 517 to 536; `direct.ts` 103 to 120, 248 to 265 |
| AM32 configurator, default | Web Serial, 115200 by default | MSP, then the 4-way interface | the VID is in its flight-controller list, which holds 0x2E8A, this board's VID | no | `SerialDevice.vue` 415, 505, 612 to 660 |
| ESC-Configurator (Bluejay, BLHeli_S, AM32) | Web Serial, the rate the user's choice, 115200 by default (`serialSlice.js` 4) | MSP, then the 4-way interface | always | no | `Serial.js` 27, 140; `index.jsx` 206 to 300, 439 to 470 |
| The Python AM32 configurator DshotDisplay's bridge was written against | 19200 8N1 | the AM32 bootloader frames, raw | not named in the branch | yes, and paced: its ACK check needs more than one byte in a 25 ms read | `usb_bridge.h` 11 to 27; `am32_bl.h` 199 to 231 |
| Betaflight's own passthrough, `escprog bl <n>` | the flight controller's port | raw bytes at 19200 to the ESC | the CLI command | yes: the flight controller echoes each byte back ("blheli loopback") | `serial_escserial.c` 52, 984 to 989; `cli.c` 4836, 4867, 8776 |
| BLHeliSuite, BLHeliSuite32 | a COM port | the 4-way interface through a flight controller's MSP, or raw one-wire through a USB-serial adapter, by the interface the user picks | the interface menu | not verified | not verified: closed source |
| Bluejay configurator (standalone) | not verified | not verified | | | Bluejay `README.md` 37 to 40 names it beside ESC-Configurator |

So a bridge that serves the owner's list needs both:

- **Passthrough**, DshotDisplay's bridge: bytes in both directions at 19200
  8N1, with the echo a shared wire gives a USB-serial adapter. It serves the
  AM32 configurator's Direct mode and the Python tool; DshotDisplay's code
  does this and nothing else.
- **MSP and the 4-way interface**: the board answers the few MSP requests
  the tools make before they ask for passthrough, then runs the 4-way
  interface itself, sending bootloader frames on SIG. It serves
  ESC-Configurator, which has no direct mode, and the AM32 configurator's
  default. None of it is in DshotDisplay.

ESC-Configurator's MSP sequence before any ESC access: `MSP_API_VERSION`
(1), `MSP_FC_VARIANT` (2), `MSP_FC_VERSION` (3), `MSP_BUILD_INFO` (5),
`MSP_BOARD_INFO` (4), `MSP_UID` (160), `MSP_MOTOR` (104), counting the
motors whose value is above 0, `MSP_FEATURE_CONFIG` (36)
(`index.jsx` 236 to 294); before a read, and only while `MSP_MOTOR` showed
no motor above 0 (`index.jsx` 310, 445 to 446), so not with the board's
answer below, `MSP_STATUS` (101), refused with
"radio on" unless bit 2 of the arming-disable flags, RX_FAILSAFE, is set
(`index.jsx` 446 to 451, `Msp.js` 404 to 425); then `MSP_SET_PASSTHROUGH`
(245), whose reply byte is the ESC count, and 1200 ms later the 4-way
frames (`index.jsx` 457 to 466). It sends MSP version 1 frames for
commands up to 254 (`Msp.js` 130 to 138). While connected it sends
`cmd_InterfaceTestAlive` when 900 ms have passed without a command, checked
every 800 ms (`FourWay.js` 125 to 135). Its motor tab sends `MSP_SET_MOTOR`
(214) (`Msp.js` 789). It reads only the four bytes of
`cmd_DeviceInitFlash`'s reply as signature, input and interface mode
(`Flash.js` 154 to 160), and asks neither the interface's name nor its
version.

The AM32 configurator's MSP sequence: `MSP_API_VERSION`, `MSP_FC_VARIANT`,
`MSP_BATTERY_STATE` (130), then `MSP_MOTOR_CONFIG` (131), or `MSP_MOTOR`
for the variants it calls `inav` and `emuf`, then `MSP_SET_PASSTHROUGH`
and a pause of 2000 ms before its first 4-way frame (`SerialDevice.vue`
618 to 657; `msp.ts` 118 to 126). It keeps the variant only if it reads
exactly `BTFL`, `KISS`, `INAV`, `ARDU` or `EMUF`, and logs any other as
the error `Unknown fc type` (`commands.queue.ts` 64 to 83). It defines
`cmd_InterfaceTestAlive` and a `testAlive()` that nothing calls
(`four_way.ts` 87, 623 to 625): it sends no keep-alive.

Both tools send 4-way frames with no MSP before them when MSP does not
answer, to recover a flight controller left in the 4-way interface: the
AM32 configurator sends `cmd_InterfaceExit` when `MSP_API_VERSION` fails
(`SerialDevice.vue` 618 to 621), ESC-Configurator `cmd_DeviceInitFlash`
and `cmd_DeviceReset` for each ESC, then `cmd_InterfaceExit`, when
`MSP_API_VERSION` times out (`index.jsx` 232 to 260).

The 4-way interface, as Betaflight implements it (protocol version 108,
`serial_4way.c` 71 to 77): a PC frame is 0x2F, command, address high,
address low, parameter length (0 meaning 256), the parameters and a
CRC-16/XMODEM, high byte first; the answer is 0x2E, the same header, the
parameters, an ACK byte and the CRC (`serial_4way.c` 191 to 196, 445 to 497,
917 to 932). The commands are 0x30 to 0x40 (`serial_4way.c` 196 to 279);
the ACK codes 0x00, 0x02, 0x03, 0x04, 0x08, 0x09, 0x0F (284 to 295).
`cmd_DeviceInitFlash` (`serial_4way.c` 641 to 661) sends the bootloader's
init string, 21 bytes in classic builds, not cloud builds or
software-in-the-loop (SITL) builds, for targets with 1024 KB of flash or
more, which define `USE_SERIAL_4WAY_SK_BOOTLOADER` (`common_pre.h` lines
103, 104, 258, 310, 324, 326), 17 bytes otherwise
(`serial_4way_avrootloader.c` 206 to 212), reads the 8-byte answer
starting `471` and decides the interface mode from the signature: SiLabs
(BLHeli_S, Bluejay) for 0xE801 to 0xF8FF, ARM (AM32, BLHeli_32) for a
high byte 0x01 to 0x8F with the low byte 0x06 (`serial_4way.c` 344 to
394; `serial_4way_avrootloader.c` 196 to 229).
Erase and the settings write wait up to 3000 ms for the bootloader's ACK, a
flash write 500 ms (`serial_4way_avrootloader.c` 315 to 333). A write sends
`SET_BUFFER`'s 4 bytes and CRC, waits up to two 2 ms start-bit timeouts,
about 4 ms, for no answer, then sends the data and its CRC as a second
message (`BL_SendCMDSetBuffer()`, lines 267 to 278).

## The board's USB today

As built on `main` at 8c7ace3:

- **One composite device.** CDC ACM on interfaces 0 and 1, MSC on
  interface 2, behind an interface association descriptor
  (`usb_descriptors.c` lines 65 to 102; `tusb_config.h` lines 51, 52). The
  RP2350 has one full-speed device controller (`tusb_config.h` lines 18 to
  27). VID 0x2E8A, PID 0x4006 derived from the interfaces present
  (`usb_descriptors.c` lines 31, 34). Endpoints 0x81, 0x02, 0x82 for CDC
  and 0x03, 0x83 for MSC (lines 84 to 88).
- **The descriptors are the firmware's own.** `pico_enable_stdio_usb()` is
  not used: it owns the descriptors and cannot share the device with the
  card reader (`CMakeLists.txt` lines 319 to 329; `usb_dev.h` lines 7 to
  17). `usbDevInit()` enables a stdio driver on the CDC interface
  (`usb_dev.cpp` line 104); `usbDevTask()` is `tud_task()` (line 107),
  called first in every pass of core0's loop (`main.cpp` line 181).
- **The serial port only talks.** `printf()` goes out on CDC and drops
  rather than blocks when the host does not read (`usb_dev.cpp` lines 44 to
  61); stdin reports no data (lines 63 to 67). `SERIAL_TELEMETRY` is 1
  (`config.h` line 578): core0 prints one telemetry line every 100 ms
  (`main.cpp` lines 212 to 229). Nothing reads what a host sends; TinyUSB
  holds 64 bytes of it and then refuses more (`tusb_config.h` line 62).
- **The card reader** is always enumerated and reports no medium until SD
  LOG's USB DRIVE hands the card over (`usb_msc.h` lines 30 to 37).

The bridge therefore adds no USB interface: the CDC port exists, and the
work is reading it.

## The design

### Two modes on one port

The bridge has two modes, chosen from the first byte the PC sends while
the bridge waits (decision 3):

| First byte | Mode | Screen |
|---|---|---|
| 0x24, `$`, an MSP frame | MSP, then the 4-way interface after `MSP_SET_PASSTHROUGH` | `4-WAY INTERFACE` |
| 0x2F, a 4-way frame | the 4-way interface at once: both tools send 4-way frames with no MSP before them to recover (above) | `4-WAY INTERFACE` |
| any other | passthrough | `PASSTHROUGH` |

How each ends:

| State | Ends on | Then |
|---|---|---|
| Waiting for the first byte | nothing; it is the state the others return to | |
| Passthrough | 2000 ms with no byte in either direction, the port closed (DTR cleared after it was set), BACK | waiting |
| MSP, before `MSP_SET_PASSTHROUGH` | 2000 ms with no byte, the port closed, BACK | waiting |
| 4-way | `cmd_InterfaceExit`, the port closed, BACK; not on a pause | waiting; the connected device and its interface mode are kept across pauses and dropped here |

The 4-way state outlasts pauses because the AM32 configurator waits exactly
2000 ms after `MSP_SET_PASSTHROUGH` and sends no keep-alive
(`SerialDevice.vue` 657, `four_way.ts` 623 to 625), and a user may leave a
connected tool idle for minutes. In the 4-way state an MSP frame is
answered as in the MSP state, where Betaflight ignores every byte but 0x2F
(`serial_4way.c` 472 to 477); the tools' recovery works either way.

An AM32 configurator in Direct mode starts with twelve 0x00 bytes
(`direct.ts` lines 104 to 106); the Python tool the same 21-byte string
(`am32_bl.cpp` lines 302 to 307 in this repository); neither starts with
0x24 or 0x2F. A raw bootloader session can contain 0x24 and 0x2F later, in
data; the mode holds until it ends as above, so later bytes do not switch
it. A passthrough tool that starts while the bridge is still in the 4-way
state, without the port closed in between, gets no answer until BACK or a
port close; the screen shows `4-WAY INTERFACE` meanwhile. The converse: an
MSP tool that starts while the bridge is still in passthrough, without a
port close in between, has its frames forwarded to the ESC until 2000 ms
pass with no byte. Which mode a BLHeliSuite interface ends in is not
verified.

### USB side

- **The same CDC port.** No second CDC interface: the user picks one port
  in the tool, and it is the board's (decision 4).
- **The console steps aside.** On entry the bridge disables the stdio
  driver (`stdio_set_driver_enabled(&s_cdcDriver, false)`, the call
  `usb_dev.cpp` line 104 makes with true), so the telemetry line of
  `main.cpp` and every other `printf()` go nowhere; on leaving it enables
  the driver again. DshotDisplay did this with a check in `loop()`
  (`DshotDisplay.ino` line 116), which misses every other `printf()`.
- **Bytes from the PC** are read with `tud_cdc_available()` and
  `tud_cdc_read()` in the bridge's step on core0, run from `loop()` after
  `usbDevTask()`, every pass, not once per UI frame.
- **Bytes to the PC** go out with `tud_cdc_write()` and
  `tud_cdc_write_flush()`, at most `tud_cdc_write_available()` per pass;
  what does not fit waits in the to-PC ring.
- **Line coding.** The rate the PC sets is ignored: the wire runs at 19200
  baud, and MSP over USB has no rate. TinyUSB reports the rate through
  `tud_cdc_get_line_coding()`; only option (b) of decision 3, not
  taken, would read it.
- **PORT OPEN** on the screen is `tud_cdc_connected()`, which is DTR.
  Whether each tool sets DTR when it opens the port is not verified; the
  bridge forwards bytes whether DTR is set or not. A port close is DTR
  falling from set to clear; a tool that never sets DTR keeps its mode
  until the idle time, `cmd_InterfaceExit` or BACK, and the chip reads
  `PORT CLOSED` throughout.
- **The card reader** keeps its state; the bridge does not touch the card
  and `usbDevTask()` serves both interfaces.
- **Outside the bridge** the port stays as today: what a PC sends is not
  read. A tool that sends MSP while the bench runs DShot gets no answer,
  and DShot keeps running: only a tap on the board opens the bridge.

### The pin

The bridge is a new claim user, beside AM32:

```c
// pin_claim.h
enum class PinUser : uint8_t { None, Am32, Servo, Kst, Hitec, Sbus, Bridge, Count };
// PIN_USERS: who, job, unplug, escCheck, deferJob
{ PinUser::Bridge, Core1Job::Bridge, false, false, false },
```

- **escCheck false**, as AM32: the bridge needs the ESC on the pin and
  answering until the claim stops DShot (the EscAnswering rule of
  `pinClaimCheck()`).
- **unplug false**, as AM32: BACK (`pinClaimEnd()`) asks the job to let the
  pin go, and DShot resumes through Leaving and Restoring once the job is
  idle, on the next call into the claim after that (`leaveStep()` and
  `settle()` in `pin_claim.cpp`). The exception, decision 10's leave page
  after an unfinished write, needs a leave page per claim rather than per
  user, which `PIN_USERS`' fixed `PinUserInfo::unplug` flag cannot say;
  `pinClaimWantLeavePage()` gives it.
- **A core1 job**, `Core1Job::Bridge`, not core0 as AM32's transport:
  the file description of `pin_claim.h` makes core1 the owner of every PIO
  change on the signal pins, with AM32 the one exception.

The flow, all of it existing claim machinery but for decision 10's leave
page:

1. The entry tap (see [entry](#entry)) disarms through `uiOpenTool()` and
   calls `pinClaimBegin(PinUser::Bridge, PIN_SIG)`; a refusal (armed,
   busy) leaves DShot running and shows the claim's refusal.
2. Stopping: core1 drains 250 ms of zero throttle, stops and releases the
   DShot pump. The strip reads `RELEASING SIG GP0`.
3. Held, then Running: core1 binds the bridge's line on the latched SIG
   GPIO. The bridge opens; the stdio driver goes off.
4. BACK: the job releases the line; DShot is rebuilt on the latched wiring;
   the stdio driver comes back; after an unfinished write the leave page
   comes before DShot is rebuilt (decision 10).

### The wire

| Item | Value | Source |
|---|---|---|
| Rate | 19200 baud 8N1, 52.08 µs a bit, 520.8 µs a byte | AM32 bootloader `main.c` 396 to 398; BLHeli_S `BLHeliBootLoad.inc` line 11; `serial_4way_avrootloader.c` 71 |
| Gap within one bootloader message | none: the bytes go back to back; as built, a stop bit of 1.25 bit times, driven high ([slice 1 as built](#slice-1-as-built)) | the AM32 bootloader ends a message after 5 bit times, 260 µs, of idle line (`main.c` 860) |
| Gap between `SET_BUFFER` and its data | at least 520 µs, 10 bit times; the 4-way handler waits 4 ms, as Betaflight | they are two messages: data closer than 260 µs behind the header become part of it and get no answer. Betaflight waits about 4 ms (`serial_4way_avrootloader.c` 267 to 278), rcbench's escwire 520 µs (`escwire/README.md` 53, 59 to 61) |
| Turnaround | the line is let go at the end of the last stop bit | Betaflight releases the line straight after its last byte (`serial_4way_avrootloader.c` 178 to 194); AM32 answers about 0.3 to 0.35 ms after the host's last byte by rcbench's specification (`escwire/README.md`), not measured |
| Idle | input with the pad's pull-up, as the claim leaves it in Held | the Held row of `pin_claim.h`'s state table; `am32_bl.cpp` lines 74 to 78 |
| Drive while sending | push-pull, both levels | `AM32_PUSH_PULL_TX` 1 (`config.h` line 628); rcbench `ew_line.h` "a transaction's bytes: driven, both levels" |
| Receive | sampled always, while sending too | below |

**PIO, not a UART and not bit-banging.** GP0's only UART function is
UART0's transmit and GP1's only UART function UART0's receive, both
function 2; the RP2350's second UART function, 11, puts UART0's transmit
and receive on GP2 and GP3 (`io_bank0.h` lines 141, 277, 423, 561;
`pio_uart_rx.h` lines 6 to 10). Neither pin carries a half-duplex line,
and SETTINGS can move SIG between them. The
AM32 transport bit-bangs on core0 with interrupts masked per byte
(`am32_bl.cpp` lines 5 to 13, 110 to 125); DshotDisplay moved that bit-bang
to core1 because core0's repaints made it miss replies
(`bridge_wire.h` lines 5 to 21). A PIO line needs neither: the state
machines keep the bit timing, and their FIFOs absorb a busy core1 pass.

The line is one transmit SM and one receive SM on the same pin, in one
block. The receive SM is `pio_uart_rx`'s program, 9 words, as it is; the
transmit SM is an 8N1 transmit program with the pin's direction switched by
side-set, 7 words as built (`src/bridge_line.pio`). During a claim DShot
and KISS have left pio2, and the claim's own line is the only program
there: KST's takes 24 of its 32 words, Hitec's 30
([pin-claim.md](pin-claim.md#pio-and-dma-budget)). The bridge's line, 16
words, fits the same way. Item (6) copied rcbench's `ew_line` (one SM, 25
words, two DMA channels, `ew_line.h` lines 1 to 20); slice 1 compared the
two and uses its own line ([slice 1 as built](#slice-1-as-built)).

**The echo is the wire's own.** The receive SM samples the pin while the
transmit SM drives it, so every byte sent comes back as received, at wire
pace, as on a USB-serial adapter with TX and RX joined. That is what the
passthrough tools expect: the AM32 configurator strips exactly the bytes it
sent from the front of each reply (`direct.ts` lines 248 to 265), and the
Python tool needs the echo's tail and the ACK in one 25 ms read
(`am32_bl.h` at 6693084 lines 199 to 231; `usb_bridge.h` lines 11 to 27).
DshotDisplay made the echo in software, one byte per byte sent, for the
same pacing (`bridge_wire.cpp` lines 159 to 165). A received echo that
differs from the byte sent is counted as a collision: the ESC drove the
line while the bench did. That the RP2350's input reads the pad while the
output drives it is from the pad's design (input enable on); on this
board it is not measured.

- **Passthrough:** everything received goes to the PC, echo included.
- **4-way:** bytes received while the transmit SM is busy are dropped; the
  4-way handler reads the bootloader's answer only.

### Cores and buffers

As DshotDisplay split it (`bridge_wire.h` lines 22 to 49), with the
blocking calls removed:

| Part | Core | Size | What it does |
|---|---|---|---|
| Frame slot to the wire | core0 fills, core1 empties | 320 bytes, `WIRE_FRAME_MAX` (`bridge_wire.h` 60) | one whole PC frame, so it goes out back to back |
| Ring from the wire | core1 fills, core0 empties | 1024 bytes, `WIRE_RX_RING` (`bridge_wire.h` 69): 533 ms of wire traffic | absorbs core0's repaints |
| Bridge job | core1 | | moves the slot into the transmit FIFO and the receive FIFO into the ring, every pass of `loop1()` |
| Bridge step | core0 | | CDC to the slot, the ring to CDC; the MSP and 4-way handlers |

Single producer, single consumer, release and acquire on each index, as
`bridge_wire.cpp` lines 41 to 51 do; no lock. The receive FIFO joined to 8
entries holds 8 bytes: 4.27 ms of the bridge's own frames, at 82 cycles a
byte as built, and 4.17 ms of an ESC sending back to back at 10 bit times
a byte; core1's pass is the claim's job dispatch,
whose length is not measured, and a full FIFO counts as `LOST`.

**Passthrough gather.** A PC frame arrives in 64-byte USB packets, and
TinyUSB's receive buffer holds one of them (`CFG_TUD_CDC_RX_BUFSIZE` 64,
`tusb_config.h` line 62): a 258-byte payload with its CRC comes in over
five packets and as many core0 passes. The bridge collects bytes until 2 ms
pass with none (`BRIDGE_GATHER_MS`, `usb_bridge.cpp` line 26) or the slot
is full, then hands the frame over. Each pass reads what TinyUSB holds
before it looks at the 2 ms timer, so a long core0 pass, a repaint of 30 ms
in the middle of a frame, ends with the next packet read and the timer
started again, not with half a frame sent. The host is held off by USB
flow control meanwhile; nothing is lost. A receive buffer of 512 bytes
(`CFG_TUD_CDC_RX_BUFSIZE` 512, 448 bytes more RAM) would take a whole
payload in one pass; it is an option for slice 2, not needed for
correctness.

The latency budget is the tools': the Python tool reads 25 ms after it
writes and needs the echo's tail and the ACK in one read; the AM32
configurator waits up to 2000 ms for the expected length
(`direct.ts` 259 to 265); the 4-way tools wait for a whole 4-way answer.
A core0 pass adds its length to each answer; the length of core0's longest
pass during the bridge is not measured.
The tools wait for an answer before they send again: the AM32 configurator
waits for the expected length (`direct.ts` lines 259 to 265), the Python
tool 25 ms. DshotDisplay's gather spins on `delay(1)`, and its handover
waits with `delay(1)` until core1 takes the frame, up to 167 ms
(`usb_bridge.cpp` lines 80 to 97); here both are states advanced once per
pass, so core0 keeps drawing and serving USB. `SET_BUFFER` and its data
reach the bridge as two PC frames, since both passthrough tools wait
between them: the AM32 configurator for the header's echo, the Python tool
25 ms. The gather does not join two PC writes more than 2 ms apart, and
the gap between them on the wire is then at least that.

**4-way.** Frames have their length in the header; the handler turns each
into bootloader frames, sends them through the slot, with a write's
`SET_BUFFER` header and its data as two messages 4 ms apart, waits for the
answer in the ring with Betaflight's limits (3000 ms for erase and the settings
write, 500 ms for a flash write, `serial_4way_avrootloader.c` 315 to 333),
and answers the PC.

### Passthrough

DshotDisplay's bridge, kept as it is in behaviour:

- transparent: the bridge never waits for the ESC before it forwards the
  next PC frame. `SET_BUFFER` has no ACK of its own, and a bridge that
  waits for one corrupts the payload exchange that follows
  (`usb_bridge.h` lines 29 to 45);
- the echo at wire pace, taken from the wire (above);
- what was queued in CDC before the bridge opened is discarded
  (`usb_bridge.cpp` lines 40 to 42);
- the counters `toEsc`, `toHost`, `frames`, `lastMs` (`usb_bridge.h` lines
  56 to 61), and `LOST`, the overruns (`bridge_wire.h` lines 116 to 122).

For the screen only, the bridge names the PC frame's first byte as an AM32
and BLHeli bootloader command: 0xFF SET ADDRESS, 0xFE SET BUFFER, 0x03
READ, 0x01 PROGRAM, 0x02 ERASE, 0x00 RUN, 0xFD KEEP ALIVE, the init string
INIT (`am32_bl.h` lines 31 to 38; `serial_4way_avrootloader.c` 49 to 64).
The same decoding marks a write-class frame (PROGRAM, ERASE, a SET BUFFER
payload) for the BACK rule under [safety](#safety). The frame itself is
forwarded unchanged whatever it holds.

### MSP and the 4-way interface

Written from the protocol as the sources above describe it; Betaflight's
GPL code is not copied.

**MSP answers.** MSP version 1 frames only: `$M<`, length, command,
payload, XOR checksum; the answer `$M>`, or `$M!` with no payload for a
command the board does not answer.

Each payload is at least as long as the tools read: both parse replies
with a JavaScript `DataView`, which throws on a read past the end, and
ESC-Configurator then closes the port (`index.jsx` 309 to 312).

| Command | The board answers | Bytes | What the tools read |
|---|---|---|---|
| `MSP_API_VERSION` (1) | protocol 0, API 1.49, Betaflight 134ce0f's (`msp_protocol.h` 59 to 62) | 3 | bytes 0 to 2 (`Msp.js` 455 to 464; `commands.queue.ts` 57 to 62) |
| `MSP_FC_VARIANT` (2) | `MRCB` (decision 5) | 4, exactly | ESC-Configurator bytes 0 to 3; the AM32 configurator decodes the whole payload (`commands.queue.ts` 53, 63 to 83) |
| `MSP_FC_VERSION` (3) | the firmware version, `version.h` | 3 | bytes 0 to 2 (`Msp.js` 477 to 484) |
| `MSP_BUILD_INFO` (5) | the build's date, 11 bytes, and time, 8 bytes, as text | 19 | bytes 0 to 18 (`Msp.js` 486 to 507) |
| `MSP_BOARD_INFO` (4) | a 4-letter board name and version 0 | 6 | bytes 0 to 5 (`Msp.js` 509 to 524) |
| `MSP_STATUS` (101) | byte 15, the count of mode bytes, 0; the arming-disable flags at bytes 17 to 20 with bit 2, RX_FAILSAFE, set: no receiver and no arming, which is true | 22 | bytes 0 to 21 with that count (`Msp.js` 404 to 425) |
| `MSP_MOTOR` (104) | one motor, 1000 | 2 | pairs of bytes (`Msp.js` 428 to 438) |
| `MSP_MOTOR_CONFIG` (131) | min 1000, max 2000, min command 1000, then the motor count, 1, at byte 6 | 7 | byte 6 (`commands.queue.ts` 114 to 117) |
| `MSP_FEATURE_CONFIG` (36) | 0 | 4 | bytes 0 to 3 (`Msp.js` 526 to 538) |
| `MSP_BATTERY_STATE` (130) | no battery: all 11 bytes 0, in Betaflight's layout (`msp.c` 881 to 895) | 11 | the AM32 configurator bytes 0 to 7 (`commands.queue.ts` 89 to 96); ESC-Configurator byte 8 too from API 1.41, and bytes 9 and 10 when present (`Msp.js` 540 to 557) |
| `MSP_UID` (160) | the chip's 8-byte unique ID, as the USB serial number, then 4 bytes of 0 | 12 | three 32-bit words (`Msp.js` 446 to 453); 8 bytes would make ESC-Configurator close the port |
| `MSP_SET_PASSTHROUGH` (245) | 1, one ESC, and the 4-way interface from the next byte; with no payload or with mode 0xFF, as Betaflight (`msp.c` 293 to 330); any other mode, 0xFD and the ESC serial modes 0 to 4 included, answers 0 | 1 | byte 0 |
| `MSP_SET_MOTOR` (214), and every other command | `$M!`; a `MSP_SET_MOTOR` also shows `MOTOR COMMAND FROM THE PC REFUSED.` | 0 | |

**4-way commands.** One ESC, index 0; any other index answers
`ACK_I_INVALID_CHANNEL`.

| Command | The board does |
|---|---|
| 0x30 TestAlive | sends the bootloader's keep-alive, 0xFD 0x00, while a device is connected |
| 0x31 ProtocolGetVersion, 0x32 GetName, 0x33 GetVersion | 108, `MRCB4W`, the firmware version |
| 0x34 Exit | answers, then leaves 4-way: the bridge waits for a first byte again |
| 0x35 DeviceReset | sends the bootloader's run command, 0x00 0x00 and its CRC. Address low byte 1, after which Betaflight also holds the line low for 300 ms (`serial_4way.c` 598 to 627), is not offered: the line is not pulled low, and the answer is the same |
| 0x37 DeviceInitFlash | the 21-byte init string, as Betaflight's classic builds and this repository's AM32 transport send it (`am32_bl.cpp` lines 302 to 307), the 8-byte answer, the mode from the signature as above |
| 0x39 PageErase, 0x3A Read, 0x3B Write, 0x3D ReadEEprom, 0x3E WriteEEprom, 0x40 Verify | the bootloader frames of the interface mode, as `serial_4way_avrootloader.c` 258 to 345 lists them |
| 0x38 EraseAll, 0x3C C2CK_LOW | `ACK_I_INVALID_CMD`: Atmel and C2 interfaces are not offered |
| 0x3F SetMode | SiLabs (1) and ARM (4) accepted; C2 (0), Atmel (2) and SimonK (3) answer `ACK_I_INVALID_PARAM` |

Atmel bootloaders and SimonK are left out: no ESC on the owner's list uses
them.

rcbench's escwire module speaks the same bootloader frames
(`escwire/README.md` lines 40 to 60), but its guard passes only settings
writes and refuses every erase and program elsewhere (`escwire/README.md`
lines 232 to 245). A firmware flash through the 4-way interface needs
those, so the bridge uses escwire's frame and CRC code, if item (6) has
copied it, and not its guard or session.

## Safety

| Rule | Where | What it guards |
|---|---|---|
| No DShot while bridged | the claim: DShot is drained, stopped and released before the line binds | a throttle frame between bootloader bytes |
| Disarmed to enter | `uiOpenTool()` disarms; `pinClaimCheck()` refuses while armed (its Armed rule) | a running motor when DShot stops |
| No MSP motor command | `MSP_SET_MOTOR` answers `$M!`; the 4-way interface has no motor command | ESC-Configurator's motor tab |
| Write-class frame: BACK disabled | the bridge, until the frame's answer or 3000 ms | leaving in the middle of an erase or a page write (decision 7) |
| No RUN on leaving | the bridge sends nothing at BACK | starting a firmware half written (decision 6) |
| A leave page once a bootloader is reached | BACK after an answered INIT or a write-class frame with no later RUN or `cmd_DeviceReset` | DShot resuming unannounced on an ESC waiting in its bootloader, where a garbled frame can pass as a command, or whose flash may be cut; the ESC still starts what it holds at its next power-up with DShot on SIG (decision 10 as revised) |
| MENU's run cue | from an answered INIT until a RUN, RESTART DSHOT or an eRPM frame | an ESC left in its bootloader taken for a dead one |
| No bridge while USB DRIVE holds the card | the entry, by decision 11 | the card served to a host and a desktop tool on one cable at once (decision 11) |
| `PROPELLER OFF.` on the screen | the screen | below |

With DShot off, a touch controller that stops answering only leaves the
bridge open: there is no motor to stop. The touch-loss stop of pull request
#49 is not merged (STATUS.md item 23).

**Bytes on SIG are not a throttle, but not nothing either.** In
passthrough the PC decides every byte. 19200 baud 8N1 holds the line low
for at most 9 bit times, 469 µs, per byte, and high between bytes for as
long as the PC is silent. A firmware that reads PWM (pulse-width
modulation) throttle could take a high stretch of 1 to 2 ms between two
bytes as a pulse. Whether any ESC firmware arms on such traffic is not
verified; AM32 CONFIG's handshake puts the same kind of bytes on SIG today.
The screen says `PROPELLER OFF.`

**An interrupted flash.** What each bootloader does, from source:

| Firmware | The bootloader itself | After a flash cut part-way |
|---|---|---|
| AM32 (bootloader 578ff29) | refuses writes below the application's start (`checkAddressWritable()`, `main.c` 576 to 579) | it starts the firmware only if the settings page's first byte is 0x01 and the firmware's first two words, stack pointer and entry, are in range (`jump()`, `main.c` 423 to 466, with `CHECK_EEPROM_BEFORE_JUMP` 1, line 52); else it stays in the bootloader and can be flashed again. A firmware whose first words were written and whose rest was not passes that check; what it then does is not verified |
| BLHeli_S 16.7 | refuses to erase or program its own segment, answering 0xC5 (`BLHeliBootLoad.inc` 205 to 215) | the bootloader survives. The firmware decides when the bootloader runs: its no-signal entry jumps to the bootloader after the line has been high for about 15 ms (`BLHeli_S.asm` 3585 to 3605). Whether a half-written firmware still reaches the bootloader at power-up is not verified |
| Bluejay 0.21 | the same check (`src/BLHeliBootLoad.inc` 238) | as BLHeli_S; not verified |
| BLHeli_32 | not verified | not verified |

**The USB cable pulled during a flash.** TinyUSB's RP2040 port, which
the RP2350 uses, reports a pulled cable as a bus suspend, not an unplug
(`usb_msc.cpp` lines 173 to 184); `tud_cdc_connected()` then reads false,
since TinyUSB counts a suspended device as not ready (`cdc_device.c`
lines 132 to 135, `usbd.h` lines 97 to 99). The screen shows
`THE PC CLOSED THE PORT.` On its own cell the board keeps the claim,
holds SIG at the pad's pull-up and sends nothing; the ESC's bootloader
waits. On USB power alone the board stops with the cable, and plugging the
cable in again restarts it with DShot on SIG, into the cut ESC: the ESC is
to be powered off before the cable goes back in. What an unpowered pad
does to SIG is not verified; the AM32 bootloader starts the firmware,
a half-written one included, after about 20 ms of low line
(`main.c` 841 to 852). The write line says `DO NOT UNPLUG ESC OR USB.`

**Into the bootloader without a power cycle, on AM32.** AM32 2.22 resets
itself after a time without a signal, by its comments 2 s while disarmed
and 0.5 s while armed (`Src/main.c` 2137 to 2163). After a software reset
the AM32 bootloader stays in the bootloader while the line reads high under
its own pull-up and with no pull (`main.c` 1125 to 1157). So once the claim
has stopped DShot and the pad's pull-up holds SIG high, an AM32 2.22 should
sit in its bootloader within about 2 s, with no power cycle. AM32 CONFIG
asks for a power cycle today (`ui_am32.cpp` lines 493 to 496). Neither
path is measured. After a power-on reset, not a software one, the
bootloader first samples the line under its own pull-down and starts the
firmware if it reads low (`main.c` 1097 to 1123); whether the bench's
pull-up, 50 to 80 kΩ by rcbench's `ew_line.h`, holds the line high against
that pull-down is not measured.

**After BACK.** DShot starts again on SIG. An AM32 still in its bootloader reads
DShot frames as bad 19200 baud frames; each bad command or CRC adds one to a
count (`main.c` 546 to 565, 815 to 816), and above 100 the bootloader calls
`jump()` (`main.c` 1276 to 1281). In builds without DroneCAN, `jump()` checks
only the settings page's first byte and the firmware's first two words (`main.c`
423 to 466); controller area network (CAN) builds also check the application's
signature (`DroneCAN_boot_ok()`, `main.c` 466 to 473). So about 100 ms after
DShot resumes AM32 starts the firmware in flash, a half-written one included
when a flash was cut after its first page. This is inferred, not measured. A
power-up does the same: the AM32 bootloader runs `checkForSignal()` at every
reset (`main.c` line 1263) and calls `jump()` once it has seen SIG low (`main.c`
1097 to 1157, "floating & low at least once", line 1156), and DShot drives SIG
low. No page on the bench keeps a cut flash from starting at the next power-up
with DShot on SIG; only a complete flash does. After a write-class frame with no
later RUN or `cmd_DeviceReset`, BACK therefore shows a leave page before DShot
resumes, with the note `FLASH THE ESC AGAIN BEFORE USE.`, by decision 10.
`cmd_InterfaceExit` does not count as a finished write: the AM32 configurator's
disconnect sends only Exit (`SerialDevice.vue` 813 to 817), and its flash sends
`cmd_DeviceReset` only after `writeHex()` returns (1076 to 1083), so a failed
flash ends in Exit alone. The cost: a session that only wrote settings and then
disconnected also gets the page, unless slice 3 counts only writes outside the
settings page, whose address decision 12 needs too. For BLHeli_S and Bluejay
what their bootloader does with DShot frames is not verified. The screen's last
lines say to power-cycle the ESC after a flash and whenever the tool finds no
ESC.

## Screen

Header `USB BRIDGE` at scale 2, 118 px, x 8 to 125, 44 px clear of BACK
at x 170. Other text is scale 1, a string of n characters (6n − 1) px
wide; scale 2 is (12n − 2) px.

```
y   0 | USB BRIDGE                      [  BACK  ]|  header 36 px
   40 | SIG GP0  19200 8N1          [PORT OPEN]   |  strip; chip to x 232
   62 | MODE                                      |  C_DIM
   72 | PASSTHROUGH                               |  scale 2, C_TEXT
   98 | PC > ESC              ESC > PC            |  C_DIM, x 8 and x 122
  108 | 1234 B                5678 B              |  scale 2
  134 | FRAMES 412  LOST 0                        |  LOST in C_AMBER above 0
  148 | LAST SET ADDRESS 7E00                     |  C_DIM
  168 | OPEN THIS PORT IN A DESKTOP ESC TOOL.     |  6 message lines,
  180 | THE ESC RUNS ON ITS OWN BATTERY.          |  12 px pitch,
  192 | PROPELLER OFF. DSHOT IS OFF.              |  y 168 to 228,
  204 |                                           |  the last ending
  216 | POWER-CYCLE THE ESC IF THE TOOL           |  at y 235
  228 | FINDS NO ESC, AND AFTER A FLASH.          |
  320 +-------------------------------------------+
```

| Element | Place | Text | Width |
|---|---|---|---|
| Strip caption | x 8, y 44 | `SIG GP0  19200 8N1`; `SIG GP1  19200 8N1` | 107 px, to x 114 |
| Strip chip | `uiChip()`, right edge x 232 | `PORT OPEN`, `PORT CLOSED` | 53 + 12 = 65 px, 65 + 12 = 77 px: from x 155 at the widest (240 − 8 − 77), 40 px clear of the caption |
| MODE caption | x 8, y 62 | `MODE` | 23 px |
| Mode | x 8, y 72, scale 2 | `WAITING`, `PASSTHROUGH`, `4-WAY INTERFACE` | 82, 130, 178 px; to x 185 at the widest |
| Counter captions | x 8 and x 122, y 98 | `PC > ESC`, `ESC > PC` | 47 px each |
| Counters | x 8 and x 122, y 108, scale 2 | `0 B` to `9999999 B`, clamped there | at most 106 px: to x 113 and x 227 |
| Frames line | x 8, y 134 | widest `FRAMES 9999999  LOST 9999999` | 167 px |
| Last command | x 8, y 148 | widest `LAST MSP SET PASSTHROUGH`; `LAST WRITE EEPROM 7C00`, `LAST SET ADDRESS 7E00` | 143, 131, 125 px |
| Message line i, i = 0 to 5 | x 8, y 168 + 12i | at most 37 characters | at most 221 px, to x 228 |

Message lines, each with its width:

| State | Lines | Colour |
|---|---|---|
| Stopping | the strip reads `RELEASING SIG GP0` (101 px), as KST SERVO's and AM32 CONFIG's do (`ui_kst.cpp` line 616) | `C_DIM` |
| The line did not bind | line 0: `SIG GP0 COULD NOT BE TAKEN (PIO)` (191 px; `(CLOCK)`, the widest, 203 px), and RETRY in the footer, x 6, y 268, 228 × 46, as KST SERVO (`ui_kst.cpp` lines 620 to 625) | `C_AMBER` |
| Waiting and running | `OPEN THIS PORT IN A DESKTOP ESC TOOL.` (221 px); `THE ESC RUNS ON ITS OWN BATTERY.` (191 px); `PROPELLER OFF. DSHOT IS OFF.` (167 px); blank; `POWER-CYCLE THE ESC IF THE TOOL` (185 px); `FINDS NO ESC, AND AFTER A FLASH.` (191 px) | `C_DIM` |
| A write-class frame out | line 0: `WRITING: DO NOT UNPLUG ESC OR USB.` (203 px); BACK drawn disabled | `C_AMBER` |
| Bytes lost | line 1: `BYTES LOST: THE BRIDGE FELL BEHIND.` (209 px) | `C_AMBER` |
| A motor command refused | line 2: `MOTOR COMMAND FROM THE PC REFUSED.` (203 px), for 5 s | `C_AMBER` |
| The port closed after traffic | line 0: `THE PC CLOSED THE PORT.` (137 px) | `C_DIM` |
| The claim refused | the claim's refusal, as AM32 CONFIG shows it | `C_AMBER` |
| USB DRIVE holds the card, by decision 11 (a) | line 0: `EJECT THE USB DRIVE FIRST.` (155 px); the claim is not begun | `C_AMBER` |

The pin in the strip is SIG's, GP0 or GP1. The screen repaints its
counters at most every 250 ms. Layout constants get `static_assert`s for
every width and gap, as the other screens have. Nothing on the screen
but BACK, and RETRY after a failed bind, takes a tap.

The leave page of decision 10 (a) is menu structure B's one leave page
(`src/ui_leave.h`), with two steps: `POWER THE ESC OFF.` (107 px from
x 20) and `RESTART DSHOT.` (83 px), and the note `FLASH THE ESC AGAIN
BEFORE USE.` (31 characters, 185 px) at y 158. It opens in Releasing, as
BACK has already asked the line to be let go: the left button reads
`BRIDGE`, which keeps the claim and returns to the bridge
(`pinClaimKeep()`), and RESTART DSHOT is live 1000 ms after the line is let
go, as for the servo tools (`ui_leave.h` lines 19 to 28).

## Entry

MENU has seven rows: BEEP THE ESC, AM32 CONFIG, SERVO
TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS
(`ui_registry.cpp` lines 22 to 30), at a 42 px pitch in a 258 px viewport,
so MENU scrolls 36 px ([menu-b.md](menu-b.md#menu), decision 33). The
bridge needs a way in, and each way changes something menu-b.md decides.
The owner chose option (c) (decision 1). The options:

| Option | MENU | Taps to the bridge | Changes |
|---|---|---|---|
| (a) a `USB BRIDGE` button on AM32 CONFIG's connect page, DshotDisplay's place (`ui_am32.cpp` at 6693084 lines 212 to 215) | 7 rows | 2 | AM32 CONFIG's page; the bridge then sits inside an AM32 claim, which has no core1 job, and is found under a firmware it also serves BLHeli_S and Bluejay for |
| (b) an eighth row `USB BRIDGE`, 118 px at scale 2 | 8 rows: 8 × 42 − 258 = 78 px of scroll | 2 | decision 33's seven rows |
| (c) AM32 CONFIG's row becomes a chooser, `ESC CONFIG`, as PROGRAMMER is ([menu-b.md](menu-b.md#programmer-the-chooser)), with the one settings screen for AM32, Bluejay and BLHeli_S of `docs/design/bluejay-blheli-s.md`, titled `ESC SETTINGS` (12 characters, 142 px at scale 2) by [decision 1 (option e) of bluejay-blheli-s.md](bluejay-blheli-s.md#decisions), and USB BRIDGE | 7 rows | 3; the settings screen one tap deeper | decision 16's row and its label; the chooser claims nothing, an entry's tap claims SIG |
| (d) a SETTINGS row | 7 rows | 3 | SETTINGS holds settings, not tools |
| (e) SD LOG's row becomes a USB chooser, with SD LOG and USB BRIDGE | 7 rows | 3 | decision 16's row; USB DRIVE stays on SD LOG |

Chosen: (c), the recommendation, together with decision 1 (option e) of
[bluejay-blheli-s.md](bluejay-blheli-s.md#decisions). AM32 CONFIG's row becomes
the chooser `ESC CONFIG`, 10 characters, 118 px at scale 2, as PROGRAMMER is. It
holds `ESC SETTINGS`, the one settings screen for AM32, Bluejay and BLHeli_S,
and `USB BRIDGE`. The settings screen and the bridge are both ESC configuration
over SIG, and a chooser takes a new entry as a table line. The labels are the
owner's.

Design call, the developer's under the owner's MENU decision, 2026-11-15:
the chooser comes with whichever of this note's slice 2 and slice 2 of
bluejay-blheli-s.md merges first, and holds both entries from the start.
The entry not yet built is drawn disabled with `NOT BUILT` under it, as RX
SIGNALS' OUTPUT is. `ESC SETTINGS` serves AM32 only until the Bluejay
slices land, and STATUS.md says so. Slice 2 of bluejay-blheli-s.md merged
first and built the chooser
([slice 2 as built](bluejay-blheli-s.md#slice-2-as-built)): `USB BRIDGE`
is the second line of `UI_ESC_CFG_ENTRIES` in `src/ui_registry.cpp`, with
no screen, so this note's slice 2 gives it its screen there, as it does
([slice 2 as built](#slice-2-as-built)).

With [esc-commands.md](esc-commands.md#menu-entry)'s
option (a), MENU's first two rows read `ESC COMMANDS` and `ESC CONFIG`.
The rows of the other decisions of 2026-11-15 make MENU eight rows; the
resulting list is the revision line in [menu-b.md](menu-b.md#menu).

## What ports from DshotDisplay

DshotDisplay at 6693084 is the Arduino-era firmware: `Arduino.h`,
`Serial`, `delay()`, a `.ino` and no pin claim.

| File | Lines | Here |
|---|---|---|
| `bridge_wire.h`, `bridge_wire.cpp` | 140, 176 | the frame slot, the ring, the overrun count and the memory ordering port as they are; `bridgeWirePoll()` becomes the core1 job's step; `am32BlBegin()` and `am32BlEnd()` give way to the job's bind and release; the bit-banged `am32WriteRawEchoed()` and `am32ReadRaw()` give way to the PIO line; the teardown wait of `bridgeWireEnd()`, `delay(1)` up to 250 ms (lines 89 to 98), becomes the job's Stopping phase |
| `usb_bridge.h`, `usb_bridge.cpp` | 83, 126 | the counters and the transparent rule port; `Serial.available()`, `read()`, `write()` and `flush()` become `tud_cdc_*`; the two `delay(1)` loops (lines 80 to 97) become states; the stdio driver goes off instead of the `loop()` check |
| `bridge_log.h`, `bridge_log.cpp` | 61, 191 | a build option, off by default, as `AM32_BRIDGE_LOG` 0 is there (`config.h` 286 to 297): records in a 4 KB RAM ring, dumped over CDC after BACK (decision 8), written with `tud_cdc_write()` before the stdio driver comes back, so no telemetry line lands inside the dump, the order DshotDisplay keeps for the same reason (`usb_bridge.cpp` lines 49 to 55); `Arduino.h` and `millis()` from `plat.h` |
| `am32_bl.cpp`, `am32_bl.h` additions | 31, 66 | not ported: the PIO line replaces the raw bit-bang |
| `ui_am32.cpp` changes | 122 | not ported: the screen above replaces `drawBridge()`, written to menu structure B's layout |
| `config.h` changes | | `SERIAL_TELEMETRY` 0 there; here it stays 1 (decision 9) |
| `test_bridge.cpp`, `test_dump.cpp` | 421, 61 | port, below |
| — | — | new: MSP, the 4-way interface, mode detection, the PIO line, the entry |

Of DshotDisplay's 777 lines of bridge sources and headers, the log's 252
and about 150 of the slot, the ring and the counters port with small
changes; the rest is rewritten or replaced. `README.md` lines 316 to 323
at 6693084 describe an older behaviour, the echo held back until the
reply, that the code at the same commit has replaced (`usb_bridge.h` lines
29 to 45); the port follows the code.

## Tests

### Host

- **DshotDisplay's eight bridge sections** port with their fakes rewritten:
  a fake CDC in `test/stubs/tusb.h` (`tud_cdc_available()`, `read()`,
  `write()`, `connected()`), a fake PIO line on the virtual clock in the
  manner of `test/fake_kst_line.h` that echoes what is sent and loses a
  reply nobody reads, and the model of the Python tool's 25 ms
  `read_all()` and `len > 1` rule (`test_bridge.cpp` lines 44 to 145). The
  cases: a frame reaches the wire intact; echo then reply, ending in the
  ACK; the reply's offsets from the end; an unacknowledged `SET_BUFFER`
  that does not stall; a late reply during a repaint; a whole late reply in
  the ring; the tool's own ACK rule; housekeeping. `test_dump.cpp`'s line
  count ports with the log.
- **The AM32 configurator's Direct rule:** a model that sends a command,
  waits for its length plus the expected reply and strips its own length
  from the front (`direct.ts` lines 259 to 265); a read of 64 bytes, a
  `SET_BUFFER` with no reply, a 256-byte write.
- **Mode detection:** `$`, `/`, 0x00 and others as first bytes; the
  2000 ms idle ending passthrough and MSP, and not at 1999 ms; a later `$`
  inside a passthrough session that does not switch; the AM32
  configurator's sequence replayed with its 2000 ms pause after
  `MSP_SET_PASSTHROUGH` and a 10 s pause later, the 4-way state and its
  device kept through both; `cmd_InterfaceExit`, a port close and BACK
  ending it; a port close only on DTR falling after it was set, and a tool
  that never sets DTR keeping its mode until the idle time; the tools'
  recovery frames, `cmd_InterfaceExit` and `cmd_DeviceInitFlash` with no
  MSP before them, answered.
- **The gather:** a 258-byte frame in five 64-byte packets with a core0
  pass of 30 ms between the third and the fourth goes to the wire as one
  frame; two writes 3 ms apart go as two.
- **MSP:** each command of the table, its checksum, a bad checksum ignored,
  `$M!` for `MSP_SET_MOTOR` and an unknown command, `MSP_STATUS`'s bit 2;
  each payload at least the length of the table, read by a model of each
  tool's parser that fails past the end, so a reply cut by one byte fails;
  `MSP_SET_PASSTHROUGH` with mode 0xFD and with mode 1 answering 0;
  ESC-Configurator's connect sequence and the AM32 configurator's, replayed
  from the lines cited.
- **4-way:** each command, the CRC-16/XMODEM, a bad CRC answering
  `ACK_I_INVALID_CRC`, the interface mode from the signature at its bounds
  (0xE800 and 0xE801, 0xF8FF and 0xF900; 0x0006 and 0x0106, 0x8F06 and
  0x9006), channel 1 refused, the 3000 ms and 500 ms waits and not 1 ms
  before, against model bootloaders: AM32 (answers 0.3 ms after the last
  byte, ends a message after 260 µs of idle, keep-alive answered 0xC1) and
  BLHeli_S. If item (6) copies escwire's simulated ESC, the 4-way tests use
  it.
- **The wire rules:** no gap within a frame; a 4-way write's `SET_BUFFER`
  header and data as two messages at least 520 µs apart, against a model
  AM32 that ends a message on 260 µs of idle and drops data joined to the
  header, so a handler that sends them with no gap fails; the echo dropped
  in 4-way and kept in passthrough; a differing echo counted as a
  collision; overruns counted.
- **The claim:** `PinUser::Bridge` begins Free only, Stopping, Held,
  Running, BACK through Leaving and Restoring to Free; BACK disabled during
  a write-class frame and live again at its answer or at 3000 ms, not at
  2999 ms; the leave page after an unfinished write, after
  `cmd_InterfaceExit` too, and not after a RUN or `cmd_DeviceReset`;
  entry refused while USB DRIVE
  holds the card; the stdio driver off from Running to Free; a port close
  during a write keeping the claim and sending nothing.
- **The screen,** by control id: every string within its rectangle, the
  states' lines, the preview `docs/bridge-preview.png`.
- Every new rule mutation-checked: break it, see a test fail, restore.

### Bench

Only an ESC and a PC settle these:

- B1. Enumeration on Windows, Linux and macOS: one serial port and the
  card reader; PORT OPEN with each tool.
- B2. AM32 configurator, Direct ticked: read the settings, write one,
  flash the same firmware version again.
- B3. AM32 configurator, default: the MSP sequence, 4-way, the same three.
- B4. ESC-Configurator in Chrome: connect, read and write a Bluejay or
  BLHeli_S ESC and an AM32 ESC; flash.
- B5. BLHeliSuite with each of its interfaces that reaches a COM port:
  which ones work.
- B6. A logic analyser on SIG: no gap within a frame, the turnaround, the
  echo's timing against the bytes sent.
- B7. AM32 2.22 reaching its bootloader without a power cycle, after a
  power-on and after DShot stops.
- B8. A cut flash on each firmware at hand: the USB cable pulled during a
  page write; the ESC's state after a power cycle; a second flash through
  the bridge.
- B9. BACK after a session: does the ESC start its firmware once DShot
  resumes, and after how long; the tester's eRPM.
- B10. The USB cable pulled during a page write, the board on its battery
  and on USB power alone: the screen, SIG's level on the analyser, the
  ESC's state.
- B11. The receiver's rate limit on an ESC: `pio_uart_rx` loses bytes from
  a sender 3 % fast that sends back to back, in the model
  ([slice 1 as built](#slice-1-as-built)). An ESC bootloader on an RC
  (resistor-capacitor) oscillator, an EFM8's internal one among them, may
  run that far off: its bit time on the analyser, and a read of 256 bytes
  through the bridge at room temperature and warm.

## Work breakdown

Each slice builds, passes the host suite and merges alone. Sizes are new
and changed lines, rough.

| Slice | Content | Code | Tests | Docs | Depends on |
|---|---|---|---|---|---|
| 1. Line and claim | `PinUser::Bridge`, `Core1Job::Bridge`, the PIO line (`src/bridge_line.pio`), the slot and the ring from `bridge_wire.*`, the leave page per claim, the PIO budget table in [pin-claim.md](pin-claim.md) | about 300 | about 300 | STATUS, CHANGELOG | |
| 2. Passthrough and the screen | `src/usb_bridge.*`, the stdio switch in `usb_dev.cpp`, the screen `src/ui_bridge.*`, the entry by decision 1 (the `ESC CONFIG` chooser, with the build rule of the design call under [entry](#entry)), `test_ids.cpp`, the preview, DshotDisplay's tests, the log by decision 8; README section, wiki pages in English and German | about 450 | about 500 | about 200 | 1 |
| 3. MSP and the 4-way interface | `src/bridge_msp.*`, `src/bridge_4way.*`, the bootloader frames (escwire's if item (6) copied them), mode detection, the 3D note: byte 0x12 of each AM32 settings page forwarded (decision 12) | about 600 | about 700 | about 80 | 2 |
| 4. Bring-up | B1 to B11 | none | none | the record in STATUS | 1 to 3 |

Slice 2 alone serves the AM32 configurator in Direct mode and the Python
tool; ESC-Configurator needs slice 3.

### Slice 1 as built

Host tests only; nothing of it has run on the chip, and nothing on core0
opens the bridge yet: the screen, the USB side and the entry are slice 2.

**Files.** `src/bridge_line.pio` (the transmitter), `src/bridge_line_port.*`
(the line on two state machines, device-only), `src/bridge_wire.*` (the
job, the frame slot, the ring, the counts), `src/bridge.cmake`;
`PinUser::Bridge` and its row in `PIN_USERS` (`src/pin_claim.h`),
`Core1Job::Bridge` and its line in the job table (`src/core1_job.*`).

**The claim.** The row is `{ PinUser::Bridge, Core1Job::Bridge, false, false,
false }`: no leave page from the row, no ESC check, the job started at Held,
as [the pin](#the-pin) has it. The leave page per claim is the mechanism ESC
SETTINGS brought ([bluejay-blheli-s.md, slice 2 as
built](bluejay-blheli-s.md#slice-2-as-built)): `pinClaimWantLeavePage()` gives
a claim that began without the page the page for its rest, and
`pinClaimLeavePage()` reports it. Slice 1 adds no call of it; slice 2, whose
passthrough names the write-class frames, makes it. One rule of the claim
changes: a claim without the leave page restores DShot only once its job is
idle (`leaveStep()` in `src/pin_claim.cpp`). Before, such a claim restored
within `pinClaimEnd()`, which was right only for AM32, the one user without a
page, which has no job; the bridge has a job, and DShot rebuilt while its
state machines still drive SIG would put DShot frames and bootloader bytes on
one pad. AM32 still restores within the call. `settle()`, which every call
into the claim runs, `pinClaimState()` included, takes Leaving on, so the
claim reaches Free once the job is idle even when the screen that ended it has
closed and only MENU looks at the claim.

**The line.** `bridge_tx`, 7 words: `pull`, the start bit with the pad's
direction set by side-set, eight data bits by `out pins`, the stop bit,
then `out pindirs` with bit 8 of the word, which keeps the pad driven for a
byte with more of its frame behind it and lets it go after the last. The
receiver is `pio_uart_rx` as it is, 9 words, on the same pad, in the block
the transmitter took, at the same divider, 976 + 144/256 at 150 MHz, and
enabled in the same cycle (`pio_enable_sm_mask_in_sync()`). Both FIFOs are
joined: 8 words each. The pad is an input with its pull-up from the open,
between frames and after the close. As measured in the model
(`test/pio_bridge_emu.py`): start bit and data bits 8 cycles each, the stop
bit driven 8 cycles; bytes of a frame 82 cycles, 10.25 bit times, apart,
the extra 2 cycles being `out pindirs` and `pull`; the last byte's pad let
go 80 cycles after its start bit; its echo in the receive FIFO 78 cycles
after the start bit, 3 cycles before the transmitter waits on `pull`.
Errors at bind are `CORE1_JOB_CLOCK` (`clk_sys` under 153,300 Hz) and
`CORE1_JOB_PIO`.

**The line against `ew_line`.** Item (6) copied rcbench's `ew_line`
(slice 1 of [bluejay-blheli-s.md](bluejay-blheli-s.md#slice-1-as-built)).
It runs one transaction: bits pre-expanded into FIFO words by DMA, the pad
let go, then bytes received until a wait in ticks runs out; between
transactions it stalls with nothing received, and it does not sample the
pad while it sends, so it gives no echo. Passthrough needs the echo at wire
pace, and needs every byte the ESC sends whenever it sends it, since the
bridge does not know where a reply ends. The bridge therefore uses its own
line: 16 words, two state machines, no DMA channel, against `ew_line`'s 25
words, one state machine and two DMA channels. The two never run together:
each belongs to its own claim. Slice 3 can still use escwire's frame and
CRC code for the 4-way interface.

**The job.** The slot and the ring port DshotDisplay's `bridge_wire.*`
with release and acquire on each index and flag, and these differences:

- no call blocks: the bind, a pass and the release each return at once;
- the ring holds all 1024 bytes, not 1023;
- the ring reads nothing while the line is closed, so the bind can empty
  it from core1;
- the echo is the wire's: the job keeps the bytes it put in the FIFO, at
  most 8, the receive FIFO's depth, so its own echo never fills that FIFO
  while core1 is away. It takes the next bytes received as their echo; a
  differing echo makes its frame a collision, and so does an echo still
  owed once the line is idle, which the job reads before it drains the
  FIFO. A frame counts once. `bridgeWireKeepEcho()` keeps echoes out of
  the ring for the 4-way interface;
- `lost` counts bytes the full ring refused and each stall of the receiver
  on a full FIFO (FDEBUG's RXSTALL), not the bytes the stall cost;
- the release finishes the frame under way and drops one not started, then
  closes the line once it is idle, or after 250 ms
  (`BRIDGE_RELEASE_MAX_MS`), where DshotDisplay's `bridgeWireEnd()` waited
  with `delay(1)`. A full slot, 320 bytes, takes 171 ms.

**Tests.** `test/pio_bridge_emu.py`, run by `make pioemu` and held to pioasm's
words in CI: 42 runs of the two programs on one pad, frames of 1, 2, 21 and 64
bytes, late bytes, an ESC's reply 0.3 ms after the release at 8 phases with bits
as long as the line's, 2 % and 3 % longer and 2 % shorter, a collision and a
full receive FIFO. A control run with the ESC's bits 3 % shorter loses bytes at
every phase: `pio_uart_rx` samples the stop bit 9.6 bit times after the start
edge and looks for the next start bit only then, so a sender that fast, back to
back, gets ahead of it. Whether an ESC's bootloader sends that fast is not
verified; bench item B11. `test/test_bridge.cpp`, 162 checks in 8 sections,
against `test/fake_bridge_line.h`, a wire on the virtual clock with the model's
cycle counts and a model ESC: the divider; the slot's refusals and handover; the
frame on the wire whole, with a pass every 1 ms and every 5 ms; the echo kept
and dropped, differing and lost; the receive FIFO and the ring full, the ring's
wrap; the release; the pin user's row and the claim's rules for it; the claim
through the job table from Stopping to Free, DShot never bound while the line is
open and asked back only once the job is idle, also with nothing but
`pinClaimState()` called after BACK; BACK while Stopping; SIG on GP1; the leave
page given and kept; RETRY. `bridge_line_port.cpp` is built by the firmware and
typechecked by `make syntax` only.

**Mutations.** 36 of the C++ code and 6 of the PIO program, each applied
alone. All 6 of the program fail the model. Of the 36, 32 fail the suite;
one, the release clearing the slot after the close, survived and showed the
line redundant, since a closed line refuses sends and the next bind clears
the slot, and the line is removed. Three survive, equivalent on the host:

- reading the idle state after draining the receive FIFO rather than
  before: on the chip the last echo can arrive between the two reads and be
  written off as missing; the fake does not move its clock within a pass;
- the release's test of the frame under way beside the idle state: on the
  chip the line can go idle between the job's write-off and its feed while
  8 echoes are owed; on the host no time passes there;
- the bind setting the owed echoes to 0: every release already does.

### Slice 2 as built

Host tests only; nothing of it has run on the chip, against an ESC or
against a desktop tool.

**Files.** `src/usb_bridge.*`, the USB side on core0;
`src/bridge_log.*`, the traffic log of decision 8; `src/ui_bridge.*`, the
screen; `usbDevConsole()` and `usbDevConsoleOn()` in `src/usb_dev.*`, the
console switch; `usbBridgeTask()` in core0's `loop()` right after
`usbDevTask()`; `UiScreenId::Bridge`; the second line of
`UI_ESC_CFG_ENTRIES` in `src/ui_registry.cpp`, now with its screen and the
lines `A DESKTOP ESC TOOL OVER USB.` and `PASSTHROUGH: AM32 DIRECT MODE.`;
`USB_BRIDGE_LOG` 0 and `USB_BRIDGE_LOG_BYTES` 4096 in `src/config.h`. Tests:
`test/fake_cdc.*`, the serial port with a PC behind it;
`test/test_usb_bridge.cpp`; `test/test_bridge_ui.cpp`;
`test/test_bridge_log.cpp`, a program of its own built with the log on;
USB BRIDGE in `test/test_ids.cpp` and ESC CONFIG's live entry in
`test/test_menu.cpp`; the preview `docs/bridge-preview.png`.

**The console switch.** `usbDevConsole(false)` disables the stdio driver,
`usbDevConsole(true)` enables it again; a call that changes nothing does
nothing. It is the only change to `usb_dev.cpp`'s behaviour. `usb_dev.cpp` is
not in the host suite; the suite's fake of the two calls records the switch.

**When the bridge is open.** `usbBridgeTask()` opens the USB side while the
claim is Running for `PinUser::Bridge` and the line is open
(`bridgeWireOpen()`), and closes it on any other state, so the screen's BACK
closes it on the next pass through `pinClaimEnd()`'s Leaving, and a screen
that has closed leaves nothing open. Only while open does it read the port
or hand a frame to the slot. The open takes the console off, drops what
TinyUSB holds, sets the echo kept, zeroes its counts and waits. The close
drops a frame being gathered and begins the log's dump. The console comes
back on the first pass after the dump at which the claim is no longer the
bridge's: Free. A claim whose bind failed never opens, and the console
stays on.

**The developer's calls of this slice**, under the note's decisions:

1. **Passthrough only.** Every first byte starts passthrough, `$` and `/`
   included; slice 3 gives those two MSP and the 4-way interface. Until
   then an MSP tool's bytes reach the ESC as through a USB-serial adapter,
   and the tool gets its own bytes back.
2. **What the open drops** is what TinyUSB holds, 64 bytes at most. A PC
   that queued more before the open, held off by flow control, has the
   rest forwarded as its first frame.
3. **The gather's clock is `micros()`**: a frame goes to the slot at the
   first pass 2000 µs or more after its last byte, not at 1999 µs, read
   before the clock is looked at. A write longer than the slot goes as 320
   bytes and the rest.
4. **A write-class frame's answer** is a byte the wire receives beyond the
   frame's echo: more than the wire's received count at the handover plus
   the frame's length. An echo of the frame before still owed at the
   handover makes that count reach its mark early, and BACK live before
   the answer; the tools wait for an answer before the next frame, and the
   leave page of decision 10 does not depend on it.
5. **The unfinished write** is set by a write-class frame and cleared by a
   RUN, both since the open. The greeting, twelve 0x00 bytes first, is not
   a RUN: a frame whose last 9 bytes are 0x0D, `BLHeli` and two more is
   `INIT`. The screen gives the claim the leave page at BACK, not at the
   write, so a RUN after the write leaves BACK without it. Once given, the
   page lasts until Free, the claim's rule: BRIDGE and BACK again show it
   whatever was sent between.
6. **The closed line**, `THE PC CLOSED THE PORT.`, shows after DTR falls
   with a byte forwarded either way since the open, and goes with the PC's
   next byte.
7. **Colours the note does not name.** `PORT OPEN` on `UI_COL_LIVE`, a live
   link; `PORT CLOSED` on `C_GRID`, as MENU's `NO ESC` chip. The frames
   line in `C_TEXT`, `LOST` amber above 0, as the note says.
8. **Before the open** the screen draws no mode and no counts: the strip
   and the six lines while the claim stops and binds; line 0 alone after a
   refusal, with USB DRIVE holding the card, or after a failed bind, which
   adds RETRY.
9. **The last command's line.** `LAST SET ADDRESS 7E00` with the address,
   `LAST PAYLOAD 258 B` with the length, `LAST FRAME 42` with the first
   byte for one with no name, the name alone otherwise.
10. **Collisions** are counted in `UsbBridgeStats` from the wire's count and
    recorded in the log; the screen shows none, as the note's frames line
    has none. A frame the PC sends while the bench still sends the last
    waits whole in the gather and goes right behind it; when the ESC's
    answer to the first meets it on the wire, that is a collision, and the
    bytes reach the PC as the wire read them.
11. **USB DRIVE** refuses the entry while it is serving or handing over the
    card (`mscHoldsCard()`). The screen asks again every frame, as it does
    after a refused claim.
12. **The log's dump** is a state advanced once a pass: as much as the port
    takes, then a flush, the last line's too. A dump that writes nothing for
    400 ms is abandoned, where DshotDisplay gave up on a line after 400 ms.
    `CFG_TUD_CDC_RX_BUFSIZE` stays 64.

**Tests.** `runUsbBridgeTests`, 143 checks in 18 sections, on the CDC fake, the
line fake and the claim: the naming; DshotDisplay's sections, a frame reaching
the wire whole, the reply's offsets from the end, an unacknowledged SET BUFFER
that does not stall, a late reply during a repaint, a whole late reply in the
ring, the Python tool's 25 ms reads and `len > 1` rule on a 258-byte write, and
the counts, the input dropped at the open and the console; the AM32
configurator's Direct rule on a model bootloader, SET ADDRESS, a read of 64
bytes, SET BUFFER with no reply, a 256-byte write and PROGRAM; the gather, a
258-byte frame across a 30 ms pass, writes 3 ms and 1 ms apart, 1999 µs and 2000
µs, 400 bytes as 320 at once and 80; the modes, `$`, `/`, 0x00 and others first,
1999 ms and 2000 ms idle, an ESC answer and a frame waiting for the slot keeping
the session, a later `$`, DTR falling only after it was set, a tool that never
sets it; a port close while gathering; the write rules, 2999 ms and 3000 ms, the
answer, ERASE, the payload, RUN, the greeting, a port close after a write; BACK
with the PC still connected, nothing read and nothing new sent; the console
through the leave page and BRIDGE; a refused bind and another screen's claim; a
collision; LOST. No PC byte reaches the wire while the claim is not Running for
the bridge, and DShot is never up with the line open, checked on every pass of
every section. `runBridgeUiTests`, 119 checks in 5 sections: the entry, the
screen's every line and width at rest, Stopping, PORT OPEN and CLOSED, the
counts and their 250 ms, the closed line, a write with BACK disabled, the leave
page and its steps, BRIDGE and RESTART DSHOT, BACK while Stopping, after a
failed bind and with no claim, RETRY, USB DRIVE, another claim, LOST, SIG on
GP1. `test_bridge_log`, 25 checks: DshotDisplay's dump test, every one of 14 and
of 146 records reaching the PC, the text, a slow PC, a transfer of more than 255
bytes, a PC that stops reading, and the dump before the console in
`usb_bridge.cpp`; it found the dump's last line left unflushed, now fixed.

**Mutations.** 54 of the new code, each applied alone: 33 in
`usb_bridge.cpp`, 14 in `ui_bridge.cpp`, 7 in `bridge_log.cpp`. In the
first run 41 failed the suite or `test_bridge_log`. Of the 13 that
survived, 8 showed a test missing and fail now: the USB side's open read
before core1's release after BACK, the open turning the echo on whatever
was set before, a full slot handed over at once, a frame waiting for the
slot keeping the session, the ESC's bytes keeping it, a SET BUFFER
forgotten at a session's end, a slow PC's dump not abandoned while it
moves, the 255 of a longer transfer. One, the close clearing the gathered
frame, showed the lines redundant, since the next open starts an empty
frame and nothing hands one over while closed; they are removed. Four
survive:

- `claimRunning()` without its holder test, and without its test of the
  open line: Running with the line open happens for the bridge's claim
  only, so each of the three tests implies the others as the claim runs;
  the state test is the one a pass between BACK and core1's release
  needs, and fails without it.
- the counters without their clamp at 9999999: no test sends 10 MB.
- RESTART DSHOT without the screen's own `pinClaimUnplugDue()` test:
  `uiLeaveTouch()` returns Restart only once the page is released and
  live, which needs the same state; `pinClaimRestart()` checks it too.

`usb_dev.cpp`'s console switch is device-only and typechecked by
`make syntax`; the suite's fake of it records the calls.

**The clock's serial line, to wire after the merge.** Slice 5 of
`run-summary.md` (pull request #98, open as this slice is built) reads a
`CLOCK` line from the same serial port in `usbDevTask()` and adds
`usbDevClockLineEnable(bool)`. TODO once #98 is on `main`: the bridge
turns the clock line off where it takes the console off and on where it
gives it back, and a test sends a `CLOCK` line while the bridge holds the
port and finds it on the wire as data, not parsed.


## Decisions

Decisions of the owner, 2026-11-15, answering the open questions of
2026-11-14. Each is the note's recommendation.

1. **Where the bridge is entered.** Option (c), with decision 1 of
   [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions) (its option e):
   AM32 CONFIG's row becomes the chooser `ESC CONFIG`, holding `ESC
   SETTINGS` and `USB BRIDGE`. `ESC SETTINGS` is the one settings screen
   for AM32, Bluejay and BLHeli_S. This is the recommendation.
2. **Which modes.** Option (a): both, passthrough first as slice 2, MSP and
   the 4-way interface as slice 3. This is the recommendation.
3. **How the mode is chosen.** Option (a): from the PC's first byte.
   This is the recommendation.
4. **One serial port or two.** Option (a): the existing port, with the
   console off while bridging. This is the recommendation.
5. **What the board calls itself in MSP.** Option (a): `MRCB`, and `MRCB4W`
   as the 4-way name. This is the recommendation.
6. **Send RUN when the bridge is left.** Option (a): nothing. This is the
   recommendation.
7. **BACK during a flash.** Option (a): drawn disabled while a write-class
   frame waits for its answer, at most 3000 ms. This is the
   recommendation.
8. **The traffic log.** Option (a): DshotDisplay's log as a build option,
   off by default. This is the recommendation.
9. **`SERIAL_TELEMETRY`.** Option (a): stays 1. This is the
   recommendation.
10. **BACK after an unfinished write.** Option (a): the leave page with
    `POWER THE ESC OFF.`, `RESTART DSHOT.` and `FLASH THE ESC AGAIN BEFORE
    USE.`. This is the recommendation. Revised by the developer on
    2026-11-15, in the review of slice 2: the page comes once a bootloader
    is reached, an INIT answered or a write-class frame sent, until a RUN.
    A session that only reads leaves the ESC waiting in its bootloader as
    one that writes does, and DShot frames then reach it; an EFM8
    bootloader checks only the CRC's low byte, so about 1 garbled frame in
    256 passes as an erase or a program
    ([bluejay-blheli-s.md](bluejay-blheli-s.md#safety), decision 8 there).
    The note `FLASH THE ESC AGAIN BEFORE USE.` stays for a write.
11. **The card served while bridging.** Option (a): the entry is refused
    while USB DRIVE holds the card, with `EJECT THE USB DRIVE FIRST.`. This
    is the recommendation.
12. **The 3D note after a bridge write.** Option (a): the bridge reads
    byte 0x12 of each AM32 settings page it forwards and sets or clears the
    note by item 24's rule. Slice 3 designs how the bridge knows the settings
    page's address in passthrough and 4-way. This is the recommendation.

## Open questions

None open. Questions 1 to 12 are answered by decisions 1 to 12.

## Not verified

- BLHeliSuite's and BLHeliSuite32's interfaces: which speak MSP and the
  4-way interface, which raw one-wire, whether they expect an echo, and at
  what rate. The Bluejay configurator's.
- Whether each tool sets DTR on opening the port.
- That the RP2350's receive SM reads the pad while the transmit SM drives
  it, on this board, and the echo's timing.
- The length of core1's pass during the bridge's claim.
- The bridge's line on the chip: the transmitter's timing, its release,
  and the receiver's reading of an ESC's bytes; only a model of the two
  programs has run them ([slice 1 as built](#slice-1-as-built)).
- AM32's answer delay of 0.3 to 0.35 ms, from rcbench's specification;
  BLHeli_S's and Bluejay's turnaround.
- That the pad's pull-up holds SIG high against AM32's bootloader pull-down
  at power-on; that AM32 2.22 reaches its bootloader without a power cycle.
- What an AM32 with a half-written firmware whose first words are in place
  does; whether a BLHeli_S or Bluejay ESC with a half-written firmware
  reaches its bootloader at power-up; BLHeli_32's bootloader.
- What AM32's, BLHeli_S's and Bluejay's bootloaders do with DShot frames
  after BACK, and whether AM32 starts its firmware about 100 ms after DShot
  resumes.
- Whether any ESC firmware arms on 19200 baud traffic on its signal wire.
- What an unpowered RP2350 pad does to SIG when the cable of a board on
  USB power alone is pulled; how a pulled cable shows on the board's
  battery, which `usb_msc.cpp` reads as a suspend from TinyUSB's source.
- How much core0's longest pass delays an answer during the bridge.
- AM32 bootloaders older than 578ff29, and AM32 firmware other than 2.22.
- Everything on hardware: no byte of this design has gone to an ESC from
  this board, and DshotDisplay's bridge has no hardware record either.
