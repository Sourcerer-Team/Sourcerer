cmake_minimum_required(VERSION 3.24)

if(TARGET SvnRepoManager)
    return()
endif()

project(SvnRepoManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core)

set(SVNREPOMANAGER_SOURCE_FILES
  include/SvnRepoManager.h
  include/SvnRepoManagerEvent.h
  include/SvnRepoManagerFsm.h
  include/SvnRepoManagerState.h
  include/SvnRepoManagerStructs.h
  include/SvnRepoManagerInitializationState.h
  include/SvnRepoManagerProcessingState.h
  src/SvnRepoManager.cpp
  src/SvnRepoManagerEvent.cpp
  src/SvnRepoManagerFsm.cpp
  src/SvnRepoManagerState.cpp
  src/SvnRepoManagerStructs.cpp
  src/SvnRepoManagerInitializationState.cpp
  src/SvnRepoManagerProcessingState.cpp
)

add_library(SvnRepoManager STATIC ${SVNREPOMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(SvnRepoManager ComponentManager)

includeLib(SvnRepoManager Fsm)
includeLib(SvnRepoManager SvnRepo)

target_link_libraries(SvnRepoManager PRIVATE
    Qt6::Core
    apr-1
)

target_include_directories(SvnRepoManager PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/subversion/subversion/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/subversion/subversion/bindings/cxx/include
)
