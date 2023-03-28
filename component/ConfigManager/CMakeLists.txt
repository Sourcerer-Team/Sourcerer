cmake_minimum_required(VERSION 3.24)

if(TARGET ConfigManager)
    return()
endif()

project(ConfigManager)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui LinguistTools)

set(CONFIGMANAGER_SOURCE_FILES
   include/ConfigKey.h
   include/ConfigKeyType.h
   include/ConfigGroup.h
   include/ConfigManager.h
   src/ConfigGroup.cpp
   src/ConfigManager.cpp
   GitVersion.h

)

add_library(ConfigManager STATIC ${CONFIGMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeLib(ConfigManager Log)
includeLib(ConfigManager Util)

target_link_libraries(ConfigManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
)

target_include_directories(ConfigManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

execute_process(COMMAND git describe --tags --always
    OUTPUT_VARIABLE GIT_REPO_VERSION)

string(STRIP "${GIT_REPO_VERSION}" GIT_REPO_VERSION)

configure_file("GitVersion.h.in" "GitVersion.h")
