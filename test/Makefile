# Host tests for MiniRcBench.
#
#   make          syntax-check, build and run
#   make build    build only
#   make syntax   typecheck the device-only sources
#   make pioemu   run the SDIO PIO programs in a cycle-level model
#   make bbcheck  round-trip a log through the upstream blackbox_decode
#   make previews rebuild the screenshots in docs/ from this run's frames
#   make wiki     rebuild the wiki's screenshots and check its links
#   make coverage line coverage of src/ from an instrumented build in cov/
#   make kstcount the KST card phases over FatFs, checked against the design note
#   make kstcount-write  rewrite that table
#   make clean
#
# Needs nothing but a C++17 compiler and the C compiler beside it: the firmware
# is compiled against the stubs in test/stubs/, which mirror the Pico SDK
# signatures.
#
# ui_am32.cpp is deliberately absent from SRCS -- test_ui.cpp #includes it so
# the tests can reach its file-static state. Compiling it here as well would
# be a duplicate-symbol error. core1_job.cpp is absent for the same reason:
# test_claim.cpp #includes it to put fake jobs in its job table.

CXX      ?= g++
CXXFLAGS ?= -std=gnu++17 -O1 -g
CFLAGS   ?= -std=c11 -O1 -g
WARN     := -Wall -Wextra -Wshadow -Wno-unused-parameter
INCLUDES := -Istubs -I../src -I.

# EXTRA_DEFINES lets CI rebuild the suite with the pin macros overridden,
# which is what makes test_settings.cpp's testPinDefaults() more than a
# tautology: a descriptor that carried literals would pass at the defaults and
# fail the moment a macro says something else.
#
#   make EXTRA_DEFINES="-DDSHOT_PIN_LCD_2_8=28 -DKISS_PIN_LCD_2_8=29" run
DEFINES := $(EXTRA_DEFINES)

# The servo tester and the KST programmer list their own sources, so the two
# never edit this file for that. @see servo.mk, kst.mk
include servo.mk kst.mk

# Each feature's C sources, their include directories and their C test
# programs. A feature with none leaves its variables undefined, and they
# expand to nothing.
FEATURE_CSRCS := $(SERVO_CSRCS) $(KST_CSRCS)
FEATURE_CTESTS := $(SERVO_CTESTS) $(KST_CTESTS)
FEATURE_CTEST_DEFINES := $(SERVO_CTEST_DEFINES) $(KST_CTEST_DEFINES)
INCLUDES += $(SERVO_INCLUDES) $(KST_INCLUDES)

SRCS := test_main.cpp test_am32.cpp test_ui.cpp test_gfx.cpp test_kiss.cpp \
        test_blackbox.cpp test_log_ring.cpp test_settings.cpp \
        test_usb_msc.cpp test_dshot_frame.cpp test_dshot_telem.cpp \
        test_dshot_edt.cpp test_sdio.cpp test_claim.cpp test_widgets.cpp test_ids.cpp test_menu.cpp test_backup_ring.cpp test_sd_cache.cpp \
        fakes.cpp \
        ../src/gfx.cpp ../src/ui.cpp ../src/ui_settings.cpp ../src/ui_menu.cpp ../src/ui_registry.cpp ../src/ui_sdlog.cpp ../src/theme.cpp \
        ../src/ui_input.cpp ../src/ui_widgets.cpp ../src/ui_list.cpp \
        ../src/ui_hold.cpp ../src/board_desc.cpp \
        ../src/settings.cpp ../src/am32_eeprom.cpp ../src/card_dir.cpp ../src/backup_ring.cpp ../src/am32_backup.cpp \
        ../src/kiss_telem.cpp ../src/esc_merge.cpp ../src/blackbox_encode.cpp \
        ../src/log_ring.cpp ../src/pin_claim.cpp \
        $(SERVO_TESTS) $(SERVO_SRCS) $(KST_TESTS) $(KST_SRCS)

# Sources the tests cannot link -- they drive real hardware -- but which still
# have to compile. CI's "Config permutations" job already builds all of src/
# with -Werror, so this is not new coverage; it moves the feedback from a CI
# round-trip to the same `make` you were going to run anyway.
#
# The touch driver is here rather than in SRCS: the host suite fakes
# touchInit/touchPoll outright so that tests can drive presses.
DEVICE_SRCS := ../src/esc_task.cpp ../src/st7789.cpp \
               ../src/usb_dev.cpp ../src/usb_msc.cpp ../src/usb_descriptors.c \
               ../src/cst328.cpp ../src/am32_bl.cpp ../src/sd_log.cpp \
               ../src/pio_uart_rx.cpp \
               ../src/settings_flash.cpp ../src/main.cpp ../src/card_cache.cpp \
               $(SERVO_DEVICE_SRCS) $(KST_DEVICE_SRCS)

# The DShot core in src/dshot/ and the SD card protocol layers in src/sdio/
# are C11, and are compiled here as C, by $(CC), into objects the suite links.
# The firmware compiles them as C, so the suite tests the same language: C and
# C++ differ in the implicit conversions this code leans on (integer to enum,
# int to bool), and a C++ build could pass code the C build reads differently.
# Their headers carry extern "C", so the C++ tests call them as the C++
# firmware does. The SDIO hardware layer (sd_pio.c, sd_card_glue.c) drives
# PIO and DMA and is not here.
CSRCS := ../src/dshot/dshot_frame.c ../src/dshot/dshot_telem.c \
         ../src/dshot/dshot_edt.c \
         ../src/sdio/sd_proto.c ../src/sdio/sd_host.c ../src/sdio/sd_cache.c \
         $(FEATURE_CSRCS)
OBJ_DIR := obj
COBJS := $(addprefix $(OBJ_DIR)/,$(notdir $(CSRCS:.c=.o)))
FEATURE_COBJS := $(addprefix $(OBJ_DIR)/,$(notdir $(FEATURE_CSRCS:.c=.o)))
# The features' C test programs, one binary each in obj/.
CTEST_BINS := $(addprefix $(OBJ_DIR)/,$(notdir $(FEATURE_CTESTS:.c=)))
# Where the features' C files are found by name; the rules below name them
# by file only.
vpath %.c $(sort $(dir $(FEATURE_CSRCS) $(FEATURE_CTESTS)))

BIN := dshot_tests

.PHONY: all build run syntax pioemu bbcheck previews wiki coverage kstcount kstcount-write clean

all: syntax run pioemu bbcheck

build: $(BIN)

# The binary depends on EXTRA_DEFINES, which make cannot see in a timestamp.
# Rebuilding unconditionally costs a couple of seconds and removes a whole
# class of "the tests passed for the other defines" confusion.
.PHONY: $(BIN)
$(BIN): $(SRCS) $(COBJS) $(wildcard ../src/*.h) $(wildcard ../src/sdio/*.h) $(wildcard *.h)
	$(CXX) $(CXXFLAGS) $(WARN) $(INCLUDES) $(DEFINES) -o $@ $(SRCS) $(COBJS)

$(OBJ_DIR)/%.o: ../src/dshot/%.c ../src/dshot/dshot.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(WARN) -Werror -c -o $@ $<

$(OBJ_DIR)/%.o: ../src/sdio/%.c ../src/sdio/sd_proto.h ../src/sdio/sd_host.h
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(WARN) -Werror -c -o $@ $<

# A feature's C source, found through vpath.
$(FEATURE_COBJS): $(OBJ_DIR)/%.o: %.c
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(WARN) -Werror $(SERVO_INCLUDES) $(KST_INCLUDES) -c -o $@ $<

# A feature's C test program, linked against the feature's C objects. Its own
# directory is on the include path, for the harness beside it.
$(CTEST_BINS): $(OBJ_DIR)/%: %.c $(FEATURE_COBJS)
	@mkdir -p $(OBJ_DIR)
	$(CC) $(CFLAGS) $(WARN) -Werror -I$(dir $<) $(SERVO_INCLUDES) $(KST_INCLUDES) \
		$(FEATURE_CTEST_DEFINES) -o $@ $< $(FEATURE_COBJS) -lm

# The suite, then each C test program. A program prints a line per case; its
# summary line is shown, and the whole output when it fails.
run: $(BIN) $(CTEST_BINS)
	./$(BIN)
	@for t in $(CTEST_BINS); do \
		out=$$(./$$t) || { echo "$$out"; exit 1; }; \
		echo "$$out" | tail -1; \
	done

# Every device source, plus both settings of the two feature switches so the
# disabled paths cannot rot unnoticed.
# -Werror, matching the firmware build. Without it this target was strictly
# weaker than CI: usb_dev.cpp typechecked here and failed the real build twice
# over -- once on a struct the stub defined too loosely, and once on a partial
# designated initialiser that only -Wextra -Werror objects to. A local check
# that is easier to pass than the one that matters is a check that sends you to
# CI to find out.
syntax:
	@for f in $(DEVICE_SRCS); do \
		$(CXX) $(CXXFLAGS) $(WARN) -Werror $(INCLUDES) $(DEFINES) \
			-fsyntax-only $$f || exit 1; \
	done
	@$(CXX) $(CXXFLAGS) $(WARN) $(INCLUDES) $(DEFINES) -DKISS_TELEM_ENABLE=0 \
		-fsyntax-only ../src/esc_task.cpp || exit 1
	@$(CXX) $(CXXFLAGS) $(WARN) $(INCLUDES) $(DEFINES) -DSD_LOG_ENABLE=0 \
		-fsyntax-only ../src/sd_log.cpp || exit 1
	@echo "device sources typecheck"

# The two SDIO PIO programs in src/sdio/sd_pio.pio, assembled from that file
# and run one system clock at a time against a model of an SD card: CMD0, R1
# and R2 responses, single and multiple block reads, CMD12 and a write with its
# CRC status token, at 400 kHz, 10 MHz and 25 MHz. Python 3 and $(CC), which
# builds sd_proto.c for the run. A model, not proof of hardware timing: the
# script's docstring lists what it covers and what it does not.
pioemu:
	python3 pio_sdio_emu.py --cc "$(CC)"

# Round-trip a generated log through the upstream decoder.
#
# The unit tests only prove the encoder agrees with my reading of the format.
# This proves it agrees with the tool people will open the log in -- and those
# came apart once already: the I-frame history-rotation bug passed every unit
# test and produced a file that decoded without complaint, but whose time column
# stalled every 32 frames.
#
# Needs blackbox-tools. Skipped with a notice when absent rather than failing,
# so a bare checkout still runs `make`. CI builds it and runs this for real.
BBDECODE ?= $(shell command -v blackbox_decode 2>/dev/null)

bbcheck:
	@if [ -z "$(BBDECODE)" ]; then \
		echo "bbcheck: blackbox_decode not found, skipping"; \
		echo "  git clone https://github.com/betaflight/blackbox-tools"; \
		echo "  make -C blackbox-tools obj/blackbox_decode   # not plain 'make':"; \
		echo "  the default target also builds blackbox_render, which needs cairo"; \
	else \
		$(CXX) $(CXXFLAGS) $(INCLUDES) $(DEFINES) -o gen_blackbox_log \
			gen_blackbox_log.cpp ../src/blackbox_encode.cpp -lm && \
		python3 check_blackbox.py "$(BBDECODE)" ./gen_blackbox_log; \
	fi

# Restitch docs/*.png from the frames the run above just wrote. Separate target
# rather than part of `all`, so a test run does not leave the working tree
# dirty -- but one command away, because the manual version of this step is
# what let the published screenshots go stale for several releases.
previews: run
	python3 ../tools/make_previews.py

# Render the wiki's screenshots and check every page still resolves. Same
# relationship to wiki/ that `previews` has to docs/, and the same reason for
# existing: the manual claims its pictures come from the real UI code, and a
# claim nobody can re-run in one command is a claim that goes stale.
wiki: run
	python3 ../tools/make_previews.py --wiki
	python3 ../tools/check_wiki.py

# Line coverage of the src/ files the suite compiles. The C files from
# lib/rcbench/ are instrumented with the rest, but coverage.py counts src/
# only, so they are not in the figure. The instrumented copy
# builds in cov/ at -O0, so each counter maps to one source line, and leaves
# $(BIN) and `make run` unchanged. The run starts from an empty cov/: counters
# from an earlier run or a removed source would otherwise add to this one.
# `python3 ../tools/coverage.py` writes the figure into README.md; --check is
# what CI runs.
COV_DIR := cov

coverage:
	rm -rf $(COV_DIR) && mkdir -p $(COV_DIR)
	for f in $(CSRCS); do \
		$(CC) -std=c11 -O0 -g --coverage $(WARN) $(SERVO_INCLUDES) $(KST_INCLUDES) \
			-c -o $(COV_DIR)/$$(basename $$f .c).o $$f || exit 1; \
	done
	$(CXX) -std=gnu++17 -O0 -g --coverage $(WARN) $(INCLUDES) $(DEFINES) \
		-o $(COV_DIR)/$(BIN) $(SRCS) $(addprefix $(COV_DIR)/,$(notdir $(CSRCS:.c=.o)))
	./$(COV_DIR)/$(BIN)
	python3 ../tools/coverage.py --print

# The KST card phases counted over FatFs R0.15 itself, from the SD library at
# the commit CMakeLists.txt pins, with the library's ffconf.h: the numbers
# behind the card-phase cap in docs/design/kst-programming.md. Fetches the
# library into obj/ once; needs git and the network for that. `kstcount`
# fails when the table in the design note differs from the counts;
# `kstcount-write` rewrites it.
FATFS_REV := $(shell awk '/Declare.no_os_fatfs/{f=1} f&&/GIT_TAG/{print $$2; exit}' ../CMakeLists.txt)
FATFS_DIR := $(OBJ_DIR)/fatfs-$(FATFS_REV)
FATFS_SRC := $(FATFS_DIR)/src/ff15/source

$(FATFS_SRC)/ff.c:
	rm -rf $(FATFS_DIR) && mkdir -p $(FATFS_DIR)
	git -C $(FATFS_DIR) init -q
	git -C $(FATFS_DIR) fetch -q --depth 1 \
		https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git $(FATFS_REV)
	git -C $(FATFS_DIR) checkout -q FETCH_HEAD

KST_NOTE := ../docs/design/kst-programming.md

KST_COUNT_SRCS := ../src/kst_files.cpp ../src/card_dir.cpp ../src/backup_ring.cpp \
                  ../src/am32_backup.cpp ../src/am32_eeprom.cpp

$(OBJ_DIR)/kst_card_count: kst_card_count.cpp $(KST_COUNT_SRCS) ../src/kst_files.h ../src/card_dir.h ../src/backup_ring.h ../src/am32_backup.h $(KST_CSRCS) $(FATFS_SRC)/ff.c
	mkdir -p $(OBJ_DIR)/kstcount
	for f in $(FATFS_SRC)/ff.c $(FATFS_SRC)/ffunicode.c $(FATFS_SRC)/ffsystem.c $(KST_CSRCS); do \
		$(CC) $(CFLAGS) -w -I$(FATFS_SRC) -I$(FATFS_DIR)/src/include $(KST_INCLUDES) \
			-c -o $(OBJ_DIR)/kstcount/$$(basename $$f .c).o $$f || exit 1; \
	done
	$(CXX) $(CXXFLAGS) $(WARN) -I$(FATFS_SRC) -I$(FATFS_DIR)/src/include -I../src $(KST_INCLUDES) \
		-o $@ kst_card_count.cpp $(KST_COUNT_SRCS) $(OBJ_DIR)/kstcount/*.o

kstcount: $(OBJ_DIR)/kst_card_count
	./$(OBJ_DIR)/kst_card_count --check $(KST_NOTE)

kstcount-write: $(OBJ_DIR)/kst_card_count
	./$(OBJ_DIR)/kst_card_count --write $(KST_NOTE)

clean:
	rm -f $(BIN) gen_blackbox_log *.ppm *.BFL
	rm -rf $(COV_DIR) $(OBJ_DIR)
