# Handover: KST programming, 2026-10-19

The state of the KST work on branch `ccr-53198405-m8uic9`, for the next
session. This file goes in the last commit before pull request (PR) #13
merges: delete it there, together with the open item in
[STATUS.md](STATUS.md) that points to it.

## Where things stand

| Step of [kst-programming.md](docs/design/kst-programming.md#work-breakdown) | State |
|---|---|
| 1. Vendor rcbench's KST core, line driver, file store | merged, PR #10 |
| 2. Search and core1 job | merged, PR #11 |
| 3. KST files on the SD card | merged, PR #12 |
| 4. The KST SERVO screen | PR #13 open: d582158, f549a65, the merge of `main` 0e498d6, 4be1e6f, and the round 2 fixes |
| 5. Switched servo supply on GP0 | not started; waits on the owner's choice below |
| 6. README and wiki | done in PR #13 for the built screen; the wiring, converter, GP0 pin and SERVO SUPPLY parts wait for step 5 |
| Decision 20: servo tester on the switched supply | not started; depends on step 5 |
| 7. Bring-up | needs the owner's servo and a scope |

## PR #13: what remains, in order

1. **Review loop.** Review round 7, of the round 6 fixes, then fix and
   review again until a review returns 0 findings.
2. **Merge rules.** CI green and the bot reviews read (Copilot posted only
   its quota notice), then a squash merge. Strip the session-link footer
   that `create_pull_request` adds; none is in the PR body now.
3. **Before the merge**, in the PR's last commit: delete this file and
   STATUS.md open item 12, which points to it.

Done on the branch since f549a65:

- `main` merged in, with the Hitec design note.
- The firmware build: `fitNote()` bounds its text with `%.40s`; built
  with `-DSTRICT=ON`, the UF2 check passes.
- The mutation check: 20 bugs put back one at a time, each failing the
  host suite (`$SP/mut_kst4.py`, and `$SP/mut5_list.py` through
  `$SP/mut_tail.py`).
- A test for the record notice after a verified write found a defect:
  BACK on the search after a lost servo went to the leave page and back
  to the search on the next frame. The fix exempts the leave page from the
  loss rule.
- The doc items of the first review and the screen texts of f549a65 in
  the wiki pages, the README and the design note.
- Review round 6 of 39eacae: 1 finding and 1 nit, fixed. The LEAVE
  removal of round 5 removed records the session never wrote: a reset
  `s_first` read `KST_STORE_OK`, which is 0. forgetFirst() resets it to
  `KST_STORE_NO_CARD`, and a card that goes away drops the session's
  claim on its record. Tests for an earlier record, a write cut off by a
  loss, a swapped card, and a card that took neither file.
- Review round 5 of 30109fa: 1 finding and 4 nits, fixed. The docs say
  only the session's first write with a card saves the backup and writes
  the record; LEAVE from a servo in programming mode and not in doubt
  removes the record; a refused read lists no register of an earlier
  read; a card mounted on the write page gets its KSTnnn name looked up
  once; the file order holds until KST999 is taken.
- Review round 4 of 4b7221f: 2 findings and 4 nits, fixed. The REGISTERS
  line names at most 9 registers, `AND n MORE` counts the rest
  (`kstBadRegsText()`, tested in the view and on Main); the backup-line
  test compares the line; the wiki's BACKUP FIRST condition, the file
  order, the SDXC expansion.
- Review round 3 of 3c48825: 3 findings and 2 nits, fixed: a stale read
  refusal, tests for the refused read and write and the backup line, the
  wiki's backup-line table, a README line, a design-doc quote.
- Review round 2 of 4be1e6f: 7 findings and 4 nits, fixed. The heartbeat
  is stamped after the first write's card calls and after the record
  lookup on finding a servo; a refused read is no read; BACK from a
  search after a loss stops it; the write page names no backup the card
  refused; the stepper's answer is cached per field and target. A host
  test reaches a refused read and a refused write by holding core1 off
  for 300 ms after the screen sends them.

## Decision the owner has to make before step 5

The card-phase cap is 15 s: how long the switched supply keeps a servo
powered in programming mode while core0 is in a card call. A first write
on a card at the specification's full times takes 50.9 s to 82.2 s
(computed from FatFs block counts, not measured). The options:

- raise the cap;
- bound the `KST/` directory below the store's 999 files;
- make fewer directory passes in the first write.

## Environment notes

- `SP` is `/tmp/claude-0/-home-user/ecbdb7fb-e40e-5d1f-81d9-7535094a99d7/scratchpad`.
  A new container starts without it.
- Arm toolchain: `$SP/armtc/arm-gnu-toolchain-13.3.rel1-x86_64-arm-none-eabi/bin`.
  `PICO_SDK_PATH=/home/user/sdk/pico-sdk`.
- `$SP/fw` is the firmware build directory, moved from its configured
  path. Configure a fresh one:
  `cmake -G Ninja -S /home/user/minircbench -B $SP/fw2`, then
  `ninja -C $SP/fw2 MiniRcBench` and
  `bash test/check_uf2.sh $SP/fw2/MiniRcBench-touch-lcd-2.8.uf2`.
- Landscape guard check: `bash $SP/guard.sh 1`.
- The old worktrees under `/home/user` (`wt-*`, `mrb-*`) are removed; all
  their content is in `main` through squash merges.
