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

The screen does not switch the servo's supply. You power the servo up and down
by hand, with a plug or a switch. See [Limitations](#limitations).

---

## Wiring

The servo needs its own supply. The board supplies the signal and ground only.
The servo answers only in the first moments after its supply comes on, so the
supply has to be switchable by hand: a plug, or a switch in its positive lead.

```
  servo supply, 5 V
     +  ----/ switch /--------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo signal (orange or white)

  SIG GP29 = J4 pin 12, GND = J4 pin 1 or 5
```

- The owner programs every KST servo, HV (high-voltage) servos included, at
  5 V. Other voltages are not tested.
- The servo's middle wire goes to its supply, never to a pin of the board.
  The board's 5 V pin is its USB (Universal Serial Bus) supply, and a servo
  can draw more than 1 A.
- Switch the board on before the servo supply, and the servo supply off
  before the board.
- Join the two grounds once, at the board.
- SIG is the ESC pin, as set under **MENU → SETTINGS**. GP29 is the
  default.
- Unplug any ESC from the ESC pin. The screen waits while an ESC answers on
  it, but it cannot see every ESC.

---

## Opening it

Open **MENU → KST SERVO**.

The screen takes SIG from the ESC driver. While it does, the strip reads
`RELEASING GP29`, then `TAKING GP29`. While an ESC answers on the pin it
reads `CHECKING GP29 FOR AN ESC` instead.

- If an ESC answers on the pin, the screen says `AN ESC ANSWERS ON GP29.`,
  then `UNPLUG IT: KST FRAMES MAY READ` and `AS THROTTLE.` It takes the pin
  when the ESC is gone.
- While the ESC is armed, the screen says `DISARM FIRST`. While the pins are
  still in use, it says `PINS BUSY`.
- If the board cannot set up the line, the screen says
  `GP29 COULD NOT BE TAKEN (` followed by `REFUSED`, `PWM`, `PIO`, `DMA` or
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
`GP29  TRY 12  1.4 S`. A spinner turns while the search runs.

The body reads `POWER UP` and `THE SERVO NOW`, then
`PLUG IT IN OR SWITCH ITS SUPPLY ON.` and
`IT ANSWERS ONLY RIGHT AFTER POWER-ON.`

**Power the servo up while the screen searches.** The servo enters programming
mode and answers. If edges arrive on the line but no reply is valid, an amber
line names the fault, for example `REPLY NOT VALID: LATE`. The words
are `EARLY`, `LINE HIGH`, `LATE`, `CELL`, `LENGTH`, `RATE`, `FLAGS`,
`LONG LAST EDGE`, `WINDOW`, `OVERFLOW` and `CAPTURE`.

| Strip | What happened | Button |
|---|---|---|
| `GP29  TRY 12  1.4 S` | The search runs | `STOP SEARCH` |
| `NO SERVO IN 30 S` | No servo answered in 30 s. The body reads `CHECK SIGNAL AND GROUND, THEN POWER THE SERVO UP AGAIN.` | `RETRY` |
| `SEARCH STOPPED` | `STOP SEARCH` ended it, or the screen stalled and the search stopped itself | `RETRY` |
| `SEARCH FAILED` | The session ended with a result, which the body names in amber | `RETRY` |

After a servo answers, the strip reads `IN PROGRAMMING MODE` and the body
reads `READING 32 REGISTERS`. The screen reads each register until 3 of 5
reads agree. If a register does not settle, the body reads
`READ FAILED: NO 3 EQUAL READS` and lists the registers, for example
`REGISTERS 03 0A`. `READ AGAIN` repeats the read.

<img src="img/kst-readfail.png" width="240" alt="READ FAILED: NO 3 EQUAL READS, register 05">

If an earlier write did not end, the card holds the record `KST/KST.TXT`. The
screen then adds
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
`BACKUP IN MEMORY` until a write has put the backup on the card, then
`BACKUP KST001`. A chip at the right reads `MODE` (green), or `IN DOUBT`
(amber) after a write left the servo in an unknown state.

Each row shows the group in small letters, the field name, and the value.
Rows are 40 px tall at a 42 px pitch. Drag the list up or down to scroll.

| Group | Fields |
|---|---|
| TUNE | `MAX. DUTY`, `BOOST`, `STRETCH`, `SOFT START`, `SPEED LIMIT`, `SPEED`, `DEAD BAND` |
| PULSE | `PULSE LOWER`, `PULSE UPPER`, `NEUTRAL`, `REVERSION`, `LEFT ANGLE`, `RIGHT ANGLE` |
| FAILSAFE | `ALLOW UNCONT`, `UNCONTROLLED POS`, `UNCONTROLLED TIME` |
| LOCKED | `20K SEL`, `LOCK`, `PROT1 CANCLE`, `PROT1 EN`, `PROT1 TIME`, `PROT1 PWM`, `PROT POT ERR`, `POT SAM TIMES`, `PROT PWM ERR`, `NODE ADDR`, `SERVO / GYRO` |

- Values carry their unit: `96 %`, `300.0 US`, `1.0 S`, `X1`, `ON`, `OFF`, or
  a count in the form `n OF m`. `US` is µs: the screen font has no micro
  sign. A raw value whose display is not established reads `RAW` and the number.
- LOCKED fields are read-only. They are drawn grey and a tap on them selects
  nothing.
- A changed value is drawn green.

<img src="img/kst-edit.png" width="240" alt="DEAD BAND selected and stepped from 2.0 US to 2.2 US">

### Editing a field

1. Tap an editable row. The editor bar under the list shows the field, its
   stored value, and the pending value, for example `DEAD BAND  2.0 US ->`.
   With no row selected, the bar reads `TAP A ROW TO EDIT IT` and
   `SWIPE IT SIDEWAYS FOR COARSE`.
2. Tap `-` or `+` (44 × 44 px) to step the value. Hold to repeat. `+`
   raises the value shown and `-` lowers it, whichever way the raw value
   goes: `+` widens `DEAD BAND`.
3. Or swipe the row sideways. One step is an eleventh of the field's range,
   rounded up, at least one raw step.
4. `REVERT` drops all pending changes.

One raw step does not always change the text: `MAX. DUTY` reads `96 %` at
raw 245 and at raw 244. The editor bar and the write page then add the raw
values: `96 % (245) -> 96 % (244)`.

Two kinds of limit apply:

- **A hard limit** is never offered. The stepper skips values that break
  one, so `-` and `+` stop at the last legal value. For example, `MAX. DUTY`
  stops at 98 %. When no further step is legal, the button is drawn disabled.
- **A soft rule** is taken. The row gets a 3 px amber mark at its left edge
  and the editor bar shows the rule's text in amber, for example
  `WITHOUT SOFT START THE SERVO JUMPS AT POWER-ON.`

Under the editor bar are three buttons, 46 px tall: `HOLD TO WRITE`
(110 px), `REVERT` (52 px) and `FILES` (54 px). `HOLD TO WRITE` acts only when
something is pending, no hard limit is broken, and the servo is not in doubt.

---

## Writing

**Hold `HOLD TO WRITE` for 1 s.** A bar fills while you hold. The write page
opens and writes nothing.

<img src="img/kst-write.png" width="240" alt="The write page, one field"> <img src="img/kst-write-many.png" width="240" alt="The write page, five fields, AND 1 MORE">

The write page is titled `KST WRITE`. The strip counts the fields, `1 FIELD`
or `2 FIELDS`. Each changed field is one line, `DEAD BAND` and
`2.0 US -> 2.2 US`, up to four, then `AND 3 MORE`. Amber lines starting with
`*` give the soft rules that apply. The line at the bottom says where the
backup goes:

| Line | When |
|---|---|
| `BACKUP FIRST: KST/KST001.TXT` | A card is mounted and no write has happened this session |
| `BACKUP: KST/KST001.TXT` | The backup is on the card |
| `NO CARD: BACKUP IN MEMORY` | No card is mounted |
| `CARD IN USE BY USB: BACKUP IN MEMORY` | A USB host holds the card |

**A second hold of 1 s writes.** Before the first frame goes to the servo,
the image read at entry goes to the card as `KST/KSTnnn.TXT`, and the servo
as it stands goes to the card as the record `KST/KST.TXT`. The write then runs
as single-register writes with a read-back, followed by a full verify. A
verified write removes the record.

While the write runs, the page reads `WRITING: DO NOT UNPLUG.` and, under it,
`PLANNING THE ORDER OF THE WRITES` or `FRAMES SENT: 94`. `BACK` during the
write is queued: the page reads `LEAVING AFTER THE WRITE ENDS.` and the leave
page follows the result.

### The result

The result replaces the buttons with `OK`. `OK` returns to the field list.

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

After an `IN DOUBT` result the chip on the field list reads `IN DOUBT` and the
write button reads `IN DOUBT` and does nothing. The screen takes no write
until a restore has run. Restore from [FILES](#files).

---

## FILES

`FILES` on the field list opens the card's `KST/*.TXT` files.

<img src="img/kst-files.png" width="240" alt="The KST FILES page">

- The strip counts the files: `CARD  12 FILES`. The page lists the first 64
  files, then the strip reads `CARD  FIRST 64 OF 100 FILES`, for example.
- The latest KSTnnn is first. When the record `KST/KST.TXT` is on the card,
  it is the first row of all, named `RECORD` with `(KST.TXT)` at the right.
  The backup of this session carries `(BACKUP)`.
- Tap a row to select it.

| Button | What it does |
|---|---|
| `SAVE AS KST003` | Writes the servo as it stands under the next free name, KST001 to KST999. The strip reads `SAVED AS KST002` in green. There is no keyboard, so there is no other name |
| `LOAD` | Holds the selected file against the servo (type, access classes, hard limits) and puts its values into the field list as pending changes. The write then goes through `HOLD TO WRITE` |
| `HOLD TO RESTORE` | Hold 1 s. Writes the image read at entry back to the servo, through the restore page |

A refusal names its line, for example `LINE 14: NOT A VALUE OF DEAD BAND`.
Other notes on the strip:

- `THE FILE IS FOR A V8 SERVO`: the file's type contradicts the servo's.
- `LINE 7: PULSE UPPER BREAKS A LIMIT`: a value in the file breaks a hard
  limit.
- `THE FILE DID NOT READ`, `THE CARD IS FULL`, `THE CARD TOOK NO FILE`.

`HOLD TO RESTORE` acts when the servo differs from the backup or is in doubt.
The restore page is titled `KST RESTORE`. It reads
`BACK TO THE SESSION'S BACKUP` and `4 REGISTERS DIFFER FROM THE BACKUP`,
for example. It counts registers, LOCKED and unnamed ones included, because
the restore writes them too. A restore that the planner did not check needs
a second hold. The page then reads `THE PLANNER DID NOT CHECK THIS` and
`RESTORE. HOLD AGAIN TO RUN IT.`

### No card, a card held by USB, a card that takes no file

- **No card.** The strip reads `NO CARD`. The page reads
  `READ, EDIT AND WRITE STILL WORK.` and offers `RETRY MOUNT`. The backup stays
  in memory.
- **A card held by USB.** The strip reads `CARD IN USE BY USB`. `RETRY MOUNT`
  is drawn disabled. The backup stays in memory, and reading, editing and
  writing work.
- **A card that takes no file.** A card that is mounted but takes neither the
  backup nor the record stops the write before any frame. The write page
  reads `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`

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
`DSHOT RESTARTS ON GP29 AND READS AS` and `A 970 US PULSE AT 1000 HZ.`
`LEAVE` also leaves a session that is in doubt.

**Unplug the servo, then tap `RESTART DSHOT`.** The ESC driver takes the pin
back and `MENU` returns. `BACK` on this page returns to the search instead and
keeps the pin.

`BACK` on the search before any servo answered releases the pin and shows the
unplug page directly.

---

## When it does not work

- **`AN ESC ANSWERS ON GP29.`** An ESC is on the ESC pin. Unplug it.
- **`NO SERVO IN 30 S`.** Check signal and ground. Power the servo up again
  while the screen searches: it answers only right after power-on.
- **`REPLY NOT VALID: ...`.** Edges arrive, but no reply is valid.
  Check the signal wire, the ground and the supply. The screen names only
  the word of the fault, not its cause.
- **`READ FAILED: NO 3 EQUAL READS`.** Tap `READ AGAIN`.
- **The screen returns to the search.** The servo stopped answering. Power it
  up again.
- **`IN DOUBT`.** Restore from FILES with `HOLD TO RESTORE`.
- **`THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`** Check the
  card, or take it out of USB use, then write again.

---

## Limitations

- No switched supply: the servo is powered up and down by hand. The design
  note's step 5 (`docs/design/kst-programming.md`) adds a converter on GP0.
- The first write's card calls run on core0, and the screen does not
  redraw until they end. With 997 KSTnnn files on the card that is under
  7.3 s for a card with access and busy times of a few ms, and up to 82.2 s
  for an SDXC (Secure Digital eXtended Capacity) card at the specification's
  full times. Both are computed from FatFs block counts, not measured on a
  card.

Nothing of the KST programmer has been tested on hardware.
