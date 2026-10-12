# Syntax highlighting test for CMake: projects, options, targets, variables,
# generator expressions, control flow, functions, macros and comments.
#[[ A bracket comment
    spanning several lines. ]]

cmake_minimum_required(VERSION 3.22)
project(Sourcerer VERSION 1.0.13 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(SOURCES src/main.cpp src/app.cpp "src/quoted name.cpp")
option(BUILD_TESTS "Build the unit tests" ON)
set(PREFIX "${CMAKE_INSTALL_PREFIX}/share" CACHE PATH "Install prefix")

if(WIN32 AND NOT MSVC)
  message(WARNING "Unsupported toolchain")
elseif(APPLE OR (UNIX AND NOT ANDROID))
  add_compile_definitions(PLATFORM_UNIX=1)
else()
  message(STATUS "Other platform: ${CMAKE_SYSTEM_NAME}")
endif()

foreach(source IN LISTS SOURCES)
  message(STATUS "Source: ${source}")
endforeach()

foreach(index RANGE 1 3)
  math(EXPR square "${index} * ${index}")
endforeach()

while(counter LESS 10)
  math(EXPR counter "${counter} + 1")
endwhile()

function(add_app name)
  cmake_parse_arguments(ARG "" "OUTPUT" "SOURCES" ${ARGN})
  add_executable(${name} ${ARG_SOURCES})
  target_include_directories(${name} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
endfunction()

macro(report_path var)
  message(STATUS "${var} = $ENV{PATH}")
endmacro()

add_library(core STATIC ${SOURCES})
target_compile_options(core PRIVATE $<$<CXX_COMPILER_ID:MSVC>:/W4> -Wall)
target_link_libraries(core PUBLIC Threads::Threads)
set_target_properties(core PROPERTIES OUTPUT_NAME "sourcerer-core" POSITION_INDEPENDENT_CODE ON)

add_app(sourcerer SOURCES src/main.cpp OUTPUT out)

if(BUILD_TESTS)
  enable_testing()
  add_test(NAME unit COMMAND sourcerer --run-tests)
endif()

install(TARGETS sourcerer DESTINATION bin)
