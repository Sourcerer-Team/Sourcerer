cmake_minimum_required(VERSION 3.24)

if(TARGET GuiManager)
    return()
endif()

project(GuiManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(libgit2 REQUIRED)
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Gui Widgets LinguistTools)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Gui Widgets LinguistTools)

set(TS_FILES translations/Sourcerer_en_US.ts)
set(QRC_FILES resources/resources.qrc)

set(GUIMANAGER_SOURCE_FILES
   include/GuiManager.h
   include/InitResources.h
   include/MainWindow.h
   include/containers/GitRepoContainer.h
   include/containers/HomeContainer.h
   include/containers/PleaseWaitContainer.h
   include/containers/SettingsContainer.h
   include/delegates/HomeRecentItemDelegate.h
   include/dialogs/AboutDialog.h
   include/models/RecentlyOpenedReposModel.h
   include/widgets/ColorButton.h
   include/widgets/ContainerWidget.h
   include/widgets/GitSubmodulesPage.h
   include/widgets/GitTagsPage.h
   include/widgets/LocalGitBranchesPage.h
   include/widgets/RecentTableView.h
   include/widgets/RemoteGitBranchesPage.h
   include/widgets/SettingsPaneWidget.h
   include/widgets/SettingsPaneItemWidget.h
   include/widgets/SettingsPaneBooleanWidget.h
   include/widgets/SettingsPaneColorWidget.h
   include/widgets/SettingsPaneDateTimeWidget.h
   include/widgets/SettingsPaneLongWidget.h
   include/widgets/SettingsPaneStringWidget.h
   include/widgets/TitleBarHomeButton.h
   include/widgets/TitleBarMenuButton.h
   include/widgets/TitleBarTabBar.h
   include/widgets/TitlebarWidget.h
   include/widgets/factories/SettingsPaneItemWidgetFactory.h
   src/GuiManager.cpp
   src/InitResources.cpp
   src/MainWindow.cpp
   src/containers/GitRepoContainer.cpp
   src/containers/HomeContainer.cpp
   src/containers/PleaseWaitContainer.cpp
   src/containers/SettingsContainer.cpp
   src/delegates/HomeRecentItemDelegate.cpp
   src/dialogs/AboutDialog.cpp
   src/dialogs/AboutDialog.ui
   src/models/RecentlyOpenedReposModel.cpp
   src/widgets/ColorButton.cpp
   src/widgets/ContainerWidget.cpp
   src/widgets/GitSubmodulesPage.cpp
   src/widgets/GitTagsPage.cpp
   src/widgets/LocalGitBranchesPage.cpp
   src/widgets/RecentTableView.cpp
   src/widgets/RemoteGitBranchesPage.cpp
   src/widgets/SettingsPaneWidget.cpp
   src/widgets/SettingsPaneItemWidget.cpp
   src/widgets/SettingsPaneBooleanWidget.cpp
   src/widgets/SettingsPaneColorWidget.cpp
   src/widgets/SettingsPaneDateTimeWidget.cpp
   src/widgets/SettingsPaneLongWidget.cpp
   src/widgets/SettingsPaneStringWidget.cpp
   src/widgets/TitleBarHomeButton.cpp
   src/widgets/TitleBarMenuButton.cpp
   src/widgets/TitleBarTabBar.cpp
   src/widgets/TitlebarWidget.cpp
   src/widgets/factories/SettingsPaneItemWidgetFactory.cpp
   src/containers/GitRepoContainer.ui
   src/containers/HomeContainer.ui
   src/containers/PleaseWaitContainer.ui
   src/containers/SettingsContainer.ui
   src/widgets/GitSubmodulesPage.ui
   src/widgets/GitTagsPage.ui
   src/widgets/LocalGitBranchesPage.ui
   src/widgets/RemoteGitBranchesPage.ui
   src/widgets/TitlebarWidget.ui
   ${QRC_FILES}
   ${TS_FILES}
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

add_library(GuiManager STATIC ${GUIMANAGER_SOURCE_FILES})

include(../../cmake/helpers.cmake)

includeComponent(GuiManager ConfigManager)
includeComponent(GuiManager StylesheetManager)

includeLib(GuiManager Log)
includeLib(GuiManager Util)
includeLib(GuiManager QtWaitingSpinner)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodWindow/QGoodWindow.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodWindow/src)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodCentralWidget/QGoodCentralWidget.cmake)

target_include_directories(Sourcerer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/QGoodCentralWidget/src)

target_link_libraries(GuiManager PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Gui
    Qt${QT_VERSION_MAJOR}::Widgets
    ${LIBGIT2_LIBRARIES}
)

target_include_directories(GuiManager PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${LIBGIT2_INCLUDE_DIR}
)
