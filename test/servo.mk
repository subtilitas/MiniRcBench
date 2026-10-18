# The servo tester's part of the host suite, included by test/Makefile.
#
# One list per feature, so the servo tester and the KST programmer (kst.mk)
# add their sources without editing a file the other edits. Slice 2 of
# docs/design/pin-claim.md replaces the stub entries here.
#
#   SERVO_SRCS         sources linked into the suite
#   SERVO_TESTS        the test files; test_servo.cpp holds runServoTests()
#   SERVO_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`
SERVO_SRCS        := ../src/ui_servo.cpp ../src/servo_model.cpp
SERVO_TESTS       := test_servo.cpp
SERVO_DEVICE_SRCS :=
