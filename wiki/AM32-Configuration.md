**English** | [Deutsch](AM32-Konfiguration)

# AM32 Configuration

This screen reads, changes and writes the settings of an AM32 ESC (electronic
speed controller) over the signal wire. It needs no laptop or configurator. It
does not update ESC firmware.

<img src="img/am32-list.png" width="240" alt="The AM32 settings list">

> **This writes to your ESC's flash.** Changes stay on the tester until you
> hold `HOLD TO WRITE` for 1 s. After that they are in the ESC.

---

## Connecting

Open **MENU → AM32 CONFIG**.

The screen hands the signal pin from the DShot driver to the bootloader link
(19200 baud, one wire, half-duplex), then starts searching. Before DShot lets
go, it sends throttle zero for at least 250 ms after the last throttle
command, so an ESC that was running sees a stop before the line goes quiet.

**Then power-cycle the ESC.** The AM32 bootloader listens only in a short
window after power-up. Unplug the ESC battery and plug it back in while this
screen is open. The firmware repeats its handshake, waiting 30 ms for each
reply, so that it is sending when the window opens. The window's length is not
measured.

The screen shows `POWER-CYCLE THE ESC` in amber, the step to take now, and
`SEARCHING...`, then `WAITING... (TRY n)` with an attempt counter, and a
spinner. On success the list appears, and the strip under the
title shows the ESC name, MCU (microcontroller) family and firmware revision.

<img src="img/am32-connect.png" width="240" alt="The connect page: POWER-CYCLE THE ESC, WAITING... (TRY 3) and the spinner">

If bytes arrive that do not parse, the screen shows the byte count, the
first three bytes and the baud rate, then `REPLY NOT UNDERSTOOD:` and
`IS THE ESC RUNNING AM32?`. The wire works; the reply is not the AM32
bootloader's. The ESC may run other firmware, or its bootloader may use a
protocol this transport does not implement.

<img src="img/am32-connect-garbled.png" width="240" alt="A reply that does not parse: the bytes, REPLY NOT UNDERSTOOD: and IS THE ESC RUNNING AM32?">

If the bootloader answers and its settings block fails the plausibility
check (first byte 0x00 or 0xFF, layout revision 0 or above 8, or 0 motor
poles), the screen shows `NO VALID SETTINGS` with
`NOT AN AM32 ESC, OR BLANK SETTINGS.` and `NOTHING WILL BE WRITTEN.` The
screen stays on this page, which has no write button, and keeps reading.

The field table is checked against layout revisions 2 to 4: every
am32-firmware/AM32 release writes one of them (2.22 writes 4), and
AlkaMotors' firmware from 1.78 writes 2. Layout 1, AlkaMotors' firmware
before 1.78, reads nothing above byte 0x29, and a revision above 4 is newer
than the table, so either may hold a field where the table expects another.
For either the screen opens a **read-only view**: the 48 bytes in hex, eight
to a line, and no field. The strip reads, in amber,
`LAYOUT 5 NOT KNOWN: READ-ONLY.` (with the layout the ESC stores) and
`NOT VERIFIED.` followed by where the backup went: `BACKUP AM001`,
`NO CARD: NO BACKUP.` without a card or with the card on USB, or
`BACKUP FAILED.` when the card refuses it. Under the bytes:
`THE BYTES AS READ. THE FIELD TABLE` / `IS NOT CHECKED FOR THIS LAYOUT.`

<img src="img/am32-readonly.png" width="240" alt="The read-only view of a layout 5 block: LAYOUT 5 NOT KNOWN: READ-ONLY. and NOT VERIFIED. BACKUP AM001 in amber, the 48 bytes in hex">

The backup goes to the card as the view opens, since nothing is written
later. It holds the 48 bytes, no field line, and the line
`Layout not known: fields not read, not verified`. The view has one control,
`BACK`: no row, no `HOLD TO WRITE`, `REVERT` or `HEX`, and the screen has no
way from this view to the write. The write itself also refuses settings read
in such a layout: `LAYOUT NOT KNOWN: NOT WRITTEN`. To change the settings of
such an ESC, use the AM32 configurator.

The firmware does not record which firmware an ESC that does not answer
runs.

---

## Browsing

Drag up or down to scroll. The list follows the finger 1:1 once the drag is
recognised. A drag never selects a row.

<img src="img/am32-scrolled.png" width="240" alt="The list mid-scroll">

A **tap without movement** selects the row under it. The editor bar at the
bottom then shows the field's name and value.

Rows are 40 px tall, so three show at a time. Rows are grouped, with the group
name in grey above each group. The available
fields depend on the ESC's settings layout revision. Only fields that apply to
the revision read are shown.

### Fields with a note

Some values change what the tester can do with the ESC, or turn a protection
off. For these the strip under the title shows two amber lines in place of the
ESC name. The selected row's note comes first, then 3D MODE, then PROTOCOL,
then firmware below 1.80. What AM32 does with each value is taken from the AM32
source code: the releases v2.05, v2.16 and v2.18 to v2.22, and the `main`
branch at 077b6a4 (November 2026), with the AM32 configurator at 06955fe, and
AlkaMotors' firmware tags v1.77 to v2.01 and their history for firmware below
2.00. It is not measured on the bench. The firmware version is the one the ESC
stores in its settings, shown on the strip as `FW`; a version byte AM32 did not
write counts as the oldest.

<img src="img/am32-3d.png" width="240" alt="3D MODE on: two amber lines on the strip in place of the ESC name"> <img src="img/am32-protection-off.png" width="240" alt="TEMP LIMIT selected at OFF: PROTECTION OFF: THE ESC DOES NOT CUT POWER ON THIS LIMIT.">

| Field | Values | Note on the strip |
|---|---|---|
| `3D MODE` | `OFF`, `ON`. AM32's `bi_direction` setting: 3D, reversible, mode. It is not bidirectional DShot, the telemetry the tester uses | While on, read or edited: `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| `TEMP LIMIT` | 70 to 140 °C; `OFF` at 141. AM32 takes any value outside 70 to 140 as no limit | Selected and `OFF`: `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| `CURRENT LIMIT` | 2 to 200 A in 2 A steps; `OFF` at 0 and above. AM32 2.21 and later limit the current for stored values 1 to 100; older AM32 for 1 to 99, so 200 A reads `OFF` there | as TEMP LIMIT |
| `LOW VOLT CUTOFF` | `OFF`, `CELL` (per cell, at CELL CUTOFF), `ABSOLUTE` (at ABS VOLT CUT, from 2.21) | as TEMP LIMIT, when `OFF`, and when `? (2)` |
| `PROTOCOL` | `AUTO`, `DSHOT`, `SERVO`, `SERIAL`, `EDT ARM`, stored as 0 to 4; a stored 5 reads `DRONECAN` | At every value but `AUTO`, `DSHOT` and 10 or above: `THE TESTER SENDS DSHOT ONLY:` / `THE ESC WILL NOT RUN HERE.` |

In 3D mode AM32 splits the DShot throttle range: values 48 to 1047 run the
motor one way, 1048 to 2047 the other way, each from low to full power. The
tester sends 47 plus its throttle of 1 to 2000, so 50 % is full power one
way, and just above 50 % the motor runs the other way at low power. With the
default throttle ceiling of 20 % the tester reaches about 40 % of AM32's
throttle range in one direction.

**On the tester.** After AM32 CONFIG has read the ESC's settings with 3D MODE
on, or written them with 3D MODE on (accepted by the ESC, whether the read-back
then shows `WRITE VERIFIED` or not), the tester shows
`3D MODE ESC: THROTTLE SCALE DIFFERS` in amber under the RPM readout, in place
of the pole count and the swipe hint, armed or not. While a swipe on the pad is
engaged, `SWIPE ACTIVE` shows in its place, and the note returns on release.
Arming is not refused on it. Only a verified write or a list read with 3D MODE
off removes it, and so does a restart: it is not stored. An edit that is not
written, a write the ESC refused, a write of 3D MODE off that is not verified, a
failed read and a read that does not open the list, the read-only view of a
layout not known among them, leave it as it was. The
tester cannot tell that the ESC on the pin was swapped for another one: the note
describes the last ESC AM32 CONFIG read or wrote in this power-up. Keep the
propeller off while testing a 3D ESC.

With `SERVO` AM32 looks for servo pulses only. With `SERIAL`, `DRONECAN` and
the stored values 6 to 9 AM32 detects DShot, but its start-up code leaves the
throttle locked (`EDT_ARMED` at 0), so it drops every throttle command; a
value of 10 or above works as `AUTO`. The editor offers 0 to 4, as the
configurator does: `DRONECAN` is for DroneCAN builds, and a stored 5 keeps
its name. `EDT ARM` takes DShot only after the EDT (Extended DShot Telemetry)
enable command, and AM32 locks the throttle again on every zero-throttle
frame. The tester sends the enable only while disarmed and zero frames after
it, before any throttle, so an ESC set to `EDT ARM` drops every throttle
command here.

### Values that depend on the firmware

| Field | Values | On older firmware |
|---|---|---|
| `LOW VOLT CUTOFF` | `OFF`, `CELL`, `ABSOLUTE` | `ABSOLUTE` from 2.21. The AM32 configurator offers it from 2.19, but 2.19 and 2.20 compare the battery voltage in 10 mV with the stored byte, so the cutoff never trips. A stored 2 below 2.21 reads `? (2)` with the PROTECTION OFF note |
| `PWM TYPE` | `FIXED`, `VARIABLE`, `BY RPM` | `BY RPM` from 2.18. 2.16 and older vary the PWM on `VARIABLE` alone |
| `BRAKE ON STOP` | `OFF`, `BRAKE`, `ACTIVE` | `ACTIVE` from 2.19. 2.18 brakes on any value but 0; 2.16 and older on `BRAKE` alone |
| `PWM FREQ` | 8 to 144 kHz | 8 to 48 kHz before 2.19 |
| `ADVANCE` | 0.0 to 30.0 degrees in 0.9375 degree steps; a stored 0 to 3 (the old encoding, 7.5 degree steps) is read too, and a press writes the new encoding | Before 2.19, and on unknown firmware, 0.0 to 22.5 degrees in 7.5 degree steps, stored as 0 to 3: that firmware takes any other byte as 15 degrees, so a stored 10 to 42 reads `? (n)` |
| `ABS VOLT CUT` | 0.5 to 50.0 V in 0.5 V steps | Before 2.21 the stored byte, without a unit: 2.19 and 2.20 do not read it as volts |
| `ACTIVE BRAKE` | 0.0 to 2.5 % of the PWM period in 0.5 % steps | The configurator shows the stored 0 to 5 as percent; AM32 uses 0.5 % a step |

On older firmware the editor stops at the last value that firmware accepts.
A stored value outside the range the tester offers on that firmware reads
`? (n)`, numbers included: on 2.18 a PWM FREQ byte of 100 reads `? (100)`,
since 2.18 ignores it. An on/off field that holds 2 or
more reads `? (n)` as well: the configurator writes 0 and 1 only, and AM32
before 2.18 acts on several of these bytes when they are 1 alone. The tester
writes back every byte you did not change as it read it: a `? (n)` stays
until you press `-` or `+` on that row, which moves it into the range. The
names are the configurator's, shortened to fit: `CELL` is "Cell based",
`BRAKE` is "Brake on stop", `ACTIVE` is "Active brake".

<img src="img/am32-fw222.png" width="240" alt="Firmware 2.22: ABS VOLT CUT reads 10.0V">

### Firmware that does not read every byte

Older firmware reads fewer bytes. A field whose byte the ESC's firmware does
not read is not listed, and its byte is written back as read:

| Bytes | Fields | Listed from |
|---|---|---|
| 0x2A to 0x2D | RUNNING LEVEL, TEMP LIMIT, CURRENT LIMIT, SINE POWER | 1.80 |
| 0x2E | PROTOCOL | 1.94 |
| 0x2F | AUTO ADVANCE | 2.16 |

In AlkaMotors' firmware history, commit 9909ea7 adds the reads of 0x2A to
0x2D and raises the version from 1.79 to 1.80, and commit aa590e8 adds the
read of 0x2E and raises it from 1.93 to 1.94. A release may store a lower
version than its code (the v1.99 tag stores 1.95), never a higher one, so a
stored version at or above the gate reads the byte. The am32-firmware/AM32
releases v2.05 to v2.12 do not read 0x2F and v2.16 does; the untagged
releases between are taken as not reading it. At 1.80 CURRENT LIMIT acts on
2 to 99 (4 to 198 A); from 1.81 (commit f917c01) on 1 to 99.

Below 1.80, and on an ESC whose version byte AM32 did not write, the strip
says `FIRMWARE BELOW 1.80 OR UNKNOWN:` / `FIELDS IT MAY NOT READ ARE HIDDEN.`

<img src="img/am32-oldfw.png" width="240" alt="Firmware 1.35: FIRMWARE BELOW 1.80 OR UNKNOWN: FIELDS IT MAY NOT READ ARE HIDDEN.">

---

## Editing

<img src="img/am32-edit.png" width="240" alt="Editing a field">

| Gesture | Effect |
|---|---|
| Tap a row | Select it |
| `-` / `+` in the editor bar | **Fine**: one step per press. Hold to repeat, speeding up |
| Swipe a row **sideways** | **Coarse**: one coarse step per 16 px of travel; 176 px cover any field's range |
| Drag **up / down** | Scroll |

A gesture is either a scroll or an edit. The axis is fixed after 6 px of
travel.

A coarse step is a whole number of the field's steps. A value on the field's
step stays on it; an even pole count stays even, for example. A value the ESC
stored off the step moves by the step from where it is. A coarse step that
would pass the end of the range stops at the end. Travel past the end does not
count, so the first coarse step back moves the value off it.

**A changed value is cyan, with a cyan bar at its row's left edge.** The
editor bar shows it in cyan too. Nothing has been sent to the ESC yet. A
value as read is in the text colour, selected or not: selecting a row only
lights the row and its name.

`REVERT`, labelled in amber, discards all unsaved changes and restores the
values that were read. `HEX` is labelled in the text colour.

---

## Writing

<img src="img/am32-written.png" width="240" alt="Write verified">

Hold `HOLD TO WRITE` for 1 s. A progress bar fills while you hold. With no
changes the button reads `NO CHANGES`.

While the BOARD voltage on the tester is red, below 3.30 V and until it
rises to 3.35 V, the hold writes nothing, neither the backup nor the
settings. The result reads `BOARD VOLTAGE IS RED: NOT WRITTEN.` with
`CHARGE THE BOARD'S CELL, HOLD AGAIN.` in amber under it, and `OK` returns to
the list with the edits kept. The screen asks once, as the hold ends,
before the backup. Amber, and a value below 2.50 V (no cell to judge),
refuse no write. The bands are a lithium-polymer cell's usual range, not
measured on this board. What the board reads on USB power with no cell is
not measured either, so a board on USB alone may refuse writes.

<img src="img/am32-board-low.png" width="240" alt="BOARD VOLTAGE IS RED: NOT WRITTEN. with CHARGE THE BOARD'S CELL, HOLD AGAIN. in amber">

After the write the firmware waits 300 ms, **reads the settings back and
compares them** byte by byte. It does not rely on the ESC's acknowledgement.
A match shows `WRITE VERIFIED` in green, with `POWER-CYCLE THE ESC TO APPLY.`
under it. A mismatch shows the first differing byte as
`DIFF @xx: GOT xx WANT xx`. A result longer than 18 characters is drawn in
the small font, so it stays on the screen. Under every result that is not a
verified write (`WRITE FAILED: ...`, `VERIFY READ: ...`, `DIFF @..`) the
screen shows `NOT VERIFIED. TAP OK, WRITE AGAIN.` in amber; see
[If the write fails](#if-the-write-fails).

<img src="img/am32-write-failed.png" width="240" alt="WRITE FAILED: REFUSED with NOT VERIFIED. TAP OK, WRITE AGAIN.">

Before the first write of a session, the settings as read at entry go to the
SD card as `BACKUP/AMnnn.TXT`, in the ring of backups that the KST and Hitec
programmers share ([KST Programming](KST-Programming#files)). The result then
reads `WRITE VERIFIED. BACKUP AM018`, for example. A card that refuses the backup
stops the write: `NO BACKUP ON THE CARD: NOT WRITTEN`, with
`CHECK THE CARD, THEN WRITE AGAIN.` under it. Without a card the
write runs, and the backup stays in memory until a write with a card saves it
or the editor closes. The file holds one line per field the firmware reads,
the 48 bytes in hex and a checksum line; nothing reads it back yet. The ESC
is written only once the backup is on the card. A blackbox log that runs
when the editor opens stops.

<img src="img/am32-written-backup.png" width="240" alt="Write verified, with the backup named and the line POWER-CYCLE THE ESC TO APPLY.">

Most AM32 settings take effect at the next ESC power-up, hence the line
under the result. This is AM32 behaviour; the tester does not check it.

### If the write fails

The write sends the settings in three steps: the address, the 48 bytes and
the command to program them. Each step waits up to 250 ms for the ESC's
acknowledgement. Then the read-back runs, up to three times, each after a
300 ms wait. The screen shows one of these results:

| Result | What the code saw |
|---|---|
| `NO BACKUP ON THE CARD: NOT WRITTEN` | The card refused the backup, or its flush failed. Nothing went to the ESC |
| `WRITE FAILED: NO REPLY` | A step got no acknowledgement within 250 ms |
| `WRITE FAILED: REFUSED` | A step got a reply byte that is not the acknowledgement |
| `WRITE FAILED: CRC FAIL` | Not reached by the write: its replies carry no checksum (CRC, cyclic redundancy check) |
| `WRITE FAILED: NOT READY` | The bootloader link holds no pin. Not expected: the list opens only after a read over the pin |
| `VERIFY READ: NO REPLY`, `REFUSED`, `CRC FAIL` or `NOT READY` | The ESC acknowledged the write, and all three read-backs failed: no reply, a reply that is not the acknowledgement, a checksum that does not match, or no pin. What the ESC holds is not known |
| `DIFF @xx: GOT yy WANT zz` | The read-back differs. `xx` is the first differing byte's offset, `yy` the ESC's value, `zz` the value written, all in hex |

Only `WRITE VERIFIED` counts as written. After any other result, `OK` returns
to the list with the edits kept, and `HOLD TO WRITE` writes them again. The
backup is made before the first write that has a card, and again on a card
swapped in since. It always holds the settings as read when the editor
opened, and a repeated hold on the same card makes no second one.

When a result repeats:

1. Check the signal and ground wires.
2. Leave with `BACK`; the edits are lost.
3. Open **MENU → AM32 CONFIG** again and power-cycle the ESC while the
   screen is open, as under [Connecting](#connecting). The screen then
   reads what the ESC holds.
4. Compare it with the backup, the newest `BACKUP/AMnnn.TXT` on the card.
   It holds the settings as read when the editor opened, before any write.
   Its number is in `BACKUP/LAST` while no KST or Hitec backup came after
   it. Without a card the backup was only in memory and is gone.

---

## The hex view

<img src="img/am32-hex.png" width="240" alt="The raw settings bytes">

`HEX` shows the raw 48 settings bytes, read-only. Use it to check a field that
your layout revision does not offer, or to see what a change did at byte
level. `FIELDS` returns to the list.

---

## Leaving

`BACK` hands the signal pin back to the DShot driver and returns to
`MENU`. The tester is disarmed.

The firmware sends the bootloader no command to start the ESC firmware when
the screen closes. What the ESC does after the screen is left is not
recorded. **Power-cycle the ESC after leaving**, before driving a motor.

After a session in which the bootloader answered, MENU shows
`POWER-CYCLE THE ESC TO RUN IT` in amber under the strip's caption. It goes
when an eRPM frame arrives, which only the ESC's firmware sends, or when the
board restarts.

<img src="img/menu-run-cue.png" width="240" alt="MENU with POWER-CYCLE THE ESC TO RUN IT in amber under the strip's caption">

---

## When it will not connect

- **`SEARCHING...` or `WAITING...` with no end.** Unplug and replug the ESC
  battery *while the screen is open*. A power-cycle before opening the screen
  does not work.
- **`REPLY NOT UNDERSTOOD`.** The link works; the framing does not. Possible
  causes: the ESC does not run AM32, or its bootloader uses a protocol this
  transport does not implement.
- **`NO VALID SETTINGS`.** The ESC answered, and the settings block failed the
  plausibility check. Do not write anything.
- **`LAYOUT n NOT KNOWN: READ-ONLY.`** The ESC stores a settings layout the
  tester does not know. The screen shows the bytes, saves the backup and
  writes nothing. Use the AM32 configurator to change this ESC.
- **`READ FAILED (...)`.** The handshake worked and the settings read did not.
  Check the signal wire and ground.

More in [Troubleshooting](Troubleshooting#am32-will-not-connect).
