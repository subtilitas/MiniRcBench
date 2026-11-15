# The S.BUS part of the host suite, the monitor and the output, included by
# test/Makefile.
#
# One list per feature, so the S.BUS monitor, the servo tester (servo.mk) and
# the programmers (kst.mk, hitec.mk) add their sources without editing a file
# the others edit.
#
#   RX_SRCS        C++ sources linked into the suite: the monitor's core1
#                  job, on the UART0 fake of fakes.cpp; the formatter, S.BUS
#                  MONITOR and RX SIGNALS, the chooser; the S.BUS output's
#                  core1 job, on the TX side of the same fake, its formatter
#                  and S.BUS OUTPUT
#   RX_TESTS       the test files; test_sbus_job.cpp holds runSbusJobTests(),
#                  test_sbus_ui.cpp runSbusUiTests(), test_sbus_out_job.cpp
#                  runSbusOutJobTests(), test_sbus_out_ui.cpp
#                  runSbusOutUiTests(); sbus_line.h is the line model the
#                  monitor's tests put on the fake
#   RX_CSRCS       C sources linked into the suite: rcbench's S.BUS decoder
#                  and encoder, copied into lib/rcbench/
#   RX_INCLUDES    their include directories
#   RX_CTESTS      rcbench's own tests of the C sources, each a program of its
#                  own, built as C11 and run by `make run`
RX_DIR      := ../lib/rcbench/protocols/sbus
RX_SRCS     := ../src/sbus_job.cpp ../src/sbus_view.cpp ../src/ui_sbus.cpp ../src/ui_rx_signals.cpp \
               ../src/sbus_out_job.cpp ../src/sbus_out_view.cpp ../src/ui_sbus_out.cpp
RX_TESTS    := test_sbus_job.cpp test_sbus_ui.cpp test_sbus_out_job.cpp test_sbus_out_ui.cpp
RX_CSRCS    := $(RX_DIR)/sbus.c
RX_INCLUDES := -I$(RX_DIR)/include
RX_CTESTS   := rcbench/test_sbus.c
