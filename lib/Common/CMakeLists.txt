cmake_minimum_required(VERSION 3.24)

if(TARGET Common)
  return()
endif()

project(Common)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(Common STATIC
    include/UserProfileManager.h
    src/UserProfileManager.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(Common ConfigManager)

includeLib(Common Log)
includeLib(Common Util)


target_link_libraries(Common PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    libgit2
    util
)

target_include_directories(Common PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../libgit2/include
)
