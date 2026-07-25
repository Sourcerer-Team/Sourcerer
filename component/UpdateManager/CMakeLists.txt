cmake_minimum_required(VERSION 3.24)

if(TARGET UpdateManager)
  return()
endif()

project(UpdateManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(UPDATEMANAGER_SOURCE_FILES
    include/UpdateManager.h
    include/UpdateManagerFsm.h
    include/UpdateManagerState.h
    include/UpdatingState.h
    include/WaitingState.h
    src/UpdateManager.cpp
    src/UpdateManagerFsm.cpp
    src/UpdateManagerState.cpp
    src/UpdatingState.cpp
    src/WaitingState.cpp
)

add_library(UpdateManager STATIC ${UPDATEMANAGER_SOURCE_FILES}
  include/UpdateManagerEvent.h
  src/UpdateManagerEvent.cpp)

include(../../cmake/helpers.cmake)

includeComponent(UpdateManager ComponentManager)
includeComponent(UpdateManager ConfigManager)
includeComponent(UpdateManager NewsTracker)

includeLib(UpdateManager Fsm)
includeLib(UpdateManager Log)
includeLib(UpdateManager Util)


target_link_libraries(UpdateManager PRIVATE
    Qt6::Core
    Qt6::Network
)

target_include_directories(UpdateManager PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../NewsTracker/include
)
