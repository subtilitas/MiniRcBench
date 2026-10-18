# The servo tester's part of the host suite, included by test/Makefile.
#
# One list per feature, so the servo tester and the KST programmer (kst.mk)
# add their sources without editing a file the other edits.
#
#   SERVO_SRCS         C++ sources linked into the suite
#   SERVO_CSRCS        C sources linked into the suite: rcbench's sweep and
#                      GPIO-to-PWM fold, copied into lib/rcbench/
#   SERVO_INCLUDES     their include directories
#   SERVO_TESTS        the test files; test_servo.cpp holds runServoTests()
#   SERVO_CTESTS       rcbench's own tests of the C sources, each a program
#                      of its own, built as C11 and run by `make run`
#   SERVO_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`
SERVO_SRCS        := ../src/ui_servo.cpp ../src/servo_model.cpp \
                     ../src/servo_pwm.cpp ../src/servo_job.cpp
SERVO_CSRCS       := ../lib/rcbench/shared/servo/servo_sweep.c \
                     ../lib/rcbench/shared/outputs/out_pwm_map.c
SERVO_INCLUDES    := -I../lib/rcbench/shared/servo/include \
                     -I../lib/rcbench/shared/outputs/include
SERVO_TESTS       := test_servo.cpp
SERVO_CTESTS      := rcbench/test_servo_sweep.c rcbench/test_out_pwm_map.c
SERVO_DEVICE_SRCS :=
