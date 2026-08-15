if(TARGET expat)
    # Provide standard variables for compatibility
    set(EXPAT_FOUND TRUE)
    set(EXPAT_INCLUDE_DIRS ${CMAKE_CURRENT_LIST_DIR}/../lib/libexpat/expat/lib)
    add_library(EXPAT::EXPAT ALIAS expat)
    set(EXPAT_LIBRARIES EXPAT::EXPAT)
else()
    set(EXPAT_FOUND FALSE)
endif()
