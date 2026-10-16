cmake_minimum_required(VERSION 3.24)

if(TARGET Log)
    return()
endif()

project(Log)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(LOG_SOURCE_FILES
   include/Formatters.h
   include/Log.h
   src/Log.cpp
)

add_library(Log STATIC ${LOG_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(Log ConfigManager)

includeLib(Log Util)

set(ADDITIONAL_LIBRARIES)

target_link_libraries(Log PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    ${ADDITIONAL_LIBRARIES}
)

target_include_directories(Log PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
