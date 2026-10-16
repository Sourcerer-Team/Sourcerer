cmake_minimum_required(VERSION 3.24)

if(TARGET ConfigManager)
    return()
endif()

project(ConfigManager)

set(CMAKE_CXX_STANDARD 17)

find_package(Qt6 REQUIRED COMPONENTS Core Gui LinguistTools)

set(CONFIGMANAGER_SOURCE_FILES
   include/ConfigKey.h
   include/ConfigKeyType.h
   include/ConfigGroup.h
   include/ConfigManager.h
   src/ConfigGroup.cpp
   src/ConfigManager.cpp
   GitVersion.h

)

add_library(ConfigManager STATIC ${CONFIGMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeLib(ConfigManager Log)
includeLib(ConfigManager Util)

target_link_libraries(ConfigManager PRIVATE
    Qt6::Core
    Qt6::Gui
)

target_include_directories(ConfigManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Get git version (commit hash) - use repository root working directory
set(GIT_REPO_ROOT "${PROJECT_SOURCE_DIR}/../..")

# Add repository to git safe directories (required for Docker/isolated environments)
execute_process(
    COMMAND git config --global --add safe.directory ${GIT_REPO_ROOT}
    WORKING_DIRECTORY "${GIT_REPO_ROOT}"
    ERROR_QUIET)

execute_process(
    COMMAND git describe --tags --always
    WORKING_DIRECTORY "${GIT_REPO_ROOT}"
    OUTPUT_VARIABLE GIT_REPO_VERSION
    ERROR_QUIET)

string(STRIP "${GIT_REPO_VERSION}" GIT_REPO_VERSION)

# If git command failed or we're not in a git repository, use a default version
if(NOT GIT_REPO_VERSION)
    set(GIT_REPO_VERSION "dev")
endif()

configure_file("GitVersion.h.in" "GitVersion.h")
