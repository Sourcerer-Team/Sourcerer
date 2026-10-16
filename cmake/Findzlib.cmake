if(TARGET zlib)
    # Provide standard variables for compatibility
    set(ZLIB_FOUND TRUE)
    set(ZLIB_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../lib/zlib)
    set(ZLIB_LIBRARIES ZLIB::ZLIB)
else()
    set(ZLIB_FOUND FALSE)
endif()
