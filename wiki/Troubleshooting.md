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
as a USB (Universal Serial Bus) drive whatever firmware it holds.

On an RP2350-Touch-LCD-2.8, a dark screen has no known firmware cause. Report
it as described under [Still stuck](#still-stuck).

---

## NO TELEMETRY

<img src="img/tester-disarmed.png" width="240" alt="No telemetry">

Nothing answers on the signal wire. Check in this order:

1. **Wrong pin.** Compare **MENU → SETTINGS → ESC PIN** with where the wire is.
   The `LINK` row on that screen is live. A packet rate above zero means the
   pin is correct.
2. **No common ground.** The ESC ground and the board ground must be
   connected.
3. **The ESC has no power.** It runs from its own battery, not from the board.
4. **The ESC does not support bidirectional DShot**, or it is disabled in the
   ESC's settings. Unidirectional DShot sends nothing back.
5. **The wire is too long or picks up noise.** Set **DSHOT KBAUD** to 300 and
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

## The motor stops on its own

Three interlocks stop the motor, and each one shows on screen:

- **Idle interlock.** 30 s without a touch while armed disarms the tester.
  `IDLE ns` counts down in the top right during the last 5 s. The backlight
  dims briefly when it disarms.
- **Opening `MENU`.** Opening it always disarms.
- **Heartbeat backstop.** If the display firmware stops, the second core sets
  the throttle to zero after 250 ms. The screen would also be frozen.

If the motor stops without any of these, check the ESC's own protections (low
voltage cutoff, temperature, desync), and look at `ESC STATUS` and `ESC TEMP`.

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

- **`3 NOT READY`**: nothing answered on the bus. No card, or not seated.
- **`13 NO FILESYSTEM`** with a size under `CARD`: the card answers and the
  filesystem is the problem. Format it as FAT32.
- **Card inserted after boot**: press `RETRY`. The card is mounted only once,
  at boot, and the board has no card-detect pin.

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
Set them again and save with `HOLD TO SAVE`.

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
Include: the firmware version, what the `LINK` row shows, and which ESC and
ESC firmware.
