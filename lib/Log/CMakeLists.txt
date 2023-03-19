cmake_minimum_required(VERSION 3.24)

if(TARGET Log)
    return()
endif()

project(Log)

set(CMAKE_CXX_STANDARD 17)

set(LOG_SOURCE_FILES
   include/Log.h
   src/Log.cpp
)

add_library(Log STATIC ${LOG_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeLib(Log Util)

target_include_directories(Log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
