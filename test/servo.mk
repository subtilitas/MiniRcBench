# The servo tester's part of the host suite, included by test/Makefile.
#
# One list per feature, so the servo tester and the KST programmer (kst.mk)
# add their sources without editing a file the other edits.
#
#   SERVO_SRCS         C++ sources linked into the suite. ui_servo.cpp is not
#                      one: test_servo.cpp includes it, to reach its
#                      file-static state, as test_ui.cpp does ui_am32.cpp
#   SERVO_CSRCS        C sources linked into the suite: rcbench's sweep, its
#                      GPIO-to-PWM fold, its move judged from the current and
#                      its servo model, copied into lib/rcbench/; the model
#                      is the host suite's only
#   SERVO_INCLUDES     their include directories
#   SERVO_TESTS        the test files; test_servo.cpp holds runServoTests(),
#                      test_move_timer.cpp runMoveTimerTests()
#   SERVO_CTESTS       rcbench's own tests of the C sources, each a program
#                      of its own, built as C11 and run by `make run`
#   SERVO_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`:
#                      the servo log, which writes through FatFs and the
#                      card cache; `make lognum` runs it over FatFs itself
SERVO_SRCS        := ../src/servo_model.cpp \
                     ../src/servo_pwm.cpp ../src/servo_job.cpp ../src/move_timer.cpp \
                     ../src/ui_slider.cpp
SERVO_CSRCS       := ../lib/rcbench/shared/servo/servo_sweep.c \
                     ../lib/rcbench/shared/outputs/out_pwm_map.c \
                     ../lib/rcbench/shared/servo/servo_move.c \
                     ../lib/rcbench/shared/servo/servo_sim.c
SERVO_INCLUDES    := -I../lib/rcbench/shared/servo/include \
                     -I../lib/rcbench/shared/outputs/include
SERVO_TESTS       := test_servo.cpp test_move_timer.cpp
SERVO_CTESTS      := rcbench/test_servo_sweep.c rcbench/test_out_pwm_map.c \
                     rcbench/test_servo_move.c
SERVO_DEVICE_SRCS := ../src/servo_log.cpp
