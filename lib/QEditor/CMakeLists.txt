cmake_minimum_required(VERSION 3.24)

if(TARGET QEditor)
  return()
endif()

project(QEditor LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

if(NOT TARGET tree-sitter)
  add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter ${CMAKE_BINARY_DIR}/tree-sitter)
endif()

qt_add_library(QEditor STATIC
  include/Qsci/qsciglobal.h
  include/Qsci/qscilexer.h
  include/Qsci/qsciscintilla.h
  src/QEditor.cpp
)

target_include_directories(QEditor PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${CMAKE_CURRENT_SOURCE_DIR}/../tree-sitter/lib/include
)

target_link_libraries(QEditor PUBLIC
  Qt6::Core
  Qt6::Gui
  Qt6::Widgets
  tree-sitter
)
