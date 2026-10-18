# The KST programmer's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, so the KST programmer and the servo tester
# (servo.cmake) add their sources, PIO programs and libraries without editing
# a file the other edits. Slice 3 of docs/design/pin-claim.md replaces the
# stub here.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/ui_kst.cpp
)
