cmake_minimum_required(VERSION 3.24)

if(TARGET NewsTracker)
  return()
endif()

project(NewsTracker)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui LinguistTools Network)

set(NEWSTRACKER_SOURCE_FILES
    include/NewsTracker.h
    include/NewsTrackerFsm.h
    include/NewsTrackerState.h
    include/NewsTrackerStructs.h
    include/TrackingState.h
    include/WaitingState.h
    src/NewsTracker.cpp
    src/NewsTrackerFsm.cpp
    src/NewsTrackerState.cpp
    src/TrackingState.cpp
    src/WaitingState.cpp
)

add_library(NewsTracker STATIC ${NEWSTRACKER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(NewsTracker ComponentManager)
includeComponent(NewsTracker ConfigManager)
includeComponent(NewsTracker EventManager)

includeLib(NewsTracker Fsm)
includeLib(NewsTracker Log)
includeLib(NewsTracker Util)


target_link_libraries(NewsTracker PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
)

target_include_directories(NewsTracker PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)
