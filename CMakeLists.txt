cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)

set(EXCLUDE_COMPONENTS lvgl lvgl_esp32_drivers)
set(EXTRA_COMPONENT_DIRS "src") 

project(Controller)

