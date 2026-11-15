# The angle meter's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk, sense.mk and
# escwire.mk.
#
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
# Nothing in src/ calls these files yet, and the firmware does not build
# them (docs/design/angle-meter.md, slice 1).
ANGLE_CSRCS    := ../lib/rcbench/shared/servo/servo_rudder.c \
                  ../lib/rcbench/shared/bench/imu.c \
                  ../lib/rcbench/protocols/witmotion/witmotion.c
ANGLE_INCLUDES := -I../lib/rcbench/shared/servo/include \
                  -I../lib/rcbench/shared/bench/include \
                  -I../lib/rcbench/protocols/witmotion/include
ANGLE_CTESTS   := rcbench/test_servo_rudder.c rcbench/test_imu_angles.c \
                  rcbench/test_witmotion.c
