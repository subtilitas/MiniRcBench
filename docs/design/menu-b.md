# Menu structure B

Status: slices 1, 3 and 4 built, host tests only; slices 2 and 5 to 10 not
built. Slice 3 is the names and SETTINGS of decisions 9, 10 and 15 b, e, f,
h, j, k, with its README.md and wiki lines and images ([slice 3 as
built](#slice-3-as-built)). The owner answered the note's eight open
questions on 2026-11-14 ([decisions 23 to 30](#decisions)), and the question
that followed from them (decision 33). Slices 2 and 5 to 10 wait on being
built. By the owner's order of work of 2026-11-14 they follow the eight
small decisions of STATUS.md items 20, 21 and 24. Three of the owner's
decisions of 2026-11-14 change this design, and the sections they touch are
marked "to be redesigned": each slice updates its own README.md and wiki
lines and images (decision 27, against this note's recommendation), so the
[slice table](#slices) is reworked and slice 10 shrinks; HITEC SERVO takes
SIG when it opens (decision 31), which changes decision 19; and MENU gains a
seventh row, RX SIGNALS (decision 32), so MENU scrolls 36 px, which revises
owner decision 1's six rows that never scroll (decision 33).

The owner decided on 2026-11-13 to build structure B ([decisions 1 to
4](#decisions)). Decisions 5 to 15 are the rules this note sets for the
findings of a graphical user interface (GUI) audit and a usability audit,
each checked against the code; decisions 16 to 22 are further choices of this
note. On 2026-11-14 the owner confirmed, in answering the open questions,
decision 5's disabled BACK on the leave page, decision 8's colour, decision
15 e and h, and decisions 16, 20, 21 and 22 (decisions 23 to 30); the rest
of both groups is not confirmed by the owner. The note describes `main`
at 9f965a3 with slice 1 added. Line numbers in `src/`, `test/`, README.md and
the wiki, and the test counts in decisions 5 to 15, name 2940f9c and are not
recounted.

Terms: UI is user interface, ESC electronic speed controller, DShot the digital
ESC protocol the tester drives, EDT Extended DShot Telemetry, eRPM electrical
revolutions per minute, KISS the telemetry protocol of the KISS ESC line on its
own wire, AM32 an open ESC firmware with a serial bootloader, KST and Chaservo
two servo brands that share one programming protocol, Hitec a servo brand with
its own protocol, SD Secure Digital, USB Universal Serial Bus, GPIO
general-purpose input/output, PWM pulse-width modulation, BEC battery eliminator
circuit. SIG and AUX are the two signal pins as
[pin-claim.md](pin-claim.md#the-two-pins) defines them. Header, strip and body
are the bands of [menu.md](menu.md): the header is y 0 to 35, the strip starts
at y 40 (`UI_STRIP_Y`), the body at y 62 (`UI_BODY_Y`).

Text widths follow `gfxTextW()`: (6 × characters − 1) × scale. At scale 1 a
line from x 8 (`UI_MARGIN`) to x 232 holds 37 characters (221 px); at scale
2, 18 characters (214 px). A title at scale 2 from x 8 ends at least 4 px
before BACK at x 170, so it holds 13 characters (154 px).

## Why

The tree as built at 9f965a3, from the tester:

```
TESTER (root, no header)
 └─ MENU: BEEP THE ESC, AM32 CONFIG, SERVO TESTER, KST SERVO,
    HITEC SERVO, SETTINGS, SD LOG: seven rows, scrolls 36 px
     ├─ KST SERVO: search ─> list; BACK ─> leave page; BACK ─> list;
     │   LEAVE ─> unplug page; BACK ─> a new search; RESTART DSHOT ─> MENU
     └─ HITEC SERVO: family view (CLASSIC, D SERIES) ─> entry ─> list;
         the same leave and unplug pages
```

What does not hold up:

- **Seven rows.** MENU scrolls: SD LOG, the seventh row, is cut until
  MENU is dragged 36 px. A third servo brand would add an eighth row. KST
  SERVO and HITEC SERVO are two rows for one task.
- **BACK means four things.** On the KST and Hitec lists BACK opens the leave
  page; on the leave page it returns to the list; on the unplug page it keeps
  the claim and starts a new session; during a write it queues the leave
  (`uiKstTick()`, `uiHitecTick()`). BACK alone never leaves the tool. On the
  unplug pages BACK is drawn disabled until the pin is let go, and then acts
  (`backActs()` and `drawUnplug()` in `ui_kst.cpp`, `ui_hitec.cpp`,
  `ui_servo.cpp`).
- **Two pages to leave.** The leave page and the unplug page are separate
  screens in two styles: scale-1 lines on the leave page, a scale-2 headline
  and a centred button on the unplug page.
- **Two meanings of HOLD TO WRITE.** On AM32 it writes the ESC; on KST and
  Hitec it opens a review page, where a second 1000 ms hold writes.
- **Three answers to unwritten edits.** AM32 drops them on BACK without a
  word; KST and Hitec name them on the leave page; SETTINGS keeps them and
  marks UNSAVED.
- **Colour.** A changed value is amber on AM32 and lime on KST and Hitec;
  green means SAFE on SERVO and "in programming mode" on KST and Hitec; cyan
  has twelve uses with unrelated meanings ([decision 11](#decisions)).
- **Pin names.** SIG and AUX on the wiring sheet, ESC PIN and KISS PIN in
  SETTINGS, GP0 and GP1 on the screens. KISS PIN is locked at `--` while
  KISS is off, although the servo tester drives servo B on that pin.

## Navigation

```
TESTER (root, no header)                                        depth 0
 ├─ MENU (tap): disarms, throttle 0, LATCH off ──> MENU         1
 ├─ 14 POLES or MAX 20% (tap, disarmed): same path ──> SETTINGS 1
 └─ MENU                                     BACK ──> TESTER
     ├─ BEEP THE ESC        action row: the DShot beacon        (2)
     ├─ AM32 CONFIG         claims SIG                          2
     │   ├─ HEX             HEX tap                             3
     │   └─ AM32 WRITE      WRITE... tap; the hold writes       3
     ├─ SERVO TESTER        claims SIG, AUX for two servos      2
     │   ├─ SERVO SET       SET tap                             3
     │   └─ leave page      BACK                                3
     ├─ SERVO PROGRAMMER    chooser, claims nothing             2
     │   ├─ KST / CHASERVO  KST SERVO: claim, search, read      3
     │   │   ├─ field list  after the read, no tap              3
     │   │   │   ├─ FILES                                       4
     │   │   │   └─ KST WRITE   WRITE... tap                    4
     │   │   └─ leave page  BACK from the list or the search    4
     │   ├─ HITEC D SERIES  HITEC SERVO: claim, entry, read     3
     │   │   └─ as KST SERVO
     │   └─ HITEC CLASSIC   disabled while SERVO SUPPLY is OWN  3
     ├─ RX SIGNALS          chooser: MONITOR, OUTPUT; to design 2
     ├─ SD LOG                                                  2
     └─ SETTINGS            opened from MENU                    2
```

RX SIGNALS is the owner's row of 2026-11-14 ([decision 32](#decisions));
its chooser and the two tools are designed in
[rx-signals.md](rx-signals.md), not here. A tap on HITEC D SERIES or HITEC
CLASSIC takes SIG ([decision 31](#decisions)).

Depth counts taps from the tester. The KST field list moves from depth 2 to
3; the Hitec field list stays at 3 (MENU, HITEC SERVO, family); the AM32
write moves from a hold at depth 2 to a hold on a page at depth 3.

What header BACK does on every view:

| View | BACK |
|---|---|
| Tester | no header |
| MENU | the tester |
| SETTINGS | where it was opened from: MENU, or the tester ([decision 17](#decisions)) |
| SD LOG | MENU |
| AM32 CONFIG, connect page | MENU |
| AM32 CONFIG, list, no edits | MENU |
| AM32 CONFIG, list, edits | first BACK arms the guard ([decision 7](#decisions)); the second goes to MENU and drops them |
| AM32 HEX | the list |
| AM32 WRITE, before the hold, or with the result | the list |
| AM32 WRITE, writing | drawn disabled |
| SERVO TESTER | the leave page |
| SERVO SET, no pending change | SERVO TESTER |
| SERVO SET, a TYPE or RATE change pending | first BACK arms the guard; the second returns and drops it |
| leave page, every phase | drawn disabled; each way on is a labelled button |
| PROGRAMMER (chooser) | MENU |
| KST SERVO, claim refused or Stopping | MENU; DShot resumes at once, as today |
| KST SERVO, search or read, no servo in the mode | the leave page, in its release phase |
| KST SERVO, search or read, a servo in the mode | the leave page; a read under way is aborted, as today |
| KST field list | the leave page |
| KST FILES | the field list |
| KST WRITE, before the hold, or with the result | the field list |
| KST WRITE, writing or flushing | drawn disabled |
| HITEC SERVO, every view | as KST SERVO |

## MENU

By [decision 32](#decisions) MENU has seven rows, BEEP THE ESC, AM32
CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG, SETTINGS. By
the owner's [decision 33](#decisions) the rows keep their 40 px height and
42 px pitch, and MENU scrolls: seven rows take 7 × 42 = 294 px of the
258 px viewport (`MENU_VIEW_H`, y 62 to 320, in `src/ui_menu.cpp`), so
the scroll range is 294 − 258 = 36 px (`uiListMaxScroll()` in
`src/ui_list.cpp`), as the seven rows on `main` scroll. SETTINGS, the
seventh row, starts at y 314 with 6 px showing at scroll 0 and is reached
with a short drag. The drawing and the tables below are the six-row design
of 2026-11-13 and are to be redrawn with the seven rows; RX SIGNALS, 10
characters at scale 2, 118 px, ends at x 134 from x 16, within
`menuRowsFit()`'s rule with no status.

```
y   0 | MENU                            [  BACK  ]|  header 36 px; BACK 64 x 32
   40 | SIG GP0  AUX GP1  DSHOT600     [EDT OFF]  |  strip; chip 12 px, C_RED
   62 | BEEP THE ESC                    DISARMED  |  row 0: x 8..231, 40 px
  104 | AM32 CONFIG                               |  row 1
  146 | SERVO TESTER                              |  row 2
  188 | SERVO PROGRAMMER                          |  row 3: no status
  230 | SD LOG                           NO CARD  |  row 4
  272 | SETTINGS                         UNSAVED  |  row 5: ends at y 311
  320 +-------------------------------------------+
```

Six rows at a 42 px pitch take 252 px of the 258 px viewport: MENU never
scrolls. The scroll code stays for test registries, as today.

The order groups the rows by what sits on SIG ([decision 16](#decisions)):
the ESC (BEEP THE ESC, AM32 CONFIG), a servo (SERVO TESTER, SERVO
PROGRAMMER), then the bench (SD LOG, SETTINGS). Rows 0, 1 and 2 keep their
place; SETTINGS stays the sixth row, and SD LOG moves from the seventh row to
the fifth.

| Row | Label, scale 2 from x 16 | Ends at x | Widest status, scale 1, right to x 222 | Starts at x |
|---|---|---|---|---|
| BEEP THE ESC | 12 characters, 142 px | 158 | `DISARMED`, 8, 47 px | 175 |
| AM32 CONFIG | 11, 130 px | 146 | none | |
| SERVO TESTER | 12, 142 px | 158 | none | |
| SERVO PROGRAMMER | 16, 190 px | 206 | none: 8 px are left, less than one character | |
| SD LOG | 6, 70 px | 86 | `CARD ERROR`, 10, 59 px | 163 |
| SETTINGS | 8, 94 px | 110 | `UNSAVED`, 7, 41 px | 181 |

`menuRowsFit()` keeps its rule: label end plus 8 px at most the status start.
SERVO PROGRAMMER passes only with no status, so the tool rows lose their pin
status (`GP0`, `GP0+1`) and statuses report state only
([decision 18](#decisions)). The strip names both pins once.

| Strip | Text | Width |
|---|---|---|
| Caption | `SIG GP0  AUX GP1  DSHOT600`, 26 characters | 155 px, x 8..162 |
| Caption, widest | `SIG GP1  AUX GP0  DSHOT1200`, 27 | 161 px, x 8..168 |
| Caption while the claim is Restoring | `DSHOT RESTARTING ON SIG GP0`, 27 | 161 px |
| Chip | `EDT ON` on `C_GREEN`; `EDT OFF` on `C_RED` while eRPM arrives without EDT; `NO ESC` on `C_GRID` while no eRPM arrives: text 35, 41 or 35 px plus 12 px | x 185, 179 or 185 to 232 |

While the claim is not Free, every row that claims or changes the wiring is
drawn disabled under its normal status, and the strip says why. The words
`PINS BUSY` go from MENU ([decision 15](#decisions), item k).

## PROGRAMMER, the chooser

The SERVO PROGRAMMER row opens the chooser. It claims nothing, so a tap on an
entry is the first step that takes SIG. The chooser generalises Hitec's
family view, which goes. By the owner's decision of 2026-11-14 HITEC SERVO
takes SIG when it opens, before the family is chosen
([decision 31](#decisions)); here the Hitec entries open HITEC SERVO, so
their tap takes SIG, as the KST entry's does.

With SERVO SUPPLY at OWN:

```
y   0 | PROGRAMMER                      [  BACK  ]|
   40 | SIG GP0  SERVO SUPPLY OWN                 |  strip, C_DIM
   62 | [         KST / CHASERVO          ]       |  6, 62, 228 x 46
  112 | POWER THE SERVO UP AFTER THE TAP.         |  C_DIM
  142 | [         HITEC D SERIES          ]       |  6, 142, 228 x 46
  192 | D485HW, D645MW AND THE OTHERS             |  C_DIM
  204 | POWER THE SERVO UP BEFORE THE TAP.        |  C_DIM
  222 | [          HITEC CLASSIC          ]       |  6, 222, 228 x 46: disabled
  272 | HS-5XXX, HS-7XXX                          |  C_DIM
  284 | NEEDS THE CONVERTER OF WIRING PART C      |  C_AMBER
  296 | AND SETTINGS > SERVO SUPPLY GP28/29.      |  C_AMBER; ends at y 302
  320 +-------------------------------------------+
```

With SERVO SUPPLY at GP28 or GP29 the power lines read
`THE BENCH SWITCHES IT ON GP28.`, HITEC CLASSIC is live, and its amber lines
give way to that line at y 284.

| Text | Characters | Width, scale | From x 8 to |
|---|---|---|---|
| title `PROGRAMMER` | 10 | 118 px, 2 | 126 |
| `SIG GP0  SERVO SUPPLY GP28` | 26 | 155 px, 1 | 163 |
| `KST / CHASERVO`, `HITEC D SERIES` | 14 | 166 px, 2 | centred in 228 px |
| `HITEC CLASSIC` | 13 | 154 px, 2 | centred |
| `POWER THE SERVO UP AFTER THE TAP.` | 33 | 197 px | 205 |
| `POWER THE SERVO UP BEFORE THE TAP.` | 34 | 203 px | 211 |
| `THE BENCH SWITCHES IT ON GP28.` | 30 | 179 px | 187 |
| `D485HW, D645MW AND THE OTHERS` | 29 | 173 px | 181 |
| `HS-5XXX, HS-7XXX` | 16 | 95 px | 103 |
| `NEEDS THE CONVERTER OF WIRING PART C` | 36 | 215 px | 223 |
| `AND SETTINGS > SERVO SUPPLY GP28/29.` | 36 | 215 px | 223 |

The title is `PROGRAMMER`: `SERVO PROGRAMMER`, 190 px, would end at x 198,
past BACK at x 170. Wiring part C is the converter of
[the wiring sheet](../../wiki/Wiring.md).

An entry is a table line, so a new brand adds a line, not a MENU row:

```c
// ui_screen.h
struct UiProgEntry {
    const char     *label;     // scale 2, at most 18 characters
    const char     *models;    // scale 1, C_DIM, or null
    const UiScreen *screen;    // the tool the tap opens
    void          (*enter)();  // in place of screen->enter: sets the family
    bool          (*enabled)();            // null: always
    void          (*power)(char *b, size_t n);  // the power line
    const char     *why[2];    // C_AMBER lines while disabled
};

// ui_registry.cpp
constexpr UiProgEntry UI_PROGRAMMERS[] = {
    { "KST / CHASERVO", nullptr, &UI_KST, uiKstEnter, nullptr,
      kstPowerLine, { nullptr, nullptr } },
    { "HITEC D SERIES", "D485HW, D645MW AND THE OTHERS", &UI_HITEC,
      uiHitecEnterD, nullptr, hitecDPowerLine, { nullptr, nullptr } },
    { "HITEC CLASSIC", "HS-5XXX, HS-7XXX", &UI_HITEC, uiHitecEnterClassic,
      servoSupplyFitted, classicPowerLine,
      { "NEEDS THE CONVERTER OF WIRING PART C",
        "AND SETTINGS > SERVO SUPPLY GP28/29." } },
};
static_assert(progEntriesFit(UI_PROGRAMMERS), "a chooser line is too wide");
```

Three entries fill the page at an 80 px pitch. A fourth entry turns the
chooser into a list on `ui_list` with 80 px rows; that change is not part of
structure B. A tap opens the entry's screen through `uiOpenTool()` in
`ui.cpp`, the one place a screen opens another
([decision 19](#decisions)). RESTART DSHOT on the leave page returns to MENU,
as today.

## The leave page

One page replaces the leave page and the unplug page of KST SERVO and HITEC
SERVO, and the unplug page of SERVO TESTER. It is a widget, `ui_leave`, that
the three screens fill. Slice 1 builds the widget ([slice 1 as
built](#slice-1-as-built)); no screen draws it before slice 6. It has three
phases: before LEAVE, releasing, and released. Its instructions are numbered
steps in one style: scale 1 from x 8, the current step in `C_AMBER` with a
3 px `C_AMBER` bar at x 0 (drawn `#` below), the other steps in `C_DIM`.

KST SERVO, servo on its own supply, two changes not written, before LEAVE:

```
y   0 | KST SERVO                       [  BACK  ]|  BACK drawn disabled
   40 | V6  SIG GP0                               |  strip, C_DIM
   66 |#1 POWER THE SERVO OFF, THEN LEAVE.        |  step 1, current
   78 |#  IT STAYS IN PROGRAMMING MODE UNTIL      |
   90 |#  ITS SUPPLY IS OFF.                      |
  104 | 2 UNPLUG THE SERVO.                       |  step 2, C_DIM
  116 |   DSHOT RESTARTS ON SIG GP0 AND READS     |
  128 |   AS A 970 US PULSE AT 1000 HZ.           |
  142 | 3 RESTART DSHOT.                          |  step 3, C_DIM
  158 | 2 CHANGES NOT WRITTEN.                    |  notes, C_AMBER, 12 px pitch,
  170 | THE SERVO IS IN DOUBT: RESTORE FIRST.     |  at most 4 lines, end y 201
  210 |                                           |  hold bar, 3 px
  214 | [   STAY   ] [ HOLD TO REBOOT ]           |  6,214,110x46; 122,214,112x46
  268 | [              LEAVE               ]      |  6,268,228x46, scale 2
  320 +-------------------------------------------+
```

Releasing, after the tap on LEAVE:

```
   66 | 1 POWER THE SERVO OFF, THEN LEAVE.        |  C_DIM: done
  104 |#2 UNPLUG THE SERVO.                       |  current from here on
  158 | SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.         |  or RELEASING THE PIN, C_DIM
  214 | [NEXT SERVO]                              |  drawn disabled; right hidden
  268 | [          RESTART DSHOT           ]      |  drawn disabled
```

Released, once `pinClaimUnplugDue()`:

```
  104 |#2 UNPLUG THE SERVO.                       |
  158 | NEXT SERVO: POWER IT AFTER THE TAP.       |  C_DIM
  214 | [NEXT SERVO]                              |  live
  268 | [          RESTART DSHOT           ]      |  live 1000 ms after this phase
```

The variants:

| Screen | Step 1 | Right button at y 214 | Left button | NEXT SERVO line |
|---|---|---|---|---|
| KST, OWN | `1 POWER THE SERVO OFF, THEN LEAVE.` and two detail lines | none | STAY, then NEXT SERVO | `NEXT SERVO: POWER IT AFTER THE TAP.` |
| KST, GP28 or GP29 | `1 LEAVE SWITCHES THE SUPPLY OFF.` | HOLD TO REBOOT, 1000 ms; `NO CARD: REBOOT LOSES THE BACKUP IN` / `MEMORY.` as notes without a card | STAY, then NEXT SERVO | none: the bench powers it |
| HITEC D SERIES, OWN | `1 LEAVE RESTARTS THE SERVO.` | READ AGAIN | STAY, then NEXT SERVO | `NEXT SERVO: POWER IT BEFORE THE TAP.` |
| HITEC, GP28 or GP29 | `1 LEAVE SWITCHES THE SUPPLY OFF.` | READ AGAIN | STAY, then NEXT SERVO | none |
| KST or Hitec, no servo in the mode | none: the page opens releasing, steps 2 and 3 numbered 1 and 2 | none | NEXT SERVO | as above |
| SERVO TESTER | none: BACK on SERVO TESTER is the leave; the page opens releasing | none | BACK TO SERVO | none |

| Text | Characters | Width at scale 1 | Room |
|---|---|---|---|
| `1 POWER THE SERVO OFF, THEN LEAVE.` | 34 | 203 px | 37 characters |
| `  IT STAYS IN PROGRAMMING MODE UNTIL` | 36 | 215 px | 37 |
| `  DSHOT RESTARTS ON SIG GP0 AND READS` | 37 | 221 px | 37: ends at x 229 |
| `  AS A 970 US PULSE AT 1000 HZ.` | 31 | 185 px | 37 |
| `1 LEAVE SWITCHES THE SUPPLY OFF.` | 32 | 191 px | 37 |
| `1 LEAVE RESTARTS THE SERVO.` | 27 | 161 px | 37 |
| `THE SERVO IS IN DOUBT: RESTORE FIRST.` | 37 | 221 px | 37 |
| `NO CARD: REBOOT LOSES THE BACKUP IN` | 35 | 209 px | 37 |
| `SUPPLY OFF FOR 2.0 S: 1.4 S LEFT.` | 33 | 197 px | 37 |
| `NEXT SERVO: POWER IT BEFORE THE TAP.` | 36 | 215 px | 37 |
| `STAY`, `NEXT SERVO`, `BACK TO SERVO` | 4, 10, 13 | 23, 59, 77 px | 110 px button |
| `HOLD TO REBOOT`, `READ AGAIN` | 14, 10 | 83, 59 px | 112 px button |
| `LEAVE`, `RESTART DSHOT` at scale 2 | 5, 13 | 58, 154 px | 228 px button |

The buttons:

- **STAY** returns to the field list, or to the search after a lost servo.
  It is today's BACK on the leave page.
- **LEAVE** is a tap and runs today's leave: KST removes the session's record
  when the servo is in the mode and not in doubt, then `endClaim()`. The
  release order of [pin-claim.md](pin-claim.md#sequence) step 5 is
  unchanged. The button then reads RESTART DSHOT, drawn disabled. LEAVE is
  drawn disabled while the screen writes to the device: the screen clears
  `leaveEnabled` then, as today's KST and Hitec leave pages draw LEAVE
  disabled while `s_writing` is set (`ui_kst.cpp` and `ui_hitec.cpp`
  `drawLeave()`). LEAVE takes only a press that lands while it is enabled
  and is still enabled at the lift; a press that landed while it was
  disabled draws no pressed look and does nothing on its lift.
- **NEXT SERVO** and **BACK TO SERVO** call `pinClaimKeep()`, live once
  `pinClaimUnplugDue()` is true: today's BACK on the unplug page. KST and
  Hitec start a new session; Hitec keeps the family chosen on the chooser.
- **RESTART DSHOT** calls `pinClaimRestart()` and returns to MENU. It takes a
  press that lands at least 1000 ms (`UI_HOLD_MS`) after the released phase
  began ([decision 21](#decisions)).
- **HOLD TO REBOOT** and **READ AGAIN** keep today's behaviour and are hidden
  from the tap on LEAVE on.

## The field lists

KST SERVO, HITEC SERVO and AM32 CONFIG share one footer and one editor bar.
SERVO SET shares the editor bar.

```
y  40 | V6  BACKUP IN MEMORY           [PROG MODE]|  strip; chip C_BLUE
   62 | TUNE                                      |  rows 40 px, 42 px pitch
      | DEAD BAND                         2.2 US  |  changed: C_CYAN, 3 px C_CYAN bar
  ... |                                           |  viewport y 62..215
  218 | [ - ]  DEAD BAND  2.0 US ->      [ + ]    |  - 6,218,44x44; + 190,218,44x44
  234 |            2.2 US                         |  scale 2, C_CYAN
  252 |        SWIPE = COARSE                     |  C_GRID, while no rule note
  264 |                                           |  no hold bar on the list
  268 | [ WRITE... ] [ REVERT ] [ FILES ]         |  6/110, 122/52, 180/54; h 46
  320 +-------------------------------------------+
```

AM32 CONFIG has HEX in place of FILES. On KST, `READ ONLY` or `IN DOUBT`
replaces `WRITE...`, drawn disabled, as today; on Hitec `READ AGAIN` replaces
it after a write that left the image unknown.

With edits not written, the first BACK on AM32 CONFIG arms the guard:

```
   40 | 2 CHANGES NOT WRITTEN.                    |  y 42, C_AMBER, 131 px
   50 | BACK AGAIN DISCARDS THEM.                 |  y 52, C_AMBER, 149 px
```

The two lines replace the strip's caption and chip until a press lands
anywhere but BACK. With one change: `1 CHANGE NOT WRITTEN.` and
`BACK AGAIN DISCARDS IT.`; on SERVO SET: `1 CHANGE NOT APPLIED.` (21
characters, 125 px).

Editor bar text, 136 px between the buttons (x 52 to 187), 22 characters at
scale 1:

| Text | Characters | Width |
|---|---|---|
| `PWM FREQ  24KHZ ->` | 18 | 107 px, scale 1 |
| `25KHZ` | 5 | 58 px, scale 2 |
| `SWIPE = COARSE` | 14 | 83 px, scale 1 |

A top line wider than 136 px drops the label and keeps `24KHZ ->`, as
`drawList()` in `ui_kst.cpp` does today.

## AM32 WRITE

`WRITE...` opens the page. It lists the change from the ESC as read
(`s_original`) to the edit (`s_eeprom`), field by field:

```
y   0 | AM32 WRITE                      [  BACK  ]|
   40 | 2 CHANGES                                 |  strip, C_DIM
   62 | PWM FREQ                 24KHZ -> 25KHZ   |  label C_TEXT; old C_DIM,
   82 | STARTUP POWER                100 -> 104   |  new C_CYAN; 20 px pitch
  ... | AND 3 MORE                                |  after 4 lines, C_DIM
  196 |                                           |  state lines, 12 px pitch
  236 | BACKUP FIRST: BACKUP/AM001.TXT            |  C_DIM
  264 |                                           |  hold bar, 3 px, C_LIME
  268 | [          HOLD TO WRITE           ]      |  6,268,228x46, scale 2
  320 +-------------------------------------------+
```

Writing: `WRITING: DO NOT UNPLUG.` in `C_AMBER` at y 196, BACK drawn disabled.
The result, then OK in the same rectangle:

```
  196 | WRITE VERIFIED. BACKUP AM018              |  C_GREEN; 28, 167 px
  208 | POWER-CYCLE THE ESC TO APPLY.             |  C_TEXT; 29, 173 px
  268 | [               OK                 ]      |
```

A failure (`WRITE FAILED: ...`, `VERIFY READ: ...`, `DIFF @12: GOT 00 WANT
01`) is `C_AMBER` and wraps at 37 characters. Whether the ESC runs the new
settings without a power cycle is unknown: `am32BlEnd()` releases the pin and
sends no command to start the firmware. The line is right either way. `main`
draws the result and the power-cycle line (`AM32_UI_POWER_CYCLE`) in the
result panel of AM32 CONFIG, with OK at {70, 200, 100, 36}; the write page
takes them over.

## SETTINGS

The WIRING section with both pins live, at 120 px of scroll:

```
y   0 | SETTINGS                        [  BACK  ]|
   40 | WIRING                                    |  caption, C_TEXT
   60 | SIG PIN            GP0  [  -  ] [  +  ]   |  value y 68, scale 2, to x 132
      |               J5 PIN 2                    |  y 87, scale 1, to x 132, C_DIM
  110 | DSHOT              600  [  -  ] [  +  ]   |
  160 | KISS TELEM              [    OFF     ]    |  toggle
  210 | AUX PIN            GP1  [  -  ] [  +  ]   |  live whatever KISS is;
  240 +-------------------------------------------+  cut: 30 px visible
  242 | LINK                    998/S  0% ERR     |  fixed
  254 | UNSAVED - HOLD SAVE TO KEEP               |  fixed
  264 |                                           |  hold bar, C_LIME
  268 | [    HOLD TO SAVE    ] [ DEFAULTS ]       |  6,268,150x46; 162,268,72x46
  320 +-------------------------------------------+
```

At 230 px of scroll, AUX PIN and SERVO SUPPLY as a core section:

```
   40 |                                           |  DSHOT, cut: 4 px visible
   50 | KISS TELEM              [    OFF     ]    |
  100 | AUX PIN            GP1  [  -  ] [  +  ]   |
      |               J5 PIN 1                    |
  150 | SERVO SUPPLY                              |  caption
  170 | SERVO SUPPLY       OWN  [  -  ] [  +  ]   |  OWN, GP28, GP29
      |              J4 PIN 11                    |  with GP28; J4 PIN 12 with GP29
  220 | CURRENT SENSE           [    NONE    ]    |  toggle; cut: 20 px visible
  240 +-------------------------------------------+
```

Content y in the list, from the caption ESC TESTER at 0: WIRING at 120, SIG
PIN 140, DSHOT 190, KISS TELEM 240, AUX PIN 290, caption SERVO SUPPLY 340,
SERVO SUPPLY 360, CURRENT SENSE 410, DISPLAY 460, CONTRAST 480, BACKLIGHT
530, BOARD 580, BUILT FOR 600, VERSION 650, DIAGNOSTICS 700, LAST RESTART
720, UPTIME 770, end 820: 620 px of scroll at most. That is today's layout
with the SERVO SUPPLY section, SERVO SUPPLY and CURRENT SENSE, in the same
place, so `SettingsRowId` keeps its order.

| Row | Label, scale 1 from x 8 | Ends at x | Value, scale 2, ends x 132 | Starts at x | Connector line, scale 1, ends x 132 |
|---|---|---|---|---|---|
| SIG PIN | 7 characters, 41 px | 49 | `GP0`, 34 px | 98 | `J5 PIN 2`, 47 px, from x 85 |
| DSHOT | 5, 29 px | 37 | `1200`, 46 px | 86 | none |
| AUX PIN | 7, 41 px | 49 | `GP1`, 34 px | 98 | `J5 PIN 1`, 47 px, from x 85 |
| SERVO SUPPLY | 12, 71 px | 79 | `GP28`, 46 px | 86 | `J4 PIN 11`, 53 px, from x 79 |

The label sits at y+18 to y+24 of the 44 px row, the value at y+8 to y+21 and
the connector line at y+27 to y+33. The value shares the label's rows, so
`settingsSectionFits()` keeps its 4 px rule against the value; the connector
line shares none of them. The note `NOTHING SAVED: DEFAULTS IN USE` is 30
characters, 179 px.

## The tester

```
y   0 |[  SAFE  ] DSHOT600             9.90V |  badge 4,3,92x20; x 104, y 3
      |           SD     BOARD               |  x 104, y 14; BOARD x 140, y 14
  102 | ERPM 84210                           |
  114 | 14 POLES             SWIPE = THROTTLE|  x 10, C_TEXT; underline y 122
  236 | THROTTLE  MAX 20%                 0% |  MAX x 88, C_TEXT; underline y 244
  248 | [ ARM TO ENABLE                    ] |  or EJECT THE USB DRIVE TO ARM
  283 | [HOLD TO ARM] [LATCH] [  MENU  ]     |  6/104, 116/54, 176/58; h 33
  320 +--------------------------------------+
```

| Text | Characters | Width | Place |
|---|---|---|---|
| `DSHOT600`, widest `DSHOT1200` | 8, 9 | 47, 53 px | x 104..156, y 3 |
| `SDERR`, widest of the log state | 5 | 29 px | x 104..132, y 14 |
| `BOARD` | 5 | 29 px | x 140..168, y 14, C_DIM |
| `9.99V`, widest: `uiBoardVoltText()` clamps there; a 200 kΩ/100 kΩ divider reads at most 9.90 V | 5 | 58 px, scale 2 | x 178..235, y 7 |
| `IDLE 5s`, which displaces BOARD and the voltage | 7 | 82 px, scale 2 | x 154..235 |
| `14 POLES` | 8 | 47 px | x 10..56 |
| `MAX 100%` | 8 | 47 px | x 88..134 |
| `LATCHED`, the caption while LATCH is on | 7 | 41 px | x 8..48 |
| `EJECT THE USB DRIVE TO ARM` | 26 | 155 px | x 16..170 |
| `LATCH` | 5 | 29 px | in 54 px |

`BOARD` and the track's `EJECT THE USB DRIVE TO ARM`, with HOLD TO ARM drawn
disabled while a USB host holds the card, are on `main` (decision 15 b and
c). The two readouts are targets while disarmed: `TesterCtl::Poles` at
{4, 106, 72, 22} and `TesterCtl::Max` at {82, 230, 60, 22}. Each opens
SETTINGS through `uiOpenTool()`, which runs the disarm of `uiOpenMenu()`
first. While armed they are inert and drawn without the underline: the pole
target lies in the throttle pad and the MAX target 8 px into the throttle
track's grab band ([decision 20](#decisions)).

## SERVO TESTER

```
y   0 | SERVO TESTER                    [  BACK  ]|  title 142 px, ends x 150
   40 | SIG GP0  50 HZ  1000-2000 US       [SAFE] |  y 42, C_DIM; chip 12 px
   50 | GP28 SUPPLY STAYS OFF HERE.               |  y 52, C_AMBER; supply set only
   62 |                                           |  dial band, as today
```

| Text | Characters | Width | Ends at x | Room |
|---|---|---|---|---|
| `SIG GP0  50 HZ  1000-2000 US` | 28 | 167 px | 175 | the chip at x 197 |
| `SIG+AUX  50 HZ  1000-2000 US`, two servos | 28 | 167 px | 175 | |
| `GP28 SUPPLY STAYS OFF HERE.` | 27 | 161 px | 169 | 30 characters, x 193 |

The second line ends at y 58, before the dial band at y 62. `main` draws it,
`GP28 SUPPLY STAYS OFF HERE.` under the caption `GP0  50 HZ  1000-2000 US`,
through `uiStripTwoLines()` (decision 12); structure B adds `SIG` to the
caption (decision 10) and the title (decision 13).

## Other views

| View | Change |
|---|---|
| KST WRITE, HITEC WRITE | strip `n CHANGE(S)` (KST read `n FIELD(S)`, Hitec `n VALUE(S)`); at most 4 lines, then `AND n MORE` (Hitec showed 6); old value `C_DIM`, new value `C_CYAN`; BACK disabled while writing |
| KST FILES, HITEC FILES | `HOLD TO RESTORE` becomes `RESTORE...`, a tap that opens the restore page, whose hold restores |
| AM32 HEX | title `AM32 HEX`; BACK returns to the list; the footer's third button reads `FIELDS`, as today |
| SERVO SET | footer `HOLD TO APPLY` {6, 268, 150, 46} and `DEFAULTS` {162, 268, 72, 46}; editor buttons 44 px |
| SD LOG | buttons from x 6: START {6, 240, 228, 44}, RETRY {6, 288, 110, 30}, USB DRIVE {122, 288, 112, 30}; labels `C_TEXT`; the read counter `C_LIME` |
| HITEC family view | removed: the chooser carries its two choices |

## Decisions

Decisions of the owner, 2026-11-13:

1. **Six rows.** MENU has six rows and never scrolls. The order is this
   note's choice, decision 16. Revised by decision 33 of 2026-11-14: seven
   rows, and MENU scrolls 36 px.
2. **SERVO PROGRAMMER.** KST SERVO and HITEC SERVO fold into one row that
   opens a chooser: KST / CHASERVO, HITEC D SERIES, HITEC CLASSIC. CLASSIC is
   drawn disabled without the switched supply and names the path to enable
   it and the converter of wiring part C. Future brands add chooser entries.
3. **One leave page.** The leave page and the unplug page merge. Its button
   reads LEAVE, then RESTART DSHOT in the same rectangle once the pin is
   released; the instructions use one style.
4. **SETTINGS from the tester.** A tap on the MAX or the pole-count readout
   opens SETTINGS through the disarming path MENU uses.

Rules for the audit findings. Each lists the code it touches, by file and
function, and what it breaks. Test counts are lines that `grep -cE` finds
for the named identifiers at 2940f9c; "rewritten" counts the lines that
change.

5. **BACK goes one level up, or is drawn disabled** (item 1). Header BACK
   leaves a sub-view for its parent and a tool's main view for its leave
   page or MENU. Where it would do nothing, or something a label should say,
   it is drawn disabled: on the leave page in every phase, and while a write
   runs. Every other effect it had is a labelled button: STAY, NEXT SERVO,
   BACK TO SERVO. BACK during a KST or Hitec write no longer queues the
   leave. [pin-claim.md](pin-claim.md#decisions) decision 4 holds: no exit
   control undoes anything dangerous, and the page still guards against
   forgetting. Reason: a control whose effect depends on the view gets
   guessed; a disabled BACK says there is nothing behind it.
   - Code: `ui_widgets.{h,cpp}` `uiHeader()` takes BACK's flag, `backEnabled`,
     default true, on `main`, and each screen gates its `uiBackTapped()` on the
     same value (`backActs()` on SERVO TESTER, KST SERVO and HITEC SERVO);
     `ui_leave.{h,cpp}`, built in slice 1; `ui_kst.cpp` `drawLeave()`,
     `drawUnplug()`, `endClaim()`, `uiKstControls()`, `uiKstTick()`, `opEnd()`
     (the leave queue goes); `ui_hitec.cpp` the same functions; `ui_servo.cpp`
     `drawUnplug()`, `uiServoControls()`, `uiServoTick()`; `ui_kst.h` `KstView`
     loses `Unplug`, `KstCtl` gains `Stay`, `NextServo`; `ui_hitec.h` the same;
     `ui_servo.h` `ServoCtl::BackToServo`.
   - Tests: 129 lines name the leave and unplug views and controls
     (test_kst_ui.cpp 70, test_hitec_ui.cpp 40, test_servo.cpp 8,
     test_menu.cpp 7, test_ids.cpp 3, test_ui.cpp 1), about 90 rewritten,
     with the three `leaveAll()`-style helpers. The KST test of BACK in every
     view (`testLeaveAndBack()`, 47 lines) and Hitec's
     `testStallAndBack()` and `testIdAndBack()` are rewritten around the
     buttons.
   - Docs: README "KST servo programming" and "Hitec servo programming",
     "Using it" of each, and "Servo tester", "Using it"; wiki
     KST-Programming / KST-Programmierung, Hitec-Programming /
     Hitec-Programmierung, Servo-Tester / Servotester, The-Screens /
     Die-Bildschirme; pin-claim.md "Restoring DShot" (the page) and decision
     4 (a revision line); kst-programming.md "Leave"; hitec-programming.md
     "Leave"; servo-tester.md "The unplug page".
6. **A tap opens the write page; only the write page holds** (item 2). On
   AM32, KST and Hitec the list's button reads `WRITE...` and opens the write
   page on a tap; the page lists every change, old -> new, and its
   HOLD TO WRITE, 1000 ms on `ui_hold`, writes. FILES' HOLD TO RESTORE
   becomes `RESTORE...` by the same rule. AM32 gains the write page and the
   result on it; its editor bar shows the old value as KST's does. Reason:
   one rectangle had two meanings, and AM32 wrote a motor controller with no
   page that names what changes.
   - Code: `ui_am32.cpp` `handleListTouch()`, `drawList()`, `drawWriting()`
     (becomes the page), `doWrite()` (result text), `uiAm32Tick()`,
     `uiAm32Controls()`, `ui_am32.h` `Am32Ctl::WriteHold`; `ui_kst.cpp`
     `listTouch()`, `drawList()`, `filesTouch()`, `drawFiles()`;
     `ui_hitec.cpp` `listTouch()`, `drawList()`, `openWrite()`,
     `filesTouch()`, `drawFiles()`; the `KstCtl::Write`, `HitecCtl::Write`
     and `Restore` docs.
   - Tests: 161 lines name the write controls (test_kst_ui.cpp 76,
     test_hitec_ui.cpp 51, test_ui.cpp 28, test_ids.cpp 6); rewritten: the
     71 holds on `Write` become taps (KST 32, Hitec 23, AM32 16), and the
     AM32 hold-timing tests (test_ui.cpp 3340 to 3490) move to
     `Am32Ctl::WriteHold`. New: AM32's page lists the change, OK on the
     wide rectangle, the press that opened the page is not timed.
   - Docs: README "AM32 ESC configuration" ("Editing", "Verification"),
     "KST servo programming", "Hitec servo programming"; wiki
     AM32-Configuration / AM32-Konfiguration, KST-Programming /
     KST-Programmierung, Hitec-Programming / Hitec-Programmierung,
     The-Screens / Die-Bildschirme; kst-programming.md "Field list",
     "Write", "Files"; hitec-programming.md "Write", "Files".
7. **No BACK loses an edit the screen did not name in amber** (item 3).
   Where BACK drops edits (AM32 CONFIG's list, SERVO SET with a pending TYPE
   or RATE), the first BACK arms a guard: the strip's two amber lines name
   the count, and a second BACK drops them. Any other press disarms the
   guard; there is no timer. Where BACK opens the leave page (KST, Hitec),
   the page names the count and LEAVE drops them. SETTINGS keeps its rule:
   its edits are live settings, never dropped by BACK, and the note line
   names them in amber whenever they differ from flash. Reason: one rule
   with three outcomes that all name the loss first.
   - Code: `ui_widgets.{h,cpp}` `UiBackGuard`, `uiBackGuard()`,
     `uiBackGuardDraw()`, `uiStripTwoLines()`, built in slice 1;
     `ui_am32.cpp` `uiAm32Tick()`, `drawHeader()`;
     `ui_servo.cpp` `uiServoTick()` (the SET branch), `drawSet()`.
   - Tests: test_servo.cpp has 24 lines on `s_pending` and APPLY, about 6
     rewritten where a BACK with a pending change now takes two taps;
     test_ui.cpp, 3 lines on `anyDirty`. New: guard armed, disarmed by
     another press, second BACK drops.
   - Docs: README "AM32 ESC configuration", "Servo tester"; wiki
     AM32-Configuration / AM32-Konfiguration, Servo-Tester / Servotester;
     servo-tester.md "SET".
8. **Changed is cyan, with a bar** (item 4). In the AM32, KST and Hitec
   lists and SERVO SET's pending row, a value that differs from the device
   is `C_CYAN`, with a 3 px `C_CYAN` bar at the row's left edge. A change
   that breaks a soft rule draws the bar in `C_AMBER` instead. Selection
   changes the row fill (`C_PANEL`) and the label (`C_TEXT`), never the
   value's colour. The editor bar's value and the write page's new value
   follow the same rule. Hitec's `*` goes: the bar is the shape cue.
   `ui_list` draws the row: `uiListDrawField()` with a `UiValue` state
   (Plain, Changed, ReadOnly, Odd) and a `UiEdge` (None, Changed, Rule).
   Reason: amber is the warning colour, and lime is a live reading. The
   owner chose `C_CYAN` on every screen on 2026-11-14 (decision 26).
   - Code: `ui_list.{h,cpp}` `uiListDrawField()`, built in slice 1 with the
     "changed" colour as a parameter; `ui_am32.cpp`
     `drawRow()`, `drawList()`; `ui_kst.cpp` `drawFieldRow()`, `drawList()`,
     `drawWrite()`; `ui_hitec.cpp` `drawFieldRow()`, `drawList()`,
     `drawWrite()`; `ui_servo.cpp` `drawSetRow()`, `drawSet()`.
   - Tests: test_kst_ui.cpp 328 counts `C_LIME` on the result page:
     rewritten to `C_CYAN`. No test reads a row's value colour or the `*`
     today. New: one test per list for the three states.
   - Docs: README "AM32 ESC configuration", "KST servo programming", "Hitec
     servo programming"; wiki AM32-Configuration / AM32-Konfiguration,
     KST-Programming / KST-Programmierung, Hitec-Programming /
     Hitec-Programmierung (the `*` and "lime"); kst-programming.md "Field
     list"; hitec-programming.md "Field list".
9. **SERVO SUPPLY is a core section** (item 5). The section holds two
   rows, SERVO SUPPLY and CURRENT SENSE. `ROWS_SUPPLY`, `SEC_SUPPLY`,
   `supplyValue()`, `supplyStep()`, `senseValue()`, `senseOn()`,
   `senseToggle()` and `senseCaptionText()` move from `ui_kst.cpp` to
   `ui_settings.cpp`; `buildItems()` places the section after WIRING, so
   `SettingsRowId` keeps its order and the stored block does not change.
   `UI_KST.settings` becomes null. Reason: Hitec depends on the row, and
   `buildItems()` finds tool sections through MENU's entries, so the section
   would vanish when KST SERVO leaves MENU in decision 2. This decision comes
   before the chooser.
   - Code: `ui_kst.cpp` (the section and `UI_KST`), `ui_settings.cpp`
     `buildItems()`, the core section table; `ui_settings.h` the
     `SettingsRowId` docs of `ServoSupply` and `CurrentSense` and of the
     enum, which name KST's section.
   - Tests: test_ui.cpp `testSettingsServoSupply()` (lines 951 to 997, 9 lines
     name the row) keeps its taps, its doc line "KST's section" changes;
     test_menu.cpp 554 (`uiSettingsSections()`) counts the same sections.
   - Docs: README "Using it", the SETTINGS table; wiki The-Screens /
     Die-Bildschirme; kst-programming.md "SETTINGS row".
10. **SIG and AUX** (item 6). Screens name a pin by its role and number
    together, `SIG GP0`, and by the role alone where a strip has no room;
    SETTINGS rows are SIG PIN and AUX PIN, with the connector pin under the
    value (`J5 PIN 2`, `J5 PIN 1`). Both rows are live whatever KISS is. On
    this board, with two free GPIOs, either stepper swaps SIG and AUX, so
    the two never share a GPIO; `settingsValidate()` moves AUX off SIG in a
    stored block. KISS TELEM stays a toggle and listens on AUX. Reason: the
    servo tester drives servo B on AUX whether or not KISS is on, and SIG
    could land on AUX's GPIO while AUX was locked at `--`; the wiring sheet
    already says SIG and AUX. This reverses issue #23's lock deliberately:
    the row now sets a pin that is in use.
    - Code: `ui_settings.cpp` `pinValue()`, `pinStep()`, `kissPinValue()`,
      `kissPinStep()`, `stepPinAvoiding()` (becomes `stepPinSwapping()`),
      `ROWS_WIRING`, `drawItem()`, `settingsSectionFits()`;
      `ui_screen.h` `SettingsRow` gains `void (*sub)(char *, size_t)`, the
      connector line; `ui_settings.h` `SettingsRowId::Pin` and `KissPin`
      become `Sig` and `Aux`; `settings.cpp` `settingsValidate()`;
      `ui_menu.cpp` `drawMenu()`; messages in `ui_servo.cpp` `setNote()`
      and `claimMessage()`, `ui_kst.cpp` `mainBody()`, `ui_hitec.cpp`
      `mainText()`, `ui_am32.cpp` `drawConnect()`. The refusal
      `ESC AND KISS PIN ARE THE SAME GPIO.` becomes unreachable through the
      UI and stays as a backstop, worded `SIG AND AUX ARE THE SAME GPIO.`
    - Tests: 34 lines name the rows or the message (test_menu.cpp 15,
      test_ui.cpp 13, test_ids.cpp 5, test_servo.cpp 1); about 50 rewritten,
      most of them test_ui.cpp 915 to 945, whose premise (KISS PIN does not
      act while KISS is off) is reversed, and test_ids.cpp 109 to 116.
      test_servo.cpp `testAuxRefused()` keeps the backstop by configuring
      the pins directly.
    - Docs: README "Pin map", "Wiring the ESC", "Using it" (SETTINGS),
      "Servo tester"; wiki Wiring / Verdrahtung, The-Screens /
      Die-Bildschirme, Servo-Tester / Servotester ("the KISS pin"),
      Telemetry / Telemetrie, First-Run / Erste-Schritte, Troubleshooting /
      Fehlersuche; pin-claim.md "The two pins" and "When the pin is
      unavailable"; kiss-telemetry.md where it names the KISS pin.
11. **One meaning per colour** (item 7). The roles, defined once in
    `ui_widgets.h` as `UI_COL_*` names for the `C_*` palette entries:

    | Role | Colour | Uses |
    |---|---|---|
    | safe, done | `C_GREEN` | SAFE badge and chip, EDT ON chip, WRITE VERIFIED, SAVED |
    | live reading | `C_LIME` | RPM, throttle fill, pulse readout, servo A's horn, hold progress bars, spinners, LINK, SD read counter, sweep count |
    | output on, failure | `C_RED` | ARMED, RUNNING, STOP, DISARM, a hold's fill while it runs, REC, save failed, a write in doubt |
    | warning | `C_AMBER` | text to read before the next action, the current step, soft-rule bars, REFUSED and EJECT fills, REVERT and DEFAULTS labels |
    | alert | `C_MAGENTA` | ESC status alert, STALLED |
    | changed | `C_CYAN` | a value edited and not yet written, applied or saved; its bar |
    | latched | `C_BLUE` | LATCH on, a toggle that is on, the PROG MODE chip, servo B |
    | neutral | `C_TEXT`, `C_DIM`, `C_GRID` | labels, navigation and command buttons, captions, source tags |

    What each existing use becomes:

    | Use today | Colour today | Becomes |
    |---|---|---|
    | KST and Hitec `MODE` chip | `C_GREEN` | `PROG MODE` on `C_BLUE` |
    | MENU `EDT OFF` chip, eRPM without EDT | `C_RED`, round | `C_RED`: a failure; every chip square, 12 px tall |
    | MENU `NO ESC` chip, no eRPM | `C_GRID`, round | `C_GRID`, square |
    | tester KISS source tag | `C_CYAN` | `C_TEXT`; the EDT tag stays `C_DIM` |
    | tester DS rate with EDT requested | `C_CYAN` | `C_TEXT`; `C_DIM` without |
    | tester `SWIPE = THROTTLE` while armed | `C_CYAN` | `C_TEXT` |
    | tester `MAX 20%` | `C_AMBER` | `C_TEXT`, underlined |
    | AM32 `HEX` / `FIELDS` label | `C_CYAN` | `C_TEXT` |
    | AM32 changed value and bar | `C_AMBER` | `C_CYAN` |
    | AM32 selected value | `C_LIME` | by state, decision 8 |
    | KST, Hitec changed value | `C_LIME` | `C_CYAN` |
    | KST, Hitec, AM32 headline (`POWER UP`, `POWER-CYCLE THE ESC`) | `C_LIME` | `C_AMBER`: the current step |
    | SD LOG `RETRY`, `USB DRIVE` labels | `C_CYAN` | `C_TEXT` |
    | SD LOG read counter | `C_CYAN` | `C_LIME` |
    | SETTINGS section captions | `C_CYAN` | `C_TEXT` |
    | SETTINGS `KISS PIN` value | `C_CYAN` | `C_TEXT` |
    | SETTINGS `ESC PIN` value | `C_LIME` | `C_TEXT` |
    | SETTINGS `MAX THROTTLE` value | `C_AMBER` | `C_TEXT` |
    | SETTINGS `SAVED` | `C_LIME` | `C_GREEN` |
    | SETTINGS `HOLD TO SAVE` while held | `C_BLUE` | `C_RED`, as every hold |
    | servo B horn, `B` label, B selector fill | `C_CYAN` | `C_BLUE` |
    | servo SPEED fill | `C_CYAN` | `C_DIM` |
    | servo MIN, CENTRE, MAX, SWEEP, SET labels | `C_CYAN` | `C_TEXT` |
    | splash `JuWi made` | `C_CYAN` | unchanged: a mark, not a state |

    Reason: a colour with several meanings carries none of them, and a chip
    that looks like a button gets tapped.
    - Code: `theme.h` (the role comments), `ui_widgets.{h,cpp}` `uiChip()` and
      the role names (`UI_COL_*`: six of the seven roles built in slice 1; the
      "changed" role, `C_CYAN` by decision 26, is not built); `ui.cpp`
      `drawSourceTag()`,
      `drawStatusBar()`, `drawRpm()`, `drawThrottle()`; `ui_menu.cpp`
      `drawMenu()`; `ui_settings.cpp` `drawItem()`, `noteText()`, `drawAll()`
      and the value functions; `ui_sdlog.cpp` the button and counter draws;
      `ui_servo.cpp` `drawTop()`, `drawDial()`, `drawReadout()`, `drawSpeed()`,
      `drawButtons()`; `ui_am32.cpp` `drawList()`, `drawConnect()`; `ui_kst.cpp`
      `drawMain()`, `drawList()`; `ui_hitec.cpp` `drawMain()`, `drawList()`.
    - Tests: 14 lines name these colours in screen tests (test_ui.cpp 12,
      test_servo.cpp 2); rewritten: test_ui.cpp 844 and 857 (DS rate
      cyan), `testScreenChangeResidue()` (2885 to 2912), which uses the
      SETTINGS captions' cyan as its marker and needs another, test_menu.cpp
      421 to 442 (5 lines, SAVED in `C_LIME`).
    - Docs: README "Using it" (tester, MENU), "Telemetry and logging" (the
      KISS tag); wiki The-Screens / Die-Bildschirme, Telemetry / Telemetrie,
      SD-Logging / SD-Aufzeichnung, Servo-Tester / Servotester, the KST and
      Hitec pages ("MODE").
12. **The servo tester names the switched supply** (item 8). With SERVO
    SUPPLY at GP28 or GP29 the strip's second line reads
    `GP28 SUPPLY STAYS OFF HERE.` in `C_AMBER`. Built on `main`.
    [servo-tester.md](servo-tester.md#decisions) decision 20 holds: the
    tester never drives the enable pin. Reason: with a converter fitted the
    servo is unpowered here unless it has a supply of its own, and nothing on
    the screen said so.
    - Code: `ui_servo.cpp` `drawTop()`, through `uiStripTwoLines()`.
    - Tests: test_servo.cpp checks the line with GP28 and GP29, running and
      SAFE, and none without a switch.
    - Docs: README "Servo tester"; wiki Servo-Tester / Servotester;
      servo-tester.md "SERVO": written.
13. **A title names its screen** (item 9). A tool's main view carries its
    MENU label, or the chooser's tool title: SERVO TESTER, AM32 CONFIG, KST
    SERVO, HITEC SERVO. A sub-view adds a word: SERVO SET, AM32 HEX, AM32
    WRITE, KST WRITE, KST FILES, HITEC WRITE, HITEC FILES. The chooser reads
    PROGRAMMER (decision 19). Reason: the MENU row SERVO TESTER opened a
    screen titled SERVO, and AM32's HEX view kept the list's title.
    - Code: `ui_servo.cpp` `uiServoTick()` (`drawTop("SERVO", ...)`) and
      `UI_SERVO.title`; `ui_am32.cpp` `drawHeader()`.
    - Tests: none name a title; the screenshots change.
    - Docs: README "Servo tester"; wiki Servo-Tester / Servotester,
      AM32-Configuration / AM32-Konfiguration; servo-tester.md "SERVO".
14. **One layout table** (item 10).

    | Item | Today | Rule |
    |---|---|---|
    | Text margin | x 8 (`UI_MARGIN`); x 10 on the tester's ERPM lines and AM32's connect page; x 6 on the hex dump | x 8; the tester keeps x 10 until its later change |
    | Button edge | x 6 (AM32, KST, Hitec, SERVO); x 8 (SETTINGS, MENU rows, servo MIN); x 14 (SD LOG) | x 6 to x 234 (`UI_EDGE` 6), as BACK; MENU rows keep x 8, they are list rows |
    | Three-button footer | y 268, h 46: AM32, KST, Hitec | y 268, h 46 (`UI_FOOT_Y`, `UI_FOOT_H`): 6/110, 122/52, 180/54 |
    | Two-button footer | SETTINGS y 268, h 48, 8/150 and 162/70; SERVO SET 6/140 and 152/82 | y 268, h 46: 6/150 and 162/72 |
    | Wide button | KST, Hitec 6/228 at y 268, h 46 | the same for every wide button, OK and RESTART DSHOT included |
    | Result OK | AM32 100 x 36 at (70, 200); KST, Hitec the wide button | the wide button |
    | Editor - and + | 44 x 44 (KST, Hitec), 58 x 44 (AM32, SERVO SET) | 44 x 44 (`UI_EDIT_BTN_W`) |
    | SERVO main 287/32, tester 283/33, SD LOG 240/44 and 288/30 | | unchanged: the touch-target change of [menu.md](menu.md#decisions) decision 5 |
    | Load defaults | RESET (SETTINGS), DEFAULTS (SERVO SET) | DEFAULTS, `C_AMBER` label |
    | Hold fill | `C_BLUE` on HOLD TO SAVE, `C_RED` elsewhere | `C_RED` |
    | Write-page count | `1 FIELD` (KST), `1 VALUE` (Hitec) | `1 CHANGE`, `2 CHANGES`, the leave page's word |
    | Write-page lines | 4 (KST), 6 (Hitec) | 4, then `AND n MORE` |
    | Strip items | two spaces (MENU, KST, SERVO); one space (Hitec) | two spaces; a strip too long drops items from the right, never the spaces |

    Reason: the differences were drift, not choices.
    - Code: `ui_widgets.h` the constants, built in slice 1: `UI_EDGE`,
      `UI_FOOT_Y`, `UI_FOOT_H` and `UI_EDIT_BTN_W`; BACK's x, the footers of
      AM32 CONFIG, KST SERVO and HITEC SERVO and the - and + of KST SERVO and
      HITEC SERVO take them at their values on `main`; `ui_settings.cpp` the
      footer defines, `drawAll()`; `ui_servo.cpp` `BTN_APPLY`, `BTN_DEFAULTS`,
      `BTN_MINUS`, `BTN_PLUS`; `ui_am32.cpp` `EDIT_BTN_W`, `BTN_OK`;
      `ui_sdlog.cpp` the button rectangles; `ui_kst.cpp` `drawWrite()`;
      `ui_hitec.cpp` `drawWrite()`, `drawList()` (strip),
      `HITEC_UI_CHANGES_MAX`; `ui_settings.h` `SettingsCtl::Reset` becomes
      `Defaults`.
    - Tests: 12 lines (test_menu.cpp 3 and test_ui.cpp 3 name
      `SettingsCtl::Reset`; test_ui.cpp 1114 checks the `C_BLUE` save fill;
      test_servo.cpp 5 name `Defaults`, which keep their name); about 8
      rewritten. Tests read rectangles from the control tables, so the
      moved rectangles break none.
    - Docs: README "Using it" (RESET); wiki The-Screens / Die-Bildschirme,
      First-Run / Erste-Schritte, Troubleshooting / Fehlersuche (RESET).
15. **Words that say what they mean** (item 11).

    | | Today | Becomes | Code |
    |---|---|---|---|
    | a | tester `HOLD` (the throttle latch) beside `HOLD TO ARM` | `LATCH`; caption `LATCHED`; `TesterCtl::Latch`. HOLD names only holds | `ui.cpp` `drawButtons()`, `drawThrottle()`; `ui.h` |
    | b | board voltage top right | `BOARD` at x 140, y 14: on `main` | `ui.cpp` `drawStatusBar()` |
    | c | HOLD TO ARM while a USB host holds the card | drawn disabled; the track reads `EJECT THE USB DRIVE TO ARM`: on `main` | `ui.cpp` `drawButtons()`, `drawThrottle()` |
    | d | Hitec D series needs the servo powered before the tap, KST after; the family view says so for D SERIES (`HITEC_UI_D_POWER`) | the chooser's power lines; HITEC SERVO's entry adds `THE SERVO MUST BE POWERED NOW.` with OWN | `ui_servo_prog.cpp`; `ui_hitec.cpp` `mainText()` |
    | e | SERVO SUPPLY `NONE` reads as "no power" | `OWN`: the servo has its own supply (decision 22) | `ui_settings.cpp` `supplyValue()` |
    | f | MENU `PINS BUSY` reads as a fault | rows drawn disabled, strip `DSHOT RESTARTING ON SIG GP0`; the unreachable tool refusals read `WAIT: DSHOT RESTARTING` | `ui_menu.cpp` `rowStatus()`, `drawMenu()`, `UI_MENU_BUSY_TEXT`; `ui_am32.cpp`, `ui_servo.cpp`, `ui_kst.cpp`, `ui_hitec.cpp` refusal text |
    | g | chip `MODE` | `PROG MODE` (9 characters, chip 65 px) | `ui_kst.cpp`, `ui_hitec.cpp` `drawList()` |
    | h | KST labels ALLOW UNCONT, UNCONTROLLED POS, UNCONTROLLED TIME, REVERSION against warnings that say FAILSAFE and REVERSED | FAILSAFE MOVE, FAILSAFE POSITION, FAILSAFE TIME, REVERSED (17 characters at most, 101 px) | `kst_view.cpp` `ROWS` |
    | i | AM32's result | `POWER-CYCLE THE ESC TO APPLY.` (`AM32_UI_POWER_CYCLE`): on `main`; the write page takes it over | `ui_am32.cpp` `drawWriting()`, the write page |
    | j | DShot rate as DS600, 600 KBIT/S, DSHOT KBAUD | `DSHOT600` as text; SETTINGS row `DSHOT` with the value `600` | `ui.cpp` `drawStatusBar()`; `ui_menu.cpp` `drawMenu()`; `ui_settings.cpp` `ROWS_WIRING` |
    | k | `COMPILED DEFAULTS IN USE` | `NOTHING SAVED: DEFAULTS IN USE` | `ui_settings.cpp` `noteText()`; `config.h` comment |

    Reason: each text was read as something else by a user of the bench.
    Rows b, c and i, and the D SERIES power line of row d, are on `main`.
    - Tests: test_ui.cpp 13 lines name `TesterCtl::Hold`, test_menu.cpp 1,
      test_claim.cpp 1: a rename. 34 lines name the old words (test_ui.cpp
      16, test_menu.cpp 9, test_servo.cpp 3, test_claim.cpp 3,
      test_kst_view.cpp 2, test_settings.cpp 1); about 25 rewritten, among them
      `statusLabelIs("DS600", ...)` and the `PINS BUSY` probes in
      test_ui.cpp 2516 to 2518, test_servo.cpp 1381 and test_claim.cpp 172.
      New: ARM drawn disabled while the card is held.
    - Docs: README "Using it", "Wiring the ESC", "AM32 ESC configuration",
      "KST servo programming", "Hitec servo programming", "Safety"; wiki
      First-Run / Erste-Schritte, The-Screens / Die-Bildschirme,
      Troubleshooting / Fehlersuche, Telemetry / Telemetrie,
      KST-Programming / KST-Programmierung (the field names),
      Hitec-Programming / Hitec-Programmierung, AM32-Configuration /
      AM32-Konfiguration.

Choices of this note where the decisions above leave the choice:

16. **MENU order.** BEEP THE ESC, AM32 CONFIG, SERVO TESTER, SERVO
    PROGRAMMER, SD LOG, SETTINGS: by what sits on SIG, with the two servo
    rows together and SETTINGS last now that the tester opens it too. The
    owner chose this order on 2026-11-14 (decision 23); decision 32 adds RX
    SIGNALS after SERVO PROGRAMMER.
17. **SETTINGS returns to where it was opened.** BACK from SETTINGS opened
    on the tester returns to the tester. `ui.cpp` keeps the origin with the
    open tool.
18. **MENU statuses report state.** BEEP, SD LOG and SETTINGS keep their
    statuses; the tool rows drop the pin, which the strip names. The
    claim's busy state is a screen state (decision 15 f).
19. **The chooser opens a tool.** `uiOpenTool()` in `ui.cpp` is the one route
    from a screen to another, used by the chooser and by the tester's
    readouts; [menu.md](menu.md#code) said no tool opens another. KST SERVO
    and HITEC SERVO keep their `UiScreenId`; the chooser is
    `UiScreenId::ServoProg` with `freePins` set, so its row is disabled
    while the claim is not Free. `uiHitecEnterD()` and
    `uiHitecEnterClassic()` replace the family view.
    Changed by the owner's decision 31 of 2026-11-14: HITEC SERVO takes SIG
    when it opens, so the chooser's Hitec entries take the pin on their
    tap, before the family's entry runs. The chooser view itself still
    claims nothing.
20. **The readouts act only while disarmed.** See the interlocks. The owner
    confirmed it on 2026-11-14 (decision 24).
21. **RESTART DSHOT waits 1000 ms.** LEAVE and RESTART DSHOT share a
    rectangle, so a double tap on LEAVE could restart DShot into a servo
    nobody unplugged, about 100 ms after LEAVE on KST and 41 ms on SERVO
    TESTER. RESTART DSHOT takes only a press that lands 1000 ms or more
    after the released phase began, and is drawn disabled until then.
22. **OWN.** SERVO SUPPLY's first value reads OWN. The stored value, 0,
    does not change.

Decisions of the owner, 2026-11-14, answering the open questions of
2026-11-13 and two points from STATUS.md:

23. **MENU order** (question 1). By device: BEEP THE ESC, AM32 CONFIG,
    SERVO TESTER, SERVO PROGRAMMER, SD LOG, SETTINGS, as decision 16.
    Decision 32 adds a row.
24. **The readouts while armed** (question 2). Inert, as decision 20.
25. **BACK on the leave page** (question 3). Drawn disabled, as decision 5.
26. **The "changed" colour** (question 4). `C_CYAN` on every screen, as
    decision 8.
27. **Docs between slices** (question 5). Each slice updates its own
    README.md and wiki lines and images, not all in slice 10. This is
    against the note's recommendation, which was to change them in slice
    10. The [slice table](#slices) is reworked for it: slice 10 shrinks to
    what remains, and slice 4, the layout table, no longer waits on this
    question.
28. **KST field labels** (question 6). FAILSAFE MOVE, FAILSAFE POSITION,
    FAILSAFE TIME, REVERSED, as decision 15 h.
29. **The no-switch value** (question 7). SERVO SUPPLY's no-switch value
    reads OWN, as decisions 15 e and 22.
30. **RESTART DSHOT's wait** (question 8). 1000 ms, as decision 21.
31. **HITEC SERVO takes SIG when it opens,** before the family is tapped
    (STATUS.md item 24). In structure B the Hitec entries of the chooser
    therefore take the pin on their tap. This changes decision 19.
32. **RX SIGNALS.** A MENU row RX SIGNALS after SERVO PROGRAMMER opens a
    chooser with MONITOR and OUTPUT ([rx-signals.md](rx-signals.md),
    decision 4). With decision 23 MENU then has seven rows: BEEP THE ESC,
    AM32 CONFIG, SERVO TESTER, SERVO PROGRAMMER, RX SIGNALS, SD LOG,
    SETTINGS. This follows from the two decisions; "after SERVO
    PROGRAMMER" puts RX SIGNALS before SD LOG. Seven rows contradicted
    decision 1, six rows that never scroll; decision 33 settles it.
33. **Seven rows scroll** (the question decision 32 raised). With seven
    rows MENU keeps its 40 px rows at the 42 px pitch and scrolls 36 px, as
    the seven-row MENU on `main` does; SETTINGS is reached with a short
    drag. This revises decision 1 of 2026-11-13, six rows that never
    scroll.

## Interlocks

No write, no pulse and no supply switch becomes reachable with fewer
deliberate actions than at 9f965a3.

| Path | Today | Structure B | Holds because |
|---|---|---|---|
| Tester to SETTINGS | MENU tap, SETTINGS row tap | readout tap, disarmed only; or as today | `uiOpenTool()` runs `uiOpenMenu()`'s disarm; flash still needs HOLD TO SAVE, refused while armed (`uiArmed()` in `uiSettingsTick()`); a readout tap is ignored while the claim is not Free, as `freePins` refuses the MENU row |
| KST claim and search (with a switch: the supply on) | MENU, KST SERVO: 2 taps | MENU, SERVO PROGRAMMER, KST / CHASERVO: 3 taps | the claim starts on the entry tap, as it started on the row tap |
| Hitec entry | MENU, HITEC SERVO, CLASSIC or D SERIES: 3 taps | MENU, SERVO PROGRAMMER, entry: 3 taps | CLASSIC needs `servoSupplyFitted()`, as `classicAllowed()` does |
| KST and Hitec write | 1000 ms hold, then 1000 ms hold on the page | tap, then 1000 ms hold on the page | two actions, as today; the first is a tap instead of a hold. The write hold stays, on a page that names every change; the press that opened the page cannot start it (`ui_hold`: a press whose first frame the hold did not see is never timed) |
| KST and Hitec restore | hold on FILES, hold on the page | tap, hold | as the write |
| AM32 write | 1000 ms hold on the list | tap, then 1000 ms hold on the page | one action more |
| DShot on SIG after a servo | LEAVE tap, RESTART DSHOT tap at another rectangle | LEAVE tap, RESTART DSHOT tap at the same rectangle | RESTART DSHOT waits for `pinClaimUnplugDue()` and then 1000 ms (decision 21); the release order of [pin-claim.md](pin-claim.md#sequence) step 5 is `endClaim()`'s, unchanged |
| A new KST or Hitec session (with a switch: the supply on) | BACK on the unplug page: 1 tap | NEXT SERVO: 1 tap | `pinClaimKeep()`, as today; live only once the pin is released |
| Servo pulses again | BACK on the unplug page, then HOLD TO START | BACK TO SERVO, then HOLD TO START | the claim is kept and SERVO TESTER opens SAFE, as today |
| HOLD TO REBOOT | 1000 ms hold | 1000 ms hold, on a 112 px button | `rebootAllowed()` unchanged |
| AUX in SETTINGS | locked while KISS is off | live | SETTINGS opens only while the claim is Free; a pin change rebuilds DShot's wiring only (`applyLive()`) |
| SERVO SUPPLY in SETTINGS | in KST's section | a core section | SETTINGS configures the enable pin and never drives it, as today |
| Discard on BACK | AM32 one BACK | AM32 two BACKs | more, and no write |

## Not changed

| What | Reason |
|---|---|
| The tester is the root | DShot runs on SIG from boot whatever screen shows; a menu as root would hide the armed state the tester exists to show |
| The tester's band layout and button row, the SERVO TESTER controls, SD LOG's 30 px buttons | the touch-target change of [menu.md](menu.md#decisions) decision 5, kept separate |
| HOLD TO ARM's own timer | [menu.md](menu.md#decisions) decision 17 |
| `UI_HOLD_MS`, 1000 ms; every hold stays a hold | the write, save, start, apply, reboot and arm holds are the deliberate actions the interlocks count |
| The pin claim, its release order and the core1 jobs | the leave page draws the claim's states; it changes none |
| The unplug hazard text, 970 µs at 1000 Hz | measured from DShot's frame, [pin-claim.md](pin-claim.md#restoring-dshot) |
| The stored settings block, `SETTINGS_VERSION` | SIG and AUX are labels of `dshotPin` and `kissPin`; OWN is value 0 |
| SETTINGS applies edits live | decision 7 names them; changing that is a different design |
| The 36 px header and BACK 64 x 32 px | [menu.md](menu.md#decisions) decision 5 |
| BEEP THE ESC as an action row | [menu.md](menu.md#decisions) decision 2 |
| KST and Hitec FILES layouts | only labels and colours change |
| The card files and their keys | the KST label change is on screen only; `kst_files` does not read `kstRows()` labels |

## Code

| File | Change | Slice |
|---|---|---|
| `src/ui_widgets.{h,cpp}` | `uiHeader()` BACK flag (on `main`); `UiBackGuard`, `uiBackGuard()`, `uiBackGuardDraw()`; `uiStripTwoLines()`; role names; `UI_EDGE`, `UI_FOOT_Y`, `UI_FOOT_H`, `UI_EDIT_BTN_W` (built); square 12 px chip | 1 (built), 2 |
| `src/ui_list.{h,cpp}` | `uiListDrawField()`, `UiField`, `UiValue`, `UiEdge` | 1 (built) |
| `src/ui_leave.{h,cpp}` | the leave page | 1 (built), 6 |
| `src/ui_screen.h` | `SettingsRow::sub`; `UiProgEntry`; `UiScreenId::ServoProg` | 3, 9 |
| `src/ui_settings.{h,cpp}` | SERVO SUPPLY section, SIG PIN and AUX PIN, footer, DEFAULTS, colours, return to origin | 2, 3, 4, 7 |
| `src/settings.cpp` | `settingsValidate()`: AUX never on SIG | 3 |
| `src/kst_view.cpp` | four labels | 3 |
| `src/ui_menu.cpp` | strip, statuses, busy state, seven rows scrolling 36 px (decisions 32 and 33) | 2, 3, 9 |
| `src/ui_registry.cpp` | seven rows (decisions 32 and 33); `UI_PROGRAMMERS` and its fit check | 9 |
| `src/ui_servo_prog.{h,cpp}` | new: the chooser | 9 |
| `src/ui.cpp` | `uiOpenTool()`, the origin, readouts, LATCH, BOARD, ARM while the card is held, DSHOT600, colours | 2, 3, 7 |
| `src/ui_am32.{h,cpp}` | write page, editor old value, guard, HEX title, colours, footer | 2, 4, 5 |
| `src/ui_kst.{h,cpp}` | leave page, WRITE..., RESTORE..., colours, section removed | 2, 3, 5, 6 |
| `src/ui_hitec.{h,cpp}` | as KST; family view removed; family entry points | 2, 5, 6, 9 |
| `src/ui_servo.{h,cpp}` | leave page, guard, title, supply line, footer, colours | 2, 4, 5, 6, 8 |
| `src/ui_sdlog.cpp` | button edge, colours | 2, 4 |

## Slices

Each slice builds, passes the host suite and merges alone. Sizes are
estimates, not measured.

To be redesigned for the owner's decision 27 of 2026-11-14: each of slices
2 to 9 also updates the README.md sections, the wiki pages in English and
German and the images that the "Docs" lists of its decisions name, with
CHANGELOG.md, STATUS.md and the design notes it touches. Slice 10 keeps
only what no single slice owns. The sizes and estimates of slices 2 to 9
below were made with the documentation in slice 10 and do not include
that work; they are to be redone. Slice 9 is to be redesigned for
decisions 31 to 33.

| Slice | Changes | Needs | Files | Lines, roughly | Tests rewritten / new | Estimate |
|---|---|---|---|---|---|---|
| 1. Widgets, built | `UiBackGuard`, `uiStripTwoLines()`, `uiListDrawField()`, `ui_leave`, layout constants, role names; nothing drawn differently. The `uiHeader()` flag is on `main` | | 9 in `src/`, 2 new | +500, −20 | 2 / 430 lines in test_widgets.cpp and test_servo.cpp | built |
| 2. Colour roles | decisions 8 and 11: the role table on every screen, square chips, PROG MODE | 1 | 10 | 150 changed | 15 / 40 | 1 day |
| 3. Names and SETTINGS, built | decisions 9, 10, 15 b, e, f, h, j, k: SERVO SUPPLY section, SIG and AUX, connector lines, OWN, DSHOT600, MENU statuses and busy strip, KST labels; with its README.md and wiki lines and images (decision 27) | 1 | 11 | +200, −80 | 110 / 60 | built |
| 4. Layout table, built | decision 14 | 1 | 11 in `src/` | +146, −79 | 63 / 236 lines in 7 test files | built |
| 5. Write rule and guard | decisions 6, 7, 13 (AM32 HEX and WRITE titles): `WRITE...`, `RESTORE...`, AM32 write page and result, editor old value, guard on AM32 and SERVO SET, BACK disabled while writing, the leave queue gone | 1, 2, 4 | 7 | +300, −120 | 110 / 120 | 2 to 3 days |
| 6. Leave page | decision 5 and owner decision 3: KST, Hitec and SERVO TESTER on `ui_leave`; STAY, NEXT SERVO, BACK TO SERVO; RESTART DSHOT's wait | 1, 5 | 6 | +180, −300 | 90 and three helpers / 80 | 2 to 3 days |
| 7. Tester | owner decision 4, decisions 15 a to c, 17: readouts, `uiOpenTool()`, origin, LATCH, BOARD, ARM while the card is held | 1, 3 | 4 | +150 | 15 / 100 | 1.5 days |
| 8. SERVO TESTER | decision 13: the title; decision 12 is on `main` | 1, 3 | 2 | +10 | 0 / 5 | 0.5 day |
| 9. Chooser | owner decisions 1 and 2, decisions 16, 18, 19, 15 d, 23, 31, 32 and 33: seven MENU rows scrolling 36 px, `ui_servo_prog`, `UI_PROGRAMMERS`, Hitec family entry points, family view removed | 3, 6, 7 | 9, 2 new | +350, −120 | 50 / 120 | 2 to 3 days |
| 10. Documentation | what no slice owns: menu.md (the tree, MENU, revision lines on decisions 1, 12 and 18), pin-claim.md's revision line on decision 4, README "Layout", "Tests" and "The user manual", and a check that no README section, wiki page or image still shows the old structure | 1 to 9 | to be redone | to be redone | none | to be redone |

Slice 1 is built ([slice 1 as built](#slice-1-as-built)). Slice 4 is
built ([slice 4 as built](#slice-4-as-built)); by decision 27 its previews,
README.md and wiki lines go with it.

Slice 3 comes before slice 9: without it the SERVO SUPPLY section leaves
SETTINGS with KST SERVO's MENU row. Slice 6 needs slice 5 because both change
BACK during a write. Slices 2, 3 and 4 merge in any order after 1; 7 and 8
after 3. The total for slices 2 to 10, 13.5 to 17.5 days, was estimated
with the documentation in slice 10 and is to be redone.

The test counts come from `grep -cE` over `test/*.cpp` at 2940f9c, per
decision above. Screenshots: 14 shots in test_kst_ui.cpp, 12 in
test_hitec_ui.cpp, 21 in test_ui.cpp, 12 in test_servo.cpp and 2 in
test_menu.cpp name screens that change. By decision 27 each slice
regenerates the wiki images of the screens it changes, which over slices 2
to 9 is every wiki image but `splash.png` and `wiring.svg`;
`hitec-family*.png`,
`kst-leave*.png`, `hitec-unplug.png`, `servo-unplug.png` and
`menu-pins-busy.png` give way to `prog-chooser*.png`, `leave-*.png` and
`menu-restarting.png`; `am32-write.png` and `am32-discard.png` are new.

Documentation beyond the per-decision lists, before decision 27 all in
slice 10: README "Using it" and "Safety", kst-programming.md and
hitec-programming.md (screens, a revision line), servo-tester.md, this
note's status line and its row in [README.md](README.md) go with the slice
that changes what they describe; README "Layout", "Tests" and "The user
manual", menu.md (the tree, MENU, a revision line on decisions 1, 12 and
18) and pin-claim.md (a revision line on decision 4) stay in slice 10.
Which slice takes each of these is to be redone with the table. backups.md and
sd-write-cache.md name KST SERVO and HITEC SERVO, which keep their titles,
and do not change.

## Slice 1 as built

The building blocks the later slices name, with host tests in
`test/test_widgets.cpp` (`runWidgetTests`). No screen draws differently: the
preview images stay as they are.

| Block | Where | Contract | Used by |
|---|---|---|---|
| BACK's enabled flag | `uiHeader(title, t, backEnabled = true)`, `ui_widgets.h` | on `main`: a disabled BACK is drawn as `uiButton()` draws a disabled control | the unplug pages today; the leave page's BACK in slice 6 |
| Layout constants | `UI_EDGE` 6, `UI_FOOT_Y` 268, `UI_FOOT_H` 46, `UI_EDIT_BTN_W` 44, `UI_STRIP_LINE1_Y` 42, `UI_STRIP_LINE2_Y` 52, `ui_widgets.h` | the values of decision 14 | BACK's x; the footers of AM32 CONFIG, KST SERVO and HITEC SERVO; the - and + of KST SERVO and HITEC SERVO; slice 4 |
| Colour roles | `UI_COL_SAFE`, `UI_COL_LIVE`, `UI_COL_ON`, `UI_COL_WARN`, `UI_COL_ALERT`, `UI_COL_LATCHED`, `ui_widgets.h` | each expands to its `C_*` entry, so it follows the theme | the widgets below; slice 2 |
| Two strip lines | `uiStripTwoLines()`, `ui_widgets.h` | clears y 40 to 61, draws scale 1 at x 8, y 42 and y 52; a null line is not drawn | SERVO TESTER's supply line; the guard |
| BACK guard | `UiBackGuard`, `uiBackGuard()`, `uiBackGuardDraw()`, `ui_widgets.h` | with changes pending, the first BACK arms and the second returns Back; a press that lands elsewhere disarms, as does no change left, but a BACK tap on that frame leaves; no timer; the two amber lines name the count and the verb | slice 5: AM32 CONFIG and SERVO SET |
| Field row | `uiListDrawField()`, `UiField`, `UiValue`, `UiEdge`, `ui_list.h` | decision 8's row: fill, group, label, value at scale 2 up to 104 px, edge bar 3 px | slice 2: AM32 CONFIG, KST SERVO, HITEC SERVO, SERVO SET |
| Leave page | `UiLeave`, `UiLeaveText`, `uiLeaveOpen()`, `uiLeavePoll()`, `uiLeaveTouch()`, `uiLeaveDraw()`, `ui_leave.h` | the three phases, numbered steps, notes, STAY and LEAVE, then NEXT SERVO or BACK TO SERVO and RESTART DSHOT; RESTART DSHOT takes a press that lands the wait or more after the released phase began, and only such a press draws it pressed; `leaveEnabled` cleared draws LEAVE disabled and ignores its tap, for the screen's writes; LEAVE takes only a press that landed while `leaveEnabled` was set (`leavePress`), and only such a press draws it pressed | slice 6 |

What slice 1 left to the open questions, which the owner answered on
2026-11-14; none of the answers is built:

- The "changed" role has no `UI_COL_*` name, and `uiListDrawField()` takes
  the changed colour as a parameter: `C_CYAN` by decision 26.
- The leave page draws no header: BACK on it is the screen's, drawn
  disabled by decision 25.
- RESTART DSHOT's wait is a parameter of `uiLeaveOpen()`: 1000 ms by
  decision 30. The widget puts RESTART DSHOT in LEAVE's rectangle, as that
  answer keeps it.
- The step texts, including the no-switch value's name, are the screen's:
  OWN by decision 29.

## Slice 3 as built

Decisions 9, 10 and 15 b, e, f, h, j, k on the screens, with host tests in
`test/test_ui.cpp`, `test/test_menu.cpp`, `test/test_settings.cpp`,
`test/test_servo.cpp`, `test/test_kst_ui.cpp`, `test/test_hitec_ui.cpp` and
`test/test_kst_view.cpp`. The stored block is unchanged. Where the build
reads the decisions' lines, it reads them as follows:

| Point | As built | Why |
|---|---|---|
| MENU's tool rows | keep their pin status, `GP0`, `GP0+1`, drawn disabled under it while the claim is not Free | decision 18, which drops them, is slice 9's; decision 10 names only `drawMenu()` for MENU |
| The restarting caption | `DSHOT RESTARTING ON SIG GP0` in amber, `UI_COL_WARN`, for every state but Free | the note gives no colour; the busy status it replaces was amber |
| SERVO SUPPLY at OWN | the value at y+8, as under GP28 and GP29, with no line under it | the value does not jump while the row steps |
| `settingsValidate()` | AUX on SIG's GPIO moves to the next free pin after SIG, KISS kept; an AUX pin the board does not free takes the default and switches KISS off, as before | decision 10 governs the collision only |
| SIG by role and number | in the functions decision 10's Code line names, SERVO TESTER's caption (this note's SERVO TESTER section), AM32's handover status (`RELEASING SIG GP0`, the text `drawConnect()` shows) and HITEC SERVO's family view, `AN ESC ANSWERS ON SIG GP0: UNPLUG IT.` and `RELEASING SIG GP0 FROM DSHOT` | the decision's rule; the rest keeps a bare `GP0` until its slice: the KST and Hitec list strips, `GP0 IS THE SERVO'S: DSHOT IS OFF.`, the unplug pages' `DSHOT RESTARTS ON GP0`, `PULSES ON GP0 ARE THROTTLE TO AN ESC.` (41 characters with SIG) and the splash's `UNPLUG ANY SERVO FROM GP0 NOW`, for slices 6 and 9 |
| SERVO TESTER's caption | the role alone, `SIG  333 HZ  1000-2000 US`, where `SIG GP0` would end within 4 px of the widest chip, RUNNING at x 179; with two servos the travel goes, `SIG+AUX  333 HZ` | the decision's "role alone where a strip has no room", then decision 14's rule for a strip too long |
| The same-pin refusal's second line | `SET SIG PIN OR AUX PIN IN SETTINGS.` | the decision gives the first line only |
| The tester's `IDLE ns` | from y 11, not y 7 | `IDLE 5s` starts at x 154, left of where `DSHOT1200` ends, x 156; at y 7 the two shared pixels. y 11 is the first row under the rate in both themes |
| SETTINGS' line over LINK with KISS on | `SWAP THE ESC'S SIGNAL AND KISS WIRES` while SIG is off the boot pin, in place of the servo warning | a step of either pin row swaps SIG and AUX, so DShot drives the wire the ESC's KISS pad was on until the wires are swapped |

## Slice 4 as built

Decision 14's table on the screens, with host tests in `test/test_ids.cpp`
(`testLayoutTable()`), `test/test_ui.cpp` (`testAm32LayoutTable()`) and the
KST, Hitec and SETTINGS suites. Where the build reads the decision's
lines, it reads them as follows:

| Point | As built | Why |
|---|---|---|
| Load defaults | SETTINGS' button reads `HOLD DEFAULTS`, `HOLD` over `DEFAULTS` on two lines, amber; `SettingsCtl::Defaults` | the rule's word is DEFAULTS; the owner's 1 s hold of 2026-11-14 stays, and every hold's label starts with HOLD. One line is 77 px, wider than the 72 px button |
| Write-page lines | `HITEC_PAGE_ROWS_MAX` 4 in `src/hitec_view.h` | the constant the line names, `HITEC_UI_CHANGES_MAX`, is this one on `main` |
| RESTART DSHOT | 160 × 40 px at (40, 200), as on `main` | in LEAVE's rectangle without decision 21's 1000 ms wait, a double tap on LEAVE restarts DShot into the servo; slice 6 brings the rectangle and the wait together |
| SERVO TESTER's MIN | x 8 | the tester's controls are under "Not changed", and the Code line does not name it |
| HOLD TO REBOOT (KST) and READ AGAIN (Hitec) on the leave page | 228 × 46 px at y 214 | wide buttons above the footer's own wide button on the same view; the Code line does not name them |
| AM32's OK | the wide button; it acts and draws pressed only for a press that lands after the write ended | it lies over HOLD TO WRITE, where the hold's finger lifts |
| Hitec's list strip | model, backup, pin, version, two spaces apart, dropped from the right; the room ends 4 px before the chip, `uiChipX()` | a classic strip with every item does not fit; the backup's state is the item the write depends on |
| Hitec strip band | every Hitec view clears y 40 to 61 | the family view's second strip line ends at y 58, below the 16 px band `uiStripClear()` clears |

## Risks

1. **Habit.** KST SERVO moves from one tap after MENU to two. BACK on the
   leave page stops working.
2. **Test rewrite.** About 400 test lines change across slices 2 to 9, most
   in test_kst_ui.cpp and test_hitec_ui.cpp.
3. **Docs lag.** Settled by decision 27: each slice updates README.md and
   the wiki for its own screens, so they lag by no slice; each slice is the
   larger for it.
4. **RESTART DSHOT's wait.** Every leave takes 1000 ms more.
5. **`uiOpenTool()`.** A screen that opens another breaks menu.md's
   invariant; the cache invalidation on entry and `uiCurrentScreen()` need
   tests for the chooser and the readouts.
6. **Flash and RAM.** The headroom is not measured here.
7. **German wiki.** Every wiki change is made twice.
8. **Unknowns.** Whether an AM32 ESC needs a power cycle to run new
   settings; whether the KST label names in the wiki pages are quoted by
   users' own notes.

## Open questions

None open. The eight questions of 2026-11-13 are answered by decisions 23
to 30, and the question of the seventh MENU row, which followed from
decisions 23 and 32, by decision 33.
