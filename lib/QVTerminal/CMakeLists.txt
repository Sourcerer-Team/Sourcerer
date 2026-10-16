cmake_minimum_required(VERSION 3.24)

if(TARGET QVTerminal)
    return()
endif()

project(QVTerminal)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Widgets Gui)

set(QVTerminal_SOURCE_FILES
    src/qvtchar.h
    src/qvtchar.cpp
    src/qvtcharformat.h
    src/qvtcharformat.cpp
    src/qvterminal.h
    src/qvterminal.cpp
    src/qvtlayout.h
    src/qvtlayout.cpp
    src/qvtline.h
    src/qvtline.cpp
    src/vt/vt.h
    src/vt/vt.cpp
    src/vt/vt100.h
    src/vt/vt100.cpp
)

add_library(QVTerminal STATIC ${QVTerminal_SOURCE_FILES})

target_link_libraries(QVTerminal 
    PUBLIC 
        Qt6::Core
        Qt6::Widgets
        Qt6::Gui
)

target_include_directories(QVTerminal 
    PUBLIC 
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src/vt
)

# Export the library
set_target_properties(QVTerminal PROPERTIES
    POSITION_INDEPENDENT_CODE ON
)
