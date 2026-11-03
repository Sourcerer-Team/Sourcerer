cmake_minimum_required(VERSION 3.24)

if(TARGET GitRepoManager)
    return()
endif()

project(GitRepoManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(GITREPOMANAGER_SOURCE_FILES
    include/GitRepoManager.h
    include/GitRepoManagerEvent.h
    include/GitRepoManagerFsm.h
    include/GitRepoManagerState.h
    include/GitRepoManagerStructs.h
    include/GitRepoManagerInitializationState.h
    include/GitRepoManagerProcessingState.h
    src/GitRepoManager.cpp
    src/GitRepoManagerEvent.cpp
    src/GitRepoManagerFsm.cpp
    src/GitRepoManagerState.cpp
    src/GitRepoManagerStructs.cpp
    src/GitRepoManagerInitializationState.cpp
    src/GitRepoManagerProcessingState.cpp
)

add_library(GitRepoManager STATIC ${GITREPOMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(GitRepoManager ComponentManager)
includeComponent(GitRepoManager ConfigManager)
includeComponent(GitRepoManager EventManager)

includeLib(GitRepoManager Fsm)
includeLib(GitRepoManager GitRepo)
includeLib(GitRepoManager Log)
includeLib(GitRepoManager Util)

target_link_libraries(GitRepoManager PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    libgit2package
    ${LIBSSH2_LIBRARY}
)

target_include_directories(GitRepoManager PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}../../lib/libgit2/include
)
