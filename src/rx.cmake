# The S.BUS firmware sources, the monitor and the output, included by
# CMakeLists.txt after add_executable(MiniRcBench).
#
# One list per feature, so the S.BUS monitor, the servo tester (servo.cmake) and
# the programmers (kst.cmake, hitec.cmake) add their sources and libraries
# without editing a file the others edit.
#
# The S.BUS decoder and encoder are rcbench's own file, copied into lib/rcbench/
# and built as C (lib/rcbench/README.md). The monitor's core1 job
# (src/sbus_job.cpp) feeds the decoder the bytes UART0 receives on GP1; the
# output's core1 job (src/sbus_out_job.cpp) probes GP0 for an ESC's bootloader
# on the escwire line (src/escwire.cmake), then sends the encoder's frames from
# UART0's TX on GP0; nothing opens it yet. The header is on the target's include
# path because core1_job.cpp includes both jobs' headers. The screens: RX
# SIGNALS, the chooser (src/ui_rx_signals.cpp), and S.BUS MONITOR
# (src/ui_sbus.cpp) with its formatter (src/sbus_view.cpp).
set(RX_LIB ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench)
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sbus_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sbus_view.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_sbus.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_rx_signals.cpp
    ${CMAKE_CURRENT_LIST_DIR}/sbus_out_job.cpp
    ${RX_LIB}/protocols/sbus/sbus.c
)
target_include_directories(MiniRcBench PRIVATE
    ${RX_LIB}/protocols/sbus/include
)
# The output's job runs on core1, whose stack is 2 KB: CI measures the
# deepest chain from its entry points in these call graphs, as it does for
# the Hitec and ESC settings jobs. The probe's classification and connect
# cycle are listed here; the escwire line's files are in src/escwire.cmake.
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/sbus_out_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bl_answer.cpp
    ${CMAKE_CURRENT_LIST_DIR}/greet_cycle.cpp
    ${RX_LIB}/protocols/sbus/sbus.c
    PROPERTIES COMPILE_OPTIONS -fcallgraph-info=su)
target_link_libraries(MiniRcBench hardware_uart hardware_gpio)
