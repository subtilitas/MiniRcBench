# The S.BUS monitor's part of the host suite, included by test/Makefile.
#
# One list per feature, so the S.BUS monitor, the servo tester (servo.mk) and
# the programmers (kst.mk, hitec.mk) add their sources without editing a file
# the others edit.
#
#   RX_CSRCS       C sources linked into the suite: rcbench's S.BUS decoder,
#                  copied into lib/rcbench/
#   RX_INCLUDES    their include directories
#   RX_CTESTS      rcbench's own tests of the C sources, each a program of its
#                  own, built as C11 and run by `make run`
RX_DIR      := ../lib/rcbench/protocols/sbus
RX_CSRCS    := $(RX_DIR)/sbus.c
RX_INCLUDES := -I$(RX_DIR)/include
RX_CTESTS   := rcbench/test_sbus.c
