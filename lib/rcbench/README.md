# Files copied from rcbench

The files under this directory are copied from
[rcbench](https://github.com/subtilitas/rcbench) with their text unchanged:
the Hitec files at commit `3e29dc1b`, every other file at commit `453cf334`. rcbench is MIT licensed by the same copyright holder as
MiniRcBench, subtilitas, so the copies carry the licence of this repository,
[LICENSE](../../LICENSE). Each file keeps its `SPDX-License-Identifier: MIT`
line. The directory layout is rcbench's, so a file here and its original
compare with one `diff`.

| File here | rcbench path |
| --- | --- |
| `shared/servo/servo_sweep.c` | `shared/servo/servo_sweep.c` |
| `shared/servo/include/servo_sweep.h` | `shared/servo/include/servo_sweep.h` |
| `shared/outputs/out_pwm_map.c` | `shared/outputs/out_pwm_map.c` |
| `shared/outputs/include/out_pwm_map.h` | `shared/outputs/include/out_pwm_map.h` |
| `protocols/kst/kst_wire.c`, `kst_reg.c`, `kst_limits.c`, `kst_plan.c`, `kst_session.c`, `kst_pio.c`, `kst_file.c` | the same paths |
| `protocols/kst/include/kst_wire.h`, `kst_reg.h`, `kst_limits.h`, `kst_plan.h`, `kst_session.h`, `kst_pio.h`, `kst_file.h` | the same paths |
| `protocols/kst/rp2350/kst_line.c`, `kst_line.h`, `kst_line.pio` | the same paths |
| `shared/logfile/kst_store.c` | `shared/logfile/kst_store.c` |
| `shared/logfile/include/kst_store.h` | `shared/logfile/include/kst_store.h` |

The Hitec files, at commit `3e29dc1b`:

| File here | rcbench path |
| --- | --- |
| `protocols/hitec/hitec_wire.c`, `hitec_pio.c`, `hitec_pre.c`, `hitec_reg.c`, `hitec_session.c` | the same paths |
| `protocols/hitec/include/hitec_wire.h`, `hitec_pio.h`, `hitec_pre.h`, `hitec_reg.h`, `hitec_session.h` | the same paths |
| `protocols/hitec/rp2350/hitec_line.c`, `hitec_line.h`, `hitec_line.pio` | the same paths |
| `shared/bench/hitec_card.c` | `shared/bench/hitec_card.c` |
| `shared/bench/include/hitec_card.h` | `shared/bench/include/hitec_card.h` |
| `shared/link/link_crc.c` | `shared/link/link_crc.c` |
| `shared/link/include/link_crc.h` | `shared/link/include/link_crc.h` |

The servo tester uses them: `servo_sweep` computes the sweep curve on core1,
and `out_pwm_map` maps each GPIO (general-purpose input/output) to its PWM
(pulse-width modulation) slice and channel for `src/servo_pwm.cpp`. Both are
built as C11, in the firmware by `src/servo.cmake` and in the host suite by
`test/servo.mk`.

Their rcbench tests are under [test/rcbench/](../../test/rcbench/):
`test_servo_sweep.c` and `greatest.h` copied unchanged, and
`test_out_pwm_map.c` adapted from the out_pwm_map cases of rcbench
`test/host/test_outputs.c`. `make -C test` builds and runs both.

The KST programming core (`protocols/kst/`), its RP2350 line driver
(`protocols/kst/rp2350/`) and the KST file store
(`shared/logfile/kst_store.*`) are for the KST programmer of
[kst-programming.md](../../docs/design/kst-programming.md): the search
(`src/kst_seek.cpp`) and the core1 job (`src/kst_job.cpp`) call them, and the
KST SERVO screen (`src/ui_kst.cpp`) starts the job. The core and the store are built as C11, in the
firmware by `src/kst.cmake` and in the host suite by `test/kst.mk`; the line
driver drives PIO (programmable input/output) and DMA (direct memory access)
and is built by the firmware only, with pioasm assembling `kst_line.pio`.
Their rcbench tests, copied unchanged from rcbench `test/host/` into
`test/rcbench/`, are `test_kst_wire.c`, `test_kst_reg.c`, `test_kst_plan.c`,
`test_kst_session.c`, `test_kst_pio.c`, `test_kst_file.c` and
`test_kst_store.c`, with the servo and line model `kst_sim.h` and the fixture
`fixtures/kst_line_words.txt`, the words pioasm assembles `kst_line.pio` to.
rcbench's `kst_fake_card.h` is not copied: it serves rcbench's card and prog
tests, whose code is not ported, and `test_kst_store.c` brings its own fake.

The Hitec programming core (`protocols/hitec/`), its RP2350 line driver
(`protocols/hitec/rp2350/`), the servo file text (`shared/bench/hitec_card.*`)
and the CRC-32 (32-bit cyclic redundancy check) that text uses
(`shared/link/link_crc.*`) are for the Hitec programmer of
[hitec-programming.md](../../docs/design/hitec-programming.md). Nothing in
`src/` calls them yet. The core, the text and the CRC are built as C11, in the
firmware by `src/hitec.cmake` and in the host suite by `test/hitec.mk`; the
line driver is built by the firmware only, with pioasm assembling
`hitec_line.pio`. Their rcbench tests, copied unchanged from rcbench
`test/host/` into `test/rcbench/`, are `test_hitec_wire.c`, `test_hitec_pre.c`,
`test_hitec_pio.c`, `test_hitec_reg.c`, `test_hitec_reg_plan.c`,
`test_hitec_session.c`, `test_hitec_session_write.c` and `test_hitec_card.c`,
115 cases, with `hitec_headers.cpp`, which compiles the core's headers as
C++11; the models and fixtures `hitec_pio_model.h`, `hitec_reg_fix.h`,
`hitec_session_rig.h` and `fixtures/hitec_line_words.txt`. CI holds both line
fixtures to pioasm's assembly of the `.pio` files.
`test/rcbench/link_pages.h` is not rcbench's: rcbench's header is the page
map of its link, which is not ported, and this stub holds only the 5 values
`test_hitec_session.c` and `hitec_pio_model.h` need, at their `3e29dc1b`
values.

Files adapted rather than copied start with the line
`Adapted from rcbench <path>, commit <commit>, MIT.`
