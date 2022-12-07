# This file was automatically generated for projects
# without default 'CMakeLists.txt' file.

FILE(GLOB_RECURSE app_sources "main.c" ${CMAKE_SOURCE_DIR}/src/sdp_task.* ${CMAKE_SOURCE_DIR}/secret/*.*)

idf_component_register(SRCS ${app_sources}

INCLUDE_DIRS "../secret" "../lib"
REQUIRES sdp #lvgl lvgl_esp32_drivers
)
