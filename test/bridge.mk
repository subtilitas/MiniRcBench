# The USB bridge's part of the host suite, included by test/Makefile.
#
# One list per feature, as servo.mk, kst.mk, hitec.mk, rx.mk, sense.mk,
# escwire.mk and angle.mk.
#
#   BRIDGE_SRCS         C++ sources linked into the suite: the job, the frame
#                       slot and the ring (docs/design/usb-bridge.md, slice 1);
#                       the USB side and the screen USB BRIDGE (slice 2).
#                       The log is off, as the firmware builds it: its
#                       entry points are bridge_log.h's empty inlines
#   BRIDGE_TESTS        the test files; test_bridge.cpp holds
#                       runBridgeTests(), test_usb_bridge.cpp
#                       runUsbBridgeTests(), test_bridge_ui.cpp
#                       runBridgeUiTests(). fake_bridge_line.cpp is the line
#                       the job drives: a wire on the virtual clock with a
#                       model ESC behind bridge_line_port.h; fake_cdc.cpp the
#                       USB serial port with a PC behind it
#   BRIDGE_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`
#   BRIDGE_LOG_SRCS     test_bridge_log, a program of its own that `run` runs:
#                       the traffic log built on (USB_BRIDGE_LOG 1), the
#                       firmware's default being off
#
# The line's two PIO programs, src/bridge_line.pio and src/pio_uart_rx.pio,
# run in test/pio_bridge_emu.py, which `make pioemu` runs.
BRIDGE_SRCS        := ../src/bridge_wire.cpp ../src/usb_bridge.cpp \
                      ../src/ui_bridge.cpp
BRIDGE_TESTS       := test_bridge.cpp fake_bridge_line.cpp test_usb_bridge.cpp fake_cdc.cpp \
                      test_bridge_ui.cpp
BRIDGE_DEVICE_SRCS := ../src/bridge_line_port.cpp
BRIDGE_LOG_SRCS    := test_bridge_log.cpp fake_cdc.cpp ../src/bridge_log.cpp ../src/usb_bridge.cpp
