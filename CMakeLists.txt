cmake_minimum_required(VERSION 3.25)
project(cutscene_skip VERSION 1.1.0 LANGUAGES C)

include(CheckCSourceCompiles)

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Cutscene Skip supports 64-bit targets only")
endif()

if(APPLE AND CMAKE_OSX_ARCHITECTURES MATCHES ";")
  message(FATAL_ERROR "Build one macOS architecture at a time (CMAKE_OSX_ARCHITECTURES=arm64 or x86_64): "
                      "Foxhollow loads lib/macos-arm64 and lib/macos-x86_64 separately")
endif()

check_c_source_compiles("
#if !defined(__x86_64__) && !defined(_M_X64)
#error not x86_64
#endif
int main(void) { return 0; }" CUTSCENE_SKIP_TARGET_X86_64)
check_c_source_compiles("
#if !defined(__aarch64__) && !defined(_M_ARM64)
#error not arm64
#endif
int main(void) { return 0; }" CUTSCENE_SKIP_TARGET_ARM64)

if(WIN32 AND CUTSCENE_SKIP_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "windows-amd64")
elseif(APPLE AND CUTSCENE_SKIP_TARGET_ARM64)
  set(MOD_PLATFORM_DIR "macos-arm64")
elseif(APPLE AND CUTSCENE_SKIP_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "macos-x86_64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CUTSCENE_SKIP_TARGET_ARM64)
  set(MOD_PLATFORM_DIR "linux-arm64")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CUTSCENE_SKIP_TARGET_X86_64)
  set(MOD_PLATFORM_DIR "linux-amd64")
else()
  message(FATAL_ERROR "Cutscene Skip supports Windows x64, Linux x86_64/arm64 and macOS x86_64/arm64; "
                      "${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR} is not supported yet")
endif()

add_library(cutscene_skip SHARED
  src/mod.c
  src/cutscene_skip.c
  src/cutscene_hooks.c)
target_include_directories(cutscene_skip PRIVATE include)

if(WIN32)
  target_sources(cutscene_skip PRIVATE src/platform_input_windows.c)
  target_link_libraries(cutscene_skip PRIVATE user32)
  set(MOD_LIBRARY_NAME "mod.dll")
else()
  target_sources(cutscene_skip PRIVATE src/platform_input_sdl.c)
  target_link_libraries(cutscene_skip PRIVATE ${CMAKE_DL_LIBS})
  set_target_properties(cutscene_skip PROPERTIES SUFFIX ".so")
  set(MOD_LIBRARY_NAME "mod.so")
endif()

set_target_properties(cutscene_skip PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mod"
  C_STANDARD 11
  C_STANDARD_REQUIRED ON
  C_VISIBILITY_PRESET hidden)

set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/cutscene-skip")
set(MOD_LIB_DIR "${MOD_OUTPUT_DIR}/lib/${MOD_PLATFORM_DIR}")

add_custom_command(TARGET cutscene_skip POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_LIB_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:cutscene_skip>" "${MOD_LIB_DIR}/${MOD_LIBRARY_NAME}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/mod.json" "${MOD_OUTPUT_DIR}/mod.json")
