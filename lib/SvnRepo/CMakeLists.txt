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
    include/SvnRepo.h
    src/SvnRepo.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(SvnRepo ConfigManager)

includeLib(SvnRepo Log)
includeLib(SvnRepo Util)

set(CYRUS_SASL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cyrus-sasl)
set(CYRUS_SASL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyrus-sasl-install)

if(WIN32)
set(CYRUS_SASL_LIBRARY ${CYRUS_SASL_INSTALL_DIR}/lib/libsasl.lib)
file(MAKE_DIRECTORY
    ${CYRUS_SASL_INSTALL_DIR}/include
    ${CYRUS_SASL_INSTALL_DIR}/lib
    ${CYRUS_SASL_INSTALL_DIR}/bin
)

add_custom_command(
    OUTPUT ${CYRUS_SASL_LIBRARY}
    COMMAND cmd /c "cd /D ${CYRUS_SASL_SOURCE_DIR}/include && nmake /f NTMakefile install prefix=${CYRUS_SASL_INSTALL_DIR} OPENSSL_INCLUDE=${OPENSSL_INCLUDE_DIR} OPENSSL_LIBPATH=$<TARGET_FILE_DIR:OpenSSL::SSL> DISABLE_RES=1 && cd /D ${CYRUS_SASL_SOURCE_DIR}/lib && nmake /f NTMakefile install prefix=${CYRUS_SASL_INSTALL_DIR} OPENSSL_INCLUDE=${OPENSSL_INCLUDE_DIR} OPENSSL_LIBPATH=$<TARGET_FILE_DIR:OpenSSL::SSL> DISABLE_RES=1"
    WORKING_DIRECTORY ${CYRUS_SASL_SOURCE_DIR}
    DEPENDS ${CYRUS_SASL_SOURCE_DIR}/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/win32/common.mak ${CYRUS_SASL_SOURCE_DIR}/lib/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/plugins/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/utils/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/sample/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/sasldb/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/include/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/win32/include/NTMakefile
    VERBATIM
)

else()
set(CYRUS_SASL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyrus-sasl-build)
set(CYRUS_SASL_LIBRARY ${CYRUS_SASL_INSTALL_DIR}/lib/libsasl2.a)
file(MAKE_DIRECTORY
    ${CYRUS_SASL_INSTALL_DIR}/include
    ${CYRUS_SASL_INSTALL_DIR}/lib
    ${CYRUS_SASL_INSTALL_DIR}/bin
    ${CYRUS_SASL_BUILD_DIR}
)

add_custom_command(
    OUTPUT ${CYRUS_SASL_LIBRARY}
    COMMAND sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-cyrus-sasl.sh ${CYRUS_SASL_SOURCE_DIR} ${CYRUS_SASL_BUILD_DIR} ${CYRUS_SASL_INSTALL_DIR}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-cyrus-sasl.sh ${CYRUS_SASL_SOURCE_DIR}/autogen.sh ${CYRUS_SASL_SOURCE_DIR}/configure.ac
    VERBATIM
)

endif()

add_custom_target(cyrus-sasl-build ALL DEPENDS ${CYRUS_SASL_LIBRARY})
add_library(cyrus-sasl STATIC IMPORTED GLOBAL)
set_target_properties(cyrus-sasl PROPERTIES
    IMPORTED_LOCATION ${CYRUS_SASL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${CYRUS_SASL_INSTALL_DIR}/include;${CYRUS_SASL_SOURCE_DIR}/include"
)
add_dependencies(cyrus-sasl cyrus-sasl-build)
add_dependencies(SvnRepo cyrus-sasl-build)

add_subdirectory(../sqlite "sqlite")

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

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/apr)
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/../apr/include/apr_version.h
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/apr
)

set(APU_HAVE_CRYPTO ON CACHE BOOL "Enable crypto")
set(apu_have_sqlite3_10 1)
SET(APR_INCLUDE_DIR         "${CMAKE_CURRENT_BINARY_DIR}/apr" CACHE STRING "Directory with APR include files" FORCE)
SET(APR_LIBRARIES           apr-1  CACHE STRING "APR library to link with" FORCE)
add_subdirectory(../apr-util "apr-util")

target_compile_definitions(aprutil-1 PRIVATE APU_HAVE_SQLITE3=1)
target_link_libraries(aprutil-1 sqlite3)
set_target_properties(aprutil-1 PROPERTIES
    INTERFACE_COMPILE_DEFINITIONS APU_HAVE_CRYPTO=1
)

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
if(UNIX AND NOT WIN32)
  set_target_properties(apr-1 PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS "PATH_MAX=4096"
  )
endif()
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
    INTERFACE_COMPILE_DEFINITIONS APU_HAVE_CRYPTO=1
)
if(UNIX AND NOT WIN32)
  set_target_properties(aprutil-1 PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS "PATH_MAX=4096"
  )
endif()
add_dependencies(aprutil-1 apr-util-build)

endif()

set(SERF_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../serf)
set(APR_UTIL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../apr-util)
set(APR_UTIL_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/apr-util)
file(GLOB SERF_SOURCES CONFIGURE_DEPENDS
    ${SERF_SOURCE_DIR}/*.c
    ${SERF_SOURCE_DIR}/auth/*.c
    ${SERF_SOURCE_DIR}/buckets/*.c
)

add_library(serf STATIC ${SERF_SOURCES})

target_include_directories(serf PRIVATE
    ${SERF_SOURCE_DIR}
    ${SERF_SOURCE_DIR}/auth
    ${SERF_SOURCE_DIR}/buckets
    ${CMAKE_CURRENT_SOURCE_DIR}/../apr-util/include
    ${CMAKE_CURRENT_BINARY_DIR}/apr-util
)

target_include_directories(serf INTERFACE
    ${SERF_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../apr-util/include
    ${CMAKE_CURRENT_BINARY_DIR}/apr-util
)

target_compile_definitions(serf PRIVATE
    OPENSSL_NO_STDIO
    APR_DECLARE_STATIC
    APU_DECLARE_STATIC
)

if(UNIX AND NOT WIN32)
    target_compile_definitions(serf PRIVATE PATH_MAX=4096)
endif()

if(WIN32)
    target_compile_definitions(serf PRIVATE SERF_HAVE_SSPI)
    target_link_libraries(serf PRIVATE
        ws2_32
        crypt32
        mswsock
        rpcrt4
        secur32
        user32
        advapi32
        gdi32
    )
endif()

target_link_libraries(serf PRIVATE
    apr-1
    aprutil-1
    OpenSSL::SSL
    OpenSSL::Crypto
    ZLIB::ZLIB
)

add_dependencies(serf apr-1 aprutil-1 zlib)
add_dependencies(SvnRepo serf)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries")
set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "Disable testing")
set(LIB_FUZZING_ENGINE OFF CACHE BOOL "Disable fuzzing")
add_subdirectory(../utf8proc "utf8proc")

add_library(external-serf INTERFACE)
target_link_libraries(external-serf INTERFACE serf)
target_include_directories(external-serf INTERFACE ${SERF_SOURCE_DIR})

set(SVN_IN_TREE_APR ON)
set(SVN_ENABLE_SVNXX ON CACHE BOOL "Enable C++ bindings" FORCE)
set(SVN_ENABLE_PROGRAMS OFF CACHE BOOL "Disable programs" FORCE)
set(SVN_ENABLE_TOOLS OFF CACHE BOOL "Disable tools" FORCE)
set(SVN_ENABLE_TESTS OFF CACHE BOOL "Disable tests" FORCE)
set(SVN_ENABLE_APACHE_MODULES OFF CACHE BOOL "Disable Apache modules" FORCE)
set(SVN_ENABLE_NLS OFF CACHE BOOL "Disable gettext functionality" FORCE)
set(SVN_ENABLE_TUI OFF CACHE BOOL "Disable TUI applications" FORCE)
set(SVN_ENABLE_AUTH_KWALLET OFF CACHE BOOL "Disable KWallet" FORCE)
set(SVN_ENABLE_AUTH_GNOME_KEYRING OFF CACHE BOOL "Disable GNOME Keyring" FORCE)
set(SVN_ENABLE_SWIG_PERL OFF CACHE BOOL "Disable Perl bindings" FORCE)
set(SVN_ENABLE_SWIG_PYTHON OFF CACHE BOOL "Disable Python bindings" FORCE)
set(SVN_ENABLE_SWIG_RUBY OFF CACHE BOOL "Disable Ruby bindings" FORCE)
set(SVN_ENABLE_JAVAHL OFF CACHE BOOL "Disable JavaHL" FORCE)
set(SVN_ENABLE_SASL ON CACHE BOOL "Enable SASL" FORCE)
set(SVN_ENABLE_LIBMAGIC OFF CACHE BOOL "Disable LIBMAGIC" FORCE)
set(SVN_ENABLE_RA_SERF ON CACHE BOOL "Enable serf RA" FORCE)
set(SVN_ENABLE_RA_SVN ON CACHE BOOL "Enable svn RA" FORCE)
set(SVN_ENABLE_RA_LOCAL ON CACHE BOOL "Enable local RA" FORCE)
set(SVN_ENABLE_FS_FS ON CACHE BOOL "Enable FSFS" FORCE)
set(SVN_ENABLE_FS_X ON CACHE BOOL "Enable FSX" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Disable shared libraries" FORCE)
add_subdirectory(../subversion "subversion")

target_link_libraries(SvnRepo PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    qca-qt6
    qca-ossl
    Threads::Threads
    cyrus-sasl
    serf
    OpenSSL::SSL
    OpenSSL::Crypto
    apr-1
    aprutil-1
    sqlite3
    expat::expat
    utf8proc
    libsvnxx
    libsvn_client
    libsvn_wc
    libsvn_ra
    libsvn_delta
    libsvn_diff
    libsvn_fs
    libsvn_subr
)

target_include_directories(SvnRepo PRIVATE
   ${CMAKE_CURRENT_SOURCE_DIR}/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/include
   ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/bindings/cxx/include
   ../../lib/qca/include/QtCrypto
   ${CMAKE_CURRENT_BINARY_DIR}/QCA
   ${CMAKE_CURRENT_BINARY_DIR}/..
)
