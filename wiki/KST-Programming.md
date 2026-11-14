**English** | [Deutsch](KST-Programmierung)

# KST Programming

This screen reads and writes the settings of a KST or Chaservo servo over its
signal wire. Both brands share one programming protocol and one set of 32
registers. The protocol is Manchester-coded, half duplex, and runs on the
RP2350's PIO (programmable I/O). The screen shows the registers as 27 named
fields with their units, steps the ones that may change, and writes them back
with a backup on the SD (Secure Digital) card first.

<img src="img/kst-list.png" width="240" alt="The KST field list">

> **Not tested on hardware.** No frame of the line driver has been on a pin.
> The screens in this wiki are rendered from the UI (user interface) code
> against a simulated servo.

> **Unplug the servo before DShot runs.** Outside this screen the ESC
> (electronic speed controller) pin carries DShot, the digital throttle
> protocol. A servo on that pin reads it as a pulse of about 970 µs at 1000 Hz
> and drives towards one end of its travel. That happens when the board
> **boots** with a servo on the ESC pin, and when you leave this screen. The
> screen asks you to unplug the servo first.

With **SETTINGS → SERVO SUPPLY** at `OWN`, the default, you power the servo
up and down by hand, with a plug or a switch. With `GP28` or `GP29` the bench
switches the servo's supply through a DC-DC (direct current to direct
current) converter: see [Switched supply](#switched-supply).

---

## Wiring

The servo needs its own supply. The board supplies the signal and ground only.
The servo answers only in the first moments after its supply comes on, so
with SERVO SUPPLY at `OWN` the supply has to be switchable by hand: a plug,
or a switch in its positive lead. With `GP28` or `GP29` the bench switches it:
see [Switched supply](#switched-supply).

```
  servo supply, 5 V
     +  ----/ switch /--------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo signal (orange or white)

  SIG GP0 = J4 pin 9 or J5 pin 2, GND = J4 pin 1 or 5, or J5 pin 4
```

> **Never connect a servo lead straight across to J5.** J5 pin 3 is the
> board's 3.3 V supply, in the place of a servo plug's plus: see
> [Setup](Setup#the-adapter-cable).

- The owner programs every KST servo, HV (high-voltage) servos included, at
  5 V. Other voltages are not tested.
- The servo's middle wire goes to its supply, never to a pin of the board.
  The board's 5 V pin is its USB (Universal Serial Bus) supply, and a servo
  can draw more than 1 A.
- Switch the board on before the servo supply, and the servo supply off
  before the board.
- Join the two grounds once, at the board.
- SIG is the ESC pin, as set under **MENU → SETTINGS → SIG PIN**, with its
  J5 pin under the value. GP0 is the default.
- Unplug any ESC from the ESC pin. The screen waits while an ESC answers on
  it, but it cannot see every ESC.

---

## Opening it

Open **MENU → KST SERVO**.

The screen takes SIG from the ESC driver. While it does, the strip reads
`RELEASING GP0`, then `TAKING GP0`. While an ESC answers on the pin it
reads `CHECKING GP0 FOR AN ESC` instead.

- If an ESC answers on the pin, the screen says `AN ESC ANSWERS ON SIG GP0.`,
  then `UNPLUG IT: KST FRAMES MAY READ` and `AS THROTTLE.` It takes the pin
  when the ESC is gone.
- While the ESC is armed, the screen says `DISARM FIRST`. While the pins are
  still in use, it says `PINS BUSY`.
- If the board cannot set up the line, the screen says
  `GP0 COULD NOT BE TAKEN (` followed by `REFUSED`, `PWM`, `PIO`, `DMA` or
  `CLOCK` and a closing bracket. PWM is pulse-width modulation and DMA is
  direct memory access. `RETRY` tries again.

When the pin is taken, the search starts at once. The screen sends no servo
pulses.

> A powered servo on SIG gets no pulses while the screen is open. It may go
> to its failsafe position.

---

## Searching

<img src="img/kst-search.png" width="240" alt="The search: POWER UP THE SERVO NOW">

The screen repeats the entry sequence of rcbench's KST code, one attempt every
116.6 ms to 118.8 ms, for 30 s. The strip counts the attempts and the time:
`SIG GP0  TRY 12  1.4 S`. A spinner turns while the search runs.

The body reads `POWER UP` and `THE SERVO NOW` in amber, the step to take now,
then
`PLUG IT IN OR SWITCH ITS SUPPLY ON.` and
`IT ANSWERS ONLY RIGHT AFTER POWER-ON.`

**Power the servo up while the screen searches.** The servo enters programming
mode and answers. If edges arrive on the line but no reply is valid, an amber
line names the fault, for example `REPLY NOT VALID: LATE`. The words
are `EARLY`, `LINE HIGH`, `LATE`, `CELL`, `LENGTH`, `RATE`, `FLAGS`,
`LONG LAST EDGE`, `WINDOW`, `OVERFLOW` and `CAPTURE`.

| Strip | What happened | Button |
|---|---|---|
| `SIG GP0  TRY 12  1.4 S` | The search runs | `STOP SEARCH` |
| `NO SERVO IN 30 S` | No servo answered in 30 s. The body reads `CHECK SIGNAL AND GROUND, THEN POWER THE SERVO UP AGAIN.` | `RETRY` |
| `SEARCH STOPPED` | `STOP SEARCH` ended it, or the screen stalled and the search stopped itself | `RETRY` |
| `SEARCH FAILED` | The session ended with a result, which the body names in amber | `RETRY` |

After a servo answers, the strip reads `IN PROGRAMMING MODE` and the body
reads `READING 32 REGISTERS`. The screen reads each register until 3 of 5
reads agree. If a register does not settle, the body reads
`READ FAILED: NO 3 EQUAL READS` and lists the registers, for example
`REGISTERS 03 0A`. The line names at most 9 registers; a second line counts
the rest, for example `AND 3 MORE`. `READ AGAIN` repeats the read. A read
stopped by `BACK` reads `ABORTED. READ THE SERVO AGAIN.` A read the screen
could not start because it stalled reads
`THE SCREEN STALLED. READ THE SERVO AGAIN.`

<img src="img/kst-readfail.png" width="240" alt="READ FAILED: NO 3 EQUAL READS, register 05">

The record `KST/KST.TXT` stays on the card when no write of the session
verifies and the session does not end with LEAVE from a servo in programming
mode that is not in doubt, for example after a power loss during the write.
A session removes only the record it wrote, and only while its card stays
mounted and the file holds the bytes it wrote. The board has no card-detect
line: a card swapped in during a session whose record holds the same bytes
counts as the session's. After such a swap, the strip, the backup line and
the `(BACKUP)` tag still name the session's backup file, which is on the
first card. With a record on the card, the screen adds
`A WRITE WAS INTERRUPTED. KST/KST.TXT` and `HOLDS THE SERVO AS IT WAS.` in
amber. See [FILES](#files).

If the servo stops answering later, the screen returns here and searches
again, from any view but the leave page and the unplug page. It notices
only when a read or a write runs: a servo powered off under an idle field
list leaves the list up until the next read or write. `BACK` during that
search stops it and opens the leave page.

---

## The field list

A finished read opens the field list.

The strip shows the servo type, `V6` or `V8`, the pin, and the backup:
`BACKUP IN RAM` until a write has put the backup on the card, then
`BACKUP KST001`. A square chip at the right reads `PROG MODE` on blue while
the servo is in programming mode, or `IN DOUBT` on red after a write left the
servo in an unknown state.

Each row shows the group in small letters, the field name, and the value.
Rows are 40 px tall at a 42 px pitch. Drag the list up or down to scroll.

| Group | Fields |
|---|---|
| TUNE | `MAX. DUTY`, `BOOST`, `STRETCH`, `SOFT START`, `SPEED LIMIT`, `SPEED`, `DEAD BAND` |
| PULSE | `PULSE LOWER`, `PULSE UPPER`, `NEUTRAL`, `REVERSED`, `LEFT ANGLE`, `RIGHT ANGLE` |
| FAILSAFE | `FAILSAFE MOVE`, `FAILSAFE POSITION`, `FAILSAFE TIME` |
| LOCKED | `20K SEL`, `LOCK`, `PROT1 CANCLE`, `PROT1 EN`, `PROT1 TIME`, `PROT1 PWM`, `PROT POT ERR`, `POT SAM TIMES`, `PROT PWM ERR`, `NODE ADDR`, `SERVO / GYRO` |

- Values carry their unit: `96 %`, `300.0 US`, `1.0 S`, `X1`, `ON`, `OFF`, or
  a count in the form `n OF m`. `US` is µs: the screen font has no micro
  sign. A raw value whose display is not established reads `RAW` and the number.
- LOCKED fields are read-only. They are drawn grey and a tap on them selects
  nothing.
- A changed value is cyan, with a 3 px cyan bar at its row's left edge. A
  value as read is in the text colour, selected or not: selecting a row
  lights the row and its name, not the value.

<img src="img/kst-edit.png" width="240" alt="DEAD BAND selected and stepped from 2.0 US to 2.2 US">

### Editing a field

1. Tap an editable row. The editor bar under the list shows the field, its
   stored value, and the pending value, for example `DEAD BAND  2.0 US ->`
   over `2.2 US`, the pending value cyan once it differs.
   With no row selected, the bar reads `TAP A ROW TO EDIT IT` and
   `SWIPE IT SIDEWAYS FOR COARSE`.
2. Tap `-` or `+` (44 × 44 px) to step the value. Hold to repeat. `+`
   raises the value shown and `-` lowers it, whichever way the raw value
   goes: `+` widens `DEAD BAND`.
3. Or swipe the row sideways. One step is an eleventh of the field's range,
   rounded up, at least one raw step.
4. `REVERT`, labelled in amber, drops all pending changes.

One raw step does not always change the text: `MAX. DUTY` reads `96 %` at
raw 245 and at raw 244. The editor bar and the write page then add the raw
values: `96 % (245) -> 96 % (244)`.

Two kinds of limit apply:

- **A hard limit** is never offered. The stepper skips values that break
  one, so `-` and `+` stop at the last legal value. For example, `MAX. DUTY`
  stops at 98 %. When no further step is legal, the button is drawn disabled.
- **A soft rule** is taken. The row's bar turns amber, the value stays cyan,
  and the editor bar shows the rule's text in amber, for example
  `WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.`

Under the editor bar are three buttons, 46 px tall: `WRITE...`
(110 px), `REVERT` (52 px) and `FILES` (54 px). `WRITE...` acts only when
something is pending, no hard limit is broken, and the servo is not in doubt.

---

## Writing

**Tap `WRITE...`.** The write page opens and writes nothing. Holding
`WRITE...` does nothing more than a tap: the page opens when the finger
lifts.

<img src="img/kst-write.png" width="240" alt="The write page, 1 CHANGE"> <img src="img/kst-write-many.png" width="240" alt="The write page, 5 CHANGES, AND 1 MORE">

The write page is titled `KST WRITE`. The strip counts the changes,
`1 CHANGE` or `2 CHANGES`. Each changed field is one line, `DEAD BAND` and
`2.0 US -> 2.2 US`, the old value and the arrow dim, the new value cyan, up
to four, then `AND 3 MORE`. Amber lines starting with
`*` give the soft rules that apply. The line at the bottom says where the
backup goes:

| Line | When |
|---|---|
| `BACKUP FIRST: BACKUP/KST001.TXT` | A card is mounted and the backup is not on it yet |
| `BACKUP: BACKUP/KST001.TXT` | The backup is on the card |
| `NO CARD: OLD VALUES LOST AT LEAVE` | No card is mounted |
| `CARD ON USB: OLD VALUES LOST AT LEAVE` | A USB host holds the card |
| `BACKUP FIRST: TO THE CARD` | A card is mounted and its `BACKUP` directory did not read to its end |
| `NO BACKUP ON THE CARD: LOST AT LEAVE` | The card took the record and refused the backup |

<img src="img/kst-write-nocard.png" width="240" alt="The write page without a card: NO CARD: OLD VALUES LOST AT LEAVE in amber">

`NO CARD`, `CARD ON USB` and `NO BACKUP ON THE CARD` are amber: the servo's
values from before the first write are in memory only, until a later write
with a card mounted saves them; LEAVE, REBOOT or a power loss loses them.
Without a card the write still goes through.

**Hold the page's `HOLD TO WRITE` for 1 s to write.** A bar fills while you
hold; the tap that opened the page does not count. Before the first frame of
the session's
first write with a card mounted, the image read at entry goes to the card as
the backup `BACKUP/KSTnnn.TXT`, and the servo as it stands goes to the card as the record
`KST/KST.TXT`. Later writes of the session save no backup and write no
record. The write then runs as single-register writes with a read-back,
followed by a full verify. A verified write removes the record, and so does
LEAVE from a servo in programming mode that is not in doubt.

With a card mounted when the screen opened, the backup and the record first
go to the card while the screen keeps drawing. The page reads
`SAVING THE BACKUP TO THE CARD.` and `BLOCKS LEFT: 12`, for example, and the
servo is written only once the card holds both. A flush the card does not
take stops the write:
`THE CARD DID NOT TAKE THE BACKUP (REMOVED OR SWAPPED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`
A flush fails when a block write fails or the card in the slot changed, not
when the card is full: a full card ends in `THE CARD TOOK NO FILE` before
anything is queued. Nothing went to the servo. The card is mounted again, and
`OK` returns to the field list; the next write, `WRITE...` and its hold,
saves the backup again and then writes.

<img src="img/kst-flush-failed.png" width="240" alt="The write page after a flush the card did not take">

While the write runs, the page reads `WRITING: DO NOT UNPLUG.` and, under it,
`PLANNING THE ORDER OF THE WRITES` or `FRAMES SENT: 94`. While the write runs,
and while it waits for the card, `BACK` is grey and does nothing: no `BACK`
leaves a servo half written. `BACK` before the hold, or under the result,
returns to the field list.

### The BOARD voltage red

While the BOARD voltage on the tester is red, below 3.30 V and until it rises to
3.35 V, the page's hold writes nothing: no file goes to the card and no frame to
the servo. The page reads, in amber, `BOARD VOLTAGE IS RED: NOT WRITTEN.` and
`CHARGE THE BOARD'S CELL, HOLD AGAIN.`, and `HOLD TO WRITE` stays. The backup
and the record are queued right after that check; the screen asks again just
before its first frame goes out, after the card flush. A voltage that turns red
during the flush stops the write too: the backup and the write's record may be
on the card by then, and no frame goes out. A write already running is not
stopped: cut off, it would leave the servo partly written and unverified, while
one that finishes ends with its read-back. Amber, and a value below 2.50 V (no
cell to judge), refuse no write. The restore page's `HOLD TO RESTORE` is refused
the same way. The bands are a lithium-polymer cell's usual range, not measured
on this board. What the board reads on USB power with no cell is not measured
either, so a board on USB alone may refuse writes.

<img src="img/kst-board-low.png" width="240" alt="The write page with the BOARD voltage red: BOARD VOLTAGE IS RED: NOT WRITTEN. and CHARGE THE BOARD'S CELL, HOLD AGAIN. in amber">

### The result

The result replaces the buttons with `OK`. `OK`, or `BACK`, returns to the
field list.

<img src="img/kst-result.png" width="240" alt="WRITE VERIFIED">

| Result | Colour | Meaning |
|---|---|---|
| `WRITE VERIFIED` | green | The servo holds the set values |
| `... NOTHING WAS WRITTEN.` | amber | No register changed |
| `ROLLED BACK: THE SERVO HOLDS ITS OLD VALUES` | amber | A write did not take and was undone |
| `IN DOUBT: ... RESTORE FROM FILES.` | red | The servo state is unknown; only a restore is taken |

The amber results that end in `NOTHING WAS WRITTEN.` include:

- `THE SERVO DIFFERS FROM THE READ. NOTHING WAS WRITTEN.`
- `A WRITE DID NOT TAKE. NOTHING WAS WRITTEN.`
- `THE VALUES BREAK A LIMIT. NOTHING WAS WRITTEN.`
- `NO SAFE ORDER OF WRITES LEADS THERE. NOTHING WAS WRITTEN.`
- `TOO MANY CHANGES AT ONCE: WRITE FEWER. NOTHING WAS WRITTEN.`

`NO SERVO ANSWERS. CHECK THE PLUG AND THE SUPPLY.` is amber too: the servo
answered nothing, and nothing was written.

The red results include:

- `IN DOUBT: THE ROLL-BACK FAILED. RESTORE FROM FILES.`
- `IN DOUBT: A REGISTER CHANGED THAT WAS NOT WRITTEN. RESTORE FROM FILES.`
- `IN DOUBT: THE SERVO DOES NOT HOLD THE VALUES. RESTORE FROM FILES.`
- `THE LINE FAILED. READ THE SERVO AGAIN.`
- `ABORTED. READ THE SERVO AGAIN.`

### In doubt

<img src="img/kst-doubt.png" width="240" alt="IN DOUBT: THE SERVO DOES NOT HOLD THE VALUES">

After an `IN DOUBT` result the chip on the field list reads `IN DOUBT` on red
and the
write button reads `IN DOUBT` and does nothing. The screen takes no write
until a restore has run. Restore from [FILES](#files).

---

## FILES

`FILES` on the field list opens the card's `KST/*.TXT` files.

<img src="img/kst-files.png" width="240" alt="The KST FILES page">

- The strip counts the files: `CARD  12 FILES`. The page lists the first 64
  files, then the strip reads `CARD  FIRST 64 OF 100 FILES`, for example.
- When the record `KST/KST.TXT` is on the card, it is the first row of all,
  named `RECORD` with `(KST.TXT)` at the right.
- The KST backups of `BACKUP/` follow, the latest first, each with `(BACKUP)`
  at the right. A backup loads as a file.
- Then the files of `KST/`, by name. The KSTnnn files stand together, the
  highest number first.
- Tap a row to select it.

| Button | What it does |
|---|---|
| `SAVE AS KST003` | Writes the servo as it stands into `KST/` under the next free name, one above the highest KSTnnn, and never over a file. The strip reads `SAVED AS KST003` in green. Once `KST/` holds 50 files the button does not act and the strip reads `KST/ HOLDS 50 FILES. REMOVE SOME.` There is no keyboard, so there is no other name |
| `LOAD` | Holds the selected file against the servo (type, access classes, hard limits) and puts its values into the field list as pending changes. The write then goes through `WRITE...` and the write page |
| `RESTORE...` | A tap opens the restore page; its `HOLD TO RESTORE`, held 1 s, writes the image read at entry back to the servo |

A refusal names its line, for example `LINE 14: NOT A VALUE OF DEAD BAND`.
Other notes on the strip:

- `THE FILE IS FOR A V8 SERVO`: the file's type contradicts the servo's.
- `LINE 7: PULSE UPPER BREAKS A LIMIT`: a value in the file breaks a hard
  limit.
- `THE FILE DID NOT READ`, `THE CARD IS FULL`, `THE CARD TOOK NO FILE`.
- `CARD MOUNTED AGAIN: CHOOSE AGAIN`: a write to the card failed earlier, so
  LOAD mounted the card again and read the list again. The card may be
  another one. Nothing loads until a row is chosen again.

`RESTORE...` acts when the servo differs from the backup or is in doubt.
The restore page is titled `KST RESTORE`, and nothing is written until its
`HOLD TO RESTORE` has been held for 1 s.

<img src="img/kst-restore.png" width="240" alt="The restore page before the hold: BACK TO THE SESSION'S BACKUP and HOLD TO RESTORE">

The page reads `BACK TO THE SESSION'S BACKUP` and
`4 REGISTERS DIFFER FROM THE BACKUP`, for example. It counts registers,
LOCKED and unnamed ones included, because the restore writes them too. A
restore that the planner did not check needs a second hold. The page then
reads `THE PLANNER DID NOT CHECK THIS` and `RESTORE. HOLD AGAIN TO RUN IT.`

### No card, a card held by USB, a card that takes no file

- **No card.** The strip reads `NO CARD`. The page reads
  `READ, EDIT AND WRITE STILL WORK.` and offers `RETRY MOUNT`. The backup stays
  in memory.
- **A card held by USB.** The strip reads `CARD IN USE BY USB`. `RETRY MOUNT`
  is drawn disabled. The backup stays in memory, and reading, editing and
  writing work.
- **A card that takes no file.** A card that is mounted but takes neither the
  backup nor the record stops the write before any frame. The write page
  reads
  `THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`

The backups share one ring of 50 numbers with the backups of the AM32
editor and the Hitec programmer, `BACKUP/KSTnnn.TXT`, `BACKUP/AMnnn.TXT` and
`BACKUP/HTnnn.TXT`. `BACKUP/LAST` holds the
latest number, 3 digits and a line feed; no list shows it. The next backup
takes the number after it, 001 after 050, and once all 50 are taken it writes
over the oldest, whatever device it belongs to. Files with any other name,
KST051 and up included, are never written over. Without `BACKUP/LAST` the
latest number is guessed from the names; a ring with all 50 numbers taken
then writes over number 001.

---

## Unknown layout: READ ONLY

The screen checks the 32 registers against rcbench's layout fingerprint. An
image that fails the check is shown and not written. With no row selected,
the editor bar reads `READ ONLY: UNKNOWN LAYOUT` in amber and
`NOTHING IS WRITTEN TO THIS SERVO`, and the write button reads `READ ONLY`
and does nothing.

---

## Leaving

`BACK` on the field list opens the leave page. `BACK` on the search opens it
too, once a servo has answered.

<img src="img/kst-leave.png" width="240" alt="The leave page: POWER THE SERVO OFF."> <img src="img/kst-leave-pending.png" width="240" alt="The leave page with 5 CHANGES NOT WRITTEN.">

The page reads `POWER THE SERVO OFF.` and
`IT STAYS IN PROGRAMMING MODE UNTIL ITS SUPPLY IS OFF.` With pending changes it
adds `2 CHANGES NOT WRITTEN.`, and in doubt
`THE SERVO IS IN DOUBT: RESTORE FIRST.` `BACK` returns to the field list,
or, after a lost servo, to the search.

**Power the servo off. Then tap `LEAVE`.** The screen holds the line low for
100 ms, lets the pin go, and shows `UNPLUG THE SERVO.` under
`DSHOT RESTARTS ON GP0 AND READS AS` and `A 970 US PULSE AT 1000 HZ.`
`LEAVE` also leaves a session that is in doubt.

**Unplug the servo, then tap `RESTART DSHOT`.** The ESC driver takes the pin
back and `MENU` returns. `BACK` on this page returns to the search instead and
keeps the pin. While the page reads `RELEASING THE PIN` or
`SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`, `BACK` is drawn disabled, an outline
with a grey label: it acts from the moment `UNPLUG THE SERVO.` shows.

`BACK` on the search before any servo answered releases the pin and shows the
unplug page directly.

---

## When it does not work

- **`AN ESC ANSWERS ON SIG GP0.`** An ESC is on the ESC pin. Unplug it.
- **`NO SERVO IN 30 S`.** Check signal and ground. Power the servo up again
  while the screen searches: it answers only right after power-on.
- **`REPLY NOT VALID: ...`.** Edges arrive, but no reply is valid.
  Check the signal wire, the ground and the supply. The screen names only
  the word of the fault, not its cause.
- **`READ FAILED: NO 3 EQUAL READS`.** Tap `READ AGAIN`.
- **`THE SCREEN STALLED. NOTHING WAS WRITTEN.`** The screen stopped for more
  than 250 ms before the write went out, and no frame reached the servo.
  Write again.
- **The screen returns to the search.** The servo stopped answering. Power it
  up again.
- **`IN DOUBT`.** Restore from FILES: `RESTORE...`, then `HOLD TO RESTORE`.
- **`THE CARD TOOK NO FILE (FULL OR REMOVED?). NOTHING WENT TO THE SERVO. CHECK THE CARD, HOLD AGAIN.`**
  Check the card, or take it out of USB use, then write again.

---

## Switched supply

With **SETTINGS → SERVO SUPPLY** at `GP28` or `GP29`, the bench powers the servo itself
through a DC-DC converter whose enable input is on GP28, J4 pin 11, or
GP29, J4 pin 12. The
converter gives 5 V, more than 1 A, and has an external pull-down on its
enable: 10 kΩ to 100 kΩ for an enable with no internal pull-up. **An ESC on
this lead gets the converter's output on its BEC wire.**

<img src="img/kst-switched.png" width="240" alt="The switched search: SUPPLY OFF 1.4 S"> <img src="img/kst-leave-switched.png" width="240" alt="The leave page with HOLD TO REBOOT"> <img src="img/kst-stalled.png" width="240" alt="The stalled screen: SUPPLY OFF">

- **The search** switches the servo off for 2 s, then on, before each try:
  `SIG GP0  TRY 3  SUPPLY OFF 1.4 S`. 14 power-ups fit in the 30 s search;
  then `NO ANSWER IN 14 POWER-UPS`.
- **The leave page** reads `LEAVE SWITCHES THE SERVO'S SUPPLY OFF.` After
  LEAVE the unplug page reads `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.`: the pin is
  let go 2 s after the switch-off, whatever ended the session.
- **HOLD TO REBOOT** (1 s) switches the servo off for 2 s and on for 1 s,
  enters programming mode and reads the servo as it is. That read is the
  new backup, and the pending changes go. Without a card the backup in
  memory is lost; the page says so above the button.
- **A screen that stalls** for 250 ms switches the supply off once a running
  operation has ended. The servo leaves programming mode and its backup in
  memory goes. RETRY powers it up and reads it again.

---

## Limitations

- With the switched supply, the card keeps the servo powered for at most
  15 s while the screen waits on it. A card that blocks the screen longer
  counts as a stall: the supply goes off and the backup in memory is lost.
  Not tested with a card.
- The first write's card reads run on core0, and the screen does not redraw
  until they end; its writes queue and flush while the screen keeps drawing.
  With no card at the screen's opening the cache stays off, and a card
  mounted later blocks for the writes too. Without the cache the first
  write's card calls take up to 1.0 s on a full card with access and busy
  times of 10 ms, and at the SD specification's full times up to 13.3 s on a
  FAT32 SDHC (Secure Digital High Capacity) card and 16.5 s on an exFAT SDXC
  (Secure Digital eXtended Capacity) card, however many logs `LOGS/` holds.
  Logs an earlier firmware wrote to the card's root slow every lookup in it:
  with 1000 of them, up to 135.0 s. These are FatFs block counts
  times card times, not measured on a card.

Nothing of the KST programmer has been tested on hardware.
