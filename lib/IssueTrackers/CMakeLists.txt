cmake_minimum_required(VERSION 3.24)

if(TARGET IssueTrackers)
  return()
endif()

project(IssueTrackers)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core)

add_library(IssueTrackers STATIC
    include/GitHubRepo.h
    include/IssueTrackerRepo.h
    src/GitHubRepo.cpp
    src/IssueTrackerRepo.cpp
)



target_link_libraries(IssueTrackers PRIVATE
    Qt6::Core
)

target_include_directories(IssueTrackers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
