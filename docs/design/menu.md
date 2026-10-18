# Menu restructure: MENU and SETTINGS

Status: slices A to C built, host tests only, not tested on hardware:
`ui_list` under the AM32 and SERVO SET lists, `ui_hold` under four holds,
a control table per screen (`src/ui_screen.h`) that the host tests tap by id,
and, from slice C, MENU, SETTINGS and the screen registry. Slices A to C come
before KST programming, slice 3 of [pin-claim.md](pin-claim.md), and
[kst-programming.md](kst-programming.md) builds on them. [As built](#as-built)
lists where the code differs from this note; the code wins. Line numbers in `src/`, `test/`, `tools/`, README.md and the
wiki name 9c0f3b9, the tree the review read, not the tree with slice A; line
numbers in STATUS.md and the design notes name 4c91db9.

Terms: UI is user interface, ESC electronic speed controller, DShot the
digital ESC protocol the tester drives, EDT Extended DShot Telemetry, AM32 an
open ESC firmware with a serial bootloader, KST and Chaservo servo brands that
share one programming protocol, SD Secure Digital, USB Universal Serial Bus,
GPIO general-purpose input/output and PWM pulse-width modulation. SIG and AUX
are the two signal pins as [pin-claim.md](pin-claim.md) defines them. A
screen's header is the band at the top with its title and BACK; its strip is
the line under the header (`UI_STRIP_Y`); its body starts at `UI_BODY_Y`
(`src/ui_widgets.h`).

Source: the menu review of 2026-10-18, made against 9c0f3b9 with the
switched-supply design of PR (pull request) #6. The owner accepted its 13
recommendations the same day ([decisions](#decisions)). Its statements about
the code are checked here against `src/`; where the code says otherwise,
this document follows the code.

## Why

The tree as built:

```
TESTER (root, no header, no BACK)                 ui.cpp:1557-1585
 │  HOLD TO ARM | HOLD | CFG                       ui.cpp:155-159
 └─ CFG: disarms, throttle 0, HOLD off             ui.cpp:1406-1416
     └─ SETTINGS                                   ui.cpp:843-933, 1209-1273
         ├─ MOTOR POLES, THROTTLE CEILING          steppers, act in place
         ├─ BEEP                                   350 ms flash
         ├─ SD LOG ──> SD LOG; BACK ──> TESTER     ui.cpp:1258-1262
         ├─ AM32 ──> AM32 CONFIG; BACK ──> SETTINGS
         ├─ SERVO ──> SERVO; BACK ──> unplug page ──> SETTINGS
         ├─ KST: drawn disabled                    ui_kst.cpp:11
         └─ SETUP ──> SETUP; BACK ──> SETTINGS     ui_setup.cpp:458-460
```

What does not scale:

- **No room for a tool.** The destination row holds AM32, SERVO, KST and
  SETUP at 50 px each with 4 px gaps, the 212 px between the margins, and an
  assert keeps it so (`ui.cpp:226`). The row spans y 258 to 305; 14 px are
  left below it.
- **SETUP is full.** Seven fixed rows (`ui_setup.cpp:66-74`); the servo
  supply switch of [kst-programming.md](kst-programming.md) needs an eighth.
- **Two screens for two values.** Pole count and throttle ceiling are edited
  on SETTINGS and saved on SETUP (`ui.cpp:873-876`).
- **Dispatch.** Six booleans pick the screen (`ui.cpp:295-300`). A new tool
  adds a boolean, an include, a branch in `uiTick()` and one in
  `handleConfigTouch()`, a rectangle, a draw call and test coordinates.
- **Lists.** At 9c0f3b9, AM32 (`ui_am32.cpp:592-720`) and SERVO SET
  (`ui_servo.cpp:757-797`) each carry a copy of the list gesture. Both lock
  the axis after 6 px and scroll 1:1; they differ in the sideways swipe: AM32
  spreads a field's range over 180 px (`ui_am32.cpp:602`), SERVO SET applies
  one coarse step per 16 px (`ui_servo.cpp:162`). The scrolling SETUP of PR #6
  adds a third copy, and KST plans a fourth list and a paged file list. Slice
  A puts both lists on `ui_list`.
- **Holds.** At 9c0f3b9, five 1000 ms holds run on four timers in four files:
  HOLD TO ARM (`ui.cpp:1376-1398`, `ARM_HOLD_MS`), AM32's HOLD TO WRITE
  (`ui_am32.cpp:514-525`, `WRITE_HOLD_MS`), SERVO's HOLD TO START and SET's
  HOLD TO APPLY (`ui_servo.cpp:527` and `:803`, one `s_holdMs`), and SETUP's
  HOLD TO SAVE (`ui_setup.cpp:395-420`, `SAVE_HOLD_MS`). KST would add a
  fifth file. Slice A puts all but HOLD TO ARM on `ui_hold`.
- **Pin users.** One switch and two comparisons decide per user:
  `jobFor()` (`pin_claim.cpp:28-34`), `needsUnplug()` (`pin_claim.cpp:37-39`)
  and the AM32 exemption in `pinClaimCheck()` (`pin_claim.h:130`).
- **SD LOG BACK.** The SD LOG tap clears the SETTINGS flag
  (`ui.cpp:1260`), so both BACK branches of the SD LOG screen
  (`ui.cpp:1164-1168` and `:1194-1198`) return to the tester. The wiki page
  The-Screens says they return to Settings. `test_ui.cpp:306-307` taps BACK
  twice; the second tap lands on the tester's status bar, so the test passes
  either way.

## The tree

```
TESTER (root)   HOLD TO ARM | HOLD | MENU         MENU disarms, as CFG does
 └─ MENU: one list of registry entries; BACK ──> TESTER
     ├─ BEEP THE ESC     action row: the DShot beacon, flash on the row
     ├─ AM32 CONFIG      tool, claims SIG
     ├─ SERVO TESTER     tool, claims SIG, and AUX for two servos
     ├─ KST SERVO        tool, claims SIG; its registry line comes in slice 3
     ├─ SD LOG           tool
     └─ SETTINGS         tool: one list in sections
 Every tool: BACK ──> MENU. SERVO and KST pass the unplug page first.
```

The tester changes in one word: CFG reads MENU. Opening MENU disarms, zeroes
the throttle and turns HOLD off, as CFG does today. Each tool keeps its
screens; AM32 CONFIG and SERVO keep their layouts, with the header and list
changes below. A new tool is one registry line, and a seventh row makes MENU
scroll. KST is that case: today's disabled KST button has no row in slice C,
and slice 3 adds KST's line (decision 4).

## MENU

```
y   0 | MENU                            [  BACK  ]|  header 36 px; BACK 64 x 32
   40 | GP29  600 KBIT/S                 (EDT ON) |  strip: caption and chip
   62 | BEEP THE ESC                    DISARMED  |  rows 40 px tall,
  104 | AM32 CONFIG                         GP29  |  42 px pitch
  146 | SERVO TESTER                     GP29+28  |
  188 | KST SERVO                           GP29  |  from slice 3
  230 | SD LOG                             READY  |
  272 | SETTINGS                         UNSAVED  |  C_AMBER
  312 |                                           |  sixth row ends at 311
      |  slice C: five rows; SD LOG at 188, SETTINGS at 230
  320 +-------------------------------------------+
```

| Item | Value |
|---|---|
| Viewport | y 62 to 319, 258 px |
| Rows | 40 px tall (`ROW_H`) at a 42 px pitch (`ROW_PITCH`), x 8 to 231 |
| Six rows | 252 px of content: no scroll |
| Seven rows | 294 px: scroll range 0 px to 36 px |
| Label | scale 2 at x 16, 13 px below the row's top |
| Status | scale 1, right-aligned to x 222, 16 px below the row's top, in the fact's colour |
| Scroll bar | 3 px wide at x 235, drawn only while the list scrolls |
| Strip | caption: the ESC pin and the DShot rate; chip: `EDT ON` in `C_GREEN` or `EDT OFF` in `C_RED`, as SETTINGS shows it today |

| Row | Kind | Status | A tap |
|---|---|---|---|
| BEEP THE ESC | action | `DISARMED` in `C_DIM`; for 350 ms after a tap, `SENT` on a `C_INK` fill or `REFUSED` on a `C_AMBER` fill (`CMD_FLASH_MS`) | `escRequestBeep(1)` |
| AM32 CONFIG | tool, SIG | the SIG pin, `GP29`; `PINS BUSY` in `C_AMBER` while the claim is not Free | opens AM32 CONFIG |
| SERVO TESTER | tool, SIG and AUX | `GP29+28`; `PINS BUSY` | opens SERVO |
| KST SERVO, from slice 3 | tool, SIG | `GP29`; `PINS BUSY` | opens KST SERVO |
| SD LOG | tool | `NO CARD`, `READY`, `RECORDING`, `CARD ERROR`, or `USB DRIVE` while a host holds the card, in the SD LOG screen's colours | opens SD LOG |
| SETTINGS | tool | `UNSAVED` in `C_AMBER` while `settingsDirty()`, else none | opens SETTINGS |

Rows fire on release, as every button does, and a press that slides off
cancels. A press that becomes a vertical drag scrolls once the list is longer
than the viewport and never fires.

`PINS BUSY` is reachable: RESTART DSHOT on the unplug page returns to MENU at
once, and the claim stays Restoring until `escTaskSuspended()` turns false.
A tap on SERVO TESTER in that time would open a screen whose claim is
refused with Busy; MENU refuses it instead, from the row's `pinUser` and
`pinClaimState()`.

BEEP's refusal is unreachable through the UI. `escRequestBeep()` refuses only
while armed (`esc_task.cpp:231`), and opening MENU disarms. The flash stays
as the backstop, and the test forces it with `escSetArmed(true)`, as
`test_ui.cpp:617` does today. The caption `BEEP NEEDS THE ESC DISARMED` under
today's BEEP button goes; the row's status says it.

**Fit.** The registry holds each row's label and its widest status in
characters, and `ui_registry.cpp` checks every line at build time: x 16 plus
the label at scale 2, plus 8 px, ends before x 222 minus the widest status at
scale 1. Text width is `gfxTextW()`'s rule, (6 × characters − 1) × scale.
SERVO TESTER is 142 px and ends at x 157; `PINS BUSY` is 53 px and starts at
x 169. `KST PROGRAMMER`, 166 px, would end at x 181 and run into `PINS BUSY`
at x 169, so the row reads `KST SERVO`, the title of the KST screen.

## SETTINGS

One list in sections: ESC TESTER, WIRING, SERVO SUPPLY (from slice 3),
DISPLAY and BOARD. Pole count and throttle ceiling lead it, under the label
`MAX THROTTLE`. The geometry is the scrolling SETUP design of PR #6, which
was kst-programming.md decisions 11, 12 and 15, moved here and adjusted for
the 36 px header, the two rows from today's SETTINGS screen and the sections.

At 0 px, the top:

```
y   0 | SETTINGS                        [  BACK  ]|  header 36 px
   40 | ESC TESTER                                |  section caption, 20 px
   60 | MOTOR POLES       14  [  -  ] [  +  ]     |  44 px rows, 50 px pitch
  110 | MAX THROTTLE     20%  [  -  ] [  +  ]     |  labels scale 1,
  160 | WIRING                                    |  values scale 2
  180 | ESC PIN         GP29  [  -  ] [  +  ]     |
  230 | DSHOT KBAUD      600  [  -  ] [  +  ]     |  cut: 10 px visible
  240 +-------------------------------------------+  end of the viewport
  242 | LINK                    998/S  0% ERR     |  fixed
  254 | UNSAVED - HOLD SAVE TO KEEP               |  fixed note line
  264 |                                           |  save progress, 3 px
  268 | [   HOLD TO SAVE     ] [ RESET ]          |  150 x 48, 70 x 48
  316 +-------------------------------------------+
```

At the end of slice C's list, 330 px:

```
   40 | KISS PIN        GP28  [  -  ] [  +  ]     |  rows 290..333: cut, 4 px
   50 | DISPLAY                                   |
   70 | CONTRAST            [   NORMAL   ]        |  toggle, label scale 2
  120 | BACKLIGHT        192  [  -  ] [  +  ]     |
  170 | BOARD                                     |
  190 | BUILT FOR         RP2350-TOUCH-LCD-2.8    |  read-only, value scale 1
```

In content coordinates, 0 px at the top of the list:

| Content y | Item | Slice |
|---|---|---|
| 0 | caption ESC TESTER | C |
| 20 | MOTOR POLES, stepper | C |
| 70 | MAX THROTTLE, stepper | C |
| 120 | caption WIRING | C |
| 140 | ESC PIN, stepper | C |
| 190 | DSHOT KBAUD, stepper | C |
| 240 | KISS TELEM, toggle | C |
| 290 | KISS PIN, stepper | C |
| 340 | caption SERVO SUPPLY | 3 |
| 360 | SERVO SUPPLY, toggle | 3 |
| 340 (C), 410 (3) | caption DISPLAY | C |
| 360 (C), 430 (3) | CONTRAST, toggle | C |
| 410 (C), 480 (3) | BACKLIGHT, stepper | C |
| 460 (C), 530 (3) | caption BOARD | C |
| 480 (C), 550 (3) | BUILT FOR, read-only | C |
| 530 (C), 600 (3) | end | |

| Item | Value |
|---|---|
| Viewport | y 40 to 239, 200 px (`VIEW_Y0`, `VIEW_H`). SETTINGS has no strip |
| Section caption | 20 px (`CAP_H`); its name at scale 1 in `C_CYAN` at x 8, 8 px below its top; takes no press |
| Rows | 44 px tall (`ROW_H`) at a 50 px pitch (`ROW_PITCH`) |
| Content | slice C: 4 captions and 9 rows, 530 px; slice 3: 5 captions and 10 rows, 600 px |
| Scroll range | slice C: 0 px to 330 px; slice 3: 0 px to 400 px (`MAX_SCROLL_PX`, content minus 200 px) |
| Label | scale 1, 7 px tall, from x 8, centred in the row. The widest, `MAX THROTTLE` and `SERVO SUPPLY`, are 71 px and end at x 78 |
| Value | scale 2, 14 px tall, right-aligned to x 132 (`VAL_R`). The widest, 4 characters (`GP29`, `1200`, `100%`), is 46 px and starts at x 86 |
| − and + | 44 × 44 px, at x 138 and x 188 |
| Toggle | 94 × 44 px, at x 138, its label at scale 2 |
| BUILT FOR value | scale 1, right-aligned to x 232: `RP2350-TOUCH-LCD-2.8`, 119 px |
| Scroll bar | 3 px wide at x 235; 75 px tall for slice C (200 px × 200 / 530), 66 px for slice 3 |
| LINK | y 242, fixed |
| Note line | y 254, fixed: `UNSAVED - HOLD SAVE TO KEEP` and the other notes |
| Save progress | y 264, 3 px, across HOLD TO SAVE |
| Footer | y 268, 48 px tall: HOLD TO SAVE at x 8, RESET at x 162 |

A label at scale 2 is 12 px a character: `MAX THROTTLE` would be 142 px and
leave 82 px for the value and both controls. So labels stay at scale 1, and
values and toggle labels are at scale 2. `THROTTLE CEILING`, 95 px at scale
1, would end at x 102 and run into a 4-character value at x 86; the row reads
`MAX THROTTLE`, as the tester's throttle caption `MAX 20%` does.

The rows and their values:

| Section | Row | Kind | Values | Widest | Label ends at x | Value or control from x |
|---|---|---|---|---|---|---|
| ESC TESTER | MOTOR POLES | stepper | 2 to 28, even (`MIN_MOTOR_POLES`, `MAX_MOTOR_POLES`) | 2 | 72 | 110 |
| ESC TESTER | MAX THROTTLE | stepper | `5%` to `100%`, 5 % steps (`MAX_THROTTLE_STEP`) | 4 | 78 | 86 |
| WIRING | ESC PIN | stepper | `GP28`, `GP29` | 4 | 48 | 86 |
| WIRING | DSHOT KBAUD | stepper | 150, 300, 600, 1200 | 4 | 72 | 86 |
| WIRING | KISS TELEM | toggle | `OFF`, `ON` | | 66 | 138 |
| WIRING | KISS PIN | stepper | `GP28`, `GP29`; `--` and disabled while KISS is off | 4 | 54 | 86 |
| SERVO SUPPLY | SERVO SUPPLY | toggle | `NONE`, `GP0` (slice 3) | | 78 | 138 |
| DISPLAY | CONTRAST | toggle | `NORMAL`, `HIGH` | | 54 | 138 |
| DISPLAY | BACKLIGHT | stepper | 16 to 255, steps of 32 | 3 | 60 | 98 |
| BOARD | BUILT FOR | read-only | `RP2350-TOUCH-LCD-2.8` | 20 at scale 1 | 60 | 113 |

The BOARD section's one row is today's SETUP row BOARD; under the caption
BOARD it reads `BUILT FOR`. Every row keeps today's behaviour: steppers reach
legal values only, ESC PIN and KISS PIN step past each other's pin
(`stepPinAvoiding()`), a change applies at once (`applyLive()`, which calls
`escSetPoles()` as the pole stepper on SETTINGS does today), and RESET loads
the compiled defaults, pole count and ceiling included, into the live
settings only.

Touch targets:

| Target | Today | SETTINGS |
|---|---|---|
| − and + on SETUP's rows | 40 × 24 px | 44 × 44 px |
| Pole and ceiling steppers on today's SETTINGS | 46 × 44 px | 44 × 44 px |
| Toggle | 84 × 24 px | 94 × 44 px |
| HOLD TO SAVE | 150 × 40 px | 150 × 48 px |
| RESET | 70 × 40 px | 70 × 48 px |
| BACK | 50 × 22 px | 64 × 32 px, shared by every screen |

Gestures, on `ui_list` ([gestures](#gestures)):

- A press on a control acts as today: − and + step on touch-down and repeat
  while held, a toggle fires on release inside it. The list does not scroll
  under a press that began on a control.
- A control's target is its rectangle clipped to the viewport. A control cut
  by the viewport's edge takes a press only on its visible part, and only
  while at least 40 px (`UI_LIST_TAP_MIN`) of it is visible.
- A press anywhere else in the viewport scrolls: on a caption, a label, a
  value, a 6 px gap between rows, or a cut control under 40 px. After 6 px of
  vertical travel (`UI_AXIS_LOCK_PX`) the list follows the finger 1:1 from the
  press point, clamped to the scroll range. There is no sideways mode and no
  selection: each row carries its own controls.
- A press outside the viewport never scrolls. HOLD TO SAVE counts only a hold
  that began on it, as today, so no drag and no scroll starts a save.
- Opening SETTINGS sets the scroll to 0 px.

HOLD TO SAVE, RESET, LINK and the note line sit below the viewport and do not
move. The note line shows `UNSAVED - HOLD SAVE TO KEEP` whatever the scroll,
and the hold works as on SETUP today: 1000 ms on `ui_hold`, a press that
began on the button, refused while armed (`ui_setup.cpp:407`, moved).

The asserts. `static_assert(LINK_Y >= R_BACKLIGHT + ROW_H)`
(`ui_setup.cpp:95`) names the last of seven fixed rows and goes. In its
place, in `ui_settings.cpp`:

- `VIEW_Y0 >= UI_HDR_H`: the list clears the header;
- `VIEW_Y0 + VIEW_H <= LINK_Y`: the list clears LINK;
- `ROW_PITCH > ROW_H`, and the content, summed at build time from the
  section tables, at least `VIEW_H`, so `MAX_SCROLL_PX` is not negative;
- `ROW_H`, `BTN_W`, the toggle and `FOOT_H` at least `UI_LIST_TAP_MIN`;
- `settingsSectionFits()`, a `constexpr` walk over each section table with
  the font rule of `gfxTextW()`: on every row the label ends at least 4 px
  before the value or the control begins. It sits beside each table: the
  core sections in `ui_settings.cpp`, a tool's section in the tool's header,
  checked again where `ui_registry.cpp` includes it.

The asserts on the note, the footer and the row buttons stay
(`ui_setup.cpp:96-101`). `test_ui.cpp` checks every value a row can show
against the table's widest value.

## The 36 px header

`UI_HDR_H` goes from 30 px to 36 px and BACK from 50 × 22 px to 64 × 32 px
(decision 5). Everything below the header that is derived from it moves
6 px down.

| Item | Header 30 px | Header 36 px |
|---|---|---|
| `UI_HDR_H` | 30 | 36 |
| BACK (`UI_BACK_X`, `UI_BACK_Y`, `UI_BACK_W`, `UI_BACK_H`) | 50 × 22 at x 184, y 4 | 64 × 32 at x 170, y 2 |
| Title, scale 2 | y 8 | y 11; the widest, `AM32 CONFIG`, is 130 px and ends at x 137, 32 px before BACK |
| `UI_STRIP_Y` | 34 | 40 |
| `UI_BODY_Y` | 56 | 62 |

The asserts in `ui_widgets.h` and `ui_widgets.cpp` hold: BACK, 32 px, fits
the 36 px band, and ends at x 233. `static_assert(UI_BACK_W >= UI_TAP_MIN)`
stays, and `static_assert(UI_BACK_H >= UI_TAP_MIN)` joins it: `UI_BACK_H`
stops being `UI_TAP_MIN` and becomes 32. The comment at `ui_widgets.h:60`,
"Sized by UI_TAP_MIN, not by its label", changes to name decision 5's
64 × 32 px.

What it does to each screen:

| Screen | Effect | Change |
|---|---|---|
| Tester | none: no header | none |
| MENU, SETTINGS | new | laid out above for 36 px |
| SD LOG | `LOG_ROW0_Y` is `UI_HDR_H + 8`, 44. Nine rows of 21 px end at y 233, past `LOG_NOTE_Y`, 228, and `static_assert(LOG_ROW0_Y + LOG_ROWS * LOG_ROW_H <= LOG_NOTE_Y)` (`ui.cpp:255`) fails | `LOG_ROW_H` 21 → 20: the rows end at y 224. They are text, not targets. The buttons below do not move |
| AM32 CONFIG | `LIST_Y0` is `UI_BODY_Y`: the viewport is y 62 to 215, 154 px instead of 160 px. The connect page's text starts at y 84 | none from the header; the 40 px rows of decision 5 below |
| SERVO | `static_assert(SERVO_DIAL_Y0 == UI_BODY_Y)` (`ui_servo.cpp:73`) needs `SERVO_DIAL_Y0` 62 (`servo_model.h:96`). Then `static_assert(SERVO_DIAL_Y - SERVO_DIAL_R_OUT >= SERVO_DIAL_Y0)` (`servo_model.cpp:274`) fails: 184 − 124 = 60. The two notes at y 58 and 70 (`ui_servo.cpp:398-404`) would start above the band the dial clears | `SERVO_DIAL_R_OUT` 124 → 122: the hit area ends 26 px beyond the 96 px arc instead of 28 px. The notes move to y 64 and 76, and the arc assert (`ui_servo.cpp:86-87`) becomes `> 82`: the second note ends at row 82, the arc's top tick starts at row 84. The shaft, the arc, the readout, SPEED and the buttons do not move |
| SERVO SET | `SET_LIST_Y0` is `UI_BODY_Y`: viewport y 62 to 215. The two strip lines at `UI_STRIP_Y + 2` and `+ 12` end at row 58 | none from the header; the 40 px rows of decision 5 below |
| SERVO unplug page | text from y 100 | none |
| KST (slice 3) | drawn for the 36 px header in [kst-programming.md](kst-programming.md) | |

The servo dial band fits with 1 px between the second note and the arc. If
a later change needs more, the notes move into the strip, which the servo
screen fills with one caption and a chip.

## Touch targets

The panel's 2.8 in diagonal spans 400 px, so a pixel is about 0.178 mm,
computed from the diagonal; the active area is not checked. 40 px is about
7.1 mm, 32 px about 5.7 mm, and today's `UI_TAP_MIN`, 22 px, about 3.9 mm.

Two floors, in `ui_widgets.h`:

- `UI_LIST_TAP_MIN`, 40 px, new in slice C: every row of a list on
  `ui_list` and every control in a SETTINGS row. Asserted where each list is
  defined. It covers MENU, SETTINGS, the AM32 list and the SERVO SET list
  from slice C, and the KST field list and FILES in slice 3.
- `UI_TAP_MIN`, 22 px, stays for what decision 5 leaves to a later change:
  the tester's button row (33 px tall) and the SERVO screen's controls (MIN,
  CENTRE, MAX and SWEEP 53 × 30 px, A, B and AB 34 × 22 px, HOLD TO START
  and SET 32 px tall, RETRY 100 × 30 px). Two buttons that are neither list
  rows nor row controls stay as they are and go with that change: SD LOG's
  RETRY and USB DRIVE, 104 × 30 px, and AM32's OK on the write result,
  100 × 36 px.

AM32 and SERVO SET use one list idiom, so they move together (decision 16).
Slice A puts both on `ui_list` with their 26 px rows and their press rule,
any visible part of a row. In slice C both go to 40 px rows at a 42 px pitch
and the 40 px partial-visibility rule. Each viewport, y 62 to 215,
154 px, then shows three whole rows instead of five and part of a sixth; the
editor bars and footers do not change. Row text keeps its layout, centred in
the taller row: today the group caption is at +3, the name at +13 and a
scale-2 value at +6 in a 26 px row (`ui_am32.cpp:249-255`,
`ui_servo.cpp:697-702`); in a 40 px row they are at +11, +21 and +13, the
two scale-1 lines and the value each centred in the 39 px the row paints.
The review counted the AM32 editor as large enough and did not list the
rows of either list.

## Gestures

`ui_list` holds the one list gesture. A list on it:

- records the row under a press as a candidate and commits nothing;
- after 6 px of travel (`UI_AXIS_LOCK_PX`) locks to one axis. Vertical: the
  list follows the finger 1:1 from the press point, clamped to its range, and
  selects nothing. Sideways, on a list that has a sideways mode: the
  candidate becomes the selection and the swipe steps it;
- selects the candidate on release when the finger never locked;
- counts only a gesture that began inside the viewport;
- never scrolls under a press that began on a control inside a row
  (SETTINGS' − and + and toggles; slice C, which brings the first row with
  controls);
- clips each row and control to the viewport, and takes a press on a cut one
  only while at least the list's minimum is visible: in slice A any visible
  part, today's rule for AM32 and SERVO SET; from slice C 40 px for every
  list;
- sets the scroll to 0 px when its screen opens.

The sideways swipe (decision 10), built in slice A: one coarse step per
16 px of travel (`UI_SWIPE_STEP_PX`), the anchor moving by whole steps, times
a factor the row gives. `uiSwipeApply()` takes the row's own steps one at a
time while the row allows them, so a coarse step that would pass a rail stops
at the rail, on the field's step. The anchor moves on by whole steps while
the value waits at the rail, so travel past the rail is not stored, the first
coarse step back moves the value off it, and both rails are reachable by
swipe. SERVO SET's steps are those of `stepRow()`, which steps while
`canStep()` allows it, as at 9c0f3b9 (`ui_servo.cpp:611`, `:624`); AM32's are
those of `am32Adjust()`, the steps of − and +.

| List | Factor per coarse step |
|---|---|
| SERVO SET | as today (`coarseOf()`), in the row's steps: 10 for MIN, CENTRE, MAX and the sweep numbers, 1 for the choices |
| AM32 | ⌈range ÷ (11 × step)⌉ of the field's steps (`coarseFactor()`), so a coarse step moves the value by that × step: 11 coarse steps, 176 px, cover any field's range, and values stay on the field's step. A range of 255 at step 1 gives 24 per coarse step, and POLES at step 2 moves 4 poles; the eleventh step lands on the rail, 255, by the clamp. Every range in the field table is a whole number of its steps, so each rail is on the step; the host suite checks both |
| KST field list (slice 3) | from the field table, set in slice 3; a value that breaks a hard limit is skipped, as the steppers skip it |
| MENU, SETTINGS | no sideways mode |

Slice A replaces AM32's swipe at 9c0f3b9, in which 180 px cover a field's
whole range in proportion to the finger's travel (`ui_am32.cpp:602`,
`:695-696`). The text that goes with it: README.md:695, `**Coarse**: 180 px
of travel covers the field's whole range`, and the same row in the wiki pages
AM32-Configuration.md:66 and AM32-Konfiguration.md:68, with the paragraph
under each table.

## Code

| File | What it holds |
|---|---|
| `src/ui_list.{h,cpp}` | slice A: the list: geometry, scroll in pixels, the gesture above, clipping, a row's rectangle on the current frame. Slice C adds the scroll bar, which MENU and SETTINGS draw and AM32 and SERVO SET do not |
| `src/ui_hold.{h,cpp}` | slice A: one 1000 ms hold (`UI_HOLD_MS`, which is `ARM_HOLD_MS`): times only a press that landed on its control while the control was live, a slide off resets the timer, one firing per press, progress 0 % to 100 %, and the 3 px progress bar |
| `src/ui_screen.h` | slice B: `UiScreenId`, `UiControlId`, `UiControl`, and `UiScreen` with its `id` and `controls` members; slice C: the rest of `UiScreen`, `SettingsRow`, `SettingsSection`, `UiMenuEntry` |
| `src/ui_registry.cpp` | slice C: one line per MENU row, in MENU order, and the fit check |
| `src/ui_menu.cpp` | MENU, the BEEP action and its flash (from `ui.cpp`) |
| `src/ui_settings.cpp` | SETTINGS: `ui_setup.cpp` with sections, on `ui_list` and `ui_hold`, plus the pole and ceiling steppers from `ui.cpp`. `ui_setup.{h,cpp}` go |
| `src/ui_sdlog.cpp` | the SD LOG screen, moved out of `ui.cpp` unchanged but for BACK |
| `src/ui.cpp` | the tester and the navigation |
| `src/pin_claim.cpp` | the `PinUser` table |

```c
// ui_screen.h, slice B: the screens as they are, and their controls
enum class UiScreenId : uint8_t {
    Tester, Cfg, Setup, SdLog, Am32, Servo, Kst
};   // slice C: Cfg becomes Menu, Setup becomes Settings, Beep joins

enum : uint8_t { UI_FIXED = 0xFF };   // row of a control outside any list
enum : uint8_t { UI_BACK = 0 };       // BACK, fixed control 0 of every screen
enum class UiPart : uint8_t { Row, Minus, Plus, Toggle };

struct UiControlId {             // which control: (row, part)
    uint8_t row;                 // list row n, or UI_FIXED
    uint8_t part;                // UiPart for a list row; the screen's own
                                 // enum (BACK, HOLD TO SAVE, ...) for UI_FIXED
};
#define UI_ROW(n, p) (UiControlId{ (uint8_t)(n), (uint8_t)UiPart::p })
#define UI_FIX(c)    (UiControlId{ UI_FIXED, (uint8_t)(c) })

struct UiControl {               // one tappable thing on the current frame
    UiControlId id;
    UiRect      rect;            // as drawn, clipped to its list's viewport
    bool        enabled;         // drawn disabled and gated on the same value
};

UiScreenId uiCurrentScreen();    // the open screen
uint8_t uiControls(UiControl *out, uint8_t max);   // its table, this frame
```

Each screen's fixed controls are an enum in its header, numbered from 1:
`TesterCtl`, `CfgCtl` and `SdLogCtl` in `ui.h`, `Am32Ctl` in `ui_am32.h`,
`ServoCtl` for all three SERVO views in `ui_servo.h`, `SetupCtl` and the row
numbers `SetupRow` in `ui_setup.h`. `UI_FIX(CfgCtl::Beep)` is BEEP;
`UI_ROW(SetupRow::Pin, Plus)` is the + of ESC PIN. A list adds its visible
rows with `uiListAddRows()`. The firmware does not call `uiControls()`.

`UI_ROW(3, Plus)` is the + of SETTINGS row 3; `UI_ROW(5, Row)` is AM32 list
row 5, which selects. Slice B gives today's screens, the tester, CFG, SETUP,
SD LOG, AM32 CONFIG, SERVO and SERVO SET (under `Servo`), and the KST stub,
a `UiScreen` with only `id` and `controls`, and `uiCurrentScreen()` reads
today's booleans. Slice C renames the ids and fills in the rest:

```c
// ui_screen.h, slice C

enum class SettingsKind : uint8_t { Stepper, Toggle, ReadOnly };

struct SettingsRow {
    const char  *label;          // scale 1
    SettingsKind kind;
    uint8_t      widest;         // widest value, characters, for the fit check
    void (*value)(char *buf, size_t n, uint16_t *col);
    bool (*enabled)();           // null: always; KISS PIN: while KISS is on
    void (*step)(int dir);       // Stepper: -1 or +1, legal values only
    void (*toggle)();            // Toggle
};

struct SettingsSection {
    const char        *caption;  // ESC TESTER, WIRING, ...
    const SettingsRow *rows;
    uint8_t            count;
};

struct UiScreen {
    UiScreenId id;
    const char *title;                        // header, scale 2
    PinUser     pinUser;                      // None: claims nothing
    void    (*status)(char *buf, size_t n, uint16_t *col);  // MENU row, right
    void    (*enter)();
    bool    (*tick)(const TouchState *t);     // false once BACK has left
    void    (*action)();                      // action row (BEEP); null: tool
    uint8_t (*controls)(UiControl *out, uint8_t max);   // from slice B
    const SettingsSection *settings;          // persisted rows, or null
};

struct UiMenuEntry {             // one MENU row
    const char     *label;       // scale 2
    uint8_t         statusWidest;  // characters, for the fit check
    const UiScreen *screen;
};
```

```c
// ui_registry.cpp: one line per row, in MENU order
const UiMenuEntry UI_MENU[] = {
    { "BEEP THE ESC",  8, &UI_BEEP     },   // ui_menu.cpp
    { "AM32 CONFIG",   9, &UI_AM32     },   // ui_am32.cpp
    { "SERVO TESTER",  9, &UI_SERVO    },   // ui_servo.cpp
    { "KST SERVO",     9, &UI_KST      },   // ui_kst.cpp; added by slice 3
    { "SD LOG",       10, &UI_SDLOG    },   // ui_sdlog.cpp
    { "SETTINGS",      7, &UI_SETTINGS },   // ui_settings.cpp
};
static_assert(menuRowsFit(UI_MENU), "a MENU label runs into its status");
```

The number in each line is the widest status in characters. The tools keep
their entry points (`uiAm32Enter()`, `uiServoTick()` and the others) behind
their `UiScreen`; each file defines its own.

**Navigation.** `ui.cpp` keeps a level, Tester, Menu or Tool, and the open
`UiScreen`, in place of the six booleans. `uiOpenMenu()` is the one route
from the tester to MENU, and it disarms. A tool's `tick()` returning false
returns to MENU and invalidates every cached region, as leaving SETUP does
today. No tool opens another.

**Settings from tools.** A tool's `UiScreen` may point at one section of
persisted rows (decision 8). SETTINGS shows the core sections, ESC TESTER
and WIRING, then each tool's section in registry order, then DISPLAY and
BOARD. Slice C has no tool section; slice 3 adds KST's, SERVO SUPPLY. Values
a tool keeps for one visit, such as the servo profile, stay on the tool's
own page and are not stored ([servo-tester.md](servo-tester.md#decisions),
decision 2).

**Pin users.** `pin_claim.cpp` holds one row per user, and `jobFor()`,
`needsUnplug()` and the AM32 exemption read it:

```c
struct PinUserInfo {
    PinUser  who;
    Core1Job job;        // None: the screen drives the pin from core0 (AM32)
    bool     unplug;     // the unplug page before DShot restarts
    bool     escCheck;   // refused while an ESC answers on SIG
};
static const PinUserInfo PIN_USERS[] = {
    { PinUser::Am32,  Core1Job::None,  false, false },
    { PinUser::Servo, Core1Job::Servo, true,  true  },
    { PinUser::Kst,   Core1Job::Kst,   true,  true  },
};
```

`pinClaimCheck()` stays a pure function and takes the user's row,
`pinUserInfo(who)`, in place of the user. A new pin user is one enum value
and one line here. Slice C adds no user.

## As built

Slice C is built. The code differs from the sketches above in these places:

- `UiScreen::status` has a fourth out parameter, `uint16_t *fill`, the row's
  fill colour. BEEP's flash uses it: `SENT` on `C_INK`, `REFUSED` on
  `C_AMBER`. MENU passes the panel fill, `C_PANEL`, and only BEEP changes it.
- `SettingsRow` has an eighth member, `bool (*on)()`, the filled state of a
  toggle (KISS TELEM, CONTRAST). Null: never filled.
- `PIN_USERS` is in `pin_claim.h`, not `pin_claim.cpp`, and has a row for
  `PinUser::None` as well. `pinClaimCheck()` keeps its `PinUser` parameter and
  reads the table through `pinUserInfo()`, so its signature does not change.
  A `static_assert` (`pinUsersComplete()`) requires one row per user.
- SETTINGS reports a `UI_ROW(n, Row)` for every row over its label and value
  area, x 0 to 137, drawn disabled. A press there scrolls, and a test drag
  can start on it. SETTINGS lays its items out in pixels: it calls
  `uiListInitPx()`, which sets up the list with pixel content and no rows.
  The layout holds at most 10 sections, 40 items and 24 rows.
- MENU's viewport is the rows' width, x 8 to 231, y 62 to 319. The scroll bar
  is drawn at x 235, outside it.
- `menuRowsFit()` takes no argument and walks `UI_MENU` in
  `ui_registry.cpp`. `UI_MENU` is `constexpr`.
- The control enums differ from the sketch. `CfgCtl` does not exist: MENU's
  controls are BACK and `UI_ROW(n, Row)`, so BEEP is `UI_ROW(0, Row)`. The
  tester's third button is `TesterCtl::Menu`. SETTINGS has `SettingsCtl`
  (`Save`, `Reset`) and `SettingsRowId` in `ui_settings.h`, in place of
  `SetupCtl` and `SetupRow`. `UiScreenId` is `Tester`, `Menu`, `Settings`,
  `SdLog`, `Am32`, `Servo`, `Kst` and `Beep`; `Kst` and `Beep` are never the
  open screen.
- Test hooks in `ui_menu.h`: `uiMenuUseRegistry()`, `uiMenuEntries()` and
  `uiMenuRowOf()`.
- `src/ui.cpp` keeps `Level::Tester`, `Level::Menu` and `Level::Tool`. A
  tool's `tick()` returning false returns to MENU.
- SETTINGS shows no tool section yet, since no registry entry has one.

## Interlocks

| Interlock | Where | Through the UI |
|---|---|---|
| Disarm before any tool | `uiOpenMenu()`: disarm, throttle 0, HOLD off, as `ui.cpp:1406-1416` | MENU is the only way to a tool |
| No claim while armed or busy | `pinClaimCheck()` (`pin_claim.h:126-139`), unchanged | Armed unreachable; Busy reachable while Restoring, refused on MENU as `PINS BUSY` |
| No flash write during a claim | `settingsSave()` refuses while a claim holds (`settings.cpp:238`) | SETTINGS is unreachable while a tool is open |
| No flash write while armed | the SETTINGS hold (`ui_setup.cpp:407`, moved) | unreachable: MENU disarms |
| No arming while a USB host holds the card | `handleMainTouch()` (`ui.cpp:1359`), unchanged | SD LOG BACK goes to MENU; the tester stays reachable through MENU's BACK |
| A tool returns to MENU | the navigation in `ui.cpp` | a test walks the registry and checks it |

## Tests by control id

Slice B gives every screen a control table (`UiScreen::controls`) and the
UI a probe, `uiCurrentScreen()`, which returns the open `UiScreenId`, with
ids for today's screens, CFG and SETUP among them ([code](#code)). Slice C
renames those ids. The host tests address controls by id:

- `tapId(id)` and `pressId(id)` in `test/ui_ids.h` look the control up on
  the current frame and fail the test, naming the id, when it is absent or
  gated against the test's expectation (`IdExpect`); `holdId(id, frames)`
  keeps a finger on a control that a `pressId()` began, and fails only when
  the control is absent; a hold timed to the millisecond takes
  `ctlCentre(id)` and keeps its own frame loop;
- `checkControlTable()` checks a frame's table: ids unique, rectangles
  inside the panel and apart, BACK at `uiBackRect()`;
- a list row's controls are `UI_ROW(n, Row)`, `UI_ROW(n, Minus)`,
  `UI_ROW(n, Plus)` and `UI_ROW(n, Toggle)`; `revealRow(n)` drags the list
  until row n is whole, then the test taps the part, so the scroll is
  exercised on the way;
- a navigation test checks `uiCurrentScreen()`, not a pixel;
- tests about geometry (gaps, the viewport's edge, the 40 px partial rule,
  the scroll range) read their coordinates from the control table.

Slice B deletes 65 coordinate macros: 49 in `test_ui.cpp`, 7 in
`test_servo.cpp` and 9 in `test_claim.cpp`. What stays is not a control
coordinate: frame lengths, pin numbers, the throttle's 16 px saturation
margin (`THR_EDGE_SAT_T`, a mapping constant), the note line's y
(`SETUP_NOTE_Y`, a pixel probe of drawn text), a scroll position in rows
for the SET screenshots, and taps where the exact point is the test (the
ends of SPEED, the edges of a list viewport). The tests then survive slice
C's header and layout changes, and slice C changes the tests only where the
behaviour changes.

Slice B found two taps that hit nothing: a second BACK from SD LOG, which
leaves to the tester in one tap, and a stale "CFG BACK" coordinate in the
pressed-look test. Both are taps by id now.

Tests slice C adds:

- SD LOG BACK goes to MENU, from the card-idle and the card-held branch, and
  MENU's BACK then reaches the tester (decision 11). It fails on today's
  code, where BACK reaches the tester. `testUsbDriveScreen` taps SD LOG's
  BACK once and checks that it lands on the tester; slice C adds a second
  tap, MENU's BACK, changes that check to MENU and then the tester, and
  keeps the check that arming is refused while the host holds the card.
- Every registry entry: opened from MENU, BACK lands on MENU, through the
  unplug page for SERVO and KST.
- MENU: disarms on open; BEEP sent and refused flash on the row; no KST row
  in slice C; `PINS BUSY` while Restoring; the fit check's widest statuses
  drawn. Slice C's registry has five rows, so the scroll test runs MENU over
  test-only registries of six and seven entries (`uiMenuUseRegistry()`, a
  test hook in `ui_menu.cpp`): seven rows scroll 36 px, six do not.
- SETTINGS: the scroll range; each row by id; no scroll from a press on a
  control; a cut control under 40 px takes no press; HOLD TO SAVE and the
  UNSAVED note at every scroll position; pole and ceiling steps reach the
  readout of RPM (revolutions per minute) and the tester's `MAX` caption;
  RESET restores them.
- The header: BACK by id on every screen; the SD LOG rows clear the note
  line; the SERVO notes and the dial's hit area at 122 px.

## Slices

| Slice | Changes | Visible change | Leaves working | Estimate, not measured |
|---|---|---|---|---|
| A. Widgets | `ui_list`, `ui_hold`; the AM32 list and SERVO SET on `ui_list` with today's 26 px rows and press rule; AM32's HOLD TO WRITE, SERVO's HOLD TO START, SET's HOLD TO APPLY and SETUP's HOLD TO SAVE on `ui_hold`; `test/test_widgets.cpp` with `runWidgetTests` | AM32's sideways swipe: 16 px per coarse step; README.md:695 and the two wiki rows | every screen as today; HOLD TO ARM keeps its own timer in `ui.cpp` | 2 days |
| B. Control ids | `ui_screen.h` with `UiScreenId` (today's screens), `UiControlId`, `UiControl` and `UiScreen::controls`; a control table per screen; `uiCurrentScreen()`; the test helpers; `test_ui.cpp`, `test_servo.cpp` and `test_claim.cpp` by id; 65 coordinate macros deleted | none | everything; the same screenshots | 2 days |
| C. MENU and SETTINGS | the rest of `ui_screen.h` and the id renames, `ui_registry.cpp`, `ui_menu.cpp`, `ui_settings.cpp` (from `ui_setup.cpp`), `ui_sdlog.cpp` (from `ui.cpp`), the navigation in `ui.cpp`; the `PinUser` table; the 36 px header and BACK 64 × 32 px; `UI_LIST_TAP_MIN`; AM32 and SERVO SET rows 40 px; SD LOG BACK to MENU; `LOG_ROW_H` 20; the servo dial's 122 px and its notes; the tests above; previews, wiki images, README, wiki, design-note and source-comment text | CFG reads MENU; MENU replaces SETTINGS; SETUP becomes SETTINGS with poles and MAX THROTTLE at its top; the disabled KST button has no row until slice 3; every header 6 px taller; AM32 and SERVO SET show three rows | every tool, the settings block as stored | 4 to 5 days |
| 3. KST | [kst-programming.md](kst-programming.md): one registry line, the SERVO SUPPLY section, the field list and FILES on `ui_list`, its holds on `ui_hold`, `SETTINGS_VERSION` 3 | KST SERVO live; SERVO SUPPLY in SETTINGS | | inside slice 3 |

A and B merge in either order; C needs both (decisions 4 and 6). Slice 3
follows C. Slice C's estimate is the review's 3 to 4 days plus the header
consequences and the AM32 and SERVO SET rows, which the review did not
count.

What C does not change: the tester's layout but for its third button's
label, the SERVO screen's controls, the editor bars and footers of AM32 and
SERVO SET, the SD LOG buttons, and every core1 job.

## Storage

The settings block does not change in slices A to C: `SETTINGS_VERSION`
stays 2 (`settings.h:52`), and a block DshotDisplay 2.x saved still loads.
Pole count and ceiling are fields of that block today (`settings.h:70-71`);
SETTINGS edits the same `Settings` and HOLD TO SAVE writes the whole block,
as SETUP does. The order of the rows on screen has no relation to the block
layout. A tool's persisted row is a field in the one block, a `SettingsRow`
in the tool's section and, because the field is new, a version step: slice
3's `SERVO SUPPLY` takes `SETTINGS_VERSION` to 3
([kst-programming.md](kst-programming.md#decisions), decision 8).

## Documentation, wiki and screenshots

README.md and the wiki describe the built system, so slice C changed them,
not slices A and B. The table lists each place by its text; its line numbers
name 9c0f3b9. Slice A changes the AM32 swipe text, the name of the
hold's constant and the source layout only.

| File | Lines today | Change | Slice |
|---|---|---|---|
| README.md | 398, 695, 703-706 | `SAVE_HOLD_MS` → `UI_HOLD_MS`; AM32 coarse swipe: 16 px per coarse step, 176 px for any field's range; the rail rule | A |
| README.md | 25-26 | "its button on the settings screen" → its row on MENU | C |
| README.md | 140, 153, 443, 456, 558, 573, 589-598, 657, 766, 799 | `CFG → SETUP`, `CFG → SD LOG`, `CFG → AM32`, `CFG → SERVO`, "under SETUP" → MENU paths and SETTINGS | C |
| README.md | 308 | the CFG bullet → MENU | C |
| README.md | 324-356 | "Settings screen" → MENU: the rows, BEEP's flash on the row, the strip; today's KST entry (347-348) goes until slice 3 | C |
| README.md | 376-407 | "SETUP screen" → SETTINGS: the sections, the row table with MOTOR POLES and MAX THROTTLE at the top, the scroll | C |
| README.md | 885 | Safety: "Opening CFG disarms" → MENU | C |
| README.md | 925-945 | Layout: `ui_list`, `ui_hold` (A); `ui_setup` → `ui_settings`; `ui_screen.h`, `ui_registry.cpp`, `ui_menu.cpp`, `ui_sdlog.cpp` (C) | A, C |
| README.md | 1012-1026 | the rendered screens: CFG and SETUP shots → MENU and SETTINGS, the scrolled SETTINGS | C |
| wiki The-Screens.md, Die-Bildschirme.md | 1-16, 28, 52, 56-125; 1-16, 28, 53, and the Settings and SETUP sections | the tree, "as set under SETUP", the `CFG` button, Settings → MENU, SETUP → SETTINGS, SD LOG's BACK | C |
| wiki First-Run.md, Erste-Schritte.md | 45, 54, 121, 132; 47, 58, 126, 137 | CFG and SETUP paths, "Both values are stored only when saved" | C |
| wiki Troubleshooting.md, Fehlersuche.md | 30, 91, 151, 158; 33, 97, 162, 169 | CFG and SETUP paths | C |
| wiki Telemetry.md, Telemetrie.md | 84, 130, 154; 87, 137, 163 | "under SETUP", `CFG → SETUP → KISS TELEM` | C |
| wiki Servo-Tester.md, Servotester.md | 44, 52, 129, 195, 209; 47, 55, 138, 208, 223 | `CFG → SERVO`, `CFG → SETUP`, "under SETUP", the unplug page returning to "the settings screen" → MENU; the SET list's 40 px rows | C |
| wiki AM32-Configuration.md, AM32-Konfiguration.md | 66, 72-74, 18, 112-113; 68, 74-76, 18, 116 | the swipe row and the paragraph under it (A); `CFG → AM32`, BACK returning to "the settings screen" → MENU, the 40 px rows (C) | A, C |
| wiki SD-Logging.md, SD-Aufzeichnung.md | 110; 118 | BACK leaves SD LOG for MENU | C |
| wiki Home.md | 60, 69 | `CFG → SETUP` | C |
| CHANGELOG.md | | each slice's entry | A, B, C |
| STATUS.md | 32, 76, 98, 156, 175, 299; the rows "Settings screen layout" (179) and "Settings in flash" (181), the suites table, open item 11 | CFG and SETUP paths, "the settings screens disarms", MENU and SETTINGS; `runUiTests`; `runWidgetTests` (A); open item 11 closed by the SD LOG BACK fix | A, C |
| docs/design/servo-tester.md | 218-235, 281, 285-303, 336 | the SERVO and SET drawings for the 36 px header (strip y 40, dial band 62 to 199, `SERVO_DIAL_R_OUT` 122), SET's 26 px rows → 40 px, the note band rows 34 to 55 → 40 to 61 | C |
| docs/design/blackbox-logging.md | 230, 236, 273 | `CFG → SD LOG` → `MENU → SD LOG`; `src/ui.cpp` → `src/ui_sdlog.cpp` | C |
| docs/design/kiss-telemetry.md | 201-202, 313 | `CFG → SETUP` → `MENU → SETTINGS`, the SETUP stepper → the SETTINGS stepper | C |
| Doxygen comments in `src/` | `config.h:13, 19, 56, 78, 105, 414`; `board_desc.h:36, 83`; `board_pins.h:17`; `esc_task.h:285, 535, 759, 913`; `pin_claim.h:102-104, 115`; `pio_uart_rx.h:59`; `sd_log.h:175`; `settings.h:115`; `ui_widgets.h:60` | CFG, SETUP, the SETTINGS screen and the setup screen → MENU and SETTINGS; BACK's size | C |
| test/test_ui.cpp | 572, 582, 601, 622, 709, 886, 1005, 2617 | `shot_config_*`, `shot_setup*` and `shot_settings*` → `shot_menu*` and `shot_settings*`, with `tools/make_previews.py:45-62` | C |

blackbox-logging.md and kiss-telemetry.md describe built features, and a
design note that does not match the code is a bug (README.md of this
directory), so slice C updates them with the code.

Screenshots: slice C regenerates every preview and wiki image of a screen
with a header, since the header grows: `docs/am32-preview.png`,
`docs/log-preview.png`, `docs/servo-preview.png`, and the wiki images
`am32-*`, `log-*`, `servo-*`. `docs/settings-preview.png` (today's
SETTINGS) becomes `docs/menu-preview.png`, and `docs/setup-preview.png`
becomes `docs/settings-preview.png`, with a scrolled frame; the wiki's
`img/settings.png`, `settings-unsaved.png`, `config-*.png`, `setup.png` and
`setup-contrast.png` become `img/menu*.png` and `img/settings*.png`.
`make_previews.py --wiki` names each wiki image after its shot,
`shot_menu_beep_flash.ppm` → `img/menu-beep-flash.png`, so the renames are
the shot names in `test_ui.cpp`; the old files in `wiki/img/` are deleted by
hand, since nothing removes them. `tools/check_wiki.py` holds no image
names: it checks that every image a page references was rendered, that
links and anchors resolve and that every page has its counterpart in the
other language, and it lists rendered images no page shows without failing.
The tester's images change in one word. README.md names no row height for
the AM32 or SERVO SET lists, so their 40 px rows change only its
screenshots there.

## Risks

1. **Habit.** Pole count and ceiling move from one tap (CFG) to two (MENU,
   SETTINGS). They lead the list, so no scroll is needed for them.
2. **Test rewrite.** `test_ui.cpp` is 2831 lines and `test_servo.cpp` 2205,
   mostly taps by coordinate. Slice B is large and mechanical.
3. **Flash and RAM.** The headroom against the limits of CI (continuous
   integration), flash under 1 MiB (mebibyte, 1,048,576 bytes) and RAM
   (random-access memory) under 400,000 bytes, is not measured here.
4. **Frame cost.** MENU and SETTINGS repaint their viewport on each scroll
   frame, as the AM32 list does today: no new cost class. Not measured.
5. **AM32 and SERVO SET lists.** Three rows at 40 px instead of five at
   26 px: more scrolling to reach a field (decision 5).
6. **Servo dial.** 1 px between the second note and the arc after the header
   grows; the hit area loses 2 px at its outer edge.
7. **Recorded decisions.** pin-claim.md decision 2 and kst-programming.md
   decisions 11, 12 and 15 name the old screens; each carries a revision
   line of 2026-10-18.
8. **German wiki.** Every wiki change is made twice.
9. **The SD LOG shortcut.** BACK from SD LOG reaches the tester in one tap
   today and in two after slice C. Whether the owner relies on it is
   unknown.

## Decisions

Decisions of the owner, 2026-10-18, on the menu review; each recommendation
is accepted.

1. **MENU.** A MENU list replaces the SETTINGS (CFG) screen: tools and
   actions as full-width rows from a registry, `ui_registry.cpp`, one line
   per tool, scrolling once more than six rows exist. CFG on the tester reads
   MENU; opening MENU disarms, as CFG does.
2. **BEEP.** An action row on MENU, with the sent or refused flash on the
   row.
3. **Poles and ceiling.** Motor poles and throttle ceiling move to the top
   of SETTINGS, the ceiling under the label MAX THROTTLE.
4. **Order.** The restructure, slices A (widgets: `ui_list`, `ui_hold`), B
   (tests by control id) and C (MENU and the sectioned SETTINGS), comes
   before KST slice 3. KST then adds one registry line and one settings row,
   and builds its field list and FILES on `ui_list`.
5. **Touch targets.** A floor of 40 px for list and row controls; the header
   36 px and BACK 64 × 32 px. The tester's button row and the servo screen's
   rows are a separate later change.
6. **Tests by control id.** Slice B, before C.
7. **Settings version.** No `SETTINGS_VERSION` step in A to C; KST's SERVO
   SUPPLY row takes it to 3.
8. **Tool settings.** Persisted tool settings sit in the global SETTINGS
   list, contributed by the tool's registry entry; values for one visit,
   such as the servo profile, stay on the tool's page.
9. **KST FILES.** Scrolls instead of paging.
10. **One sideways swipe.** 16 px per coarse step with a factor per row. It
    changes AM32's "180 px covers the range"; README.md:695 and the wiki
    rows name it.
11. **SD LOG BACK.** Goes to MENU, with a test that pins it.
12. **Names.** MENU and SETTINGS.
13. **One SETTINGS list.** One list in sections, ESC TESTER, WIRING, SERVO
    SUPPLY (from slice 3), DISPLAY and BOARD, and no hub of sections, until
    about 20 rows.

Choices made in this design where the decisions above left the choice; the
owner has not decided them:

14. **The list drag.** A drag scrolls only when it starts off a control, and
    a press on a control never scrolls; − and + keep their first step on
    touch-down. Carried from
    [kst-programming.md](kst-programming.md#decisions), decision 15, which
    this supersedes.
15. **What the 36 px header costs.** SD LOG's rows go from 21 px to 20 px;
    the servo dial's hit area from 124 px to 122 px and its two notes 6 px
    down; SETTINGS' fixed footer 4 px down, to y 268, so the viewport keeps
    200 px; MENU's row pitch is 42 px, so six 40 px rows fit under the
    strip. Each is the smallest change that keeps the existing asserts.
16. **Two floors.** `UI_TAP_MIN` stays 22 px for the controls decision 5
    leaves to the later change, and `UI_LIST_TAP_MIN`, 40 px, binds list rows
    and row controls. AM32 and SERVO SET share one list idiom and are treated
    alike: in slice A both keep today's 26 px rows and the any-visible-part
    press rule; in slice C both move to 40 px rows and `UI_LIST_TAP_MIN`.
    The SERVO screen's own controls stay with the later change. SD LOG's
    RETRY and USB DRIVE and AM32's OK are neither list rows nor row controls
    and stay until the later change.
17. **HOLD TO ARM.** Keeps its own timer in `ui.cpp`. It draws its progress
    in the status badge and adds the 250 ms zero-throttle condition; it moves
    to `ui_hold` with the tester's later change.
18. **MENU rows.** `KST SERVO` in place of `KST PROGRAMMER`, which does not
    fit beside `PINS BUSY`; no row for KST before slice 3, as decision 4
    gives KST its registry line there; the strip carries the ESC pin, the
    DShot rate and the EDT chip; SD LOG moves to `ui_sdlog.cpp`, so that
    every tool's `UiScreen` sits in a file of its own and `ui.cpp` holds only
    the tester and the navigation. BEEP's `UiScreen` sits in `ui_menu.cpp`
    with MENU, since BEEP is an action on MENU and has no screen.
19. **Section order and BOARD.** Tool sections sit between WIRING and
    DISPLAY in registry order. The BOARD section's row reads `BUILT FOR`.
