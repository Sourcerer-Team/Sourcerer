set(DIR_OF_HELPER_CMAKE ${CMAKE_CURRENT_LIST_DIR})

if(NOT MSVC)
  add_compile_options(-Wno-deprecated-declarations)
endif()

function(includeComponent linkTo componentName)
      message(STATUS "Adding component ${componentName}")

      if (NOT TARGET ${componentName})
         add_subdirectory(${DIR_OF_HELPER_CMAKE}/../component/${componentName}/ ${CMAKE_BINARY_DIR}/${componentName}/)
      endif()

      include(${DIR_OF_HELPER_CMAKE}/../component/${componentName}/path.cmake)

      if (NOT DEFINED ${componentName}_INCLUDE_PATH)
        message(FATAL_ERROR "${componentName} don't have defined ${componentName}_INCLUDE_PATH" )
      endif()

      message(STATUS "Setting ${linkTo} include path ${${componentName}_INCLUDE_PATH}")

      target_include_directories(${linkTo} PUBLIC ${${componentName}_INCLUDE_PATH})
      target_link_libraries(${linkTo} PUBLIC ${componentName})
      include_directories(${${componentName}_INCLUDE_PATH})

      if (DEFINED ${componentName}_INCLUDE_LIB)
        target_link_libraries(${linkTo} PUBLIC ${${componentName}_INCLUDE_LIB})
      endif()

endfunction()

function(includeLib linkTo libraryName)
      message(STATUS "Adding library ${libraryName}")

      if (NOT TARGET ${libraryName})
         add_subdirectory(${DIR_OF_HELPER_CMAKE}/../lib/${libraryName}/ ${CMAKE_BINARY_DIR}/${libraryName}/)
      endif()

      include(${DIR_OF_HELPER_CMAKE}/../lib/${libraryName}/path.cmake)

      if (NOT DEFINED ${libraryName}_INCLUDE_PATH)
        message(FATAL_ERROR "${libraryName} don't have defined ${libraryName}_INCLUDE_PATH")
      endif()

      message(STATUS "Setting ${linkTo} include path ${${libraryName}_INCLUDE_PATH}")

      target_include_directories(${linkTo} PUBLIC ${${libraryName}_INCLUDE_PATH})
      target_link_libraries(${linkTo} PUBLIC ${libraryName})
      include_directories(${${libraryName}_INCLUDE_PATH})

      if (DEFINED ${libraryName}_INCLUDE_LIB)
        target_link_libraries(${linkTo} PUBLIC ${${libraryName}_INCLUDE_LIB})
      endif()

endfunction()

function(includeModule linkTo moduleName)
      message(STATUS "Adding module ${moduleName}")

      if (NOT TARGET ${moduleName})
         add_subdirectory(${DIR_OF_HELPER_CMAKE}/../module/${moduleName}/ ${CMAKE_BINARY_DIR}/${moduleName}/)
      endif()

      include(${DIR_OF_HELPER_CMAKE}/../module/${moduleName}/path.cmake)

      if (NOT DEFINED ${moduleName}_INCLUDE_PATH)
        message(FATAL_ERROR "${moduleName} don't have defined ${moduleName}_INCLUDE_PATH")
      endif()

      message(STATUS "Setting ${linkTo} include path ${${moduleName}_INCLUDE_PATH}")

      target_include_directories(${linkTo} PUBLIC ${${moduleName}_INCLUDE_PATH})
      target_link_libraries(${linkTo} PUBLIC ${moduleName})
      include_directories(${${moduleName}_INCLUDE_PATH})

      if (DEFINED ${moduleName}_INCLUDE_LIB)
        target_link_libraries(${linkTo} PUBLIC ${${moduleName}_INCLUDE_LIB})
      endif()

endfunction()

function(includeLibFiles linkTo libraryName)
      message(STATUS "Adding library ${libraryName}")

      include(${DIR_OF_HELPER_CMAKE}/../lib/${libraryName}/path.cmake)

      if (NOT DEFINED ${libraryName}_INCLUDE_PATH)
        message(FATAL_ERROR "${libraryName} don't have defined ${libraryName}_INCLUDE_PATH")
      endif()

      message(STATUS "Setting ${linkTo} include path ${${libraryName}_INCLUDE_PATH}")

      target_include_directories(${linkTo} PUBLIC ${${libraryName}_INCLUDE_PATH})

      if (DEFINED ${libraryName}_INCLUDE_LIB)
        target_link_libraries(${linkTo} PUBLIC ${${libraryName}_INCLUDE_LIB})
      endif()

endfunction()
