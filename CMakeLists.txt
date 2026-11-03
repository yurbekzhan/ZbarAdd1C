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
  target_sources(ZbarAdd1C PRIVATE resources/exports.def)
  enable_language(RC)
  if(CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ZBAR1C_ARCH x64)
    set(ZBAR1C_MANIFEST_ARCH amd64)
    set(ZBAR1C_BUNDLE_ARCH x86_64)
  else()
    set(ZBAR1C_ARCH x86)
    set(ZBAR1C_MANIFEST_ARCH x86)
    set(ZBAR1C_BUNDLE_ARCH i386)
  endif()
  configure_file(resources/component.manifest "${CMAKE_CURRENT_BINARY_DIR}/component.manifest" @ONLY)
  configure_file(resources/Manifest.single.xml.in "${CMAKE_CURRENT_BINARY_DIR}/Manifest.xml" @ONLY)
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

option(ZBAR1C_DUAL_ARCH "Build and package both Windows x86 and x64" OFF)
if(WIN32 AND ZBAR1C_BUNDLED_DEPS)
  set(package_dependencies ZbarAdd1C)
  set(package_arguments
      "-DCOMPONENT=$<TARGET_FILE:ZbarAdd1C>"
      "-DARCH=${ZBAR1C_ARCH}"
      "-DSINGLE_MANIFEST=${CMAKE_CURRENT_BINARY_DIR}/Manifest.xml")
  if(ZBAR1C_DUAL_ARCH)
    if(NOT MSVC)
      message(FATAL_ERROR "Dual architecture builds require MSVC and Visual Studio 2022")
    endif()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
      set(other_platform Win32)
      set(other_arch x86)
    else()
      set(other_platform x64)
      set(other_arch x64)
    endif()
    set(other_build "${CMAKE_BINARY_DIR}/other-${other_arch}")
    add_custom_target(build_other_arch
      COMMAND "${CMAKE_COMMAND}" -S "${CMAKE_SOURCE_DIR}" -B "${other_build}"
        -G "Visual Studio 17 2022" -A "${other_platform}"
        -DZBAR1C_DUAL_ARCH=OFF -DZBAR1C_BUNDLED_DEPS=ON "-DBUILD_TESTING=${BUILD_TESTING}"
      COMMAND "${CMAKE_COMMAND}" --build "${other_build}" --config "$<CONFIG>" --parallel 2
      VERBATIM)
    list(APPEND package_dependencies build_other_arch)
    list(APPEND package_arguments "-DOTHER_COMPONENT=${other_build}/$<CONFIG>/ZbarAdd1C.dll")
  endif()
  add_custom_target(package_windows ALL
    COMMAND "${CMAKE_COMMAND}"
      ${package_arguments}
      "-DDESTINATION=${CMAKE_BINARY_DIR}/dist/$<CONFIG>"
      "-DSOURCE=${CMAKE_SOURCE_DIR}"
      "-DDEPENDENCIES=${CMAKE_BINARY_DIR}/_deps"
      -P "${CMAKE_SOURCE_DIR}/cmake/PackageWindows.cmake"
    DEPENDS ${package_dependencies}
    COMMENT "Prepare Windows DLLs, 1C manifest and ZIP bundle"
    VERBATIM)
endif()
