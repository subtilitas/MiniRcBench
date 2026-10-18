# The servo tester's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, so the servo tester and the KST programmer
# (kst.cmake) add their sources, PIO programs and libraries without editing a
# file the other edits.
#
# The sweep curve and the GPIO-to-PWM fold are rcbench's own files, copied
# into lib/rcbench/ and built as C (lib/rcbench/README.md). Their headers are
# on the target's include path because core1_job.cpp includes servo_job.h.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ui_servo.cpp
    ${CMAKE_CURRENT_LIST_DIR}/servo_model.cpp
    ${CMAKE_CURRENT_LIST_DIR}/servo_pwm.cpp
    ${CMAKE_CURRENT_LIST_DIR}/servo_job.cpp
    ${CMAKE_CURRENT_LIST_DIR}/ui_slider.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/servo/servo_sweep.c
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/outputs/out_pwm_map.c
)
target_include_directories(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/servo/include
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/outputs/include
)
