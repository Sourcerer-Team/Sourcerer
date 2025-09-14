cmake_minimum_required(VERSION 3.24)

if (TARGET GuiManager)
    return()
endif ()

project(GuiManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(libgit2 REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui GuiPrivate Svg Widgets LinguistTools)

set(TS_FILES translations/Sourcerer_en_US.ts)
set(QRC_FILES resources/resources.qrc)

set(GUIMANAGER_SOURCE_FILES
        include/DarkStyle.h
        include/GuiManager.h
        include/HitDetector.h
        include/InitResources.h
        include/MainWindow.h
        include/containers/CloneGitRepoContainer.h
        include/containers/GitRepoContainer.h
        include/containers/HomeContainer.h
        include/containers/InitGitRepoContainer.h
        include/containers/PleaseWaitContainer.h
        include/containers/SettingsContainer.h
        include/delegates/ElideDelegate.h
        include/delegates/ElidePartialColorDelegate.h
        include/delegates/HistorySliceTableColumnDelegate.h
        include/delegates/HistorySummaryTableColumnDelegate.h
        include/delegates/HomeRecentItemDelegate.h
        include/dialogs/AboutDialog.h
        include/models/BranchesModel.h
        include/models/GitCommitListModel.h
        include/models/GitHistoryModel.h
        include/models/RecentlyOpenedReposModel.h
        include/models/SubmodulesModel.h
        include/models/TagsModel.h
        include/widgets/CentralWidget.h
        include/widgets/ClickableHeaderView.h
        include/widgets/ColorButton.h
        include/widgets/ContainerWidget.h
        include/widgets/ElidedLabel.h
        include/widgets/ErrorToast.h
        include/widgets/ExtendedTabBar.h
        include/widgets/ExtendedTabWidget.h
        include/widgets/GitCredentialsOverlayWidget.h
        include/widgets/GitSubmodulesPage.h
        include/widgets/GitProgressToast.h
        include/widgets/GitTagsPage.h
        include/widgets/InfoToast.h
        include/widgets/LocalGitBranchesPage.h
        include/widgets/MouseTrackingSplitter.h
        include/widgets/OverlayWidget.h
        include/widgets/RecentTableView.h
        include/widgets/RemoteGitBranchesPage.h
        include/widgets/SettingsPaneWidget.h
        include/widgets/SettingsPaneItemWidget.h
        include/widgets/SettingsPaneBooleanWidget.h
        include/widgets/SettingsPaneColorWidget.h
        include/widgets/SettingsPaneDateTimeWidget.h
        include/widgets/SettingsPaneLongWidget.h
        include/widgets/SettingsPaneStringWidget.h
        include/widgets/ShadeIconToolbox.h
        include/widgets/SpinnerDialog.h
        include/widgets/SpinnerOverlayWidget.h
        include/widgets/TitleBarButton.h
        include/widgets/TitleBarHomeButton.h
        include/widgets/TitleBarMenuButton.h
        include/widgets/TitleBarTabBar.h
        include/widgets/TitlebarWidget.h
        include/widgets/ToastCloseButton.h
        include/widgets/ToastHostWidget.h
        include/widgets/ToastWidget.h
        include/widgets/TopBarWidget.h
        include/widgets/factories/SettingsPaneItemWidgetFactory.h
        src/DarkStyle.cpp
        src/GuiManager.cpp
        src/HitDetector.cpp
        src/InitResources.cpp
        src/MainWindow.cpp
        src/containers/CloneGitRepoContainer.cpp
        src/containers/GitRepoContainer.cpp
        src/containers/HomeContainer.cpp
        src/containers/InitGitRepoContainer.cpp
        src/containers/PleaseWaitContainer.cpp
        src/containers/SettingsContainer.cpp
        src/delegates/HistorySliceTableColumnDelegate.cpp
        src/delegates/HistorySummaryTableColumnDelegate.cpp
        src/delegates/HomeRecentItemDelegate.cpp
        src/dialogs/AboutDialog.cpp
        src/dialogs/AboutDialog.ui
        src/models/BranchesModel.cpp
        src/models/GitCommitListModel.cpp
        src/models/GitHistoryModel.cpp
        src/models/RecentlyOpenedReposModel.cpp
        src/models/SubmodulesModel.cpp
        src/models/TagsModel.cpp
        src/widgets/CentralWidget.cpp
        src/widgets/ColorButton.cpp
        src/widgets/ContainerWidget.cpp
        src/widgets/ElidedLabel.cpp
        src/widgets/ErrorToast.cpp
        src/widgets/GitCredentialsOverlayWidget.cpp
        src/widgets/GitProgressToast.cpp
        src/widgets/GitSubmodulesPage.cpp
        src/widgets/GitTagsPage.cpp
        src/widgets/InfoToast.cpp
        src/widgets/LocalGitBranchesPage.cpp
        src/widgets/MouseTrackingSplitter.cpp
        src/widgets/RecentTableView.cpp
        src/widgets/RemoteGitBranchesPage.cpp
        src/widgets/SettingsPaneWidget.cpp
        src/widgets/SettingsPaneItemWidget.cpp
        src/widgets/SettingsPaneBooleanWidget.cpp
        src/widgets/SettingsPaneColorWidget.cpp
        src/widgets/SettingsPaneDateTimeWidget.cpp
        src/widgets/SettingsPaneLongWidget.cpp
        src/widgets/SettingsPaneStringWidget.cpp
        src/widgets/ShadeIconToolbox.cpp
        src/widgets/SpinnerOverlayWidget.cpp
        src/widgets/TitleBarButton.cpp
        src/widgets/TitleBarHomeButton.cpp
        src/widgets/TitleBarMenuButton.cpp
        src/widgets/TitleBarTabBar.cpp
        src/widgets/TitlebarWidget.cpp
        src/widgets/ToastCloseButton.cpp
        src/widgets/ToastHostWidget.cpp
        src/widgets/ToastWidget.cpp
        src/widgets/TopBarWidget.cpp
        src/widgets/factories/SettingsPaneItemWidgetFactory.cpp
        src/containers/CloneGitRepoContainer.ui
        src/containers/GitRepoContainer.ui
        src/containers/HomeContainer.ui
        src/containers/InitGitRepoContainer.ui
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

add_library(GuiManager STATIC ${GUIMANAGER_SOURCE_FILES}
        include/models/GitCommitTreeModel.h
        src/models/GitCommitTreeModel.cpp)

include(../../cmake/helpers.cmake)

includeComponent(GuiManager ComponentManager)
includeComponent(GuiManager ConfigManager)
includeComponent(GuiManager StylesheetManager)

includeLib(GuiManager Common)
includeLib(GuiManager GitRepo)
includeLib(GuiManager Log)
includeLib(GuiManager Util)
includeLib(GuiManager QtWaitingSpinner)

target_link_libraries(GuiManager PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::GuiPrivate
        Qt6::Svg
        Qt6::Widgets
        ${LIBGIT2_LIBRARIES}
)

target_include_directories(GuiManager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBGIT2_INCLUDE_DIR}
)
