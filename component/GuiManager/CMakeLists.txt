cmake_minimum_required(VERSION 3.24)

if(TARGET GuiManager)
    return()
endif()

project(GuiManager)

set(CMAKE_CXX_STANDARD 17)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets LinguistTools)

set(TS_FILES translations/Sourcerer_en_US.ts)

set(GUIMANAGER_SOURCE_FILES
   include/ContainerWidget.h
   include/GuiManager.h
   include/HomeContainer.h
   include/MainWindow.h
   include/PleaseWaitContainer.h
   include/TitlebarWidget.h
   src/ContainerWidget.cpp
   src/GuiManager.cpp
   src/HomeContainer.cpp
   src/MainWindow.cpp
   src/PleaseWaitContainer.cpp
   src/TitlebarWidget.cpp
   src/HomeContainer.ui
   src/PleaseWaitContainer.ui
   src/TitlebarWidget.ui
   ${TS_FILES}
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

add_library(GuiManager STATIC ${GUIMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(GuiManager ConfigManager)

includeLib(GuiManager Log)
includeLib(GuiManager Util)
includeLib(GuiManager QtWaitingSpinner)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodWindow/QGoodWindow.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodWindow/src)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodCentralWidget/QGoodCentralWidget.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodCentralWidget/src)

target_link_libraries(GuiManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Widgets
)

target_include_directories(GuiManager PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
