# The Bluejay and BLHeli_S settings' firmware sources, included by
# CMakeLists.txt after add_executable(MiniRcBench).
#
# One list per feature, so the ESC settings, the servo tester (servo.cmake),
# the programmers (kst.cmake, hitec.cmake), the S.BUS monitor (rx.cmake) and
# the current sensor (sense.cmake) add their sources, PIO programs and
# libraries without editing a file the others edit.
#
# The escwire core and its RP2350 line driver are rcbench's own files, copied
# into lib/rcbench/ and built as C (lib/rcbench/README.md). The line's PIO
# program is assembled from its source by pioasm. The core1 job runs them
# (src/escwire_job.cpp) on the line of src/escwire_port.cpp, and ESC
# SETTINGS shows the page through src/efm8_view.cpp
# (docs/design/bluejay-blheli-s.md, slice 3); the linker drops what no
# caller reaches.
set(ESCWIRE_LIB ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/protocols/escwire)
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/escwire_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/escwire_port.cpp
    ${CMAKE_CURRENT_LIST_DIR}/efm8_view.cpp
    ${ESCWIRE_LIB}/ew_wire.c
    ${ESCWIRE_LIB}/ew_pio.c
    ${ESCWIRE_LIB}/ew_a.c
    ${ESCWIRE_LIB}/ew_text.c
    ${ESCWIRE_LIB}/ew_guard.c
    ${ESCWIRE_LIB}/ew_id.c
    ${ESCWIRE_LIB}/ew_map.c
    ${ESCWIRE_LIB}/ew_map_s.c
    ${ESCWIRE_LIB}/ew_map_bj.c
    ${ESCWIRE_LIB}/ew_map_am.c
    ${ESCWIRE_LIB}/ew_map_e32.c
    ${ESCWIRE_LIB}/ew_session.c
    ${ESCWIRE_LIB}/ew_session_text.c
    ${ESCWIRE_LIB}/ew_file.c
    ${ESCWIRE_LIB}/ew_file_read.c
    ${ESCWIRE_LIB}/ew_file_load.c
    ${ESCWIRE_LIB}/rp2350/ew_line.c
)
target_include_directories(MiniRcBench PRIVATE
    ${ESCWIRE_LIB}/include
    ${ESCWIRE_LIB}/rp2350
)
pico_generate_pio_header(MiniRcBench ${ESCWIRE_LIB}/rp2350/ew_line.pio)
# The call graph and stack frames of what the job runs on core1, whose stack
# is 2 KB. CI's "ESC settings job stack on core1" finds the deepest chain in
# them (tools/stack_depth.py).
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/escwire_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/escwire_port.cpp
    ${ESCWIRE_LIB}/ew_wire.c
    ${ESCWIRE_LIB}/ew_pio.c
    ${ESCWIRE_LIB}/ew_a.c
    ${ESCWIRE_LIB}/ew_text.c
    ${ESCWIRE_LIB}/ew_guard.c
    ${ESCWIRE_LIB}/ew_id.c
    ${ESCWIRE_LIB}/ew_map.c
    ${ESCWIRE_LIB}/ew_map_s.c
    ${ESCWIRE_LIB}/ew_map_bj.c
    ${ESCWIRE_LIB}/ew_map_am.c
    ${ESCWIRE_LIB}/ew_map_e32.c
    ${ESCWIRE_LIB}/ew_session.c
    ${ESCWIRE_LIB}/ew_session_text.c
    ${ESCWIRE_LIB}/rp2350/ew_line.c
    PROPERTIES COMPILE_OPTIONS -fcallgraph-info=su)
# The SDK libraries the line driver uses, as rcbench's rp2350/CMakeLists.txt
# links them.
target_link_libraries(MiniRcBench
    hardware_pio hardware_gpio hardware_dma hardware_clocks hardware_timer
)
