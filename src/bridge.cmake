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
# the KISS receiver.
#
# Slice 2: the USB side on core0 (src/usb_bridge.cpp), the traffic log, a
# build option off by default (src/bridge_log.cpp), and the screen USB
# BRIDGE (src/ui_bridge.cpp), opened from ESC PROGRAM.
#
# Slice 3: MSP (src/bridge_msp.cpp) and the 4-way interface
# (src/bridge_4way.cpp), whose bootloader frames are escwire's
# (escwire.cmake builds lib/rcbench/protocols/escwire/ew_a.c).
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/bridge_wire.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bridge_line_port.cpp
    ${CMAKE_CURRENT_LIST_DIR}/usb_bridge.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bridge_msp.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bridge_4way.cpp
    ${CMAKE_CURRENT_LIST_DIR}/bridge_log.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_bridge.cpp
)
pico_generate_pio_header(MiniRcBench ${CMAKE_CURRENT_LIST_DIR}/bridge_line.pio)
target_link_libraries(MiniRcBench hardware_pio hardware_gpio hardware_clocks)
