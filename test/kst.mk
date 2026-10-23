# The KST programmer's part of the host suite, included by test/Makefile.
#
# One list per feature, so the KST programmer and the servo tester (servo.mk)
# add their sources without editing a file the other edits.
#
#   KST_SRCS            C++ sources linked into the suite. kst_job.cpp is not
#                       one: test_kst_job.cpp includes it, to count calls the
#                       job must never make
#   KST_CSRCS           C sources linked into the suite: rcbench's KST core
#                       and its file store, copied into lib/rcbench/
#   KST_INCLUDES        their include directories
#   KST_TESTS           the test files; test_kst.cpp holds runKstTests().
#                       fake_kst_line.cpp is the line the job drives: the
#                       simulated servo behind kst_line_port.h.
#                       fake_fatfs.cpp is the card kst_files.cpp writes: an
#                       in-memory FAT volume behind the FatFs calls
#   KST_CTESTS          rcbench's own tests of the C sources, each a program
#                       of its own, built as C11 and run by `make run`
#   KST_CTEST_DEFINES   the paths test_kst_pio reads: the line's PIO source
#                       and the words pioasm assembles it to
#   KST_DEVICE_SRCS     sources the suite cannot link, typechecked by `syntax`
#
# The line driver, lib/rcbench/protocols/kst/rp2350/kst_line.c, drives PIO
# and DMA and is built by the firmware only (src/kst.cmake), as is
# src/kst_line_port.cpp, the job's line on it; its header is on the include
# path for the error codes and for `syntax`.
KST_DIR           := ../lib/rcbench/protocols/kst
KST_SRCS          := ../src/ui_kst.cpp ../src/kst_seek.cpp ../src/kst_files.cpp \
                     ../src/kst_view.cpp
KST_CSRCS         := $(KST_DIR)/kst_wire.c $(KST_DIR)/kst_reg.c \
                     $(KST_DIR)/kst_limits.c $(KST_DIR)/kst_plan.c \
                     $(KST_DIR)/kst_session.c $(KST_DIR)/kst_pio.c \
                     $(KST_DIR)/kst_file.c \
                     ../lib/rcbench/shared/logfile/kst_store.c
KST_INCLUDES      := -I$(KST_DIR)/include -I$(KST_DIR)/rp2350 \
                     -I../lib/rcbench/shared/logfile/include
KST_TESTS         := test_kst.cpp test_kst_seek.cpp test_kst_job.cpp \
                     test_kst_view.cpp test_kst_ui.cpp \
                     test_kst_files.cpp fake_kst_line.cpp fake_fatfs.cpp
KST_CTESTS        := rcbench/test_kst_wire.c rcbench/test_kst_reg.c \
                     rcbench/test_kst_plan.c rcbench/test_kst_session.c \
                     rcbench/test_kst_pio.c rcbench/test_kst_file.c \
                     rcbench/test_kst_store.c
KST_CTEST_DEFINES := -DKST_LINE_PIO=\"$(CURDIR)/$(KST_DIR)/rp2350/kst_line.pio\" \
                     -DKST_LINE_WORDS=\"$(CURDIR)/rcbench/fixtures/kst_line_words.txt\"
KST_DEVICE_SRCS   := ../src/kst_line_port.cpp
