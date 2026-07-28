cmake_minimum_required(VERSION 3.24)

if(TARGET StylesheetManager)
    return()
endif()

project(StylesheetManager)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Svg LinguistTools)

set(STYLESHEETMANAGER_SOURCE_FILES
   include/StylesheetKey.h
   include/StylesheetManager.h

   src/StylesheetManager.cpp
)

add_library(StylesheetManager STATIC ${STYLESHEETMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(StylesheetManager ConfigManager)

includeLib(StylesheetManager Log)
includeLib(StylesheetManager Util)

target_link_libraries(StylesheetManager PRIVATE
    Qt6::Core
    Qt6::Widgets
    Qt6::Svg
)

target_include_directories(StylesheetManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
