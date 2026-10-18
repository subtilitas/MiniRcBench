# Design notes

These documents record the design decisions behind each feature: protocol
details, pin choices, and what runs on which core. The document for a planned
feature describes the system as it will be; the document for a built feature
describes the code as it is. Release history is in
[CHANGELOG.md](../../CHANGELOG.md).

A document that no longer matches the code is a bug. When a feature changes,
update its document, or fold it into the README and delete it.
[STATUS.md](../../STATUS.md) holds the verification record for the whole
firmware; the status lines here summarise it per feature.

| Document | Status |
|---|---|
| [kiss-telemetry.md](kiss-telemetry.md) | Implemented. Working on hardware (owner report). Off by default. |
| [blackbox-logging.md](blackbox-logging.md) | Implemented. Hardware use recorded with the previous SDIO driver; none recorded with the current one. Buffer sizing unmeasured. |
| [sdio-driver.md](sdio-driver.md) | Implemented, not tested on hardware. MIT SDIO driver written from the SD specification; replaces the SD library's GPL-derived one. Host tests against a simulated card; the PIO programs in a cycle-level model. |
| [pin-claim.md](pin-claim.md) | Slices 1 and 2 built, host tests only, not tested on hardware: a screen takes the ESC and KISS pins from DShot while it is open, after a 250 ms zero-throttle drain; the AM32 editor and the servo tester run on it. Shared hardware facts and the slice plan. |
| [servo-tester.md](servo-tester.md) | Built, host tests only, not tested on hardware. Servo PWM (pulse-width modulation) output, manual position, sweep, two servos, on the pin claim. |
| [menu.md](menu.md) | Slice A built, host tests only, not tested on hardware: one list widget, `ui_list`, under the AM32 and SERVO SET lists, and one hold widget, `ui_hold`, under four holds. Slices B and C planned: a MENU list of tools and actions from a registry replaces the settings screen; SETUP becomes one sectioned SETTINGS list; host tests by control id; a 36 px header. Slices A to C come before KST programming. |
| [kst-programming.md](kst-programming.md) | Planned. KST and Chaservo servo programming, ported from rcbench, with an optional bench-switched servo supply on GP0. Depends on the pin claim and on the menu restructure. |

## Shared context

KISS telemetry, the serial protocol of the KISS ESC (electronic speed
controller) line, sent on its own wire, supplies finer voltage and current
than EDT (Extended DShot Telemetry, carried in the replies of the DShot digital
ESC protocol). The log is the main use of that resolution: a voltage trace in
0.25 V steps hides a 200 mV sag.

MiniRcBench runs on one board, the Waveshare RP2350-Touch-LCD-2.8.

SD (Secure Digital) logging runs over four-bit PIO (the RP2350's programmable
input/output) SDIO (Secure Digital input/output) at 10 MHz, on the driver in
`src/sdio/` ([sdio-driver.md](sdio-driver.md)), which has no hardware record
yet. A USB (Universal Serial Bus) host can read the card as a read-only drive.

KISS telemetry is implemented. The owner reports it working on hardware, the
display's own telemetry request included; whether that report covers the
RP2350-Touch-LCD-2.8 is not recorded. The receiver is a PIO state machine, so
any free GPIO (general-purpose input/output) can carry the wire. The board
frees two GPIOs, GP28 and GP29. The ESC takes GP29 and KISS defaults to GP28,
switched off until the user turns it on.

What remains is measurement. BUF PEAK and WORST FLUSH under sustained logging
should size the ring buffer; 8 kB (`SD_LOG_BUFFER_BYTES`, 8192 bytes) is
unmeasured, and the SD LOG screen shows both numbers so they can be read off
the device. On the KISS side, the line voltage is not measured.
