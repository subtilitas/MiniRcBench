# The KST programmer's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, so the KST programmer and the servo tester
# (servo.cmake) add their sources, PIO programs and libraries without editing
# a file the other edits.
#
# The KST core, its RP2350 line driver and the file store are rcbench's own
# files, copied into lib/rcbench/ and built as C (lib/rcbench/README.md). The
# line's PIO program is assembled from its source by pioasm. The search and
# the core1 job run them; the file store's code is dropped from the image
# until the KST screen calls it.
set(KST_LIB ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench)
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ui_kst.cpp
    ${CMAKE_CURRENT_LIST_DIR}/kst_seek.cpp
    ${CMAKE_CURRENT_LIST_DIR}/kst_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/kst_line_port.cpp
    ${KST_LIB}/protocols/kst/kst_wire.c
    ${KST_LIB}/protocols/kst/kst_reg.c
    ${KST_LIB}/protocols/kst/kst_limits.c
    ${KST_LIB}/protocols/kst/kst_plan.c
    ${KST_LIB}/protocols/kst/kst_session.c
    ${KST_LIB}/protocols/kst/kst_pio.c
    ${KST_LIB}/protocols/kst/kst_file.c
    ${KST_LIB}/protocols/kst/rp2350/kst_line.c
    ${KST_LIB}/shared/logfile/kst_store.c
)
target_include_directories(MiniRcBench PRIVATE
    ${KST_LIB}/protocols/kst/include
    ${KST_LIB}/protocols/kst/rp2350
    ${KST_LIB}/shared/logfile/include
)
pico_generate_pio_header(MiniRcBench ${KST_LIB}/protocols/kst/rp2350/kst_line.pio)
# The SDK libraries the line driver uses, as rcbench's rp2350/CMakeLists.txt
# links them.
target_link_libraries(MiniRcBench
    hardware_pio hardware_gpio hardware_dma hardware_clocks hardware_timer
)
