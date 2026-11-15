# Bluejay and BLHeli_S settings

Status: designed, not built; waits on the owner's answers to the eight
[open questions](#open-questions). The feature is item (6) of the owner's
feature choice of 2026-11-14, recorded in [STATUS.md](../../STATUS.md)
open item 28: Bluejay and BLHeli_S settings, rcbench's
`protocols/escwire` copied, a design note first, then slices with reviews.
No frame of this design has gone to an ESC (electronic speed controller)
from this board, and rcbench's
module has sent none to an ESC either. The firmware facts below are read
from the sources under [sources](#sources), with the file and line; the
ones no readable source settles are marked "not verified" and listed again
under [not verified](#not-verified).

## Terms

UI is user interface, MCU microcontroller
unit, PIO the RP2350's programmable input/output block, DMA direct memory
access, CRC cyclic redundancy check, RAM random-access memory. EFM8 is the
Silicon Labs 8051 MCU family that BLHeli_S and Bluejay ESCs use: EFM8BB1,
EFM8BB2 and EFM8BB51, which the bootloader names by the signatures `E8B1`,
`E8B2` and `E8B5`. Neither part has an EEPROM (electrically erasable
programmable read-only memory); both firmwares keep their settings in one
flash page and call it "EEPROM". The settings page is that page, as the
bootloader reads and writes it. A tag is one of the three 16-byte texts at
the page's offsets 0x40 to 0x6F: the layout tag, which names the ESC's
hardware layout and dead time (for example `#A_L_30#`), the MCU tag
(`#BLHELI$EFM8B21#`) and the name tag (`Bluejay (.0)` or 16 blanks). The
marker is the pair 0x55 0xAA at offsets 0x0D and 0x0E. A layout revision
is the byte at offset 0x02 that says which settings are where.

SIG is the claim's signal pin ([pin-claim.md](pin-claim.md#the-two-pins)).
DShot is the digital ESC protocol the tester sends on SIG; eRPM is the
electrical speed an ESC answers with in bidirectional DShot. 3D is a motor
that runs both ways from one throttle range: Bluejay and BLHeli_S call it
bidirectional, which is not bidirectional DShot.

BLHeli_S, Bluejay, BLHeli_32, AM32 and ESCape32 are names of their owners'
firmware, used here only to say which ESCs a statement is about.

## Sources

Read for this note; commit and file give the place, line numbers are at
that commit.

| Source | Commit | Files | Licence, access |
|---|---|---|---|
| Bluejay (`bird-sanctuary/bluejay`), version 0.21, layout revision 208 (`src/Bluejay.asm` lines 330 to 332) | `main` at 0368d11 | `src/Bluejay.asm` lines 326 to 386, 408, 471 to 480, 485 to 600, 680, 681; `src/Modules/Eeprom.asm` lines 35, 36, 42 to 84, 95 to 150, 204 to 215, 233 to 261, 268 to 293; `src/Modules/Codespace.asm` lines 28 to 49; `src/Modules/Common.asm` lines 144 to 148, 175 to 197; `src/Modules/Isrs.asm` lines 175, 469, 508 to 532; `src/Modules/Settings.asm` lines 94 to 99, 139 to 152; `src/Settings/BluejaySettings.asm` lines 30 to 50; `src/BLHeliBootLoad.inc` lines 9, 17 to 21, 76, 79, 82 to 126, 145 to 198, 233 to 280, 329 to 360, 401 to 407, 425 to 427 | GPL-3.0; read, nothing copied |
| Bluejay release tags v0.2.0 to v0.21.1-RC1 | the tags' commits, listed under [Versions and layouts](#versions-and-layouts) | `Bluejay.asm` or `src/Bluejay.asm`: `EEPROM_FW_SUB_REVISION`, `EEPROM_LAYOUT_REVISION`, `Eep_Name` | as above |
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 (`BLHeli_S SiLabs/BLHeli_S.asm` lines 534 to 536) | a5bcf99 | `BLHeli_S SiLabs/BLHeli_S.asm` lines 44 to 63, 314 to 330, 529 to 588, 3563, 3586 to 3607; `BLHeliPgm.inc` lines 40 to 87, 96 to 136, 194 to 202, 230 to 262; `A.inc` lines 50 to 104; `BLHeliBootLoad.inc` lines 10, 76, 163, 164, 369 to 375, 390 to 394 | GPL-3.0; read, nothing copied |
| AM32 bootloader (`am32-firmware/AM32-bootloader`) | 578ff29 (2026-09-17); 8b1206a (2024-12-06); the first commit with `bootloader/main.c`, 3a9e057 (2024-08-21) | `bootloader/main.c` lines 46, 628 to 633, 835 to 857, 1097 to 1158, 1263, 1278 to 1283; at 3a9e057 lines 580 to 635 | GPL-3.0; read, nothing copied |
| AM32 firmware (`am32-firmware/AM32`) | `main` at 2a695a8; v2.21 at 6b3ef3d | `Src/main.c` lines 1739, 2137 to 2163 and `Src/dshot.c` lines 77, 105 at 2a695a8; lines 1977 to 2003 at 6b3ef3d | GPL-3.0; read, nothing copied |
| BLHeli_S history | the commits that changed `BLHeli_S.asm`, de5ad83 (2016-04-30) to 80ac909 (2022-12-11) | `EEPROM_FW_SUB_REVISION`, `EEPROM_LAYOUT_REVISION` | as above |
| rcbench (`subtilitas/rcbench`) | `main` at 2b4f934a; `protocols/escwire` last changed at f4ba5321 | `protocols/escwire/README.md` lines 42 to 95, 200 to 248, 283 to 286, 368 to 498; `ew_a.c` lines 52 to 60; `ew_id.c` lines 11 to 16, 53 to 86, 128 to 189; `ew_map_s.c` lines 56 to 82; `ew_map_bj.c` lines 9, 57 to 84; `include/ew_session.h` lines 55 to 97, 167 to 259; `include/ew_map.h` lines 33 to 50; `rp2350/ew_line.c`; `docs/EscWire.md` lines 11, 18 to 23, 150; `docs/LinkEsc.md` lines 77 to 111, 153 to 160; `test/host/test_ew_*.c`, `ew_sim.h` lines 234 to 262 | MIT, same holder |
| This repository | `main` at 8c7ace3 | `src/am32_bl.{h,cpp}` (handshake lines 292 to 345), `src/ui_am32.{h,cpp}` (connect page lines 489 to 530, `tryConnect()` lines 810 to 880), `src/pin_claim.h` lines 69, 88, `src/config.h` lines 600 to 640, `src/ui_registry.cpp` line 24, `src/ui_list.{h,cpp}`, `src/backup_ring.h`, [backups.md](backups.md), [menu-b.md](menu-b.md), [esc-commands.md](esc-commands.md), README "AM32 ESC configuration" | |

The Pico SDK (software development kit) is 2.1.1 at
`/home/user/sdk/pico-sdk`; rcbench builds `ew_line.c` against 2.3.0.

## What it is for

- Read a Bluejay or BLHeli_S ESC's settings on the bench, with no flight
  controller and no computer: direction, 3D, timing, startup power,
  braking, beeps and beacon, temperature limit, LED (light-emitting
  diode).
- Change them, with a backup on the card first and a read-back after.
- Recognise an ESC that lost its tags, say so, and write nothing to it.

It does not flash firmware, change a tag, write an older layout, or drive
the motor. Item (5) of the same feature choice, a USB (Universal Serial
Bus) bridge for desktop ESC tools, is the way to flash; its note is
written separately.

## The ESC side

### Entering the bootloader

Both firmwares jump to their bootloader from the same place,
`init_no_signal`, when the signal line stays high long enough:

| | Bluejay 0.21 | BLHeli_S 16.7 |
|---|---|---|
| Check | `input_high_check`, 9 × 256 × 256 loops, "about ~150ms" (`Bluejay.asm` 495 to 508) | 250 × 250 loops, "more than 15ms" (`BLHeli_S.asm` 3590 to 3605) |
| Reached | after the start-up melody at power-up (`Bluejay.asm` 471 to 480), and whenever the pulses stop: the timeout counter starts at 10 (`Isrs.asm` 175, 469) and counts down every 32 ms before arming (`Isrs.asm` 508, 532), so about 320 ms after the last frame (`Bluejay.asm` 680, 681) | after its start-up beeps, and whenever the pulses stop (`BLHeli_S.asm` 3779, 3785, 3985, 4004) |
| Bootloader at | 0x1C00 on EFM8BB1 and EFM8BB2, 0xF000 on EFM8BB51 (`Codespace.asm` 31 to 49) | 0x1C00, or 0xF000 for the EFM8BB51 builds (`BLHeli_S.asm` 3601 to 3605) |

So an EFM8 ESC that is powered when its DShot frames stop enters its
bootloader by itself once the line rests high: Bluejay about 320 ms plus
150 ms after the last frame, BLHeli_S after its timeout plus 15 ms. No power
cycle is needed. AM32's bootloader decides at every reset, about 46 ms
after it, and then stays; AM32's firmware resets itself 0.5 s or 2 s after
its last frame ([AM32's bootloader](#am32s-bootloader)). Not
verified on an ESC: the loop times are the source's comments, not measured.

The bootloader then waits for the first low edge with no timeout
(`wait_for_low`, `BLHeliBootLoad.inc` 96 to 98). It scans for the six
characters `BLHeli` and a CRC; a character that does not match restarts the
scan, 250 times at most (lines 82, 117 to 126); the greeting below uses 12
of the 250 tries, its first 0x00 serving as the edge the scan starts on.
A scan with no edge for
`BOOT_DELAY`, "about 250ms", starts the firmware (lines 9, 101 to 115). Once
the CRC holds it answers 9 bytes: `471d`, the two signature bytes, the
bootloader version 6, the page count 1 (lines 131 to 143, 425 to 427), and
0x30 for success (lines 149 to 151). After that it waits for commands,
again with no timeout.

### The greeting already fits

AM32 CONFIG's greeting is twelve 0x00, 0x0D, `BLHeli` and 0xF4 0x7D
(`am32_bl.cpp` line 302). 0x7DF4 is the CRC-16 (polynomial 0xA001, initial
value 0) of `BLHeli` alone, computed for this note, and the EFM8 bootloader
resets its CRC at each new scan (line 89), so the greeting is a valid EFM8
greeting; the comment in `am32_bl.cpp` that F4 7D "is a literal, not a
checksum" holds only for the 19 bytes before it. rcbench's first contact
is the same with eight 0x00 (`ew_a.c` 52 to 60).

An EFM8 answer today ends in 0x30, so `am32BlHandshake()` passes it and
reads the fifth byte from the end as the AM32 family (lines 330, 334):
0xE8, no family it knows, `AM32_ERR_NAK` (line 341). The connect page then
shows `9B: 34 37 31 @19200`, `REPLY NOT UNDERSTOOD:` and `IS THE ESC
RUNNING AM32?` (`ui_am32.cpp` lines 830 to 836). The signature sits where
the family byte sits in an AM32 answer, and 0xE8 is none of 0x1F, 0x2B and
0x35, so one greeting tells the two apart. Read from the code, not seen on
an ESC.

A second first contact to a bootloader that already answered: its leading
four 0x00 read as the command 0x00 0x00 with the CRC 0x00 0x00, which the
bootloader takes as "restart bootloader" (`Cmdh` 0 and `Cmdl` 0, lines 149
to 198, `rst` to `init`), and the rest of the string is a new greeting. So
rcbench's session can make its own first contact after AM32 CONFIG's
greeting found the ESC. From the source; not verified on an ESC.

### AM32's bootloader

AM32's bootloader (`am32-firmware/AM32-bootloader` at 578ff29,
`bootloader/main.c`) decides at every reset in `checkForSignal()` (lines
1097 to 1158). It pulls its pin down, waits 0.5 ms and takes up to 4000 samples
10 µs apart; more than 450 low samples start the firmware. It then pulls
the pin up, waits 0.5 ms and takes 500 more samples. A line that read high
in every sample keeps it in the bootloader; any low sample leads to a last
5 ms with the pin floating, where one low sample starts the firmware. So
the line has to stay high, against the pin's pull-down, for about 46 ms
after power-up. The bootloader then waits for a start bit with no timeout
(`serialreadChar()`, lines 835 to 857) and leaves only on a line low for
more than 20 ms (lines 841 to 851), on more than 100 invalid commands
(lines 1278 to 1280), on its run command (lines 628 to 633), or, in
DroneCAN builds, on CAN (lines 1281 to 1283). The 4000 samples came in
8b1206a (2024-12-06); the first commit with `bootloader/main.c`, 3a9e057
(2024-08-21), has the same rule with 500 samples pulled down, about 11 ms
in all.

After a software reset the rule is milder. With `CHECK_SOFTWARE_RESET` 1
(line 46) the more-than-450 jump is skipped when `bl_was_software_reset()`
holds (lines 1116 to 1119); a low sample before the floating check then
only leads to that check, so only a low in its last 5 ms starts the
firmware. And AM32's firmware resets itself: `NVIC_SystemReset()` after
0.5 s with no valid frame while armed and after 2 s while disarmed
(`Src/main.c` lines 2137 to 2163 at 2a695a8, the counter at 1739, cleared
by a DShot frame at `Src/dshot.c` lines 77 and 105; lines 1977 to 2003 in
v2.21 at 6b3ef3d).

So under today's loop, which holds the line low for at least 6 ms of every
41 ms, a power-up most likely starts the firmware, but the firmware sees no
DShot, resets itself after 2 s, and the bootloader after that reset stays
unless a greeting's low bits fall into its last 5 ms, about 16 of every
41 ms (the 11 ms greeting and the 5 ms check). A powered AM32 is reached
within a few resets; this fits AM32 CONFIG's hardware use recorded in
STATUS.md (ab79c67, bd90e17). A quiet line keeps an AM32 in its bootloader
as it keeps an EFM8. The README's "Connecting" states this rule as the
bootloader's source gives it, not measured. Not verified: which bootloader
AM32 ESCs in the field run, whether AM32's firmware reads the greeting as a
signal and so does not reset, and whether the bench's pull-up, 50 to
80 kΩ, holds the line high against the AM32 pin's pull-down.

### The settings page

| | Bluejay 0.21 | BLHeli_S 16.7 |
|---|---|---|
| Page | 0x1A00 on EFM8BB1 and BB2, 0x3000 on BB51 (`Codespace.asm` 31 to 48) | 0x1A00, 0x3000 on BB51 builds (`BLHeli_S.asm` 529 to 532) |
| Flash page erased | 512 bytes on BB2, 2048 on BB51 (rcbench `ew_id.h`) | as Bluejay |
| Revisions | 0x00 main 0, 0x01 sub 21, 0x02 layout 208 (`Bluejay.asm` 330 to 337) | 0x00 main 16, 0x01 sub 7, 0x02 layout 33 (`BLHeli_S.asm` 534 to 540) |
| Marker | 0x55 0xAA at 0x0D, 0x0E (`Bluejay.asm` 348, 349) | the same (`BLHeli_S.asm` 552, 553) |
| Settings | 0x03 to 0x0C, 0x0F to 0x2A (`Bluejay.asm` 338 to 378) | 0x03 to 0x0C, 0x0F to 0x28 (`BLHeli_S.asm` 542 to 579) |
| Tags | layout 0x40, MCU 0x50, name 0x60, 16 bytes each (`Codespace.asm` 44 to 46; `Common.asm` 175 to 197; name `Bluejay.asm` 381, 382) | the same (`A.inc` 50 to 104; name, 16 blanks, `BLHeli_S.asm` 586 to 588) |
| More | the start-up melody, 140 bytes from 0x70 (`Eeprom.asm` 36, `Codespace.asm` 47) | none |
| Read and written by rcbench | 255 bytes, 0x00 to 0xFE | 112 bytes, 0x00 to 0x6F |

### Why an interrupted write loses the tags

The survey's fact, "an interrupted BLHeli_S/Bluejay session loses the 48 tag
bytes and leaves settings read-only", holds by both sources, under one
condition: the ESC restarts while its page is erased or half programmed.

1. A host writes a page in two commands: erase the whole flash page, then
   program the new bytes (rcbench README lines 209 to 211; the bootloader's
   erase and program, `BLHeliBootLoad.inc` 228 to 280). Between the two the
   page reads 0xFF. The program writes in rising address order, so a
   program cut short can leave the marker at 0x0D written and the tags at
   0x40 to 0x6F still 0xFF.
2. The bootloader never leaves on its own while it waits for a command. A
   cut link or a bench restart leaves the ESC in its bootloader with the
   page as it is, for as long as the ESC keeps power.
3. At its next start the firmware checks the marker (`Eeprom.asm` 42 to 53;
   `BLHeliPgm.inc` 40 to 52). Without it, it loads its defaults and calls
   `erase_and_store_all_in_eeprom` (`Eeprom.asm` 55 to 60; `BLHeliPgm.inc`
   54 to 61).
4. That routine first copies the 48 tag bytes from flash into RAM
   (`read_esc_layout`, `ESC_LAYOUT_SIZE` 48, `Eeprom.asm` 35, 97, 233 to
   245; `read_tags`, `#48`, `BLHeliPgm.inc` 98, 230 to 244), erases the
   page, writes the defaults, and writes the 48 bytes back from RAM
   (`Eeprom.asm` 141; `BLHeliPgm.inc` 132, 252 to 262). The tags in flash
   were 0xFF, so 0xFF goes back. Bluejay copies its melody the same way
   (`Eeprom.asm` 98, 142).
5. Nothing in either firmware writes a tag from anywhere else: the tag
   labels appear only in these two routines (`grep` for `Eep_ESC_Layout`
   finds `Eeprom.asm` 237 and 254 and the definitions). The ESC runs, on
   its defaults when the marker was missing, but its tags stay 0xFF.
6. rcbench identifies an EFM8 page by its name tag (`ew_id.c` 93 to 98,
   148 to 189). A name tag of 0xFF with BLHeli_S's revisions or Bluejay's
   is `EW_WHY_RESET_PAGE`, shown and not written (lines 164 to 179); its
   guard refuses a write that changes a tag (README line 247). The page
   stays read-only here.

rcbench's guard keeps the tags as read and has no way to set them, and
rcbench gives no reason. This note reads one: the layout tag names the
ESC's hardware, its FET (field-effect transistor) drive and dead time; a
desktop tool that picks the firmware image to flash by it (not verified
here) could pick an image for other hardware from a wrong tag. The tags return
with a firmware flash for that ESC's layout, whose image carries them
(`CSEG AT 1A40h`, `A.inc` line 50), or with a restore from a backup of the
same ESC ([question 4](#open-questions)).

The firmware's own saves have the same window: a DShot save command, or the
defaults above, erase the page and write it back with interrupts off, and a
power cut in between loses the tags the same way (`Eeprom.asm` 95 to 150).
That is outside this tool.

Exposure, from byte counts at 19200 baud, 520.8 µs a byte: from the erase's
answer to the program command, one address frame (6 bytes), the announce
(6 bytes), 520 µs, the data and its CRC (257 bytes for Bluejay, 114 for
BLHeli_S), the program frame (4 bytes) and two 1-byte answers with their
lead-in, about 1.3 ms: about 144 ms for Bluejay and 70 ms for BLHeli_S, plus
the part's erase and write times and its answer delays, which are not
measured. rcbench states a whole write, its reads
included, at about 3848 ms (`docs/EscWire.md` line 150), a figure from
its simulated ESC.

### Versions and layouts

Bluejay, from the release tags:

| Versions | Tag commits | Layout revision | Here |
|---|---|---|---|
| 0.2 to 0.8 | 003c43b (v0.2.0) to 26a1507 (v0.8) | 33 | shown |
| 0.9 | ad8f3ed | 200 | shown |
| 0.10, 0.11 | 34f34c6, e4d7d60 | 201 | shown |
| 0.12 to 0.14 | 9a6c8d0 to f6db49b | 203 | shown |
| 0.15 to 0.18 | 8df0db8 to 8a86a15 (v0.18.2) | 204 | shown |
| 0.19 | f5bb174 to b803b8d (v0.19.2) | 206 | shown |
| 0.20 | 23b55c6, 80bd588 | 207 | shown |
| 0.21 | 93bf3e1 (v0.21.0), 1b0802d (v0.21.1-RC1), `main` 0368d11 | 208 | written |

No release tag stores 202 or 205; the test tags test-melody (860e3ff) and
test-melody-pwm (cd792a0) store 205. rcbench refuses both (`ew_map_bj.c`
line 9, `ew_id.c` 128 to 133). The tags v0.2.0, v0.2.1, v0.3.0 and v0.4.0
store sub revisions 20, 21, 30 and 40, and v0.17.0 and v0.18.0 (c151252)
both store 17, so the strip reads 0.20, 0.21, 0.30, 0.40 and 0.17 for them;
layout 33 tells v0.2.1 from 0.21. The name tag starts
with `Bluejay` at each tag read for it: `Bluejay (BETA)` at v0.2.0 and v0.9,
`Bluejay` at v0.15, v0.19.0 and v0.20.0, `Bluejay (.0)` at v0.21.0, and
`Bluejay (.1 RC1)` on `main`.

BLHeli_S, from the commits to `BLHeli_S.asm`:

| Versions | Commits | Layout revision | Here |
|---|---|---|---|
| 16.0 to 16.2 | de5ad83 to 4300db1 | 32: TEMPERATURE an on/off switch | shown |
| 16.3 to 16.7 | 3959250 to 80ac909 | 33: TEMPERATURE 80 °C to 140 °C, "Implemented programmable temperature protection" (line 56) | written |
| 16.71, 16.72 | 381c9aa, 72fc0af, 46dc50f (2019) | 33 | written, as 33 |

DShot arrived in 16.5 (line 60); 16.0 to 16.4 do not run on the tester at
all. Whether 16.7 takes the tester's inverted DShot is not verified
([esc-commands.md](esc-commands.md#rules), bench item B7 there); BLHeli_S
sends no eRPM. rcbench refuses a blank name tag with sub revision 8 or 9
(`EW_WHY_VARIANT`, `ew_id.c` 180 to 183); no such build is in `bitdump/BLHeli`
and what it is, is not verified here.

## The fields

rcbench's maps, `ew_map_s.c` and `ew_map_bj.c`, checked here offset by
offset against the two sources: every offset matches the `Eep_` label at
that place. The ranges are rcbench's; they agree with the comments of the
defaults (`BLHeli_S.asm` 314 to 330, `BluejaySettings.asm` 30 to 50) where
those give one. Whether a desktop configurator offers narrower ranges is not
verified.

The screen shows rcbench's field name as the row's label and rcbench's
value text (`ew_field_text()`) as its value, so the screen and the backup
file use the same words. The font draws letters in capitals, so `10min`
reads `10MIN`. One exception: DIRECTION's values 3 and 4 read `3D` and `3D
REVERSED` on the screen, where rcbench's file says `BIDIRECTIONAL` and
`BIDIRECTIONAL REVERSED`. The shorter words keep a change line within 37
characters (`3D REVERSED -> REVERSED`, 23 characters, 137 px) and avoid the
mix-up with bidirectional DShot. A value outside the field's values reads
`RAW 17`, in the warning colour, as rcbench writes it.

Widths are at scale 1, (6n − 1) px for n characters, the label from x 8; the
value at scale 2 while it is at most 104 px wide, else at scale 1, right to
x 231 (`uiListDrawField()`, `src/ui_list.h`). The gap column is the space
between the label's end and the widest value's start; the narrowest is
23 px.

BLHeli_S, layouts 32 and 33:

| Offset | Label | Values | Unit | Edit | Label px | Gap px |
|---|---|---|---|---|---|---|
| 0x09 | STARTUP POWER | 1 to 13: 0.031, 0.047, 0.063, 0.094, 0.125, 0.188, 0.25, 0.38, 0.50, 0.75, 1.00, 1.25, 1.50 | | yes | 77 | 65 |
| 0x0B | DIRECTION | 1 to 4: NORMAL, REVERSED, 3D, 3D REVERSED | | yes | 53 | 77 |
| 0x0F | STICK PROGRAMMING | 0, 1: OFF, ON | | yes | 101 | 41 |
| 0x15 | TIMING | 1 to 5: LOW, MEDIUM LOW, MEDIUM, MEDIUM HIGH, HIGH | | yes | 35 | 107 |
| 0x19 | MIN THROTTLE | 4 × value + 1000 | µs, `US` | shown | 71 | 83 |
| 0x1A | MAX THROTTLE | as MIN THROTTLE | µs | shown | 71 | 83 |
| 0x1B | BEEP STRENGTH | 1 to 255 | | yes | 77 | 89 |
| 0x1C | BEACON STRENGTH | 1 to 255 | | yes | 89 | 77 |
| 0x1D | BEACON DELAY | 1 to 5: 1MIN, 2MIN, 5MIN, 10MIN, INFINITE | | yes | 71 | 59 |
| 0x1F | DEMAG | 1 to 3: OFF, LOW, HIGH | | yes | 29 | 113 |
| 0x21 | CENTRE THROTTLE | as MIN THROTTLE; used in 3D | µs | shown | 89 | 65 |
| 0x23 | TEMPERATURE | layout 33: 0 to 7: OFF, 80C to 140C in 10 °C steps; layout 32: OFF, ON | | yes | 65 | 77 |
| 0x24 | LOW-SPEED PROTECTION | 0, 1: OFF, ON | | yes | 119 | 23 |
| 0x27 | BRAKE ON STOP | 0, 1: OFF, ON | | yes | 77 | 65 |
| 0x28 | LED | 0 to 255, 2 bits per LED | | yes | 17 | 173 |

12 fields are written in layout 33, as rcbench's README states (line 73).
The throttle bytes are shown and not edited because their range is not
known (`ew_map_s.c` lines 42 to 46).

Bluejay, layouts 33 to 208; "Layouts" names those that have the row, and
only 208 is written:

| Offset | Label | Values | Unit | Layouts | Label px | Gap px |
|---|---|---|---|---|---|---|
| 0x04 | STARTUP BOOST | 0 to 255 | | 200 | 77 | 113 |
| 0x04 | MIN STARTUP POWER | 0 to 255; "(1000..1125 Throttle)" | | 201 to 208 | 101 | 89 |
| 0x05 | STARTUP SOUND | 0, 1: SHORT BEEP, MELODY | | 200 to 208 | 77 | 65 |
| 0x06 | DITHERING | 0, 1: OFF, ON | | 200 to 207 | 53 | 89 |
| 0x07 | MAX STARTUP POWER | 0 to 255; Bluejay uses at most 80 (`Settings.asm` 94 to 99), 81 to 255 are stored and not used | | 201 to 208 | 101 | 89 |
| 0x09 | STARTUP POWER | 1 to 13, the BLHeli_S scale | | 33, 200 | 77 | 65 |
| 0x09 | POWER SLOPE | 0 to 13, 0 is off | | 201 to 208 | 65 | 77 |
| 0x0A | PWM INDICATOR | 24, 48 or 96: the build's PWM (pulse-width modulation) frequency, "Temporary method for display" (`Bluejay.asm` 345) | kHz; the screen shows the number alone | 203 to 208, shown | 77 | 113 |
| 0x0B | DIRECTION | 1 to 4: NORMAL, REVERSED, 3D, 3D REVERSED | | 33 to 208 | 53 | 77 |
| 0x10 | BRAKING | 0 to 255 as 0 to 100 | % | 204 to 208 | 41 | 137 |
| 0x15 | TIMING | 1 to 5, as BLHeli_S | | 33 to 208 | 35 | 107 |
| 0x1B | BEEP STRENGTH | 0 to 255 | | 33 to 208 | 77 | 89 |
| 0x1C | BEACON STRENGTH | 0 to 255 | | 33 to 208 | 89 | 77 |
| 0x1D | BEACON DELAY | 1 to 5, as BLHeli_S | | 33 to 208 | 71 | 59 |
| 0x1F | DEMAG | 1 to 3, as BLHeli_S | | 33 to 208 | 29 | 113 |
| 0x23 | TEMPERATURE | 0 to 7, as BLHeli_S 33 | | 33 to 208 | 65 | 77 |
| 0x24 | LOW-SPEED PROTECTION | 0, 1 | | 33, 200 | 119 | 23 |
| 0x27 | BRAKE ON STOP | 0, 1 | | 33 to 208 | 77 | 65 |
| 0x28 | LED | 0 to 255 | | 33 to 208 | 17 | 173 |
| 0x29 | POWER RATING | 1, 2: 1S, 2S AND ABOVE | | 206 to 208 | 71 | 71 |
| 0x2A | SAFETY ARM | bit 0: arm only after the EDT (Extended DShot Telemetry) enable | | 207, 208, shown | 59 | 131 |

15 fields are written in layout 208. The revision bytes 0x00 to 0x02 go on
the strip, not into the list. Rows group as AM32 CONFIG's do, the group in
`C_GRID` above its first row: MOTOR (DIRECTION, TIMING, DEMAG, and on
BLHeli_S STARTUP POWER), STARTUP (Bluejay's startup rows and STARTUP SOUND),
BRAKE, LIMITS (TEMPERATURE, LOW-SPEED PROTECTION, POWER RATING), SOUND
(BEEP STRENGTH, BEACON STRENGTH, BEACON DELAY), INPUT (STICK PROGRAMMING and
the throttle bytes), OTHER (LED, PWM INDICATOR, SAFETY ARM). A shown byte is
a row drawn `ReadOnly`.

SAFETY ARM set makes Bluejay arm only after the EDT enable (`Bluejay.asm`
378, 694); the tester sends the enable first after every pump start, so the
bit does not stop it. POWER RATING
picks the reference with which a Bluejay on EFM8BB2 reads its temperature,
the internal 1.65 V for 1S and the 3.3 V supply above (`Settings.asm` 139
to 152); a wrong value misreads the TEMPERATURE limit.

## The design

### One screen, two transports

AM32 CONFIG grows; no second screen. Its connect page sends the greeting it
sends today, and the answer decides which editor opens:

- An AM32 family byte: the AM32 editor as today, on `am32_bl` on core0.
- `471` with signature `E8B1`, `E8B2` or `E8B5`: the screen ends `am32_bl`
  (`am32BlEnd()`) and hands the ESC to an rcbench escwire session on
  core1, which makes its own first contact, reads, identifies and opens
  the Bluejay or BLHeli_S list.
- Anything else: `REPLY NOT UNDERSTOOD:` as today.

The classification moves out of `am32BlHandshake()` into a pure function
on the reply's bytes, tested on the host; the bit-banged link stays as it
is. The MENU row, the titles and the strip follow
[question 1](#open-questions); this note writes them for its recommendation,
`ESC CONFIG`.

A separate screen would need its own MENU row, against the seven rows of
[menu-b.md](menu-b.md#menu), or a chooser, and the user would have to know
which firmware the ESC runs before the first byte. The connect page needs
no such choice: the greeting is the same, the bootloader's answer names the
part. The cost is the connect cycle below, which serves both firmwares at
once.

### The connect cycle

The greeting that AM32 CONFIG repeats today, every 30 ms
(`AM32_GREET_TMO_MS`, `config.h` line 611) plus 11 ms on the wire, holds
the line low at least every 41 ms, for at least 6 ms. Bluejay's 150 ms
check then never passes, and BLHeli_S's 15 ms check fits between two
greetings and passes about 1 time in 3; not measured. The same low bits,
inside the 46 ms after an AM32's power-up, start its firmware
([AM32's bootloader](#am32s-bootloader)). All three bootloaders, once
entered, wait on a high line with no timeout. So the line rests high most of
the time and the greetings come in short bursts. The connect page runs in
phases, by [question 2](#open-questions) (d):

| Phase | Time | Line | Status line |
|---|---|---|---|
| Claim | as today | DShot drains, then pull-up | `RELEASING SIG GP0` |
| Quiet | 1000 ms | on the pull-up, as between greetings today; nothing sent | `LINE HIGH, NOTHING SENT` |
| Greet | 3 greetings, about 125 ms | the greeting as today, the line on the pull-up for the answer | `WAITING... (TRY n)` |
| Quiet, greet, ... | until an answer or BACK | | |

The quiet phase leaves the line on the pull-up. Driving it high, as
`ew_session_hold()` does, would hold it against an AM32 pin's pull-down in
the power-up check, but it costs more: the line would be driven about 89 %
of the time, exactly while the user unplugs and replugs the ESC, and a
driven pin can feed an unplugged ESC through its signal pin's protection
diode; 2 mA is the pad's drive setting, not a limit. Today the line is
driven only in greetings and DShot frames (`src/dshot/out_dshot.c`
line 183 puts the pull-up on for the rest). An AM32 missed at power-up on
the pull-up still returns by its firmware's reset. No source gives a reason
to drive it; the current into SIG is not measured either way (B13).

A Bluejay that was running on DShot drops to `init_no_signal` about 320 ms
after the pump stops and enters its bootloader 150 ms later, inside the
first quiet second. One that is power-cycled has had no signal: it fails
its check if a burst falls into it, looks for DShot300 and DShot600 for
100 ms each, and checks again, about every 200 ms (`Bluejay.asm` 574 to
592); a check that starts in the first 850 ms of a quiet second passes.
BLHeli_S needs 15 ms of the 1000. An AM32 stays when its 46 ms after
power-up fall into a quiet stretch; a power-up within the 125 ms of a
burst or the 46 ms before it starts its firmware, about 171 of every
1125 ms, 15 %, and comes back about 2 s later by its firmware's own reset
(not measured). All of this follows from the sources; none is measured.

### The escwire session

Copied unchanged from rcbench (see [what is copied](#what-is-copied)), run
by a core1 job, as KST and Hitec run theirs:

- `src/escwire_job.{h,cpp}`: core0 posts one request at a time, ENTER,
  READ, WRITE with the draft, LEAVE, STOP; core1 calls `ew_session_step()`
  on every loop pass while the claim holds and posts the result. Core0
  touches the session only between requests.
- The driver is rcbench's `ew_line` on one PIO state machine, 25
  instructions, and 2 DMA channels (rcbench README lines 392 to 395); the
  claim has freed pio2 from DShot and KISS. Every SDK call `ew_line.c`
  makes exists by name in the 2.1.1 headers (checked with `grep`); the build
  is slice 1's check.
- `PinUser::Am32` binds no core1 job today (`pin_claim.h` lines 69, 88). It
  gets `Core1Job::EscWire` with `deferJob` true, as HITEC (`pin_claim.h`
  line 91): the job starts only when the connect page hands it an EFM8.
  `am32BlEnd()` leaves the pin an input with its pulls off, floating
  (`am32_bl.cpp` lines 233 to 238), `ew_line`'s open parks it with its
  pull-down (rcbench README line 407), and `ew_session_hold()` drives it
  high. The open's pull-down park cannot be skipped without changing
  rcbench's code, so the gap is to be one core1 loop pass, with
  `am32BlEnd()` called last, right before the open; a low in it reads to a
  bootloader that answered as part of a command, which the contact's
  leading 0x00 then turn into the restart above. Not verified.
- ENTER: `ew_session_hold()`, the line driven high, then
  `ew_session_enter(s, 500)`, the shortest hold rcbench takes, then the
  first contact with 3 tries of 100 ms (`ew_a.h`). After 3 silent tries
  the session also tries ESCape32's text interface, the line high
  2500 ms, then `info` at 38,400 baud (rcbench README lines 113 to 119),
  which the EFM8 bootloader most likely refuses: it checks only the CRC's
  low byte (`get3`, `BLHeliBootLoad.inc` lines 401 to 406; `jnz mai1`,
  line 177), so a garbled frame passes 1 time in 256. Not verified. Silence or
  another answer: the job closes the line, `am32_bl` takes the pin again,
  and the connect cycle resumes with `THE ESC ANSWERED, THEN STOPPED.` and
  `SEARCHING AGAIN.`
- READ: the page, 112 or 255 bytes, then `ew_id_efm8()`.
- WRITE: rcbench's write. The guard judges the draft: only editable fields
  of a written layout may change, each to one of its values, tags, marker
  and unnamed bytes as read. The session reads the page again and compares
  it with the first read; then address, erase, address, announce and data,
  program; then reads back and compares with the draft. A failed step or a
  differing read-back runs the procedure again from the erase, 3 times at
  most (rcbench README lines 200 to 230).
- LEAVE: `ew_session_leave()`: the frame `00 01`, which starts the firmware
  (`BLHeliBootLoad.inc` 184 to 197), 500 µs idle, 300 ms low, then the line
  parked, and the claim gives SIG back to DShot. [Question 3](#open-questions).
- The heartbeat: when core0's heartbeat lapses for 250 ms the job calls
  `ew_session_stop()`, never `ew_session_abort()`: a stop lets a write that
  has sent its erase run to its read-back, then parks the line (rcbench
  `ew_session.h`, `ew_session_stop()`), as a KST write finishes
  ([pin-claim.md](pin-claim.md#decisions), decision 1). Outside a write a
  stop cuts at once, so a running read is cut, where the KST and Hitec jobs
  let a read finish.

The rest of rcbench's session, AM32 over the newer bootloader protocol and
ESCape32's text interface, comes along in the copied files and is not
called: the AM32 editor stays on `am32_bl`, and the session is only ever
entered for an EFM8 that already answered.

### What each page shows

`ew_id_efm8()` decides the view:

| Identification | View | Strip |
|---|---|---|
| Bluejay 208, BLHeli_S 33, marker 0x55 0xAA | the list, editable | `BLUEJAY 0.21  LAYOUT 208  E8B2` (30 characters, 179 px), `C_DIM` |
| Bluejay 33 to 207 or BLHeli_S 32 (`EW_WHY_OLD_LAYOUT`) | the list, every row `ReadOnly`; WRITE... drawn disabled; the backup saved as the view opens | `BLUEJAY 0.19, LAYOUT 206: READ-ONLY.` (36, 215 px) over `SHOWN ONLY. BACKUP BJ018` (24, 143 px), amber |
| Name tag 0xFF with BLHeli_S layout 33 or Bluejay layout 208 (`EW_WHY_RESET_PAGE`, `ew_id.c` 164 to 179); an older layout with lost tags reads PAGE NOT KNOWN, the last row | as above | `ESC LOST ITS TAGS: READ-ONLY.` (29, 173 px) over `REFLASH ON A COMPUTER. BACKUP BH018` (35, 209 px) |
| Marker not 0x55 0xAA (`EW_WHY_MARKER`) | as above | `SETTINGS MARKER WRONG: READ-ONLY.` (33, 197 px) over the backup line |
| Page 0xFF throughout | the connect page, which has no WRITE... | status `EFM8 E8B2: SETTINGS PAGE EMPTY` (30, 179 px); `PAGE EMPTY, AS AFTER A CUT WRITE.` (33, 197 px) and `KEEP THE ESC POWERED: SEE MANUAL.` (33, 197 px) |
| Any other refusal (`EW_WHY_NOT_KNOWN`, `BJ_ON_BB1`, `LAYOUT`, `VARIANT`) | the connect page | status `EFM8 E8B2: PAGE NOT KNOWN` (25, 149 px), or `BLUEJAY ON EFM8BB1: NOT TAKEN`, `BLUEJAY LAYOUT 205: NOT TAKEN` (29, 173 px), `BLHELI_S 16.9: NOT TAKEN` (24, 143 px); `NOT BLUEJAY OR BLHELI_S AS KNOWN:` (33, 197 px) and `NOTHING IS WRITTEN.` (19, 113 px) |

The second strip line of a read-only view without a card reads `SHOWN
ONLY. NO CARD: NO BACKUP.` (31, 185 px), with a card that refuses it `SHOWN
ONLY. BACKUP FAILED.` (26, 155 px); for lost tags `REFLASH ON A COMPUTER.
NO CARD.` (31, 185 px) and `REFLASH ON A COMPUTER. BACKUP FAILED.` (37,
221 px). The name in the version, `BLHELI_S 16.7  LAYOUT 33  E8B1`, is
`BLUEJAY` or `BLHELI_S` and the main and sub revision as numbers.

### The list, the editor and the write page

As AM32 CONFIG's: the field rows of `ui_list`, a changed value `C_CYAN`
with its 3 px bar, the editor bar with `-` and `+` and the swipe's coarse
step, the footer `WRITE...`, `REVERT`, `HEX`, the BACK guard on unwritten
edits, `WRITE...` opening the write page on a tap and only its 1000 ms
HOLD TO WRITE writing, BACK drawn disabled while a write runs, the BOARD
voltage refusal, the page writing what it shows. HEX lists the 112 or 255
bytes, 8 a line, in a scrolling list. A coarse step on a field of 0 to 255
is 24, as on AM32.

Strip notes, two amber lines in place of the caption, the selected row's
first:

| When | Lines |
|---|---|
| DIRECTION at 3D or 3D REVERSED | AM32's `3D MODE: 50% THROTTLE IS FULL POWER,` / `ABOVE 50% THE OTHER WAY. PROP OFF.`: the three open firmwares split DShot alike in 3D ([esc-commands.md](esc-commands.md#3d-and-the-testers-throttle)) |
| TEMPERATURE or LOW-SPEED PROTECTION selected at OFF | AM32's `PROTECTION OFF: THE ESC DOES NOT` / `CUT POWER ON THIS LIMIT.` |
| POWER RATING selected | `SETS HOW THE ESC READS ITS TEMP:` (32, 191 px) / `A WRONG VALUE MISREADS THE LIMIT.` (33, 197 px) |

A read with DIRECTION at 3 or 4 sets the tester's 3D note, and a verified
write with 1 or 2 clears it, by the rule AM32 CONFIG's 3D MODE follows
(`uiAm32Esc3d()`).

The write page is titled `ESC WRITE` and lists `label` and `old -> new` as
AM32 WRITE does; a change that meets its label moves to the line below
(`drawWritePage()`). The widest change text is `MEDIUM HIGH -> MEDIUM LOW`,
25 characters, 149 px. The backup line and its no-card and USB variants are
AM32's, with the backup's own name, `BACKUP FIRST: BACKUP/BJ001.TXT` (30,
179 px).

Results, at y 196 and 208:

| Result | First line | Second line |
|---|---|---|
| `EW_SES_OK` | `WRITE VERIFIED. BACKUP BJ018` (28, 167 px), green | `LEAVING STARTS THE ESC WITH THEM.` (33, 197 px); with question 3 (b), AM32's `POWER-CYCLE THE ESC TO APPLY.` |
| `EW_SES_ERR_READ` | `READ FAILED: NOTHING ERASED.` (28, 167 px), amber | `TAP OK, WRITE AGAIN.` (20, 119 px) |
| `EW_SES_ERR_CHANGED` | `ESC PAGE CHANGED: NOT WRITTEN.` (30, 179 px) | `TAP OK, READ AGAIN.` (19, 113 px) |
| `EW_SES_ERR_WRITE_FAILED` | `WRITE FAILED: PAGE AS READ.` (27, 161 px) | `TAP OK, WRITE AGAIN.` |
| `EW_SES_ERR_TORN` | `HALF WRITTEN: KEEP THE ESC POWERED.` (35, 209 px) | `TAP OK, WRITE AGAIN NOW.` (24, 143 px) |
| `EW_SES_ERR_LOST` | `NO ANSWER IN THE WRITE: KEEP POWER.` (35, 209 px) | `TAP OK, THEN READ AGAIN.` (24, 143 px) |
| no backup, BOARD voltage red | AM32's lines | AM32's lines |

After `EW_SES_ERR_LOST` and `EW_SES_ERR_CHANGED`, `READ AGAIN` takes
`WRITE...`'s place in the footer at scale 1, as on HITEC SERVO, until a read
succeeds.

While the page is half written (rcbench's `dirty`), the session takes a
write only after a read and only of the draft or of the page as first read,
and it refuses LEAVE (`ew_session.c` lines 812 to 822, 868 to 925). A
`TORN` result ends in a read-back, so WRITE may follow at once; after
`LOST` the read comes first. A draft equal to the page as read is then a
write too: REVERT, then WRITE..., opens a write page with no change line and
`THE PAGE AS READ, TAGS INCLUDED.` (32 characters, 191 px), and its hold is
live, where AM32 WRITE draws the hold disabled for a page with no change.

## Safety

| Rule | Where | What it guards |
|---|---|---|
| Disarmed, DShot drained | the pin claim, as for AM32 | a throttle frame in the middle of a session; MENU disarms on the way in |
| Propeller off | the README and the manual, as for AM32 | a motor that starts when the firmware starts after LEAVE |
| Guard on every frame | rcbench's `ew_guard_frame()`: no erase or program outside the settings page, none into the bootloader, a page of a written layout with tags, marker and unnamed bytes as read | a write that changes a tag, a write of an older layout, a stray erase |
| Backup before the first write | the ring of `BACKUP/`, from the page as first read in this session ([backups.md](backups.md)); a card that refuses it stops the write, as for AM32 | the old settings and the tags, kept for a reflash's choice or a restore ([question 4](#open-questions)) |
| Re-read before the erase | rcbench's write, step 2 | a write over a page that changed since the read |
| Read-back after the program | rcbench's write, step 4; up to 3 procedures | a write reported done that is not |
| 1000 ms hold | AM32 WRITE's HOLD TO WRITE | a brush of the glass writing |
| BACK disabled while writing | the write page | leaving between erase and program |
| BACK guarded while half written | the list after `TORN` or `LOST` | leaving an erased page behind: the first BACK shows `HALF WRITTEN. BACK AGAIN LEAVES IT;` (35, 209 px) over `A POWER CYCLE THEN LOSES ITS TAGS.` (34, 203 px); the second aborts the session, sends no LEAVE, and goes to the leave page of [question 8](#open-questions) |
| Stop, not abort | the job on a heartbeat lapse | a core0 stall cutting a write between erase and program |
| BOARD voltage red | the write page, as AM32 | a bench brown-out during the write |
| Run cue | MENU's `POWER-CYCLE THE ESC TO RUN IT`, set at the EFM8's answer, cleared by a LEAVE sent, by RESTART DSHOT on the leave page, whose steps power-cycle the ESC, or by an eRPM frame after it ([question 3](#open-questions)) | an ESC left in its bootloader, unseen |
| No DShot into a waiting bootloader | after an EFM8 answered and no LEAVE went, SIG returns to DShot only from a leave page after the user unplugs the ESC's supply ([question 8](#open-questions)) | DShot frames read as commands: the EFM8 bootloader checks only the CRC's low byte (`BLHeliBootLoad.inc` 401 to 406, 177; BLHeli_S's copy 369 to 375, 164), so a garbled 4-byte frame passes about 1 time in 256 for arbitrary bytes, and can be a run, a set address, an erase or a program; only the bootloader's own area is refused (lines 233 to 238). What DShot frames read as is B12. Not verified on an ESC |

What an interrupted write leaves, and the way back:

| Cut | Page | ESC | Screen | Way back |
|---|---|---|---|---|
| Before the erase (the re-read, a failed address) | as read | in its bootloader | `READ FAILED: NOTHING ERASED.` | WRITE again |
| The link, between erase and read-back, the ESC powered | empty or partly programmed | in its bootloader, waiting | `TORN` or `LOST`; BACK guarded | the wire back on, READ AGAIN, then WRITE: the draft, or REVERT and WRITE for the page as read. The session's RAM copy holds the tags, so the write restores them |
| The ESC's power, between erase and program end | as above | at its next start: defaults, tags 0xFF; or, cut inside the program, the marker written and later bytes 0xFF, which the firmware reads without a range check (`Eeprom.asm` 62 to 84) and runs on, tags 0xFF | `LOST` | the next session shows `ESC LOST ITS TAGS: READ-ONLY.`; the tags return with a firmware flash for the ESC's layout on a computer, the backup's byte lines naming the layout tag (bytes 0x40 to 0x4F), or with a restore ([question 4](#open-questions)) |
| The bench (cell, USB cable, reset), after the erase | as above | in its bootloader while powered; DShot frames reach it after the bench restarts | none until reopened: then `SETTINGS PAGE EMPTY` or a refusal | keep the ESC powered; a restore, if built, writes the backup that went to the card before the erase; without one, a computer tool; a power cycle loses the tags |
| A heartbeat lapse | | | none | the write runs to its read-back |

The cue for the user is the same in each case: after a cut write, keep the
ESC powered. The page says so in the result lines and the manual explains
the rest. A bench restart while an EFM8 waits in its bootloader sends DShot
into it whatever the screen does: the bench cannot know.

### The firmware as it is today

The same hazard exists now, before any of this is built. A BLHeli_S or
Bluejay ESC that enters its bootloader while AM32 CONFIG is open and answers
its greeting gets `REPLY NOT UNDERSTOOD:`; `s_blReached`, which sets MENU's
run cue, is set only on `AM32_OK` (`ui_am32.cpp` line 846), so no cue
shows; and BACK resumes the DShot pump at once (`uiAm32Exit()`, lines 783
to 791; `PinUser::Am32` has no leave page, `pin_claim.h` line 88). The ESC
is left in its bootloader with DShot on SIG. Until slice 2 closes this,
unplug such an ESC's battery before BACK and plug it in after; STATUS.md,
the README and the manual record it as a known limitation.

## What is copied

From rcbench `main` at 2b4f934a, unchanged, into `lib/rcbench/` with
rcbench's layout ([lib/rcbench/README.md](../../lib/rcbench/README.md)):

| Files | Built by | Why |
|---|---|---|
| `protocols/escwire/ew_wire.c`, `ew_pio.c`, `ew_a.c`, `ew_text.c`, `ew_guard.c`, `ew_id.c`, `ew_map.c`, `ew_map_s.c`, `ew_map_bj.c`, `ew_map_am.c`, `ew_map_e32.c`, `ew_session.c`, `ew_session_text.c`, `ew_file.c`, `ew_file_read.c`, `ew_file_load.c`, `ew_session_int.h`, `ew_file_int.h`, `include/ew_*.h` (9 headers) | the firmware (`src/escwire.cmake`) and the host suite (`test/escwire.mk`), as C11 | the core: one unit, since `ew_session.c` holds every firmware's paths |
| `protocols/escwire/rp2350/ew_line.c`, `ew_line.h`, `ew_line.pio` | the firmware only, pioasm assembling the program | the pin driver, as KST's and Hitec's line drivers |
| `shared/logfile/servo_store.c`, `include/servo_store.h` | the host suite only | `test_ew_file_load.c` includes it; nothing in `src/` calls it |

Into `test/rcbench/`, unchanged: `test_ew_a.c` (9 cases), `test_ew_id.c`
(9), `test_ew_map.c` (9), `test_ew_guard.c` (15), `test_ew_session.c` (13),
`test_ew_session_write.c` (26), `test_ew_file.c` (28), `test_ew_file_load.c`
(10), `test_ew_pio.c` (28), `test_ew_text.c` (20), `test_ew_session_text.c`
(34), `test_ew_file_e32.c` (9), 210 cases, counted as `TEST_CASE(` lines;
`escwire_headers.cpp`, which compiles the headers as C++11; the simulated
ESC `ew_sim.h` and `ew_sim_text.h`, `ew_pio_model.h`, `ew_file_rig.h`, and
`fixtures/ew_line_words.txt`, the words pioasm makes of `ew_line.pio`, which
CI (continuous integration) holds to pioasm as it does the KST and Hitec
fixtures. `greatest.h` is
already there and identical.

Not copied: rcbench's module `README.md` and `CMakeLists.txt`, which this
repository's `.cmake` and `.mk` files replace; `test_ew_line.c` (18 cases)
and its `fake_pico/`, as for the KST and Hitec line drivers; the panel and
link code of rcbench (`esc_port.c`, `esc_link.c`, the `ESC_WIRE` pages),
which serves rcbench's two-processor bench.

New in `src/`: the reply classification (in `am32_bl` or a small
`esc_detect.h`), `escwire_job.{h,cpp}`, the EFM8 half of `ui_am32.cpp` or a
`ui_efm8.cpp` beside it, and the backup's two ring prefixes in
`backup_ring`. The backup's text is rcbench's (`ew_file_capture()`,
`ew_file_format()`): `ESC settings backup, format 1`, `Firmware:`,
`Layout:`, `Processor: EFM8`, `Signature: E8B2`, one line per field, the
byte lines and a CRC-32 line (rcbench README lines 259 to 290, 283 to 286),
at most `EW_FILE_MAX`, 5280 bytes.

## Tests

### Host

- **The copies:** the 210 rcbench cases, run by `make -C test`.
- **Classification:** AM32 answers of the three families stay AM32; `471d`,
  `E8 B1`, `E8 B2`, `E8 B5`, 6, 1, 0x30 is an EFM8 with that signature; an
  answer with an echo before it; `E8 B3`, a short answer, no 0x30: not
  understood. Mutation: a classification that drops one signature.
- **The connect cycle:** quiet 1000 ms first and not 999 ms; then 3
  greetings and not 4; no greeting in a quiet phase; tries counted only in
  greet phases; across a `millis()` wrap; the AM32 fake's answer in a burst
  opens the AM32 list as today.
- **The job on rcbench's simulated ESC** (`ew_sim.h`, EFM8 kind), through
  the screen by control id: BLHeli_S 33 and Bluejay 208 read, edited, held,
  backed up, written, read back, left; the frames the sim logs against
  rcbench's order; a fault script for each row of the interrupted-write
  table, the screen's lines and its BACK guard; after `TORN`, WRITE of the
  draft and REVERT then WRITE of the page as read, its hold live with no
  change line; a heartbeat lapse in a write ending in a read-back. A
  second contact after the AM32 greeting's answer is not on the sim: it
  answers a first contact once and a second one, as any frame with a bad
  CRC, 0xC2 (`ew_sim.h` lines 234 to 262), and it does not model the
  `00 00` restart. Bench item B4 settles it, or the restart is added to
  rcbench's sim first.
- **Today's limitation closed (slice 2):** an AM32 claim with no EFM8
  answer resumes DShot within `pinClaimEnd()` as today; after one it waits
  for RESTART DSHOT on the leave page. An EFM8 answer sets the run cue, and
  RESTART DSHOT clears it.
- **Views:** each identification row: the strip lines, rows drawn
  `ReadOnly`, WRITE... disabled, the backup saved on opening a read-only
  view and not on the connect page.
- **Texts:** every string of this note within its rectangle by a
  `static_assert` or a fit check, as the other screens have; the previews.
- **Backups:** `BJnnn.TXT` and `BHnnn.TXT` in the ring with KST's, AM32's
  and Hitec's; the text's checksum.
- Every new rule mutation-checked: break it, see a test fail, restore.

### Bench

Only an ESC settles these:

- B1. A Bluejay 0.21 ESC on DShot, ESC CONFIG opened: it answers in the
  first quiet second, `471d E8 B2 06 01 30`; the time from the pump's stop.
- B2. The same with BLHeli_S 16.7, which may not have taken the inverted
  frames before.
- B3. Each power-cycled during a greet phase: the time to the answer; an
  AM32 ESC under the same cycle: how many power-ups it takes, and whether
  an AM32 answers without a second power cycle, and after how long.
- B4. escwire's own first contact after the AM32 greeting's answer.
- B5. The EFM8's answer drives the line open-drain (`BLHeliBootLoad.inc` 76,
  79): its rising edges on the RP2350's pull-up and the EFM8's weak
  pull-up at 52 µs a bit, on a scope.
- B6. Read, edit, write and read-back on Bluejay 0.21 and BLHeli_S 16.7,
  and on an EFM8BB51 Bluejay if one is at hand; DIRECTION reversed, tape on
  the bell, 5 % on the tester; after LEAVE, the melody and, on Bluejay,
  eRPM again without a power cycle.
- B7. The time of a write, and of the stretch from erase to program.
- B8. On a spare ESC: the battery pulled during a write; the next session
  shows lost tags; the ESC runs on defaults; a reflash on a computer brings
  the tags back.
- B9. The signal wire pulled during a write, the ESC powered: `TORN` or
  `LOST`; the wire back; READ AGAIN and WRITE restore the page, tags
  included.
- B10. The bench's USB cable pulled during a write: the ESC stays in its
  bootloader; what the next session reads.
- B11. After LEAVE, the time to the pump's first frame against the ESC's
  start-up and its 15 ms or 150 ms check: LEAVE has no answer, and a line
  idle high for 150 ms after the melody takes Bluejay back to its
  bootloader.
- B12. On a spare ESC in its bootloader, DShot frames for 60 s: the page
  afterwards, and the firmware after a power cycle.
- B13. With the ESC's battery unplugged and the quiet phase running: the
  current into SIG and the ESC's 3.3 V rail; whether a power cycle restarts
  the MCU.

## Work breakdown

Each slice builds, passes the host suite and merges alone, and updates its
own README and wiki lines (the owner's decision 27 of
[menu-b.md](menu-b.md#decisions)). Sizes are new and changed lines, rough.

| Slice | Content | Code | Tests | Docs | Depends on |
|---|---|---|---|---|---|
| 1. Copies | the files of [what is copied](#what-is-copied), `src/escwire.cmake`, `test/escwire.mk`, the fixture check in CI, `lib/rcbench/README.md` | about 40 new | the 210 copied cases | `lib/rcbench/README.md`, STATUS, CHANGELOG | |
| 2. Detection | the classification; an EFM8 answer named on the connect page, `EFM8 E8B2 ANSWERED: NOT READ YET.` (33 characters, 197 px), nothing read; the run cue set by that answer, cleared by RESTART DSHOT or eRPM; a leave page per claim in `pin_claim`, set for `PinUser::Am32` at an EFM8's answer (today `unplug` is fixed per user, `pin_claim.h` line 88), the leave page of question 8, which closes [today's limitation](#the-firmware-as-it-is-today); the connect loop stays as today, so no ESC is drawn into its bootloader more often than now; the MENU label and titles by question 1 | about 200 | about 250 | README "AM32 ESC configuration", wiki in both languages, previews, STATUS | |
| 3. Read | the connect cycle, `escwire_job`, the claim's job, the read, the list read-only for every page, the views, HEX, the strip notes, the 3D note, LEAVE's `00 01` frame | about 650 | about 650 | README section, wiki, previews | 1, 2 |
| 4. Write | WRITE..., ESC WRITE, the backups and their prefixes, the results, the guarded BACK, the heartbeat stop | about 400 | about 500 | as above | 3 |
| 5. Bring-up | B1 to B13 | none | none | the record in STATUS | 1 to 4 |
| 6. Restore | by question 4, after rcbench has one | | | | 4 |

## Open questions

1. **Entry and naming.** (a) AM32 CONFIG's row grows: its MENU label and
   title become `ESC CONFIG` (10 characters, 118 px at scale 2, x 8 to
   125), the sub-views `ESC WRITE` (106 px) and `ESC HEX` (82 px) for every
   firmware, the firmware on the strip; MENU keeps seven rows
   (recommended). This changes the label of menu-b's MENU table and the AM32
   titles of decision 13; with [esc-commands.md](esc-commands.md#open-questions)
   question 1 (a) MENU's first two rows read `ESC COMMANDS` and `ESC
   CONFIG`. (b) The row and title stay `AM32 CONFIG` and show a Bluejay or
   BLHeli_S ESC under that name. (c) A chooser in the row, as PROGRAMMER:
   AM32 and BLUEJAY / BLHELI_S, each with its own connect; one more tap, no
   connect cycle needed. (d) An eighth MENU row; MENU scrolls 78 px. (e)
   With question 1 (c) of [usb-bridge.md](usb-bridge.md#open-questions),
   the row is a chooser; this screen is its first line for every firmware,
   titled `ESC SETTINGS` (12 characters, 142 px at scale 2), beside USB
   BRIDGE, and option (c) here becomes one more chooser line. The owner
   answers the two questions together.
2. **The connect cycle.** (a) 1000 ms quiet at entry, then 5000 ms of
   greetings and 1000 ms quiet in turn: a power-cycled Bluejay answers
   within 6 s, but an AM32 powered up in a greet phase starts its firmware,
   about 5 times in 6, and by AM32's source returns about 2 s later. (b)
   The quiet second at entry only, then greetings as today: a power-cycled
   Bluejay most likely does not answer; an AM32 answers after its
   firmware's reset, as today. (c) No cycle: the user picks the firmware
   first, question 1 (c). (d) 1000 ms quiet on the pull-up, then 3
   greetings, about 125 ms, in turn (recommended: a quiet line keeps all
   three bootloaders waiting, and a powered EFM8 answers in the first
   second; an AM32 power-up meets a burst about 15 % of the time and then
   answers after its firmware's 2 s reset; not measured).
3. **Leaving.** (a) LEAVE sends rcbench's `00 01`, which starts the
   firmware, and the result line reads `LEAVING STARTS THE ESC WITH THEM.`
   (recommended: rcbench does it, the bootloader takes it, and the firmware
   reads its page at every start, `Bluejay.asm` 471). With it, (a1) the
   run cue is cleared when LEAVE went out (recommended; LEAVE has no
   answer, B11), or (a2) only by eRPM, which BLHeli_S never sends, so its
   cue stays until RESTART DSHOT or a bench restart. (b) Nothing, as AM32
   CONFIG today: the result line asks for a power cycle, the run cue stays until
   RESTART DSHOT on question 8's leave page or eRPM, and SIG returns to
   DShot only by question 8.
4. **Repair after a cut write.** (a) None here: lost tags read read-only,
   and the manual explains a reflash on a computer, the backup naming the
   layout tag. (b) An EFM8 restore written in rcbench first, as its AM32
   restore: a verified backup with the same signature, firmware and layout,
   written onto a page whose name tag reads 0xFF or that reads empty, tags
   included; then a slice here (recommended: build (a) now and ask rcbench
   for (b); a bench restart in a write is the likely cut, and only a restore
   brings that ESC back without a computer).
5. **Backup names.** (a) `BJnnn.TXT` for Bluejay and `BHnnn.TXT` for
   BLHeli_S in the ring of `BACKUP/` (recommended: the name says the
   firmware, as `AM` and `HT` do). (b) One prefix, `ES`, for both.
6. **A write without a card.** (a) As AM32: the write runs, the backup stays
   in RAM until a write with a card saves it, and the write page says `NO
   CARD: OLD VALUES LOST ON LEAVING` in amber (recommended: in the session
   the RAM copy is enough to repair a cut write). (b) Refused without a card,
   since only a backup on the card outlives a bench restart; worth taking
   with question 4 (b).
7. **Older layouts.** (a) Bluejay 0.2 to 0.20 and BLHeli_S 16.0 to 16.2
   shown as read-only lists with rcbench's field names, as rcbench does
   (recommended). (b) Only the bytes, as AM32 CONFIG's view of a layout it
   does not know.
8. **Leaving without LEAVE.** After an EFM8 answered and no LEAVE went: the
   BACK guard's second press after `TORN` or `LOST`, BACK on the connect
   page after an answer the screen did not read, question 3 (b). (a) The
   leave page of SERVO TESTER, KST SERVO and HITEC SERVO, its steps
   `UNPLUG THE ESC BATTERY, THEN LEAVE.` (35 characters, 209 px from x 20)
   with `ITS BOOTLOADER WAITS FOR COMMANDS` (33, 197 px) and `AND MAY READ
   DSHOT AS ONE.` (26, 155 px), then `PLUG IT IN AFTER RESTART DSHOT.` (31,
   185 px); RESTART DSHOT acts 1000 ms after LEAVE, as there (recommended:
   DShot into a waiting bootloader can pass as a command, see
   [safety](#safety)). (b) DShot at once, as AM32 CONFIG does today.

## Not verified

- Everything on an ESC: no greeting, read or write of this design, and no
  frame of rcbench's escwire, has reached a Bluejay or BLHeli_S ESC.
- The bootloader entry times, about 320 ms plus 150 ms on Bluejay and the
  15 ms check on BLHeli_S, which are the sources' loop counts and comments;
  whether a Bluejay power-cycled during the greetings returns to its check
  about every 200 ms.
- That a second first contact restarts a bootloader that already answered,
  as the bootloader's source says; rcbench's sim does not model it.
- The rise of the EFM8's open-drain answer on the RP2350 pull-up, 50 to
  80 kΩ, at 52 µs a bit; the ESC's own pull-ups and filters.
- The part's page erase and byte write times, and so the length of the
  window in which a cut loses the tags.
- Which bootloader AM32 ESCs in the field run, and whether the bench's
  pull-up holds the line high against the AM32 pull-down.
- AM32 firmware's self-reset with the greetings on the line: whether it
  reads them as a signal.
- The current into SIG, and into an unplugged ESC, while the line rests on
  the pull-up (B13).
- What a waiting EFM8 bootloader does with DShot frames (B12).
- What BLHeli_S 16.7 does with inverted DShot; what the builds with sub
  revision 8 or 9 that rcbench refuses are.
- `ew_line.c` built against SDK 2.1.1: the calls exist by name; nothing has
  been compiled.
- Whether a desktop configurator offers narrower ranges than rcbench's maps
  for any field.
