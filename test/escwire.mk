# The Bluejay and BLHeli_S settings' part of the host suite, included by
# test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk and sense.mk.
#
#   ESCWIRE_CSRCS           C sources linked into the suite: rcbench's escwire
#                           core and its file store
#                           (shared/logfile/servo_store.c), whose names for a
#                           settings file test_ew_file_load.c checks, copied
#                           into lib/rcbench/. The firmware does not build the
#                           store
#   ESCWIRE_INCLUDES        their include directories
#   ESCWIRE_CTESTS          rcbench's own tests of the C sources, each a
#                           program of its own, built as C11 and run by
#                           `make run`
#   ESCWIRE_CXXTESTS        rcbench's test of the core's headers in C++: built
#                           as C++11 against the C objects and run by
#                           `make run`
#   ESCWIRE_CTEST_DEFINES   the paths test_ew_pio reads: the line's PIO
#                           source and the words pioasm assembles it to
#
# The line driver, lib/rcbench/protocols/escwire/rp2350/ew_line.c, drives
# PIO and DMA and is built by the firmware only (src/escwire.cmake); its
# header is on the include path for `syntax`. Nothing in src/ calls the
# module yet (docs/design/bluejay-blheli-s.md, slice 1).
ESCWIRE_DIR           := ../lib/rcbench/protocols/escwire
ESCWIRE_CSRCS         := $(ESCWIRE_DIR)/ew_wire.c $(ESCWIRE_DIR)/ew_pio.c \
                         $(ESCWIRE_DIR)/ew_a.c $(ESCWIRE_DIR)/ew_text.c \
                         $(ESCWIRE_DIR)/ew_guard.c $(ESCWIRE_DIR)/ew_id.c \
                         $(ESCWIRE_DIR)/ew_map.c $(ESCWIRE_DIR)/ew_map_s.c \
                         $(ESCWIRE_DIR)/ew_map_bj.c $(ESCWIRE_DIR)/ew_map_am.c \
                         $(ESCWIRE_DIR)/ew_map_e32.c \
                         $(ESCWIRE_DIR)/ew_session.c \
                         $(ESCWIRE_DIR)/ew_session_text.c \
                         $(ESCWIRE_DIR)/ew_file.c \
                         $(ESCWIRE_DIR)/ew_file_read.c \
                         $(ESCWIRE_DIR)/ew_file_load.c \
                         ../lib/rcbench/shared/logfile/servo_store.c
ESCWIRE_INCLUDES      := -I$(ESCWIRE_DIR)/include -I$(ESCWIRE_DIR)/rp2350 \
                         -I../lib/rcbench/shared/logfile/include
ESCWIRE_CTESTS        := rcbench/test_ew_a.c rcbench/test_ew_id.c \
                         rcbench/test_ew_map.c rcbench/test_ew_guard.c \
                         rcbench/test_ew_session.c \
                         rcbench/test_ew_session_write.c \
                         rcbench/test_ew_file.c rcbench/test_ew_file_load.c \
                         rcbench/test_ew_pio.c rcbench/test_ew_text.c \
                         rcbench/test_ew_session_text.c \
                         rcbench/test_ew_file_e32.c
ESCWIRE_CXXTESTS      := rcbench/escwire_headers.cpp
ESCWIRE_CTEST_DEFINES := -DEW_LINE_PIO=\"$(CURDIR)/$(ESCWIRE_DIR)/rp2350/ew_line.pio\" \
                         -DEW_LINE_WORDS=\"$(CURDIR)/rcbench/fixtures/ew_line_words.txt\"
