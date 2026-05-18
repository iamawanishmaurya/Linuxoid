cmake_minimum_required(VERSION 4.0)

project(
  Linuxoid
  VERSION 0.1.55
  LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(CTest)
find_package(ZLIB REQUIRED)

find_path(WAYLAND_CLIENT_INCLUDE_DIR wayland-client.h)
find_library(WAYLAND_CLIENT_LIBRARY wayland-client)
find_path(EGL_INCLUDE_DIR EGL/egl.h)
find_library(EGL_LIBRARY EGL)

add_library(
  wfa_core
  src/android_binary_xml.cpp
  src/apk_host_integration.cpp
  src/art_activity_bootstrap_fixture.cpp
  src/art_bootstrap_execution_fixture.cpp
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
target_link_libraries(wfa_core PUBLIC ZLIB::ZLIB)

add_library(
  linuxoid_p1
  src/apk_compatibility_bridge.cpp
  src/apk_activity_launch_bridge.cpp
  src/apk_asset_bridge.cpp
  src/apk_dex_bridge.cpp
  src/apk_java_proof_bridge.cpp
  src/apk_lifecycle_bridge.cpp
  src/apk_native_launch.cpp
  src/apk_permission_bridge.cpp
  src/apk_process_bridge.cpp
  src/apk_runtime_bridge.cpp
  src/apk_self_healing_watchdog.cpp
  src/apk_storage_bridge.cpp
  src/apk_window_bridge.cpp
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
target_link_libraries(linuxoid_p1 PUBLIC dl wfa_core)
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

add_library(linuxoid_android_compat_libc SHARED src/android_bionic_libc_compat.cpp)
target_link_libraries(linuxoid_android_compat_libc PRIVATE dl m pthread)
target_link_options(linuxoid_android_compat_libc PRIVATE "-Wl,-soname,libc.so")
set_target_properties(linuxoid_android_compat_libc PROPERTIES OUTPUT_NAME "c")

add_library(linuxoid_android_compat_liblog SHARED src/android_log_compat.cpp)
target_link_libraries(linuxoid_android_compat_liblog PRIVATE dl m pthread)
target_link_options(linuxoid_android_compat_liblog PRIVATE "-Wl,-soname,liblog.so")
set_target_properties(linuxoid_android_compat_liblog PROPERTIES OUTPUT_NAME "log")

add_library(linuxoid_android_compat_libm SHARED src/android_passthrough_compat.cpp)
target_link_libraries(linuxoid_android_compat_libm PRIVATE m)
target_link_options(linuxoid_android_compat_libm PRIVATE "-Wl,-soname,libm.so")
set_target_properties(linuxoid_android_compat_libm PROPERTIES OUTPUT_NAME "m")

add_library(linuxoid_android_compat_libdl SHARED src/android_passthrough_compat.cpp)
target_link_libraries(linuxoid_android_compat_libdl PRIVATE dl)
target_link_options(linuxoid_android_compat_libdl PRIVATE "-Wl,-soname,libdl.so")
set_target_properties(linuxoid_android_compat_libdl PROPERTIES OUTPUT_NAME "dl")

add_library(linuxoid_p1_fixture SHARED tests/p1_fixture_native_activity.cpp)
set_target_properties(linuxoid_p1_fixture PROPERTIES PREFIX "lib")

add_library(linuxoid_p1_sidecar_jni_fixture SHARED tests/p1_fixture_sidecar_jni.cpp)
set_target_properties(linuxoid_p1_sidecar_jni_fixture PROPERTIES PREFIX "lib")

add_library(linuxoid_p1_jni_only_fixture SHARED tests/p1_fixture_jni_only.cpp)
set_target_properties(linuxoid_p1_jni_only_fixture PROPERTIES PREFIX "lib")

add_library(linuxoid_android_compat_fixture SHARED tests/p1_fixture_android_compat.cpp)
target_link_libraries(linuxoid_android_compat_fixture
  PRIVATE
    linuxoid_android_compat_libc
    linuxoid_android_compat_liblog
    linuxoid_android_compat_libm
    linuxoid_android_compat_libdl)
set_target_properties(linuxoid_android_compat_fixture PROPERTIES PREFIX "lib")

add_library(
  linuxoid_android_compat_missing_symbol_fixture
  SHARED
  tests/p1_fixture_android_compat_missing_symbol.cpp)
target_link_libraries(linuxoid_android_compat_missing_symbol_fixture
  PRIVATE
    linuxoid_android_compat_libc
    linuxoid_android_compat_liblog
    linuxoid_android_compat_libm
    linuxoid_android_compat_libdl)
set_target_properties(linuxoid_android_compat_missing_symbol_fixture PROPERTIES PREFIX "lib")

add_test(NAME wfa_tests COMMAND wfa_tests)
