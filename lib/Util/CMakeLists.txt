cmake_minimum_required(VERSION 3.24)

if(TARGET Util)
    return()
endif()

project(Util)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(Util STATIC
   include/FileLock.h
   include/General.h
   include/GitRepo.h
   include/LockGuard.h
   include/TokenString.h
   src/FileLock.cpp
   src/General.cpp
   src/GitRepo.cpp
   src/LockGuard.cpp
   src/TokenString.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(Util ConfigManager)

includeLib(Util Log)


target_link_libraries(Util PRIVATE
    Threads::Threads
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_include_directories(Util PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
