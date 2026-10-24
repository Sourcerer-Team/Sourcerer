cmake_minimum_required(VERSION 3.24)

if(TARGET HgRepo)
  return()
endif()

project(HgRepo)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core)

add_compile_definitions(QT_USE_STRING_BUILDER)

# Mercurial is not linked in: HgRepo drives the hg executable installed by
# the user through its command server.
add_library(HgRepo STATIC
    include/HgRepo.h
    src/HgCommandServer.h
    src/HgCommandServer.cpp
    src/HgRc.h
    src/HgRc.cpp
    src/HgRepoPrivate.h
    src/HgRepo.cpp
    src/HgRepoAuth.cpp
    src/HgRepoBisect.cpp
    src/HgRepoBranches.cpp
    src/HgRepoDiff.cpp
    src/HgRepoHistory.cpp
    src/HgRepoRemotes.cpp
    src/HgRepoShelves.cpp
    src/HgRepoShares.cpp
    src/HgRepoSubrepos.cpp
    src/HgRepoLfs.cpp
    src/HgRepoWorkflows.cpp
    src/HgRepoWorkingCopy.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(HgRepo ConfigManager)

includeLib(HgRepo Log)
includeLib(HgRepo Util)
# Only the GitRepo types are used, HgRepo doesn't link GitRepo or libgit2.
includeLibFiles(HgRepo GitRepo)

target_link_libraries(HgRepo PUBLIC
    Qt6::Core
)

target_include_directories(HgRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_include_directories(HgRepo PUBLIC
   # GitRepo.h includes libgit2 headers.
   ${CMAKE_CURRENT_SOURCE_DIR}/../libgit2/include
)
set(HgRepo_INCLUDE_PATH ${CMAKE_CURRENT_LIST_DIR}/include)
