cmake_minimum_required(VERSION 3.5)

project(Sourcerer VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets LinguistTools)

set(TS_FILES Sourcerer_en_US.ts)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        ${TS_FILES}
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(Sourcerer
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET Sourcerer APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation

    qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
else()
    if(ANDROID)
        add_library(Sourcerer SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(Sourcerer
            ${PROJECT_SOURCES}
        )
    endif()

    qt5_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})
endif()

include("../cmake/helpers.cmake")

includeComponent(Sourcerer ComponentManager)
includeComponent(Sourcerer EventManager)

includeLib(Sourcerer Log)

include(${CMAKE_CURRENT_SOURCE_DIR}/../lib/QGoodWindow/QGoodWindow.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/QGoodWindow/src)

include(${CMAKE_CURRENT_SOURCE_DIR}/../lib/QGoodCentralWidget/QGoodCentralWidget.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../lib/QGoodCentralWidget/src)

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

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(Sourcerer)
endif()
