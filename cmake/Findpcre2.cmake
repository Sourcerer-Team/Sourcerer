# Findpcre2.cmake
# Find the PCRE2 library
#
# This module defines:
#  PCRE2_FOUND - system has PCRE2
#  PCRE2_INCLUDE_DIRS - the PCRE2 include directories
#  PCRE2_LIBRARIES - link these to use PCRE2
#  PCRE2_DEFINITIONS - compile definitions needed by PCRE2
#  PCRE2::PCRE2 - imported target

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
  pkg_check_modules(PC_PCRE2 QUIET libpcre2-8)
endif()

set(PCRE2_DEFINITIONS ${PC_PCRE2_CFLAGS_OTHER})

find_path(PCRE2_INCLUDE_DIR
    NAMES pcre2.h
    HINTS
        ${PC_PCRE2_INCLUDEDIR}
        ${PC_PCRE2_INCLUDE_DIRS}
        ${PCRE2_ROOT}
        $ENV{PCRE2_ROOT}
        $ENV{PCRE2_INCLUDEDIR}
    PATH_SUFFIXES include
)

find_library(PCRE2_LIBRARY
    NAMES pcre2-8 pcre2-8d libpcre2-8 libpcre2-8d
    HINTS
        ${PC_PCRE2_LIBDIR}
        ${PC_PCRE2_LIBRARY_DIRS}
        ${PCRE2_ROOT}
        $ENV{PCRE2_ROOT}
        $ENV{PCRE2_LIBDIR}
    PATH_SUFFIXES lib lib64 bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(pcre2
    REQUIRED_VARS PCRE2_LIBRARY PCRE2_INCLUDE_DIR
    VERSION_VAR PC_PCRE2_VERSION
)

if(PCRE2_FOUND)
  set(PCRE2_LIBRARIES ${PCRE2_LIBRARY})
  set(PCRE2_INCLUDE_DIRS ${PCRE2_INCLUDE_DIR})

  if(NOT TARGET PCRE2::PCRE2)
    add_library(PCRE2::PCRE2 UNKNOWN IMPORTED)
    set_target_properties(PCRE2::PCRE2 PROPERTIES
            IMPORTED_LOCATION "${PCRE2_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${PCRE2_INCLUDE_DIR}"
            INTERFACE_COMPILE_DEFINITIONS "PCRE2_CODE_UNIT_WIDTH=8"
        )
  endif()
endif()

mark_as_advanced(PCRE2_INCLUDE_DIR PCRE2_LIBRARY)
