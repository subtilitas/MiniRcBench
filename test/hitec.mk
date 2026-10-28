# The Hitec programmer's part of the host suite, included by test/Makefile.
#
# One list per feature, so the Hitec programmer, the KST programmer (kst.mk)
# and the servo tester (servo.mk) add their sources without editing a file
# the others edit.
#
#   HITEC_SRCS           C++ sources linked into the suite: the core1 job and
#                        the files on the card
#   HITEC_TESTS          the test files; test_hitec.cpp holds runHitecTests().
#                        fake_hitec_line.cpp is the line the job drives: a
#                        simulated servo behind hitec_line_port.h
#   HITEC_DEVICE_SRCS    sources the suite cannot link, typechecked by `syntax`
#   HITEC_CSRCS          C sources linked into the suite: rcbench's Hitec core,
#                        its servo file text and the CRC that text uses,
#                        copied into lib/rcbench/
#   HITEC_INCLUDES       their include directories
#   HITEC_CTESTS         rcbench's own tests of the C sources, each a program
#                        of its own, built as C11 and run by `make run`
#   HITEC_CXXTESTS       rcbench's test of the core's headers in C++: built as
#                        C++11 against the C objects and run by `make run`
#   HITEC_CTEST_DEFINES  the paths test_hitec_pio reads: the line's PIO source
#                        and the words pioasm assembles it to
#
# The line driver, lib/rcbench/protocols/hitec/rp2350/hitec_line.c, drives
# PIO and DMA and is built by the firmware only (src/hitec.cmake), as is
# src/hitec_line_port.cpp, the job's line on it; its header is on the include
# path for the error codes and for `syntax`.
HITEC_DIR           := ../lib/rcbench/protocols/hitec
HITEC_SRCS          := ../src/hitec_job.cpp ../src/hitec_files.cpp
HITEC_TESTS         := test_hitec.cpp test_hitec_job.cpp test_hitec_files.cpp \
                       fake_hitec_line.cpp
HITEC_DEVICE_SRCS   := ../src/hitec_line_port.cpp
HITEC_CSRCS         := $(HITEC_DIR)/hitec_wire.c $(HITEC_DIR)/hitec_pio.c \
                       $(HITEC_DIR)/hitec_pre.c $(HITEC_DIR)/hitec_reg.c \
                       $(HITEC_DIR)/hitec_session.c \
                       ../lib/rcbench/shared/bench/hitec_card.c \
                       ../lib/rcbench/shared/link/link_crc.c
HITEC_INCLUDES      := -I$(HITEC_DIR)/include -I$(HITEC_DIR)/rp2350 \
                       -I../lib/rcbench/shared/bench/include \
                       -I../lib/rcbench/shared/link/include
HITEC_CTESTS        := rcbench/test_hitec_wire.c rcbench/test_hitec_pre.c \
                       rcbench/test_hitec_pio.c rcbench/test_hitec_reg.c \
                       rcbench/test_hitec_reg_plan.c \
                       rcbench/test_hitec_session.c \
                       rcbench/test_hitec_session_write.c \
                       rcbench/test_hitec_card.c
HITEC_CXXTESTS      := rcbench/hitec_headers.cpp
HITEC_CTEST_DEFINES := -DHITEC_LINE_PIO=\"$(CURDIR)/$(HITEC_DIR)/rp2350/hitec_line.pio\" \
                       -DHITEC_LINE_WORDS=\"$(CURDIR)/rcbench/fixtures/hitec_line_words.txt\"
