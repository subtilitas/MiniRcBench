# The angle meter's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, as servo.cmake, kst.cmake, hitec.cmake, rx.cmake and
# sense.cmake. The QMI8658 driver reads the board's IMU (inertial measurement
# unit) on the shared I2C1 bus and reports its transfers to the current
# sensor's bus record (sense.h), whose include path sense.cmake sets.
# docs/design/angle-meter.md lists what later slices add.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/qmi8658.cpp
)
