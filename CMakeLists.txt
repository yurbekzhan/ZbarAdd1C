cmake_minimum_required(VERSION 3.20)
cmake_policy(SET CMP0091 NEW)
project(ZbarAdd1C VERSION 0.1.0 LANGUAGES C CXX)
if(MSVC)
  add_compile_options(/utf-8)
  set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(ZBAR1C_BUNDLED_DEPS "Build dependencies from the included source archives" ON)
if(ZBAR1C_BUNDLED_DEPS)
  include(cmake/BundledDependencies.cmake)
else()
  find_package(PNG REQUIRED)
  find_path(ZBAR_INCLUDE_DIR NAMES zbar.h REQUIRED)
  find_library(ZBAR_LIBRARY NAMES zbar libzbar REQUIRED)
  add_library(system_zbar UNKNOWN IMPORTED)
  set_target_properties(system_zbar PROPERTIES IMPORTED_LOCATION "${ZBAR_LIBRARY}"
      INTERFACE_INCLUDE_DIRECTORIES "${ZBAR_INCLUDE_DIR}")
  set(QR_PNG_TARGET PNG::PNG)
  set(QR_ZBAR_TARGET system_zbar)
endif()

add_library(qr_core STATIC src/qr_reader.cpp)
target_compile_features(qr_core PUBLIC cxx_std_17)
set_target_properties(qr_core PROPERTIES POSITION_INDEPENDENT_CODE ON)
target_include_directories(qr_core PUBLIC include)
target_link_libraries(qr_core PRIVATE ${QR_PNG_TARGET} ${QR_ZBAR_TARGET})
if(MSVC)
  target_compile_options(qr_core PRIVATE /W4 /utf-8)
else()
  target_compile_options(qr_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(qr_scan tools/qr_scan.cpp)
target_link_libraries(qr_scan PRIVATE qr_core)
if(MINGW)
  target_link_options(qr_scan PRIVATE -municode)
endif()

add_library(ZbarAdd1C SHARED src/component.cpp)
target_include_directories(ZbarAdd1C PRIVATE third_party/1c/include)
target_link_libraries(ZbarAdd1C PRIVATE qr_core)
target_compile_definitions(ZbarAdd1C PRIVATE NOMINMAX)
set_target_properties(ZbarAdd1C PROPERTIES PREFIX "" CXX_VISIBILITY_PRESET hidden)
if(WIN32)
  enable_language(RC)
  configure_file(resources/component.rc.in component.rc @ONLY)
  target_sources(ZbarAdd1C PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/component.rc")
  if(MSVC)
    # The resource already contains the DLL manifest with resource ID 2.
    target_link_options(ZbarAdd1C PRIVATE /MANIFEST:NO)
  endif()
else()
  find_path(UUID_INCLUDE_DIR uuid/uuid.h REQUIRED)
  target_include_directories(ZbarAdd1C PRIVATE ${UUID_INCLUDE_DIR})
endif()
if(WIN32 AND NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "The selected Windows target must be x64")
endif()

include(CTest)
if(BUILD_TESTING)
  add_executable(qr_tests tests/qr_tests.cpp)
  target_link_libraries(qr_tests PRIVATE qr_core)
  add_test(NAME qr_reader COMMAND qr_tests "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
  add_executable(native_api_tests tests/native_api_tests.cpp)
  target_compile_features(native_api_tests PRIVATE cxx_std_17)
  target_include_directories(native_api_tests PRIVATE include third_party/1c/include)
  target_compile_definitions(native_api_tests PRIVATE NOMINMAX)
  target_link_libraries(native_api_tests PRIVATE ${CMAKE_DL_LIBS})
  if(NOT WIN32)
    target_include_directories(native_api_tests PRIVATE ${UUID_INCLUDE_DIR})
  endif()
  add_dependencies(native_api_tests ZbarAdd1C)
  add_test(NAME native_api COMMAND native_api_tests "$<TARGET_FILE:ZbarAdd1C>" "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
endif()

if(WIN32 AND ZBAR1C_BUNDLED_DEPS)
  add_custom_target(package_win64 ALL
    COMMAND "${CMAKE_COMMAND}"
      "-DCOMPONENT=$<TARGET_FILE:ZbarAdd1C>"
      "-DDESTINATION=${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}"
      "-DDEPENDENCIES=${CMAKE_BINARY_DIR}/_deps"
      -P "${CMAKE_SOURCE_DIR}/cmake/PackageWindows.cmake"
    DEPENDS ZbarAdd1C
    COMMENT "Prepare DLL, 1C Manifest.xml and ZIP bundle"
    VERBATIM)
endif()
