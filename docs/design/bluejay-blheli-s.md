# Bluejay and BLHeli_S settings

Status: designed; the owner answered its questions on 2026-11-15, see
[decisions](#decisions). Slices 1 to 4 are built
([slice 1 as built](#slice-1-as-built), [slice 2 as built](#slice-2-as-built),
[slice 3 as built](#slice-3-as-built), [slice 4 as built](#slice-4-as-built)):
slice 1's copies of rcbench's escwire and their tests, slice 2's
detection, slice 3's read and slice 4's write, slices 2 to 4 tested on the
host only; slices 5 and 6 wait on being built. The
feature is item (6) of the owner's
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
| BLHeli_S (`bitdump/BLHeli`), revision 16.7 (`BLHeli_S SiLabs/BLHeli_S.asm` lines 534 to 536) | a5bcf99 | `BLHeli_S SiLabs/BLHeli_S.asm` lines 44 to 63, 314 to 330, 529 to 588, 3563, 3586 to 3607; `BLHeliPgm.inc` lines 40 to 87, 96 to 136, 194 to 202, 230 to 262; `A.inc` lines 50 to 104, 119 to 124; `A_X.inc` lines 91, 92; `BLHeliBootLoad.inc` lines 10, 12, 13, 76, 163, 164, 369 to 375, 390 to 394 | GPL-3.0; read, nothing copied |
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
same ESC (decision 4).

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

The AM32 CONFIG screen grows into `ESC SETTINGS`; there is no second
settings screen. Its connect page sends the greeting it sends today, and
the answer decides which editor opens:

- An AM32 family byte: the AM32 editor as today, on `am32_bl` on core0.
- `471` with signature `E8B1`, `E8B2` or `E8B5`: the screen ends `am32_bl`
  (`am32BlEnd()`) and hands the ESC to an rcbench escwire session on
  core1, which makes its own first contact, reads, identifies and opens
  the Bluejay or BLHeli_S list.
- Anything else: `REPLY NOT UNDERSTOOD:` as today.

The classification moves out of `am32BlHandshake()` into a pure function
on the reply's bytes, tested on the host; the bit-banged link stays as it
is. MENU's row 2 is the chooser `ESC CONFIG` (decision 1), whose first entry,
`ESC SETTINGS` (12 characters, 142 px at scale 2), is this screen, for every
firmware, and whose second is `USB BRIDGE`
([usb-bridge.md](usb-bridge.md#entry)). The chooser claims nothing; an
entry's tap claims SIG. The firmware shows on the strip.

Two design calls, the developer's under the owner's MENU decision,
2026-11-15. First, the sub-view titles under `ESC SETTINGS` are one set
for every firmware, `ESC WRITE` (106 px at scale 2) and `ESC HEX` (82 px),
each fitting beside BACK at x 170, with the firmware named on the strip.
AM32's `AM32 WRITE` and `AM32 HEX` (menu-b.md decision 13) become `ESC
WRITE` and `ESC HEX` when `ESC SETTINGS` is built, and the EFM8 hex view
is `ESC HEX` too. Second, the `ESC CONFIG` chooser comes with whichever of
this note's slice 2 and slice 2 of usb-bridge.md merges first, and holds
both entries from the start. The entry not yet built is drawn disabled
with `NOT BUILT` under it. `ESC SETTINGS` serves
AM32 only until the Bluejay slices land, and STATUS.md says so.

A screen per firmware would make the user know which firmware the ESC runs
before the first byte. The `ESC SETTINGS` entry needs no such choice: the
greeting is the same, the bootloader's answer names the part. The cost is the
connect cycle below, which serves both firmwares at once.

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
phases, by decision 2:

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
  makes exists by name in the 2.1.1 headers (checked with `grep`), and
  `ew_line.c` compiles against them ([slice 1 as built](#slice-1-as-built)).
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
  parked, and the claim gives SIG back to DShot (decision 3).
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
| `EW_SES_OK` | `WRITE VERIFIED. BACKUP BJ018` (28, 167 px), green | `LEAVING STARTS THE ESC WITH THEM.` (33, 197 px); for AM32 the line stays `POWER-CYCLE THE ESC TO APPLY.` |
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
| Backup before the first write | the ring of `BACKUP/`, from the page as first read in this session ([backups.md](backups.md)); a card that refuses it stops the write, as for AM32 | the old settings and the tags, kept for a reflash's choice or a restore (decision 4) |
| Re-read before the erase | rcbench's write, step 2 | a write over a page that changed since the read |
| Read-back after the program | rcbench's write, step 4; up to 3 procedures | a write reported done that is not |
| 1000 ms hold | AM32 WRITE's HOLD TO WRITE | a brush of the glass writing |
| BACK disabled while writing | the write page | leaving between erase and program |
| BACK guarded while half written | the list after `TORN` or `LOST` | leaving an erased page behind: the first BACK shows `HALF WRITTEN. BACK AGAIN LEAVES IT;` (35, 209 px) over `A POWER CYCLE THEN LOSES ITS TAGS.` (34, 203 px); the second aborts the session, sends no LEAVE, and goes to the leave page of decision 8 |
| Stop, not abort | the job on a heartbeat lapse | a core0 stall cutting a write between erase and program |
| BOARD voltage red | the write page, as AM32 | a bench brown-out during the write |
| Run cue | MENU's `POWER-CYCLE THE ESC TO RUN IT`, set at the EFM8's answer, cleared by a LEAVE sent, by RESTART DSHOT on the leave page, whose steps power-cycle the ESC, or by an eRPM frame after it (decision 3) | an ESC left in its bootloader, unseen |
| No DShot into a waiting bootloader | after an EFM8 answered and no LEAVE went, SIG returns to DShot only from a leave page after the user unplugs the ESC's supply (decision 8) | DShot frames read as commands: the EFM8 bootloader checks only the CRC's low byte (`BLHeliBootLoad.inc` 401 to 406, 177; BLHeli_S's copy 369 to 375, 164), so a garbled 4-byte frame passes about 1 time in 256 for arbitrary bytes, and can be a run, a set address, an erase or a program; only the bootloader's own area is refused (lines 233 to 238). What DShot frames read as is B12. Not verified on an ESC |

What an interrupted write leaves, and the way back:

| Cut | Page | ESC | Screen | Way back |
|---|---|---|---|---|
| Before the erase (the re-read, a failed address) | as read | in its bootloader | `READ FAILED: NOTHING ERASED.` | WRITE again |
| The link, between erase and read-back, the ESC powered | empty or partly programmed | in its bootloader, waiting | `TORN` or `LOST`; BACK guarded | while the session goes on: the wire back on, READ AGAIN, then WRITE: the draft, or REVERT and WRITE for the page as read. The session's RAM copy holds the tags, so the write restores them. A driver failure, a lapse or the guard's second BACK ends the session: as the lapse after the erase, below |
| The ESC's power, between erase and program end | as above | at its next start: defaults, tags 0xFF; or, cut inside the program, the marker written and later bytes 0xFF, which the firmware reads without a range check (`Eeprom.asm` 62 to 84) and runs on, tags 0xFF | `LOST` | the next session shows `ESC LOST ITS TAGS: READ-ONLY.`; the tags return with a firmware flash for the ESC's layout on a computer, the backup's byte lines naming the layout tag (bytes 0x40 to 0x4F), or with a restore (decision 4) |
| The bench (cell, USB cable, reset), after the erase | as above | in its bootloader while powered; DShot frames reach it after the bench restarts | none until reopened: then `SETTINGS PAGE EMPTY` or a refusal | keep the ESC powered; a restore, if built, writes the backup that went to the card before the erase; without one, a computer tool; a power cycle loses the tags |
| A heartbeat lapse, before the erase | as read | in its bootloader; the session ended, the line parked | `STOPPED BEFORE THE ERASE: AS READ.` over `THE SESSION ENDED. TAP OK TO SEARCH.` | OK greets again; the next session reads the page as it was |
| A heartbeat lapse, after the erase | as written once the read-back proves it, else empty or partly programmed | in its bootloader; the write ran to its read-back, then the session ended, the line parked | the result over `THE SESSION ENDED. TAP OK TO SEARCH.`, or, half written, over `SESSION ENDED: RESTORE ON A COMPUTER.` | written: OK greets again, and LEAVE goes from the next session's list. Half written: a later session reads the page as it is, mostly empty or with lost tags, which rcbench writes neither of; a page whose marker and tags survived reads as writable, and a write there starts from the torn page, not the old one; keep the ESC powered and restore it on a computer, from the backup on the card or the old page on `OLD PAGE` while ESC SETTINGS stays open |

The cue for the user is the same in each case: after a cut write, keep the
ESC powered. The page says so in the result lines and the manual explains
the rest. A bench restart while an EFM8 waits in its bootloader sends DShot
into it whatever the screen does: the bench cannot know.

### The firmware as it is today

With slice 4 built, a BLHeli_S or Bluejay ESC that enters its bootloader
while ESC SETTINGS is open and answers its greeting is read through the
escwire job; Bluejay 208 and BLHeli_S 33 are edited and written, the other
pages shown read-only ([slice 4 as built](#slice-4-as-built)). The answer
sets MENU's run cue and
gives the claim the leave page (`pinClaimWantLeavePage()`). BACK from the
list sends LEAVE, `00 01`, and DShot returns once it went out; every other
BACK after the answer opens the leave page of decision 8, and DShot returns
to SIG only from its RESTART DSHOT, after its steps have told the user to
unplug the ESC's battery. The bench cannot see the battery: RESTART DSHOT
with the ESC still powered, or a bench restart while it waits, sends DShot
into the bootloader. LEAVE has no answer, so a LEAVE the ESC did not take
leaves it waiting when DShot returns (B11); MENU's run cue stays until an
eRPM frame after LEAVE. STATUS.md records both as known limitations.

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

- B1. A Bluejay 0.21 ESC on DShot, `ESC SETTINGS` opened: it answers in the
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
| 1. Copies, built | the files of [what is copied](#what-is-copied), `src/escwire.cmake`, `test/escwire.mk`, the fixture check in CI, `lib/rcbench/README.md` | about 40 new | the 210 copied cases | `lib/rcbench/README.md`, STATUS, CHANGELOG | |
| 2. Detection | the classification; an EFM8 answer named on the connect page, `EFM8 E8B2 ANSWERED: NOT READ YET.` (33 characters, 197 px), nothing read; the run cue set by that answer, cleared by RESTART DSHOT or eRPM; a leave page per claim in `pin_claim`, set for `PinUser::Am32` at an EFM8's answer (today `unplug` is fixed per user, `pin_claim.h` line 88), the leave page of decision 8, which closes [today's limitation](#the-firmware-as-it-is-today); the connect loop stays as today, so no ESC is drawn into its bootloader more often than now; the chooser `ESC CONFIG` and the title `ESC SETTINGS` by decision 1 | about 200 | about 250 | README "AM32 ESC configuration", wiki in both languages, previews, STATUS | |
| 3. Read | the connect cycle, `escwire_job`, the claim's job, the read, the list read-only for every page, the views, HEX, the strip notes, the 3D note, LEAVE's `00 01` frame | about 650 | about 650 | README section, wiki, previews | 1, 2 |
| 4. Write | WRITE..., ESC WRITE, the backups and their prefixes, the results, the guarded BACK, the heartbeat stop | about 400 | about 500 | as above | 3 |
| 5. Bring-up | B1 to B13 | none | none | the record in STATUS | 1 to 4 |
| 6. Restore | by decision 4, after rcbench has one | | | | 4 |

### Slice 1 as built

The files of [what is copied](#what-is-copied), byte for byte those of
rcbench `main` at 2b4f934a (2b4f934ad6b6d67f20b46d5ba80001c091e14b89), 51
files compared with `cmp`, `greatest.h` among them; rcbench's `main` had
not changed any of them on 2026-11-15. Into `lib/rcbench/`: the 16 core
sources, `ew_session_int.h`, `ew_file_int.h`, the 9 headers of `include/`,
`rp2350/ew_line.c`, `ew_line.h` and `ew_line.pio`, and
`shared/logfile/servo_store.c` with its header. Into `test/rcbench/`: the
12 tests, `escwire_headers.cpp`, `ew_sim.h`, `ew_sim_text.h`,
`ew_pio_model.h`, `ew_file_rig.h` and `fixtures/ew_line_words.txt`.
[lib/rcbench/README.md](../../lib/rcbench/README.md) lists them with the
commit.

- **Firmware.** `src/escwire.cmake`, included last by `CMakeLists.txt`,
  builds the 16 core sources and `ew_line.c`, assembles `ew_line.pio` with
  pioasm and links `hardware_pio`, `hardware_gpio`, `hardware_dma`,
  `hardware_clocks` and `hardware_timer`, as rcbench's
  `rp2350/CMakeLists.txt` does. With arm-none-eabi-gcc 13.3, SDK 2.1.1 and
  `STRICT=ON` (`-std=gnu11 -Wall -Wextra -Wshadow -Werror`) all 17 compile
  unchanged. Nothing calls them, and the image holds no `ew_` symbol
  (`arm-none-eabi-nm`): the linker drops them. The servo file store is not
  in the firmware.
- **Host suite.** `test/escwire.mk` lists the 16 core sources and
  `servo_store.c` as the feature's C sources, built with `-std=c11 -Wall
  -Wextra -Wshadow -Werror` as the KST and Hitec copies are and linked into
  each C test program and into `dshot_tests`; the 12 tests as C test
  programs, 210 cases, which `make run` runs; `escwire_headers.cpp` as a
  C++11 program; and the two paths `test_ew_pio.c` reads, `EW_LINE_PIO` and
  `EW_LINE_WORDS`, as rcbench's `test/host/CMakeLists.txt` sets them. Every
  file compiles unchanged under these flags. `test/Makefile` adds the
  feature to its `FEATURE_*` lists.
- **CI.** The step that holds the line fixtures to pioasm's words takes
  `ew_line_words.txt` with the KST and Hitec fixtures; its loop names each
  directory and line, since `ew_line.pio` is in `protocols/escwire/`. The
  25 words match pioasm's output of SDK 2.1.1. The "Config permutations"
  job has the escwire `include/` and `rp2350/` folders on its include path.
- **Checks of the wiring.** A changed fixture word fails 1 case of
  `test_ew_pio.c`; a CRC in `ew_a.c` off by one bit fails 3 cases of
  `test_ew_a.c` and 12 of `test_ew_session.c`. Both restored.
- The coverage figure counts `src/` only; the copies are instrumented and
  do not count, so it is unchanged.
### Slice 2 as built

Files: `src/bl_answer.{h,cpp}`, the classification, pure;
`am32BlAnswer()` and the classification in `am32BlHandshake()`
(`src/am32_bl.*`); `pinClaimWantLeavePage()` and `pinClaimLeavePage()`
(`src/pin_claim.*`), the leave page per claim; the EFM8 answer, its leave
page and the titles in `src/ui_am32.*`; `src/ui_esc_config.{h,cpp}`, the
`ESC CONFIG` chooser, `UiScreenId::EscConfig`, its entries
`UI_ESC_CFG_ENTRIES` and their fit check in `src/ui_registry.cpp`; MENU's
row 2; SD LOG's stop line. Host tests in `test/test_am32.cpp`
(`testBlAnswer()`), `test/test_claim.cpp` (`testLeavePagePerClaim()`),
`test/test_ui.cpp` (`testAm32Efm8()`), `test/test_menu.cpp`
(`testEscConfig()`) and `test/test_ids.cpp`; the screenshots
`shot_esc_chooser`, `shot_am32_efm8` and `shot_am32_leave`.

Texts, at scale 1 unless named, (6n − 1) px for n characters:

| Where | Text | Width |
|---|---|---|
| MENU, row 2, scale 2 | `ESC CONFIG` | 10, 118 px |
| Chooser title, scale 2 | `ESC CONFIG` | 10, 118 px |
| Chooser strip, `C_DIM` | `SIG GP0` | 7, 41 px |
| Entry 1, scale 2 | `ESC SETTINGS` | 12, 142 px |
| Its lines, `C_DIM`, y 112 and 124 | `AM32 SETTINGS, READ AND WRITTEN.` / `BLUEJAY, BLHELI_S: NAMED, NOT READ.` | 32, 191 px; 35, 209 px |
| Entry 2, scale 2, drawn disabled | `USB BRIDGE` | 10, 118 px |
| Its lines, `C_DIM`, y 192 and 204 | `A DESKTOP ESC TOOL OVER USB.` / `NOT BUILT` | 28, 167 px; 9, 53 px |
| Titles, scale 2 | `ESC SETTINGS`, `ESC HEX`, `ESC WRITE` | 142, 82, 106 px, each ending 4 px or more before BACK |
| List strip | `NEOESC f051  F051  AM32 2.16`; without a name `F051  AM32 2.16  LAYOUT 2` | 28, 167 px; 25, 149 px |
| Connect page, y 176, amber, and the strip | `EFM8 E8B2 ANSWERED: NOT READ YET.` | 33, 197 px |
| Leave page strip, `C_DIM` | `EFM8 E8B2  SIG GP0` | 18, 107 px |
| Leave step 1, from x 20 | `UNPLUG THE ESC BATTERY, THEN LEAVE.` / `ITS BOOTLOADER WAITS FOR COMMANDS` / `AND MAY READ DSHOT AS ONE.` | 35, 209 px, to x 228; 33, 197 px; 26, 155 px |
| Leave step 2 | `PLUG IT IN AFTER RESTART DSHOT.` | 31, 185 px |
| SD LOG, a stop by the claim | `RECORDING STOPPED BY ESC SETTINGS` | 33, 197 px |

Choices the note left open, the developer's:

- **The classification's rule.** An AM32 family at the fifth byte from the
  end is AM32 whatever the bytes before it, as `am32BlHandshake()` has always
  taken it, so no AM32 answer changes meaning. An EFM8 answer is 9 bytes or
  more ending in `471`, any byte, 0xE8, 0xB1, 0xB2 or 0xB5, any two bytes and
  0x30: rcbench's first contact takes 9 bytes starting `471` (`ew_a.c` lines
  165 to 177, `protocols/escwire` at f4ba5321) and reads the signature from
  bytes 4 and 5 (`ew_id.c` lines 53 to 75). The fourth byte, `d` in
  Bluejay's answer, the bootloader version and the page count are not
  checked. BLHeli_S's copy of the bootloader sends version 6 and 1 page, as
  Bluejay's does (`BLHeliBootLoad.inc` lines 12 and 13), after the signature
  E8B1 or E8B2 by `MCU_48MHZ` (`A.inc` lines 119 to 124) or E8B5 (`A_X.inc`
  lines 91 and 92).
- **The handshake's result.** `am32BlHandshake()` still returns
  `AM32_ERR_NAK` for an EFM8 answer and `AM32_ERR_TIMEOUT` for silence;
  `am32BlAnswer()` returns the classification, so `Am32Result` and its texts
  keep their widths.
- **The connect loop after an EFM8 answer** greets as it greets after any
  other answer it cannot use, by "the connect loop stays as today" of the
  work breakdown. A later greeting reads to the waiting bootloader as a
  restart and a new greeting ([the greeting already
  fits](#the-greeting-already-fits)); not verified on an ESC. The EFM8 line
  stays through later silence, since the bootloader waits with no timeout; a
  later answer of another kind replaces it. The rest of the connect page,
  `POWER-CYCLE THE ESC` and its three lines, is AM32's and unchanged.
- **The leave page opens before LEAVE**, with STAY and LEAVE, as decision 8's
  first step ends `THEN LEAVE.`: the transport keeps SIG until LEAVE, which
  calls `am32BlEnd()` and `pinClaimEnd()`. STAY returns to the view BACK
  left, the claim kept, and the connect page greets again. After LEAVE there
  is no left button: there is no next ESC to keep the claim for. RESTART
  DSHOT is live 1000 ms after SIG is let go, which for an AM32 claim, with no
  job, is the frame after LEAVE. The note `RELEASING THE PIN` stands for that
  one frame. The strip names the signature and SIG.
- **Which BACK opens it.** Any BACK that would leave the screen once an EFM8
  bootloader answered in the session: from the connect page, the list after
  a later AM32 answer, its guard's second BACK included, and the read-only
  view. The guard is disarmed when it opens, so STAY returns to the list
  with the edits kept and the guard disarmed. BACK from ESC HEX and ESC WRITE
  goes to the list first.
- **The leave page per claim.** `PinUserInfo::unplug` stays the default,
  copied into the claim at `pinClaimBegin()`. `pinClaimWantLeavePage()` acts
  only for the holder and only while Stopping, Held or Running, so it cannot
  hold back a claim already leaving; `pinClaimLeavePage()` reads false while
  Free. A claim still Stopping has driven nothing on SIG and resumes at once.
- **The run cue** is set at each EFM8 answer and stamped then, so an eRPM
  frame after the last answer clears it as after an AM32 handshake. In this
  slice the only way from ESC SETTINGS to MENU after an EFM8 answer is
  RESTART DSHOT, which clears it, so MENU does not show it for an EFM8
  answer; it matters once slice 3's LEAVE exists.
- **Call A here.** The sub-view titles follow the title `ESC SETTINGS`,
  which this slice builds: `ESC WRITE` and `ESC HEX`, not `AM32 WRITE` and
  `AM32 HEX`. The firmware's name is in the list's strip: `AM32 2.16`, not
  `FW 2.16`. `test/check_docs.py` rule 9
  refuses the old titles and `FW` in README.md and the wiki.
- **The chooser's code.** A file of its own, `ui_esc_config.*`, as RX
  SIGNALS' `ui_rx_signals.*`, with an entry type of a label, two lines and a
  screen; a null screen draws the entry disabled. `USB BRIDGE`'s line says
  what it is for, then `NOT BUILT`. ESC SETTINGS'
  lines say what it does today. The strip names SIG alone: the chooser's
  tools use SIG only.

Left for slice 3, found in review:

- The connect page after an EFM8 answer still shows AM32's power-cycle
  lines, `POWER-CYCLE THE ESC` and the three under it, which do not apply to
  a bootloader that waits.
- An EFM8 answer captured with a trailing glitch byte, or with a byte of it
  garbled, is not caught: the rule reads the last 9 bytes only, so the
  connect page reads `REPLY NOT UNDERSTOOD:` and no leave page opens, as
  before this slice. Whether to match more loosely, such as `471` with 0xE8
  four bytes later anywhere in the reply, is a question for slice 3.

Mutations, each applied to the tree, the suite built and run, then restored: 39,
38 failing the suite: the classification dropping each of B1, B2 and B5, the
`471` check, its `7`, the 0xE8 check, the 0x30 check, the 9-byte floor, the
signature's low byte; the claim's leave page per user again, set by any screen,
set in any state, not taken from the user's row at `pinClaimBegin()`, reported
while Free; no leave page on an EFM8 answer, no run cue on it, RESTART DSHOT
keeping the cue, BACK leaving at once after an EFM8 answer, the leave page
opened from the connect page only, not from the read-only view, not from the
list, the EFM8 line dropped on silence,
BACK live on the leave page, LEAVE keeping the transport, STAY going to the
list, STAY going to the connect page, the page opened releasing, RESTART DSHOT
without its wait, the steps swapped, an EFM8 answer taken as silence, the hex
title `AM32 HEX`, the strip's `FW`; every chooser entry acting, the chooser not
`freePins`, USB BRIDGE without `NOT BUILT`, the entries' controls swapped,
MENU's row back to `AM32 CONFIG`, SD LOG's old stop line. The one that
passes is equivalent: `openLeave()` without its reset of the guard. The list
reaches the leave page only through the guard's BACK, which disarms the guard
itself (`uiBackGuard()`), and the guard arms nowhere but on the list, so the
reset never finds it armed; it stays as a backstop. The script is not kept.

Not verified: everything on an ESC. No Bluejay or BLHeli_S ESC has
answered this board's greeting; the answer's bytes are the bootloader
source's ([the greeting already fits](#the-greeting-already-fits)); what a
waiting bootloader does with the greetings that follow is the source's;
whether the user's unplugging reaches the ESC before RESTART DSHOT is the
user's.

### Slice 3 as built

Files: `src/greet_cycle.{h,cpp}`, the connect cycle, pure, its two numbers
`ESC_GREET_QUIET_MS` and `ESC_GREET_BURST` in `src/config.h`;
`src/bl_answer.*`, the classification's EFM8 rule loosened;
`src/escwire_job.{h,cpp}`, the job, `Core1Job::EscWire` in
`src/core1_job.*`; `src/escwire_port.{h,cpp}`, its line on `ew_line`;
`src/efm8_view.{h,cpp}`, the rows and texts of a page, pure; the screen's
states `S_EW`, `S_EW_LIST`, `S_EW_EXIT` and `S_REGREET` in
`src/ui_am32.*`; `PinUser::Am32`'s row in `src/pin_claim.h`,
`Core1Job::EscWire` with `deferJob`; ESC SETTINGS' second chooser line in
`src/ui_registry.cpp`; `src/escwire.cmake` builds the three new sources
and the job's call graph. Host tests in `test/test_am32.cpp`
(`testBlAnswer()`, `testGreetCycle()`), `test/test_escwire_job.cpp`,
`test/test_efm8_view.cpp`, `test/test_ui.cpp` (`testAm32Efm8()`, run with
core1 once a millisecond in every frame), `test/test_claim.cpp`,
`test/test_menu.cpp` and `test/test_ids.cpp`, the job and the screen
against rcbench's simulated ESC behind `test/fake_escwire_port.h`. CI's
"ESC settings job stack on core1" holds the job's deepest chain to
1,792 bytes, which leaves 256 bytes of core1's 2 KB for `loop1()` and
`core1JobPoll()` above it, as for the Hitec job. The chain is 408 bytes: 296
from `escwireJobStep()` through `ew_session_step()` to `ew_text_show()`,
and 112 for the line driver's functions, which the session calls through
the pointers of `ew_driver_t` and the call graph has no edge for; the step
adds the deepest of them, `ew_line_start()`, to every chain
(`tools/stack_depth.py --indirect`). SDK and library calls count 0 bytes.
The screenshots `shot_am32_quiet`,
`shot_am32_efm8`, `shot_efm8_list`, `shot_efm8_note`, `shot_efm8_hex`,
`shot_efm8_3d`, `shot_efm8_readonly`, `shot_efm8_empty`,
`shot_efm8_refused`, `shot_efm8_leaving`, `shot_efm8_stopped` and
`shot_am32_leave`.

Texts, at scale 1 unless named, (6n − 1) px for n characters:

| Where | Text | Width |
|---|---|---|
| Connect page, y 176, quiet phase | `LINE HIGH, NOTHING SENT` | 23, 137 px |
| Connect page and strip, an EFM8 answer | `EFM8 E8B2 ANSWERED: READING.` | 28, 167 px |
| Connect page while the job holds SIG, y 84, scale 2, `C_TEXT` | `EFM8 BOOTLOADER` | 15, 178 px |
| Under it, y 116, 130, 144, `C_DIM` | `THE ESC WAITS IN ITS BOOTLOADER` / `FOR COMMANDS. ITS FIRMWARE DOES` / `NOT RUN UNTIL IT LEAVES.` | 31, 31, 24 |
| A session that stopped, y 196 and 210, amber | `THE ESC ANSWERED, THEN STOPPED.` / `SEARCHING AGAIN.` | 31, 185 px; 16, 95 px |
| The list's bar, `C_GRID` | `SHOWN ONLY: WRITING IS NOT BUILT.` / `BACK SENDS LEAVE: THE ESC STARTS.` | 33, 197 px each |
| A read-only list's second strip line | `SHOWN ONLY. NO BACKUP SAVED.`; lost tags `REFLASH ON A COMPUTER. NO BACKUP.` | 28, 167 px; 33, 197 px |
| The page while LEAVE goes out, y 84, scale 2 | `LEAVING` | 7, 82 px |
| Under it | `THE ESC STARTS ITS FIRMWARE ON` / `LEAVE, THEN DSHOT RETURNS.` / `KEEP THE PROPELLER OFF.`, the last amber | 30, 26, 23 |
| Its status | `SENDING LEAVE: 00 01`, then `LEAVE SENT.` | 20, 11 |
| A part not known | `EFM8 E8B3: PART NOT KNOWN` | 25, 149 px |
| Chooser, ESC SETTINGS' second line | `BLUEJAY, BLHELI_S: READ, NOT WRITTEN.` | 37, 221 px |

The other texts are this note's ([what each page shows](#what-each-page-shows),
[the list](#the-list-the-editor-and-the-write-page)); `test/test_efm8_view.cpp`
and `static_assert`s in `src/ui_am32.cpp` hold each to its width.

Choices the note left open, the developer's:

- **The looser EFM8 match** (left from slice 2's review). An EFM8 answer is
  `471` with 0xE8 four bytes after its `4`, anywhere in the reply, the last
  such `471` counting; the bytes after the 0xE8, the 0x30 among them, are
  not checked, and the signature is 0xE8 and the byte after it, 0xE800 when
  the reply ends there. An AM32 answer is read first, so no AM32
  answer changes meaning; an AM32 answer with a glitch byte stays not
  understood, as its family is never 0xE8. A false EFM8 costs a leave page
  and an escwire entry that ends silent, after which the cycle greets
  again; a missed one costs DShot into a waiting bootloader. The session's
  own first contact reads the answer again: `ew_a_contact_reply()` checks
  its 9 bytes and its `471`, `ew_id_contact()` its signature.
- **The connect page after an EFM8 answer** (left from slice 2's review).
  While the job holds SIG, `POWER-CYCLE THE ESC` and its three lines give
  way to `EFM8 BOOTLOADER` and three lines that say the bootloader waits.
  Once a session stopped and the cycle greets again, the power-cycle lines
  return: the screen is searching again.
- **The connect cycle.** One greeting a frame while the phase is Greet; the
  quiet phase begins when the third greeting's wait ends. The status shows
  the phase, `LINE HIGH, NOTHING SENT` or `WAITING... (TRY n)`, only after
  silence: an answer's status, `READ FAILED`, the bytes of a reply not
  understood, stays through the quiet phase until the next answer. The
  line rests on the pull-up the AM32 transport leaves between greetings
  (decision 2).
- **The job's commands** are Enter, Read, Leave and Stop; there is no write
  command in this slice. The bind opens `ew_line`, which parks the pad on
  its pull-down, and calls `ew_session_hold()` in the same call, so the
  line is driven high at once; the gap after `am32BlEnd()` is the one core1
  pass before the bind. Enter holds 500 ms more, the shortest hold rcbench
  takes. A lapse of the heartbeat stops a running entry or read with
  `ew_session_stop()`, and lets a running LEAVE finish, its frame having
  gone; the release does the same, then closes the line and puts the
  pull-up on, as DShot's teardown does.
- **Which BACK sends LEAVE.** BACK from a list, the read-only lists among
  them: the page was read, and starting the firmware is what LEAVE does.
  A page not taken, an empty one among them, gets no LEAVE: what its
  firmware does at its start is not known, and an empty page has no marker,
  so the firmware would store its defaults with the tags 0xFF
  ([why an interrupted write loses the
  tags](#why-an-interrupted-write-loses-the-tags)). BACK there opens the leave
  page of decision 8.
- **DShot after LEAVE.** Once the job reports LEAVE went out, the screen
  ends the claim; the claim keeps its leave page flag, so it waits in
  Leaving until the job is idle, and the screen then calls
  `pinClaimRestart()` itself, without the leave page's 1000 ms or steps.
  The run cue stays: LEAVE has no answer (B11), so an ESC that missed
  `00 01` still waits when DShot returns, and only an eRPM frame after
  LEAVE clears the cue. Decision 3 (a1) reads "the run cue is cleared when
  LEAVE went out"; this build keeps it, and the owner may restore (a1).
  `src/pin_claim.*` gains no function. A LEAVE refused for the heartbeat is
  asked again; one that ends without going out opens the leave
  page.
- **A session that ends without the page**, silent, another answer, a
  processor other than an EFM8, a read that failed or a stop: the screen
  ends the claim, waits for the job to be idle, and calls `pinClaimKeep()`,
  which returns the claim to Held with no job and its leave page kept; the
  AM32 transport takes SIG and the cycle greets again.
- **No backup in this slice.** The backups and their prefixes are slice 4,
  so a read-only list saves none and its second strip line says so,
  `SHOWN ONLY. NO BACKUP SAVED.`, in place of the `BACKUP BJ018` of
  [what each page shows](#what-each-page-shows).
- **The rows.** DITHERING, which the note's grouping does not name, is a
  MOTOR row; POWER SLOPE and Bluejay's STARTUP POWER are STARTUP rows. Every
  row is drawn `ReadOnly`, a value none of the field's `Odd`, `RAW 17` in
  amber. A tap selects a row for its note; a swipe changes nothing.
- **The strip's order** on a list: the selected row's note, then why the
  list is read-only, then the 3D note, then the caption.
- **The 3D note.** A read that opens a list sets it from DIRECTION; a page
  whose marker is wrong leaves it, since its firmware loads its defaults at
  its next start.
- **A bind refused** puts the pull-up back on SIG, which `am32BlEnd()` left
  floating, and reads `SIG GP0 COULD NOT BE TAKEN (DMA)`, `(PIO)`,
  `(CLOCK)` or, for a pin the driver does not take, `(PIN)`
  (`CORE1_JOB_PIN`), on the connect page with no RETRY; BACK opens the
  leave page.
- **A piece of an EFM8 answer.** A reply not understood that holds `471`,
  or `71`, a byte and 0xE8, or 0xE8 followed by 0xB1, 0xB2 or 0xB5
  (`blAnswerEfm8Fragment()`, `BlAnswer::fragment`), as an EFM8 answer with
  its `d` or its leading `4` lost does, gives the claim the leave page and
  MENU the run cue; no job starts, and the cycle greets on. The leave
  page's strip reads `EFM8  SIG GP0`, naming no part. A reply with none of
  the pieces keeps the leave page off, so noise does not ask for an unplug;
  a reply classified AM32 is never a piece, but an AM32 answer with a
  glitch byte holds `471` and is one. A false hit costs a leave page.

Mutations, each applied to a copy of the tree, the suite built and run: 70,
65 failing the suite. Four pass and are equivalent: Read taken outside
InMode and Enter taken twice without the job's own state check, which the
session refuses the same way (`ew_session_read()`, `ew_session_enter()`); a
refusal in `started()` that changes the last operation, since every refusal
the session makes in this slice the job's state check makes first; and a
status published on every pass, which only holds the spin lock longer. One
applied to the burst's own check in `greetCyclePhase()`, which the quiet
phase's start already makes, and the code has no such check. The others
fail the suite: the loose rule's 0xE8, `7`, last `471`, the 0x30 required
and the signature's low byte; each piece of an EFM8 answer dropped, an
0xE8 piece taking any byte, the piece never flagged, an AM32 answer
flagged; the cycle's 1000 ms, 3 greetings and the quiet phase from the
third; the bind's hold, the heartbeat's stop, a leave cut by a lapse or by
the release, the release's close, commands under a lapse, a Stop not held,
the bind errors, a pin the driver lacks named PIO, no pull-up on a refused
bind, a pull-up on a pin the chip lacks, an operation ended at once taken as
refused, a LEAVE counted as left whatever its result, an operation's end or
a command not published; the revision bytes listed, the order by group,
STARTUP POWER's group, the 3D words, the capitals, 3D at 4, the lost tags'
lines, the empty page's 112 bytes, LOW-SPEED PROTECTION's note, the written
layouts, a version too long; the leave page and the transport's end at an
EFM8 answer, a piece ignored, a piece without the run cue, BACK keyed to a
signature alone, the leave page's strip naming E800, BACK from the list
sending LEAVE, DShot without LEAVE, the run cue cleared at LEAVE, a refused
LEAVE not asked again, a read sent after a contact that names no EFM8, the
transport after a stopped session, the quiet phase's status, the connect
page's lines, the 3D note from a wrong marker, a refusal greeted again, a
row's selection, WRITE... live, BACK live while leaving, a greeting every
frame, rows not read-only, DShot restarted before the job is idle, the hex
lines of a short page, the bind error's line and its PIN name. The script
is not kept.

Not verified: everything on an ESC. No frame of the escwire job has reached
a Bluejay or BLHeli_S ESC; the entry after the AM32 greeting's answer (B4),
the floating gap between `am32BlEnd()` and the bind, LEAVE starting the
firmware and the time to DShot after it (B11), and the connect cycle's
entry times (B1 to B3) are the sources', not measured.

### Slice 4 as built

Files: `src/efm8_backup.{h,cpp}`, the backup's text and save;
`BackupDevice::Bluejay` and `BackupDevice::BlheliS`, the prefixes `BJ` and
`BH`, in `src/backup_ring.*`; `EwJobOp::Write`, `escwireJobWrite()` and
the status's `dirty`, `reread`, `writing`, `refuse` and `badOff` in
`src/escwire_job.*`; `efm8RowEditable()`, `efm8Step()`,
`efm8CoarseFactor()`, `efm8Changes()`, `efm8WriteResult()` and the write's
texts in `src/efm8_view.*`; the screen's states `S_EW_PAGE` and
`S_EW_WRITING`, the editable list, READ AGAIN (`Am32Ctl::ReadAgain`), the
guard on a half-written page, `backupFirst()`, shared with AM32's write,
and `drawWriteTail()`, shared with AM32's page, in `src/ui_am32.*`; ESC
CONFIG's line `BLUEJAY, BLHELI_S: READ AND WRITTEN.` in
`src/ui_registry.cpp`. Host tests in `test/test_escwire_job.cpp` (the
write on the simulated ESC), `test/test_efm8_view.cpp` (the editable rows,
the steps, the results' lines, the backup's text and save),
`test/test_backup_ring.cpp` (BJ and BH in the one ring) and
`test/test_ui.cpp` (`testEfm8Write()`, through the screen);
`fakeEwOnlySettingsPage()` in `test/fake_escwire_port.*` compares the
simulated ESC's whole flash, 64 KiB, outside the settings page with its
state before, and checks that every erase and program frame the ESC saw
came after an address frame naming the settings page with no read between,
and that the log held every transaction. `fakeEwReset()` fills every byte
outside the settings page with a pattern holding no 0xFF, so an erase of
the firmware or the bootloader changes bytes the comparison sees. The base
address, page size and bootloader address of E8B1, E8B2 and E8B5 are
rcbench's (`ew_id.h`, and the simulated ESC's own copy in `ew_sim.h`): the
check holds the write to the page those constants name, and it does not
check them against the parts. The screenshots
`shot_efm8_edit`, `shot_efm8_write`, `shot_efm8_writing`,
`shot_efm8_verified`, `shot_efm8_lost`, `shot_efm8_guard`,
`shot_efm8_asread`, `shot_efm8_ended` and `shot_efm8_oldpage`.

Texts, at scale 1 unless named, (6n − 1) px for n characters; the note's
own are under
[the list, the editor and the write
page](#the-list-the-editor-and-the-write-page) and
[what each page shows](#what-each-page-shows), and `test/test_efm8_view.cpp`
holds each to its width:

| Where | Text | Width |
|---|---|---|
| The editable list's bar, first line, `C_GRID` | `TAP A SETTING TO EDIT` | 21, 125 px |
| A read-only list's bar, first line | `SHOWN ONLY: NOTHING IS WRITTEN.` | 31, 185 px |
| The strip of a half-written list, under `HALF WRITTEN: KEEP THE ESC POWERED.` | `READ AGAIN, THEN WRITE.`; once read, `WRITE AGAIN NOW. REVERT: AS READ.` | 23, 137 px; 33, 197 px |
| A heartbeat stop before the erase | `STOPPED BEFORE THE ERASE: AS READ.` | 34, 203 px |
| The line driver failed before the erase | `LINE DRIVER FAILED: NOTHING ERASED.` | 35, 209 px |
| A draft the session or the screen refuses | `NOT WRITTEN: REFUSED, NOTHING SENT.` | 35, 209 px |
| Under a write that ended the session | `THE SESSION ENDED. TAP OK TO SEARCH.` | 36, 215 px |
| ...with the page half written | `SESSION ENDED: RESTORE ON A COMPUTER.` | 37, 221 px |
| ESC WRITE's backup line then, no backup on a card, amber | `NO BACKUP: OLD PAGE ON THIS SCREEN.` | 35, 209 px |
| The connect page's button, scale 1 | `OLD PAGE` | 8, 47 px |
| The old page's ESC HEX, strip, amber, and the heading in a list's dump | `OLD PAGE, BEFORE THE CUT WRITE:` over `ON THIS SCREEN ONLY: COPY IT.` or `ALSO ON THE CARD: BACKUP BJ018` | 31, 185 px; 29, 173 px; 30, 179 px |
| A read-only list's second line, USB holding the card | `SHOWN ONLY. USB DRIVE: NO BACKUP.`; lost tags `REFLASH ON A COMPUTER. USB DRIVE.` | 33, 197 px each |
| Chooser, ESC SETTINGS' second line | `BLUEJAY, BLHELI_S: READ AND WRITTEN.` | 36, 215 px |

Choices the note left open, the developer's:

- **The backup.** The page as first read in an escwire session, with the
  contact and identification it was read under, is the session's backup;
  a later session after the cycle greets again has its own. It is saved as
  AM32's is, in `backupFirst()`, on core0 with the flush waited for: the
  EFM8 bootloader waits for its next command with no timeout, and the job
  is idle in mode, so a heartbeat lapse during the flush stops nothing; the
  screen feeds the heartbeat before it sends Write. A Write the job refuses
  for a lapse is asked again. The card-swap check is AM32's. A read-only
  list saves its backup as it opens, as AM32's read-only view does.
- **The job's write.** `escwireJobWrite()` copies the draft into the
  command block; core1 copies it into its own buffer when it takes the
  command and calls `ew_session_write()`, which judges and copies it. The
  status is published when the session's `writing` or `dirty` changes, so
  core0 sees a half-written page from the erase on. The lapse and the
  release call `ew_session_stop()` for a write as for a read: rcbench's
  stop cuts a write before its first erase or program at once and lets one
  after it run to its read-back.
- **Editing.** A row edits where rcbench's map has it `EW_ACC_EDIT` on a
  written layout; the rest are drawn `ReadOnly`. A step goes to the next of
  the field's values, `alt` among them; a value none of the field's steps
  to the nearest value in that direction. A coarse step is the range over
  11, rounded up, as on AM32: 24 for 0 to 255. A value wider than the
  editor bar's 140 px at scale 2, `2S AND ABOVE`, is drawn at scale 1 there,
  on AM32's bar too.
- **A half-written page.** The list edits nothing; REVERT stays, so that
  `WRITE...` writes the page as first read; `WRITE...` writes the draft sent
  last or the page as read, as the session takes, once a read came. The
  strip names the state under the row's note and the read-only lines.
- **READ AGAIN.** It reads in the list, BACK drawn disabled meanwhile. A
  page not half written read again is the page as read from then on, and
  the edits go onto it field by field, so a changed page keeps the user's
  changes; a half-written page keeps the page as first read, which the
  session does too. A read that fails keeps READ AGAIN.
- **A session that ended in a write.** A heartbeat stop, a driver failure
  or a frame the guard refuses after the erase leave the session ended:
  the result's second line says `THE SESSION ENDED. TAP OK TO SEARCH.`, OK
  greets the ESC again as after a session that stopped
  ([slice 3 as built](#slice-3-as-built)), and BACK opens the leave page. A
  half-written page is then read by the next session as what it is, most
  likely empty or with lost tags, which rcbench writes neither of; a page
  whose marker and tags survived reads as writable, and its first read is
  the torn page. Either way the RAM copy of the old page went with the
  session. So the second line reads
  `SESSION ENDED: RESTORE ON A COMPUTER.` instead, and the screen keeps the
  session's first read, the page the backup holds, for the rest of the
  visit (`ewKeepOldPage()`): `OLD PAGE`
  on the connect page opens its bytes as ESC HEX, and a later EFM8 list's
  ESC HEX shows them under its own, so that they can be copied by hand.
  Without a backup on a card ESC WRITE's backup line says the page is on
  the screen only. A READ AGAIN that ends the session on a half-written
  page, and the guard's second BACK, keep it the same way. Only the first
  page kept stays: a later session that ends half written, on a torn page
  it read as writable, does not replace it.
- **The guard's second BACK** sends Stop, which aborts the idle session and
  parks the line with no LEAVE, and opens the leave page with STAY
  returning to the connect page, which greets again.
- **The tester's 3D note.** A verified write sets it from the page written;
  a half-written page with DIRECTION at 3D in the draft sets it, as AM32's
  write sets it once the ESC accepted the block.
- **The AM32 path.** An AM32 read after an EFM8 session in the same visit
  starts with no backup saved, as a new device.
- **READ AGAIN's page.** READ AGAIN takes the session's identification and
  rebuilds the rows. A page read again that is not half written and differs
  from the page the backup holds becomes the backup's page: the next write
  saves a backup of it first, under the next number.
- **The BOARD voltage after the backup.** The write asks it again after the
  backup's flush, as KST does. The flush runs on the spot and `ui.cpp`
  judges the board's cell once a frame, so today the second check sees what
  the first saw; it holds for a flush that one day spans frames.
- **Read-only backups.** A USB host holding the card has its own lines. A
  list opened again on a page whose text is the ring's latest backup, of the
  same device, saves no second copy and names that backup
  ([backups.md](backups.md#what-the-ring-holds)).

Mutations, each applied to a copy of the tree, the suite built and run: 78, 69
failing the suite, among them 10 earlier ones whose code changed, run again.
Nine pass. Six are equivalent: Write taken outside InMode without the
job's own check, which `ew_session_write()` refuses the same way; `WRITE...`
live on a read-only list without its layout check, where no row edits, so
there is never a change to write; LEAVE asked again on a half-written page,
which the guard keeps from ever sending LEAVE; the write's refusal of a page
other than ESC WRITE showed, which no path reaches, as on AM32; READ AGAIN
copying the session's identification and rows, which a read does not change;
and the backup's check that the ring's latest number is the same device's,
whose file name, with its prefix, exists only then. Three are backstops the
suite cannot see: the screen's own `ew_image_check()` before Write, whose
drafts the session's guard refuses alike (the job's tests hold the guard to
each refusal); the clamp of a draft longer than 256 bytes in
`escwireJobWrite()`, which no caller sends and which would only overrun
memory; and the BOARD check after the backup's flush, which sees the cell the
check before it saw. The others fail the suite: the draft not copied to core1
or its length lost, the erase not published, a lapse or the release aborting,
`dirty` or `reread` not reported; rows editable on read-only pages or
shown-only fields editable, a step to any byte, the coarse factor rounded
down, CHANGED or LOST needing no read, an ended session not said or a
half-written one said as any, a stopped half-written page called stopped, the
backup name cut, the no-card lines swapped or a USB host's taken for them,
changes counted wrong; Bluejay saved as BH, a capture failure ignored, the
next name taken as the latest, BJ and BH swapped in the ring, a copy equal to
the latest saved again or one differing not saved; `WRITE...` dead on a
half-written page, edits on it, the page as read not writable, no BOARD
refusal, a write with no backup or after a failed one, the verified page not
taken, the 3D note not set by a write or by a half-written draft, a Write
refused for a lapse not asked again, no guard on a half-written page, its
second BACK not aborting or sending LEAVE, BACK live while writing, edits
dropped or an ended session kept by READ AGAIN, a failed READ AGAIN taken as a
read, a page read again getting no backup, OK after an ended session going to
the list, the old page not kept by an ended write, an ended READ AGAIN or the
guard's abort, replaced by a later session's, kept across visits, not offered
on the session's page, not under a list's dump or without its backup's name,
the screen-only backup line missing, the read-only backup not saved, the
backup state kept into a second session or into an AM32 answer after it, the
as-read line missing, the half-written strip's lines swapped, REVERT or a
swipe doing nothing, shown rows drawn editable, the backup line naming AM, the
sent draft not kept, a wide value at scale 2; in the fake, no pattern outside
the page, a read keeping the address, an overflowed log passing. The script
and its list are kept with the build's scratch files, outside the repository.

Not verified: everything on an ESC. No write of this slice has reached a
Bluejay or BLHeli_S ESC: the erase, program and read-back are rcbench's
simulated ESC's, the window in which a power cut loses the tags is not
measured, and what an ESC does on LEAVE after a write, and whether it runs
on the page written, is B6 and B11. The card's flush on core0 before the
write is not timed on the board.

## Decisions

Decisions of the owner, 2026-11-15, answering the open questions of
2026-11-14:

1. **Entry and naming.** Option (e), together with decision 1 of
   [usb-bridge.md](usb-bridge.md#decisions) (its option c). MENU's row 2
   is the chooser `ESC CONFIG`, holding `ESC SETTINGS`, the one settings
   screen for AM32, Bluejay and BLHeli_S, and `USB BRIDGE`. This note
   recommended option (a), one renamed screen; (e) is the pair that the
   bridge's recommendation and this note's (a) make together.
2. **The connect cycle.** Option (d): 1000 ms quiet on the pull-up, then 3
   greetings, about 125 ms, in turn. This is the recommendation.
3. **Leaving.** Option (a1): LEAVE sends rcbench's `00 01`, the result line
   reads `LEAVING STARTS THE ESC WITH THEM.`, and the run cue is cleared
   when LEAVE went out. This is the recommendation.
4. **Repair after a cut write.** Option (a) now: no repair here, lost tags
   read read-only. rcbench is asked for option (b), the EFM8 restore;
   slice 6 waits on it. This is the recommendation.
5. **Backup names.** Option (a): `BJnnn.TXT` for Bluejay and `BHnnn.TXT`
   for BLHeli_S. This is the recommendation.
6. **A write without a card.** Option (a): as AM32, the write runs, the
   backup stays in RAM and the write page says `NO CARD: OLD VALUES LOST ON
   LEAVING` in amber. This is the recommendation.
7. **Older layouts.** Option (a): read-only lists with rcbench's field
   names. This is the recommendation.
8. **Leaving without LEAVE.** Option (a): the leave page of SERVO TESTER,
   KST SERVO and HITEC SERVO. This is the recommendation.

## Open questions

None open. Questions 1 to 8 are answered by decisions 1 to 8. Asking
rcbench for the EFM8 restore (decision 4) is a request to another project,
not a question to the owner.

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
- `ew_line.c` on SDK 2.1.1: it compiles and links
  ([slice 1 as built](#slice-1-as-built)); nothing calls it, and no frame of
  it has been on a pin.
- Whether a desktop configurator offers narrower ranges than rcbench's maps
  for any field.
