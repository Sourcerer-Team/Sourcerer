cmake_minimum_required(VERSION 3.24)

if(TARGET Util)
    return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

add_library(Util STATIC
   include/FileLock.h
   include/LockGuard.h
   include/TokenString.h
   src/FileLock.cpp
   src/LockGuard.cpp
   src/TokenString.cpp
)

set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

target_link_libraries(Util PRIVATE
    Threads::Threads
    Qt${QT_VERSION_MAJOR}::Core
)

target_include_directories(Util PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)

includeLib(Util Log)
