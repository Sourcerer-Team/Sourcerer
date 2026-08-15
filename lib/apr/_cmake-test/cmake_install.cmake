# Install script for directory: C:/Work/Sourcerer/lib/apr

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files (x86)/APR")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

# Is this installation the result of a crosscompile?
if(NOT DEFINED CMAKE_CROSSCOMPILING)
  set(CMAKE_CROSSCOMPILING "FALSE")
endif()

# Set path to fallback-tool for dependency-resolution.
if(NOT DEFINED CMAKE_OBJDUMP)
  set(CMAKE_OBJDUMP "C:/Qt/Tools/mingw1310_64/bin/objdump.exe")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY OPTIONAL FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/liblibapr-1.dll.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE SHARED_LIBRARY FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/liblibapr-1.dll")
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/liblibapr-1.dll" AND
     NOT IS_SYMLINK "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/liblibapr-1.dll")
    if(CMAKE_INSTALL_DO_STRIP)
      execute_process(COMMAND "C:/Qt/Tools/mingw1310_64/bin/strip.exe" "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/bin/liblibapr-1.dll")
    endif()
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/libapr-1.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/liblibaprapp-1.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/libaprapp-1.a")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(EXISTS "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/apr/apr-config.cmake")
    file(DIFFERENT _cmake_export_file_changed FILES
         "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/apr/apr-config.cmake"
         "C:/Work/Sourcerer/lib/apr/_cmake-test/CMakeFiles/Export/4057a8c4c38b0940fe40238b60644dc9/apr-config.cmake")
    if(_cmake_export_file_changed)
      file(GLOB _cmake_old_config_files "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/apr/apr-config-*.cmake")
      if(_cmake_old_config_files)
        string(REPLACE ";" ", " _cmake_old_config_files_text "${_cmake_old_config_files}")
        message(STATUS "Old export file \"$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/lib/cmake/apr/apr-config.cmake\" will be replaced.  Removing files [${_cmake_old_config_files_text}].")
        unset(_cmake_old_config_files_text)
        file(REMOVE ${_cmake_old_config_files})
      endif()
      unset(_cmake_old_config_files)
    endif()
    unset(_cmake_export_file_changed)
  endif()
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/apr" TYPE FILE FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/CMakeFiles/Export/4057a8c4c38b0940fe40238b60644dc9/apr-config.cmake")
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^()$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/apr" TYPE FILE FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/CMakeFiles/Export/4057a8c4c38b0940fe40238b60644dc9/apr-config-noconfig.cmake")
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib/cmake/apr" TYPE FILE FILES "C:/Work/Sourcerer/lib/apr/_cmake-test/apr-config-version.cmake")
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_CONFIG_NAME MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo]|[Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/bin" TYPE FILE FILES
      )
  endif()
endif()

if(CMAKE_INSTALL_COMPONENT STREQUAL "Unspecified" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include" TYPE FILE FILES
    "C:/Work/Sourcerer/lib/apr/include/apr_allocator.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_atomic.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_cstr.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_dso.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_encode.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_env.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_errno.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_escape.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_file_info.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_file_io.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_fnmatch.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_general.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_getopt.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_global_mutex.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_hash.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_inherit.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_lib.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_mmap.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_network_io.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_perms_set.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_poll.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_pools.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_portable.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_proc_mutex.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_random.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_ring.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_shm.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_signal.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_skiplist.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_strings.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_support.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_tables.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_thread_cond.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_thread_mutex.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_thread_proc.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_thread_rwlock.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_time.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_user.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_version.h"
    "C:/Work/Sourcerer/lib/apr/include/apr_want.h"
    "C:/Work/Sourcerer/lib/apr/_cmake-test/apr.h"
    )
endif()

string(REPLACE ";" "\n" CMAKE_INSTALL_MANIFEST_CONTENT
       "${CMAKE_INSTALL_MANIFEST_FILES}")
if(CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Work/Sourcerer/lib/apr/_cmake-test/install_local_manifest.txt"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
if(CMAKE_INSTALL_COMPONENT)
  if(CMAKE_INSTALL_COMPONENT MATCHES "^[a-zA-Z0-9_.+-]+$")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INSTALL_COMPONENT}.txt")
  else()
    string(MD5 CMAKE_INST_COMP_HASH "${CMAKE_INSTALL_COMPONENT}")
    set(CMAKE_INSTALL_MANIFEST "install_manifest_${CMAKE_INST_COMP_HASH}.txt")
    unset(CMAKE_INST_COMP_HASH)
  endif()
else()
  set(CMAKE_INSTALL_MANIFEST "install_manifest.txt")
endif()

if(NOT CMAKE_INSTALL_LOCAL_ONLY)
  file(WRITE "C:/Work/Sourcerer/lib/apr/_cmake-test/${CMAKE_INSTALL_MANIFEST}"
     "${CMAKE_INSTALL_MANIFEST_CONTENT}")
endif()
