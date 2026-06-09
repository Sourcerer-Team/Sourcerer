cmake_minimum_required(VERSION 3.24)

if(TARGET QtWaitingSpinner)
    return()
endif()

project(QtWaitingSpinner)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

set(QtWaitingSpinner_SOURCE_FILES
   include/WaitingSpinnerWidget.h
   src/WaitingSpinnerWidget.cpp
)

add_library(QtWaitingSpinner STATIC ${QtWaitingSpinner_SOURCE_FILES})

target_link_libraries(QtWaitingSpinner PRIVATE Qt6::Widgets)

target_include_directories(QtWaitingSpinner PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
