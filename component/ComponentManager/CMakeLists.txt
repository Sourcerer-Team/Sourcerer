cmake_minimum_required(VERSION 3.24)

if(TARGET ComponentManager)
    return()
endif()

project(ComponentManager)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(COMPONENTMANAGER_SOURCE_FILES
   include/ComponentManager.h
   src/ComponentManager.cpp
)

add_library(ComponentManager STATIC ${COMPONENTMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(ComponentManager ConfigManager)
includeComponent(ComponentManager EventManager)

includeLib(ComponentManager Log)
includeLib(ComponentManager Util)

target_link_libraries(ComponentManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_include_directories(ComponentManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
