# FindWinPTY.cmake
# Finds the WinPTY library (MSYS2/MinGW version)
#
# This will define:
#  WINPTY_FOUND - System has WinPTY
#  WINPTY_INCLUDE_DIRS - The WinPTY include directories
#  WINPTY_LIBRARIES - The libraries needed to use WinPTY

message("dezelin: " ${CMAKE_SOURCE_DIR})

find_path(WINPTY_INCLUDE_DIR
    NAMES winpty.h
    PATHS
        ${CMAKE_SOURCE_DIR}/../lib/winpty/include/winpty
    PATH_SUFFIXES include
)

find_library(WINPTY_LIBRARY
    NAMES winpty winpty
    PATHS
        ${CMAKE_SOURCE_DIR}/../lib/winpty/lib
    PATH_SUFFIXES lib bin
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(winpty
    REQUIRED_VARS WINPTY_LIBRARY WINPTY_INCLUDE_DIR
)

if(WINPTY_FOUND)
    set(WINPTY_LIBRARIES ${WINPTY_LIBRARY})
    set(WINPTY_INCLUDE_DIRS ${WINPTY_INCLUDE_DIR})
    
    if(NOT TARGET winpty::winpty)
        add_library(winpty::winpty UNKNOWN IMPORTED)
        set_target_properties(winpty::winpty PROPERTIES
            IMPORTED_LOCATION "${WINPTY_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${WINPTY_INCLUDE_DIR}"
        )
    endif()
    
    message(STATUS "Found WinPTY: ${WINPTY_LIBRARY}")
    message(STATUS "WinPTY include dir: ${WINPTY_INCLUDE_DIR}")
endif()

mark_as_advanced(WINPTY_INCLUDE_DIR WINPTY_LIBRARY)
