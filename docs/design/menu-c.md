# Menu structure C

Status: proposed, not built. The owner decided its questions on 2026-11-15
([Decisions](#decisions)). [Open questions](#open-questions) holds what the
answers raise.

The owner's request of 2026-11-15: "Optimize the Menu structure and make it
similar to the Rcbench structure but adapted to the small display."

Decisions of the owner, 2026-11-15, that this note takes as fixed. They reach
this note through its task and are not yet in [STATUS.md](../../STATUS.md):

- **MENU is the start screen**, in place of the ESC tester.
- **No DShot at power-up.** DShot sends frames only while ESC TESTER or ESC
  COMMANDS is open; back on MENU the frames stop.
- **0.1.0 waits for this work.**
- **Six rows:** ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS, SETUP.

This note covers the structure: which screens exist, how they are grouped,
where each tool lives, the names, the depth, the navigation, and MENU as the
root. [The build plan](#build-plan) orders its slices and those of the look.
It does not cover three things:

- The DShot lifetime, the pin claim's changes and the core1 jobs.
  `docs/design/dshot-lifetime.md` covers them; it is written beside this
  note and is not on this branch. [What the structure requires of
  DShot](#what-the-structure-requires-of-dshot) is its input.
- Colours and the look of widgets. [ui-theme.md](ui-theme.md) plans them:
  [the widgets of this note](ui-theme.md#the-widgets-of-menu-cmd) and
  [the header](ui-theme.md#widgets). This note names a widget where the
  structure needs one.
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
classes under rcbench's names: the ESC's programming, SERVO, RECEIVER, LOGS
and SETUP. Three rows open a hub, the one chooser widget; every tool is at
most two taps from power-up.

```
SPLASH                      as today
 └─ MENU (root, no BACK)    six rows, no scroll                  depth 0
     ├─ ESC TESTER          no header; MENU button ──> MENU      1
     │   └─ 14 POLES or MAX 20% (tap, disarmed) ──> SETUP        2
     ├─ ESC PROGRAM         hub, 3 entries                       1
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
     ├─ LOGS                the screen SD LOG, renamed           1
     └─ SETUP               the screen SETTINGS, renamed:        1
                            one list in sections, as today
```

How it follows rcbench, and where it differs:

| rcbench | MiniRcBench | Why |
|---|---|---|
| The menu is the root, with no home tag | MENU is the root, with no BACK | the owner's decision; nothing lies behind a root |
| MOTOR & ESC, one tile, tasks LIVE and PROGRAM as tabs | two rows, ESC TESTER and ESC PROGRAM | owner decision 4: the tester keeps its row. The second row takes the name of rcbench's task PROGRAM. The tester has no header and no room for a tab strip, and a tab 40 px high does not fit over three hub entries (62 + 40 + 4 + 2 × 80 + 81 = 347 px) |
| SERVO, a hub of tiles in the order of use | SERVO, a hub of entries in the same order: drive, measure, program | the same |
| RUDDER on the SERVO hub | ANGLE METER on the SERVO hub | owner decision 5: the surface throw is measured where the servo is driven; TOOLS and its one entry go |
| RECEIVER, an instrument tile | RECEIVER, a hub of MONITOR and OUTPUT | the name fits both: OUTPUT stands in for a receiver |
| LOGS, a strip of two tabs that browses and plots files | LOGS, one screen | owner decision 6 takes the name. The screen starts and stops the recorder and shows the last run; it browses nothing |
| SETUP, a hub of categories | SETUP, one list in sections | owner decision 6 takes the name. 15 rows in 6 sections; [menu.md](menu.md#decisions) decision 13 keeps one list until about 20 rows ([SETUP stays one list](#setup-stays-one-list)) |
| SUPPLY, BATTERY, BALANCE | none | no such tool exists |
| STOP in the band on every screen | none | every output here stops on the screen that drives it; the touch-loss stop is pull request #49 |
| A new maker adds a registry row | a new brand, protocol or tool adds a hub entry | the same rule; MENU gains a row only for a new device class |

### MENU

```
y   0 | MENU                                      |  header 36 px; no BACK
   40 | SIG GP0  AUX GP1  DSHOT600                |  strip, C_DIM
   62 | ESC TESTER                                |  row 0: x 8..231, 40 px
  104 | ESC PROGRAM                             > |  row 1: hub
  146 | SERVO                                   > |  row 2: hub
  188 | RECEIVER                                > |  row 3: hub
  230 | LOGS                             NO CARD  |  row 4
  272 | SETUP                            UNSAVED  |  row 5: ends at y 311
  320 +-------------------------------------------+
```

Six rows take 6 × 42 = 252 px of the 258 px viewport, so
`uiListMaxScroll()` returns 0 and the `static_assert` "six MENU rows do not
fit the viewport" in `src/ui_menu.cpp` covers the layout. Row i starts at
y 62 + 42 × i. The last row ends at y 311; 8 px stay free under it.

| Row | Label, scale 2 from x 16 | Ends at x | Widest status, right to x 222 | Starts at x |
|---|---|---|---|---|
| ESC TESTER | 10 characters, 118 px | 134 | none | |
| ESC PROGRAM | 11, 130 px | 146 | `>`, scale 2, 10 px | 213 |
| SERVO | 5, 58 px | 74 | `>` | 213 |
| RECEIVER | 8, 94 px | 110 | `>` | 213 |
| LOGS | 4, 46 px | 62 | `CARD ERROR`, scale 1, 10 characters, 59 px | 163 |
| SETUP | 5, 58 px | 74 | `UNSAVED`, scale 1, 7, 41 px | 181 |

`menuRowsFit()` holds for each: the label's end plus 8 px
(`UI_MENU_LABEL_GAP`) is at most the status' start. The check reads a
status at scale 1 today (`textW(e.statusWidest, 1)`); for a hub row it
counts the mark's 10 px. The widest label, 130 px, is 60 px narrower than
`SERVO PROGRAMMER`, 190 px, the row that forced the tool rows to drop their
status ([menu-b.md](menu-b.md#decisions) decision 18).

`>` marks a row that opens a hub, as rcbench's row `THROTTLE OUTPUT >`
marks a session one level down.
[ui-theme.md](ui-theme.md#the-widgets-of-menu-cmd) draws it at scale 2 in
`C_CYAN`, and in `C_GRID` on a disabled row.

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
([choice 9](#decisions)).

### The hub

One widget, `ui_hub`, replaces the four choosers. A hub is a title, a strip
and a table of entries; its look is
[ui-theme.md's](ui-theme.md#the-widgets-of-menu-cmd). The geometry is
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
| ESC PROGRAM | 11 characters, 130 px, ends x 138 | `SIG GP0`, as today | 3 |
| SERVO | 5, 58 px, ends x 66 | `SIG GP0  AUX GP1  SERVO SUPPLY GP28`, 35 characters, 209 px, ends x 217 | 5 |
| RECEIVER | 8, 94 px, ends x 102 | `SIG GP0  AUX GP1`, as today | 2 |

Each title ends 4 px or more before BACK at x 170 (`UI_BACK_X`), the rule of
the choosers' `static_assert`s; a title holds 13 characters there.

| Hub | Entry | Lines under it | Characters, width |
|---|---|---|---|
| ESC PROGRAM | `ESC COMMANDS` | `BEEP 1 TO 5, DIRECTION, 3D MODE.` | 32, 191 px |
| | | `SENT AS DSHOT COMMANDS.` | 23, 137 px |
| | `ESC SETTINGS` | the two lines of today | 36 at most |
| | `USB BRIDGE` | the two lines of today | 28 at most |
| SERVO | `SERVO TESTER` | `ONE SERVO ON SIG, OR TWO WITH AUX.` | 34, 203 px |
| | `ANGLE METER` | `INCIDENCE, DECALAGE, DIHEDRAL, THROW`, as today | 36, 215 px |
| | `KST / CHASERVO` | the power line of today | 33 at most |
| | `HITEC D SERIES` | the models and the power line of today | 33 at most |
| | `HITEC CLASSIC` | the models, then the power line or the two amber lines, the second renamed to `AND SETUP > SERVO SUPPLY GP28/29.` | 36 at most |
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
  310 | AND SETUP > SERVO SUPPLY GP28/29.         |  ends at y 316
  320 +-------------------------------------------+
```

The content is 5 × 80 + 4 = 404 px in the 258 px viewport, so the scroll
range is 404 − 258 = 146 px. Entry i starts at y 62 + 80 × i − scroll. At
scroll 0 three entries are whole and HITEC D SERIES shows 18 px, less than
the 40 px press floor: it is listed and takes no press, and it shows that
the list goes on. HITEC D SERIES is whole from 28 px of scroll (its button
ends at y 320) and HITEC CLASSIC from 108 px.

ESC PROGRAM and RECEIVER do not scroll: 3 and 2 entries.

### Navigation

BACK climbs one level, and a screen returns to the one that opened it
(owner decision 7).

| View | BACK, or the way back |
|---|---|
| MENU | none: the header draws no BACK |
| ESC TESTER | no header; the `MENU` button disarms, zeroes the throttle, turns LATCH off and returns to MENU, as today |
| SETUP | where it was opened from: MENU, or ESC TESTER after a readout tap |
| LOGS | MENU |
| ESC PROGRAM, SERVO, RECEIVER | MENU |
| ESC COMMANDS, ANGLE METER | their hub |
| A claiming tool, claim refused or not yet held | its hub; today MENU |
| A claiming tool's main view | its leave page, as today |
| The leave page's last button, today RESTART DSHOT | the hub; today MENU |
| A sub-view: SET, HEX, WRITE, FILES, SETUP, FLIP CAL | its parent, as today |

The navigation is a stack of at most three screens: MENU; a hub, ESC TESTER,
LOGS or SETUP; a tool, or SETUP over ESC TESTER. BACK pops one. It
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
| LOGS | 2, after a drag of 34 px | 1 |
| SETUP | 2, after a drag of 76 px; 1 from a readout | 1; 2 through a readout |

Taps from MENU: ESC COMMANDS and SERVO TESTER go from 1 to 2. Every other
tool keeps its count, and LOGS and SETUP lose their drag.

### Every screen, old place and new place

| Screen (`UiScreenId`) | Title | Old place | New place |
|---|---|---|---|
| `Tester` | none | the root | MENU row 1, ESC TESTER |
| `Menu` | `MENU` | behind the tester's MENU button | the root |
| `EscCmd` | `ESC COMMANDS` | MENU row 1 | ESC PROGRAM, entry 1 |
| `EscConfig` | `ESC CONFIG`, becomes `ESC PROGRAM` | MENU row 2, a chooser of two | MENU row 2, a hub of three |
| `Am32` | `ESC SETTINGS`, `ESC HEX`, `ESC WRITE` | ESC CONFIG, entry 1 | ESC PROGRAM, entry 2 |
| `Bridge` | `USB BRIDGE` | ESC CONFIG, entry 2 | ESC PROGRAM, entry 3 |
| `Servo` | `SERVO TESTER`, `SERVO SET` | MENU row 3 | SERVO, entry 1 |
| `Angle` | `ANGLE METER`, `ANGLE SETUP`, `FLIP CAL` | TOOLS, entry 1 | SERVO, entry 2 |
| `ServoProg` | `PROGRAMMER` | MENU row 4 | gone: its three entries are SERVO's entries 3 to 5 |
| `Kst` | `KST SERVO` and its sub-views | PROGRAMMER, entry 1 | SERVO, entry 3 |
| `Hitec` | `HITEC SERVO` and its sub-views | PROGRAMMER, entries 2 and 3 | SERVO, entries 4 and 5 |
| `RxSignals` | `RX SIGNALS`, becomes `RECEIVER` | MENU row 5 | MENU row 4 |
| `Sbus` | `S.BUS MONITOR` | RX SIGNALS, entry 1 | RECEIVER, entry 1 |
| `SbusOut` | `S.BUS OUTPUT`, `OUTPUT SET` | RX SIGNALS, entry 2 | RECEIVER, entry 2 |
| `Tools` | `TOOLS` | MENU row 6 | gone |
| `SdLog` | `SD LOG`, becomes `LOGS` | MENU row 7 | MENU row 5 |
| `Settings` | `SETTINGS`, becomes `SETUP` | MENU row 8; the tester's readouts | MENU row 6; the tester's readouts |

No tool's own views change. The leave pages, the write pages, FILES, SET,
HEX, ANGLE SETUP and FLIP CAL stay as built; ANGLE SETUP's name is
[open question 1](#open-questions). The identifiers keep their words:
`UiScreenId::Settings`, `SdLog`, `RxSignals` and `EscConfig`
([Names](#names)).

## Names

Owner decision 6 takes all four of rcbench's names.

| At cff09ae | Becomes | rcbench's | Carried by |
|---|---|---|---|
| `ESC CONFIG` | `ESC PROGRAM` | PROGRAM, the task of MOTOR & ESC | MENU row 2; the hub's title |
| `RX SIGNALS` | `RECEIVER` | the instrument RECEIVER | MENU row 4; the hub's title |
| `SD LOG` | `LOGS` | the tile LOGS | MENU row 5; the screen's title |
| `SETTINGS` | `SETUP` | the tile SETUP | MENU row 6; the screen's title; the paths in six screen texts |

### Fit

A title names its screen ([menu-b.md](menu-b.md#decisions) decision 13), so
a MENU label and the title of the screen it opens are one text. Widths by
`textW()`.

| Text | As | Characters, width | Ends at x | Limit |
|---|---|---|---|---|
| `ESC PROGRAM` | MENU label, scale 2 from x 16 | 11, 130 px | 146 | 205: the mark `>` starts at x 213, less `UI_MENU_LABEL_GAP` |
| | hub title, scale 2 from x 8 | | 138 | 166: BACK at x 170 (`UI_BACK_X`), less 4 px |
| `RECEIVER` | MENU label | 8, 94 px | 110 | 205 |
| | hub title | | 102 | 166 |
| `LOGS` | MENU label | 4, 46 px | 62 | 155: `CARD ERROR` starts at x 163 |
| | header title | | 54 | 166 |
| `SETUP` | MENU label | 5, 58 px | 74 | 173: `UNSAVED` starts at x 181 |
| | header title | | 66 | 166 |

Each new name is narrower than the limit that holds today's, so
`menuRowsFit()` and the title `static_assert`s of `src/ui_esc_config.cpp`
and `src/ui_rx_signals.cpp` (lines 17) hold. `ESC PROGRAM` is the one name
longer than the word it replaces, by one character.

Screen texts that name the screen SETTINGS as a place. Each loses three
characters, so each fits where it fits today:

| Text at cff09ae | Becomes | Characters | Where |
|---|---|---|---|
| `CHECK MENU > SETTINGS` | `CHECK MENU > SETUP` | 18 | the splash, `src/ui.cpp` line 1306 |
| `CHECK MENU > SETTINGS, THEN SAVE` | `CHECK MENU > SETUP, THEN SAVE` | 29 | the splash, `UI_SPLASH_CARRIED_TEXT2` in `src/ui.h` |
| `AND SETTINGS > SERVO SUPPLY GP28/29.` | `AND SETUP > SERVO SUPPLY GP28/29.` | 33 | HITEC CLASSIC's second amber line, `src/ui_registry.cpp` line 56 |
| `A SERVO ON ITS OWN SUPPLY NEEDS SETTINGS > SERVO SUPPLY AT OWN.` | `A SERVO ON ITS OWN SUPPLY NEEDS SETUP > SERVO SUPPLY AT OWN.` | 60, wrapped to two lines | KST SERVO, `src/ui_kst.cpp` line 691 |
| `SET A PIN TO GP1 IN SETTINGS.` | `SET A PIN TO GP1 IN SETUP.` | 26 | S.BUS MONITOR, `SBUS_UI_NO_GP1_2` |
| `SET A PIN TO GP0 IN SETTINGS.` | `SET A PIN TO GP0 IN SETUP.` | 26 | S.BUS OUTPUT, `SBOUT_UI_NO_GP0_2` |
| `SET SIG PIN OR AUX PIN IN SETTINGS.` | `SET SIG PIN OR AUX PIN IN SETUP.` | 32 | SERVO TESTER, `SERVO_UI_SAME_PIN_2` |

No strip text names one of the four words: the strips name pins, rates and
states.

### What keeps its word

- **`ESC SETTINGS`,** the tool for AM32, Bluejay and BLHeli_S, its title
  and its hub entry, and the texts that name it: `RECORDING STOPPED BY ESC
  SETTINGS` and the run summary's `ESC SETTINGS`. It is not one of the
  four names. With SETUP for the bench's own list, SETTINGS names one
  screen only.
- **"Settings" as the thing stored,** not the place: `SETTINGS RESET BY
  UPDATE`, `SETTINGS CARRIED OVER FROM AN UPDATE`, `SAVED SETTINGS IN USE`,
  `AM32 SETTINGS, READ AND WRITTEN.`, `NO VALID SETTINGS`, `NOT AN AM32
  ESC, OR BLANK SETTINGS.`, `SETTINGS PAGE EMPTY` and `SETTINGS MARKER
  WRONG: READ-ONLY.`
- **The tester's tags** `SD`, `NOSD`, `SDERR` and `REC`, and the card's
  texts `NO CARD` and `CARD ERROR`: they name the card and the recorder,
  not the screen.
- **Identifiers and file names:** `UiScreenId::Settings`, `SdLog`,
  `RxSignals` and `EscConfig`, `UI_SETTINGS`, `SettingsCtl`, the files
  `src/ui_settings.cpp`, `src/ui_sdlog.cpp`, `src/ui_rx_signals.cpp` and
  `src/ui_esc_config.cpp`, `SETTINGS_VERSION`, and the previews
  `docs/settings-preview.png` and `docs/log-preview.png`. No user reads
  them, and 61 files of `src/` name `SETTINGS`.
- **SETUP's sections** ESC TESTER, WIRING, SERVO SUPPLY, DISPLAY, BOARD and
  DIAGNOSTICS, and its rows.
- **CHANGELOG.md and the design notes.** They are records;
  `test/check_docs.py` rule 9 does not check them. The build adds the
  revision lines of [the tables below](#decisions-of-menu-bmd-this-revises)
  and no other edit.

### The rename slice's work

Slice 5 of [the build plan](#build-plan). The counts are lines that name
the word at cff09ae, comments included; `SETTINGS` is counted without `ESC
SETTINGS` and `AM32 SETTINGS`. A line that names the stored settings stays,
so fewer lines change than are counted.

| Where | What changes | `ESC CONFIG` | `RX SIGNALS` | `SD LOG` | `SETTINGS` |
|---|---|---|---|---|---|
| `src/` | the four labels of `UI_MENU`, `UI_ESC_CFG_TITLE`, `UI_RX_TITLE`, the two `uiHeader()` titles and the two `UiScreen` titles of LOGS and SETUP, the seven texts above, the messages of the `static_assert`s, the comments | 24 lines in 10 files | 24 in 10 | 32 in 15 | 241 in 61, `ESC SETTINGS` included |
| `test/` | the expectations that find a row, a title or a text by its words, and the helpers' comments. 18 lines pass one of the four words as a string | 24 | 27 | 68 | 191 |
| `test/check_docs.py` | rule 9 gains the four old words as MENU paths and titles: `ESC CONFIG`, `RX SIGNALS`, `SD LOG`, and `SETTINGS` after `MENU >`, `MENU →` or before `>` and `→`; the bare word stays legal for `ESC SETTINGS` | | | | |
| README.md | "Using it", "Layout", every tool's section, "The user manual" | 12 | 10 | 18 | 80 |
| `wiki/`, English and German | every page but Setup, Einrichtung, USB-Bridge and USB-Bruecke for `SETTINGS`; The-Screens and Die-Bildschirme hold the tree | 32 on 7 pages | 26 on 4 | 65 on 11 | 213 on 25 |
| `tools/make_wiring.py` | four texts of the drawing: `MENU > SETTINGS; these are the`, `optional, SETTINGS > KISS TELEM`, `(SETTINGS > SERVO SUPPLY GP%d)`, `(SETTINGS > CURRENT SENSE)`. `docs/wiring.svg` and the wiki's copy are drawn again | | | | 4 |
| `.github/workflows/release.yml` | the release text, lines 254, 261 and 273 | | | | 3 |
| `tools/make_previews.py`, `test/ui_ids.h`, `test/rx.mk` | comments | 2 | 2 | | 2 |
| STATUS.md, `docs/bench-plan.md` | the lines that describe the bench as it is; the record of what was built keeps its words | 13, 3 | 21, 3 | 21, 11 | 54, 18 |
| The previews | the 17 of `docs/*-preview.png` and the wiki's images, by `tools/make_previews.py --wiki` | | | | |

Wiki page titles. A page is renamed only where it is named after a screen:

| Page | Named after | Renamed |
|---|---|---|
| RX-Signals, RX-Signale | the screen RX SIGNALS | yes: `Receiver` and `Empfaenger`, titled "Receiver" and "Empfänger"; `PAIRS` in `tools/check_wiki.py`, `_Sidebar.md`, Home and every link follow |
| SD-Logging, SD-Aufzeichnung | the function, recording to the card | no |
| AM32-Configuration, AM32-Konfiguration | the ESC firmware and the task | no |
| Setup, Einrichtung | the first steps with the hardware, "from an empty bench to the splash screen" | no; see [open question 2](#open-questions) |
| The-Screens, Die-Bildschirme and the others | no screen | no |

The German pages keep the screen's words as the screen shows them: the
firmware has one language.

### SETUP stays one list

rcbench's SETUP is a hub of tiles: PANEL, SENSORS, SD CARD and WIRELESS
open category lists, and OUTPUTS, WI-FI, UPDATES and IMU open screens.
MiniRcBench's SETUP stays one list in sections, with the name only taken.

- It has 15 rows in 6 sections (`ROWS_TESTER` to `ROWS_DIAG` in
  `src/ui_settings.cpp`): 2, 4, 2, 2, 3 and 2 rows.
- [menu.md](menu.md#decisions) decision 13, the owner's, keeps one list
  until about 20 rows. Five rows are left.
- As a hub of six sections SETUP would scroll 6 × 80 + 4 − 258 = 226 px,
  and every setting would be one tap further away. The tester's readouts
  open SETUP on its first section, ESC TESTER, whose two rows are the ones
  tapped, MOTOR POLES and MAX THROTTLE; a hub would put them behind a
  second tap.
- The form to take past about 20 rows is the hub on `ui_hub`, one entry a
  section. No slice of this note builds it.

This is not put to the owner as a question: decision 13 answers it. The
same name on the two benches then stands for a list here and for tiles
there.

## What the structure requires of DShot

Input for `docs/design/dshot-lifetime.md`, which decides the DShot lifetime
and the pin claim. This note decides none of it.

1. **Screens that need DShot running:** ESC TESTER and ESC COMMANDS. No
   other screen sends a frame or reads the ESC's telemetry for its own work.
2. **MENU and the three hubs run without DShot.** They are open with the
   claim Free and nothing on SIG. What the claim's states mean then, and
   whether "not Free" still exists on MENU, is that design's.
3. **One gate.** MENU and the hubs refuse a row or an entry through
   `uiPinsRefuse()` only. ESC TESTER and ESC COMMANDS have `freePins` set.
4. **ESC TESTER's readouts open SETUP.** By the owner's rule DShot stops
   there and starts again on BACK. SETUP can change SIG's pin and the DShot
   rate meanwhile (`applyLive()`).
5. **SETUP's LINK line,** `LINK  998/S  0% ERR`, reads the running DShot's
   telemetry (`src/ui_settings.cpp`, `LINK_Y`). Without DShot it has no
   source.
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
   button returns to the hub (owner decision 7).
8. **The splash** waits "while the ESC finishes booting" (`src/main.cpp`).
   With no DShot at power-up the reason changes.
9. **The first frame.** DShot starts on the tap on MENU's row ESC TESTER,
   or on the entry ESC COMMANDS of the hub ESC PROGRAM. The time from that
   tap until the ESC answers is not measured.
10. **The log.** The recorder follows the tester's arm state
    (`sdLogSetArmed()` in `src/ui.cpp`). LOGS, the screen, needs no DShot.
11. **Leaving ESC TESTER disarms first,** by the `MENU` button and by a
    readout tap, as `uiOpenMenu()` and `uiOpenTool()` do today.
12. **ESC COMMANDS shares a hub with two claiming tools.** BACK from ESC
    COMMANDS returns to ESC PROGRAM, where ESC SETTINGS and USB BRIDGE are
    one tap away and claim SIG. DShot's stop is complete, or the two
    entries are drawn disabled through `uiPinsRefuse()`, before a tap on
    them acts.
13. **A claiming tool returns to its hub.** After the leave page's last
    button the hub is open with nothing on SIG. From ESC SETTINGS or USB
    BRIDGE the next tap can be ESC COMMANDS, which starts DShot on the pin
    just released.
14. **New texts use the names of [Names](#names):** `SETUP`, `LOGS`,
    `RECEIVER`, `ESC PROGRAM`.
15. **The look asks nothing of DShot.** [ui-theme.md](ui-theme.md) changes
    HOLD TO ARM's fill and the tester's badge, and no state behind them.

## Decisions

Decisions of the owner, 2026-11-15, given with the task of this note:

1. **MENU is the start screen,** in place of the ESC tester.
2. **No DShot at power-up.** DShot sends frames only while ESC TESTER or
   ESC COMMANDS is open. `docs/design/dshot-lifetime.md` covers the
   lifetime and the pin claim.
3. **0.1.0 waits for this work.**

Decisions of the owner, 2026-11-15, answering the open questions of this
note as it stood at 71a060f:

4. **ESC TESTER's row** (questions 1 and 2). ESC TESTER keeps its own MENU
   row, the first. MENU has six rows and does not scroll. No tool takes a
   seventh row: ESC COMMANDS and SERVO TESTER live in their hubs. This is
   the recommendation.
5. **ANGLE METER's place** (question 3). Option (a): the second entry of
   the hub SERVO. TOOLS goes. This is the recommendation.
6. **rcbench's names** (question 4). All four: RX SIGNALS becomes RECEIVER,
   SETTINGS becomes SETUP, SD LOG becomes LOGS and ESC CONFIG becomes ESC
   PROGRAM. This is not the recommendation, which took RECEIVER only.
   [Names](#names) lists what changes.
7. **Where a hub's tool returns** (question 5). Option (a): to its hub, by
   BACK and by the leave page's last button, as rcbench's home tag climbs
   one level. This is the recommendation.

Choices of this note where the decisions above leave the choice. The owner
has not decided them:

8. **Three hubs on one widget.** ESC PROGRAM holds ESC COMMANDS, ESC
   SETTINGS and USB BRIDGE; SERVO holds SERVO TESTER, ANGLE METER and the
   three programmer entries; RECEIVER holds MONITOR and OUTPUT. PROGRAMMER
   and TOOLS go.
9. **The gate is at the entry.** A hub opens whatever the claim's state and
   draws the entries `uiPinsRefuse()` refuses disabled. The hubs lose
   `freePins`.
10. **The root has no BACK.** The header draws none, as rcbench's menu has
    no home tag. A BACK drawn disabled for good would be a control that
    never acts.
11. **The tester keeps its `MENU` button** and gains no header.
12. **MENU's strip loses the EDT chip.** The caption stays.
13. **`>` marks a hub row.**
14. **SETUP stays one list** ([SETUP stays one list](#setup-stays-one-list)).
15. **The four names change texts, not identifiers**
    ([What keeps its word](#what-keeps-its-word)).
16. **One wiki page pair is renamed,** RX-Signals and RX-Signale
    ([The rename slice's work](#the-rename-slices-work)).

## Decisions of menu-b.md this revises

[menu-b.md](menu-b.md) is not edited now. The build adds each line below to
the decision it names, when the slice that changes it merges.

| Decision of menu-b.md | Whose | The line the build adds |
|---|---|---|
| 1, six rows that never scroll; 33, seven rows scroll 36 px; the revision of 2026-11-15 under [MENU](menu-b.md#menu), eight rows scroll 78 px | owner | Revised by menu-c.md decision 4: MENU is the start screen and has six rows, ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS, SETUP; it does not scroll. |
| 2, SERVO PROGRAMMER and its chooser | owner | Revised by menu-c.md: no MENU row reads SERVO PROGRAMMER and no screen is titled PROGRAMMER. KST / CHASERVO, HITEC D SERIES and HITEC CLASSIC are entries 3 to 5 of the hub SERVO, with the lines and the gate of this decision; CLASSIC's second line reads `AND SETUP > SERVO SUPPLY GP28/29.` A future brand adds a hub entry. |
| 4, SETTINGS from the tester | owner | Revised by menu-c.md decision 6: the screen reads SETUP. |
| 5, BACK goes one level up, or is drawn disabled | this note's rule; the owner confirmed the leave page's BACK (25) | Revised by menu-c.md decisions 7 and 10: MENU, the root, draws no BACK. A tool opened from a hub returns to the hub, where this decision's table says MENU. |
| 13, a title names its screen | rule | Revised by menu-c.md decision 6: the hubs are titled ESC PROGRAM, SERVO and RECEIVER; SD LOG's title reads LOGS and SETTINGS' reads SETUP; PROGRAMMER and TOOLS are gone. |
| 16 and 23, MENU order by device | 23 owner | Revised by menu-c.md decision 4: the order by device stays, one row per device class after ESC TESTER: ESC TESTER, ESC PROGRAM, SERVO, RECEIVER, LOGS, SETUP. |
| 17, SETTINGS returns to where it was opened | choice | Revised by menu-c.md decision 6: the screen reads SETUP. |
| 18, MENU statuses report state | choice | Revised by menu-c.md: a row that opens a hub carries `>`; the rows SD LOG and SETTINGS read LOGS and SETUP. |
| 19, the chooser opens a tool | choice | Revised by menu-c.md decision 7: the hub widget `ui_hub` replaces the chooser; `UiScreenId::ServoProg` and `Tools` are gone; a chosen tool's origin is its hub, not MENU. |
| 32, RX SIGNALS after SERVO PROGRAMMER; 35, RX SIGNALS shown | owner | Revised by menu-c.md decision 6: the row follows SERVO and reads RECEIVER. |
| "Not changed": the tester is the root | reason of the note | Revised by the owner's decisions of 2026-11-15, menu-c.md decisions 1 and 2: MENU is the root; DShot runs only while ESC TESTER or ESC COMMANDS is open. |

Not revised: owner decisions 3 (one leave page), 24 to 31 and 34, and
decisions 6 to 12, 14, 15 and 20 to 22. [ui-theme.md](ui-theme.md#revisions)
revises decisions 8, 11 and 14 for the look.

Decisions of other notes, each the owner's:

| Decision | The line the build adds |
|---|---|
| [menu.md](menu.md#decisions) 1, MENU behind the tester's button | Revised by menu-c.md decision 1: MENU is the root; the tester is MENU's first row and its MENU button returns there. |
| [menu.md](menu.md#decisions) 12, the names MENU and SETTINGS | Revised by menu-c.md decision 6: SETTINGS reads SETUP. |
| [menu.md](menu.md#decisions) 13, one SETTINGS list | Revised by menu-c.md decision 6 for the name only: SETUP stays one list in sections. |
| [esc-commands.md](esc-commands.md#decisions) 1, ESC COMMANDS as MENU's first row | Revised by menu-c.md decision 4: ESC COMMANDS is the first entry of the hub ESC PROGRAM. |
| [bluejay-blheli-s.md](bluejay-blheli-s.md#decisions) 1 and [usb-bridge.md](usb-bridge.md#decisions) 1, the chooser ESC CONFIG with ESC SETTINGS and USB BRIDGE | Revised by menu-c.md decisions 4 and 6: the hub reads ESC PROGRAM and also holds ESC COMMANDS, as its first entry. |
| [angle-meter.md](angle-meter.md#decisions) 1, the MENU row TOOLS | Revised by menu-c.md decision 5: TOOLS is gone; ANGLE METER is the second entry of the hub SERVO. |
| [rx-signals.md](rx-signals.md#decisions) 4, one row RX SIGNALS | Revised by menu-c.md decision 6: the row and its hub read RECEIVER. |

A decision of another note that names SD LOG or SETTINGS in passing gains
no line. `docs/design/README.md` gains the table of [Names](#names) once,
for a reader of the older notes.

## What stays, and why

| What | Why |
|---|---|
| The pin claim, its states, its release order and the core1 jobs | the structure moves entry points; a claim still starts on the tap that opens a claiming tool. Their changes for DShot's lifetime are those of `docs/design/dshot-lifetime.md` |
| `uiPinsRefuse()` as the one gate | MENU and the hubs draw what it says |
| The leave page, `ui_leave`, its three phases, STAY, LEAVE, NEXT SERVO, BACK TO SERVO, its BACK drawn disabled and the 1000 ms wait of its last button | [menu-b.md](menu-b.md#the-leave-page), owner decisions 3, 25 and 30. Only where its last button returns changes |
| Every hold: HOLD TO ARM, HOLD TO WRITE, HOLD TO SAVE, HOLD TO START, HOLD TO APPLY, HOLD TO REBOOT, HOLD TO KEEP, and `UI_HOLD_MS`, 1000 ms | the holds are the deliberate actions the interlocks count. [ui-theme.md](ui-theme.md#decisions) changes their fill, not their time |
| A tap opens the write page and only the write page holds | menu-b.md decision 6 |
| The BACK guard over unwritten edits | menu-b.md decision 7 |
| HITEC CLASSIC's gate on the switched supply and its two amber lines; the power lines; Hitec D series on OWN waiting for READ | menu-b.md owner decisions 2, 31 and 34 |
| OUTPUT's warning lines and S.BUS OUTPUT's probe before START | [sbus-output.md](sbus-output.md) |
| The tester's readouts open SETUP only while disarmed, and SETUP returns to the tester | menu-b.md owner decisions 4 and 24, decision 17 |
| The tester's band layout and button row | [menu.md](menu.md#decisions) decision 5 keeps them for a later change |
| The 36 px header, BACK 64 x 32 px, the 40 px row floor, the 42 px MENU pitch | menu.md decisions 5 and 15 |
| The hub entry's geometry and texts | they are PROGRAMMER's, checked at build time today |
| SETUP as one list in sections, its footer and its statuses | menu.md decision 13 |
| The stored settings block, `SETTINGS_VERSION` | no setting changes |
| The stored names: `UiScreenId`, the control ids, the file names | [What keeps its word](#what-keeps-its-word) |

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

## Build plan

One ordered list for this note and [ui-theme.md](ui-theme.md), which links
here. Slices 1 to 6 build the structure, 7 to 14 the look. The DShot
lifetime's slices are those of `docs/design/dshot-lifetime.md`; that note
numbers them, and this plan calls them the DShot slices.

Each slice is host-tested and mutation-checked, and updates its own README,
wiki pages in English and German, previews, CHANGELOG and STATUS lines, by
[menu-b.md](menu-b.md#decisions) owner decision 27. A slice that removes a
name adds it to rule 9 of `test/check_docs.py`. No slice needs hardware:
by [ui-theme.md](ui-theme.md#decisions) decision 5 the look is built on the
host, and every colour stays marked "not seen on the panel".

The model a slice suits: Opus for safety and core logic; Fable for look,
widgets and screen layout; Sonnet for mechanical renames, the docs and wiki
pairs, and the previews.

| Slice | What | Model | Host tests | The DShot slices |
|---|---|---|---|---|
| 1. `ui_hub` | The widget, the one entry type and `hubEntriesFit()`. The four choosers become four tables on it. No screen, text or tap changes. | Fable | The existing chooser cases of `test_menu.cpp`, `test_kst_ui.cpp`, `test_hitec_ui.cpp`, `test_sbus_ui.cpp`, `test_sbus_out_ui.cpp`, `test_bridge_ui.cpp` and `test_angle_ui.cpp` pass unchanged. New: the control table of a hub lists each entry at its rectangle; a disabled entry reports disabled and takes no tap. Mutation: break `enabled`, the HITEC CLASSIC case fails. | independent |
| 2. Navigation stack | In `src/ui.cpp`: BACK pops one screen; a chooser's tool returns to its chooser, by BACK and by the leave page's last button (owner decision 7); a chooser keeps its scroll; the choosers lose `freePins` and gate each entry on `uiPinsRefuse()`. The tester stays the root. | Opus | New, by control id: for each of the eight chooser entries, open it and leave it by BACK, and by the leave page's last button where it has one; `uiCurrentScreen()` is the opener. SETTINGS from a readout returns to the tester. A chooser opened while the claim is not Free draws its claiming entries disabled. Mutation: restore the origin MENU, the return cases fail. | independent. The DShot slices change what the leave page's last button does to the pin and what it reads; this slice changes where it returns. Whichever merges second keeps both |
| 3. MENU as the root | The frame function starts on MENU; the header draws no BACK (`uiHeader()` gains the variant); the row ESC TESTER, first, with `freePins`; the tester opens disarmed and its MENU button returns to MENU; the EDT chip goes from the strip. Nine rows meanwhile, scrolling 9 × 42 − 258 = 120 px. | Opus | New: after `uiInit()` the screen is `Menu`; MENU's control table has no `UI_BACK`; a tap on row 0 opens `Tester`; `TesterCtl::Menu` disarms and returns; armed state, throttle and LATCH are clear on each open; the row is disabled while the claim is not Free. The suites that start on the tester (`test_ui.cpp` and the helpers `openMenu()`, `openSettings()`, `openServo()` and the others) open it through the row. Mutation: draw BACK at the root, the control-table case fails. | **they need this slice** and follow it at once. Until they merge, DShot runs on MENU as at cff09ae, so `main` is no less safe than cff09ae |
| 4. Six rows, three hubs | SERVO holds SERVO TESTER, ANGLE METER and the three programmers, scrolling 146 px; ESC COMMANDS joins ESC CONFIG; PROGRAMMER and TOOLS go; `>` on the hub rows. The names are still those of cff09ae: ESC CONFIG, RX SIGNALS, SD LOG, SETTINGS. `buildItems()` of `src/ui_settings.cpp`, which walks `uiMenuEntries()` for a tool's section, walks the hubs' entries too. | Fable | New: `UI_MENU_COUNT` is 6 and `uiListMaxScroll()` of MENU is 0; every row is whole and acts at scroll 0; SERVO's range is 146 px; at scroll 0 entry 3 is listed and does not act; at 146 px HITEC CLASSIC's third line ends at y 316; each hub's entries open the screens of [the table](#every-screen-old-place-and-new-place). `menuRowsFit()` and `hubEntriesFit()` hold at build time. Mutation: a sixth entry line too long fails the build; a content of 400 px fails the y 316 case. | independent. ESC COMMANDS keeps `freePins` as a hub entry; requirement 12 of [the list above](#what-the-structure-requires-of-dshot) is theirs |
| 5. The rename | Owner decision 6, as [The rename slice's work](#the-rename-slices-work) lists it: the four labels and titles, the seven screen texts, the tests' expectations, README, the wiki in both languages with the page pair Receiver and Empfaenger, the wiring drawing, the release text, rule 9, the previews. It needs [open questions 1 and 2](#open-questions) answered. | Sonnet | The suite passes with the expectations changed and no case removed; `menuRowsFit()` and the title `static_assert`s hold. `python3 test/check_docs.py` with the four old words in rule 9; `tools/check_wiki.py` with the renamed pair; `tools/make_wiring.py --check`. Mutation: `SETTINGS` back in one screen text, its expectation fails; an old word in a wiki page, rule 9 fails. | independent. A text the DShot slices add uses the new names (requirement 14); if they merge first, this slice renames what they added |
| 6. The structure's records | The revision lines of [the tables above](#decisions-of-menu-bmd-this-revises) in menu-b.md, menu.md, esc-commands.md, bluejay-blheli-s.md, usb-bridge.md, angle-meter.md and rx-signals.md; the names table in `docs/design/README.md`; README "Layout" and "The user manual"; the tree of The-Screens and Die-Bildschirme. | Sonnet | `python3 test/check_docs.py` with the removed names `SERVO PROGRAMMER`, `PROGRAMMER` and `TOOLS` as MENU paths; `tools/check_wiki.py`. | independent |
| 7. Palette | `THEME_DARK` in `src/theme.cpp` as `rgb()` triples; the comments of `theme.h`: the table's `C_ONACCENT` row and the entries' meanings. Dark ink on every fill (ui-theme.md decision 3). | Fable | New, `test_widgets.cpp`: each word equals its triple; every entry but `TC_PAPER` differs from every other in both themes, so the colour probes stay valid; [the contrast table](ui-theme.md#contrast) as a check, each text pair 4.5 or more but the named exceptions, each label on a fill 4.5 or more. Mutation: one value changed. | independent |
| 8. Neutral fills | The editor `-` and `+` of ESC SETTINGS, KST SERVO, HITEC SERVO, SERVO SET and OUTPUT SET, and REPAIR, as neutral buttons; the neutral chip, `uiChip()` with `C_GRID`; the header's rule; the hints in `C_DIM`; a slider's fill in `C_GRID`. | Fable | Each button's fill and label by control id, 5 screens. `test_ui.cpp` 1650 and 1670, `test_angle_ui.cpp` 329, `test_bridge_ui.cpp` 264 and 333 rewritten: frame, `C_BG` inside, `C_DIM` label; `chipIs()` gains the case. The rule's row on every screen with a header; `test_servo.cpp` 3394; the hints by text and colour. No fit check changes: no text moves. | independent |
| 9. Radius | ui-theme.md decision 4: the corner solver of `gfxRoundRect()` and `roundFrame1px()` draws `UI_RADIUS`, 5 px. Chips stay square, and the tester's badge and its `SDERR` chip are drawn square. | Fable | `test_gfx.cpp`: the corner pixels of a 5 px fill and frame, insets 2, 1, 0. `test_widgets.cpp`: a chip's corner pixel lit, a button's not; the badge's corner pixel lit. Mutation: the solver put back. | independent |
| 10. Cyan | ui-theme.md decision 1: BACK and the tester's `MENU` as the home tag; SETUP's values and section captions, and the plain values of SERVO SET, OUTPUT SET and ANGLE METER's settings view; a changed value as `C_ONACCENT` on a `C_CYAN` fill with the bar; `>` in `C_CYAN`. Needs slices 3, 4, 7 and 9. | Fable | `test_widgets.cpp`: `uiHeader()` draws BACK on `C_CYAN` with a `C_ONACCENT` label, disabled on `C_BG`, and none at the root; the 130 checks "BACK is drawn live" read the new fill through their one helper. `uiListDrawField()`: Changed is a `C_CYAN` rectangle behind `C_ONACCENT` ink, with the bar; Plain is `C_TEXT` on a device list and `C_CYAN` on a settings list; the fill ends 4 px or more after the label. SETUP: a value and a caption in `C_CYAN`, a warning value still `C_AMBER`. Mutation: the fill removed, the Changed case fails; BACK neutral, the header case fails. | independent |
| 11. Green on the tools | ui-theme.md decision 2: HOLD TO WRITE and HOLD TO RESTORE of ESC WRITE, KST and Hitec, HOLD TO START of SERVO TESTER and S.BUS OUTPUT and HOLD TO APPLY are `C_GREEN` at rest and `C_RED` while held; `uiChip()` with `C_GREEN` draws the outline. Needs slices 7 and 9. | Fable | By control id: each hold's fill and label at rest, while held and disabled. `chipIs()` gains the outlined case; the SAFE, OK, SETTLED and EDT ON checks are rewritten. A walk over the suite's frames: every `C_GREEN` rectangle larger than a chip is a control of the start list. Mutation: one hold left neutral; the chip filled. | independent |
| 12. Green on the tester | ui-theme.md decision 2 on the arm control: HOLD TO ARM `C_GREEN` at rest and `C_RED` from the first frame of the hold; the SAFE badge as an outlined, square chip; ARMED and DISARM as built. Needs slices 3, 9 and 11. | Opus | HOLD TO ARM: `C_GREEN` at rest, `C_RED` while held, `C_BG` while a USB host holds the card. SAFE: the frame and the word in `C_GREEN`, `C_BG` inside. ARMED: the `C_RED` fill. While armed the tester's frame holds no `C_GREEN` pixel. Mutation: the badge filled green; ARM left green while armed. | independent of their logic. It draws in `drawButtons()` and `drawStatusBar()` of `src/ui.cpp`, which they may touch: the second to merge rebases |
| 13. Screens | One pass per screen in each theme for a pair the tables of ui-theme.md do not name: red text on `C_PANEL`, text on a role fill drawn outside `uiChip()` and `uiButton()`. | Fable | A walk over the suite's frames: no text pixel under 4.5 against its background outside the listed exceptions, as a suite check or a script over the shots. | **after them**, so that their screens and texts are in the walk |
| 14. The look's records and previews | The revision lines of [ui-theme.md](ui-theme.md#revisions); `tools/make_previews.py --wiki`; README's "High contrast" and the screens' texts; "grey" replaced by the role in README and the wiki, both languages; STATUS.md open item 25 closed. | Sonnet | `python3 test/check_docs.py`, `tools/check_wiki.py`. | **after them**: the previews show MENU without DShot |
| The DShot slices | `docs/design/dshot-lifetime.md`. | Opus | theirs | after slice 3; before 0.1.0 |

The order. Slices 1 to 6 are a chain. Slices 7 to 9 are a chain of their
own and touch `src/theme.cpp`, `src/gfx.cpp` and `src/ui_widgets.cpp`, not
the registry or the navigation, so they can run beside slices 1 to 4.
Slices 10 to 12 join the two chains. 0.1.0 follows slice 14 and the DShot
slices (owner decision 3).

What each slice leaves on `main`:

- **After slice 2** a chooser's tool returns to its chooser. The screens
  are those of cff09ae.
- **After slice 3** MENU has nine rows and scrolls 120 px, until slice 4.
- **After slice 7** the six `C_GRID` fills carry dark ink at 3.51, until
  slice 8.
- **Between slices 4 and 5** the six rows read ESC TESTER, ESC CONFIG,
  SERVO, RX SIGNALS, SD LOG, SETTINGS.

## Open questions

Questions 1 to 5 of this note as it stood at 71a060f are answered: owner
decisions 4 to 7. The answers raise two.

1. **ANGLE METER's `SETUP`.** ANGLE METER has a footer button `SETUP`
   and a view titled `ANGLE SETUP`, with the texts `SET LENGTH IN SETUP FOR
   MM` and `SETUP NOT SAVED TO FLASH.` (`src/ui_angle.cpp`,
   `src/ui_angle.h`). With owner decision 6 SETUP is also MENU's sixth row,
   and `SET LENGTH IN SETUP FOR MM` then points at the wrong screen.
   (a) The view becomes `ANGLE SET` and its button `SET`, as SERVO TESTER
   has SERVO SET and S.BUS OUTPUT has OUTPUT SET; the texts read `SET
   LENGTH IN ANGLE SET FOR MM`, 30 characters, and `ANGLE SET NOT SAVED TO
   FLASH.`, 29. `SET` at scale 2 is 34 px in the 72 px button. (b) `OPTIONS`,
   rcbench's word for a tool's own settings; at scale 2 it is 82 px and
   does not fit the 72 px button. (c) Nothing changes, and SETUP names two
   places. Recommendation: (a). It revises the texts of
   [angle-meter.md](angle-meter.md), whose decisions are the owner's.
2. **The wiki page Setup.** The page `Setup`, in German `Einrichtung`,
   covers the parts, the cable and the firmware, "from an empty bench to
   the splash screen". It is not named after a screen, so the rename
   leaves it. With decision 6 a reader who looks for the screen SETUP
   finds this page first. (a) The page keeps its name and gains one line
   under its title that sends the reader to The-Screens for the screen
   SETUP. (b) The English page becomes `Installation`; every link to it,
   in the wiki and from outside, changes. Recommendation: (a).

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
5. **The rename's reach.** `SETTINGS` is named on 80 lines of README.md and
   213 lines of the wiki, and the word stays legal as the stored settings
   and in `ESC SETTINGS`. Rule 9 can refuse the old word only as a path or
   a title, so a sentence that names the screen without a path is found by
   reading, not by the check.
6. **Two meanings of LOGS.** rcbench's LOGS browses and plots files.
   MiniRcBench's LOGS starts and stops the recorder and browses nothing.
7. **The DShot note is not read here.** It is written beside this note.
   The plan's column on the DShot slices states this note's side only.
