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
| 4. The KST SERVO screen | PR #13 open, 2 commits: d582158 and f549a65 |
| 5. Switched servo supply on GP0 | not started; waits on the owner's choice below |
| 6. README and wiki | done in PR #13 for the built screen; the wiring, converter, GP0 pin and SERVO SUPPLY parts wait for step 5 |
| Decision 20: servo tester on the switched supply | not started; depends on step 5 |
| 7. Bring-up | needs the owner's servo and a scope |

## PR #13: what remains, in order

1. **Review loop.** An Opus review of the head after the merge of `main`,
   then fix and review again until a review returns 0 findings. The first
   review (of d582158) had 14 findings, addressed in f549a65; the fixes
   since are not reviewed.
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
