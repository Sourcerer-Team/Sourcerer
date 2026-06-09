cmake_minimum_required(VERSION 3.24)

project(EventManager)

set(CMAKE_CXX_STANDARD 17)

find_package(Threads REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(EVENT_MANAGER_SOURCE_FILES
   include/Events.h
   include/EventManager.h
   src/EventManager.cpp
)

add_library(EventManager STATIC ${EVENT_MANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(EventManager ConfigManager)

includeLib(EventManager Log)
includeLib(EventManager Util)

target_link_libraries(EventManager PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(EventManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
