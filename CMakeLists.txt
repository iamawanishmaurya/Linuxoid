cmake_minimum_required(VERSION 4.0)

project(
  Linuxoid
  VERSION 0.1.46
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)

find_path(WAYLAND_CLIENT_INCLUDE_DIR wayland-client.h)
find_library(WAYLAND_CLIENT_LIBRARY wayland-client)
find_path(EGL_INCLUDE_DIR EGL/egl.h)
find_library(EGL_LIBRARY EGL)

add_library(
  wfa_core
  src/apk_host_integration.cpp
  src/art_classloader_fixture.cpp
  src/art_class_resolution_fixture.cpp
  src/art_runtime_smoke.cpp
  src/apk_archive.cpp
  src/apk_loader.cpp
  src/binder_service_manager.cpp
  src/checkpoint.cpp
  src/desktop_integration.cpp
  src/manifest_assessment.cpp
  src/native_lifecycle.cpp
  src/native_spike.cpp
  src/package_layout.cpp
  src/project_status.cpp
  src/runtime_bridge.cpp
  src/runtime_health.cpp
  src/waydroid_integration.cpp
)

target_include_directories(wfa_core PUBLIC include)

add_library(
  linuxoid_p1
  src/asset_manager_stub.cpp
  src/jni_stub.cpp
  src/looper_stub.cpp
  src/native_execute_stub.cpp
  src/native_input_queue_fixture.cpp
  src/egl_smoke_fixture.cpp
  src/native_window_surface.cpp
  src/wayland_surface_fixture.cpp
  src/signal_handler.cpp
)
target_include_directories(linuxoid_p1 PUBLIC include)
target_link_libraries(linuxoid_p1 PUBLIC dl)
if(WAYLAND_CLIENT_INCLUDE_DIR AND WAYLAND_CLIENT_LIBRARY)
  target_include_directories(linuxoid_p1 PRIVATE ${WAYLAND_CLIENT_INCLUDE_DIR})
  target_link_libraries(linuxoid_p1 PUBLIC ${WAYLAND_CLIENT_LIBRARY})
  target_compile_definitions(linuxoid_p1 PRIVATE WFA_HAVE_WAYLAND_CLIENT=1)
endif()
if(EGL_INCLUDE_DIR AND EGL_LIBRARY)
  target_include_directories(linuxoid_p1 PRIVATE ${EGL_INCLUDE_DIR})
  target_link_libraries(linuxoid_p1 PUBLIC ${EGL_LIBRARY})
  target_compile_definitions(linuxoid_p1 PRIVATE WFA_HAVE_EGL=1)
endif()

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
