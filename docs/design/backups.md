# Device backups on the SD card

Planned, nothing built. Every tool that writes settings into a device saves
the device's settings, as read before the first write of a session, to the
SD (Secure Digital) card first. All of these backups share one directory,
`BACKUP/`, and one ring of 50 numbers. The SD write cache of
[sd-write-cache.md](sd-write-cache.md) runs during the pin claims of these
tools. The owner's decisions of 2026-10-27 set the layout, the names, what the
ring holds and the cache's scope.

| Tool | Device | Backup | Prefix | Today |
|---|---|---|---|---|
| KST SERVO | KST and Chaservo servos | the image read at entry, rcbench's text format | `KST` | in the ring of `KST/` ([kst-programming.md](kst-programming.md#storage)) |
| AM32 CONFIG | AM32 ESCs (electronic speed controllers) | the settings block read at entry, as text | `AM` | none |
| Hitec | Hitec servos | none: [hitec-reading.md](hitec-reading.md) reads and dumps and never writes | `HT`, reserved | none |

## The ring

- Names: the device's prefix and a ring number of 3 digits, `.TXT`:
  `BACKUP/KST017.TXT`, `BACKUP/AM018.TXT`. The numbers 001 to 050 are one
  sequence for all devices, so a computer's file list sorted by date and the
  ring's order agree, and a number names one backup whatever its device.
- `BACKUP/LAST` holds the latest number as 3 digits and a line feed, 4 bytes,
  as `KST/LAST` does today.
- The next backup takes the number after the latest, 001 after 050:
  1. `BACKUP/LAST` is rewritten in place with the new number.
  2. A file of the same device under that number is written over in place.
     A file of another device under it is renamed to the new name first,
     which keeps its cluster, then written over. Further files under the
     number, copied on from a computer, are removed.
  3. No file under the number: a new file.
- A save that stops part way leaves the number claimed, and the old file or
  part of the new bytes under it, which the file's checksum line refuses at
  LOAD. The latest backup is never the one written over.
- Without a readable `BACKUP/LAST` the latest number is guessed from the
  names, as `kstNamesGuess()` does today: the highest until 050 is taken, then
  the one before the lowest free number, and 050 with all 50 taken.
- A name with another prefix, a number above 050 or any other form is outside
  the ring: never written over, never removed. A directory with a ring name is
  passed over.
- The directory holds at most 50 backups and the marker: 53 entries with `.`
  and `..`, 4 sectors on FAT32, about 10 on exFAT, as `KST/` does today.

One ring for all devices means that 50 sessions of one device push out the
backups of the others. The name of the ring's latest file shows on the write
page and the leave page of each tool, as today's `BACKUP: KST/KST001.TXT`.

## What the ring holds

Automatic backups only: one per session, before the session's first write.

- KST's SAVE AS writes to `KST/` under the next free `KSTnnn`, and its files
  are never written over, as rcbench's `kst_store_preset()` proposes.
- The record of a write under way stays `KST/KST.TXT`. A record left by an
  interrupted write is moved into the ring as a backup of the device, with
  the next number, before the session's first write, as it moves into the
  ring of `KST/` today.
- KST's LOAD lists `KST/` and the KST backups of `BACKUP/`, the latest backup
  first. A backup loads as a file.
- AM32 has no record and no LOAD in this plan.

## AM32 backup

Before the first WRITE of an AM32 CONFIG session, the 48 bytes of the
settings block as read at entry (`AM32_SETTINGS_SIZE`, `src/am32_eeprom.h`)
go to the ring as `AMnnn.TXT`. The melody and the CAN (Controller Area
Network) block, 0x30 to 0xBF, are not written by the editor and not saved.

```text
AM32 backup, format 1
MCU family: F051
Settings address: 0x7C00
Layout revision: 3
Firmware: 2.21                       main and sub revision, bytes 0x03 and 0x04
MOTOR POLES = 14
MOTOR KV = 2000 KV
TIMING ADVANCE = 15.0 DEG
...                                  one line per field the layout has
RAW 0x00: 01 03 01 02 15 ...         16 bytes a line, 3 lines
CRC32: 0x1A2B3C4D
```

- One line per field of `AM32_FIELDS` (`src/am32_eeprom.h`) that the
  block's layout revision has: group, name, and value with its unit, as the
  screen shows them.
- The `RAW` lines hold all 48 bytes, so a byte no field covers (the device
  name before layout revision 3, reserved bytes) is kept too.
- The checksum line is a CRC-32 (cyclic redundancy check) over every line
  before it, as in rcbench's KST files.
- `doWrite()` (`src/ui_am32.cpp:475`) writes only after the card took the
  backup, or with no card. A card that is mounted and refuses the backup stops
  the write: `THE CARD TOOK NO BACKUP. NOTHING WAS WRITTEN TO THE ESC.`

## The write cache

The cache is on during the pin claims of KST SERVO and AM32 CONFIG, the
tools that save backups, and off during SERVO and a Hitec claim. Its rules are
those of [sd-write-cache.md](sd-write-cache.md); a tool's write to its device
starts only once the flush of its backup ends clean.

## Card phases

A KST first write touches two directories: `BACKUP/` for the backup and
`KST/` for the record. FatFs has one current directory, so a path into the
other directory looks it up in the root, and the root holds the blackbox logs:
with 1000 logs that is 62 block reads on FAT32 and 187 on exFAT per lookup
(`make -C test kstcount`). Open, below.

## Moving the built KST ring

The ring of `KST/` (`src/kst_files.*`) becomes the ring of `BACKUP/` with the
prefix `KST`:

- `kst_files.cpp`'s ring functions move into a module of their own,
  `src/backup_ring.*`, with the prefix as a parameter; `KST_FILES_NAMES` becomes
  `BACKUP_RING_NAMES`, 50.
- SAVE AS goes back to `kst_store_preset()` and `kst_store_save()` in `KST/`.
- `KST/LAST` is no longer written. A `KST/LAST` and `KST/KSTnnn.TXT` files a
  card already holds stay as they are: SAVE AS files.
- The tests of the ring move with it; the card-phase table of
  kst-programming.md is counted again.

No release carries the ring of `KST/`, so no card in use holds one.

## Work breakdown

1. `src/backup_ring.*` from the ring of `kst_files.cpp`, the prefix as a
   parameter, the rename for another device's number; tests over the
   in-memory volume.
2. KST: the backup and the kept record into `BACKUP/`, SAVE AS back into
   `KST/`, LOAD over both; kst-programming.md, the counting tool's
   scenarios.
3. AM32: the text format, the backup before the first WRITE, the refusal;
   tests through the AM32 screen.
4. The write cache of sd-write-cache.md, on for the two claims.
5. README, wiki pages, STATUS.md.

About 2 days for 1 to 3 and 5, not measured; the cache's own estimate is in
its note.

## Open

- **Two directories in one card phase.** Either the blackbox logs move from
  the root into `LOGS/`, so the root holds a few directories and every lookup
  in it is one block, or the record moves into `BACKUP/` as `BACKUP/KST.TXT`,
  so a KST card phase uses one directory. Proposal: the logs into `LOGS/`; it
  also keeps a Hitec SAVE into `HITEC/` short.
- **SAVE AS in `KST/` is not bounded.** rcbench's store proposes up to 999
  names. A SAVE AS walks the directory 3 to 4 times, and 999 files are 63
  sectors on FAT32 per walk: about 26 s at 105 ms a read, in a card phase.
  Proposal: SAVE AS refuses once `KST/` holds 50 files, `KST/ HOLDS 50 FILES.
  REMOVE SOME ON A COMPUTER.`
- **One ring, many devices.** A bench that programs 50 servos in a row loses
  the backups of every ESC before them. A ring per device was the other
  choice and is not taken.
