# The USB bridge's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk, sense.mk,
# escwire.mk and angle.mk.
#
#   BRIDGE_SRCS         C++ sources linked into the suite: the job, the frame
#                       slot and the ring (docs/design/usb-bridge.md, slice 1)
#   BRIDGE_TESTS        the test files; test_bridge.cpp holds
#                       runBridgeTests(). fake_bridge_line.cpp is the line the
#                       job drives: a wire on the virtual clock with a model
#                       ESC behind bridge_line_port.h
#   BRIDGE_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`
#
# The line's two PIO programs, src/bridge_line.pio and src/pio_uart_rx.pio,
# run in test/pio_bridge_emu.py, which `make pioemu` runs.
BRIDGE_SRCS        := ../src/bridge_wire.cpp
BRIDGE_TESTS       := test_bridge.cpp fake_bridge_line.cpp
BRIDGE_DEVICE_SRCS := ../src/bridge_line_port.cpp
