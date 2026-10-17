# MiniRcBench — native Pico SDK build.
#
#   cmake -B build -G Ninja -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .
#   ninja -C build
#
# Produces build/MiniRcBench-touch-lcd-2.8.uf2, for the Waveshare
# RP2350-Touch-LCD-2.8, the one board this firmware supports.
#
# PICO_PLATFORM selects the core. Only rp2350-arm-s (Cortex-M33) is supported and
# built: nothing here is built or tested for the Hazard3 RISC-V cores
# (rp2350-riscv).
#
# Dependencies are fetched at configure time rather than vendored, so the tree
# stays free of third-party source, and pinned to exact tags so a build here
# matches a build anywhere. That pinning is the whole point: an unpinned
# FetchContent is a lock file that silently rewrites itself. The DShot driver in
# src/dshot/ is not a dependency in that sense: it comes from rcbench, which has
# the same copyright holder and licence, and it is part of this tree.

# 3.18, not 3.13: SOURCE_SUBDIR inside FetchContent_Declare is only honoured by
# FetchContent_MakeAvailable from 3.18 onwards. On 3.13-3.17 it is silently
# ignored, add_subdirectory runs against the dependency's repo root, no targets
# are defined, and the failure surfaces much later as a missing ff.h -- which is
# precisely the confusion the SOURCE_SUBDIR comment below exists to prevent.
cmake_minimum_required(VERSION 3.18)

# SD_DEBUG=ON turns on the FatFs driver's own tracing, which prints to USB
# serial. Off by default: it is noisy and costs flash, but when a card is not
# detected it is the difference between reading the driver's account of what
# happened and guessing.
option(SD_DEBUG "Enable verbose SD driver logging over USB serial" OFF)

# There is no board to choose. A BOARD naming any other target is refused
# rather than ignored: a script that still asks for another board would
# otherwise get a 2.8" image without a word.
if(DEFINED BOARD AND NOT "${BOARD}" STREQUAL "BOARD_RP2350_TOUCH_LCD_2_8")
    message(FATAL_ERROR
        "MiniRcBench builds for the Waveshare RP2350-Touch-LCD-2.8 only, and "
        "BOARD=${BOARD} names another target. Configure without -DBOARD, in a "
        "fresh build directory.")
endif()

# Which GPIOs the ESC signal and telemetry wires land on. Empty means "leave
# config.h's default alone".
#
#   cmake -B build -DDSHOT_PIN_LCD_2_8=28 -DKISS_PIN_LCD_2_8=29 ...
#
# The board brings out GP28 (J4 pin 11) and GP29 (J4 pin 12) and nothing else,
# so those are the only two choices and the two wires take one each. An
# override outside the board's free mask, or one putting both wires on the same
# pin, is a static_assert in the descriptor -- it fails the build, it is not
# repaired at run time. The suffix names the board, as the image's file name
# does.
set(DSHOT_PIN_LCD_2_8 "" CACHE STRING "ESC signal GPIO; blank = config.h default")
set(KISS_PIN_LCD_2_8  "" CACHE STRING "Telemetry GPIO; blank = config.h default")

# Spellings this build does not take, refused rather than ignored. An option
# CMake accepts and nothing reads configures cleanly and leaves the image on
# the default pin, which is a build that lies about which pin it made.
foreach(oldopt DSHOT_PIN KISS_PIN DSHOT_PIN_LCD_2 KISS_PIN_LCD_2)
    if(NOT "${${oldopt}}" STREQUAL "")
        message(FATAL_ERROR
            "${oldopt} is not an option of this build. Use "
            "-DDSHOT_PIN_LCD_2_8=n for the ESC signal GPIO and "
            "-DKISS_PIN_LCD_2_8=n for the telemetry GPIO.")
    endif()
endforeach()


# Pinned by commit, not by tag. A tag is a movable reference, and README.md
# calls an unpinned FetchContent "a lock file that silently rewrites itself" --
# which is just as true of one that pins to something upstream can move.
# The comment records which tag each SHA was, so a bump is still readable.
set(PICO_SDK_FETCH_FROM_GIT_TAG bddd20f928ce76142793bef434d4f75f4af6e433)  # 2.1.1
include(pico_sdk_import.cmake)

# The version lives in src/version.h, where the firmware reads it too, so the
# build and the image cannot disagree. release.yml holds the tag to it.
file(STRINGS ${CMAKE_CURRENT_LIST_DIR}/src/version.h FW_VERSION_LINES
     REGEX "^#define FW_VERSION_(MAJOR|MINOR|PATCH) +[0-9]+")
foreach(part MAJOR MINOR PATCH)
    string(REGEX MATCH "FW_VERSION_${part} +([0-9]+)" _m "${FW_VERSION_LINES}")
    if(NOT _m)
        message(FATAL_ERROR "src/version.h defines no FW_VERSION_${part}")
    endif()
    set(FW_VERSION_${part} ${CMAKE_MATCH_1})
endforeach()

project(MiniRcBench
    VERSION ${FW_VERSION_MAJOR}.${FW_VERSION_MINOR}.${FW_VERSION_PATCH}
    LANGUAGES C CXX ASM)
message(STATUS "MiniRcBench ${PROJECT_VERSION}")

# An empty CMAKE_BUILD_TYPE is not a default build: it is *no* per-config flags
# at all -- no optimisation, no -g -- and neither CMake nor the pinned SDK
# fills it in. An image built that way is unoptimised, and CI's check of the
# image's DWARF compile units can never pass against it. Release opts in to both: the SDK's set_flags adds -O3 and,
# via PICO_DEBUG_INFO_IN_RELEASE (default ON), keeps the debug info in the ELF.
# The UF2 is unaffected by debug info; only the ELF carries it.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

pico_sdk_init()

# RP2350 only: the pin maps, the powman-era SD driver choice and the SDIO PIO
# constraints all assume it. Without this a bare `cmake -B build .` takes the
# SDK's defaults -- pico / rp2040 -- and builds something that cannot work, with
# no diagnostic anywhere until it fails on hardware.
#
# After pico_sdk_init() rather than before: PICO_RP2350 is one of the variables
# that call defines, so checking earlier fails for every configuration.
if(NOT PICO_RP2350)
    message(FATAL_ERROR
        "MiniRcBench targets RP2350 only, and this build is configured for "
        "'${PICO_PLATFORM}'. Configure with:\n"
        "  cmake -B build -G Ninja -DPICO_BOARD=pico2 -DPICO_PLATFORM=rp2350-arm-s .")
endif()

include(FetchContent)

# FatFs, its glue and the card layer for the microSD slot, from
# no-OS-FatFS-SD-SDIO-SPI-RPi-Pico: Apache-2.0, with FatFs under its own
# BSD-style licence.
#
# The SDIO-SPI repo, not the older SPI-only one: that one's rtc.c includes
# hardware/rtc.h, which does not exist on RP2350 -- the RP2040 RTC was replaced
# by powman. It builds fine for RP2040 and fails outright here.
#
# The library's own src/CMakeLists.txt is not used. Its SDIO driver derives
# from GPL-3.0 code, and that CMakeLists compiles it, and assembles its PIO
# program, into every image. So the source is only fetched -- SOURCE_SUBDIR
# names a directory with no CMakeLists.txt, which FetchContent_MakeAvailable
# populates and does not add -- and the target below is rebuilt from the
# library's other files. The SDIO driver in src/sdio/ is MIT and written from
# the SD specification; see docs/design/sdio-driver.md.
FetchContent_Declare(no_os_fatfs
    GIT_REPOSITORY https://github.com/carlk3/no-OS-FatFS-SD-SDIO-SPI-RPi-Pico.git
    GIT_TAG        6c644cfc3ab03c161fee2dd7be4877e5b832fa71  # v3.6.2
    SOURCE_SUBDIR  fetch-only-no-cmake
)
FetchContent_MakeAvailable(no_os_fatfs)

# A copy of the library's src/ without the SDIO driver and the two GPL
# projects it carries. A copy and not the fetched tree, because sd_card.h
# includes "SDIO/rp2040_sdio.h" with quotes, which looks beside sd_card.h
# before any include path: in the fetched tree that finds the GPL header. In
# the copy the directory is absent and the include falls through to
# src/sdio/compat/, which supplies the two header names the library's public
# files include. dma_interrupts.c serves only the old SDIO driver and is left
# out with it; so is rp2040_sdio.pio.h, that driver's assembled program.
set(SD_FATFS_DIR ${CMAKE_BINARY_DIR}/sd_fatfs)
file(REMOVE_RECURSE ${SD_FATFS_DIR})
file(COPY ${no_os_fatfs_SOURCE_DIR}/src/ DESTINATION ${SD_FATFS_DIR}
    PATTERN "SDIO"              EXCLUDE
    PATTERN "ZuluSCSI-firmware" EXCLUDE
    PATTERN "SdFat"             EXCLUDE
    PATTERN "rp2040_sdio.pio.h" EXCLUDE
    PATTERN "dma_interrupts.*"  EXCLUDE
    PATTERN "CMakeLists.txt"    EXCLUDE
)
if(EXISTS ${SD_FATFS_DIR}/sd_driver/SDIO)
    message(FATAL_ERROR "the copy of the SD library still holds its SDIO driver")
endif()

add_library(sd_fatfs INTERFACE)
target_sources(sd_fatfs INTERFACE
    ${SD_FATFS_DIR}/ff15/source/ff.c
    ${SD_FATFS_DIR}/ff15/source/ffsystem.c
    ${SD_FATFS_DIR}/ff15/source/ffunicode.c
    ${SD_FATFS_DIR}/sd_driver/sd_card.c
    ${SD_FATFS_DIR}/sd_driver/sd_timeouts.c
    # The library's SPI card driver. The board's card is on SDIO, but
    # sd_card.c's init path calls sd_spi_ctor() and my_spi_init() for a card
    # of type SD_IF_SPI, and those call into sd_spi.c, so the image does not
    # link without all three.
    ${SD_FATFS_DIR}/sd_driver/SPI/my_spi.c
    ${SD_FATFS_DIR}/sd_driver/SPI/sd_card_spi.c
    ${SD_FATFS_DIR}/sd_driver/SPI/sd_spi.c
    ${SD_FATFS_DIR}/src/crash.c
    ${SD_FATFS_DIR}/src/crc.c
    ${SD_FATFS_DIR}/src/f_util.c
    ${SD_FATFS_DIR}/src/ff_stdio.c
    ${SD_FATFS_DIR}/src/file_stream.c
    ${SD_FATFS_DIR}/src/glue.c
    ${SD_FATFS_DIR}/src/my_debug.c
    ${SD_FATFS_DIR}/src/my_rtc.c
    ${SD_FATFS_DIR}/src/util.c
    # The SDIO driver: protocol, PIO and DMA bus, library callbacks.
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio/sd_proto.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio/sd_host.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio/sd_pio.c
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio/sd_card_glue.c
)
target_include_directories(sd_fatfs INTERFACE
    ${SD_FATFS_DIR}/ff15/source
    ${SD_FATFS_DIR}/sd_driver
    ${SD_FATFS_DIR}/include
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio/compat
    ${CMAKE_CURRENT_LIST_DIR}/src/sdio
)
# As the library's own build sets it.
target_compile_definitions(sd_fatfs INTERFACE PICO_MAX_SHARED_IRQ_HANDLERS=8u)
target_link_libraries(sd_fatfs INTERFACE
    hardware_clocks
    hardware_dma
    hardware_gpio
    hardware_pio
    hardware_spi
    pico_aon_timer
    pico_stdlib
    cmsis_core
)
pico_generate_pio_header(sd_fatfs ${CMAKE_CURRENT_LIST_DIR}/src/sdio/sd_pio.pio)

add_executable(MiniRcBench
    src/main.cpp
    src/ui.cpp
    src/ui_am32.cpp
    src/ui_setup.cpp
    src/ui_input.cpp
    src/ui_widgets.cpp
    src/usb_dev.cpp
    src/usb_descriptors.c
    src/usb_msc.cpp
    src/theme.cpp
    src/settings.cpp
    src/settings_flash.cpp
    src/gfx.cpp
    src/st7789.cpp
    src/cst328.cpp
    src/board_desc.cpp
    src/esc_task.cpp
    src/am32_bl.cpp
    src/am32_eeprom.cpp
    src/kiss_telem.cpp
    src/pio_uart_rx.cpp
    src/esc_merge.cpp
    src/blackbox_encode.cpp
    src/log_ring.cpp
    src/sd_log.cpp
    src/sd_hw_config.c
    # Bidirectional DShot: the portable core and the RP2350 pin driver, from
    # rcbench. See the header comment of each file for the commit.
    src/dshot/dshot_frame.c
    src/dshot/dshot_telem.c
    src/dshot/dshot_edt.c
    src/dshot/out_dshot.c
)

# The KISS receiver is a PIO program (see pio_uart_rx.h for why it is not one
# of the two hardware UARTs). pioasm assembles it into the build tree, and this
# also puts that directory on the include path so `#include "pio_uart_rx.pio.h"`
# resolves.
pico_generate_pio_header(MiniRcBench ${CMAKE_CURRENT_LIST_DIR}/src/pio_uart_rx.pio)
# The DShot transmitter and receiver, assembled the same way into dshot.pio.h.
pico_generate_pio_header(MiniRcBench ${CMAKE_CURRENT_LIST_DIR}/src/dshot/dshot.pio)

target_include_directories(MiniRcBench PRIVATE src)
# One ESC, so one driver slot. A pin change releases the old pin before it binds
# the new one, so it never needs two.
target_compile_definitions(MiniRcBench PRIVATE OUT_DSHOT_MAX_PINS=1u)
# STREQUAL against the empty string, not if(${pinopt}): CMake's if() tests the
# *value*, and "0" is one of its false constants, so a test of the value drops
# an override of 0 without a word.
foreach(pinopt DSHOT_PIN_LCD_2_8 KISS_PIN_LCD_2_8)
    if(NOT "${${pinopt}}" STREQUAL "")
        target_compile_definitions(MiniRcBench PRIVATE ${pinopt}=${${pinopt}})
        message(STATUS "Pin override: ${pinopt}=GP${${pinopt}}")
    endif()
endforeach()

target_link_libraries(MiniRcBench
    pico_stdlib
    pico_multicore
    hardware_spi
    hardware_i2c
    hardware_dma
    hardware_pio
    hardware_pwm
    hardware_adc
    hardware_clocks
    hardware_flash
    hardware_sync
    hardware_watchdog
    sd_fatfs
    tinyusb_device
    pico_unique_id
)

# tusb_config.h is found by bare name from inside TinyUSB's own sources, so
# src/ has to be on the include path for the library as well as for us.
target_include_directories(MiniRcBench PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)

# Serial telemetry goes over USB CDC, not a UART: the board has no spare pins
# for a debug UART, and the USB port is already there for flashing.
#
# pico_enable_stdio_usb() is deliberately *not* used, and this is the one line
# of this file worth reading twice. That helper owns the USB descriptors and
# cannot share the device with another interface -- and the SD card reader is
# another interface. So the descriptors, and the serial port that rides on them,
# are ours: src/usb_descriptors.c and src/usb_dev.cpp. Turning this back on
# would not conflict at build time; it would duplicate the descriptor callbacks
# and fail at link, which is the good outcome. @see usb_dev.h
pico_enable_stdio_uart(MiniRcBench 0)

# The board goes in the filename, so an image names the board it was built
# for after it has left the build tree.
set_target_properties(MiniRcBench PROPERTIES OUTPUT_NAME "MiniRcBench-touch-lcd-2.8")

# The program name defaults to the target name. The version is what
# `picotool info` reports for an image or a connected board.
pico_set_program_version(MiniRcBench "${PROJECT_VERSION}")

# .uf2 for drag-and-drop flashing, plus .elf/.map/.dis for debugging.
pico_add_extra_outputs(MiniRcBench)

# Warnings on our own sources only. The fetched dependencies are not ours to
# keep warning-clean, and -Werror on them would make an upstream bump a build
# break rather than a review.
target_compile_options(MiniRcBench PRIVATE
    -Wall -Wextra -Wshadow -Wno-unused-parameter
)

# STRICT=ON adds -Werror, to this target only.
#
# Worth having as a switch rather than always-on: the only build that uses the
# real arm-none-eabi toolchain had no -Werror at all, so any warning that only
# the cross compiler produces -- the newlib %lu/uint32_t class of thing this
# firmware has already been bitten by once -- was printed by CI and ignored.
# The host permutation job cannot see those; it is a different compiler.
#
# Not the default, because a toolchain bump turning a new warning on should be
# something you read, not something that breaks everyone's build.
option(STRICT "Treat warnings as errors in this project's own sources" OFF)
if(STRICT)
    target_compile_options(MiniRcBench PRIVATE -Werror)
endif()

# Standalone SD bring-up test: nothing but the card, sweeping the SDIO clock.
# Built alongside the firmware because the whole value of it is being able to
# flash one, then the other, without changing anything else. It shares
# sd_hw_config.c with the firmware, so the card wiring it exercises is the
# firmware's.
add_executable(sdtest
    tools/sdtest/main.c
    tools/sdtest/msc_stubs.c
    src/sd_hw_config.c
)
target_include_directories(sdtest PRIVATE src)
target_link_libraries(sdtest
    pico_stdlib
    hardware_clocks
    sd_fatfs
)
target_compile_definitions(sdtest PRIVATE USE_PRINTF=1 USE_DBG_PRINTF=1)
pico_enable_stdio_usb(sdtest 1)
pico_enable_stdio_uart(sdtest 0)
pico_add_extra_outputs(sdtest)

if(SD_DEBUG)
    # USE_PRINTF routes the driver's messages to stdio; USE_DBG_PRINTF adds the
    # per-command tracing that shows exactly which step of card init failed.
    target_compile_definitions(MiniRcBench PRIVATE USE_PRINTF=1 USE_DBG_PRINTF=1)
endif()
