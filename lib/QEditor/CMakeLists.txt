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

if(NOT TARGET tree-sitter)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter
                   ${CMAKE_BINARY_DIR}/tree-sitter)
endif()

file(GLOB TREE_SITTER_LANGUAGE_SOURCES CONFIGURE_DEPENDS
  ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/src/*.c
  ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/*/src/*.c
)

file(GLOB TREE_SITTER_LANGUAGE_INCLUDE_DIRS CONFIGURE_DEPENDS
     LIST_DIRECTORIES true
     ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/src
     ${CMAKE_CURRENT_SOURCE_DIR}/tree-sitter/*/*/src)

qt_add_library(QEditor STATIC
  include/Qsci/qsciglobal.h
  include/Qsci/qscilexer.h
  include/Qsci/qscilexerbash.h
  include/Qsci/qscilexercsharp.h
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
  ${TREE_SITTER_LANGUAGE_SOURCES}
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
