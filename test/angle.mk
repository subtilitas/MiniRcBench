# The angle meter's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk, sense.mk and
# escwire.mk.
#
#   ANGLE_SRCS      C++ sources linked into the suite: the QMI8658 driver
#   ANGLE_TESTS     the test files; test_qmi8658.cpp holds runQmi8658Tests().
#                   The register fake of the QMI8658 is a part of
#                   fake_i2c.cpp, which sense.mk lists
#   ANGLE_CSRCS     C sources linked into the suite: rcbench's control-surface
#                   arithmetic (servo_rudder.c), its IMU (inertial measurement
#                   unit) model (imu.c) and the WitMotion decoder imu.h
#                   includes (witmotion.c), copied into lib/rcbench/
#   ANGLE_INCLUDES  their include directories
#   ANGLE_CTESTS    rcbench's own tests of the C sources, each a program of
#                   its own, built as C11 and run by `make run`.
#                   test_imu_angles.c tests imu_feed_angles(), the model fed
#                   three angles and a time; wit_frame.h, beside it, builds
#                   the WitMotion frames it compares with
#
# test_qmi8658.cpp includes src/cst328.cpp in a namespace of its own, as
# test_sense.cpp does, to run touch, the current sensor and the IMU on one
# bus. Nothing in src/ calls the C sources yet, and the firmware does not
# build them (docs/design/angle-meter.md, slice 1).
ANGLE_SRCS     := ../src/qmi8658.cpp
ANGLE_TESTS    := test_qmi8658.cpp
ANGLE_CSRCS    := ../lib/rcbench/shared/servo/servo_rudder.c \
                  ../lib/rcbench/shared/bench/imu.c \
                  ../lib/rcbench/protocols/witmotion/witmotion.c
ANGLE_INCLUDES := -I../lib/rcbench/shared/servo/include \
                  -I../lib/rcbench/shared/bench/include \
                  -I../lib/rcbench/protocols/witmotion/include
ANGLE_CTESTS   := rcbench/test_servo_rudder.c rcbench/test_imu_angles.c \
                  rcbench/test_witmotion.c
