# The KST programmer's part of the host suite, included by test/Makefile.
#
# One list per feature, so the KST programmer and the servo tester (servo.mk)
# add their sources without editing a file the other edits. Slice 3 of
# docs/design/pin-claim.md replaces the stub entries here.
#
#   KST_SRCS         sources linked into the suite
#   KST_TESTS        the test files; test_kst.cpp holds runKstTests()
#   KST_DEVICE_SRCS  sources the suite cannot link, typechecked by `syntax`
KST_SRCS        := ../src/ui_kst.cpp
KST_TESTS       := test_kst.cpp
KST_DEVICE_SRCS :=
