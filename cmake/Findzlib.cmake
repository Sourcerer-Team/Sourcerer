# Findzlib.cmake
# Find the zlib library
#
# This module defines:
#  ZLIB_FOUND - system has zlib
#  ZLIB_INCLUDE_DIRS - the zlib include directories
#  ZLIB_LIBRARIES - link these to use zlib
#  ZLIB_DEFINITIONS - compile definitions needed by zlib
#  zlib::zlib - imported target

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_ZLIB QUIET zlib)
endif()

set(ZLIB_DEFINITIONS ${PC_ZLIB_CFLAGS_OTHER})

find_path(ZLIB_INCLUDE_DIR
    NAMES zlib.h
    HINTS
        ${PC_ZLIB_INCLUDEDIR}
        ${PC_ZLIB_INCLUDE_DIRS}
        ${ZLIB_ROOT}
        $ENV{ZLIB_ROOT}
        $ENV{ZLIB_INCLUDEDIR}
    PATH_SUFFIXES include
)

find_library(ZLIB_LIBRARY
    NAMES z zlib zlib1 libz libzlib zlibstatic
    HINTS
        ${PC_ZLIB_LIBDIR}
        ${PC_ZLIB_LIBRARY_DIRS}
        ${ZLIB_ROOT}
        $ENV{ZLIB_ROOT}
        $ENV{ZLIB_LIBDIR}
    PATH_SUFFIXES lib lib64 bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(ZLIB
    REQUIRED_VARS ZLIB_LIBRARY ZLIB_INCLUDE_DIR
    VERSION_VAR PC_ZLIB_VERSION
)

if(ZLIB_FOUND)
  set(zlib_FOUND TRUE)
  set(ZLIB_LIBRARIES ${ZLIB_LIBRARY})
  set(ZLIB_INCLUDE_DIRS ${ZLIB_INCLUDE_DIR})

  if(NOT TARGET ZLIB::ZLIB)
    add_library(ZLIB::ZLIB UNKNOWN IMPORTED)
    set_target_properties(ZLIB::ZLIB PROPERTIES
        IMPORTED_LOCATION "${ZLIB_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${ZLIB_INCLUDE_DIR}"
    )
  endif()

  set(ZLIB_LIBRARY "${ZLIB_LIBRARY}" CACHE FILEPATH "zlib library" FORCE)
  set(ZLIB_INCLUDE_DIR "${ZLIB_INCLUDE_DIR}" CACHE PATH "zlib include directory" FORCE)
  set(ZLIB_LIBRARIES "${ZLIB_LIBRARIES}" CACHE STRING "zlib libraries" FORCE)
  set(ZLIB_INCLUDE_DIRS "${ZLIB_INCLUDE_DIRS}" CACHE STRING "zlib include directories" FORCE)
endif()

mark_as_advanced(ZLIB_INCLUDE_DIR ZLIB_LIBRARY)
