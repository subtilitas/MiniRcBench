# The USB bridge's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, as servo.cmake, kst.cmake, hitec.cmake, rx.cmake,
# sense.cmake and escwire.cmake, so each adds its sources, PIO programs and
# libraries without editing a file the others edit.
#
# Slice 1 of docs/design/usb-bridge.md: the core1 job with its frame slot and
# ring (src/bridge_wire.cpp) and its line on two PIO state machines
# (src/bridge_line_port.cpp). The line's transmitter is src/bridge_line.pio;
# its receiver is pio_uart_rx.pio, whose header CMakeLists.txt generates for
# the KISS receiver. Nothing on core0 opens the bridge yet.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bridge_wire.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bridge_line_port.cpp
)
pico_generate_pio_header(MiniRcBench ${CMAKE_CURRENT_LIST_DIR}/bridge_line.pio)
target_link_libraries(MiniRcBench hardware_pio hardware_gpio hardware_clocks)
