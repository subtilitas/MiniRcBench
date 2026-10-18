# Files copied from rcbench

The files under this directory are copied from
[rcbench](https://github.com/subtilitas/rcbench) at commit `453cf334`, with
their text unchanged. rcbench is MIT licensed by the same copyright holder as
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

The servo tester uses them: `servo_sweep` computes the sweep curve on core1,
and `out_pwm_map` maps each GPIO (general-purpose input/output) to its PWM
(pulse-width modulation) slice and channel for `src/servo_pwm.cpp`. Both are
built as C11, in the firmware by `src/servo.cmake` and in the host suite by
`test/servo.mk`.

Their rcbench tests are under [test/rcbench/](../../test/rcbench/):
`test_servo_sweep.c` and `greatest.h` copied unchanged, and
`test_out_pwm_map.c` adapted from the out_pwm_map cases of rcbench
`test/host/test_outputs.c`. `make -C test` builds and runs both.

Files adapted rather than copied are in `src/` and start with the line
`Adapted from rcbench <path>, commit 453cf334, MIT.`
