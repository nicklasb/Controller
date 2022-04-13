cmake_minimum_required(VERSION 3.16.0)
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
include_directories(./src)
include_directories(./lib)
include_directories(./secret)
include_directories(./components)

project(Controller)
