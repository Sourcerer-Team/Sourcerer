cmake_minimum_required(VERSION 3.24)

if(TARGET Fsm)
    return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(Fsm STATIC
    include/FsmBase.h
    include/FsmEvent.h
    include/FsmState.h
    src/FsmBase.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(Fsm ConfigManager)

includeLib(Fsm Log)
includeLib(Fsm Util)


target_link_libraries(Fsm PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(Fsm PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
