cmake_minimum_required(VERSION 4.0)

project(
  Linuxoid
  VERSION 0.1.28
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

add_library(
  wfa_core
  src/apk_host_integration.cpp
  src/apk_loader.cpp
  src/checkpoint.cpp
  src/desktop_integration.cpp
  src/manifest_assessment.cpp
  src/native_lifecycle.cpp
  src/native_spike.cpp
  src/package_layout.cpp
  src/project_status.cpp
  src/runtime_bridge.cpp
  src/waydroid_integration.cpp
)

target_include_directories(wfa_core PUBLIC include)

add_library(
  linuxoid_p1
  src/asset_manager_stub.cpp
  src/jni_stub.cpp
  src/looper_stub.cpp
  src/native_execute_stub.cpp
  src/signal_handler.cpp
)
target_include_directories(linuxoid_p1 PUBLIC include)
target_link_libraries(linuxoid_p1 PUBLIC dl)

add_executable(compatctl src/main.cpp)
target_link_libraries(compatctl PRIVATE wfa_core linuxoid_p1)
set_target_properties(compatctl PROPERTIES ENABLE_EXPORTS ON)

add_executable(
  wfa_tests
  tests/test_main.cpp
)
target_link_libraries(wfa_tests PRIVATE wfa_core linuxoid_p1)

add_library(linuxoid_p1_fixture SHARED tests/p1_fixture_native_activity.cpp)
set_target_properties(linuxoid_p1_fixture PROPERTIES PREFIX "lib")

add_test(NAME wfa_tests COMMAND wfa_tests)
