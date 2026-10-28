# Device backups on the SD card

Built, host tests only, not tested on hardware. Every tool that writes
settings into a device saves the device's settings, as read before the first
write of a session, to the SD (Secure Digital) card first. All of these
backups share one directory, `BACKUP/`, and one ring of 50 numbers
(`src/backup_ring.*`). The owner's decisions of 2026-10-27 set the layout, the
names, what the ring holds and the write cache's scope; the SD write cache of
[sd-write-cache.md](sd-write-cache.md) runs during these tools' claims.

| Tool | Device | Backup | Prefix |
|---|---|---|---|
| KST SERVO | KST and Chaservo servos | the image read at entry, rcbench's text format ([kst-programming.md](kst-programming.md#storage)) | `KST` |
| AM32 CONFIG | AM32 ESCs (electronic speed controllers) | the settings block read at entry, as text (`src/am32_backup.*`) | `AM` |
| Hitec | Hitec servos | the first full read after the entry, rcbench's servo file text, saved by `hitecFilesFirstWrite()` (`src/hitec_files.h`, [hitec-programming.md](hitec-programming.md#storage)); no screen calls it yet | `HT` |

## Card layout

| Path | What |
|---|---|
| `BACKUP/<prefix><nnn>.TXT` | the ring of backups |
| `BACKUP/LAST` | the ring's latest number |
| `KST/KSTnnn.TXT` | KST's SAVE AS files, at most 50 |
| `KST/KST.TXT` | KST's record of a write under way |
| `HITEC/HTnnn.TXT` | Hitec's SAVE AS files; with any other `.TXT` there, at most 50 |
| `HITEC/HITEC.TXT` | Hitec's record of a write under way |
| `LOGS/LOGnnnnn.BFL` | the blackbox logs (`SD_LOG_DIR_PATH`) |

The root holds these four directories, so a lookup in it reads one block.
Logs an earlier firmware wrote to the root stay there and slow every lookup
in it ([kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat)).

## The ring

- Names: the device's prefix and a ring number of 3 digits, `.TXT`:
  `BACKUP/KST017.TXT`, `BACKUP/AM018.TXT`. The numbers 001 to 050
  (`BACKUP_RING_NAMES`) are one sequence for all devices, so a number names
  one backup whatever its device.
- `BACKUP/LAST` holds the latest number as 3 digits and a line feed, 4 bytes.
  The name has no extension, so no list shows it.
- The next backup takes the number after the latest, 001 after 050
  (`backupSave()`):
  1. `BACKUP/LAST` is rewritten in place with the new number
     (`backupClaim()`).
  2. A file of the same device under that number is written over in place. A
     file of another device under it is renamed to the new name first, which
     keeps its cluster, then written over. Further files under the number,
     copied on from a computer, are removed (`backupPut()`).
  3. No file under the number: a new file.
- A save that stops part way leaves the number claimed, and the old file or
  part of the new bytes under it, which the file's checksum line refuses at
  LOAD. The latest backup is never the one written over. A marker the card
  refuses stops the save with nothing written over.
- Without a readable `BACKUP/LAST` the latest number is guessed from the
  names (`backupGuess()`): the highest until 050 is taken, then the one before
  the lowest free number, and 050 with all 50 taken.
- A name with another prefix, a number above 050, or any other form is
  outside the ring: never written over, never removed. A directory with a
  ring name is passed over.
- The directory holds at most 50 backups and the marker: 53 entries with `.`
  and `..`, 4 sectors on FAT32, about 10 on exFAT.

One ring for all devices means that 50 sessions of one device push out the
backups of the others. A ring per device was the other choice and is not
taken.

Current limitations:

- A ring that has turned and lost `BACKUP/LAST`, deleted on a computer or
  unreadable, guesses its latest number from the names. With all 50 numbers
  taken the guess is 050, so the next backup writes over 001, which need not
  be the oldest: a ring whose true latest is 004 loses its 4th-newest
  backup. The board has no real-time clock, so the files' dates cannot tell
  the order either.
- A card on which 49 or 50 ring names are directories, made on a computer,
  leaves no free number, or one: a first write that keeps an old record then
  writes the session's backup over the record it just moved.

## What the ring holds

Automatic backups only: one per session, before the session's first write.

- KST's SAVE AS writes to `KST/` under the next free `KSTnnn`
  (`kst_store_preset()`), and its files are never written over. It refuses
  once `KST/` holds 50 files: `KST/ HOLDS 50 FILES. REMOVE SOME.`
- KST's record of a write under way stays `KST/KST.TXT`. A record left by an
  interrupted write is moved into the ring as a KST backup, under the next
  number, before the session's first write (`backupMoveIn()`).
- KST's FILES page lists the record, then the KST backups of `BACKUP/`, the
  latest first, then `KST/`. A backup loads as a file.
- Hitec's SAVE AS writes to `HITEC/` under the next free `HTnnn` and
  refuses once `HITEC/` holds 50 `.TXT` files. Its record of a write under
  way is `HITEC/HITEC.TXT`; a record left by an interrupted write is moved
  into the ring as a Hitec backup, under the next number, before the
  session's first write, and the session's backup takes the number after
  it. A Hitec ring entry holds a servo file or such a record; both load.
- AM32 has no record and no LOAD.

## AM32 backup

Before the first WRITE of an AM32 CONFIG session, the 48 bytes of the
settings block as read at entry (`AM32_SETTINGS_SIZE`, `src/am32_eeprom.h`)
go to the ring as `AMnnn.TXT`. The melody and the CAN (Controller Area
Network) block, 0x30 to 0xBF, are not written by the editor and not saved.

```text
AM32 ESC settings, format 1
ESC: F051
Settings address: 7C00
Layout revision: 3
Firmware: 2.21                       main and sub revision, bytes 0x03 and 0x04
MOTOR POLES = 14
MOTOR KV = 2000 KV
TIMING ADVANCE = 15.0 DEG
...                                  one line per field the layout has
bytes 48
00: 01 03 01 02 15 ...               16 bytes a line, 3 lines
checksum 1A2B3C4D
```

- One line per field of `AM32_FIELDS` that the block's layout revision has:
  group, name, and value with its unit, as the screen shows them.
- The byte lines hold all 48 bytes, so a byte no field covers (the device
  name before layout revision 3, reserved bytes) is kept too.
- The checksum line is a CRC-32 (cyclic redundancy check) of IEEE 802.3 over
  every line before it, as in rcbench's KST files.
- `doWrite()` (`src/ui_am32.cpp`) writes only after the card took the
  backup, or with no card. A card that is mounted and refuses the backup stops
  the write: `NO BACKUP ON THE CARD: NOT WRITTEN`. With no card the write
  runs, and the backup stays in memory until a write with a card saves it, or
  the editor closes. A verified write with a backup reads
  `WRITE VERIFIED. BACKUP AM018`.

## Card calls

`src/card_dir.*` holds what the KST files and the backups share: the mount
rule (`cardMounted()`), the scope that makes one directory FatFs's current
one (`CardDirScope`), and the file calls (`cardCreate()`, `cardOverwrite()`,
`cardRead()`, `cardSize()`, `cardWalk()`). A KST first write works in
`BACKUP/` and reaches `KST/KST.TXT` from the root. The counted card phases,
the AM32 backup among them, are in
[kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat).

## The write cache

The cache of [sd-write-cache.md](sd-write-cache.md) runs during the pin
claims of KST SERVO and AM32 CONFIG, the tools that save backups, and not
during SERVO. The HITEC SERVO claim runs it as well once its screen is built
([hitec-programming.md](hitec-programming.md#decisions-of-the-owner),
decision 11, work breakdown step 4). Its rules are that note's; a tool's write to
its device starts only once the flush of its backup ends clean. A claim of
either tool stops a blackbox log that runs when it opens: during a claim the
DShot pump stops, so the log has nothing to record.

## Tests

- `test_backup_ring.cpp` (`runBackupTests`): the rules (the number after,
  the guess, the marker, the names of each device), 51 saves on a fresh
  card, an ESC's backup renamed under a servo's number, two files under one
  number, the list of one device from the latest, a gap left on a computer,
  no marker and an unreadable one, a directory under a ring name, names
  outside the ring kept through a turn, a marker or a read-only oldest file
  the card refuses, a change of directory that fails, load, no card and a
  card held by USB with no FatFs call.
- `test_kst_files.cpp`: the KST first write into `BACKUP/`, an old record
  moved there, SAVE AS into `KST/` up to 50 files.
- `test_kst_ui.cpp`: the backup line, a backup row on FILES and its LOAD,
  SAVE AS refused at 50 files.
- `test_am32.cpp`: the AM32 backup's text and its checksum.
- `test_ui.cpp`: the AM32 backup through the screen, a card that refuses it,
  no second backup in a session.

## Work breakdown

1. `src/backup_ring.*` and `src/card_dir.*`, with their tests. Built.
2. KST: the backup and the kept record into `BACKUP/`, SAVE AS in `KST/` with
   its limit, LOAD over both. Built.
3. AM32: the text format, the backup before the first WRITE, the refusal.
   Built.
4. The blackbox logs into `LOGS/`. Built.
5. The write cache of sd-write-cache.md, on for the two claims, and the
   claim that stops a running log. Built.
