# The angle meter's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk and sense.mk.
#
#   ANGLE_SRCS   C++ sources linked into the suite: the QMI8658 driver
#   ANGLE_TESTS  the test files; test_qmi8658.cpp holds runQmi8658Tests().
#                The register fake of the QMI8658 is a part of fake_i2c.cpp,
#                which sense.mk lists
#
# test_qmi8658.cpp includes src/cst328.cpp in a namespace of its own, as
# test_sense.cpp does, to run touch, the current sensor and the IMU on one
# bus. docs/design/angle-meter.md lists what later slices add.
ANGLE_SRCS  := ../src/qmi8658.cpp
ANGLE_TESTS := test_qmi8658.cpp
