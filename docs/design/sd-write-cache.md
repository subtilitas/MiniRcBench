# SD write cache for the backup claims

Built, host tests only, not tested on hardware. While the pin claim of KST
SERVO or AM32 CONFIG holds, the two tools that save device backups
([backups.md](backups.md)), block writes to the SD (Secure Digital) card go
into a queue in SRAM (static random-access memory) and return at once. A
flush step on core0 sends them to the card in the order FatFs wrote them, one
block per loop pass, without waiting on the card's busy line. Block reads come
from the queue, from a read cache, or from the card. Core0 keeps drawing,
reading touch and sending the heartbeat while the card programs.

Outside those two claims nothing changes: SERVO, a Hitec claim, the blackbox
logger, SD LOG and USB (Universal Serial Bus) DRIVE use the synchronous path
of [sdio-driver.md](sdio-driver.md).

## What it solves

Without the cache a card call blocks core0 until the card has programmed
every block. The SD specification lets a card take 250 ms per written block
on SDHC (high capacity) and 500 ms on SDXC (extended capacity) (4.6.2.2), and
100 ms per read block. With the cache, the writes leave core0's path; the
servo or the ESC (electronic speed controller) is written only once the flush
ends clean:

| Volume | Card phase | Block writes | Flush at 10 ms a write | Flush at 511 ms a write |
|---|---|---:|---:|---:|
| FAT32 | KST first write, no old record | 12 | 0.12 s | 6.1 s |
| FAT32 | KST first write, an old record kept | 18 | 0.18 s | 9.2 s |
| FAT32 | AM32 backup | 6 | 0.06 s | 3.1 s |
| exFAT | KST first write, no old record | 10 | 0.10 s | 5.1 s |
| exFAT | KST first write, an old record kept | 14 | 0.14 s | 7.2 s |
| exFAT | AM32 backup | 6 | 0.06 s | 3.1 s |

The block writes are the counting tool's
([kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat)). Each
batch adds one uncached read of the boot sector, 105 ms at the specification's
times ([card swap](#card-swap)). Reads still block core0; with the read cache
filled, the counting tool, with a 16-block cache modelled and no log in the
root, counts 0 to 2 reads for every phase but the FILES page right after a
full first write. The table of kst-programming.md counts the phases without
the cache.

## Layers

```
 kst_files.cpp, backup_ring.cpp      card calls, unchanged
 FatFs R0.15                         unchanged
 library glue.c -> sd_card_t         read_blocks, write_blocks, sync
 src/sdio/sd_card_glue.c             sdioRead, sdioWrite: the cache while it is on
 src/sdio/sd_cache.c                 read cache, write queue, flush step
 src/sdio/sd_host.c                  sdh_write_start(), sdh_write_poll()
 src/sdio/sd_pio.c                   unchanged
```

- `src/sdio/sd_cache.*`: the queue and the read cache over a card given as
  three functions (`sdc_dev_t`: read, write start, write poll), so the host
  suite runs it against a fake. MIT, part of the driver.
- `src/sdio/sd_card_glue.c`: `sdioRead()` and `sdioWrite()` go through the
  cache while it is on; `sd_sdio_cache_*()` (`src/sdio/sd_card_cache.h`) turn
  it on and off, step, flush and drop it, each under the card's lock.
- `src/card_cache.*`: the calls the screens make. `cardCacheBegin()` at the
  claim's start, `cardCacheEnd()` at its end, `cardCacheState()`,
  `cardCacheFlush()`, `cardCacheRecover()`, and `cardCachePoll()` from
  `loop()` in `src/main.cpp`, after `sdLogFlush()`.

## The write queue

- 32 entries of one 512-byte block and its LBA (logical block address):
  16,384 bytes of data (`SDC_QUEUE_BLOCKS`).
- `sdioWrite()` splits a call into blocks and appends each one. A call that
  does not fit flushes on the spot until it fits, core0 blocking for that
  time.
- The queue is strictly first in, first out. FatFs orders its writes so that
  a power loss leaves a volume that a check repairs: the directory entry
  before the data, the FAT (File Allocation Table) after it, FSINFO last. A
  merge of two writes to one sector, or any reordering, breaks that order, so
  a sector FatFs writes twice is queued twice.
- A read of an LBA that is queued returns the newest queued copy.
- `sdioSync()` returns at once; `sd_sdio_cache_flush()` is the call that waits
  for the card.

## The flush step

`sdc_poll()` does at most one of these and returns:

1. No block in flight and the queue not empty: at the first block of a batch,
   the swap check ([card swap](#card-swap)); then `sdh_write_start()`: CMD24,
   the block and its CRC (cyclic redundancy check) status token. At 10 MHz
   on 4 data lines a block's transfer takes about 0.1 ms.
2. A block in flight: `sdh_write_poll()` reads DAT0 once. Low is busy, and
   the step returns. High: CMD13 for the status, as `writeChunk()` does, then
   the entry is dropped.
3. A block busy for `SDH_BUSY_TIMEOUT_US`, 1,000,000 µs: a timeout.

`sdh_write_start()` and `sdh_write_poll()` are `writeChunk()` split at its
busy wait, for one block. The SD clock runs from the block's send until
`sdh_write_poll()` sees the block done, as it does during `writeChunk()`'s
busy wait; a block that waits several loop passes keeps the clock running
for that time. A transfer error after CMD24 waits for busy as well: the
error is reported once DAT0 is high. A block is tried again on a CRC,
frame, timeout or no-response error, up to `SDC_ATTEMPTS`, 3 times in all;
a command that gets no response leaves the card ready, so each attempt
reaches it. A card still busy at `SDH_BUSY_TIMEOUT_US` is marked not ready:
it would answer no CMD24 while programming, so the batch ends there. A
block that still fails ends the batch ([errors](#errors)).

## The flush barrier

`cardCacheState()` reads `Off` (no cache), `Idle`, `Flushing` with the blocks
still to go, or `Failed`.

- **KST SERVO.** `kstFilesFirstWrite()` runs as before; its files are then
  queued. The write page reads `SAVING THE BACKUP TO THE CARD.` and
  `BLOCKS LEFT: n`, offers no hold, and sends `Edit` or `Restore` to the job
  only at `Idle`. At `Failed` it mounts the card again
  (`cardCacheRecover()`), forgets the first write, so the next write saves
  the backup again, and reads
  `THE CARD DID NOT TAKE THE BACKUP. NOTHING WAS WRITTEN TO THE SERVO.` BACK
  during the flush leaves the page; the write is not sent, and the flush goes
  on.
- **Any failed batch on KST.** `Idle` after a recovery says only that the
  queue is empty. A batch that fails off the write page, after BACK during
  the flush, in SAVE AS or in a removal of the record, may have held the
  backup, so every recovery forgets the first write. A write, a SAVE AS, a
  LOAD or the FILES page that finds the cache at `Failed` recovers first.
- **AM32 CONFIG** recovers after any failed batch of its backup, the save's
  own included, so the next WRITE saves the backup again.
- **KST SAVE AS** flushes on the spot (`cardCacheFlush()`) and reports what
  the card holds: a failed flush is `THE CARD TOOK NO FILE`, and the next
  write saves the backup again.
- **The record's removal** after a verify, or at LEAVE, stays queued and goes
  to the card in the flush that follows, at the claim's end at the latest.
- **AM32 CONFIG.** Its transport blocks core0 anyway, so after the backup
  `doWrite()` flushes on the spot and writes the ESC only at `Idle`. A failed
  flush reads `NO BACKUP ON THE CARD: NOT WRITTEN`.

`kstFilesWriteAllowed()` lets the write run when the card took either file.
With the queue, a failed batch does not say which file reached the card, so
a failed flush stops the write whatever the queued results were. With no
card the rule is unchanged: the write runs, and the backup stays in memory.

## Read cache

- 16 entries of one block, 8,192 bytes, least recently used out first
  (`SDC_READ_BLOCKS`).
- A read that misses fills it; a queued write of a cached LBA updates the
  entry.
- The tool's screen fills it as it opens: KST walks the ring of `BACKUP/` for
  the backup's name and looks its record up in `KST/`. The root holds `LOGS`,
  `KST` and `BACKUP` ([backups.md](backups.md#card-layout)), so its one block
  stays in the cache with the two directories' 4 blocks each on FAT32.
- A read that misses waits for the block in flight first: a card that is
  programming takes no read command. That wait is up to
  `SDH_BUSY_TIMEOUT_US`, 1 s, of busy, against the specification's 500 ms,
  and 105 ms of read. A wait that ends the batch refuses the read. The card stamp of
  [kst-programming.md](kst-programming.md#card-calls-and-the-heartbeat),
  planned for step 5, stays for such reads.

## Card swap

The board has no card-detect line. A card swapped during the claim answers
the first command after the swap, and a block written from the queue would
then land on the new card, built from the old card's directory and FAT: the
new card's volume would be corrupted.

`cardCacheBegin()` reads the volume's serial number from its boot sector
(`sdLogVolumeId()`: `BS_VolID`, offset 67, on FAT32; `VolumeSerialNumber`,
offset 100, on exFAT; offset 39 on FAT12 and FAT16). Before the first block
of every batch the flush step reads those 4 bytes again, past the cache. A
different number, or a read that fails, drops the queue and the read cache
and sets `Failed`. A card swapped for a byte-for-byte copy with the same
serial number is not detected.

## Errors

A batch that fails:

- the queue and the read cache are dropped, and later reads and writes are
  refused until `cardCacheRecover()`: the card's copy of a dropped sector is
  older than the one FatFs wrote;
- the card is marked not ready, so the remount initialises it again, whether
  it stopped answering or another card is in the slot;
- `cardCacheRecover()` mounts the card again (`sdLogRemount()`), so that FatFs
  drops its own sector window, which holds the state it believed written,
  then reads the serial number of the volume it mounted for the swap check.
  A swapped card takes the claim's later batches. Without a mounted card the
  cache turns off;
- the volume holds the blocks before the one that failed: FatFs's write order
  leaves it as a power loss at that block would. The files of the batch may
  be missing or empty, and a check on a computer repairs the FAT.

## Turning on and off

- `cardCacheBegin()` runs when the KST SERVO or AM32 CONFIG screen's claim
  is taken. A blackbox log that runs then stops first: during a claim the
  DShot pump stops, so the log has nothing to record, and its writes, 7.3
  kB/s or 14.3 blocks a second, would fill the queue behind the backup in
  under 3 s at 511 ms a block (the owner's decision of 2026-10-27). Without
  a mounted card the cache stays off for the claim.
- `cardCacheEnd()` runs when the claim ends: KST's LEAVE, BACK and the loss
  rule, AM32's BACK. It flushes, core0 blocking, recovers a failed flush, and
  turns the cache off. BACK on KST's unplug page keeps the claim and turns
  the cache on again.
- KST's RETRY MOUNT keeps the cache on with the serial number taken at the
  claim's start. A card swapped before it fails the next batch at the swap
  check, with nothing written, and the recovery that follows takes the new
  card's number. A recovery takes the number of the card it mounted, and
  turns the cache off when no card mounts.
- USB DRIVE is started from SD LOG, which neither screen reaches: their BACK
  and KST's LEAVE return to MENU.

## Memory

| Item | Bytes |
|---|---:|
| write queue, 32 blocks and their LBAs | 16,512 |
| read cache, 16 blocks, LBAs and ages | 8,384 |
| swap check and scratch sector | 516 |
| total | 25,412 |

CI's size budget is 400,000 bytes of RAM. The firmware's RAM use is not
recorded in this note.

## Tests

- `test_sdio.cpp`: `sdh_write_start()` and `sdh_write_poll()` against the
  simulated card: one block, busy until the card's time, CMD13 after it, a
  CRC token, busy 1 µs short of the limit and at it, the card still ready
  and the next attempt landing, a lost token reported after busy, a card not
  ready.
- `test_sd_cache.cpp` (`runSdCacheTests`): the cache over a fake card with a
  busy time per block. The order kept for a sector written twice; a read of a
  queued sector; one swap check per batch; no command while busy; read cache
  hits, the oldest pushed out, a written sector's entry; a miss that waits
  for the block in flight; a full queue that flushes until the write fits;
  two retries that land, three that fail; reads refused while failed, and a
  miss whose wait ends the batch; a write error token; a changed serial
  number and a check that does not read; refusals with the cache off.
- `test_kst_ui.cpp`: the claim turns the cache on and stops a running log;
  no write frame while the flush runs; the page's count; the write after the
  flush; a failed flush with no write frame and the card mounted again; the
  next write saving the backup again; the claim's end turning the cache off;
  BACK during the flush and a SAVE AS whose flush fails, then the backup
  saved again; a batch failed before a write, recovered; BACK on the unplug
  page turning the cache on again.
- `test_ui.cpp`: the AM32 write refused after a failed flush.
- The host suite links a fake of `card_cache.h` (`test/fakes.cpp`);
  `card_cache.cpp` and the glue are built by the firmware only.

## Work breakdown

1. `sdh_write_start()` and `sdh_write_poll()`, with their tests. Built.
2. `sd_cache.c` and its tests. Built.
3. The glue, `card_cache.*`, the poll in `loop()`, the logger's stop. Built.
4. KST's write page, SAVE AS and AM32's write on the barrier. Built.
5. Bring-up: busy polled per pass on a scope, a swap during a flush, power
   pulled during a flush and the volume checked on a computer. Not done.
