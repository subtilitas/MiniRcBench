# Menu structure C

Status: proposed, not built. The owner has not decided its open questions.

The owner's request of 2026-11-15: "Optimize the Menu structure and make it
similar to the Rcbench structure but adapted to the small display."

Decisions of the owner, 2026-11-15, that this note takes as fixed. They reach
this note through its task and are not yet in [STATUS.md](../../STATUS.md):

- **MENU is the start screen**, in place of the ESC tester.
- **No DShot at power-up.** DShot sends frames only while ESC TESTER or ESC
  COMMANDS is open; back on MENU the frames stop.
- **0.1.0 waits for this work.**
- **ESC TESTER is MENU's first row.**

This note covers the structure: which screens exist, how they are grouped,
where each tool lives, the names, the depth, the navigation, and MENU as the
root. It does not cover three things:

- The DShot lifetime, the pin claim's changes and the core1 jobs. A separate
  design follows this note; [What the structure requires of
  DShot](#what-the-structure-requires-of-dshot) is its input.
- Colours and the look of widgets. `docs/design/ui-theme.md`, on the branch
  `ccr-ui-theme`, plans them. This note names a widget where the structure
  needs one.
- The touch-loss stop of draft pull request #49.

Terms: UI is user interface, ESC electronic speed controller, DShot the
digital ESC protocol the tester drives, EDT Extended DShot Telemetry, eRPM
electrical revolutions per minute, KST and Chaservo two servo brands that
share one programming protocol, Hitec a servo brand with its own protocol,
S.BUS a serial receiver protocol, SD Secure Digital, USB Universal Serial
Bus, IMU inertial measurement unit, GPIO general-purpose input/output, PWM
pulse-width modulation. SIG and AUX are the two signal pins of
[pin-claim.md](pin-claim.md#the-two-pins). Header, strip and body are the
bands of [menu.md](menu.md): the header is y 0 to 35, the strip starts at
y 40 (`UI_STRIP_Y`), the body at y 62 (`UI_BODY_Y`).

Text widths follow `textW()` in `src/ui_registry.cpp`, which is
`gfxTextW()`'s rule: (6 × characters − 1) × scale. "Ends at x" in the tables
is the start plus the width, as in [menu-b.md](menu-b.md#menu).

MiniRcBench is read at `main` cff09ae. rcbench is read at `origin/main`
a2d10ccf; the checkout read is d75113c1, whose `shared/ui/` is the same
(`git diff --stat d75113c1 a2d10ccf -- shared/ui` is empty).

## rcbench's structure

rcbench's panel is 1024 x 600 px. The router owns a 60 px status band and
hands each screen the 1024 x 540 px under it
(`shared/ui/include/ui_screen.h`, the file comment).

```
SPLASH                      start-up steps; opens the menu after 1.6 s
 └─ MENU (title "rcbench")  8 tiles, 4 x 2; no home tag            depth 0
     ├─ MOTOR & ESC         workbench; the tile opens LIVE          1
     │   ├─ LIVE            task tab: PLOT, TABLE, BALANCE views    1
     │   ├─ PROGRAM         task tab: the list of ESC programmers   1
     │   │   ├─ ESC STICK   session over the whole body             2
     │   │   └─ signal wire AM32, Bluejay, BLHeli_S, ESCape32       2
     │   └─ OPTIONS         footer: the 8 motor and pack settings   2
     ├─ SERVO               workbench; the tile opens the hub       1
     │   ├─ LIVE, SWEEP, TEST, LIMIT RUN                            2
     │   ├─ RUDDER, RUDDER AUTO   deflection in degrees and mm      2
     │   ├─ KST, HITEC      the programmers, one tile each          2
     │   └─ OPTIONS         footer: the servo profile               2
     ├─ SUPPLY              workbench                               1
     ├─ RECEIVER            instrument: CHANNELS, RAW DATA tabs     1
     ├─ BATTERY             instrument                              1
     ├─ BALANCE             instrument                              1
     ├─ LOGS                strip of two tabs: LOGS, FILES          1
     └─ SETUP               hub of up to 9 tiles                    1
         ├─ PANEL, SENSORS, SD CARD, WIRELESS   category lists      2
         └─ OUTPUTS, WI-FI, UPDATES, IMU        screens             2
```

What the code and the manual state:

| Fact | Where |
|---|---|
| The menu is the first screen after the splash. | `docs/Screens.md`, "Splash" |
| Eight tiles "by kind, each in a fixed place": row 1 the workbenches MOTOR & ESC, SERVO, SUPPLY, which drive outputs; row 2 the instruments RECEIVER, BATTERY, BALANCE, which only read; the last column LOGS over SETUP. | `shared/ui/overview_screen.c`, `k_workbenches`, `k_instruments`, `k_last_column` (lines 101 to 133) |
| "A maker, a protocol, a run or a setting never adds a tile; only a new device class or instrument does". A build-time check refuses a fourth workbench or instrument. | the same file, the comment over the tables and the `_Static_assert`s under them |
| Programming is a task of a workbench, not a tile: MOTOR & ESC > PROGRAM and SERVO's KST and HITEC tiles. | `shared/ui/include/overview_screen.h`; `shared/ui/include/prog_registry.h` |
| A new maker adds one registry row, "never a home tile, a SETUP tile or a tab". | `shared/ui/include/prog_registry.h` |
| MOTOR & ESC's tile always opens LIVE; LIVE and PROGRAM are tabs and add no navigation level. | `docs/Screens.md`, "Motor & ESC" |
| SERVO opens a hub of eight tiles "in the order of use"; the hub drives nothing. | `docs/Screens.md`, "Servo"; `shared/ui/include/servo_screen.h`, `SERVO_FN_*` (lines 41 to 50) |
| A tile whose tool cannot open is drawn grey with its reason in place of its line, and a tap names the reason. | `docs/Screens.md`, "Programmer" |
| The home tag, the screen's title at the band's left, climbs one level: a list or a keypad closes first, a function screen returns to its hub, a hub to the menu. | `ui_screen_t::back` in `shared/ui/include/ui_screen.h`; `ui_router_parent()` in `shared/ui/ui_router.c` (line 326) |
| A screen with several openers returns to the one that opened it. | `k_openers` in `shared/ui/ui_router.c` (lines 181 to 205) |
| The menu has no home tag. | `has_home()` in `shared/ui/ui_router.c` (line 707) |
| Leaving a bench screen switches the bench to SAFE. | `ui_screen_t::leave` in `shared/ui/include/ui_screen.h`; `docs/Screens.md`, "Servo" |
| A tool's own settings open from OPTIONS in its footer; bench-wide settings are in SETUP. | `docs/Screens.md`, "The footer and the settings list" |
| SETUP is a hub of tiles: four category lists and four screens of their own. | `k_tiles` in `shared/ui/settings_screen_int.h` (lines 158 to 167) |
| STOP is on every screen, in the band. | `docs/Screens.md`, "The status band" |

rcbench has no screen for a USB bridge to a desktop ESC tool and none that
sends S.BUS: a search of `shared/ui/` and `docs/` for "passthrough" and for
the S.BUS encoder finds neither. Its RECEIVER screen has no data source
(`docs/Screens.md`, "Receiver"). Which rcbench screen sends the DShot
commands that `docs/BLHeli32.md` lists is not verified here.

The rules that carry over to a small display:

1. The menu is the root and is grouped by device class, not by tool.
2. Workbenches first, then instruments, then logs and settings.
3. A device class opens a hub; a tool is one level under its hub.
4. A new brand, protocol or tool adds a hub entry, never a menu entry.
5. The way back climbs one level and is in the same place on every screen.
6. A tool that cannot open stays visible, drawn disabled, with its reason.

## MiniRcBench's structure at cff09ae

The panel is 240 x 320 px. The tester is the root (`Level::Tester` in
`src/ui.cpp`). Depth counts taps from the tester.

```
TESTER (root, no header)                                         depth 0
 ├─ MENU button: disarms ──> MENU                                1
 ├─ 14 POLES or MAX 20% (tap, disarmed) ──> SETTINGS             1
 └─ MENU: eight rows, scrolls 78 px          BACK ──> TESTER
     ├─ ESC COMMANDS       DShot commands; claims nothing        2
     ├─ ESC CONFIG         chooser                               2
     │   ├─ ESC SETTINGS   claims SIG; HEX, WRITE, leave page    3
     │   └─ USB BRIDGE     claims SIG; leave page                3
     ├─ SERVO TESTER       claims SIG, AUX for two; SET, leave   2
     ├─ SERVO PROGRAMMER   chooser, titled PROGRAMMER            2
     │   ├─ KST / CHASERVO     KST SERVO: list, FILES, WRITE     3
     │   ├─ HITEC D SERIES     HITEC SERVO: the same views       3
     │   └─ HITEC CLASSIC      needs the switched supply         3
     ├─ RX SIGNALS         chooser                               2
     │   ├─ MONITOR        S.BUS MONITOR: claims AUX's role      3
     │   └─ OUTPUT         S.BUS OUTPUT: claims SIG's role; SET  3
     ├─ TOOLS              chooser with one entry                2
     │   └─ ANGLE METER    claims nothing; SETUP, FLIP CAL       3
     ├─ SD LOG             6 px show at scroll 0                 2
     └─ SETTINGS           hidden at scroll 0                    2
```

| What | Where in the code |
|---|---|
| The eight rows | `UI_MENU` in `src/ui_registry.cpp` |
| Rows 40 px at a 42 px pitch in a 258 px viewport; six fit; eight scroll 8 × 42 − 258 = 78 px | `MENU_ROW_H`, `MENU_ROW_PITCH`, `MENU_VIEW_H` and the `static_assert`s in `src/ui_menu.cpp`; `uiListMaxScroll()` in `src/ui_list.cpp` |
| A row cut below 40 px takes no press | `uiListSetBody(&s_list, MENU_ROW_H, UI_LIST_TAP_MIN)` in `uiMenuEnter()` |
| Four choosers, each a file, an entry type and a fit check of its own | `src/ui_servo_prog.cpp`, `src/ui_esc_config.cpp`, `src/ui_rx_signals.cpp`, `src/ui_tools.cpp`; `UiProgEntry`, `UiEscCfgEntry`, `UiRxEntry`, `UiToolsEntry`; `progEntriesFit()`, `escCfgEntriesFit()`, `rxEntriesFit()`, `toolsEntriesFit()` |
| A chooser entry is a 228 x 46 px button at an 80 px pitch from y 62 with scale-1 lines under it; three fill the page | `UI_PROG_ENTRY_*`, `UI_PROG_LINE_*` in `src/ui_servo_prog.h` |
| A tool chosen on a chooser opens with MENU as its origin: its BACK and its RESTART DSHOT return to MENU, not to the chooser | `uiChooseTool()` and the `s_chosen` branch of the frame function in `src/ui.cpp` |
| SETTINGS opened from a readout returns to the tester | `s_toolFrom` in `src/ui.cpp` |
| A row or readout is refused while the pin claim is not Free, for a screen with a `pinUser` or with `freePins` | `uiPinsRefuse()` in `src/ui_screen.h` |
| MENU's strip: the pins and the DShot rate, a chip `EDT ON`, `EDT OFF` or `NO ESC` from the telemetry, and the run cue after a bootloader session | `drawMenu()` and `uiMenuTick()` in `src/ui_menu.cpp` |

Sub-views inside a tool keep their own BACK rules of
[menu-b.md](menu-b.md#navigation): BACK leaves a sub-view for its parent and
a claiming tool's main view for its leave page.

What does not hold up:

- **MENU scrolls 78 px.** SETTINGS is not on the panel at scroll 0 and
  SD LOG shows 6 px. Both need a drag before the tap.
- **A row per tool, then a row per group, mixed.** ESC COMMANDS and SERVO
  TESTER are tools on MENU; ESC CONFIG, SERVO PROGRAMMER, RX SIGNALS and
  TOOLS are groups. Two rows hold the ESC and two hold the servo.
- **TOOLS is a chooser with one entry.** Its tap chooses nothing.
- **BACK from a chosen tool skips its chooser.** The way in has two steps and
  the way out has one.
- **Four copies of one chooser.** A fifth group would add a fifth.

## The proposed structure

MENU is the root. It has six rows in rcbench's order, and it does not
scroll. The first row is the owner's ESC TESTER. The others are rcbench's
classes: the ESC's programming, SERVO, RECEIVER, the log and the settings.
Three rows open a hub, the one chooser widget; every tool is at most two
taps from power-up.

```
SPLASH                      as today
 └─ MENU (root, no BACK)    six rows, no scroll                  depth 0
     ├─ ESC TESTER          no header; MENU button ──> MENU      1
     │   └─ 14 POLES or MAX 20% (tap, disarmed) ──> SETTINGS     2
     ├─ ESC CONFIG          hub, 3 entries                       1
     │   ├─ ESC COMMANDS    claims nothing                       2
     │   ├─ ESC SETTINGS    claims SIG; HEX, WRITE, leave page   2
     │   └─ USB BRIDGE      claims SIG; leave page               2
     ├─ SERVO               hub, 5 entries, scrolls 146 px       1
     │   ├─ SERVO TESTER    claims SIG, AUX for two; SET, leave  2
     │   ├─ ANGLE METER     claims nothing; SETUP, FLIP CAL      2
     │   ├─ KST / CHASERVO  KST SERVO                            2
     │   ├─ HITEC D SERIES  HITEC SERVO                          2
     │   └─ HITEC CLASSIC   HITEC SERVO; needs the supply        2
     ├─ RECEIVER            hub, 2 entries                       1
     │   ├─ MONITOR         S.BUS MONITOR                        2
     │   └─ OUTPUT          S.BUS OUTPUT; SET                    2
     ├─ SD LOG                                                   1
     └─ SETTINGS            one list in sections, as today       1
```

How it follows rcbench, and where it differs:

| rcbench | MiniRcBench | Why |
|---|---|---|
| The menu is the root, with no home tag | MENU is the root, with no BACK | the owner's decision; nothing lies behind a root |
| MOTOR & ESC, one tile, tasks LIVE and PROGRAM as tabs | two rows, ESC TESTER and ESC CONFIG | the owner's first row; the tester has no header and no room for a tab strip, and a tab 40 px high does not fit over three hub entries (62 + 40 + 4 + 2 × 80 + 81 = 347 px) |
| SERVO, a hub of tiles in the order of use | SERVO, a hub of entries in the same order: drive, measure, program | the same |
| RUDDER on the SERVO hub | ANGLE METER on the SERVO hub | the surface throw is measured where the servo is driven; TOOLS and its one entry go |
| RECEIVER, an instrument tile | RECEIVER, a hub of MONITOR and OUTPUT | the name fits both: OUTPUT stands in for a receiver |
| LOGS | SD LOG | rcbench's LOGS browses and plots files; SD LOG starts and stops the recorder and browses nothing, so its name stays |
| SETUP, a hub of categories | SETTINGS, one list in sections | 15 rows in 6 sections; [menu.md](menu.md#decisions) decision 13 keeps one list until about 20 rows |
| SUPPLY, BATTERY, BALANCE | none | no such tool exists |
| STOP in the band on every screen | none | every output here stops on the screen that drives it; the touch-loss stop is pull request #49 |
| A new maker adds a registry row | a new brand, protocol or tool adds a hub entry | the same rule; MENU gains a row only for a new device class |

### MENU

```
y   0 | MENU                                      |  header 36 px; no BACK
   40 | SIG GP0  AUX GP1  DSHOT600                |  strip, C_DIM
   62 | ESC TESTER                                |  row 0: x 8..231, 40 px
  104 | ESC CONFIG                              > |  row 1: hub
  146 | SERVO                                   > |  row 2: hub
  188 | RECEIVER                                > |  row 3: hub
  230 | SD LOG                           NO CARD  |  row 4
  272 | SETTINGS                         UNSAVED  |  row 5: ends at y 311
  320 +-------------------------------------------+
```

Six rows take 6 × 42 = 252 px of the 258 px viewport, so
`uiListMaxScroll()` returns 0 and the `static_assert` "six MENU rows do not
fit the viewport" in `src/ui_menu.cpp` covers the layout. Row i starts at
y 62 + 42 × i. The last row ends at y 311; 8 px stay free under it.

| Row | Label, scale 2 from x 16 | Ends at x | Widest status, scale 1, right to x 222 | Starts at x |
|---|---|---|---|---|
| ESC TESTER | 10 characters, 118 px | 134 | none | |
| ESC CONFIG | 10, 118 px | 134 | `>`, 1, 5 px | 217 |
| SERVO | 5, 58 px | 74 | `>` | 217 |
| RECEIVER | 8, 94 px | 110 | `>` | 217 |
| SD LOG | 6, 70 px | 86 | `CARD ERROR`, 10, 59 px | 163 |
| SETTINGS | 8, 94 px | 110 | `UNSAVED`, 7, 41 px | 181 |

`menuRowsFit()` holds for each: the label's end plus 8 px
(`UI_MENU_LABEL_GAP`) is at most the status' start. The widest label,
118 px, is 72 px narrower than `SERVO PROGRAMMER`, 190 px, the row that
forced the tool rows to drop their status
([menu-b.md](menu-b.md#decisions) decision 18).

`>` marks a row that opens a hub, as rcbench's row `THROTTLE OUTPUT >`
marks a session one level down.
It is a status text in `C_DIM`; its look belongs to `ui-theme.md`.

The header reads `MENU` and draws no BACK. `MENU` at scale 2 is 46 px.

The strip keeps its caption, `SIG GP0  AUX GP1  DSHOT600`, 26 characters,
155 px; the widest, `SIG GP1  AUX GP0  DSHOT1200`, is 161 px. These are
settings, not readings. The chip `EDT ON`, `EDT OFF` or `NO ESC` and the
run cue `POWER-CYCLE THE ESC TO RUN IT` read the telemetry of a running
DShot; with no DShot on MENU they have no source. The chip goes from MENU.
Where the run cue goes is a question for the DShot design
([requirement 6](#what-the-structure-requires-of-dshot)).

A row is drawn disabled and takes no tap while `uiPinsRefuse()` refuses its
screen, as today. ESC TESTER has `freePins` set, as ESC COMMANDS has: DShot
needs the pins. A hub row is never refused; the hub gates each entry
([choice 4](#decisions)).

### The hub

One widget, `ui_hub`, replaces the four choosers. A hub is a title, a strip
and a table of entries; its look belongs to `ui-theme.md`. The geometry is
PROGRAMMER's, kept: an entry is a 228 x 46 px button at x 6 with its label
at scale 2, at an 80 px pitch from y 62, with its lines at scale 1 from x 8
starting 50 px under the entry's top at a 12 px pitch.

One entry type replaces the four. It is `UiProgEntry` with the two plain
lines of the other three:

| Field | Meaning | From |
|---|---|---|
| `label` | scale 2, at most 18 characters (214 px in the 228 px button) | all four |
| `lines[2]` | scale 1, `C_DIM`, at most 37 characters; `models` is the first | all four |
| `screen`, `enter` | the tool, and its entry point for this route | `UiProgEntry` |
| `enabled`, `why[2]` | the entry's own gate and the amber lines that say why | `UiProgEntry` |
| `powerOwn`, `powerSwitchedFmt` | the power line by SERVO SUPPLY | `UiProgEntry` |

Up to three entries fill the page, as today. With more, the hub is a list
on `ui_list` with 80 px rows, a 46 px body and the 40 px press floor
(`uiListInit()`, `uiListSetBody()`), as
[menu-b.md](menu-b.md#programmer-the-chooser) foresees for a fourth entry.
The scroll bar is `uiListDrawBar()` at x 235, as MENU's; the buttons end at
x 233.

One fit check, `hubEntriesFit()`, replaces the four. Its rule is today's: a
label within 18 characters, a line within 37, and an entry's lines ending
4 px or more above the next entry, so at most two lines (the second ends
69 px under the entry's top, the next entry starts at 80). The last entry
may carry three lines, ending 81 px under its top; a scrolling hub's content
is then entries × 80 + 4 px, which `UiList::contentPx` holds.

An entry acts on one value, which also draws it and fills the control
table: its own `enabled` holds, and `uiPinsRefuse()` does not refuse its
screen. An entry whose `enabled` fails shows its `why` lines under it; the
strip says why while the claim refuses.

The three hubs:

| Hub | Title, scale 2 from x 8 | Strip | Entries |
|---|---|---|---|
| ESC CONFIG | 10 characters, 118 px, ends x 126 | `SIG GP0`, as today | 3 |
| SERVO | 5, 58 px, ends x 66 | `SIG GP0  AUX GP1  SERVO SUPPLY GP28`, 35 characters, 209 px, ends x 217 | 5 |
| RECEIVER | 8, 94 px, ends x 102 | `SIG GP0  AUX GP1`, as today | 2 |

Each title ends 4 px or more before BACK at x 170 (`UI_BACK_X`), the rule of
the choosers' `static_assert`s; a title holds 13 characters there.

| Hub | Entry | Lines under it | Characters, width |
|---|---|---|---|
| ESC CONFIG | `ESC COMMANDS` | `BEEP 1 TO 5, DIRECTION, 3D MODE.` | 32, 191 px |
| | | `SENT AS DSHOT COMMANDS.` | 23, 137 px |
| | `ESC SETTINGS` | the two lines of today | 36 at most |
| | `USB BRIDGE` | the two lines of today | 28 at most |
| SERVO | `SERVO TESTER` | `ONE SERVO ON SIG, OR TWO WITH AUX.` | 34, 203 px |
| | `ANGLE METER` | `INCIDENCE, DECALAGE, DIHEDRAL, THROW`, as today | 36, 215 px |
| | `KST / CHASERVO` | the power line of today | 33 at most |
| | `HITEC D SERIES` | the models and the power line of today | 33 at most |
| | `HITEC CLASSIC` | the models, then the power line or the two amber lines of today | 36 at most |
| RECEIVER | `MONITOR`, `OUTPUT` | the lines of today | 30 at most |

The three new lines are within 37 characters, 221 px. They state what the
code does: ESC COMMANDS sends beacons 1 to 5, the direction and the 3D mode
as DShot commands ([esc-commands.md](esc-commands.md)); the servo tester
drives one servo on SIG or two on SIG and AUX
([servo-tester.md](servo-tester.md)).

SERVO at scroll 0:

```
y   0 | SERVO                           [  BACK  ]|
   40 | SIG GP0  AUX GP1  SERVO SUPPLY OWN        |  strip, C_DIM
   62 | [          SERVO TESTER           ]       |  entry 0: 6, 62, 228 x 46
  112 | ONE SERVO ON SIG, OR TWO WITH AUX.        |
  142 | [           ANGLE METER           ]       |  entry 1
  192 | INCIDENCE, DECALAGE, DIHEDRAL, THROW      |
  222 | [         KST / CHASERVO          ]       |  entry 2
  272 | POWER THE SERVO UP AFTER THE TAP.         |
  302 | [         HITEC D SERIES          ]       |  entry 3: 18 px show
  320 +-------------------------------------------+
```

SERVO at its full scroll, 146 px:

```
   62 |                                           |  ANGLE METER is above
   76 | [         KST / CHASERVO          ]       |  entry 2
  126 | POWER THE SERVO UP AFTER THE TAP.         |
  156 | [         HITEC D SERIES          ]       |  entry 3
  206 | D485HW, D645MW AND THE OTHERS             |
  218 | POWER THE SERVO UP AFTER THE TAP.         |
  236 | [          HITEC CLASSIC          ]       |  entry 4: disabled
  286 | HS-5XXX, HS-7XXX                          |
  298 | NEEDS THE CONVERTER OF WIRING PART C      |  C_AMBER
  310 | AND SETTINGS > SERVO SUPPLY GP28/29.      |  ends at y 316
  320 +-------------------------------------------+
```

The content is 5 × 80 + 4 = 404 px in the 258 px viewport, so the scroll
range is 404 − 258 = 146 px. Entry i starts at y 62 + 80 × i − scroll. At
scroll 0 three entries are whole and HITEC D SERIES shows 18 px, less than
the 40 px press floor: it is listed and takes no press, and it shows that
the list goes on. HITEC D SERIES is whole from 28 px of scroll (its button
ends at y 320) and HITEC CLASSIC from 108 px.

ESC CONFIG and RECEIVER do not scroll: 3 and 2 entries.

### Navigation

BACK climbs one level, and a screen returns to the one that opened it.

| View | BACK, or the way back |
|---|---|
| MENU | none: the header draws no BACK |
| ESC TESTER | no header; the `MENU` button disarms, zeroes the throttle, turns LATCH off and returns to MENU, as today |
| SETTINGS | where it was opened from: MENU, or ESC TESTER after a readout tap |
| SD LOG | MENU |
| ESC CONFIG, SERVO, RECEIVER | MENU |
| ESC COMMANDS, ANGLE METER | their hub |
| A claiming tool, claim refused or not yet held | its hub; today MENU |
| A claiming tool's main view | its leave page, as today |
| The leave page's last button, today RESTART DSHOT | the hub; today MENU |
| A sub-view: SET, HEX, WRITE, FILES, SETUP, FLIP CAL | its parent, as today |

The navigation is a stack of at most three screens: MENU; a hub, ESC TESTER,
SD LOG or SETTINGS; a tool, or SETTINGS over ESC TESTER. BACK pops one. It
replaces `Level`, `s_toolFrom` and the rule that a chosen tool's origin is
MENU.

A hub keeps its scroll while a tool it opened is on top, so BACK from HITEC
CLASSIC returns to the entry that was tapped.

ESC TESTER opens disarmed, at throttle 0, with LATCH off. Its layout, its
button row and its readouts do not change. Its button keeps the label
`MENU`: the label names where the tap leads.

### Tap counts

Taps from power-up, after the splash. "Drag" is a scroll before the tap.

| Tool | At cff09ae | Proposed |
|---|---|---|
| ESC TESTER | 0 | 1 |
| ESC COMMANDS | 2 | 2 |
| ESC SETTINGS | 3 | 2 |
| USB BRIDGE | 3 | 2 |
| SERVO TESTER | 2 | 2 |
| ANGLE METER | 3 | 2 |
| KST SERVO | 3 | 2 |
| HITEC SERVO, D series | 3 | 2, whole after a drag of 28 px |
| HITEC SERVO, classic | 3 | 2, after a drag of 108 px |
| S.BUS MONITOR | 3 | 2 |
| S.BUS OUTPUT | 3 | 2 |
| SD LOG | 2, after a drag of 34 px | 1 |
| SETTINGS | 2, after a drag of 76 px; 1 from a readout | 1; 2 through a readout |

Taps from MENU: ESC COMMANDS and SERVO TESTER go from 1 to 2. Every other
tool keeps its count, and SD LOG and SETTINGS lose their drag.

### Every screen, old place and new place

| Screen (`UiScreenId`) | Title | Old place | New place |
|---|---|---|---|
| `Tester` | none | the root | MENU row 1, ESC TESTER |
| `Menu` | `MENU` | behind the tester's MENU button | the root |
| `EscCmd` | `ESC COMMANDS` | MENU row 1 | ESC CONFIG, entry 1 |
| `EscConfig` | `ESC CONFIG` | MENU row 2, a chooser of two | MENU row 2, a hub of three |
| `Am32` | `ESC SETTINGS`, `ESC HEX`, `ESC WRITE` | ESC CONFIG, entry 1 | ESC CONFIG, entry 2 |
| `Bridge` | `USB BRIDGE` | ESC CONFIG, entry 2 | ESC CONFIG, entry 3 |
| `Servo` | `SERVO TESTER`, `SERVO SET` | MENU row 3 | SERVO, entry 1 |
| `Angle` | `ANGLE METER`, `ANGLE SETUP`, `FLIP CAL` | TOOLS, entry 1 | SERVO, entry 2 |
| `ServoProg` | `PROGRAMMER` | MENU row 4 | gone: its three entries are SERVO's entries 3 to 5 |
| `Kst` | `KST SERVO` and its sub-views | PROGRAMMER, entry 1 | SERVO, entry 3 |
| `Hitec` | `HITEC SERVO` and its sub-views | PROGRAMMER, entries 2 and 3 | SERVO, entries 4 and 5 |
| `RxSignals` | `RX SIGNALS` | MENU row 5 | MENU row 4, titled `RECEIVER` |
| `Sbus` | `S.BUS MONITOR` | RX SIGNALS, entry 1 | RECEIVER, entry 1 |
| `SbusOut` | `S.BUS OUTPUT`, `OUTPUT SET` | RX SIGNALS, entry 2 | RECEIVER, entry 2 |
| `Tools` | `TOOLS` | MENU row 6 | gone |
| `SdLog` | `SD LOG` | MENU row 7 | MENU row 5 |
| `Settings` | `SETTINGS` | MENU row 8; the tester's readouts | MENU row 6; the tester's readouts |

No tool's own views change. The leave pages, the write pages, FILES, SET,
HEX, ANGLE SETUP and FLIP CAL stay as built.

## What the structure requires of DShot

Input for the design of the DShot lifetime. This note decides none of it.

1. **Screens that need DShot running:** ESC TESTER and ESC COMMANDS. No
   other screen sends a frame or reads the ESC's telemetry for its own work.
2. **MENU and the three hubs run without DShot.** They are open with the
   claim Free and nothing on SIG. What the claim's states mean then, and
   whether "not Free" still exists on MENU, is that design's.
3. **One gate.** MENU and the hubs refuse a row or an entry through
   `uiPinsRefuse()` only. ESC TESTER and ESC COMMANDS have `freePins` set.
4. **ESC TESTER's readouts open SETTINGS.** By the owner's rule DShot stops
   there and starts again on BACK. SETTINGS can change SIG's pin and the
   DShot rate meanwhile (`applyLive()`).
5. **SETTINGS' LINK line,** `LINK  998/S  0% ERR`, reads the running
   DShot's telemetry (`src/ui_settings.cpp`, `LINK_Y`). Without DShot it has
   no source.
6. **MENU's run cue.** `uiAm32RunCue()`, `usbBridgeRunCue()` and
   `sbusOutRunCue()` clear on the first eRPM frame, which MENU no longer
   receives.
7. **The texts that name a DShot restart:** the leave page's `RESTART
   DSHOT`, `DSHOT RESTARTS ON SIG GP0 AND READS` and `AS A 970 US PULSE AT
   1000 HZ.` (`src/ui_leave.h`), MENU's `DSHOT RESTARTING ON SIG GP0`
   (`UI_MENU_RESTARTING_FMT`), `WAIT: DSHOT RESTARTING`
   (`UI_WAIT_RESTART_TEXT`) and HITEC SERVO's `GP0 IS THE SERVO'S: DSHOT IS
   OFF.` With no DShot behind a leave page these state something that no
   longer happens. The structure requires only that the leave page's last
   button returns to the hub.
8. **The splash** waits "while the ESC finishes booting" (`src/main.cpp`).
   With no DShot at power-up the reason changes.
9. **The first frame.** DShot starts on the tap on ESC TESTER or on ESC
   COMMANDS. The time from that tap until the ESC answers is not measured.
10. **The log.** The recorder follows the tester's arm state
    (`sdLogSetArmed()` in `src/ui.cpp`). SD LOG, the screen, needs no DShot.
11. **Leaving ESC TESTER disarms first,** by the `MENU` button and by a
    readout tap, as `uiOpenMenu()` and `uiOpenTool()` do today.

## Decisions

Decisions of the owner, 2026-11-15, as listed at the top: MENU is the start
screen; no DShot at power-up; 0.1.0 waits; ESC TESTER is MENU's first row.

Choices of this note. The owner has not decided them; those marked with a
question are [open questions](#open-questions).

1. **Six rows, no scroll.** ESC TESTER, ESC CONFIG, SERVO, RECEIVER, SD
   LOG, SETTINGS (questions 1 and 2).
2. **Three hubs on one widget.** ESC CONFIG holds ESC COMMANDS, ESC SETTINGS
   and USB BRIDGE; SERVO holds SERVO TESTER, ANGLE METER and the three
   programmer entries; RECEIVER holds MONITOR and OUTPUT. PROGRAMMER and
   TOOLS go (questions 2 and 3).
3. **A screen returns to its opener.** A hub's tool returns to the hub, by
   BACK and by the leave page's last button (question 5).
4. **The gate is at the entry.** A hub opens whatever the claim's state and
   draws the entries `uiPinsRefuse()` refuses disabled. The hubs lose
   `freePins`.
5. **The root has no BACK.** The header draws none, as rcbench's menu has no
   home tag. A BACK drawn disabled for good would be a control that never
   acts.
6. **The tester keeps its `MENU` button** and gains no header.
7. **MENU's strip loses the EDT chip.** The caption stays.
8. **Names.** `SERVO` and `RECEIVER` are rcbench's. `ESC CONFIG`, `SD LOG`
   and `SETTINGS` stay (question 4).
9. **`>` marks a hub row.**
10. **SETTINGS stays one list.** rcbench's SETUP hub is the form to take
    once the list passes about 20 rows, by
    [menu.md](menu.md#decisions) decision 13. It has 15.

## Decisions of menu-b.md this revises

[menu-b.md](menu-b.md) is not edited now. The build adds each line below to
the decision it names, when the slice that changes it merges.

| Decision of menu-b.md | Whose | The line the build adds |
|---|---|---|
| 1, six rows that never scroll; 33, seven rows scroll 36 px; the revision of 2026-11-15 under [MENU](menu-b.md#menu), eight rows scroll 78 px | owner | Revised by menu-c.md: MENU is the start screen and has six rows, ESC TESTER, ESC CONFIG, SERVO, RECEIVER, SD LOG, SETTINGS; it does not scroll. |
| 2, SERVO PROGRAMMER and its chooser | owner | Revised by menu-c.md: no MENU row reads SERVO PROGRAMMER and no screen is titled PROGRAMMER. KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC are entries 3 to 5 of the hub SERVO, with the lines and the gate of this decision. A future brand adds a hub entry. |
| 4, SETTINGS from the tester | owner | not revised |
| 5, BACK goes one level up, or is drawn disabled | this note's rule; the owner confirmed the leave page's BACK (25) | Revised by menu-c.md: MENU, the root, draws no BACK. A tool opened from a hub returns to the hub, where this decision's table says MENU. |
| 13, a title names its screen | rule | Revised by menu-c.md: the hubs are titled ESC CONFIG, SERVO and RECEIVER; PROGRAMMER and TOOLS are gone. |
| 16 and 23, MENU order by device | 23 owner | Revised by menu-c.md: the order by device stays, one row per device class after ESC TESTER: ESC TESTER, ESC CONFIG, SERVO, RECEIVER, SD LOG, SETTINGS. |
| 18, MENU statuses report state | choice | Revised by menu-c.md: a row that opens a hub carries `>`. |
| 19, the chooser opens a tool | choice | Revised by menu-c.md: the hub widget `ui_hub` replaces the chooser; `UiScreenId::ServoProg` and `Tools` are gone; a chosen tool's origin is its hub, not MENU. |
| 32, RX SIGNALS after SERVO PROGRAMMER | owner | Revised by menu-c.md: the row follows SERVO and reads RECEIVER. |
| "Not changed": the tester is the root | reason of the note | Revised by the owner's decision of 2026-11-15 and menu-c.md: MENU is the root; DShot runs only while ESC TESTER or ESC COMMANDS is open. |

Not revised: owner decisions 3 (one leave page), 24 to 31, 34 and 35, and
decisions 6 to 12, 14, 15, 17 and 20 to 22.

Decisions of other notes, each the owner's:

| Decision | The line the build adds |
|---|---|
| [menu.md](menu.md#decisions) 1, MENU behind the tester's button | Revised by menu-c.md: MENU is the root; the tester is MENU's first row and its MENU button returns there. |
| [esc-commands.md](esc-commands.md#decisions) 1, ESC COMMANDS as MENU's first row | Revised by menu-c.md: ESC COMMANDS is the first entry of the hub ESC CONFIG. |
| [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions) 1 and [usb-bridge.md](usb-bridge.md#decisions) 1, ESC CONFIG with ESC SETTINGS and USB BRIDGE | Revised by menu-c.md: ESC CONFIG also holds ESC COMMANDS, as its first entry. |
| [angle-meter.md](angle-meter.md#decisions) 1, the MENU row TOOLS | Revised by menu-c.md: TOOLS is gone; ANGLE METER is the second entry of the hub SERVO. |
| [rx-signals.md](rx-signals.md#decisions) 4, one row RX SIGNALS | Revised by menu-c.md: the row and its hub read RECEIVER. |

## What stays, and why

| What | Why |
|---|---|
| The pin claim, its states, its release order and the core1 jobs | the structure moves entry points; a claim still starts on the tap that opens a claiming tool. Their changes for DShot's lifetime are the other design's |
| `uiPinsRefuse()` as the one gate | MENU and the hubs draw what it says |
| The leave page, `ui_leave`, its three phases, STAY, LEAVE, NEXT SERVO, BACK TO SERVO, its BACK drawn disabled and the 1000 ms wait of its last button | [menu-b.md](menu-b.md#the-leave-page), owner decisions 3, 25 and 30. Only where its last button returns changes |
| Every hold: HOLD TO ARM, HOLD TO WRITE, HOLD TO SAVE, HOLD TO START, HOLD TO APPLY, HOLD TO REBOOT, HOLD TO KEEP, and `UI_HOLD_MS`, 1000 ms | the holds are the deliberate actions the interlocks count |
| A tap opens the write page and only the write page holds | menu-b.md decision 6 |
| The BACK guard over unwritten edits | menu-b.md decision 7 |
| HITEC CLASSIC's gate on the switched supply and its two amber lines; the power lines; Hitec D series on OWN waiting for READ | menu-b.md owner decisions 2, 31 and 34 |
| OUTPUT's warning lines and S.BUS OUTPUT's probe before START | [sbus-output.md](sbus-output.md) |
| The tester's readouts open SETTINGS only while disarmed, and SETTINGS returns to the tester | menu-b.md owner decisions 4 and 24, decision 17 |
| The tester's band layout and button row | [menu.md](menu.md#decisions) decision 5 keeps them for a later change |
| The 36 px header, BACK 64 x 32 px, the 40 px row floor, the 42 px MENU pitch | menu.md decisions 5 and 15 |
| The hub entry's geometry and texts | they are PROGRAMMER's, checked at build time today |
| SETTINGS as one list in sections, its footer and its statuses | menu.md decision 13 |
| The stored settings block, `SETTINGS_VERSION` | no setting changes |
| The colour roles | menu-b.md decision 11; `ui-theme.md` |

The interlocks of [menu-b.md](menu-b.md#interlocks) count deliberate actions
against 9f965a3, from the tester. From power-up:

| Path | At 9f965a3 | At cff09ae | Proposed | Holds because |
|---|---|---|---|---|
| Arm the ESC | hold | hold | 1 tap, then the hold | one action more |
| KST claim and search (with a switch: the supply on) | 2 taps | 3 taps | 2 taps: SERVO, KST / CHASERVO | not fewer than at 9f965a3; the claim starts on the entry tap |
| Hitec entry | 3 taps | 3 taps | 2 taps: SERVO, the entry | one tap fewer than at 9f965a3. HITEC SERVO opens on the family the entry names, as today; the family view that was the third tap is gone since structure B. CLASSIC keeps its gate, and a D series servo on OWN still needs READ |
| ESC SETTINGS claim | 2 taps | 3 taps | 2 taps | not fewer than at 9f965a3 |
| Servo pulses | 2 taps, then HOLD TO START | 2 taps, then the hold | 2 taps, then the hold | the same |
| S.BUS OUTPUT, USB BRIDGE claim | not built | 3 taps | 2 taps | one tap fewer than today. S.BUS OUTPUT still sends only after its probe and START; the bridge sends only what the PC sends |
| Every write, restore, save, apply, reboot | | | | unchanged: the tool's own taps and holds |

Two paths are one tap shorter than their first build: the Hitec entry, and
the claims of S.BUS OUTPUT and USB BRIDGE. Each is a claim, not an output:
no pulse, no write and no supply switch follows without the tool's own
action, except the switched supply that KST and Hitec turn on with their
claim, which the Hitec row above states.

## Slices

Each slice is host-tested and mutation-checked, and updates its own README,
wiki pages in English and German, previews, CHANGELOG and STATUS lines, by
[menu-b.md](menu-b.md#decisions) owner decision 27. Each adds the names it
removes to rule 9 of `test/check_docs.py`.

The DShot lifetime's slices come from the separate design. They need slice
3 of this list, MENU as the root, and can follow it at once; slices 4 to 6
do not depend on them. Until they merge, DShot runs on MENU as it does
today, so no slice here leaves `main` less safe than cff09ae.

| Slice | What | Host tests |
|---|---|---|
| 1 | `ui_hub`: the widget, the one entry type and `hubEntriesFit()`. The four choosers become four tables on it. No screen, text or tap changes. | The existing chooser cases of `test_menu.cpp`, `test_kst_ui.cpp`, `test_hitec_ui.cpp`, `test_sbus_ui.cpp`, `test_sbus_out_ui.cpp`, `test_bridge_ui.cpp` and `test_angle_ui.cpp` pass unchanged. New: the control table of a hub lists each entry at its rectangle; a disabled entry reports disabled and takes no tap. Mutation: break `enabled`, the HITEC CLASSIC case fails. |
| 2 | The navigation stack in `src/ui.cpp`: BACK pops one screen; a hub's tool returns to its hub; a hub keeps its scroll; the hubs lose `freePins` and gate each entry on `uiPinsRefuse()`. The tester stays the root. | New, by control id: for each of the ten hub entries, open it and leave it by BACK, and by the leave page's last button where it has one; `uiCurrentScreen()` is the opener. SETTINGS from a readout returns to the tester. A hub opened while the claim is not Free draws its claiming entries disabled. Mutation: restore the origin MENU, the return cases fail. |
| 3 | MENU as the root: the frame function starts on MENU; the header draws no BACK (`uiHeader()` gains the variant); the row ESC TESTER, first, with `freePins`; the tester opens disarmed and its MENU button returns to MENU; the EDT chip goes from the strip. Nine rows meanwhile, scrolling 9 × 42 − 258 = 120 px. | New: after `uiInit()` the screen is `Menu`; MENU's control table has no `UI_BACK`; a tap on row 0 opens `Tester`; `TesterCtl::Menu` disarms and returns; armed state, throttle and LATCH are clear on each open; the row is disabled while the claim is not Free. The suites that start on the tester (`test_ui.cpp` and the helpers `openMenu()`, `openSettings()`, `openServo()` and the others) open it through the row. Mutation: draw BACK at the root, the control-table case fails. |
| 4 | The six rows: SERVO holds SERVO TESTER, ANGLE METER and the three programmers, scrolling 146 px; ESC COMMANDS joins ESC CONFIG; RX SIGNALS reads RECEIVER; PROGRAMMER and TOOLS go; `>` on the hub rows. | New: `UI_MENU_COUNT` is 6 and `uiListMaxScroll()` of MENU is 0; every row is whole and acts at scroll 0; SERVO's range is 146 px; at scroll 0 entry 3 is listed and does not act; at 146 px HITEC CLASSIC's third line ends at y 316; each hub's entries open the screens of [the table](#every-screen-old-place-and-new-place). `menuRowsFit()` and `hubEntriesFit()` hold at build time. Mutation: a sixth entry line too long fails the build; a content of 400 px fails the y 316 case. |
| 5 | The revision lines of [the tables above](#decisions-of-menu-bmd-this-revises) in menu-b.md, menu.md, esc-commands.md, bluejay-blheli-s.md, usb-bridge.md, angle-meter.md and rx-signals.md; README "Layout" and "The user manual"; the wiki's The-Screens and Die-Bildschirme tree. | `python3 test/check_docs.py` with the removed names `SERVO PROGRAMMER`, `PROGRAMMER`, `RX SIGNALS` and `TOOLS` as MENU paths; `tools/check_wiki.py`. |

Slices 1 and 2 change no screen and can merge before the owner answers the
questions. Slices 3 and 4 depend on questions 1 to 5.

## Open questions

1. **ESC TESTER: its own row, or the first entry of a hub MOTOR & ESC?**
   The owner chose the row before asking for rcbench's structure, and this
   note keeps it. rcbench has one tile for the ESC. As a hub of four, ESC
   TESTER, ESC COMMANDS, ESC SETTINGS and USB BRIDGE, MENU has five rows
   and the two device classes look alike; the hub scrolls 4 × 80 − 258 =
   62 px, and the tester is 2 taps from power-up instead of 1.
   Recommendation: keep the row. The tester is the tool the bench is
   named for, and six rows fit without a scroll.
2. **A seventh row.** Each tool that takes a row of its own makes MENU
   scroll: seven rows scroll 36 px and SETTINGS shows 6 px at scroll 0.
   The candidates are SERVO TESTER and ESC COMMANDS, each 1 tap from MENU
   today and 2 in this proposal. Recommendation: none. From power-up both
   stay at 2 taps, and a MENU that never scrolls shows every row at once.
3. **ANGLE METER's place.** (a) The hub SERVO, as rcbench has RUDDER on its
   SERVO hub; SERVO then has five entries and scrolls. (b) TOOLS stays, a
   seventh row with one entry, 3 taps in all. (c) A row of its own, a
   seventh row. Three of its four uses, incidence, decalage and dihedral,
   measure the airframe and not a servo. Recommendation: (a). A row or a
   hub for one tool costs MENU its fit.
4. **rcbench's names.** (a) `RECEIVER` for RX SIGNALS. (b) `SETUP` for
   SETTINGS. (c) `LOGS` for SD LOG. (d) `ESC PROGRAM` for ESC CONFIG, after
   rcbench's task PROGRAM. Each rename changes the README, the wiki in two
   languages and the previews. Recommendation: (a) only. RX SIGNALS is the
   one row whose hub is renamed without touching another screen's text.
   `SETTINGS` is named in screen texts (`AND SETTINGS > SERVO SUPPLY
   GP28/29.`) and in [menu.md](menu.md#decisions) decision 12; SD LOG is not
   rcbench's LOGS; ESC CONFIG is the owner's name of 2026-11-15.
5. **Where a hub's tool returns.** (a) To its hub, by BACK and by the leave
   page's last button, as rcbench's home tag climbs one level. (b) To MENU,
   as today. With (a) a second servo of another brand, or ESC SETTINGS
   after ESC COMMANDS, is 1 tap away; with (b) MENU is 1 tap nearer after
   a session. Recommendation: (a).

## Risks

1. **Test rewrite.** Slice 3 changes where every UI suite starts. The suites
   reach tools through helpers and control ids, not coordinates, so the
   helpers change and most cases do not; the count is not measured.
2. **Habit.** ESC COMMANDS and SERVO TESTER are one tap further from MENU.
3. **The hub's scroll.** HITEC CLASSIC is not on the panel when SERVO
   opens. The 18 px of HITEC D SERIES and the scroll bar are the cue.
4. **Unverified on hardware.** Nothing here is tested on the board. The
   press floor of a cut entry and the drag on an 80 px list are host-tested
   only.
