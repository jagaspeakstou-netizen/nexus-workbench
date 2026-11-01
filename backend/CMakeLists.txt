cmake_minimum_required(VERSION 3.16)
project(nexus_backend CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

# httplib needs threads
find_package(Threads REQUIRED)

add_library(nexus_core
  src/core/status.cpp
  src/thermal/thermal.cpp
  src/geometry/stl_parser.cpp
  src/geometry/csv_parser.cpp
  src/performance/performance.cpp
  src/performance/hardware.cpp
)
target_include_directories(nexus_core PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/httplib
  ${CMAKE_CURRENT_SOURCE_DIR}/third_party/json
)
target_link_libraries(nexus_core PUBLIC Threads::Threads)

# Lets src/performance/hardware.cpp report the *actual* build type and
# flags it was compiled with, rather than a hardcoded guess -- part of
# the Performance module's "never fabricate... report optimization
# flags" requirement.
string(TOUPPER "${CMAKE_BUILD_TYPE}" NEXUS_BUILD_TYPE_UPPER)
target_compile_definitions(nexus_core PRIVATE
  NEXUS_BUILD_TYPE="${CMAKE_BUILD_TYPE}"
  NEXUS_CXX_FLAGS="${CMAKE_CXX_FLAGS} ${CMAKE_CXX_FLAGS_${NEXUS_BUILD_TYPE_UPPER}}"
)

add_executable(nexus_server src/api/main.cpp)
target_link_libraries(nexus_server PRIVATE nexus_core)

add_executable(nexus_thermal_export src/thermal/export_main.cpp)
target_link_libraries(nexus_thermal_export PRIVATE nexus_core)

add_executable(nexus_performance_export src/performance/export_main.cpp)
target_link_libraries(nexus_performance_export PRIVATE nexus_core)

# Tests (optional - only built if GoogleTest is available).
# Prefer a system/vcpkg-provided GoogleTest (find_package) over downloading
# the source at configure time: CI runners and the Windows build both have
# gflaky/blocked network access to raw source archives, so FetchContent is
# a fragile default here. See docs/architecture for the vcpkg manifest used
# on the Windows CI runner (Day 12).
option(NEXUS_BUILD_TESTS "Build unit tests" ON)
if(NEXUS_BUILD_TESTS)
  find_package(GTest QUIET)
  if(GTest_FOUND)
    enable_testing()
    add_executable(nexus_tests
      tests/test_status.cpp
      tests/test_thermal.cpp
      tests/test_geometry.cpp
      tests/test_performance.cpp
    )
    target_link_libraries(nexus_tests PRIVATE nexus_core GTest::gtest_main)
    include(GoogleTest)
    gtest_discover_tests(nexus_tests)
  else()
    message(WARNING "GoogleTest not found (find_package(GTest)) - skipping nexus_tests target. Install libgtest-dev (Linux) or the vcpkg gtest port (Windows).")
  endif()
endif()
