**English** | [Deutsch](Hitec-Programmierung)

# Hitec Programming

This screen reads and writes the settings of a Hitec D series servo over its
signal wire. The protocol code is rcbench's: its session, register model,
wire layer and PIO (programmable I/O) line driver. D series frames run at
115,200 bit/s, half duplex, on the ESC (electronic speed controller) pin. The
screen reads the servo's 21 registers, shows 20 parameters with their units,
steps the ones that may change, and writes them back with a backup on the SD
(Secure Digital) card first.

<img src="img/hitec-list.png" width="240" alt="The Hitec field list of a D485HW">

> **Not tested on hardware.** No byte of the line driver has been on a pin of
> this board, and no write plan of rcbench's has run on a servo. The screens
> in this wiki are rendered from the UI (user interface) code against a
> simulated servo.

> **Only the D series opens.** The classic family (HS-5xxx, HS-7xxx) is
> shown and disabled. Its line level is not measured. See
> [Classic servos](#classic-servos).

> **Unplug the servo before DShot runs.** Outside this screen the ESC pin
> carries DShot, the digital throttle protocol. A servo on that pin reads it
> as a pulse of about 970 µs at 1000 Hz and drives towards one end of its
> travel. That happens when the board **boots** with a servo on the ESC pin,
> and when you leave this screen. The screen asks you to unplug the servo
> first.

Hitec is a name or trademark of its owner. It is used here only to say which
servos this screen works with. MiniRcBench is not affiliated with Hitec.

---

## Wiring

Wire the servo as for [KST Programming](KST-Programming#wiring): the servo on
its own supply, its signal on SIG, the two grounds joined once at the board.

```
  servo supply, 5 V
     +  ----/ switch /--------------------------------  servo +   (red, middle)
     -  ----------------+-----------------------------  servo -   (brown or black)
                        |
  board GND  -----------+
  board SIG  ------------------------------------------  servo signal (orange or white)

  SIG GP29 = J4 pin 12, GND = J4 pin 1 or 5
```

- **One servo on the lead.** Every frame goes to ID (identifier) 0, and
  every D series servo on the line takes ID 0.
- **No pull-up resistor** on the signal wire. The bench receives the
  servo's answer by precharge: before each sample the pin lifts the line for
  100 ns. rcbench's receive through a pull-up resistor is not offered.
- The servo's middle wire goes to its supply, never to a pin of the board.
  The board's 5 V pin is its USB (Universal Serial Bus) supply.
- Switch the board on before the servo supply, and the servo supply off
  before the board.
- SIG is the ESC pin, as set under **MENU → SETTINGS**. GP29 is the
  default.
- Servo supplies other than 5.0 V are not tested.
- Unplug any ESC from the ESC pin. The screen checks for an ESC on the pin,
  but it sees only an ESC that answers bidirectional DShot.

With **SETTINGS → SERVO SUPPLY** at `GP0`, the bench powers the servo itself,
through the converter described under
[KST Programming](KST-Programming#switched-supply). See
[Switched supply](#switched-supply). **An ESC on this lead gets the
converter's output on its BEC (battery eliminator circuit) wire**, the wire
of its own regulated supply.

---

## Choosing the family

Open **MENU → HITEC SERVO**. The screen opens on the family view. It claims
no pin yet.

<img src="img/hitec-family.png" width="240" alt="The family view: CLASSIC disabled with LINE LEVEL NOT MEASURED, D SERIES on the servo's own supply">

The strip reads `CHOOSE THE SERVO'S FAMILY`. Two buttons follow:

- `CLASSIC`, with `HS-5XXX, HS-7XXX` under it, drawn disabled. A line in
  amber gives the reason: `LINE LEVEL NOT MEASURED`. Once the level is
  measured, the line reads `NEEDS SERVO SUPPLY GP0` while SERVO SUPPLY is
  `NONE`.
- `D SERIES`, with `D485HW, D645MW AND THE OTHERS` under it, and
  `SUPPLY SWITCHED ON GP0` or `ON THE SERVO'S OWN SUPPLY`.

**Tap `D SERIES`.** `BACK` on this view returns to `MENU`.

The screen then takes SIG from the ESC driver. While it does, the strip reads
`RELEASING GP29`, then `TAKING GP29`. While an ESC answers on the pin it
reads `CHECKING GP29 FOR AN ESC` instead.

- If an ESC answers on the pin, the screen says `AN ESC ANSWERS ON GP29.`,
  then `UNPLUG IT: A HITEC ENTRY SENDS` and
  `FRAMES INTO WHAT IS ON THE WIRE.` It takes the pin when the ESC is gone.
- While the ESC is armed, the screen says `DISARM FIRST`. While the pins are
  still in use, it says `PINS BUSY`.
- If the board cannot set up the line, the screen says
  `GP29 COULD NOT BE TAKEN (` followed by `REFUSED`, `PWM`, `PIO`, `DMA` or
  `CLOCK` and a closing bracket. PWM is pulse-width modulation and DMA is
  direct memory access. `RETRY` tries again.

> A powered servo on SIG gets no servo pulses while the screen is open. It
> may go to its failsafe position.

---

## Entering and reading

When the pin is taken, the entry starts at once.

- **With SERVO SUPPLY at `NONE`**, the screen reads the servo on the supply
  it has. Power the servo before you tap `D SERIES`. The body reads
  `READING THE MODEL`.
- **With SERVO SUPPLY at `GP0`**, the screen switches the supply off for
  2.0 s and on again. The strip counts the off time, `GP29  SUPPLY OFF 1.4 S`,
  and the body reads `SUPPLY OFF`. Then the strip reads `GP29  SUPPLY ON` and
  the body `SERVO STARTING` while the servo starts, about 1 s. The screen
  shows no count for this second. Two amber lines stay under the text:
  `AN ESC ON THIS LEAD GETS THE` and `CONVERTER'S OUTPUT ON ITS BEC WIRE.`

`STOP` ends the entry. The screen then reads `THE ENTRY WAS STOPPED.` with
`RETRY`.

A servo that does not answer gives the strip `NO SERVO ANSWERS`. An amber
line names the last reply, for example `REPLY: SILENT`, and the body reads
`CHECK THE SERVO'S SUPPLY, THE SIGNAL AND GROUND.` With `GP0` the screen adds
`THE SUPPLY IS OFF.` `RETRY` enters again. The reply words are `SILENT`,
`EARLY`, `LATE`, `NAK`, `LENGTH`, `FRAMING`, `GAP`, `NO ACK`, `HEADER`,
`ADDRESS`, `SUM`, `CUT`, `OVERFLOW` and `ARG`; `NONE` means no reply code.

Once the servo answers, the strip reads `GP29  IN SERIAL MODE` and the body
counts the read: `READING 21 REGISTERS`, `7 OF 21`. This first full read is
the session's backup. A read that fails reads `READ FAILED AT 0x32`, for
example, with the reply word under it, and offers `READ AGAIN`. A read
stopped with `STOP` reads `THE READ WAS STOPPED.` `BACK` during a read stops
it and opens the leave page.

A finished read opens the field list.

With a record of an interrupted write on the card, the entry and read views
add in amber `A WRITE WAS INTERRUPTED. HITEC/` and
`HITEC.TXT HOLDS THE SERVO AS IT WAS.` See [FILES](#files).

---

## The field list

The strip shows the model, the pin and the backup:
`D485HW GP29 BACKUP IN MEMORY` until a write has put the backup on the card,
then `BACKUP HT001`, for example. A model that has no name in rcbench's list
reads `D SERIES` and its number. A green chip at the right reads `MODE`.

Each row shows the group in small letters, the parameter name and the value.
Rows are 40 px tall at a 42 px pitch. Drag the list up or down to scroll.

| Group | Parameters |
|---|---|
| MOTION | `SPEED`, `DEAD BAND`, `SOFT START` |
| TRAVEL | `NEUTRAL`, `LEFT END`, `RIGHT END`, `DIRECTION` |
| PROTECT | `FAILSAFE`, `OVERLOAD`, `SMART SENSOR`, `SENSITIVITY` |
| INFO | `MODEL`, `ID`, `POSITION`, `R04`, `R22`, `R8A`, `R8C`, `RD4`, `RD6` |

- Values carry their unit and the stored value in brackets: `90 % (18)`,
  `STEP 1 (1)`, `CW (0)`. `CW` is clockwise and `CCW` counter-clockwise.
  `NEUTRAL`, the end points and `POSITION` are counts, for example `8192`,
  with no bracket. `FAILSAFE` reads `LIMP`, `OFF`, or a position in `US`.
  `US` is µs: the screen font has no micro sign.
- A stored value that no encoding takes reads `RAW` and the number in amber,
  for example `RAW 1234`. A value that does not decode reads `?`.
- Read-only rows are drawn dim and a tap on them selects nothing:
  `SENSITIVITY`, and every INFO row but `ID`.
- A changed value is drawn green, and its name carries a `*`.

<img src="img/hitec-preview.png" width="240" alt="SPEED selected and stepped from 100 % (4095) to 90 % (18)">

### Editing a parameter

1. Tap an editable row. The editor bar under the list shows the parameter,
   its stored value and the pending value, for example
   `SPEED  100 % (4095) ->` above `90 % (18)`. With no row selected, the bar
   reads `TAP A ROW TO EDIT IT` and `SWIPE IT SIDEWAYS FOR COARSE`.
2. Tap `-` or `+` (44 × 44 px) to step the value to the next value its
   encoding takes. Hold to repeat. `+` raises the value shown and `-` lowers
   it.
3. Or swipe the row sideways: one coarse step per 16 px. A coarse step is an
   eleventh of the range, rounded up, at least one step.
4. `REVERT` drops all pending changes.

`FAILSAFE` steps through `LIMP`, `OFF`, then positions of 900 µs to
2,100 µs. A `RAW` value takes the nearest legal value on its first step.

Reversing `DIRECTION` mirrors `NEUTRAL` and both end points, as Hitec's
programmer does.

Two kinds of limit apply:

- **A hard limit** is never offered. The stepper skips values that break
  one, so `-` and `+` stop at the last legal value. The encoding's range and
  steps are hard limits, and so is the order `LEFT END` below `NEUTRAL`
  below `RIGHT END`. When no further step is legal, the button is drawn
  disabled.
- **A soft rule** is taken. The row gets a 3 px amber mark at its left edge
  and the editor bar shows the rule's text in amber. The rules are
  rcbench's seven, compared with the servo as last read:

| Rule | Text |
|---|---|
| `SPEED` below 30 % | `A SLOW SERVO CAN CAUSE LOSS OF CONTROL OF THE MODEL.` |
| `DEAD BAND` below the servo's | `A NARROW DEAD BAND CAN CAUSE BACK-AND-FORTH MOVEMENT AND HEAT.` |
| `OVERLOAD` switched off | `WITHOUT OVERLOAD PROTECTION A STALLED SERVO HEATS UP.` |
| An end point other than the servo's | `END POINT CHANGED. THE SERVO TRAVEL PER STICK MOVEMENT CHANGES TOO.` |
| `NEUTRAL` other than the servo's | `THE CENTRE SHIFTS. ONE SIDE MOVES CLOSER TO THE MECHANICAL STOP.` |
| `DIRECTION` reversed | `DIRECTION REVERSED. CHECK ALL CONTROL SURFACES BEFORE FLIGHT.` |
| A fail-safe position more than 300 µs from 1,500 µs | `FAILSAFE POSITION IS FAR FROM THE CENTRE.` |

### The ID

`ID` sets the servo's own ID, 0 to 255. The bench addresses ID 0 whatever
the servo holds. Whether a servo with another ID still answers ID 0 is not
measured. While `ID` holds another value than 0, the editor bar and the
write page read `THE BENCH ADDRESSES ID 0. ANOTHER ID IS NOT TESTED.`

<img src="img/hitec-id.png" width="240" alt="The write page for ID 0 -> 1, with the amber ID note">

Under the editor bar are three buttons, 46 px tall: `HOLD TO WRITE`
(110 px), `REVERT` (52 px) and `FILES` (54 px). `HOLD TO WRITE` acts only
when something is pending and the servo has been read since the last write
that failed.

---

## Writing

**Hold `HOLD TO WRITE` for 1 s.** A bar fills while you hold. The write page
opens and writes nothing.

<img src="img/hitec-write.png" width="240" alt="The write page: SPEED 100 % (4095) -> 90 % (18), KEEP THE SERVO HORN CLEAR.">

The write page is titled `HITEC WRITE`. The strip counts the values,
`1 VALUE` or `2 VALUES`. Each changed parameter is one line, `SPEED` and
`100 % (4095) -> 90 % (18)`, up to six, then `AND 3 MORE`, for example.
Amber lines starting with `*` give the soft rules that apply. Other amber
lines:

- `MODEL NOT MEASURED: WRITTEN IN THE D LAYOUT.` for a model rcbench has not
  measured. The measured D series model is the D485HW.
- `THE BENCH ADDRESSES ID 0. ANOTHER ID IS NOT TESTED.`
- `FILE OF ANOTHER MODEL: D645MW.`, for example, after a LOAD of a file of
  another model.

`KEEP THE SERVO HORN CLEAR.` shows on the session's first write, and on
every write that changes `DIRECTION`, `NEUTRAL` or an end point.

The line at the bottom says where the backup goes:

| Line | When |
|---|---|
| `BACKUP FIRST: BACKUP/HT001.TXT` | A card is mounted and the backup is not on it yet |
| `BACKUP: BACKUP/HT001.TXT` | The backup is on the card |
| `NO CARD: BACKUP IN MEMORY` | No card is mounted |
| `CARD IN USE BY USB: BACKUP IN MEMORY` | A USB host holds the card |
| `BACKUP FIRST: TO THE CARD` | A card is mounted and the next backup number did not read from it |
| `NO BACKUP ON THE CARD: IN MEMORY` | The card took the record and refused the backup |

**A second hold of 1 s writes.** The card steps come first:

1. Before the session's first write, with a card mounted: a record left on
   the card by an interrupted write moves into the ring of `BACKUP/`, then
   the backup goes to the card as `BACKUP/HTnnn.TXT`.
2. Before every write: the servo as it is and the target go to the card as
   the record `HITEC/HITEC.TXT`.

With a card mounted when the screen opened, these files first go to the
card while the screen keeps drawing. The page reads
`SAVING THE BACKUP TO THE CARD.` (or `SAVING THE FILES TO THE CARD.` on a
later write) and `BLOCKS LEFT: 12`, for example. The servo is written only
once the card holds the files. A flush the card does not take stops the
write: `THE CARD DID NOT TAKE THE BACKUP. NOTHING WAS WRITTEN TO THE SERVO.`

The write then sends the changed registers 20 ms apart, saves them in the
servo, restarts the servo (1.0 s) and reads all 21 registers back. For one
setting this takes about 1.7 s, computed from rcbench's timings and not
measured. The page reads `WRITING: DO NOT UNPLUG.` and, under it,
`WRITES SENT: 2 OF 4`, then `READING BACK`. `BACK` during the write is
queued: the page reads `LEAVING AFTER THE WRITE ENDS.` and the leave page
follows the result. The screen never stops a write.

A verified write removes its record. Later writes of the session save no
second backup.

### The result

The result replaces the hold with `OK`. `OK` returns to the field list.

<img src="img/hitec-write-error.png" width="240" alt="THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.">

| Result | Colour | Meaning |
|---|---|---|
| `1 VALUE WRITTEN AND VERIFIED.` | green | The servo holds the new values |
| `NOTHING WAS WRITTEN ...` | amber | No frame reached the servo |
| `THE SERVO DOES NOT HOLD THE VALUES: 0x54. READ AND CHECK.` | red | The read-back differs at the named registers, at most four, then `...` |
| `REGISTER 0x54 GOT NO ANSWER. READ THE SERVO BEFORE A WRITE.` | red | A write got no answer |
| `THE WRITE STOPPED AT STEP 2 OF 4. READ THE SERVO BEFORE A WRITE.` | red | Part of the plan ran; the servo state is not known |
| `ALL WRITES SENT; READ-BACK FAILED. READ THE SERVO BEFORE A WRITE.` | red | Every write went out and the read-back did not end |

The amber results include:

- `NOTHING WAS WRITTEN: READ THE SERVO FIRST.`
- `NOTHING WAS WRITTEN: NO PLAN (CODE n).`, with the planner's code.
- `NOTHING WAS WRITTEN (CODE n).`, with the session's code.
- `THE SCREEN STALLED. NOTHING WAS WRITTEN.`
- `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`
- `THE CARD DID NOT TAKE A FILE. NOTHING WAS WRITTEN TO THE SERVO.`

After a write that failed once frames had gone out, the image no longer
holds the registers the write touched: the write button reads `READ AGAIN`
until a read fills them. The record stays on the card.

---

## FILES

`FILES` on the field list opens the card's Hitec files.

<img src="img/hitec-files.png" width="240" alt="The HITEC FILES page after SAVE AS: SAVED AS HT001">

- The strip counts the files: `CARD  12 FILES`.
- When the record `HITEC/HITEC.TXT` is on the card, it is the first row,
  named `RECORD` with `(HITEC.TXT)` in amber at the right.
- The Hitec backups of `BACKUP/` follow, the latest first, each with
  `(BACKUP)` at the right.
- Then the `.TXT` files of `HITEC/`, by name without case, at most 64. A
  name longer than 8.3 (12 characters) is not listed. A row shows the name
  without `.TXT`, `HITEC001` for an rcbench file.
- Tap a row to select it.

| Button | What it does |
|---|---|
| `SAVE AS HT002` | Writes the servo as last read into `HITEC/` under the next free name, one above the highest HTnnn, and never over a file. The strip reads `SAVED AS HT002` in green. Once `HITEC/` holds 50 `.TXT` files, the record left out, the button does not act and the strip reads `HITEC/ HOLDS 50 FILES. REMOVE SOME.` |
| `LOAD` | Puts the selected file's writable values into the field list as pending changes. The write then goes through `HOLD TO WRITE`. A record loads the servo as it was before its write |
| `HOLD TO RESTORE` | Hold 1 s. Opens the restore page, titled `HITEC RESTORE`, which writes the session's backup back. It acts while the servo differs from the backup |

The restore page's strip reads `BACK TO THE BACKUP: 3 VALUES`, for example.
Its hold, `HOLD TO RESTORE`, writes as `HOLD TO WRITE` does.

A file of the other family or of another layout is refused:
`NOT A FILE FOR THIS SERVO: CLASSIC` or `NOT A FILE FOR THIS SERVO: LAYOUT`.
A file of another model of the same layout loads, and the write page names
it. Other notes on the strip:

- `THE FILE DID NOT PARSE`: the text is not a servo file, or its CRC
  (cyclic redundancy check) line does not match.
- `THE FILE DID NOT READ`, `THE CARD IS FULL`, `THE CARD TOOK NO FILE`,
  `THE CARD'S LIST DID NOT READ`.
- `CARD MOUNTED AGAIN: CHOOSE AGAIN`: a write to the card failed earlier, so
  LOAD mounted the card again and read the list again. Nothing loads until a
  row is chosen again.

The files are rcbench's servo file text with a CRC-16 line. A file from
rcbench's card loads once copied into `HITEC/`. A file edited by hand does
not load.

### No card, a card held by USB, a card that takes no file

- **No card.** The strip reads `NO CARD`. The page reads
  `READ, EDIT AND WRITE STILL WORK.` and offers `RETRY MOUNT`. The backup
  and the record stay in memory.
- **A card held by USB.** The strip reads `CARD IN USE BY USB`.
  `RETRY MOUNT` is drawn disabled. The backup stays in memory, and reading,
  editing and writing work.
- **A card that takes no file.** A card that is mounted but takes neither
  the backup nor the record stops the write before any frame. The write page
  reads `THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`

The backups share one ring of 50 numbers with the KST and AM32 backups,
`BACKUP/HTnnn.TXT`, `BACKUP/KSTnnn.TXT` and `BACKUP/AMnnn.TXT`. Once all 50
are taken, the next backup writes over the oldest, whatever device it
belongs to. See [KST Programming](KST-Programming#files) for the ring.

---

## Leaving

`BACK` on the field list opens the leave page. `BACK` during a read opens it
too.

The strip names the model, the pin and, with `GP0`, `SUPPLY ON` or
`SUPPLY OFF`. The page reads `LEAVE SWITCHES THE SERVO'S SUPPLY OFF.` with
`GP0`, or `LEAVE RESTARTS THE SERVO.` with `NONE`. With pending changes it
adds `2 CHANGES NOT WRITTEN.`, and after a write that failed
`A WRITE DID NOT END VERIFIED.` in amber.

| Button | What it does |
|---|---|
| `READ AGAIN` | Reads the servo again and returns to the field list. Pending changes stay |
| `LEAVE` | Ends the session: with `GP0` it switches the supply off, with `NONE` it restarts the servo from its saved values |
| `BACK` | Returns to the field list |

After `LEAVE` with `GP0`, the next page reads
`SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` until the line closes. Then it reads
`UNPLUG THE SERVO.` over `DSHOT RESTARTS ON GP29 AND READS AS` and
`A 970 US PULSE AT 1000 HZ.`

<img src="img/hitec-unplug.png" width="240" alt="The unplug page: UNPLUG THE SERVO. and RESTART DSHOT">

**Unplug the servo, then tap `RESTART DSHOT`.** The ESC driver takes the pin
back and `MENU` returns. `BACK` on this page keeps the pin and enters the
servo again.

`BACK` during the entry, before the servo answered, stops it and shows the
unplug page.

---

## When it does not work

- **`AN ESC ANSWERS ON GP29.`** An ESC is on the ESC pin. Unplug it.
- **`NO SERVO ANSWERS` with `REPLY: SILENT`.** Nothing answered. Check the
  servo's supply, the signal wire and the ground. With `NONE`, power the
  servo before the entry.
- **`NO SERVO ANSWERS` with another reply word.** Edges arrive, but no reply
  is valid. Check the signal wire and the ground, and that no pull-up
  resistor and no second servo is on the lead.
- **`READ FAILED AT 0x..`.** Tap `READ AGAIN`.
- **`THE SCREEN STALLED. NOTHING WAS WRITTEN.`** The screen stopped for more
  than 250 ms before the write went out. Write again.
- **A red result.** Tap `OK`, then `READ AGAIN`, and check the values. To
  go back to the servo as it was, use `HOLD TO RESTORE` in FILES.
- **`THE CARD TOOK NO FILE. NOTHING WAS WRITTEN TO THE SERVO.`** Check the
  card, or take it out of USB use, then write again.

---

## Switched supply

With **SETTINGS → SERVO SUPPLY** at `GP0`, the bench powers the servo through
the DC-DC converter (direct-current voltage converter) whose enable input is
on GP0, J5 pin 2. Its wiring is on the
[KST Programming](KST-Programming#switched-supply) page. **An ESC on this
lead gets the converter's output on its BEC wire.**

<img src="img/hitec-stall.png" width="240" alt="The stalled screen: GP29 SUPPLY OFF, RETRY">

- **The entry** switches the servo off for 2.0 s, then on, and waits about
  1 s for its start.
- **The supply is on only while the screen runs.** A screen that stalls for
  250 ms switches it off: at once during the entry, otherwise once a running
  read or write has ended. The page then reads
  `THE SCREEN STALLED, SO THE BENCH SWITCHED THE SERVO'S SUPPLY OFF.` and
  `RETRY POWERS IT UP AND READS IT AGAIN.` Without a card, after a write, it
  adds `NO CARD: THE BACKUP IN MEMORY IS LOST.`
- **LEAVE** switches the supply off. The pin is let go 2.0 s after the
  switch-off.

With `NONE`, a stall ends the session too. The page reads
`THE SCREEN STALLED. RETRY ENTERS AGAIN.`

---

## Classic servos

`CLASSIC` (HS-5xxx, HS-7xxx) stays disabled with `LINE LEVEL NOT MEASURED`.
The reason is the signal level:

- A classic servo drives the signal line high itself, through about 1 kΩ.
- The voltage it drives is not measured. A 5 V servo may put 5 V on GP29.
- The board's GPIO (general-purpose input/output) pins allow 3.6 V.

The classic family opens once bring-up has measured that level and the
firmware is built with it marked as measured. It then needs SERVO SUPPLY at
`GP0`: a classic servo enters programming mode only at its power-up. Under
`NONE` the family view reads `NEEDS SERVO SUPPLY GP0`.

The classic screens are built and tested on the host only, against a
simulated servo. The pictures below show them with the line level set as
measured and SERVO SUPPLY at `GP0`.

<img src="img/hitec-family-classic.png" width="240" alt="The family view with CLASSIC enabled, rendered with the line level set as measured and SERVO SUPPLY at GP0"> <img src="img/hitec-repair.png" width="240" alt="A classic HS-5645MG: CHECKSUM BAD with REPAIR"> <img src="img/hitec-not-measured.png" width="240" alt="A classic model not measured: MODEL NOT MEASURED: WRITTEN IN THE CLASSIC LAYOUT.">

- **CHECKSUM.** A classic servo whose checksum does not sum shows
  `CHECKSUM` `BAD` in amber. The row can be selected, and the editor bar
  reads `CHECKSUM BAD. THE SERVO MAY IGNORE PWM AND DRAW HIGH CURRENT.` with
  `REPAIR` in place of `-` and `+`. REPAIR is then written through the write
  page.
- **FACTORY VALUES.** FILES lists the servo's own factory values as a row
  tagged `(SERVO)`. LOAD puts them into the field list as pending changes.
- **Models.** The measured classic models are the HS-5645MG and the
  HS-7955TG. Another model is written in its series' layout, and the write
  page says `MODEL NOT MEASURED: WRITTEN IN THE CLASSIC LAYOUT.`

---

## Limitations

- The bench sends every frame to ID 0. A D series servo with another ID is
  not tested.
- With `GP0`, the screen's card calls keep the supply on for at most 15 s
  while the screen does not redraw, as for KST. At the SD specification's
  full times, the first write that keeps an old record takes 17.9 s on an
  exFAT SDXC (Secure Digital eXtended Capacity) card. That is over the cap,
  and the supply goes off. On FAT32 every Hitec card phase takes at most
  13.2 s with no logs in the card's root. These are FatFs block counts times
  card times, not measured on a card.
- A later write whose record the card does not take still runs, on the first
  write's backup. The write page does not say so.
- `READ AGAIN` drops a pending classic REPAIR. The read shows `BAD` again.
- The entry shows no countdown for the servo's 1 s start.

Nothing of the Hitec programmer has been tested on hardware.
