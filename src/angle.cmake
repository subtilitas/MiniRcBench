# The angle meter's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, as servo.cmake, kst.cmake, hitec.cmake, rx.cmake and
# sense.cmake. The QMI8658 driver reads the board's IMU (inertial measurement
# unit) on the shared I2C1 bus and reports its transfers to the current
# sensor's bus record (sense.h), whose include path sense.cmake sets. The
# model, angle_model.cpp, turns its samples into the angle through rcbench's
# control-surface arithmetic (servo_rudder.c) and IMU model (imu.c), copied
# into lib/rcbench/ and built as C (lib/rcbench/README.md), with the WitMotion
# decoder imu.h includes (witmotion.c). The screen, ui_angle.cpp, opens the
# driver and shows the model's reading; TOOLS, ui_tools.cpp, is the chooser
# behind its MENU row.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/qmi8658.cpp
    ${CMAKE_CURRENT_LIST_DIR}/angle_model.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_angle.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_tools.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/servo/servo_rudder.c
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/bench/imu.c
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/protocols/witmotion/witmotion.c
)
target_include_directories(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/servo/include
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/bench/include
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/protocols/witmotion/include
)

# arm-none-eabi makes an enum as narrow as its values allow: imu_axis_t, 0 to
# 2, is one unsigned byte, so imu.c's `(int)axis < 0` in imu_deg() and
# imu_still_ms() is always false and -Wtype-limits warns, which STRICT=ON
# makes an error. The copy stays as rcbench has it; the warning is off for
# that file alone.
set_source_files_properties(
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/bench/imu.c
    PROPERTIES COMPILE_OPTIONS -Wno-type-limits)
