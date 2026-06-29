cmake_minimum_required(VERSION 3.24)

if(TARGET GitRepo)
    return()
endif()

project(GitRepo)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(LibSSH2 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets LinguistTools)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(GitRepo STATIC
    include/GitRepo.h
    include/GitCredentialStorage.h
    include/GitKeyStorage.h
    src/GitRepo.cpp
    src/GitCredentialStorage.cpp
    src/GitKeyStorage.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(GitRepo ConfigManager)

includeLib(GitRepo Log)
includeLib(GitRepo OpenSSLHelper)
includeLib(GitRepo Util)

set(WITH_ossl_PLUGIN ON CACHE BOOL "Enable OpenSSL plugin for QCA")
set(BUILD_WITH_QT6 ON CACHE BOOL "Build with Qt 6")
set(BUILD_TESTS OFF CACHE BOOL "Create test")
set(BUILD_TOOLS OFF CACHE BOOL "Compile mozcerts and qcatool")
set(BUILD_PLUGINS "none" CACHE STRING "Plugins for building (also possible values: none, all and auto)")
set(BUILD_SHARED_LIBS ON CACHE BOOL "Build shared library")

add_subdirectory(../../lib/qca "QCA")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Disable test suite" FORCE)
set(BUILD_CLI OFF CACHE BOOL "Disable CLI" FORCE)
set(USE_THREADS ON CACHE BOOL "Enable threads" FORCE)
set(USE_SSH ON CACHE BOOL "Enable SSH" FORCE)
set(USE_HTTP ON CACHE BOOL "Enable HTTP" FORCE)
set(USE_HTTPS OpenSSL CACHE BOOL "Enable HTTPS" FORCE)
add_subdirectory(../../lib/libgit2 "libgit2")

target_link_libraries(GitRepo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    qca-qt6
    Threads::Threads
    OpenSSL::SSL   
    OpenSSL::Crypto
    libgit2
    util
    ${LIBSSH2_LIBRARY}
)

target_include_directories(GitRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../libgit2/include
   ../../lib/qca/include/QtCrypto
   ${CMAKE_CURRENT_BINARY_DIR}/QCA
   ${CMAKE_CURRENT_BINARY_DIR}/..
)
