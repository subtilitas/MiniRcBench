**English** | [Deutsch](Fehlersuche)

# Troubleshooting

Symptom first, then cause. ESC means electronic speed controller.

---

## The screen stays dark

MiniRcBench runs on the Waveshare RP2350-Touch-LCD-2.8 only. Check the board:
the 2.8" has JST-SH connectors and no 2.54 mm headers. For the
RP2350-Touch-LCD-2, use
[DshotDisplay](https://github.com/subtilitas/DshotDisplay).

To reflash: hold **BOOT**, tap **RESET**, release **BOOT**. The board appears
as a USB (Universal Serial Bus) drive whatever firmware it holds. Copy the
`.uf2` file onto it: [Setup](Setup#installing-the-firmware).

On an RP2350-Touch-LCD-2.8, a dark screen has no known firmware cause. Report
it as described under [Still stuck](#still-stuck).

---

## The screen does not react to touch

<img src="img/splash-no-touch.png" width="240" alt="The splash with TOUCH CONTROLLER NOT FOUND in amber above the name">

`TOUCH CONTROLLER NOT FOUND` in amber on the splash means the touch
controller did not answer the firmware at power-up. The line goes as soon
as it answers. If it is still there when the tester appears, touch did not
answer during the splash; the tester keeps reading it, and a controller
that answers later works. If taps still do nothing, try the steps below.
After an update that reset the settings, it shows under the two settings
lines.

<img src="img/splash-reset-no-touch.png" width="240" alt="The splash with SETTINGS RESET BY UPDATE, CHECK MENU > SETTINGS and TOUCH CONTROLLER NOT FOUND above the name">

Without touch the board cannot be operated: arming, `DISARM`, `HOLD` and
`MENU` all need it. The idle interlock disarms 30 s after the last contact
the controller reports. A wet screen, or something lying on the glass,
counts as a contact and keeps an armed tester armed. To stop a motor,
disconnect the ESC's battery.

Try, in this order:

1. Dry the screen, and take away anything lying on the glass, a lead or a
   tool: it can count as a finger and block every tap.
2. Switch the board off and on: unplug USB. With a cell fitted the board
   keeps running from the cell, so disconnect the cell too.

Not known: why a controller does not answer. A controller that does not
start, its flex cable inside the display and a stuck I2C (Inter-Integrated
Circuit) bus are not told apart. The flex cable is part of the board and
not something to open; no repair step is verified here. If the line stays
after a restart, report it as described under [Still stuck](#still-stuck).

---

## NO TELEMETRY

<img src="img/tester-noesc.png" width="240" alt="No telemetry">

Nothing answers on the signal wire, and the chip under the title on the
`MENU` screen reads `NO ESC`. Check in this order:

1. **The ESC has not been power-cycled since AM32 CONFIG.** Leaving that
   screen sends the bootloader no command to start the ESC firmware, and
   what the ESC does then is not recorded. Power-cycle the ESC before you
   test it: [AM32 Configuration](AM32-Configuration#leaving).
2. **Wrong pin.** Compare **MENU → SETTINGS → ESC PIN** with where the wire is.
   The `LINK` row on that screen is live. A packet rate above zero means the
   pin is correct.
3. **No common ground.** The ESC ground and the board ground must be
   connected.
4. **The ESC has no power.** It runs from its own battery, not from the board.
5. **The ESC does not support bidirectional DShot**, or it is disabled in the
   ESC's settings. Unidirectional DShot sends nothing back.
6. **The wire is too long or picks up noise.** Set **DSHOT KBAUD** to 300 and
   watch `LINK`.

---

## RPM works, every tile reads `--`

The ESC sends eRPM (electrical RPM) and no EDT (Extended DShot Telemetry).
Either it does not support EDT, or EDT is not enabled yet.

Check the chip under the title on the `MENU` screen. If it reads `EDT OFF`,
see the next section. If the ESC firmware does not support EDT, two wires give
RPM only. A [KISS telemetry wire](Telemetry#the-optional-third-wire) provides
voltage, current, temperature and mAh.

---

## EDT stays off

The firmware sends the enable every 1 s while eRPM arrives and EDT does not.
A chip that stays red for more than a few attempts means the ESC does not
accept them.

- **Is the motor stopped?** The enable is sent only while eRPM is zero. A motor
  coasts for seconds after a disarm.
- **Is the tester disarmed?** The enable is not sent while armed.
- **Does the ESC firmware support EDT?** ESC firmware without EDT ignores the
  enable and looks like this.
- **Power-cycle the ESC.** After 500 ms without eRPM the firmware resets its
  EDT state and starts again.

If EDT stays off with an ESC whose firmware supports EDT, open an issue (see
[Still stuck](#still-stuck)).

---

## Known limitation: BLHeli_32 32.6 on RAZOR32 and EXUAV 70A

Reported by a user in
[DshotDisplay issue #1](https://github.com/subtilitas/DshotDisplay/issues/1), open, not
reproduced by the project. With RAZOR32 and EXUAV 70A ESCs running BLHeli_32
32.6, the ESC does not finish initialising, and the zero-throttle beep is
missing. The cause is unknown. No workaround is known.

---

## `HOLD TO ARM` is an outline and does nothing

<img src="img/tester-card-held.png" width="240" alt="HOLD TO ARM drawn as an outline, the bar reading EJECT THE USB DRIVE TO ARM">

The throttle bar reads `EJECT THE USB DRIVE TO ARM`: a computer holds the SD
card over USB (**MENU → SD LOG → USB DRIVE**; drag `MENU` up 36 px to reach
SD LOG), and arming is refused until it lets go. Eject the drive on the
computer, or press `EJECT` on SD LOG. See [SD Logging](SD-Logging).

A computer that unconfigures the drive also gives the card back. Pulling the
cable with the board on its battery does not: the USB stack of this firmware
sees a pulled cable as a computer that went to sleep, and a sleeping computer
keeps the card. Press `EJECT` on SD LOG then. Neither case is tested on
hardware.

---

## The motor stops on its own

Four interlocks stop the motor, and each one shows on screen:

- **Idle interlock.** 30 s without a contact reported by the touch
  controller while armed disarms the tester. A wet screen or something
  lying on the glass counts as a contact.
  `IDLE ns` counts down in the top right during the last 5 s. The backlight
  dims briefly when it disarms.
- **Opening `MENU`.** Opening it always disarms.
- **Touch not answering.** Four failed reads of the touch controller in
  a row, over 100 ms or more, disarm an armed tester. `TOUCH NOT
  ANSWERING` shows in amber under the RPM readout until it answers again.
  A loose flex cable to the panel or a stuck I2C (Inter-Integrated
  Circuit) bus can cause it; which one is not told apart.
- **Heartbeat backstop.** If the display firmware stops, the second core sets
  the throttle to zero after 250 ms. The screen would also be frozen.

If the motor stops without any of these, check the ESC's own protections (low
voltage cutoff, temperature, desync), and look at `ESC STATUS` and `ESC TEMP`.

---

## The board restarts on its own

Open **MENU → SETTINGS** and scroll to DIAGNOSTICS. `LAST RESTART` names why
the board last started. `BROWN-OUT` or `GLITCH` in amber after a restart while
a motor ran points at the board's supply: power the ESC from its own battery
and the board from USB. See [The Screens](The-Screens#settings).

---

## `ESC STATUS` reads `WARN`, `ERROR` or `ALERT`

The tile is amber for `WARN`, red for `ERROR` and magenta for `ALERT`. The
ESC sends the status over EDT; the ESC firmware defines what each one means.

1. **Disarm.** Press `DISARM`.
2. **Let the ESC cool.**
3. **Read `ESC TEMP` and `CURRENT`.** `ESC TEMP` turns red at 90 °C.
4. Look the status up in the ESC firmware's documentation before you arm
   again. See [Telemetry](Telemetry#the-tiles).

---

## `LINK` shows a high error rate

The error rate is amber above 5 %. The usual cause is the signal wire: length,
routing, or a missing ground. Shorten it, route it away from the motor phase
wires, then lower **DSHOT KBAUD**.

Failed packets are frames the ESC did not act on, even while the RPM readout
works.

---

## The card is not detected

<img src="img/log-nocard.png" width="240" alt="No card">

Check the `MOUNT` row on the SD LOG screen.

- **`3 NOT READY`**: the card did not start. With
  `INSERT A CARD, THEN RETRY MOUNT` nothing answered: no card, or not seated.
  With `CARD NOT USABLE: RESEAT OR REPLACE` a card answered and failed:
  reseat it, or try another.
- **`13 NO FILESYSTEM`** with a size under `CARD`: the card answers and the
  filesystem is the problem. Format it as FAT32.
- **Card inserted after boot**: press `RETRY MOUNT`. The card is mounted only
  once, at boot, and the board has no card-detect pin.

---

## Dropped frames in a log

`DROPPED FRAMES` above zero means the log has gaps. Check `BUF PEAK` and
`WORST FLUSH`. A peak near the buffer size (8192 bytes), or a flush longer than
the buffer covers (about 1.1 s), means the card is too slow.

Try a different card first.

---

## AM32 will not connect

The bootloader listens only briefly after power-up. **Unplug and replug the
ESC battery while the AM32 screen is open.** A power-cycle before opening the
screen does not work.

- **`REPLY NOT UNDERSTOOD`**: the link works and the framing does not. Possible
  cause: the ESC does not run AM32.
- **`NO VALID SETTINGS`**: the ESC answered, and the settings block failed the
  plausibility check. Do not write anything.
- **`LAYOUT n NOT KNOWN`**: the ESC stores a settings layout the tester does
  not know; it shows `THE TESTER KNOWS LAYOUTS 2 TO 4.` and writes nothing.
  Use the AM32 configurator for this ESC.
- **`READ FAILED (...)`**: the handshake worked and the settings read did not.
  Check the signal wire and ground.

---

## My settings reset themselves

They were not saved, or new firmware changed the settings layout. Pole
count and throttle ceiling are edited on **MENU → SETTINGS**, and
`HOLD TO SAVE` there writes them to flash. The note line reads
`UNSAVED - HOLD SAVE TO KEEP` while changes are unsaved, and `MENU` shows
`UNSAVED` on its `SETTINGS` row.

Firmware whose settings layout differs from the one in flash starts from its
defaults. Updating from earlier firmware to firmware with settings layout
version 4 returns every saved setting to its default once, the ESC pin
included: the ESC pin is then GP0, KISS is off and SERVO SUPPLY is `NONE`.
Version 5 adds CURRENT SENSE, so updating from version 4 to version 5 does
the same once more: every saved setting returns to its default, and
CURRENT SENSE is `NONE`. Set them again and save with `HOLD TO SAVE`.

Such a reset is shown: the splash reads `SETTINGS RESET BY UPDATE` and
`CHECK MENU > SETTINGS`, MENU's `SETTINGS` row reads `UPDATED`, and the note
line in SETTINGS reads `RESET BY UPDATE - CHECK AND SAVE` until a save
lands. A block that is damaged, rather than written by another version, is
discarded without these notes; the note line then reads
`COMPILED DEFAULTS IN USE`.

---

## `SDERR` on the tester, or a log that stopped

The recording ended without `STOP`. **MENU → SD LOG** says how above
`START`: a full card, a failed write, a file that did not open, or the tool
(KST SERVO, HITEC SERVO, AM32 CONFIG) that took the card. See
[SD Logging](SD-Logging#how-the-last-log-ended).

---

## KISS telemetry is off

That is the default. Turn it on under **MENU → SETTINGS → KISS TELEM**.

The board has two free GPIOs (general-purpose input/output pins), GP0 and
GP1, and the ESC signal starts on GP0. KISS defaults to off so that the
spare pin is not used for a wire that may not be fitted.

Turning KISS on moves the KISS pin to the other free pin if it coincides with
the ESC pin. The pin stepper skips the ESC pin. The receiver is a PIO
(programmable I/O) state machine and works on either pin.

If KISS switches itself off with `KISS OFF: NEEDS A PIN OF ITS OWN`, the
stored KISS pin was not free or was the ESC pin. Move the ESC signal to the
other pin first, then turn KISS on.

---

## Still stuck

Open an issue at
[github.com/subtilitas/MiniRcBench/issues](https://github.com/subtilitas/MiniRcBench/issues).
Include: the firmware version (SETTINGS → VERSION), LAST RESTART, what the
`LINK` row shows, and which ESC and ESC firmware.
