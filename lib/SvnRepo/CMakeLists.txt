cmake_minimum_required(VERSION 3.24)

if(TARGET SvnRepo)
  return()
endif()

project(SvnRepo)

set(CMAKE_CXX_STANDARD 17)
set(THREADS_PREFER_PTHREAD_FLAG ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(Qt6 REQUIRED COMPONENTS Core Gui Network Widgets LinguistTools)
find_package(Threads REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(zlib REQUIRED)

add_compile_definitions(QT_USE_STRING_BUILDER)

add_library(SvnRepo STATIC
)

include(../../cmake/helpers.cmake)

includeComponent(SvnRepo ConfigManager)

includeLib(SvnRepo Log)
includeLib(SvnRepo Util)

set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "Disable tools")
set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples")
set(EXPAT_BUILD_TESTS OFF CACHE BOOL "Disable tests")
set(EXPAT_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries")
set(EXPAT_BUILD_DOCS  OFF CACHE BOOL "Disable docs")
set(EXPAT_BUILD_FUZZERS  OFF CACHE BOOL "Disable fuzzers")
set(EXPAT_BUILD_PKGCONFIG  OFF CACHE BOOL "Disable pkgconfig")
add_subdirectory(../libexpat/expat "expat")

if(WIN32)

set(APR_BUILD_SHARED OFF CACHE BOOL "Disable shared libraries")
set(APR_BUILD_STATIC ON CACHE BOOL "Enable static libraries")
add_subdirectory(../apr "apr")

set(APU_HAVE_CRYPTO ON CACHE BOOL "Enable crypto")
SET(APR_INCLUDE_DIR         "${CMAKE_CURRENT_BINARY_DIR}/apr" "${CMAKE_CURRENT_SOURCE_DIR}/../apr/include" CACHE STRING "Directory with APR include files")
SET(APR_LIBRARIES           apr-1  CACHE STRING "APR library to link with" FORCE)
add_subdirectory(../apr-util "apr-util")

else()

set(APR_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/apr-install)
set(APR_UTIL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/apr-util-install)
set(APR_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/apr-build)
set(APR_UTIL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/apr-util-build)
file(MAKE_DIRECTORY
    ${APR_INSTALL_DIR}/include/apr-1
    ${APR_INSTALL_DIR}/lib
    ${APR_UTIL_INSTALL_DIR}/include
    ${APR_UTIL_INSTALL_DIR}/lib
    ${APR_BUILD_DIR}
    ${APR_UTIL_BUILD_DIR}
)

set(APR_LIBRARY ${APR_INSTALL_DIR}/lib/libapr-1.a)
set(APR_UTIL_LIBRARY ${APR_UTIL_INSTALL_DIR}/lib/libaprutil-1.a)

add_custom_command(
    OUTPUT ${APR_LIBRARY}
    COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr.sh ${CMAKE_CURRENT_LIST_DIR}/../apr ${APR_BUILD_DIR} ${APR_INSTALL_DIR}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr.sh ${CMAKE_CURRENT_LIST_DIR}/../apr/buildconf ${CMAKE_CURRENT_LIST_DIR}/../apr/configure.in
    VERBATIM
)

add_custom_target(apr-build ALL DEPENDS ${APR_LIBRARY})

add_library(apr-1 STATIC IMPORTED GLOBAL)
set_target_properties(apr-1 PROPERTIES
    IMPORTED_LOCATION ${APR_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${APR_INSTALL_DIR}/include/apr-1
)
add_dependencies(apr-1 apr-build)

add_custom_command(
    OUTPUT ${APR_UTIL_LIBRARY}
    COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr-util.sh ${CMAKE_CURRENT_LIST_DIR}/../apr-util ${APR_UTIL_BUILD_DIR} ${APR_UTIL_INSTALL_DIR} ${APR_INSTALL_DIR} ${CMAKE_CURRENT_LIST_DIR}/../apr ${CMAKE_CURRENT_LIST_DIR}/../libexpat/expat ${CMAKE_CURRENT_BINARY_DIR}/expat
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr-util.sh ${CMAKE_CURRENT_LIST_DIR}/../apr-util/buildconf ${CMAKE_CURRENT_LIST_DIR}/../apr-util/configure.in ${APR_LIBRARY} expat
    VERBATIM
)

add_custom_target(apr-util-build ALL DEPENDS ${APR_UTIL_LIBRARY})

add_library(aprutil-1 STATIC IMPORTED GLOBAL)
set_target_properties(aprutil-1 PROPERTIES
    IMPORTED_LOCATION ${APR_UTIL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES ${APR_UTIL_INSTALL_DIR}/include
)
add_dependencies(aprutil-1 apr-util-build)

endif()

target_link_libraries(SvnRepo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    qca-qt6
    qca-ossl
    Threads::Threads
    OpenSSL::SSL
    OpenSSL::Crypto
    apr-1
    aprutil-1
    expat::expat
)

target_include_directories(SvnRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ../../lib/qca/include/QtCrypto
   ${CMAKE_CURRENT_BINARY_DIR}/QCA
   ${CMAKE_CURRENT_BINARY_DIR}/..
)
