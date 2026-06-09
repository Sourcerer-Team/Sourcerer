# FindLibZip.cmake
# Find the libzip library
#
# This module defines:
#  LIBZIP_FOUND - system has libzip
#  LIBZIP_INCLUDE_DIRS - the libzip include directories
#  LIBZIP_LIBRARIES - link these to use libzip
#  libzip::zip - imported target

find_package(PkgConfig QUIET)
if(PKG_CONFIG_FOUND)
    pkg_check_modules(PC_LIBZIP QUIET libzip)
endif()

set(LIBZIP_DEFINITIONS ${PC_LIBZIP_CFLAGS_OTHER})

find_path(LIBZIP_INCLUDE_DIR
    NAMES zip.h
    HINTS
        ${PC_LIBZIP_INCLUDEDIR}
        ${PC_LIBZIP_INCLUDE_DIRS}
        $ENV{LIBZIP_INCLUDEDIR}
    PATH_SUFFIXES include
)

find_library(LIBZIP_LIBRARY
    NAMES zip libzip
    HINTS
        ${PC_LIBZIP_LIBDIR}
        ${PC_LIBZIP_LIBRARY_DIRS}
        $ENV{LIBZIP_LIBDIR}
    PATH_SUFFIXES lib lib64
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(libzip
    REQUIRED_VARS LIBZIP_LIBRARY LIBZIP_INCLUDE_DIR
    VERSION_VAR PC_LIBZIP_VERSION
)

if(LIBZIP_FOUND)
    set(LIBZIP_LIBRARIES ${LIBZIP_LIBRARY})
    set(LIBZIP_INCLUDE_DIRS ${LIBZIP_INCLUDE_DIR})
    
    if(NOT TARGET libzip::zip)
        add_library(libzip::zip UNKNOWN IMPORTED)
        set_target_properties(libzip::zip PROPERTIES
            IMPORTED_LOCATION "${LIBZIP_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LIBZIP_INCLUDE_DIR}"
        )
    endif()
endif()

mark_as_advanced(LIBZIP_INCLUDE_DIR LIBZIP_LIBRARY)
