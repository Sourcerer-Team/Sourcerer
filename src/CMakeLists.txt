cmake_minimum_required(VERSION 3.5)

project(Sourcerer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(PROJECT_SOURCES
        main.cpp
)

qt_add_executable(Sourcerer
    MANUAL_FINALIZATION
    ${PROJECT_SOURCES}
)

include("../cmake/helpers.cmake")

includeComponent(Sourcerer ComponentManager)
includeComponent(Sourcerer ConfigManager)
includeComponent(Sourcerer EventManager)
includeComponent(Sourcerer GuiManager)
includeComponent(Sourcerer StylesheetManager)

includeLib(Sourcerer Util)

includeLib(Sourcerer Log)

target_link_libraries(Sourcerer PRIVATE Qt${QT_VERSION_MAJOR}::Widgets)

set_target_properties(Sourcerer PROPERTIES
    MACOSX_BUNDLE_GUI_IDENTIFIER www.sourcerer.com
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

install(TARGETS Sourcerer
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})


qt_finalize_executable(Sourcerer)
