cmake_minimum_required(VERSION 4.0)

project(
  wine_for_android
  VERSION 0.1.1
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(
  wfa_core
  src/apk_loader.cpp
  src/checkpoint.cpp
  src/manifest_assessment.cpp
  src/package_layout.cpp
  src/project_status.cpp
  src/runtime_bridge.cpp
)

target_include_directories(wfa_core PUBLIC include)

add_executable(compatctl src/main.cpp)
target_link_libraries(compatctl PRIVATE wfa_core)

add_executable(
  wfa_tests
  tests/test_main.cpp
)
target_link_libraries(wfa_tests PRIVATE wfa_core)

add_test(NAME wfa_tests COMMAND wfa_tests)
