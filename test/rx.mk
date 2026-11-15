# The S.BUS monitor's part of the host suite, included by test/Makefile.
#
# One list per feature, so the S.BUS monitor, the servo tester (servo.mk) and
# the programmers (kst.mk, hitec.mk) add their sources without editing a file
# the others edit.
#
#   RX_SRCS        C++ sources linked into the suite: the core1 job, on the
#                  UART0 fake of fakes.cpp
#   RX_TESTS       the test files; test_sbus_job.cpp holds runSbusJobTests(),
#                  sbus_line.h is the line model it puts on the fake
#   RX_CSRCS       C sources linked into the suite: rcbench's S.BUS decoder,
#                  copied into lib/rcbench/
#   RX_INCLUDES    their include directories
#   RX_CTESTS      rcbench's own tests of the C sources, each a program of its
#                  own, built as C11 and run by `make run`
RX_DIR      := ../lib/rcbench/protocols/sbus
RX_SRCS     := ../src/sbus_job.cpp
RX_TESTS    := test_sbus_job.cpp
RX_CSRCS    := $(RX_DIR)/sbus.c
RX_INCLUDES := -I$(RX_DIR)/include
RX_CTESTS   := rcbench/test_sbus.c
