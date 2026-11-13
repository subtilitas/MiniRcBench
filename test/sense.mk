# The current sensor's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk and hitec.mk.
#
#   SENSE_SRCS      C++ sources linked into the suite: the INA219 driver
#   SENSE_CSRCS     C sources linked into the suite: rcbench's sensor bus,
#                   copied into lib/rcbench/
#   SENSE_INCLUDES  their include directories
#   SENSE_TESTS     the test files; test_sense.cpp holds runSenseTests().
#                   fake_i2c.cpp is the I2C1 bus every host test runs on: the
#                   parts on it by address, an INA219 and a CST328 among them
#
# rcbench has no test of sense_bus.c of its own: its tests reach it through
# the INA228 and INA3221 drivers, which are not copied. test_sense.cpp tests
# it through the INA219 driver. src/cst328.cpp stays in the device sources;
# test_sense.cpp includes it, in a namespace of its own, to run touch and the
# sensor on one bus beside the scripted touch of fakes.cpp.
SENSE_SRCS     := ../src/sense.cpp
SENSE_CSRCS    := ../lib/rcbench/shared/sense/sense_bus.c
SENSE_INCLUDES := -I../lib/rcbench/shared/sense/include
SENSE_TESTS    := test_sense.cpp fake_i2c.cpp
