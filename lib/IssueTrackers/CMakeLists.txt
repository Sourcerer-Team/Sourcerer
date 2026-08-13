cmake_minimum_required(VERSION 3.24)

if(TARGET IssueTrackers)
  return()
endif()

project(IssueTrackers)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Network Widgets)
find_package(OpenSSL REQUIRED)

add_library(IssueTrackers STATIC
    include/AzureRepo.h
    include/BitbucketRepo.h
    include/GitHubRepo.h
    include/GitLabRepo.h
    include/IssueTrackerRepo.h
    include/TokensStorage.h
    src/AzureRepo.cpp
    src/BitbucketRepo.cpp
    src/GitHubRepo.cpp
    src/GitLabRepo.cpp
    src/IssueTrackerRepo.cpp
    src/TokensStorage.cpp
)

include(../../cmake/helpers.cmake)

includeLib(IssueTrackers Log)
includeLib(IssueTrackers Util)

target_link_libraries(IssueTrackers PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    OpenSSL::SSL
    OpenSSL::Crypto
)

target_include_directories(IssueTrackers PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
