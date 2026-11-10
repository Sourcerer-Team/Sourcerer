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
    src/SvnRepoPrivate.h
    src/SvnRepo.cpp
    src/SvnRepoBranches.cpp
    src/SvnRepoDiff.cpp
    src/SvnRepoHistory.cpp
    src/SvnRepoRemotes.cpp
    src/SvnRepoShelves.cpp
    src/SvnRepoUnsupported.cpp
    src/SvnRepoWorkingCopy.cpp
)

include(../../cmake/helpers.cmake)

includeComponent(SvnRepo ConfigManager)

includeLib(SvnRepo GitRepo)
includeLib(SvnRepo Log)
includeLib(SvnRepo Util)

useSharedThirdPartyLibs()
file(MAKE_DIRECTORY ${SOURCERER_SHARED_LIB_DIR})

set(CYRUS_SASL_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../cyrus-sasl)
set(CYRUS_SASL_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyrus-sasl-install)

if(WIN32)
set(CYRUS_SASL_IMPLIB ${CYRUS_SASL_INSTALL_DIR}/lib/libsasl.lib)
set(CYRUS_SASL_LIBRARY ${SOURCERER_SHARED_LIB_DIR}/libsasl.dll)
file(MAKE_DIRECTORY
    ${CYRUS_SASL_INSTALL_DIR}/include
    ${CYRUS_SASL_INSTALL_DIR}/lib
    ${CYRUS_SASL_INSTALL_DIR}/bin
)

add_custom_command(
    OUTPUT ${CYRUS_SASL_LIBRARY} ${CYRUS_SASL_IMPLIB}
    COMMAND cmd /c "cd /D ${CYRUS_SASL_SOURCE_DIR}/include && nmake /f NTMakefile install prefix=${CYRUS_SASL_INSTALL_DIR} OPENSSL_INCLUDE=${OPENSSL_INCLUDE_DIR} OPENSSL_LIBPATH=$<TARGET_FILE_DIR:OpenSSL::SSL> DISABLE_RES=1 && cd /D ${CYRUS_SASL_SOURCE_DIR}/lib && nmake /f NTMakefile install prefix=${CYRUS_SASL_INSTALL_DIR} OPENSSL_INCLUDE=${OPENSSL_INCLUDE_DIR} OPENSSL_LIBPATH=$<TARGET_FILE_DIR:OpenSSL::SSL> DISABLE_RES=1"
    COMMAND ${CMAKE_COMMAND} -E copy ${CYRUS_SASL_INSTALL_DIR}/bin/libsasl.dll ${SOURCERER_SHARED_LIB_DIR}
    WORKING_DIRECTORY ${CYRUS_SASL_SOURCE_DIR}
    DEPENDS ${CYRUS_SASL_SOURCE_DIR}/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/win32/common.mak ${CYRUS_SASL_SOURCE_DIR}/lib/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/plugins/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/utils/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/sample/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/sasldb/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/include/NTMakefile ${CYRUS_SASL_SOURCE_DIR}/win32/include/NTMakefile
    VERBATIM
)

else()
# The autotools-based dependencies (APR, apr-util, Cyrus SASL) are built by
# shell scripts. On macOS build-universal.sh runs them once per architecture
# in CMAKE_OSX_ARCHITECTURES and merges the library with lipo; elsewhere they
# run directly. Sets <out_var> to the command prefix for building <library>.
function(svnrepo_autotools_runner out_var library)
  if(APPLE)
    set(archs "${CMAKE_OSX_ARCHITECTURES}")
    if(NOT archs)
      set(archs native)
    endif()
    string(REPLACE ";" "," archs "${archs}")
    set(deployment_target "${CMAKE_OSX_DEPLOYMENT_TARGET}")
    if(NOT deployment_target)
      set(deployment_target default)
    endif()
    set(${out_var} sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-universal.sh
        ${archs} ${deployment_target} ${library} PARENT_SCOPE)
  else()
    set(${out_var} sh PARENT_SCOPE)
  endif()
endfunction()

set(CYRUS_SASL_BUILD_DIR ${CMAKE_CURRENT_BINARY_DIR}/cyrus-sasl-build)
if(APPLE)
  set(CYRUS_SASL_SONAME libsasl2.3.dylib)
else()
  set(CYRUS_SASL_SONAME libsasl2.so.3)
endif()
set(CYRUS_SASL_LIBRARY ${SOURCERER_SHARED_LIB_DIR}/${CYRUS_SASL_SONAME})
file(MAKE_DIRECTORY
    ${CYRUS_SASL_INSTALL_DIR}/include
    ${CYRUS_SASL_INSTALL_DIR}/lib
    ${CYRUS_SASL_INSTALL_DIR}/bin
    ${CYRUS_SASL_BUILD_DIR}
)

# Point configure at the OpenSSL CMake found; on macOS it is keg-only
# in Homebrew and not on the default include path.
get_filename_component(CYRUS_SASL_OPENSSL_ROOT ${OPENSSL_INCLUDE_DIR} DIRECTORY)

if(APPLE)
  set(CYRUS_SASL_SET_ID COMMAND install_name_tool -id @rpath/${CYRUS_SASL_SONAME} ${CYRUS_SASL_LIBRARY})
endif()

svnrepo_autotools_runner(CYRUS_SASL_RUN ${CYRUS_SASL_SONAME})
add_custom_command(
    OUTPUT ${CYRUS_SASL_LIBRARY}
    COMMAND ${CYRUS_SASL_RUN} ${CMAKE_CURRENT_LIST_DIR}/cmake/build-cyrus-sasl.sh ${CYRUS_SASL_SOURCE_DIR} ${CYRUS_SASL_BUILD_DIR} ${CYRUS_SASL_INSTALL_DIR} ${CYRUS_SASL_OPENSSL_ROOT}
    COMMAND ${CMAKE_COMMAND} -E copy ${CYRUS_SASL_INSTALL_DIR}/lib/${CYRUS_SASL_SONAME} ${SOURCERER_SHARED_LIB_DIR}
    ${CYRUS_SASL_SET_ID}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-cyrus-sasl.sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-universal.sh ${CYRUS_SASL_SOURCE_DIR}/autogen.sh ${CYRUS_SASL_SOURCE_DIR}/configure.ac ${CYRUS_SASL_SOURCE_DIR}/lib/Makefile.am
    VERBATIM
)

endif()

add_custom_target(cyrus-sasl-build ALL DEPENDS ${CYRUS_SASL_LIBRARY})
add_library(cyrus-sasl SHARED IMPORTED GLOBAL)
set_target_properties(cyrus-sasl PROPERTIES
    IMPORTED_LOCATION ${CYRUS_SASL_LIBRARY}
    INTERFACE_INCLUDE_DIRECTORIES "${CYRUS_SASL_INSTALL_DIR}/include;${CYRUS_SASL_SOURCE_DIR}/include"
)
if(WIN32)
  set_target_properties(cyrus-sasl PROPERTIES IMPORTED_IMPLIB ${CYRUS_SASL_IMPLIB})
elseif(APPLE)
  set_target_properties(cyrus-sasl PROPERTIES IMPORTED_SONAME @rpath/${CYRUS_SASL_SONAME})
else()
  set_target_properties(cyrus-sasl PROPERTIES IMPORTED_SONAME ${CYRUS_SASL_SONAME})
endif()
add_dependencies(cyrus-sasl cyrus-sasl-build)

# Host-specific toolchain paths; only needed when not running inside a VS developer
# environment (VsDevCmd/vcvars set INCLUDE/LIB for the active toolset).
if(MSVC AND NOT DEFINED ENV{VCToolsInstallDir})
  include_directories(SYSTEM
      C:/PROGRA~1/MIB055~1/18/COMMUN~1/VC/Tools/MSVC/1451~1.362/include
      "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/ucrt"
      "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/shared"
      "C:/Program Files (x86)/Windows Kits/10/Include/10.0.26100.0/um")
  link_directories(
      "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/um/x64"
      "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.26100.0/ucrt/x64"
      C:/PROGRA~1/MIB055~1/18/COMMUN~1/VC/Tools/MSVC/1451~1.362/lib/x64)
endif()

add_subdirectory(../sqlite "sqlite")

set(EXPAT_BUILD_TOOLS OFF CACHE BOOL "Disable tools")
set(EXPAT_BUILD_EXAMPLES OFF CACHE BOOL "Disable examples")
set(EXPAT_BUILD_TESTS OFF CACHE BOOL "Disable tests")
set(EXPAT_SHARED_LIBS ON CACHE BOOL "Enable shared libraries" FORCE)
set(EXPAT_BUILD_DOCS  OFF CACHE BOOL "Disable docs")
set(EXPAT_BUILD_FUZZERS  OFF CACHE BOOL "Disable fuzzers")
set(EXPAT_BUILD_PKGCONFIG  OFF CACHE BOOL "Disable pkgconfig")
add_subdirectory(../libexpat/expat "expat")

if(WIN32)

set(APR_BUILD_SHARED ON CACHE BOOL "Enable shared libraries" FORCE)
set(APR_BUILD_STATIC OFF CACHE BOOL "Disable static libraries" FORCE)
add_subdirectory(../apr "apr")

file(MAKE_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/apr)
file(COPY
    ${CMAKE_CURRENT_SOURCE_DIR}/../apr/include/apr_version.h
    DESTINATION ${CMAKE_CURRENT_BINARY_DIR}/apr
)

set(APU_BUILD_SHARED ON CACHE BOOL "Build a shared aprutil-1 library" FORCE)
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

# build-apr.sh and build-apr-util.sh name the shared libraries like libtool.
if(APPLE)
  set(APR_SONAME libapr-1.0.dylib)
  set(APR_UTIL_SONAME libaprutil-1.0.dylib)
  set(APR_SONAME_PREFIX @rpath/)
else()
  set(APR_SONAME libapr-1.so.0)
  set(APR_UTIL_SONAME libaprutil-1.so.0)
  set(APR_SONAME_PREFIX)
endif()
set(APR_LIBRARY ${SOURCERER_SHARED_LIB_DIR}/${APR_SONAME})
set(APR_UTIL_LIBRARY ${SOURCERER_SHARED_LIB_DIR}/${APR_UTIL_SONAME})

svnrepo_autotools_runner(APR_RUN ${APR_SONAME})
add_custom_command(
    OUTPUT ${APR_LIBRARY}
    COMMAND ${APR_RUN} ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr.sh ${CMAKE_CURRENT_LIST_DIR}/../apr ${APR_BUILD_DIR} ${APR_INSTALL_DIR}
    COMMAND ${CMAKE_COMMAND} -E copy ${APR_INSTALL_DIR}/lib/${APR_SONAME} ${SOURCERER_SHARED_LIB_DIR}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr.sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-universal.sh ${CMAKE_CURRENT_LIST_DIR}/../apr/buildconf ${CMAKE_CURRENT_LIST_DIR}/../apr/configure.in
    VERBATIM
)

add_custom_target(apr-build ALL DEPENDS ${APR_LIBRARY})

add_library(apr-1 SHARED IMPORTED GLOBAL)
set_target_properties(apr-1 PROPERTIES
    IMPORTED_LOCATION ${APR_LIBRARY}
    IMPORTED_SONAME ${APR_SONAME_PREFIX}${APR_SONAME}
    INTERFACE_INCLUDE_DIRECTORIES ${APR_INSTALL_DIR}/include/apr-1
)
# Matches EXTRA_LIBS detected by APR's configure (see apr-build/build/apr_rules.mk).
# On macOS uuid and crypt live in libSystem, so configure only adds pthread.
if(APPLE)
  set_target_properties(apr-1 PROPERTIES INTERFACE_LINK_LIBRARIES "pthread;${CMAKE_DL_LIBS}")
else()
  set_target_properties(apr-1 PROPERTIES INTERFACE_LINK_LIBRARIES "uuid;crypt;pthread;${CMAKE_DL_LIBS}")
endif()
if(UNIX AND NOT WIN32)
  set_target_properties(apr-1 PROPERTIES
      INTERFACE_COMPILE_DEFINITIONS "PATH_MAX=4096"
  )
endif()
add_dependencies(apr-1 apr-build)

svnrepo_autotools_runner(APR_UTIL_RUN ${APR_UTIL_SONAME})
add_custom_command(
    OUTPUT ${APR_UTIL_LIBRARY}
    COMMAND ${APR_UTIL_RUN} ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr-util.sh ${CMAKE_CURRENT_LIST_DIR}/../apr-util ${APR_UTIL_BUILD_DIR} ${APR_UTIL_INSTALL_DIR} ${APR_INSTALL_DIR} ${CMAKE_CURRENT_LIST_DIR}/../apr ${CMAKE_CURRENT_LIST_DIR}/../libexpat/expat $<TARGET_FILE_DIR:expat>
    COMMAND ${CMAKE_COMMAND} -E copy ${APR_UTIL_INSTALL_DIR}/lib/${APR_UTIL_SONAME} ${SOURCERER_SHARED_LIB_DIR}
    DEPENDS ${CMAKE_CURRENT_LIST_DIR}/cmake/build-apr-util.sh ${CMAKE_CURRENT_LIST_DIR}/cmake/build-universal.sh ${CMAKE_CURRENT_LIST_DIR}/../apr-util/buildconf ${CMAKE_CURRENT_LIST_DIR}/../apr-util/configure.in ${APR_LIBRARY} expat
    VERBATIM
)

add_custom_target(apr-util-build ALL DEPENDS ${APR_UTIL_LIBRARY})

add_library(aprutil-1 SHARED IMPORTED GLOBAL)
set_target_properties(aprutil-1 PROPERTIES
    IMPORTED_LOCATION ${APR_UTIL_LIBRARY}
    IMPORTED_SONAME ${APR_SONAME_PREFIX}${APR_UTIL_SONAME}
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

add_library(serf SHARED ${SERF_SOURCES})
# serf has no export macros. Subversion uses only its functions, not its data.
set_target_properties(serf PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)

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

set(UTF8PROC_ENABLE_TESTING OFF CACHE BOOL "Disable testing")
set(LIB_FUZZING_ENGINE OFF CACHE BOOL "Disable fuzzing")
add_subdirectory(../utf8proc "utf8proc")
# utf8proc is plain C, but the global CMAKE_AUTOMOC still adds a generated
# mocs_compilation.cpp, and utf8proc's target-wide COMPILE_FLAGS (-std=c99)
# then reach the C++ compiler, which clang rejects.
set_target_properties(utf8proc PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)

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
set(SVN_ENABLE_SASL OFF CACHE BOOL "Disable SASL (libsasl.dll crashes at exit on Windows)" FORCE)
set(SVN_ENABLE_LIBMAGIC OFF CACHE BOOL "Disable LIBMAGIC" FORCE)
set(SVN_ENABLE_RA_SERF ON CACHE BOOL "Enable serf RA" FORCE)
set(SVN_ENABLE_RA_SVN ON CACHE BOOL "Enable svn RA" FORCE)
set(SVN_ENABLE_RA_LOCAL ON CACHE BOOL "Enable local RA" FORCE)
set(SVN_ENABLE_FS_FS ON CACHE BOOL "Enable FSFS" FORCE)
set(SVN_ENABLE_FS_X ON CACHE BOOL "Enable FSX" FORCE)
set(SVN_BUILD_SHARED_FS ON CACHE BOOL "Build shared FS modules" FORCE)
set(SVN_BUILD_SHARED_RA ON CACHE BOOL "Build shared RA modules" FORCE)

add_subdirectory(../subversion "subversion")

target_link_libraries(SvnRepo PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Network
    Qt6::Widgets
    qca-qt6
    qca-ossl
    Threads::Threads
    serf
    OpenSSL::SSL
    OpenSSL::Crypto
    apr-1
    aprutil-1
    sqlite3
    expat::expat
    utf8proc
    $<$<PLATFORM_ID:Windows>:Version>
    libsvnxx
    libsvn_client
    libsvn_wc
    libsvn_repos
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
   ${CMAKE_CURRENT_SOURCE_DIR}/../libgit2/include
)

# Standalone Subversion command-line client bundled with installers.
# Subversion's CMake only stubs out programs, so the svn target is defined here.
option(SOURCERER_BUILD_SVN_CLI "Build the standalone svn command-line client" OFF)
if(SOURCERER_BUILD_SVN_CLI)
    file(GLOB SVN_CLI_SOURCES CONFIGURE_DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/svn/*.c
    )
    add_executable(svn ${SVN_CLI_SOURCES})
    # The packaging scripts take it from here, not from the shared library folder.
    set_target_properties(svn PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    target_include_directories(svn PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/include/private
        ${CMAKE_CURRENT_SOURCE_DIR}/../subversion/subversion/svn
        ${CMAKE_CURRENT_BINARY_DIR}/subversion
        ${CMAKE_CURRENT_BINARY_DIR}/apr
        ${CMAKE_CURRENT_BINARY_DIR}/apr-util
        ${CMAKE_CURRENT_SOURCE_DIR}/../apr-util/include
    )
    target_link_libraries(svn PRIVATE
        libsvn_client
        libsvn_ra
        libsvn_ra_local
        libsvn_ra_svn
        libsvn_ra_serf
        libsvn_repos
        libsvn_fs
        libsvn_fs_util
        libsvn_fs_fs
        libsvn_fs_x
        libsvn_wc
        libsvn_delta
        libsvn_diff
        libsvn_subr
        serf
        apr-1
        aprutil-1
        sqlite3
        expat::expat
        utf8proc
        ZLIB::ZLIB
        OpenSSL::SSL
        OpenSSL::Crypto
        Threads::Threads
        $<$<PLATFORM_ID:Windows>:Version>
    )
endif()
