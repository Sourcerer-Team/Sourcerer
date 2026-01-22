cmake_minimum_required(VERSION 3.24)

if(TARGET GitRepo)
    return()
endif()

project(GitRepo)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(libgit2 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(GitRepo STATIC
    include/GitRepo.h
    include/GitCredentialStorage.h
    src/GitRepo.cpp
    src/GitCredentialStorage.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(GitRepo ConfigManager)

includeLib(GitRepo Log)
includeLib(GitRepo Util)


target_link_libraries(GitRepo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    ${LIBGIT2_LIBRARIES}
)

target_include_directories(GitRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${LIBGIT2_INCLUDE_DIR}
)
