cmake_minimum_required(VERSION 3.20)

# Unit tests of the Layouts core engines. Pure C++17: no OBS, no Qt.
#   cmake -S tests -B build_tests && cmake --build build_tests && ctest --test-dir build_tests

project(layouts-core-tests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src/core")

add_executable(
  layouts-core-tests
  test-main.cpp
  test-geometry.cpp
  test-item-model.cpp
  test-arrange.cpp
  test-snapping.cpp
  test-expression.cpp
  test-guides.cpp
  test-layout.cpp
  test-broadcast.cpp
  test-roles.cpp
  ${CORE_DIR}/geometry.cpp
  ${CORE_DIR}/item-model.cpp
  ${CORE_DIR}/arrange.cpp
  ${CORE_DIR}/grid.cpp
  ${CORE_DIR}/snapping.cpp
  ${CORE_DIR}/expression.cpp
  ${CORE_DIR}/guides.cpp
  ${CORE_DIR}/layout.cpp
  ${CORE_DIR}/broadcast.cpp
  ${CORE_DIR}/roles.cpp
)
target_include_directories(layouts-core-tests PRIVATE "${CORE_DIR}")

if(MSVC)
  target_compile_options(layouts-core-tests PRIVATE /W4)
else()
  target_compile_options(layouts-core-tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME layouts-core COMMAND layouts-core-tests)
