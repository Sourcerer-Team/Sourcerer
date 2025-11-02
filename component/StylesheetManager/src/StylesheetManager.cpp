#include "StylesheetManager.h"

#include <Log.h>

#include <QMap>

#define THREAD_ID "StylesheetManager"

namespace StylesheetManager {

QMap<StylesheetKey, QString> STYLESHEETS = {{StylesheetKey::Default,
                                             R"(
      QColorDialog{
        font-size:14px;
      }
      QGroupBox#groupBoxTextOverlayWidgetInput {
        border: 2px solid gray;
        border-radius: 10px;
        padding: 10px;
      }
      QGroupBox#groupBoxGiCredentialsTextOverlayWidgetCredentials {
        border: 2px solid gray;
        border-radius: 10px;
        padding: 10px;
      }
      QLabel#labelRecent {
        font-weight:bold;
      }
      QLabel#labelWorkspaces {
        font-weight:bold;
      }
      QLineEdit{
        color:white;
      }
      QLineEdit::placeholder{
        color: #C0C0C0;
      }
      QLineEdit#lineEditTextOverlayWidgetInput{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInput:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputRemote{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputRemote:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputBranch{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputBranch:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetUsername{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetUsername:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetPassword{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetPassword:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetUrl{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetUrl:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetName{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetName:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QListView::item:selected {
        background-color: #384D59;
      }
      QPushButton[flat=true]{
        color:white;
        text-decoration:none;
      }
      QPushButton[flat=true]:hover{
        color:#2A82DA;
        text-decoration: underline;
      }
      QPushButton#commitAndMergeIndexTreeUnstageAllButton{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeIndexTreeUnstageAllButton:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeWorkingDirTreeStageAllButton{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeWorkingDirTreeStageAllButton:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified:checked{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide:checked{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputSubmit{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputSubmit:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputCancel{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputCancel:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QTextEdit:focus {
        border: 2px solid #53A0ED;
      }
      QTreeView::item:selected {
        background-color: #384D59;
      }
      QGroupBox#groupBoxTabCommit{
        background-color: black;
      }
      QGroupBox#groupBoxInfo{
        border: none;
      }
      QGroupBox#groupBoxSummary{
        background-color: black;
      }
      QGroupBox#groupBoxFiles{
        border: none;
      }
      QListView#listViewProfiles {
        background-color: #505050;
        border: none;
      }
      QTabBar::tab:selected{
        font-weight:bold;
      }
      QTabWidget#tabWidgetCommit::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetCommitTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabBar#tabWidgetCommitTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabBar#tabWidgetCommitTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QTabWidget#tabWidgetIndex::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetIndexTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabBar#tabWidgetIndexTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabBar#tabWidgetIndexTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QTabWidget#tabWidgetWorkingTree::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QTabWidget#tabWidgetPageDisplayFiles::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QTabWidget#tabWidgetPageDisplayFilesTabBar::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QToolBox::tab{
        background: #353535;
        border: 1px solid gray;
        border-bottom gray: 1px solid;
        border-radius: 5px;
        padding: 5px;
      }
      QToolBox::tab:selected{
        background: #384D59;
        border-bottom: 1px solid gray;
        font-weight:bold;
      }
      QToolTip{
        font-size:14px;
        background-color:#384D59;
        color:white;border:1px;
        padding: 3px
      }
      QTableView::item:selected{
        font-weight:bold;
        background-color:#384D59;
      }
      GuiManager--Widgets--TitleBarMenuButton::menu-indicator{
        width:0px;
      }
      GuiManager--Widgets--TitlebarWidget,GuiManager--Widgets--TitlebarWidget * {
        font-size:14px;
      }
      GuiManager--Containers--GitRepoContainer{
        font-size:14px;
        background-color:#353535;
      }
      GuiManager--Containers--HomeContainer{
        font-size:14px;
        background-color:#353535;
      }
      GuiManager--Containers--SettingsContainer{
        font-size:14px;
        background-color:#353535;
      }
      GuiManager--Containers--SettingsContainer QSplitter{
        margin-top:30px;
      }
      GuiManager--Containers--SettingsContainer QSplitter::handle:horizontal{
        border:1px solid #353535;
      }
      GuiManager--Containers--SettingsContainer QScrollArea{
        margin-top:30px;
      }
      GuiManager--Containers--SettingsContainer QTreeView{
        font-size:14px;
        margin-top:30px;
      }
      GuiManager--Containers--SettingsContainer QDateTimeEdit#dateTimeEdit{
        max-width:145px;
      }
      GuiManager--Containers--SettingsContainer QLabel#groupLabel{
        font-size:20px;
        font-weight:bold;
        background-color:#202020;
        padding:8px;
      }
      GuiManager--Containers--SettingsContainer QLabel#nameLabel{
        font-weight:bold;
      }
      GuiManager--Containers--SettingsContainer GuiManager--Widgets--ColorButton{
        max-width:75px;
      }
      GuiManager--Containers--SettingsContainer QSpinBox#longSpinBox{
        max-width:75px;
      }
      GuiManager--Containers--SettingsContainer GuiManager--Widgets--SettingsPaneItemWidget{
        padding:0px;
        qproperty-hoverColor:#282828;
      }
      GuiManager--Widgets--ToastWidget{
        font-size:14px;
        background-color:yellow;
      }
    )"}};

StylesheetManager::StylesheetManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

StylesheetManager::~StylesheetManager() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

StylesheetManager &StylesheetManager::getInstance() {
  static StylesheetManager instance;
  return instance;
}

const QString &StylesheetManager::stylesheet(StylesheetKey key) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("Stylesheet {} requested.", static_cast<int>(key));

  LOG_PERF(0, "Exit");

  return STYLESHEETS[key];
}

} // namespace StylesheetManager
