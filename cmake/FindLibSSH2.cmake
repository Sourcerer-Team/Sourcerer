# FindLibSSH2.cmake
# This module assumes libssh2 is added via add_subdirectory()
# and defines the imported target libssh2::libssh2

if(TARGET libssh2)
    # Provide standard variables for compatibility
    set(LIBSSH2_FOUND TRUE)
    set(LIBSSH2_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../lib/libssh2/include)
    set(LIBSSH2_INCLUDE_DIR ${CMAKE_CURRENT_LIST_DIR}/../lib/libssh2/include)
    set(LIBSSH2_LIBRARIES libssh2_static)
    set(LIBSSH2_LIBRARY libssh2_static)
else()
    set(LIBSSH2_FOUND FALSE)
endif()
