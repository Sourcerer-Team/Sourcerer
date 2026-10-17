cmake_minimum_required(VERSION 3.24)

if (TARGET GuiManager)
  return()
endif ()

project(GuiManager)

set(CMAKE_CXX_STANDARD 17)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/../cmake")

find_package(libzip REQUIRED)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Svg Widgets Network LinguistTools
             OPTIONAL_COMPONENTS WebEngineWidgets)

set(TS_FILES translations/Sourcerer_en_US.ts)
set(QRC_FILES resources/resources.qrc)

set(UI_FILES
        src/containers/BugReportContainer.ui
        src/containers/CloneRepoContainer.ui
        src/containers/GitCredentialsContainer.ui
        src/containers/GitKeysContainer.ui
        src/containers/RepoContainer.ui
        src/containers/HomeContainer.ui
        src/containers/InitRepoContainer.ui
        src/containers/IssueTrackerContainer.ui
        src/containers/PleaseWaitContainer.ui
        src/containers/ReleaseNotesContainer.ui
        src/containers/SettingsContainer.ui
        src/dialogs/AboutDialog.ui
        src/widgets/GitSubmodulesPage.ui
        src/widgets/GitTagsPage.ui
        src/widgets/GitWorktreesPage.ui
        src/widgets/LocalGitBranchesPage.ui
        src/widgets/ProfileBoardWidget.ui
        src/widgets/RemoteGitBranchesPage.ui
        src/widgets/RemotesPage.ui
        src/widgets/StashesPage.ui
        src/widgets/TitlebarWidget.ui
)

if(WIN32)
  set(PLATFORM_SOURCES
        include/widgets/win32/PowerShellWidget.h
        src/widgets/win32/PowerShellWidget.cpp
)
endif()

if(UNIX)
  set(PLATFORM_SOURCES
        include/widgets/linux/BashShellWidget.h
        src/widgets/linux/BashShellWidget.cpp
)
endif()


set(GUIMANAGER_SOURCE_FILES
        include/Constants.h
        include/DarkStyle.h
        include/DarkThemeLexersHighlighter.h
        include/GuiManager.h
        include/HitDetector.h
        include/WindowResize.h
        include/InitResources.h
        include/RepositoryType.h
        include/MainWindow.h
        include/MousePositionCorrection.h
        include/QScintillaLexerFactory.h
        include/ReleaseNotes.h
        include/containers/BugReportContainer.h
        include/containers/CloneRepoContainer.h
        include/containers/GitCredentialsContainer.h
        include/containers/GitKeysContainer.h
        include/containers/RepoContainer.h
        include/containers/RepoManagerBridge.h
        include/containers/GitRepoManagerBridgeImpl.h
        include/containers/HgRepoManagerBridgeImpl.h
        include/containers/SvnRepoManagerBridgeImpl.h
        include/containers/HomeContainer.h
        include/containers/InitRepoContainer.h
        include/containers/IssueTrackerContainer.h
        include/containers/PleaseWaitContainer.h
        include/containers/ReleaseNotesContainer.h
        include/containers/SettingsContainer.h
        include/delegates/ElideDelegate.h
        include/delegates/ElidePartialColorDelegate.h
        include/delegates/FixedHeightDelegate.h
        include/delegates/GraphiteBranchDelegate.h
        include/CustomWorkflowStore.h
        include/widgets/GitFlowNextWorkflowOverlayWidget.h
        include/delegates/HistorySliceTableColumnDelegate.h
        include/delegates/HistorySummaryTableColumnDelegate.h
        include/delegates/HomeRecentItemDelegate.h
        include/dialogs/AboutDialog.h
        include/layouts/FlowLayout.h
        include/models/BranchesModel.h
        include/models/GitCommitListModel.h
        include/models/GitCommitTreeModel.h
        include/models/GitHistoryModel.h
        include/models/IssueTrackersModel.h
        include/models/RecentlyOpenedReposModel.h
        include/models/RemotesModel.h
        include/models/StashesModel.h
        include/models/SubmodulesModel.h
        include/models/TagsModel.h
        include/models/WorkspacesModel.h
        include/models/WorktreesModel.h
        include/widgets/AddRemoteOverlayWidget.h
        include/widgets/AddSubmoduleOverlayWidget.h
        include/widgets/AddTargetOverlayWidget.h
        include/widgets/AddWorktreeOverlayWidget.h
        include/widgets/AnnotatedTagOverlayWidget.h
        include/widgets/CentralWidget.h
        include/widgets/ClickableHeaderView.h
        include/widgets/ColorButton.h
        include/widgets/ConsoleWidget.h
        include/widgets/ContainerWidget.h
        include/widgets/EditRemoteOverlayWidget.h
        include/widgets/EditTargetOverlayWidget.h
        include/widgets/ElidedLabel.h
        include/widgets/ErrorToast.h
        include/widgets/ExtendedTabBar.h
        include/widgets/ExtendedTabWidget.h
        include/widgets/GitCredentialsOverlayWidget.h
        include/widgets/SshPassphraseOverlayWidget.h
        include/widgets/GitSubmodulesPage.h
        include/widgets/GitProgressToast.h
        include/widgets/GitTagsPage.h
        include/widgets/GitWorktreesPage.h
        include/widgets/InfoToast.h
        include/widgets/LocalGitBranchesPage.h
        include/widgets/NewsToast.h
        include/widgets/MergeDiffView.h
        include/widgets/MoveProxy.h
        include/widgets/MouseTrackingSplitter.h
        include/widgets/OverlayWidget.h
        include/widgets/OpaqueGroupBox.h
        include/widgets/OpaqueLabel.h
        include/widgets/OpaquePushButton.h
        include/widgets/PersistentPlaceholderLineEdit.h
        include/widgets/ProfileBoardWidget.h
        include/widgets/IssueTrackersTableView.h
        include/widgets/RecentTableView.h
        include/widgets/WorkspacesTableView.h
        include/widgets/RemoteGitBranchesPage.h
        include/widgets/RightClickSelectListView.h
        include/widgets/RightClickSelectTreeView.h
        include/widgets/RemotesPage.h
        include/widgets/SettingsPaneWidget.h
        include/widgets/SettingsPaneItemWidget.h
        include/widgets/SettingsPaneBooleanWidget.h
        include/widgets/SettingsPaneColorWidget.h
        include/widgets/SettingsPaneDateTimeWidget.h
        include/widgets/SettingsPaneLongWidget.h
        include/widgets/SettingsPaneStringWidget.h
        include/widgets/ShadeIconToolbox.h
        include/widgets/ShortcutsToolbarWidget.h
        include/widgets/SideBySideDiffView.h
        include/widgets/SpinnerDialog.h
        include/widgets/RefBadgesOverlay.h
        include/widgets/SpinnerOverlayWidget.h
        include/widgets/StashesPage.h
        include/widgets/TextInputOverlayWidget.h
        include/widgets/MultilineTextInputOverlayWidget.h
        include/widgets/TitleBarButton.h
        include/widgets/TitleBarHomeButton.h
        include/widgets/TitleBarMenuButton.h
        include/widgets/TitleBarTabBar.h
        include/widgets/TitlebarWidget.h
        include/widgets/ToastCloseButton.h
        include/widgets/ToastHostWidget.h
        include/widgets/ToastWidget.h
        include/widgets/ToolBoxFixer.h
        include/widgets/TopBarWidget.h
        include/widgets/UpstreamInputOverlayWidget.h
        include/widgets/factories/SettingsPaneItemWidgetFactory.h
        src/DarkStyle.cpp
        src/DarkThemeLexersHighlighter.cpp
        src/GuiManager.cpp
        src/HitDetector.cpp
        src/WindowResize.cpp
        src/InitResources.cpp
        src/MainWindow.cpp
        src/MousePositionCorrection.cpp
        src/QScintillaLexerFactory.cpp
        src/ReleaseNotes.cpp
        src/containers/BugReportContainer.cpp
        src/containers/CloneRepoContainer.cpp
        src/containers/GitCredentialsContainer.cpp
        src/containers/GitKeysContainer.cpp
        src/containers/RepoContainer.cpp
        src/containers/GitRepoManagerBridgeImpl.cpp
        src/containers/HgRepoManagerBridgeImpl.cpp
        src/containers/SvnRepoManagerBridgeImpl.cpp
        src/containers/HomeContainer.cpp
        src/containers/InitRepoContainer.cpp
        src/containers/IssueTrackerContainer.cpp
        src/containers/PleaseWaitContainer.cpp
        src/containers/ReleaseNotesContainer.cpp
        src/containers/SettingsContainer.cpp
        src/delegates/GraphiteBranchDelegate.cpp
        src/CustomWorkflowStore.cpp
        src/widgets/GitFlowNextWorkflowOverlayWidget.cpp
        src/delegates/HistorySliceTableColumnDelegate.cpp
        src/delegates/HistorySummaryTableColumnDelegate.cpp
        src/delegates/HomeRecentItemDelegate.cpp
        src/dialogs/AboutDialog.cpp
        src/layouts/FlowLayout.cpp
        src/models/BranchesModel.cpp
        src/models/GitCommitListModel.cpp
        src/models/GitCommitTreeModel.cpp
        src/models/GitHistoryModel.cpp
        src/models/IssueTrackersModel.cpp
        src/models/RecentlyOpenedReposModel.cpp
        src/models/RemotesModel.cpp
        src/models/StashesModel.cpp
        src/models/SubmodulesModel.cpp
        src/models/WorkspacesModel.cpp
        src/models/WorktreesModel.cpp
        src/models/TagsModel.cpp
        src/widgets/AddRemoteOverlayWidget.cpp
        src/widgets/AddSubmoduleOverlayWidget.cpp
        src/widgets/AddTargetOverlayWidget.cpp
        src/widgets/AddWorktreeOverlayWidget.cpp
        src/widgets/AnnotatedTagOverlayWidget.cpp
        src/widgets/CentralWidget.cpp
        src/widgets/ColorButton.cpp
        src/widgets/ConsoleWidget.cpp
        src/widgets/ContainerWidget.cpp
        src/widgets/EditRemoteOverlayWidget.cpp
        src/widgets/EditTargetOverlayWidget.cpp
        src/widgets/ElidedLabel.cpp
        src/widgets/ErrorToast.cpp
        src/widgets/GitCredentialsOverlayWidget.cpp
        src/widgets/SshPassphraseOverlayWidget.cpp
        src/widgets/GitProgressToast.cpp
        src/widgets/GitSubmodulesPage.cpp
        src/widgets/GitTagsPage.cpp
        src/widgets/GitWorktreesPage.cpp
        src/widgets/InfoToast.cpp
        src/widgets/LocalGitBranchesPage.cpp
        src/widgets/NewsToast.cpp
        src/widgets/MergeDiffView.cpp
        src/widgets/MouseTrackingSplitter.cpp
        src/widgets/ProfileBoardWidget.cpp
        src/widgets/IssueTrackersTableView.cpp
        src/widgets/RecentTableView.cpp
        src/widgets/WorkspacesTableView.cpp
        src/widgets/RemoteGitBranchesPage.cpp
        src/widgets/RemotesPage.cpp
        src/widgets/SettingsPaneWidget.cpp
        src/widgets/SettingsPaneItemWidget.cpp
        src/widgets/SettingsPaneBooleanWidget.cpp
        src/widgets/SettingsPaneColorWidget.cpp
        src/widgets/SettingsPaneDateTimeWidget.cpp
        src/widgets/SettingsPaneLongWidget.cpp
        src/widgets/SettingsPaneStringWidget.cpp
        src/widgets/ShadeIconToolbox.cpp
        src/widgets/ShortcutsToolbarWidget.cpp
        src/widgets/SideBySideDiffView.cpp
        src/widgets/RefBadgesOverlay.cpp
        src/widgets/SpinnerOverlayWidget.cpp
        src/widgets/StashesPage.cpp
        src/widgets/TextInputOverlayWidget.cpp
        src/widgets/MultilineTextInputOverlayWidget.cpp
        src/widgets/TitleBarButton.cpp
        src/widgets/TitleBarHomeButton.cpp
        src/widgets/TitleBarMenuButton.cpp
        src/widgets/TitleBarTabBar.cpp
        src/widgets/TitlebarWidget.cpp
        src/widgets/ToastCloseButton.cpp
        src/widgets/ToastHostWidget.cpp
        src/widgets/ToastWidget.cpp
        src/widgets/TopBarWidget.cpp
        src/widgets/UpstreamInputOverlayWidget.cpp
        src/widgets/factories/SettingsPaneItemWidgetFactory.cpp
        ${UI_FILES}
        ${QRC_FILES}
        ${TS_FILES}
)

qt_create_translation(QM_FILES ${CMAKE_SOURCE_DIR} ${TS_FILES})

add_library(GuiManager STATIC ${GUIMANAGER_SOURCE_FILES} ${PLATFORM_SOURCES})

include(../../cmake/helpers.cmake)

includeComponent(GuiManager ComponentManager)
includeComponent(GuiManager ConfigManager)
includeComponent(GuiManager GitRepoManager)
includeComponent(GuiManager HgRepoManager)
includeComponent(GuiManager NewsTracker)
includeComponent(GuiManager SvnRepoManager)
includeComponent(GuiManager StylesheetManager)

includeLib(GuiManager Common)
includeLib(GuiManager GitRepo)
includeLib(GuiManager HgRepo)
includeLib(GuiManager IssueTrackers)
includeLib(GuiManager Log)
includeLib(GuiManager SvnRepo)
includeLib(GuiManager Util)
includeLib(GuiManager QtWaitingSpinner)
includeLib(GuiManager QVTerminal)
includeLib(GuiManager QEditor)

target_link_libraries(GuiManager PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Svg
        Qt6::Widgets
        Qt6::Network
        libgit2
        util
        libzip::zip
        apr-1
)

if(TARGET Qt6::WebEngineWidgets)
  target_link_libraries(GuiManager PRIVATE Qt6::WebEngineWidgets)
  target_compile_definitions(GuiManager PRIVATE GUI_MANAGER_HAS_WEBENGINE=1)
endif()

target_include_directories(GuiManager PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${LIBGIT2_INCLUDE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/subversion/subversion/include
        ${CMAKE_CURRENT_SOURCE_DIR}/../../lib/subversion/subversion/bindings/cxx/include
)
