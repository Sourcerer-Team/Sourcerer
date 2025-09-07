cmake_minimum_required(VERSION 3.24)

if(TARGET Util)
    return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)

add_library(Util STATIC
   include/Dispatcher.h
   include/FileLock.h
   include/General.h
   include/Graph.h
   include/Git.h
   include/LockGuard.h
   include/ResizableVector.h
   include/ResizableMatrix.h
   include/TokenString.h
   include/Uuid.h
   src/Dispatcher.cpp
   src/FileLock.cpp
   src/General.cpp
   src/Git.cpp
   src/LockGuard.cpp
   src/TokenString.cpp
   src/Uuid.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(Util ConfigManager)

includeLib(Util Log)


target_link_libraries(Util PRIVATE
    Threads::Threads
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

target_include_directories(Util PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
