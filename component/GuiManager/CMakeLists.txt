cmake_minimum_required(VERSION 3.24)

if(TARGET GuiManager)
    return()
endif()

project(GuiManager)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(GUIMANAGER_SOURCE_FILES
   include/GuiManager.h
   src/GuiManager.cpp
)

add_library(GuiManager STATIC ${GUIMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeLib(GuiManager Log)
includeLib(GuiManager Util)

target_link_libraries(GuiManager PRIVATE Qt${QT_VERSION_MAJOR}::Core)

target_include_directories(GuiManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
