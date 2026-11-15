# Files copied from rcbench

The files under this directory are copied from
[rcbench](https://github.com/subtilitas/rcbench) with their text unchanged:
the Hitec files at commit `3e29dc1b`, the S.BUS files, the sensor bus and
the move timing's files at commit `5ef42efd`, the escwire files and the
servo file store at commit `2b4f934a`, the angle meter's files at commit
`e98345aa`, every other file at commit `453cf334`. rcbench is MIT
licensed by the same copyright holder as MiniRcBench, subtilitas, so the
copies carry the licence of this repository, [LICENSE](../../LICENSE). Each
file keeps its `SPDX-License-Identifier: MIT` line. The directory layout is
rcbench's, so a file here and its original compare with one `diff`.

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

The S.BUS files and the sensor bus, at commit `5ef42efd`:

| File here | rcbench path |
| --- | --- |
| `protocols/sbus/sbus.c` | `protocols/sbus/sbus.c` |
| `protocols/sbus/include/sbus.h` | `protocols/sbus/include/sbus.h` |
| `shared/sense/sense_bus.c` | `shared/sense/sense_bus.c` |
| `shared/sense/include/sense_bus.h` | `shared/sense/include/sense_bus.h` |

The move timing's files, at commit `5ef42efd`
(`5ef42efdc9eb092ae7b3b37f24779663e6b9d67d`):

| File here | rcbench path |
| --- | --- |
| `shared/servo/servo_move.c` | `shared/servo/servo_move.c` |
| `shared/servo/include/servo_move.h` | `shared/servo/include/servo_move.h` |
| `shared/servo/servo_sim.c` | `shared/servo/servo_sim.c` |
| `shared/servo/include/servo_sim.h` | `shared/servo/include/servo_sim.h` |

The escwire files and the servo file store, at commit `2b4f934a`
(`2b4f934ad6b6d67f20b46d5ba80001c091e14b89`):

| File here | rcbench path |
| --- | --- |
| `protocols/escwire/ew_wire.c`, `ew_pio.c`, `ew_a.c`, `ew_text.c`, `ew_guard.c`, `ew_id.c`, `ew_map.c`, `ew_map_s.c`, `ew_map_bj.c`, `ew_map_am.c`, `ew_map_e32.c`, `ew_session.c`, `ew_session_text.c`, `ew_file.c`, `ew_file_read.c`, `ew_file_load.c` | the same paths |
| `protocols/escwire/ew_session_int.h`, `ew_file_int.h` | the same paths |
| `protocols/escwire/include/ew_wire.h`, `ew_pio.h`, `ew_a.h`, `ew_text.h`, `ew_guard.h`, `ew_id.h`, `ew_map.h`, `ew_session.h`, `ew_file.h` | the same paths |
| `protocols/escwire/rp2350/ew_line.c`, `ew_line.h`, `ew_line.pio` | the same paths |
| `shared/logfile/servo_store.c` | `shared/logfile/servo_store.c` |
| `shared/logfile/include/servo_store.h` | `shared/logfile/include/servo_store.h` |

The angle meter's files, at commit `e98345aa`
(`e98345aa5219643f25edb7a25afc1d32173bc43f`):

| File here | rcbench path |
| --- | --- |
| `shared/servo/servo_rudder.c` | `shared/servo/servo_rudder.c` |
| `shared/servo/include/servo_rudder.h` | `shared/servo/include/servo_rudder.h` |
| `shared/bench/imu.c` | `shared/bench/imu.c` |
| `shared/bench/include/imu.h` | `shared/bench/include/imu.h` |
| `protocols/witmotion/witmotion.c` | `protocols/witmotion/witmotion.c` |
| `protocols/witmotion/include/witmotion.h` | `protocols/witmotion/include/witmotion.h` |

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
KST SERVO screen (`src/ui_kst.cpp`) starts the job. The core and the store
are built as C11, in the firmware by `src/kst.cmake` and in the host suite by `test/kst.mk`; the line
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
and the CRC-16 (16-bit cyclic redundancy check) that text uses
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
`test_hitec_session.c` needs, at their `3e29dc1b` values.

The sensor bus (`shared/sense/sense_bus.*`) is for the INA219 current sensor
on the I2C (Inter-Integrated Circuit) bus, of
[current-sense.md](../../docs/design/current-sense.md): it decides when a part
goes offline and when the bus is stuck, and `src/sense.cpp` supplies its wire
over the Pico SDK (software development kit) and reads the INA219 through it.
It is built as C11, in the firmware by `src/sense.cmake` and in the host suite
by `test/sense.mk`. rcbench has no test of it of its own: its tests reach it
through the INA228 and INA3221 drivers, which are not copied, so none is in
`test/rcbench/`; `test/test_sense.cpp` tests it through the INA219 driver.

The S.BUS decoder (`protocols/sbus/`) is for the S.BUS monitor of
[rx-signals.md](../../docs/design/rx-signals.md). S.BUS is Futaba's serial bus
for receiver channels. The S.BUS job, `src/sbus_job.cpp`, feeds it the bytes
UART0 (universal asynchronous receiver-transmitter 0) receives. It is built as
C11, in the firmware by `src/rx.cmake` and in the host suite by `test/rx.mk`.
Its rcbench test, copied unchanged from rcbench `test/host/test_sbus.c` into
`test/rcbench/`, is `test_sbus.c`, 12 cases.
rcbench's `README.md` and `CMakeLists.txt` of the module are not copied:
this repository lists its sources in its own `.cmake` and `.mk` files.

The move judged from the current (`shared/servo/servo_move.*`) is for the
move timing of [current-sense.md](../../docs/design/current-sense.md), slice
5: `src/move_timer.cpp` feeds it the INA219's readings in the servo tester's
core1 job and keeps what it measured for the SET screen. It is built as C11, in
the firmware by `src/servo.cmake` and in the host suite by `test/servo.mk`.
The servo model (`shared/servo/servo_sim.*`) is built by the host suite only:
`test/test_servo.cpp` and rcbench's test of the move draw the current of a
modelled servo from it. Their rcbench test, copied unchanged from rcbench
`test/host/test_servo_move.c` into `test/rcbench/`, is `test_servo_move.c`,
18 cases. `servo_sim.h` has no `extern "C"` of its own, so the C++ suite
includes it inside one.

The control-surface arithmetic (`shared/servo/servo_rudder.*`), the IMU
(inertial measurement unit) model (`shared/bench/imu.*`) and the WitMotion
decoder (`protocols/witmotion/`) are for the angle meter of
[angle-meter.md](../../docs/design/angle-meter.md), slice 1. `servo_rudder`
gives a deflection from a zero, its travel in mm at the trailing edge and the
largest deflection each side; `imu` keeps the filtered angles, the still time
per axis and the age, fed three angles and a time by `imu_feed_angles()`.
`imu.h` includes `witmotion.h`, so the decoder comes with it; the angle meter
does not use the WitMotion stream. The angle model, `src/angle_model.cpp`, calls
them; nothing calls the model yet. They are built as C11, in the firmware by
`src/angle.cmake` and in the host suite by `test/angle.mk`. `src/angle.cmake`
turns `-Wtype-limits` off for `imu.c`: arm-none-eabi makes `imu_axis_t` one
unsigned byte, so its `(int)axis < 0` is always false, and the firmware's strict
build would stop on the warning. The fix belongs in rcbench: `imu_deg()` and
`imu_still_ms()`, `imu.c` lines 352 and 363, drop `(int)axis < 0` and test
`(unsigned)axis > 2u`, which also refuses a negative value where the enum is
signed, as on the host, where rcbench's `test_imu.c` passes `(imu_axis_t)-1`.
With that copied, the exception goes. Their rcbench tests, copied unchanged from
rcbench `test/host/` into `test/rcbench/`, are `test_servo_rudder.c`, 4 cases,
`test_imu_angles.c`, 7 cases, the test of `imu_feed_angles()`, and
`test_witmotion.c`, 14 cases, with `wit_frame.h`, which builds WitMotion frames
for `test_imu_angles.c`. rcbench's `test_imu.c` and `test_imu_filter.c` include
rcbench's BLE (Bluetooth Low Energy) simulator, its sensor link `imu_sensor.h`
and servo modules that are not ported, so they are adapted, as
`test_out_pwm_map.c` was: their cases that need `imu.h` alone, at commit
`e98345aa`.

- `test_imu.c`, all 9 cases. Its rig's four frame builders, from
  `imu_rig.h`, are copied into the file; the rest of the rig drives the BLE
  simulator. The line of `imu_stale_equals_the_gap_allowance` and the
  `_Static_assert` that compare `IMU_STALE_MS` with `servo_gap.h` are left
  out; the case keeps its check of 300 ms.
  `a_still_yaw_counts_while_pitch_creeps` takes `SERVO_RAUTO_STILL_MS` of
  `servo_rudder_auto.h`, which is defined in the file at its value at that
  commit, 500 ms.
- `test_imu_filter.c`, 10 of 11 cases.
  `a_connect_a_reconnect_and_a_bind_seed_the_filter` is left out: it drives
  `imu_sensor.h`. The lines of
  `the_limits_1_and_50_hold_and_out_of_range_values_are_clamped` that hold
  the sensor link's stored value are left out; the case's other lines stay.

`sbus.h` cites `(NOTICE, "Trademarks")`: that NOTICE file is rcbench's, and
this repository has none. Here the trademark notice of
[README.md](../../README.md#trademark-notice) covers Futaba and S.BUS.

The escwire core (`protocols/escwire/`) and its RP2350 line driver
(`protocols/escwire/rp2350/`) are for the Bluejay and BLHeli_S settings of
[bluejay-blheli-s.md](../../docs/design/bluejay-blheli-s.md), slice 1.
escwire reads and writes an ESC's (electronic speed controller's) settings
over the signal wire. The ESC settings job (`src/escwire_job.h`) runs its
session on the line driver to read a Bluejay or BLHeli_S page, slice 3 of
that note; `test/fake_escwire_port.cpp` puts the simulated ESC `ew_sim.h`
behind the job in the host suite. The core is built as
C11, in the firmware by `src/escwire.cmake` and in the host suite by
`test/escwire.mk`; the line driver is built by the firmware only, with
pioasm assembling `ew_line.pio`. The servo file store
(`shared/logfile/servo_store.*`) is built by the host suite only:
`test_ew_file_load.c` checks the names it gives a settings file. Their
rcbench tests, copied unchanged from rcbench `test/host/` into
`test/rcbench/`, are `test_ew_a.c`, `test_ew_id.c`, `test_ew_map.c`,
`test_ew_guard.c`, `test_ew_session.c`, `test_ew_session_write.c`,
`test_ew_file.c`, `test_ew_file_load.c`, `test_ew_pio.c`, `test_ew_text.c`,
`test_ew_session_text.c` and `test_ew_file_e32.c`, 210 cases, with
`escwire_headers.cpp`, which compiles the core's headers as C++11; the
simulated ESC `ew_sim.h` and `ew_sim_text.h`, the line model
`ew_pio_model.h`, the file rig `ew_file_rig.h` and
`fixtures/ew_line_words.txt`, the words pioasm assembles `ew_line.pio` to,
which CI holds to pioasm as it does the KST and Hitec fixtures. rcbench's
module `README.md` and `CMakeLists.txt` are not copied, nor are
`test_ew_line.c` and its `fake_pico/`, as for the KST and Hitec line
drivers.

The STEP writer and the specs of two modules, at commit `1b6029b8`:

| File here | rcbench path |
| --- | --- |
| `hardware/kicad/tools/step_gen.py` | the same path |
| `hardware/mechanical/ina219_cjmcu219.py`, `boost_5v_en.py` | the same paths |
| `hardware/mechanical/_solid.py`, `_parts.py`, `_font.py`, `_board.py` | the same paths |
| `hardware/kicad/modules/ina219_cjmcu219.py`, `boost_5v_en.py` | the same paths |

It writes STEP (Standard for the Exchange of Product model data) files, ISO
10303-21 with the AP214 schema, from prisms, turned profiles and solids with
flat and curved faces, using the Python standard library only.
[hardware/mechanical/gen_models.py](../../hardware/mechanical/gen_models.py)
writes the model of the board with it. The files of this table have no
`SPDX-License-Identifier` line in rcbench, and none is added here.

`ina219_cjmcu219.py` and `boost_5v_en.py` in `hardware/mechanical/` draw the
INA219 current sensor board and the 5V boost converter board part by part;
`_solid.py`, `_parts.py`, `_font.py` and `_board.py` are the helpers they
import, and the two files in `hardware/kicad/modules/` give each board's
outline, pads and holes. `gen_models.py` writes both models from them. The
directory layout is rcbench's because `_board.py` finds the module files by
their place beside `hardware/mechanical/`.

Files adapted rather than copied start with the line
`Adapted from rcbench <path>, commit <commit>, MIT.`
