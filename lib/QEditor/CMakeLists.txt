cmake_minimum_required(VERSION 3.24)

if(TARGET QEditor)
  return()
endif()

project(QEditor LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

include(../../cmake/helpers.cmake)

if(NOT TARGET tree-sitter)
  # tree-sitter adds /W4 itself, drop the default /W3 to avoid MSVC D9025
  # warnings.
  set(_SAVED_C_FLAGS "${CMAKE_C_FLAGS}")
  set(_SAVED_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  string(REGEX REPLACE "/W[0-4]" "" CMAKE_C_FLAGS "${CMAKE_C_FLAGS}")
  string(REGEX REPLACE "/W[0-4]" "" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  useSharedThirdPartyLibs()
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter
                   ${CMAKE_BINARY_DIR}/tree-sitter)
  # The API has no export macros on Windows; it has only functions.
  set_target_properties(tree-sitter PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
  set(CMAKE_C_FLAGS "${_SAVED_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${_SAVED_CXX_FLAGS}")
endif()

file(GLOB TREE_SITTER_LANGUAGE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/*/src/*.c
)

file(GLOB TREE_SITTER_LANGUAGE_INCLUDE_DIRS CONFIGURE_DEPENDS
     LIST_DIRECTORIES true
     ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/src
     ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/*/src)

# The generated tree-sitter parser.c files are huge (hundreds of thousands of
# lines) and each one can need several GB of compiler memory. Compile them in
# their own object library with a dedicated Ninja job pool so only a few run at
# once, regardless of the overall build parallelism.
set(SOURCERER_GRAMMAR_JOBS 2 CACHE STRING
    "Max number of tree-sitter grammars compiled in parallel (Ninja only)")
set_property(GLOBAL APPEND PROPERTY JOB_POOLS
             tree_sitter_grammars=${SOURCERER_GRAMMAR_JOBS})

add_library(QEditorGrammars OBJECT ${TREE_SITTER_LANGUAGE_SOURCES})
set_target_properties(QEditorGrammars PROPERTIES
  JOB_POOL_COMPILE tree_sitter_grammars
  POSITION_INDEPENDENT_CODE ON)
target_include_directories(QEditorGrammars PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter/lib/include
  ${TREE_SITTER_LANGUAGE_INCLUDE_DIRS}
)

qt_add_library(QEditor STATIC
  include/Qsci/qsciglobal.h
  include/Qsci/qscilexer.h
  include/Qsci/qscilexerbash.h
  include/Qsci/qscilexercsharp.h
  include/Qsci/qscilexerdart.h
  include/Qsci/qscilexerhaskell.h
  include/Qsci/qscilexerjava.h
  include/Qsci/qscilexerocaml.h
  include/Qsci/qscilexerocamltype.h
  include/Qsci/qscilexerpowershell.h
  include/Qsci/qscilexerrust.h
  include/Qsci/qscilexertsx.h
  include/Qsci/qscilexertypescript.h
  include/Qsci/qsciscintilla.h
  src/QEditor.cpp
  $<TARGET_OBJECTS:QEditorGrammars>
)

target_include_directories(QEditor PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter/lib/include
  ${TREE_SITTER_LANGUAGE_INCLUDE_DIRS}
)

target_link_libraries(QEditor PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  tree-sitter
)
