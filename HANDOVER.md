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

1. **Firmware build, red on f549a65.** CI job "Firmware (touch-lcd-2.8)"
   stops at `src/ui_kst.cpp:904`, in `fitNote()`:
   `'snprintf' output may be truncated` (`-Werror=format-truncation`):
   `"LINE %u: %s"` with a 48-byte `what` into the 64-byte note. The host
   build does not warn. Fix: size the note for the worst case, or bound
   `what` with `%.*s`. Build the firmware locally before the push. Every
   other check is green on f549a65: host tests, coverage, docs, config
   permutations, blackbox round-trip, workflow lint.
2. **Mutation check of the review fixes.** Not run. Script:
   `$SP/mut_kst4.py` with the list in `$SP/mut5_list.py` (9 mutations,
   one per fix: B1, B2, S1, S2, S3, S4, N1, N2, S9). Each must fail the
   host suite. The first 10 screen mutations of d582158 were all caught.
3. **Doc items from the first review**, not yet changed:
   - `wiki/KST-Programming.md:66-67` and `wiki/KST-Programmierung.md:71`:
     the strip reads `CHECKING GP29 FOR AN ESC` only while an ESC answers;
     the normal sequence is `RELEASING GP29`, then `TAKING GP29`.
   - `docs/design/kst-programming.md:1184`: the write page reads
     `NO CARD: BACKUP IN MEMORY`.
   - `wiki/KST-Programming.md:232-234` and the German page `:247-250`:
     `NO SERVO ANSWERS. CHECK THE PLUG AND THE SUPPLY.` is listed among
     results that end in `NOTHING WAS WRITTEN.`; it does not end so.
   - Prose lines over 80 characters: `README.md:361`,
     `docs/design/kst-programming.md:1287`.
   - The screen changed in f549a65 and the docs do not say so yet:
     - + on DEAD BAND widens the band;
     - `REPLY NOT VALID: <word>` replaces `A REPLY CAME, NOT VALID:`;
     - the restore page counts registers (`n REGISTERS DIFFER FROM THE
       BACKUP`);
     - READ ONLY sits in the hint area of the field list.
4. **Review loop.** An Opus review of the new head, then fix and review
   again until a review returns 0 findings. The first review (of d582158)
   had 14 findings; all 14 are addressed in f549a65 but not yet
   re-reviewed.
5. **Merge rules.** CI green and the bot reviews read (Copilot posted only
   its quota notice), then a squash merge. Strip the session-link footer
   that `create_pull_request` adds; none is in the PR body now.

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
