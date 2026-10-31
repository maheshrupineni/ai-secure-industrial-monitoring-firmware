cmake_minimum_required(VERSION 3.22)

if(NOT DEFINED PROJECT_VER)
    set(PROJECT_VER "0.1.0")
endif()

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(industrial_monitor VERSION 0.1.0)
