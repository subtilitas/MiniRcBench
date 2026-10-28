# The Hitec programmer's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, so the Hitec programmer, the KST programmer
# (kst.cmake) and the servo tester (servo.cmake) add their sources, PIO
# programs and libraries without editing a file the others edit.
#
# The Hitec core, its RP2350 line driver, the servo file text and the CRC it
# uses are rcbench's own files, copied into lib/rcbench/ and built as C
# (lib/rcbench/README.md). The line's PIO program is assembled from its
# source by pioasm. The core1 job runs them (src/hitec_job.cpp) on the line
# of src/hitec_line_port.cpp.
set(HITEC_LIB ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench)
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/hitec_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hitec_line_port.cpp
    ${HITEC_LIB}/protocols/hitec/hitec_wire.c
    ${HITEC_LIB}/protocols/hitec/hitec_pio.c
    ${HITEC_LIB}/protocols/hitec/hitec_pre.c
    ${HITEC_LIB}/protocols/hitec/hitec_reg.c
    ${HITEC_LIB}/protocols/hitec/hitec_session.c
    ${HITEC_LIB}/protocols/hitec/rp2350/hitec_line.c
    ${HITEC_LIB}/shared/bench/hitec_card.c
    ${HITEC_LIB}/shared/link/link_crc.c
)
target_include_directories(MiniRcBench PRIVATE
    ${HITEC_LIB}/protocols/hitec/include
    ${HITEC_LIB}/protocols/hitec/rp2350
    ${HITEC_LIB}/shared/bench/include
    ${HITEC_LIB}/shared/link/include
)
pico_generate_pio_header(MiniRcBench ${HITEC_LIB}/protocols/hitec/rp2350/hitec_line.pio)
# The call graph and stack frames of what the job runs on core1, whose stack
# is 2 KB. CI's "Hitec job stack on core1" finds the deepest chain in them
# (tools/stack_depth.py).
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/hitec_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/hitec_line_port.cpp
    ${CMAKE_CURRENT_LIST_DIR}/servo_supply.cpp
    ${HITEC_LIB}/protocols/hitec/hitec_session.c
    ${HITEC_LIB}/protocols/hitec/hitec_reg.c
    ${HITEC_LIB}/protocols/hitec/hitec_wire.c
    ${HITEC_LIB}/protocols/hitec/hitec_pio.c
    ${HITEC_LIB}/protocols/hitec/hitec_pre.c
    ${HITEC_LIB}/protocols/hitec/rp2350/hitec_line.c
    PROPERTIES COMPILE_OPTIONS -fcallgraph-info=su)
# The SDK libraries the line driver uses, as rcbench's rp2350/CMakeLists.txt
# links them.
target_link_libraries(MiniRcBench
    hardware_pio hardware_gpio hardware_dma hardware_clocks hardware_timer
)
