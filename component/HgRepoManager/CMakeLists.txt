cmake_minimum_required(VERSION 3.24)

if(TARGET HgRepoManager)
    return()
endif()

project(HgRepoManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Network)

set(HGREPOMANAGER_SOURCE_FILES
  include/HgRepoManager.h
  include/HgRepoManagerEvent.h
  include/HgRepoManagerFsm.h
  include/HgRepoManagerState.h
  include/HgRepoManagerStructs.h
  include/HgRepoManagerInitializationState.h
  include/HgRepoManagerProcessingState.h
  include/HgRepoManagerWorkers.h
  src/HgRepoManager.cpp
  src/HgRepoManagerEvent.cpp
  src/HgRepoManagerFsm.cpp
  src/HgRepoManagerState.cpp
  src/HgRepoManagerInitializationState.cpp
  src/HgRepoManagerProcessingState.cpp
  src/HgRepoManagerWorkers.cpp
  src/HgRepoManagerCredentials.h
  src/HgRepoManagerCredentials.cpp
  src/HgRepoManagerAskPass.h
  src/HgRepoManagerAskPass.cpp
)

add_library(HgRepoManager STATIC ${HGREPOMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(HgRepoManager ComponentManager)
includeComponent(HgRepoManager ConfigManager)
includeComponent(HgRepoManager EventManager)

includeLib(HgRepoManager Fsm)
includeLib(HgRepoManager HgRepo)
# Git and Subversion subrepos are cloned and updated with GitRepo and SvnRepo.
includeLib(HgRepoManager GitRepo)
includeLib(HgRepoManager SvnRepo)
includeLib(HgRepoManager Log)
includeLib(HgRepoManager Util)

target_link_libraries(HgRepoManager PRIVATE
    Qt6::Core
    Qt6::Network
    libgit2package
)

target_include_directories(HgRepoManager PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/libgit2/include
)
set(HgRepoManager_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)
