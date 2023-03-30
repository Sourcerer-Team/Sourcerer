cmake_minimum_required(VERSION 3.24)

if(TARGET GitRepoManager)
    return()
endif()

project(GitRepoManager)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(GITREPOMANAGER_SOURCE_FILES
    include/GitRepoManager.h
    include/GitRepoManagerEvent.h
    include/GitRepoManagerFsm.h
    include/GitRepoManagerState.h
    include/InitializationState.h
    include/ProcessingState.h
    src/GitRepoManager.cpp
    src/GitRepoManagerEvent.cpp
    src/GitRepoManagerFsm.cpp
    src/GitRepoManagerState.cpp
    src/InitializationState.cpp
    src/ProcessingState.cpp
)

add_library(GitRepoManager STATIC ${GITREPOMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(GitRepoManager ConfigManager)
includeComponent(GitRepoManager EventManager)

includeLib(GitRepoManager Fsm)
includeLib(GitRepoManager GitRepo)
includeLib(GitRepoManager Log)
includeLib(GitRepoManager Util)

target_link_libraries(GitRepoManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_include_directories(GitRepoManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
