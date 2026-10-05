cmake_minimum_required(VERSION 3.24)

if(TARGET GitRepo)
  return()
endif()

project(GitRepo)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets LinguistTools)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(zlib REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(GitRepo STATIC
    include/GitRepo.h
    include/WorkflowUtils.h
    include/GraphiteUtils.h
    include/GitFlowNextWorkflow.h
    include/GitCredentialStorage.h
    include/GitKeyStorage.h
    include/PatchFileUtils.h
    include/GitLfs.h
    src/GitRepo.cpp
    src/GitCredentialStorage.cpp
    src/GitKeyStorage.cpp
    src/GitLfs.cpp
    src/PatchFileUtils.cpp
)

if(MSVC)
    # fopen() and strdup() are used portably, silence MSVC deprecation.
    target_compile_definitions(GitRepo PRIVATE _CRT_SECURE_NO_WARNINGS
                                               _CRT_NONSTDC_NO_WARNINGS)
endif()

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
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared library")

add_subdirectory(../../lib/qca "QCA")


set(ZLIB_BUILD_STATIC OFF CACHE BOOL "Enable static libraries" FORCE)
set(ZLIB_BUILD_SHARED ON CACHE BOOL "Enable shared libraries" FORCE)
set(ZLIB_BUILD_TESTING OFF CACHE BOOL "Disable tests" FORCE)
add_subdirectory(../../lib/zlib "zlib")

set(BUILD_STATIC_LIBS ON CACHE BOOL "Enable static libraries" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Disable tests" FORCE)
set(LIBSSH2_BUILD_DOCS OFF CACHE BOOL "Disable docs" FORCE)
set(LIBSSH2_DISABLE_INSTALL ON CACHE BOOL "Disable install targets" FORCE)

# libssh2 adds /W4 itself, drop the default /W3 to avoid MSVC D9025 warnings.
set(_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
set(_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
add_subdirectory(../../lib/libssh2 "libssh2")
set(CMAKE_C_FLAGS "${_SAVED_C_FLAGS}")
set(CMAKE_CXX_FLAGS "${_SAVED_CXX_FLAGS}")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries" FORCE)
set(BUILD_TOOLS OFF CACHE BOOL "Disable tools" FORCE)
set(BUILD_REGRESS OFF CACHE BOOL "Disable regress" FORCE)
set(BUILD_OSSFUZZ OFF CACHE BOOL "Disable ossfuzz" FORCE)
set(BUILD_EXAMPLES OFF CACHE BOOL "Disable examples" FORCE)
set(BUILD_DOC OFF CACHE BOOL "Disable docs" FORCE)
set(LIBZIP_DO_INSTALL OFF CACHE BOOL "Disable install targets" FORCE)
set(ENABLE_ZSTD OFF CACHE BOOL "Disable Zstandard" FORCE)
set(ENABLE_BZIP2 OFF CACHE BOOL "Disable BZip2" FORCE)
set(ENABLE_LZMA OFF CACHE BOOL "Disable LZMA" FORCE)

add_subdirectory(../../lib/libzip "libzip")

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries" FORCE)
set(BUILD_TESTS OFF CACHE BOOL "Disable test suite" FORCE)
set(BUILD_CLI OFF CACHE BOOL "Disable CLI" FORCE)
set(USE_THREADS ON CACHE BOOL "Enable threads" FORCE)
set(USE_SSH ON CACHE BOOL "Enable SSH" FORCE)
set(USE_HTTP ON CACHE BOOL "Enable HTTP" FORCE)
set(USE_HTTPS OpenSSL CACHE BOOL "Enable HTTPS" FORCE)
set(USE_AUTH_NTLM OFF CACHE BOOL "Enable NTLM authentication" FORCE)
set(USE_COMPRESSION "zlib" CACHE STRING "Selects compression backend.")
set(USE_REGEX "builtin" CACHE STRING "Selects regex provider.")

set(USE_GSSAPI OFF CACHE BOOL "Disable Kerberos/GSSAPI" FORCE)
# Use the same dynamic CRT as the rest of the application.
set(STATIC_CRT OFF CACHE BOOL "Link the static CRT libraries" FORCE)
# libgit2 still declares cmake_minimum_required() < 3.10, raise its policy
# version to the project minimum to avoid the CMake deprecation warning.
set(CMAKE_POLICY_VERSION_MINIMUM 3.10)
add_subdirectory(../../lib/libgit2 "libgit2")
unset(CMAKE_POLICY_VERSION_MINIMUM)

add_subdirectory(../../lib/qtkeychain "qtkeychain")

target_link_libraries(GitRepo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    qca-qt6
    qca-ossl
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    libgit2
    libssh2_static
    util
    qt6keychain
    ${LIBSSH2_LIBRARY}
)

target_include_directories(GitRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../libgit2/include
   ../../lib/qca/include/QtCrypto
   ${CMAKE_CURRENT_BINARY_DIR}/QCA
   ${CMAKE_CURRENT_BINARY_DIR}/..
)
