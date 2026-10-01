cmake_minimum_required(VERSION 3.25)
project(cutscene_skip VERSION 1.0.0 LANGUAGES C)

if(NOT WIN32 OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
  message(FATAL_ERROR "Cutscene Skip currently supports Windows x64 only")
endif()

add_library(cutscene_skip SHARED
  src/mod.c
  src/cutscene_skip.c
  src/cutscene_hooks.c)
target_include_directories(cutscene_skip PRIVATE include)
target_link_libraries(cutscene_skip PRIVATE user32)

set(MOD_PLATFORM_DIR "windows-amd64")
set(MOD_LIBRARY_NAME "mod.dll")

set_target_properties(cutscene_skip PROPERTIES
  PREFIX ""
  OUTPUT_NAME "mod"
  C_STANDARD 11
  C_STANDARD_REQUIRED ON)

set(MOD_OUTPUT_DIR "${CMAKE_BINARY_DIR}/cutscene-skip")
set(MOD_LIB_DIR "${MOD_OUTPUT_DIR}/lib/${MOD_PLATFORM_DIR}")

add_custom_command(TARGET cutscene_skip POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory "${MOD_LIB_DIR}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:cutscene_skip>" "${MOD_LIB_DIR}/${MOD_LIBRARY_NAME}"
  COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_SOURCE_DIR}/mod.json" "${MOD_OUTPUT_DIR}/mod.json")
