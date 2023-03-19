cmake_minimum_required(VERSION 3.24)

if(TARGET Log)
    return()
endif()

project(Log)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(LOG_SOURCE_FILES
   include/Log.h
   src/Log.cpp
)

add_library(Log STATIC ${LOG_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(Log ConfigManager)

includeLib(Log Util)

target_link_libraries(Log PRIVATE Qt${QT_VERSION_MAJOR}::Core stdc++fs)

target_include_directories(Log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
