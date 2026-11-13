# The current sensor's firmware sources, included by CMakeLists.txt after
# add_executable(MiniRcBench).
#
# One list per feature, as servo.cmake, kst.cmake and hitec.cmake. The bus and
# part decisions are rcbench's sense_bus.c, copied into lib/rcbench/ and built
# as C (lib/rcbench/README.md); src/sense.cpp supplies its wire over the SDK.
# Its header is on the target's include path because cst328.cpp, ui.cpp and
# settings.cpp include sense.h.
target_sources(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/sense.cpp
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/sense/sense_bus.c
)
target_include_directories(MiniRcBench PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../lib/rcbench/shared/sense/include
)
