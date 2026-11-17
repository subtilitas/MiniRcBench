**English** | [Deutsch](AM32-Konfiguration)

# AM32 Configuration

This screen reads, changes and writes the settings of an AM32 ESC (electronic
speed controller), and of a Bluejay or BLHeli_S ESC, over the signal wire.
It needs no laptop or configurator. It does not update ESC firmware.

<img src="img/am32-list.png" width="240" alt="The AM32 settings list">

> **This writes to your ESC's flash.** Changes stay on the tester until you
> tap `WRITE...`, check the page that lists them, and hold its
> `HOLD TO WRITE` for 1 s. After that they are in the ESC.

---

## Connecting

Open **MENU → ESC PROGRAM**, then tap **ESC SETTINGS**.

<img src="img/esc-chooser.png" width="240" alt="ESC PROGRAM: ESC COMMANDS, ESC SETTINGS and USB BRIDGE">

`ESC PROGRAM` is a hub, a list of the three ESC tools. It takes no pin, and
DShot is off while it shows: the signal pin is held low.
`ESC COMMANDS` sends DShot commands: see
[The Screens](The-Screens#esc-commands). `USB BRIDGE` lets a desktop ESC
tool reach the ESC through the board: see [USB Bridge](USB-Bridge). `BACK`
returns to MENU. The screen is titled `ESC SETTINGS` for every ESC
firmware; it reads and writes AM32, Bluejay and BLHeli_S: see
[EFM8 ESCs](#efm8-escs) for the last two.

The screen takes the signal pin, held low until then, for the bootloader
link (19200 baud, one wire, half-duplex), then starts searching. Opened
within 250 ms of leaving ESC TESTER with the motor driven, it first lets the
tester's zero-throttle frames run out, so an ESC that was running sees a stop
before the line goes quiet.

**Then power-cycle the ESC.** Unplug the ESC battery and plug it back in
while this screen is open. By the AM32 bootloader's source, the bootloader
decides about 46 ms after power-up and then waits while the line rests high;
if the handshake's bytes fall into those 46 ms, the ESC firmware starts
instead, and it restarts itself 2 s later with no DShot, so the bootloader
gets another chance, in which only a low in its last 5 ms starts the
firmware. Not measured.

The screen searches in a cycle: for one second it sends nothing and the
line rests high, `LINE HIGH, NOTHING SENT`; then it sends its handshake 3
times, waiting 30 ms for each reply, `WAITING... (TRY n)` with an attempt
counter; then quiet again. The quiet second lets a Bluejay or BLHeli_S ESC
that was running enter its bootloader by itself, and keeps an AM32 that is
powered up meanwhile in its bootloader. Not measured.

<img src="img/am32-quiet.png" width="240" alt="The connect page in its quiet second: LINE HIGH, NOTHING SENT">

The screen shows `POWER-CYCLE THE ESC` in amber, the step to take now, the
search's status and a spinner. On success the list appears, and the strip
under the title shows the ESC name, MCU (microcontroller) family and firmware
with its revision, for example `NEOESC f051  F051  AM32 2.16`.

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

The backup goes to the card as the view opens, since nothing is written later.
It holds the whole 192-byte page, no field line, and the line
`Layout not known: fields not read, not verified`. The view has one control,
`BACK`, which goes to ESC PROGRAM: no row, no `WRITE...`, `REVERT` or `HEX`, and
the screen has no way from this view to the write. The title stays
`ESC SETTINGS`. The write itself also refuses settings read in such a layout:
`LAYOUT NOT KNOWN: NOT WRITTEN`. To change the settings of such an ESC, use the
AM32 configurator.

The firmware does not record which firmware an ESC that does not answer
runs.

### EFM8 ESCs

The handshake is also a valid greeting for the bootloader of Bluejay and
BLHeli_S ESCs, so such an ESC can answer it. The screen tells the answer
apart from an AM32 one: `471` with 0xE8 four bytes later names the EFM8
chip of a Bluejay or BLHeli_S ESC (`E8B1`, `E8B2` or `E8B5`). The tester
then reads that ESC's settings, and on Bluejay 0.21 and BLHeli_S 16.3 to
16.7 lets you change and write them. From the bootloaders' source; not
seen on an ESC.

The page reads `EFM8 E8B2 ANSWERED: READING.` and, in place of
`POWER-CYCLE THE ESC`, `EFM8 BOOTLOADER`: the ESC waits in its bootloader
for commands, and its firmware does not run until it leaves. The tester
holds the signal line high for half a second, greets the bootloader again
and reads its settings page, 112 bytes for BLHeli_S and 255 for Bluejay.
Reading erases and writes nothing. With rcbench's simulated ESC this takes
under one second; not measured on an ESC.

<img src="img/am32-efm8.png" width="240" alt="The connect page while a Bluejay ESC is read: EFM8 BOOTLOADER, EFM8 E8B2 ANSWERED: READING.">

Then the list opens. The strip names the firmware, its version, the
settings layout and the chip, for example `BLUEJAY 0.21  LAYOUT 208  E8B2`.
The names and values are those of rcbench's tables, in capitals;
DIRECTION's bidirectional settings read `3D` and `3D REVERSED`. A value that
is none of the setting's own reads, for example, `RAW 17` in amber. On
Bluejay 0.21 and BLHeli_S 16.3 to 16.7 you edit the settings as on AM32,
below: 15 on Bluejay, 12 on BLHeli_S. Faint rows are shown only: BLHeli_S'
throttle limits, Bluejay's PWM INDICATOR and SAFETY ARM. Under the list:
`TAP A SETTING TO EDIT` / `BACK SENDS LEAVE: THE ESC STARTS.`

<img src="img/efm8-list.png" width="240" alt="A Bluejay 0.21 ESC's settings, the editable rows white, WRITE... and REVERT faint until a change">

Tap a row to select it. Some rows have a note, two amber lines on the
strip:

| Row | Note |
|---|---|
| DIRECTION at `3D` or `3D REVERSED` | `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.` |
| TEMPERATURE or LOW-SPEED PROTECTION at `OFF` | `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| POWER RATING | `SETS HOW THE ESC READS ITS TEMP:` / `A WRONG VALUE MISREADS THE LIMIT.` |

<img src="img/efm8-note.png" width="240" alt="POWER RATING selected: SETS HOW THE ESC READS ITS TEMP: in amber on the strip">

DIRECTION at `3D` or `3D REVERSED` also shows the 3D note on the strip when
no row is selected, and sets the tester's
`3D MODE ESC: 0-50% REVERSED AT 2X`. `HEX` shows the bytes as read, 8 to
a line; drag to scroll the 255 bytes of a Bluejay page.

<img src="img/efm8-3d.png" width="240" alt="A BLHeli_S ESC with DIRECTION at 3D: the 3D note in amber on the strip, the row reading 3D">

<img src="img/efm8-hex.png" width="240" alt="ESC HEX: the Bluejay page's bytes, 8 to a line">

An older version, and a page the firmware rewrote, open read-only: every
row faint, `WRITE...` faint, `SHOWN ONLY: NOTHING IS WRITTEN.` under the
list, and two amber lines on the strip. With a card in, the settings go to
the card as the list opens, as `BACKUP/BJnnn.TXT` for Bluejay or
`BACKUP/BHnnn.TXT` for BLHeli_S:

| Page | Strip |
|---|---|
| Bluejay before 0.21, BLHeli_S before 16.3 | `BLUEJAY 0.19, LAYOUT 206: READ-ONLY.` / `SHOWN ONLY. BACKUP BJ018` |
| The ESC lost its tags, the 48 bytes that name its hardware | `ESC LOST ITS TAGS: READ-ONLY.` / `REFLASH ON A COMPUTER. BACKUP BH018` |
| The settings marker is wrong | `SETTINGS MARKER WRONG: READ-ONLY.` / `SHOWN ONLY. BACKUP BJ018` |

Without a card the second line reads `SHOWN ONLY. NO CARD: NO BACKUP.`
(`REFLASH ON A COMPUTER. NO CARD.` for lost tags), while USB DRIVE holds
the card `SHOWN ONLY. USB DRIVE: NO BACKUP.` (`REFLASH ON A COMPUTER. USB
DRIVE.`), and with a card that refuses the file `... BACKUP FAILED.` The
same page opened again saves no second copy while its backup is the
newest on the card.

<img src="img/efm8-readonly.png" width="240" alt="Bluejay 0.19: BLUEJAY 0.19, LAYOUT 206: READ-ONLY. in amber">

**Writing.** Edit as on AM32: tap a row, then `-` and `+`, or swipe it
sideways. `WRITE...` opens `ESC WRITE`, which lists each change and the
backup it saves first, for example `BACKUP FIRST: BACKUP/BJ001.TXT`. Hold
`HOLD TO WRITE` for 1 s. The tester saves the settings as it first read
them to the card, with the tags that name the ESC's hardware, then writes
the changes, reads the page back and compares it. Only the settings page is
written; the ESC's firmware and bootloader are never erased. A card that
refuses the backup stops the write; without a card the write runs and the
old values are lost when you leave the screen, as the page says in amber.

<img src="img/efm8-edit.png" width="240" alt="BEEP STRENGTH changed from 40 to 88, cyan, in the editor bar"> <img src="img/efm8-write.png" width="240" alt="ESC WRITE: 3 changes and BACKUP FIRST: BACKUP/BJ001.TXT">

<img src="img/efm8-writing.png" width="240" alt="WRITING: DO NOT UNPLUG. in amber, BACK a dim frame, OK faint"> <img src="img/efm8-verified.png" width="240" alt="WRITE VERIFIED. BACKUP BJ001 in green, LEAVING STARTS THE ESC WITH THEM.">

| Result | Meaning and next step |
|---|---|
| `WRITE VERIFIED. BACKUP BJ018` / `LEAVING STARTS THE ESC WITH THEM.` | Read back equal. `BACK` from the list sends LEAVE, and the ESC starts with the new settings |
| `READ FAILED: NOTHING ERASED.` | The read before the erase failed. Tap OK and write again |
| `ESC PAGE CHANGED: NOT WRITTEN.` | The ESC's page differs from what was read. Tap OK, then `READ AGAIN` |
| `WRITE FAILED: PAGE AS READ.` | Three tries did not take; the ESC holds its old settings. Write again |
| `HALF WRITTEN: KEEP THE ESC POWERED.` | The page is neither the old nor the new. Write again at once |
| `NO ANSWER IN THE WRITE: KEEP POWER.` | The ESC stopped answering after its page was erased. Keep it powered, fix the wire, `READ AGAIN`, then write |

**After a cut write keep the ESC powered.** Its page may be erased or half
written. While it is powered the ESC waits in its bootloader, and the
tester still holds the page as it was read, tags included: `READ AGAIN`,
then `WRITE...` writes your changes, or, after `REVERT`, the page as read
(`THE PAGE AS READ, TAGS INCLUDED.`). A power cycle before that makes the
ESC store its defaults and lose its tags for good: the next session reads
`ESC LOST ITS TAGS: READ-ONLY.`, and only a firmware flash for that ESC on a
computer brings them back. The backup file on the card holds them. While a
page is half written, `BACK` warns first,
`HALF WRITTEN. BACK AGAIN LEAVES IT;` / `A POWER CYCLE THEN LOSES ITS TAGS.`;
a second `BACK` opens the leave page, and no LEAVE is sent.

That way back holds only while the session goes on. If it ended with the
page half written, by a failed line driver, a heartbeat stop or the second
`BACK`, the result reads `SESSION ENDED: RESTORE ON A COMPUTER.`: the next
session reads the page as it is, most likely empty or with lost tags, which
it does not write, or as a torn page that only looks whole, and the tester
cannot restore the old one. Keep the ESC powered and restore it on a
computer. The old page, as the session first read it, is in the backup on
the card, `ALSO ON THE CARD: BACKUP BJ018`; without a card the backup line
reads `NO BACKUP: OLD PAGE ON THIS SCREEN.` Until you leave ESC SETTINGS,
`OLD PAGE` on the connect page shows its bytes, and so does ESC HEX of a
later list, under the page's own: copy them by hand before you leave.

<img src="img/efm8-ended.png" width="240" alt="NO ANSWER IN THE WRITE: KEEP POWER. SESSION ENDED: RESTORE ON A COMPUTER. NO BACKUP: OLD PAGE ON THIS SCREEN."> <img src="img/efm8-oldpage.png" width="240" alt="ESC HEX of the old page in amber: OLD PAGE, BEFORE THE CUT WRITE: ON THIS SCREEN ONLY: COPY IT.">

<img src="img/efm8-lost.png" width="240" alt="NO ANSWER IN THE WRITE: KEEP POWER. TAP OK, THEN READ AGAIN."> <img src="img/efm8-guard.png" width="240" alt="HALF WRITTEN. BACK AGAIN LEAVES IT; A POWER CYCLE THEN LOSES ITS TAGS. on the strip, READ AGAIN">

<img src="img/efm8-asread.png" width="240" alt="ESC WRITE with 0 CHANGES: THE PAGE AS READ, TAGS INCLUDED. and HOLD TO WRITE">

None of this is verified on an ESC yet: the write has run against
rcbench's simulated ESC only.

A page the tester does not take stays on the connect page, with what it
is: `EFM8 E8B2: SETTINGS PAGE EMPTY` with `PAGE EMPTY, AS AFTER A CUT
WRITE.` / `KEEP THE ESC POWERED: SEE MANUAL.`, or `EFM8 E8B2: PAGE NOT
KNOWN`, `BLUEJAY ON EFM8BB1: NOT TAKEN` and the like with `NOT BLUEJAY OR
BLHELI_S AS KNOWN:` / `NOTHING IS WRITTEN.` An empty page has lost its
tags already; keeping the ESC powered keeps it in its bootloader.

<img src="img/efm8-empty.png" width="240" alt="An empty settings page: EFM8 E8B2: SETTINGS PAGE EMPTY and its two lines"> <img src="img/efm8-refused.png" width="240" alt="A page not known: EFM8 E8B5: PAGE NOT KNOWN, NOT BLUEJAY OR BLHELI_S AS KNOWN: and NOTHING IS WRITTEN.">

A reply the tester does not understand that holds a piece of such an
answer, for example the answer with one byte lost, gets the same leave page
from `BACK`: that ESC's bootloader may wait too. Its strip reads
`EFM8  SIG GP0`. A reply with no such piece does not.

**Leaving the list.** `BACK` from the list sends the bootloader LEAVE,
which starts the ESC's firmware: the page reads `LEAVING` and
`KEEP THE PROPELLER OFF.` in amber. About a third of a second later the
signal pin is held low and the screen returns to ESC PROGRAM; DShot runs only
once ESC TESTER or ESC COMMANDS opens. LEAVE has no answer: whether the ESC
started is not seen, and not verified on an ESC. MENU's
`POWER-CYCLE THE ESC TO RUN IT` stays until `START DSHOT` on the start step of
ESC TESTER or ESC COMMANDS ([The Screens](The-Screens)), the leave page's last
button after its power-cycle steps, or a restart of the board.

<img src="img/efm8-leaving.png" width="240" alt="LEAVING: THE ESC STARTS ITS FIRMWARE ON LEAVE. ESC TESTER THEN RUNS IT. KEEP THE PROPELLER OFF.">

If the ESC stops answering before its page is read, the page shows
`THE ESC ANSWERED, THEN STOPPED.` / `SEARCHING AGAIN.` and greets again.

<img src="img/efm8-stopped.png" width="240" alt="THE ESC ANSWERED, THEN STOPPED. and SEARCHING AGAIN. under the connect page's quiet status">

Everywhere else after such an answer, on the connect page while reading,
after a page that is not taken, or after the search started again, the
bootloader may still wait for commands, and DShot frames could pass as one.
So `BACK` opens a leave page instead of returning to ESC PROGRAM at once:

1. `UNPLUG THE ESC BATTERY, THEN LEAVE.` / `ITS BOOTLOADER WAITS FOR
   COMMANDS` / `AND MAY READ DSHOT AS ONE.`
2. `PLUG IT IN ONCE ESC PROGRAM SHOWS.`

<img src="img/am32-leave.png" width="240" alt="The leave page: step 1 UNPLUG THE ESC BATTERY, THEN LEAVE., STAY and LEAVE">

`STAY` returns to the screen. While the leave page is open the tester's
greeting and its first look at the page run on, and the read of the whole
page waits for `STAY`. Unplug the ESC battery, then tap `LEAVE`.
`ESC PROGRAM`, the hub's title, takes LEAVE's place and acts 1 s later; it
holds the signal pin low, starts no DShot and returns to ESC PROGRAM. Plug the
battery back in once ESC PROGRAM shows: on a low line a Bluejay or BLHeli_S
ESC starts its firmware rather than its bootloader (read from the firmwares'
source, not measured). On this page the header's top-right button reads `LEAVE` before the release and acts as `LEAVE`; while the claim releases it does not act; after the release it reads `BACK` and, 1 s later, returns to the hub as the wide button does. A button that does not act is a dim frame with a dim label. The last button clears
MENU's `POWER-CYCLE THE ESC TO RUN IT`, since the steps power-cycled the
ESC. The tester cannot see the battery: an ESC left powered whose bootloader
still waits meets DShot at the next ESC TESTER or ESC COMMANDS, and so does
one that waits across a restart of the tester.

---

## Browsing

Drag up or down to scroll. The list follows the finger 1:1 once the drag is
recognised. A drag never selects a row.

<img src="img/am32-scrolled.png" width="240" alt="The list mid-scroll">

A **tap without movement** selects the row under it. The editor bar at the
bottom then shows the field's name and value.

Rows are 40 px tall, so three show at a time. Rows are grouped, with the group
name in faint above each group. The available
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
stores in its settings, shown on the strip after `AM32`; a version byte AM32 did
not write counts as the oldest.

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

**On the tester.** After ESC SETTINGS has read the ESC's settings with 3D MODE
on, or written them with 3D MODE on (accepted by the ESC, whether the read-back
then shows `WRITE VERIFIED` or not), the tester shows
`3D MODE ESC: 0-50% REVERSED AT 2X` in amber under the RPM readout, in place
of the pole count and the swipe hint, armed or not. While a swipe on the pad is
engaged, `SWIPE ACTIVE` shows in its place, and the note returns on release.
Arming is not refused on it. Only a verified write or a list read with 3D MODE
off removes it, and so does a restart: it is not stored. An edit that is not
written, a write the ESC refused, a write of 3D MODE off that is not verified, a
failed read and a read that does not open the list, the read-only view of a
layout not known among them, leave it as it was. The
tester cannot tell that the ESC on the pin was swapped for another one: the note
describes the last ESC ESC SETTINGS read or wrote in this power-up. `3D ON` on
ESC COMMANDS sets the same note, and its `3D OFF` leaves it
([The Screens](The-Screens#esc-commands)). Keep the
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

**A changed value is dark on a cyan fill in its row, with a cyan bar at the
row's left edge.** The editor bar shows it as cyan text, and above it the
value as read: for example
`PWM FREQ  24KHZ ->` over `25KHZ`. Where the name and the old value do not
fit between `-` and `+`, the bar shows `24KHZ ->` alone. Nothing has been
sent to the ESC yet. A value as read is in the text colour, selected or not:
selecting a row only lights the row and its name.

`REVERT`, labelled in amber, discards all unsaved changes and restores the
values that were read. `HEX` is labelled in the text colour.

**BACK with changes not written asks first.** The first `BACK` keeps the list
and puts two amber lines on the strip: `2 CHANGES NOT WRITTEN.` and
`BACK AGAIN DISCARDS THEM.` (`1 CHANGE NOT WRITTEN.` and
`BACK AGAIN DISCARDS IT.` for one). A second `BACK` returns to ESC PROGRAM and
drops the changes; nothing goes to the ESC. Any other touch takes the lines
away and keeps the changes. There is no timeout. Without changes, `BACK`
returns to ESC PROGRAM at once.

<img src="img/am32-discard.png" width="240" alt="The strip after BACK with two changes: 2 CHANGES NOT WRITTEN. and BACK AGAIN DISCARDS THEM. in amber">

---

## Writing

Tap `WRITE...` under the list. It opens the page `ESC WRITE`; nothing is
written yet. With no changes `WRITE...` is drawn disabled and does nothing.

<img src="img/am32-write.png" width="240" alt="ESC WRITE with five changes: four lines of name and old -> new, AND 1 MORE, BACKUP FIRST: BACKUP/AM001.TXT and HOLD TO WRITE">

The page lists every setting that changes, its name and `old -> new`, the old
value faint and the new one cyan: four lines, then `AND n MORE`. The strip
counts them, `5 CHANGES`. The line above the button says where the backup
goes: `BACKUP FIRST: BACKUP/AM001.TXT` before the first write with a card in,
`BACKUP: BACKUP/AM001.TXT` once it is on the card. Without a card it reads
`NO CARD: OLD VALUES LOST ON LEAVING` in amber, and with the card on USB
`USB DRIVE: OLD VALUES LOST ON LEAVING`.

Hold `HOLD TO WRITE` for 1 s. A progress bar fills while you hold. The tap
that opened the page does not count towards the hold. While the write runs
the page reads `WRITING: DO NOT UNPLUG.` in amber and `BACK` does not act and is a dim frame with a dim label. `BACK` before the hold, or under the result, returns to the list.

<img src="img/am32-written.png" width="240" alt="Write verified">

While the BOARD voltage on the tester is red, below 3.30 V and until it rises to
3.35 V, the hold writes nothing, neither the backup nor the settings. The result
reads `BOARD VOLTAGE IS RED: NOT WRITTEN.` with `CHARGE THE CELL. TAP OK, WRITE
AGAIN.` in amber under it, and `OK` returns to the list with the edits kept;
`WRITE...` and the page's hold write again. The screen asks once, as the hold
ends, before the backup. Amber, and a value below 2.50 V (no cell to judge),
refuse no write. The bands are a lithium-polymer cell's usual range, not
measured on this board. What the board reads on USB power with no cell is not
measured either, so a board on USB alone may refuse writes.

<img src="img/am32-board-low.png" width="240" alt="BOARD VOLTAGE IS RED: NOT WRITTEN. with CHARGE THE CELL. TAP OK, WRITE AGAIN. in amber">

After the write the firmware waits 300 ms, **reads the settings back and
compares them** byte by byte. It does not rely on the ESC's acknowledgement.
A match shows `WRITE VERIFIED` in green, with `POWER-CYCLE THE ESC TO APPLY.`
under it. A mismatch shows the first differing byte as
`DIFF @xx: GOT xx WANT xx`. The result is one line in the small font under
the list of changes. A result that is not a verified write is amber, and
under it (`WRITE FAILED: ...`, `VERIFY READ: ...`, `DIFF @..`) the screen
shows `NOT VERIFIED. TAP OK, WRITE AGAIN.` in amber; see
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
the 192 bytes of the settings page in hex (the file's format is 2: the
line `bytes 192` and 12 hex lines) and a checksum line; nothing reads it back
yet. The startup melody and the CAN (Controller Area Network) block are in it. The ESC
is written only once the backup is on the card. A blackbox log that runs
when the editor opens stops.

<img src="img/am32-written-backup.png" width="240" alt="Write verified, with the backup named and the line POWER-CYCLE THE ESC TO APPLY.">

Most AM32 settings take effect at the next ESC power-up, hence the line
under the result. This is AM32 behaviour; the tester does not check it.

### If the write fails

The write sends the whole settings page, 192 bytes (0x00 to 0xBF), because the
AM32 bootloader erases the page before it programs a write that starts at the
page's start. The first 48 bytes are the settings the list, the `HEX` view
and the edits cover. The startup melody (0x30 to 0xAF) and the CAN block (0xB0
to 0xBF) go back as they were read. The write sends three steps: the address,
the 192 bytes and the command to program them. Each step waits up to 250 ms for the ESC's
acknowledgement. Then the read-back runs, up to three times, each after a
300 ms wait. The screen shows one of these results:

| Result | What the code saw |
|---|---|
| `NO BACKUP ON THE CARD: NOT WRITTEN` | The card refused the backup, or its flush failed. Nothing went to the ESC |
| `ESC CHANGED: NOT WRITTEN`, with `LEAVE AND READ IT AGAIN.` under it | The page read again just before the backup and the write differs from the page read at entry, or that read failed: another ESC on the wire. Byte 2, the bootloader version, is not compared. After a write that did not verify, a byte that holds what was sent, or 0xFF where the bootloader erased, is accepted, so RETRY goes. Nothing went to the ESC |
| `WRITE FAILED: NO REPLY` | A step got no acknowledgement within 250 ms |
| `WRITE FAILED: REFUSED` | A step got a reply byte that is not the acknowledgement |
| `WRITE FAILED: CRC FAIL` | Not reached by the write: its replies carry no checksum (CRC, cyclic redundancy check) |
| `WRITE FAILED: NOT READY` | The bootloader link holds no pin. Not expected: the list opens only after a read over the pin |
| `VERIFY READ: NO REPLY`, `REFUSED`, `CRC FAIL` or `NOT READY` | The ESC acknowledged the write, and all three read-backs failed: no reply, a reply that is not the acknowledgement, a checksum that does not match, or no pin. What the ESC holds is not known |
| `DIFF @xx: GOT yy WANT zz` | The read-back differs. `xx` is the first differing byte's offset, `yy` the ESC's value, `zz` the value written, all in hex |

`OK` takes the place of `HOLD TO WRITE` in the wide button at the bottom. It
takes a fresh tap: lifting the finger that held `HOLD TO WRITE` leaves the
result on the screen.

Only `WRITE VERIFIED` counts as written. After any other result, `OK` returns
to the list with the edits kept, and `WRITE...` and `HOLD TO WRITE` write
them again. The
backup is made before the first write that has a card, and again on a card
swapped in since. It always holds the settings as read when the editor
opened, and a repeated hold on the same card makes no second one.

When a result repeats:

1. Check the signal and ground wires.
2. Leave with `BACK`, twice: the first names the edits, the second drops
   them.
3. Open **MENU → ESC PROGRAM → ESC SETTINGS** again and power-cycle the ESC
   while the screen is open, as under [Connecting](#connecting). The screen
   then reads what the ESC holds.
4. Compare it with the backup, the newest `BACKUP/AMnnn.TXT` on the card.
   It holds the settings as read when the editor opened, before any write.
   Its number is in `BACKUP/LAST` while no KST or Hitec backup came after
   it. Without a card the backup was only in memory and is gone.

---

## The hex view

<img src="img/am32-hex.png" width="240" alt="The raw settings bytes">

`HEX` shows the raw 48 settings bytes, read-only, under the title
`ESC HEX`. Use it to check a field that your layout revision does not offer,
or to see what a change did at byte level. `FIELDS` or `BACK` returns to the
list.

---

## Leaving

`BACK` on the list holds the signal pin low again, with no DShot, and returns
to ESC PROGRAM, the hub it was opened from; with changes not written it
asks first, as under
[Editing](#editing). The tester is disarmed.

The firmware sends an AM32 bootloader no command to start the ESC firmware
when the screen closes. What the ESC does after the screen is left is not
recorded. **Power-cycle the ESC after leaving**, before driving a motor.
After a Bluejay or BLHeli_S answer, `BACK` from its list sends LEAVE, and
`BACK` from anywhere else opens the leave page instead
([EFM8 ESCs](#efm8-escs)).

After a session in which the bootloader answered, MENU shows
`POWER-CYCLE THE ESC TO RUN IT` in amber under the strip's caption. While it
stands, ESC TESTER and ESC COMMANDS open on a start step with DShot off. It
goes on `START DSHOT` there, on the leave page's last button, when an eRPM
frame arrives, which only the ESC's firmware sends, or when the board
restarts. LEAVE from a Bluejay
or BLHeli_S list leaves it, as LEAVE has no answer.

<img src="img/menu-run-cue.png" width="240" alt="MENU with POWER-CYCLE THE ESC TO RUN IT in amber under the strip's caption">

---

## When it will not connect

- **`LINE HIGH, NOTHING SENT` and `WAITING...` with no end.** Unplug and
  replug the ESC battery *while the screen is open*. Whether an ESC powered
  before the screen opened is reached without a power cycle is not
  measured.
- **`REPLY NOT UNDERSTOOD`.** The link works; the framing does not. Possible
  causes: the ESC does not run AM32, or its bootloader uses a protocol this
  transport does not implement.
- **`EFM8 E8B2 ANSWERED: READING.` that stays.** A Bluejay or BLHeli_S ESC
  answered and its page is being read; it takes about one second. See
  [EFM8 ESCs](#efm8-escs).
- **`THE ESC ANSWERED, THEN STOPPED.`** The ESC answered the handshake and
  then not the tester's own greeting or read. The search runs again. Check
  the signal wire and keep the ESC powered.
- **`NO VALID SETTINGS`.** The ESC answered, and the settings block failed the
  plausibility check. Do not write anything.
- **`LAYOUT n NOT KNOWN: READ-ONLY.`** The ESC stores a settings layout the
  tester does not know. The screen shows the bytes, saves the backup and
  writes nothing. Use the AM32 configurator to change this ESC.
- **`READ FAILED (...)`.** The handshake worked and the settings read did not.
  Check the signal wire and ground.

More in [Troubleshooting](Troubleshooting#am32-will-not-connect).
