# SD write cache for the backup claims

Planned, nothing built. While the pin claim of KST SERVO or AM32 CONFIG
holds, the two tools that save device backups
([backups.md](backups.md)), block writes to the SD
(Secure Digital) card go into a queue in SRAM (static random-access memory)
and return at once; a flush task on core0 sends them to the card in the
order FatFs wrote them, one block per loop pass, without waiting on the
card's busy line. Block reads come from a read cache that the tool's screen
fills when it opens. Core0 then keeps drawing, reading touch and sending
the heartbeat while the card programs, and a card phase of
[kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat) holds
no card wait longer than one block read.

Outside those two claims nothing changes: SERVO, a Hitec claim, the
blackbox logger, SD LOG and USB
(Universal Serial Bus) DRIVE use the synchronous path of
[sdio-driver.md](sdio-driver.md).

## What it solves

Today a card call blocks core0 until the card has programmed every block.
The SD specification lets a card take 250 ms per written block on SDHC (high
capacity) and 500 ms on SDXC (extended capacity) (4.6.2.2), and 100 ms per
read block. The counted card phases on a full KST ring
([kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat),
`make -C test kstcount`) reach 12.8 s on FAT32 and 16.3 s on exFAT with no
log in the root, and 35.9 s with 1000 logs, against a card-phase cap of
15 s.

With the cache, the writes leave core0's path:

| Volume | Card phase | Block writes | Flush at 10 ms a write | Flush at 511 ms a write |
|---|---|---:|---:|---:|
| FAT32 | first write, no old record | 12 | 0.12 s | 6.1 s |
| FAT32 | first write, an old record kept | 18 | 0.18 s | 9.2 s |
| FAT32 | SAVE AS | 5 | 0.05 s | 2.6 s |
| exFAT | first write, no old record | 10 | 0.10 s | 5.1 s |
| exFAT | first write, an old record kept | 14 | 0.14 s | 7.2 s |
| exFAT | SAVE AS | 5 | 0.05 s | 2.6 s |

The writes are the counting tool's. Each flush adds one uncached read of the
boot sector, 105 ms at the specification's times ([card swap](#card-swap)).
The flush time does not depend on the logs in the root: the root is passed
over once, when the screen opens ([read cache](#read-cache)).

What it does not solve: the backup and the record still have to be on the
card before the first frame of a servo write. The servo waits in programming
mode for the whole flush, as it waits for the blocking calls today; the
difference is that the screen stays live and the heartbeat holds.

## Layers

```
 kst_files.cpp, kst_store.c          card calls, unchanged
 FatFs R0.15                         unchanged
 library glue.c -> sd_card_t         read_blocks, write_blocks, sync
 src/sdio/sd_cache.c        NEW      read cache, write queue, flush task
 src/sdio/sd_card_glue.c             sdioRead, sdioWrite call the cache
 src/sdio/sd_host.c                  + sdh_write_start(), sdh_write_poll()
 src/sdio/sd_pio.c                   unchanged
```

`sd_cache.c` sits between the library's `sd_card_t` callbacks in
`src/sdio/sd_card_glue.c` (`sdioRead`, `sdioWrite`, set at `:177` to `:179`)
and `sd_host.c`. FatFs and the SD library are not changed. The cache is MIT,
part of the driver in `src/sdio/`.

## The write queue

- 32 entries of one 512-byte block and its LBA (logical block address):
  16,384 bytes of data.
- `sdioWrite()` with the cache on splits a call into blocks and appends each
  one. A call that does not fit waits for room on the synchronous path: it
  flushes entries until the call fits, and core0 blocks for that time.
- The queue is strictly first in, first out. FatFs orders its writes so that
  a power loss leaves a volume that a check repairs: the directory entry
  before the data, the FAT (File Allocation Table) after it, FSINFO last. A
  merge of two writes to one sector, or any reordering, breaks that order, so
  a sector FatFs writes twice is queued twice.
- A read of an LBA that is queued returns the newest queued copy.
- `sdioSync()` returns at once; the flush barrier below is the only call that
  waits for the card.

## The flush task

`sdCachePoll()` runs from `loop()` in `src/main.cpp` after `sdLogFlush()`, on
every pass, core0 only. Each call does at most one of these steps and
returns:

1. No block in flight and the queue not empty: if this is the first block
   of a batch, read the boot sector uncached and compare its volume serial
   number ([card swap](#card-swap)). Then `sdh_write_start()`: CMD24, the
   block, the CRC (cyclic redundancy check) status token. At 10 MHz on 4 data
   lines a block's transfer takes about 0.1 ms.
2. A block in flight: `sdh_write_poll()` reads DAT0 once. Low is busy: return.
   High: CMD13 for the status, as `writeChunk()` does today
   (`src/sdio/sd_host.c:220`), then drop the entry.
3. A block in flight for longer than `SDH_BUSY_TIMEOUT_US`, 1,000,000 µs: a
   timeout, as today.

A block that fails is tried again up to `SDH_ATTEMPTS`, 3 times, as
`sdh_write()` does. A block that fails 3 times ends the batch:
[errors](#errors).

`sdh_write_start()` and `sdh_write_poll()` are `writeChunk()` split at its
`busyWait()` (`src/sdio/sd_host.c:212`), for one block. Whether DAT0 shows
busy with the SD clock stopped between two polls, or the clock has to run
from the start to the end of the busy time, is to be checked against the
specification and on hardware. The bus table's comment on `dat0` today says
the clock runs while the caller polls (`src/sdio/sd_host.h:84`).

## The flush barrier

```
sd_cache_state_t sdCacheFlushState(uint16_t *left);   // Idle, Flushing, Failed
```

- `Flushing` while the queue holds entries or a block is in flight; `left`
  is the number of blocks still to go.
- `Idle` once the queue is empty and the last block's status was clean.
- `Failed` after a block that failed 3 times, until the cache is reset.

The KST screen calls `kstFilesFirstWrite()` as today, and AM32 CONFIG saves
its backup the same way ([backups.md](backups.md#am32-backup)); the results
then mean
"queued". The write page shows the flush:

```
  SAVING THE BACKUP: 7 OF 12 BLOCKS
  [ STOP ]                          BACK as today
```

and sends `Edit` or `Restore` to the job only at `Idle`. At `Failed` it
shows `THE CARD DID NOT TAKE THE BACKUP. NOTHING WAS WRITTEN TO THE SERVO.`
and sends nothing. SAVE AS and the removal of the record after a verify
show the same count in the FILES strip and the result line.

`kstFilesWriteAllowed()` today lets the write run when the card took either
file. With the queue, a failed batch does not say which file reached the
card, so a failed flush stops the write whatever the queued results were.
With no card the rule is unchanged: the write runs, and the backup stays in
memory.

## Read cache

- 16 entries of one block, 8,192 bytes, least recently used out first.
- A read fills it; a queued write of the same LBA updates the entry.
- The tool's screen fills it when it opens, while no supply is switched on:
  it walks the ring of `BACKUP/`, and KST also looks its record up in `KST/`.
  The root holds `LOGS`, `KST` and `BACKUP`
  ([backups.md](backups.md#card-layout)), so its one block stays in the
  cache with the two directories' 4 blocks each on FAT32.

A card phase reads 0 to 2 blocks with the cache filled: the counting tool,
with a 16-block cache modelled and no log in the root, counts 0 to 2 reads
for every phase but the FILES page right after a full first write. A read that misses blocks core0 for the read, and for the
block in flight to end first: up to 500 ms busy and 105 ms read within the
specification. That is longer than the 250 ms heartbeat, so the card stamp
of [kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat)
stays for reads, with its cap lowered to 2000 ms.

## Card swap

The board has no card-detect line. A card swapped during the claim answers
the first command after the swap, and a block written from the queue would
then land on the new card, built from the old card's directory and FAT: the
new card's volume is corrupted.

Before the first block of every batch the flush task reads the boot sector
past the cache and compares the volume serial number: `BS_VolID`, offset 67,
on FAT32; `VolumeSerialNumber`, offset 100, on exFAT. A different number, or
a read that fails, drops the queue and the read cache, ends the batch as
`Failed` and unmounts the card, so that FatFs reads the new card from
scratch. A card swapped for a byte-for-byte copy with the same serial number
is not detected.

## Errors

A batch that fails:

- the queue and the read cache are dropped;
- the card is unmounted (`sdLogRemount()` remounts it on the next RETRY), so
  that FatFs drops its own sector window, which holds the state it believed
  written;
- the volume may hold part of the batch. FatFs's write order leaves it in
  the state a power loss at that block would leave: the files of the batch
  may be missing or empty, and a check on a computer repairs the FAT.

## Handovers

The cache turns on when the KST SERVO or AM32 CONFIG screen opens its claim
and off when the claim
ends. Each of these waits for `Idle` or `Failed` first, on the synchronous
path, core0 blocking:

- the end of the claim: KST's LEAVE, BACK and the loss rule, AM32's BACK;
- `sdLogRemount()` and every `f_unmount()`;
- the USB DRIVE handover (`mscTick()`). SD LOG starts it, and SD LOG is not
  reachable while either screen is open: their BACK and KST's LEAVE return
  to MENU. The wait covers a handover that a later change makes reachable.

## The blackbox logger during the claim

A log's writes, 7.3 kB/s, 14.3 blocks a second, would join the queue behind
the backup's blocks; at the specification's 511 ms a block the flush sends 2
blocks a second, and the queue would fill in under 3 s. The owner's decision
of 2026-10-27: a KST SERVO or AM32 CONFIG claim stops a log that runs when it
opens, before the cache turns on. During a claim the DShot pump stops, so the
log has nothing to record; arming, which starts a log, is not possible while
a claim holds.

## Memory

| Item | Bytes |
|---|---:|
| write queue, 32 blocks | 16,384 |
| write queue LBAs, 32 × 4 | 128 |
| read cache, 16 blocks | 8,192 |
| read cache LBAs and order, 16 × 8 | 128 |
| total | 24,832 |

CI's size budget is 400,000 bytes of RAM. The firmware's RAM use today is
not recorded in this note.

## Tests

- `test_sdio.cpp`: `sdh_write_start()` and `sdh_write_poll()` against the
  simulated card, its busy time stepped by the test clock: one block, a CRC
  token, a write error token, the timeout at 1,000,000 µs, 3 tries.
- `test_sd_cache.cpp`, new: the cache over a fake block device with a busy
  time per block. The queue's order kept for a sector written twice; a read
  of a queued LBA; a full queue that waits; the barrier's three states; the
  serial number check, a changed number and a failed read; a failed block
  and the unmount; every handover waits; the cache off outside the claim.
- `test_kst_ui.cpp`: the write page's count, STOP and BACK during the flush,
  `Edit` sent only at `Idle`, the failed flush, the heartbeat sent every UI
  frame while the flush runs.
- `test/kst_card_count.cpp`: the queue depth of each card phase and the
  flush time at the three sets of card times, beside today's table.
- `test/pio_sdio_emu.py`: a write with DAT0 polled once per call.

## Work breakdown

1. `sdh_write_start()` and `sdh_write_poll()` in `sd_host.c`, with their
   tests; the clock question above answered from the specification.
2. `sd_cache.c`: queue, read cache, flush task, barrier, serial check,
   errors; `sdCachePoll()` in `loop()`; `test_sd_cache.cpp`.
3. The KST and AM32 claims turn the cache on and off; `f_chdir()` once per
   claim;
   the logger's paths from `/`; the handover waits.
4. The write page, SAVE AS and the record removal on the barrier;
   `kstFilesWriteAllowed()` on the flush result; UI tests.
5. The counting tool's flush columns; kst-programming.md's card phase and
   cap for reads only; STATUS.md, README and wiki.
6. Bring-up: busy polled per pass on a scope, a swap during a flush, power
   pulled during a flush and the volume checked on a computer.

About 3 to 4 days for 1 to 5, not measured. Bring-up is not estimated.

## Open

- DAT0 busy with the clock stopped (above).
- The cap of the card stamp for reads: 2000 ms is a proposal.
