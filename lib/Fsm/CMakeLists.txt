cmake_minimum_required(VERSION 3.24)

if(TARGET Fsm)
    return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(Fsm STATIC
    include/Fsm.h
    include/FsmEvent.h
    include/FsmState.h
    src/Fsm.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(Fsm ConfigManager)

includeLib(Fsm Log)
includeLib(Fsm Util)


target_link_libraries(Fsm PRIVATE
    Threads::Threads
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_include_directories(Fsm PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
