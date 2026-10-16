# Prefer the OpenSSL installation distributed with Qt when available.
# This module provides the standard variables and imported targets expected by
# `find_package(OpenSSL REQUIRED)`.

if(TARGET OpenSSL::SSL AND TARGET OpenSSL::Crypto)
    set(OPENSSL_FOUND TRUE)
    set(OpenSSL_FOUND TRUE)
    return()
endif()

include(FindPackageHandleStandardArgs)

set(_openssl_candidate_roots)

# An explicitly requested OpenSSL installation always wins on every platform.
# This is how CI points the build at a custom (e.g. universal macOS) build.
foreach(_openssl_root_hint IN ITEMS
        "${OPENSSL_ROOT_DIR}" "$ENV{OPENSSL_ROOT_DIR}")
    if(NOT "${_openssl_root_hint}" STREQUAL "")
        list(APPEND _openssl_candidate_roots "${_openssl_root_hint}")
    endif()
endforeach()

# Qt on Windows typically installs OpenSSL under C:\Qt\Tools\OpenSSLv3\Win_x64
# when the OpenSSL tools package is added. Prefer that installation first.
foreach(_qt_root_var IN ITEMS QT_DIR Qt6_DIR)
    if(DEFINED ${_qt_root_var} AND NOT "${${_qt_root_var}}" STREQUAL "")
        if(_qt_root_var STREQUAL "Qt6_DIR")
            # Qt6_DIR usually points at .../lib/cmake/Qt6, so walk back to the
            # Qt installation root before appending Tools/OpenSSLv3.
            set(_qt6_dir_value "${${_qt_root_var}}")
            get_filename_component(_qt_root
                "${_qt6_dir_value}/../../../../.."
                ABSOLUTE
            )
        else()
            set(_qt_root "${${_qt_root_var}}")
        endif()

        list(APPEND _openssl_candidate_roots
            "${_qt_root}/Tools/OpenSSLv3/Win_x64"
            "${_qt_root}/Tools/OpenSSLv3/x64"
            "${_qt_root}/Tools/OpenSSLv3/x86"
        )
    endif()
endforeach()

if(DEFINED ENV{QT_DIR} AND NOT "$ENV{QT_DIR}" STREQUAL "")
    list(APPEND _openssl_candidate_roots
        "$ENV{QT_DIR}/Tools/OpenSSLv3/Win_x64"
        "$ENV{QT_DIR}/Tools/OpenSSLv3/x64"
        "$ENV{QT_DIR}/Tools/OpenSSLv3/x86"
    )
endif()

# Common installation locations used by package managers and installers.
if(APPLE)
    # Homebrew installs OpenSSL keg-only, so it is never in a default prefix.
    # Note: Homebrew libraries are single-architecture; universal builds must
    # pass a universal OPENSSL_ROOT_DIR (see installer/macos/build-openssl-universal.sh).
    list(APPEND _openssl_candidate_roots
        "/opt/homebrew/opt/openssl@3"
        "/usr/local/opt/openssl@3"
    )
else()
    list(APPEND _openssl_candidate_roots "/usr")
endif()

list(APPEND _openssl_candidate_roots
    "C:/Program Files/OpenSSL"
    "C:/Program Files/OpenSSL-Win64"
    "C:/Program Files (x86)/OpenSSL"
    "C:/Program Files (x86)/OpenSSL-Win64"
)

list(REMOVE_DUPLICATES _openssl_candidate_roots)

set(OPENSSL_FOUND FALSE)
set(OpenSSL_FOUND FALSE)
set(OPENSSL_ROOT_DIR "")
set(OpenSSL_ROOT_DIR "")
set(OPENSSL_INCLUDE_DIR "")
set(OpenSSL_INCLUDE_DIR "")
set(OPENSSL_SSL_LIBRARY "")
set(OPENSSL_CRYPTO_LIBRARY "")
set(OPENSSL_LIBRARIES "")

foreach(_candidate_root IN LISTS _openssl_candidate_roots)
    if(NOT EXISTS "${_candidate_root}")
        continue()
    endif()

    find_path(_openssl_include_dir
        NAMES openssl/ssl.h
        HINTS
            "${_candidate_root}/include"
        NO_DEFAULT_PATH
    )

    find_library(_openssl_ssl_library
        NAMES ssl libssl libssl-3-x64
        HINTS
            "${_candidate_root}/lib"
            "${_candidate_root}/lib/x86_64-linux-gnu"
            "${_candidate_root}/bin"
        NO_DEFAULT_PATH
    )

    find_library(_openssl_crypto_library
        NAMES crypto libcrypto libcrypto-3-x64
        HINTS
            "${_candidate_root}/lib"
            "${_candidate_root}/lib/x86_64-linux-gnu"
            "${_candidate_root}/bin"
        NO_DEFAULT_PATH
    )

    if(_openssl_include_dir AND _openssl_ssl_library AND
       _openssl_crypto_library)
        set(OPENSSL_FOUND TRUE)
        set(OpenSSL_FOUND TRUE)
        set(OPENSSL_ROOT_DIR "${_candidate_root}")
        set(OpenSSL_ROOT_DIR "${_candidate_root}")
        set(OPENSSL_INCLUDE_DIR "${_openssl_include_dir}")
        set(OpenSSL_INCLUDE_DIR "${_openssl_include_dir}")
        set(OPENSSL_SSL_LIBRARY "${_openssl_ssl_library}")
        set(OPENSSL_CRYPTO_LIBRARY "${_openssl_crypto_library}")
        set(OPENSSL_LIBRARIES
            "${_openssl_ssl_library};${_openssl_crypto_library}")
        break()
    endif()
endforeach()

# Fall back to a normal system search if Qt's bundled OpenSSL is not present.
if(NOT OPENSSL_FOUND)
    find_path(OPENSSL_INCLUDE_DIR
        NAMES openssl/ssl.h
    )
    find_library(OPENSSL_SSL_LIBRARY
        NAMES ssl libssl
    )
    find_library(OPENSSL_CRYPTO_LIBRARY
        NAMES crypto libcrypto
    )

    if(OPENSSL_INCLUDE_DIR AND OPENSSL_SSL_LIBRARY AND OPENSSL_CRYPTO_LIBRARY)
        set(OPENSSL_FOUND TRUE)
        set(OpenSSL_FOUND TRUE)
        set(OPENSSL_LIBRARIES
            "${OPENSSL_SSL_LIBRARY};${OPENSSL_CRYPTO_LIBRARY}")
        if(NOT OPENSSL_ROOT_DIR)
            get_filename_component(
                OPENSSL_ROOT_DIR
                "${OPENSSL_INCLUDE_DIR}/.."
                ABSOLUTE
            )
            set(OpenSSL_ROOT_DIR "${OPENSSL_ROOT_DIR}")
        endif()
        set(OpenSSL_INCLUDE_DIR "${OPENSSL_INCLUDE_DIR}")
    endif()
endif()

if(OPENSSL_FOUND)
    # A universal build can only link against OpenSSL libraries that contain
    # every requested architecture. Fail early with a clear message instead of
    # a cryptic linker error.
    list(LENGTH CMAKE_OSX_ARCHITECTURES _openssl_arch_count)
    if(APPLE AND _openssl_arch_count GREATER 1)
        find_program(_openssl_lipo lipo)
        if(_openssl_lipo)
            foreach(_openssl_lib IN ITEMS
                    "${OPENSSL_SSL_LIBRARY}" "${OPENSSL_CRYPTO_LIBRARY}")
                execute_process(
                    COMMAND "${_openssl_lipo}" -archs "${_openssl_lib}"
                    OUTPUT_VARIABLE _openssl_lib_archs
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
                string(REPLACE " " ";" _openssl_lib_archs
                    "${_openssl_lib_archs}")
                foreach(_openssl_arch IN LISTS CMAKE_OSX_ARCHITECTURES)
                    if(NOT _openssl_arch IN_LIST _openssl_lib_archs)
                        message(FATAL_ERROR
                            "OpenSSL library '${_openssl_lib}' does not contain "
                            "architecture '${_openssl_arch}' (found: "
                            "${_openssl_lib_archs}). Build a universal OpenSSL "
                            "with installer/macos/build-openssl-universal.sh and "
                            "pass -DOPENSSL_ROOT_DIR=<prefix>.")
                    endif()
                endforeach()
            endforeach()
        endif()
    endif()

    if(NOT TARGET OpenSSL::Crypto)
        add_library(OpenSSL::Crypto UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::Crypto PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_CRYPTO_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
        )
    endif()

    if(NOT TARGET OpenSSL::SSL)
        add_library(OpenSSL::SSL UNKNOWN IMPORTED)
        set_target_properties(OpenSSL::SSL PROPERTIES
            IMPORTED_LOCATION "${OPENSSL_SSL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${OPENSSL_INCLUDE_DIR}"
            INTERFACE_LINK_LIBRARIES OpenSSL::Crypto
        )
    endif()
endif()

find_package_handle_standard_args(OpenSSL
    REQUIRED_VARS
        OPENSSL_INCLUDE_DIR
        OPENSSL_SSL_LIBRARY
        OPENSSL_CRYPTO_LIBRARY
)
