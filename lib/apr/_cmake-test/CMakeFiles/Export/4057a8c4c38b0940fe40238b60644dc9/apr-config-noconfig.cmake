#----------------------------------------------------------------
# Generated CMake target import file.
#----------------------------------------------------------------

# Commands may need to know the format version.
set(CMAKE_IMPORT_FILE_VERSION 1)

# Import target "apr::libapr-1" for configuration ""
set_property(TARGET apr::libapr-1 APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(apr::libapr-1 PROPERTIES
  IMPORTED_IMPLIB_NOCONFIG "${_IMPORT_PREFIX}/lib/liblibapr-1.dll.a"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/bin/liblibapr-1.dll"
  )

list(APPEND _cmake_import_check_targets apr::libapr-1 )
list(APPEND _cmake_import_check_files_for_apr::libapr-1 "${_IMPORT_PREFIX}/lib/liblibapr-1.dll.a" "${_IMPORT_PREFIX}/bin/liblibapr-1.dll" )

# Import target "apr::apr-1" for configuration ""
set_property(TARGET apr::apr-1 APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(apr::apr-1 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libapr-1.a"
  )

list(APPEND _cmake_import_check_targets apr::apr-1 )
list(APPEND _cmake_import_check_files_for_apr::apr-1 "${_IMPORT_PREFIX}/lib/libapr-1.a" )

# Import target "apr::libaprapp-1" for configuration ""
set_property(TARGET apr::libaprapp-1 APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(apr::libaprapp-1 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/liblibaprapp-1.a"
  )

list(APPEND _cmake_import_check_targets apr::libaprapp-1 )
list(APPEND _cmake_import_check_files_for_apr::libaprapp-1 "${_IMPORT_PREFIX}/lib/liblibaprapp-1.a" )

# Import target "apr::aprapp-1" for configuration ""
set_property(TARGET apr::aprapp-1 APPEND PROPERTY IMPORTED_CONFIGURATIONS NOCONFIG)
set_target_properties(apr::aprapp-1 PROPERTIES
  IMPORTED_LINK_INTERFACE_LANGUAGES_NOCONFIG "C"
  IMPORTED_LOCATION_NOCONFIG "${_IMPORT_PREFIX}/lib/libaprapp-1.a"
  )

list(APPEND _cmake_import_check_targets apr::aprapp-1 )
list(APPEND _cmake_import_check_files_for_apr::aprapp-1 "${_IMPORT_PREFIX}/lib/libaprapp-1.a" )

# Commands beyond this point should not need to know the version.
set(CMAKE_IMPORT_FILE_VERSION)
