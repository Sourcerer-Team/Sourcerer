if(TARGET libzip)
    # Provide standard variables for compatibility
    set(LIBZIP_FOUND TRUE)
    set(LIBZIP_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../lib/libzip/include)
    set(LIBZIP_LIBRARIES libzip::zip)
else()
    set(LIBZIP_FOUND FALSE)
endif()
