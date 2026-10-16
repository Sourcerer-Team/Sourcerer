cmake_minimum_required(VERSION 3.24)

if(TARGET sqlite3)
  return()
endif()

project(sqlite3 C)

set(CMAKE_C_STANDARD 99)

find_package(Threads REQUIRED)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/VERSION" SQLITE3_VERSION_TEXT)
string(STRIP "${SQLITE3_VERSION_TEXT}" SQLITE3_VERSION_TEXT)

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/manifest" SQLITE3_MANIFEST_TEXT)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/manifest.uuid" SQLITE3_SOURCE_UUID)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/manifest.tags" SQLITE3_MANIFEST_TAGS_TEXT)

string(STRIP "${SQLITE3_SOURCE_UUID}" SQLITE3_SOURCE_UUID)
string(REGEX MATCH "^([0-9]+)\.([0-9]+)\.([0-9]+)" _ "${SQLITE3_VERSION_TEXT}")
math(EXPR SQLITE3_VERSION_NUMBER "${CMAKE_MATCH_1} * 1000000 + ${CMAKE_MATCH_2} * 1000 + ${CMAKE_MATCH_3}")

string(REGEX MATCH "D ([0-9-]+)T([0-9:.]+)" _ "${SQLITE3_MANIFEST_TEXT}")
set(SQLITE3_SOURCE_ID "${CMAKE_MATCH_1} ${CMAKE_MATCH_2} ${SQLITE3_SOURCE_UUID}")

string(REPLACE "\r" "" SQLITE3_MANIFEST_TAGS_TEXT "${SQLITE3_MANIFEST_TAGS_TEXT}")
string(REPLACE "\n" ";" SQLITE3_MANIFEST_TAGS_LINES "${SQLITE3_MANIFEST_TAGS_TEXT}")
set(SQLITE3_SCM_BRANCH "unknown")
set(SQLITE3_SCM_TAGS "unknown")
set(SQLITE3_TAGS_LIST)
foreach(_line IN LISTS SQLITE3_MANIFEST_TAGS_LINES)
  if(_line MATCHES "^branch[ \t]+(.+)$")
    set(SQLITE3_SCM_BRANCH "${CMAKE_MATCH_1}")
  elseif(_line MATCHES "^tag[ \t]+(.+)$")
    list(APPEND SQLITE3_TAGS_LIST "${CMAKE_MATCH_1}")
  endif()
endforeach()
if(SQLITE3_TAGS_LIST)
  string(JOIN ", " SQLITE3_SCM_TAGS ${SQLITE3_TAGS_LIST})
endif()

file(READ "${CMAKE_CURRENT_SOURCE_DIR}/src/sqlite.h.in" SQLITE3_HEADER_TEMPLATE)
string(REPLACE "--VERS--" "${SQLITE3_VERSION_TEXT}" SQLITE3_HEADER_CONTENT "${SQLITE3_HEADER_TEMPLATE}")
string(REPLACE "--VERSION-NUMBER--" "${SQLITE3_VERSION_NUMBER}" SQLITE3_HEADER_CONTENT "${SQLITE3_HEADER_CONTENT}")
string(REPLACE "--SOURCE-ID--" "${SQLITE3_SOURCE_ID}" SQLITE3_HEADER_CONTENT "${SQLITE3_HEADER_CONTENT}")
string(REPLACE "--SCM-BRANCH--" "${SQLITE3_SCM_BRANCH}" SQLITE3_HEADER_CONTENT "${SQLITE3_HEADER_CONTENT}")
string(REPLACE "--SCM-TAGS--" "${SQLITE3_SCM_TAGS}" SQLITE3_HEADER_CONTENT "${SQLITE3_HEADER_CONTENT}")
string(APPEND SQLITE3_HEADER_CONTENT "\n#endif /* SQLITE3_H */\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/sqlite3.h" "${SQLITE3_HEADER_CONTENT}")

add_executable(sqlite_lemon EXCLUDE_FROM_ALL tool/lemon.c)
add_executable(sqlite_mkkeywordhash EXCLUDE_FROM_ALL tool/mkkeywordhash.c)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/tool/lempar.c
  ${CMAKE_CURRENT_BINARY_DIR}/lempar.c COPYONLY)

set(SQLITE3_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}")
set(SQLITE3_PARSE_C "${SQLITE3_GENERATED_DIR}/parse.c")
set(SQLITE3_PARSE_H "${SQLITE3_GENERATED_DIR}/parse.h")
set(SQLITE3_KEYWORDHASH_H "${SQLITE3_GENERATED_DIR}/keywordhash.h")
set(SQLITE3_PRAGMA_H "${CMAKE_CURRENT_SOURCE_DIR}/src/pragma.h")
set(SQLITE3_OPCODES_H "${SQLITE3_GENERATED_DIR}/opcodes.h")
set(SQLITE3_OPCODES_C "${SQLITE3_GENERATED_DIR}/opcodes.c")


add_custom_command(
  OUTPUT ${SQLITE3_PARSE_C} ${SQLITE3_PARSE_H}
  COMMAND $<TARGET_FILE:sqlite_lemon> -q ${CMAKE_CURRENT_SOURCE_DIR}/src/parse.y
  DEPENDS sqlite_lemon ${CMAKE_CURRENT_SOURCE_DIR}/src/parse.y ${CMAKE_CURRENT_SOURCE_DIR}/tool/lempar.c
  COMMENT "Generating SQLite parser"
  VERBATIM
)

add_custom_command(
  OUTPUT ${SQLITE3_KEYWORDHASH_H}
  COMMAND ${CMAKE_COMMAND}
          -D SQLITE3_MKKEYWORDHASH=$<TARGET_FILE:sqlite_mkkeywordhash>
          -D SQLITE3_OUTPUT_FILE=${SQLITE3_KEYWORDHASH_H}
          -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_keywordhash.cmake
  DEPENDS sqlite_mkkeywordhash ${CMAKE_CURRENT_SOURCE_DIR}/tool/mkkeywordhash.c
  COMMENT "Generating SQLite keyword hash"
  VERBATIM
)



execute_process(
  COMMAND ${CMAKE_COMMAND}
          -D SQLITE3_VDBE_C=${CMAKE_CURRENT_SOURCE_DIR}/src/vdbe.c
          -D SQLITE3_OUTPUT_FILE=${SQLITE3_OPCODES_H}
          -D SQLITE3_OUTPUT_SOURCE_FILE=${SQLITE3_OPCODES_C}
          -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_opcodes.cmake
  WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
  RESULT_VARIABLE SQLITE3_GENERATE_OPCODES_RESULT
)
if(NOT SQLITE3_GENERATE_OPCODES_RESULT EQUAL 0)
  message(FATAL_ERROR "Failed to generate SQLite opcodes header")
endif()

add_custom_command(
  OUTPUT ${SQLITE3_OPCODES_H}
  COMMAND ${CMAKE_COMMAND}
          -D SQLITE3_VDBE_C=${CMAKE_CURRENT_SOURCE_DIR}/src/vdbe.c
          -D SQLITE3_OUTPUT_FILE=${SQLITE3_OPCODES_H}
          -D SQLITE3_OUTPUT_SOURCE_FILE=${SQLITE3_OPCODES_C}
          -P ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_opcodes.cmake
  DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/vdbe.c ${CMAKE_CURRENT_LIST_DIR}/cmake/generate_opcodes.cmake
  COMMENT "Generating SQLite opcodes table"
  VERBATIM
)

file(GLOB SQLITE3_SOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/test[^/]*\\.c$")
list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/tclsqlite\\.c$")

if(WIN32)
  list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/os_unix\\.c$")
  list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/mutex_unix\\.c$")
else()
  list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/os_win\\.c$")
  list(FILTER SQLITE3_SOURCE_FILES EXCLUDE REGEX ".*/mutex_w32\\.c$")
endif()

set_source_files_properties(
  ${SQLITE3_KEYWORDHASH_H}
  ${SQLITE3_OPCODES_H}
  PROPERTIES GENERATED TRUE
)

add_library(sqlite3 STATIC
  ${SQLITE3_SOURCE_FILES}
  ${CMAKE_CURRENT_SOURCE_DIR}/ctime.c
  ${SQLITE3_KEYWORDHASH_H}
  ${SQLITE3_OPCODES_H}
  ${SQLITE3_OPCODES_C}
)
add_library(sqlite::sqlite3 ALIAS sqlite3)

add_dependencies(sqlite3 sqlite_lemon sqlite_mkkeywordhash)

set_target_properties(sqlite3 PROPERTIES
  POSITION_INDEPENDENT_CODE ON
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
  SKIP_AUTOGEN ON
)



target_include_directories(sqlite3 PUBLIC
  ${CMAKE_CURRENT_BINARY_DIR}
  ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(sqlite3 PUBLIC Threads::Threads)

if(NOT WIN32 AND CMAKE_DL_LIBS)
  target_link_libraries(sqlite3 PUBLIC ${CMAKE_DL_LIBS})
endif()
