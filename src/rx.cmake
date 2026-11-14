# The S.BUS monitor's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, so the S.BUS monitor, the servo tester (servo.cmake)
# and the programmers (kst.cmake, hitec.cmake) add their sources and
# libraries without editing a file the others edit.
#
# The S.BUS decoder is rcbench's own file, copied into lib/rcbench/ and built
# as C (lib/rcbench/README.md). The core1 job (src/sbus_job.cpp) feeds it the
# bytes UART0 receives on GP1. Its header is on the target's include path
# because core1_job.cpp includes sbus_job.h.
set(RX_LIB ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench)
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sbus_job.cpp
    ${RX_LIB}/protocols/sbus/sbus.c
)
target_include_directories(MiniRcBench PRIVATE
    ${RX_LIB}/protocols/sbus/include
)
target_link_libraries(MiniRcBench hardware_uart hardware_gpio)
