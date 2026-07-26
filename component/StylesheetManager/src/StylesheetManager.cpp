#include "StylesheetManager.h"

#include <Log.h>

#include <QAbstractButton>
#include <QApplication>
#include <QColor>
#include <QImage>
#include <QMap>
#include <QPalette>
#include <QWidget>

#define THREAD_ID "StylesheetManager"

#define SOURCERER_DARK_THEME_NAME "SourcererDark"
#define SOURCERER_LIGHT_THEME_NAME "SourcererLight"

namespace StylesheetManager {
namespace {

void applyPalette(QApplication *app, StylesheetKey key) {
  if (!app)
    return;

  QPalette palette;

  if (key == StylesheetKey::SourcererLight) {
    palette.setColor(QPalette::Window, QColor(245, 245, 245));
    palette.setColor(QPalette::WindowText, QColor(32, 32, 32));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(160, 160, 160));
    palette.setColor(QPalette::Base, QColor(255, 255, 255));
    palette.setColor(QPalette::AlternateBase, QColor(248, 248, 248));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(32, 32, 32));
    palette.setColor(QPalette::Text, QColor(32, 32, 32));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(160, 160, 160));
    palette.setColor(QPalette::Dark, QColor(220, 220, 220));
    palette.setColor(QPalette::Shadow, QColor(180, 180, 180));
    palette.setColor(QPalette::Button, QColor(245, 245, 245));
    palette.setColor(QPalette::ButtonText, QColor(32, 32, 32));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(160, 160, 160));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Link, QColor(0, 120, 215));
    palette.setColor(QPalette::Highlight, QColor(0, 120, 215));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
                     QColor(200, 200, 200));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(160, 160, 160));
  } else {
    palette.setColor(QPalette::Window, QColor(53, 53, 53));
    palette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::Base, QColor(42, 42, 42));
    palette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    palette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    palette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    palette.setColor(QPalette::Text, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(127, 127, 127));
    palette.setColor(QPalette::Dark, QColor(35, 35, 35));
    palette.setColor(QPalette::Shadow, QColor(20, 20, 20));
    palette.setColor(QPalette::Button, QColor(53, 53, 53));
    palette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(127, 127, 127));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
                     QColor(80, 80, 80));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(127, 127, 127));
  }

  app->setPalette(palette);
}

} // namespace

const QString SOURCERER_DARK_STYLESHEET = QStringLiteral(R"(
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
      QWidget#ProfileBoardWidget {
        background-color: palette(window);
      }
      QAbstractItemView {
        background-color: palette(window);
        color: palette(window-text);
      }
      QAbstractItemView::viewport {
        background-color: palette(window);
      }
      QListView#listViewProfiles {
        selection-background-color: #4A5A66;
        selection-color: white;
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
      QLineEdit#lineEditTextSshPassphraseOverlayWidgetPassphrase{
        border: 2px solid gray;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextSshPassphraseOverlayWidgetPassphrase:focus{
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
      QCheckBox {
        color: palette(window-text);
        spacing: 6px;
      }
      QCheckBox::indicator {
        width: 13px;
        height: 13px;
        border: 1px solid palette(button-text);
        border-radius: 3px;
        background-color: transparent;
      }
      QCheckBox::indicator:checked {
        border: 1px solid #0078d7;
        background-color: transparent;
        image: url(:/icons/checkbox-check-dark.svg);
      }
      QCheckBox::indicator:disabled {
        border: 1px solid palette(mid);
        background-color: palette(button);
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
      QPushButton#stashApplyButton{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashApplyButton:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashPopButton{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashPopButton:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#displayTreeExpandAllButton{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#displayTreeExpandAllButton:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop,
      QPushButton#pushButtonStash,
      QToolButton#pullFetchDropdown,
      QToolButton#pushButtonPush{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:hover,
      QPushButton#pushButtonStash:hover,
      QToolButton#pullFetchDropdown:hover,
      QToolButton#pushButtonPush:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QToolButton#pullFetchDropdown::menu-button,
      QToolButton#pushButtonPush::menu-button{
        border: none;
        background: transparent;
      }
      QToolButton#pullFetchDropdown::menu-indicator,
      QToolButton#pushButtonPush::menu-indicator{
        subcontrol-origin: padding;
        subcontrol-position: right center;
        width: 8px;
        height: 8px;
      }
      QPushButton#pushButtonAbort,
      QPushButton#pushButtonCancel,
      QPushButton#deleteButton{
        background-color: #8b0000;
        border: 1px solid gray;
        border-radius: 5px;
        color: white;
        padding: 5px;
      }
      QPushButton#pushButtonAbort:hover,
      QPushButton#pushButtonCancel:hover,
      QPushButton#deleteButton:hover{
        background-color: #b22222;
        border: 1px solid gray;
        border-radius: 5px;
        color: white;
        padding: 5px;
      }
      QPushButton#refreshButton,
      QPushButton#generateButton,
      QPushButton#addButton,
      QPushButton#editButton,
      QPushButton#signingKeyBrowseButton,
      QPushButton#signingProgramBrowseButton,
      QPushButton#signingResetButton,
      QPushButton#signingSaveButton{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#refreshButton:hover,
      QPushButton#generateButton:hover,
      QPushButton#addButton:hover,
      QPushButton#editButton:hover,
      QPushButton#signingKeyBrowseButton:hover,
      QPushButton#signingProgramBrowseButton:hover,
      QPushButton#signingResetButton:hover,
      QPushButton#signingSaveButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#openButton{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#openButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#cloneButton{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#cloneButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QToolButton#connectToButton{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QToolButton#connectToButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#initButton{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#initButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#pushButtonBrowse{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonBrowse:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#pushButtonClone{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClone:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonInit{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonInit:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonCommit{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonCommit:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonSubmit{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonSubmit:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMerge{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMerge:hover{
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
      QPushButton#pushButtonDiffClose{
        background-color: #353535;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffClose:hover{
        background-color: #384D59;
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
      QPushButton#pushButtonMergeDiffDown{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffDown:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffUp{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffUp:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffSave{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffSave:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffClose{
        background-color: #353535;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffClose:hover{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified:checked{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide:checked{
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
      QTextEdit {
        color: white;
      }
      QTextEdit::placeholder {
        color: #C0C0C0;
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
      QToolButton#toolButtonCommitSigningBlockToggle,
      QToolButton#toolButtonPageDisplaySignatureToggle {
        background: transparent;
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QToolButton#toolButtonCommitSigningBlockToggle:hover,
      QToolButton#toolButtonPageDisplaySignatureToggle:hover {
        background: transparent;
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
        border-bottom: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        color: white;
      }
      QToolBox::tab:selected{
        background: #384D59;
        border-bottom: 1px solid gray;
        font-weight:bold;
        color: white;
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
      QVTerminal {
        background-color: #012456;
        border: none;
      }
      GuiManager--Widgets--TitleBarMenuButton::menu-indicator{
        width:0px;
      }
      GuiManager--Widgets--TitleBarButton {
        border: none;
        padding: 0px;
        margin: 0px;
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton,
      GuiManager--Widgets--TitleBarButton#titleBarMinimizeButton,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton {
        background-color: #353535;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarMinimizeButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton:hover {
        background-color: #5E5E5E;
      }
      GuiManager--Widgets--TitleBarButton#titleBarCloseButton {
        background-color: #B00020;
      }
      GuiManager--Widgets--TitleBarButton#titleBarCloseButton:hover {
        background-color: #D32F2F;
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
      QScrollBar:vertical {
        border: none;
        background: #3e3e42;
        width: 14px;
        margin: 0px;
      }
      QScrollBar::handle:vertical {
        background: #686868;
        min-height: 20px;
        border-radius: 7px;
      }
      QScrollBar::handle:vertical:hover {
        background: #9e9e9e;
      }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
      }
      QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: none;
      }
      QScrollBar:horizontal {
        border: none;
        background: #3e3e42;
        height: 14px;
        margin: 0px;
      }
      QScrollBar::handle:horizontal {
        background: #686868;
        min-width: 20px;
        border-radius: 7px;
      }
      QScrollBar::handle:horizontal:hover {
        background: #9e9e9e;
      }
      QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
        width: 0px;
      }
      QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
        background: none;
      }
      QTabWidget#tabWidgetStashFiles::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab {
        background: transparent;
        border: 1px solid #6D6E6E;
        border-radius: 8px;
        padding: 6px 12px;
        margin: 4px;
        font-weight: normal;
        color: white;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
    )");

const QString SOURCERER_LIGHT_STYLESHEET = QStringLiteral(R"(

      QColorDialog{
        font-size:14px;
      }
      QGroupBox#groupBoxTextOverlayWidgetInput {
        border: 1px solid #B8C0C8;
        border-radius: 10px;
        padding: 10px;
      }
      QGroupBox#groupBoxGiCredentialsTextOverlayWidgetCredentials {
        border: 1px solid #B8C0C8;
        border-radius: 10px;
        padding: 10px;
      }
      QLabel#labelRecent {
        font-weight:bold;
      }
      QLabel#labelWorkspaces {
        font-weight:bold;
      }
      QWidget#ProfileBoardWidget {
        background-color: palette(window);
      }
      QAbstractItemView {
        background-color: palette(window);
        color: palette(window-text);
      }
      QAbstractItemView::viewport {
        background-color: palette(window);
      }
      QListView#listViewProfiles {
        selection-background-color: #E7EEF4;
        selection-color: #202020;
      }
      QLineEdit{
        color:#202020;
      }
      QLineEdit::placeholder{
        color: #666666;
      }
      QLineEdit#lineEditTextOverlayWidgetInput{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInput:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputRemote{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputRemote:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputBranch{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextOverlayWidgetInputBranch:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetUsername{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetUsername:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetPassword{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextGitCredentialsOverlayWidgetPassword:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextSshPassphraseOverlayWidgetPassphrase{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditTextSshPassphraseOverlayWidgetPassphrase:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetUrl{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetUrl:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetName{
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QLineEdit#lineEditAddSubmoduleOverlayWidgetName:focus{
        border: 2px solid #0078d7;
        border-radius: 5px;
      }
      QAbstractItemView {
        background-color: palette(window);
        color: palette(window-text);
        alternate-background-color: palette(window);
        selection-background-color: #E7EEF4;
        selection-color: #202020;
        gridline-color: #D0D7DE;
        outline: 0;
      }
      QCheckBox {
        color: palette(window-text);
        spacing: 6px;
      }
      QCheckBox::indicator {
        width: 13px;
        height: 13px;
        border: 1px solid palette(button-text);
        border-radius: 3px;
        background-color: transparent;
      }
      QCheckBox::indicator:checked {
        border: 1px solid #0078d7;
        background-color: transparent;
        image: url(:/icons/checkbox-check-light.svg);
      }
      QCheckBox::indicator:disabled {
        border: 1px solid palette(mid);
        background-color: palette(button);
      }
      QListView, QTreeView, QTableView {
        background-color: palette(window);
        color: palette(window-text);
      }
      QTreeView::branch {
        background: palette(window);
      }
      QTreeView::branch:closed:has-children {
        image: url(:/icons/tree_branch_closed.svg);
        width: 12px;
        height: 12px;
      }
      QTreeView::branch:open:has-children {
        image: url(:/icons/tree_branch_open.svg);
        width: 12px;
        height: 12px;
      }
      QTableView#tableViewRecent, QTableView#tableViewWorkspaces {
        color: palette(window-text);
        background-color: palette(window);
        alternate-background-color: palette(window);
      }
      QListView::item, QTreeView::item, QTableView::item {
        padding: 2px 4px;
      }
      QListView::item:selected,
      QTreeView::item:selected,
      QTableView::item:selected {
        background-color: #E7EEF4;
        color: #202020;
      }
      QHeaderView::section {
        background-color: palette(window);
        color: palette(window-text);
        border: 1px solid #B8C0C8;
        padding: 3px 5px;
      }
      QCheckBox {
        color: palette(window-text);
        spacing: 6px;
      }
      QCheckBox::indicator {
        width: 13px;
        height: 13px;
        border: 1px solid palette(button-text);
        border-radius: 3px;
        background-color: transparent;
      }
      QCheckBox::indicator:checked {
        border: 1px solid palette(highlight);
        background-color: transparent;
        image: url(:/icons/checkbox-check-light.svg);
      }
      QCheckBox::indicator:disabled {
        border: 1px solid palette(mid);
        background-color: palette(button);
      }

      QPushButton[flat=true]{
        color:#202020;
        text-decoration:none;
      }
      QPushButton[flat=true]:hover{
        color:#2A82DA;
        text-decoration: underline;
      }
      QPushButton#commitAndMergeIndexTreeUnstageAllButton{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeIndexTreeUnstageAllButton:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeWorkingDirTreeStageAllButton{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#commitAndMergeWorkingDirTreeStageAllButton:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashApplyButton{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashApplyButton:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashPopButton{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#stashPopButton:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#displayTreeExpandAllButton{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#displayTreeExpandAllButton:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop,
      QPushButton#pushButtonStash,
      QToolButton#pullFetchDropdown,
      QToolButton#pushButtonPush{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:hover,
      QPushButton#pushButtonStash:hover,
      QToolButton#pullFetchDropdown:hover,
      QToolButton#pushButtonPush:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QToolButton#pullFetchDropdown::menu-button,
      QToolButton#pushButtonPush::menu-button{
        border: none;
        background: transparent;
      }
      QToolButton#pullFetchDropdown::menu-indicator,
      QToolButton#pushButtonPush::menu-indicator{
        subcontrol-origin: padding;
        subcontrol-position: right center;
        width: 8px;
        height: 8px;
      }
      QPushButton#pushButtonAbort,
      QPushButton#pushButtonCancel,
      QPushButton#deleteButton{
        background-color: #8b0000;
        border: 1px solid gray;
        border-radius: 5px;
        color: white;
        padding: 5px;
      }
      QPushButton#pushButtonAbort:hover,
      QPushButton#pushButtonCancel:hover,
      QPushButton#deleteButton:hover{
        background-color: #b22222;
        border: 1px solid gray;
        border-radius: 5px;
        color: white;
        padding: 5px;
      }
      QPushButton#refreshButton,
      QPushButton#generateButton,
      QPushButton#addButton,
      QPushButton#editButton,
      QPushButton#signingKeyBrowseButton,
      QPushButton#signingProgramBrowseButton,
      QPushButton#signingResetButton,
      QPushButton#signingSaveButton{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#refreshButton:hover,
      QPushButton#generateButton:hover,
      QPushButton#addButton:hover,
      QPushButton#editButton:hover,
      QPushButton#signingKeyBrowseButton:hover,
      QPushButton#signingProgramBrowseButton:hover,
      QPushButton#signingResetButton:hover,
      QPushButton#signingSaveButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#openButton{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#openButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#cloneButton{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#cloneButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QToolButton#connectToButton{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QToolButton#connectToButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#initButton{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#initButton:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#pushButtonBrowse{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonBrowse:hover{
        background-color: #4a6070;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
        text-decoration: none;
      }
      QPushButton#pushButtonClone{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClone:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonInit{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonInit:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonCommit{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonCommit:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonSubmit{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonSubmit:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMerge{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMerge:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffUnified:checked{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffClose{
        background-color: #353535;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffClose:hover{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonDiffSideBySide:checked{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffDown{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffDown:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffUp{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffUp:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffSave{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffSave:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffClose{
        background-color: #353535;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMergeDiffClose:hover{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashClose{
        background-color: #353535;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashClose:hover{
        background-color: #384D59;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffUnified:checked{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide{
        background-color: #E7EEF4;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonStashDiffSideBySide:checked{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputSubmit{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputSubmit:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputCancel{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonTextOverlayWidgetInputCancel:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QTextEdit {
        color: #202020;
      }
      QTextEdit::placeholder {
        color: #666666;
      }
      QTextEdit:focus {
        border: 2px solid #53A0ED;
      }
      QTreeView::item:selected {
        background-color: #E7EEF4;
      }
      QGroupBox#groupBoxTabCommit{
        background-color: #F3F6F8;
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QGroupBox#groupBoxInfo{
        border: none;
      }
      QGroupBox#groupBoxSummary{
        background-color: #F3F6F8;
        border: 1px solid #B8C0C8;
        border-radius: 5px;
      }
      QGroupBox#groupBoxFiles{
        border: none;
      }
      QListView#listViewProfiles {
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
      QToolButton#toolButtonCommitSigningBlockToggle,
      QToolButton#toolButtonPageDisplaySignatureToggle {
        background: transparent;
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QToolButton#toolButtonCommitSigningBlockToggle:hover,
      QToolButton#toolButtonPageDisplaySignatureToggle:hover {
        background: transparent;
      }
      QTabBar#tabWidgetCommitTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabBar#tabWidgetCommitTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabBar#tabWidgetCommitTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }
      QTabWidget#tabWidgetIndex::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetIndexTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabBar#tabWidgetIndexTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabBar#tabWidgetIndexTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }
      QTabWidget#tabWidgetWorkingTree::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabBar#tabWidgetWorkingTreeTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }
      QTabWidget#tabWidgetPageDisplayFiles::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }
      QTabWidget#tabWidgetPageDisplayFilesTabBar::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabBar#tabWidgetPageDisplayFilesTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }
      QToolBox::tab{
        background: #F3F6F8;
        border: 1px solid #B8C0C8;
        border-bottom: 1px solid #B8C0C8;
        border-radius: 5px;
        padding: 5px;
        color: #202020;
      }
      QToolBox::tab:selected{
        background: #E7EEF4;
        border-bottom: 1px solid #B8C0C8;
        font-weight: bold;
        color: #202020;
      }
      QToolTip{
        font-size:14px;
        background-color:#E7EEF4;
        color:#202020;border:1px;
        padding: 3px
      }
      QTableView::item:selected{
        font-weight:bold;
        background-color:#E7EEF4;
      }
      QVTerminal {
        background-color: #012456;
        border: none;
      }
      GuiManager--Widgets--TitleBarMenuButton::menu-indicator{
        width:0px;
      }
      GuiManager--Widgets--TitleBarButton {
        border: none;
        padding: 0px;
        margin: 0px;
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton {
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarMinimizeButton,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton {
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarMinimizeButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton:hover {
        background-color: #c4bbbb;
      }
      GuiManager--Widgets--TitleBarButton#titleBarCloseButton {
        background-color: #C62828;
      }
      GuiManager--Widgets--TitleBarButton#titleBarCloseButton:hover {
        background-color: #E53935;
      }
      GuiManager--Widgets--TitlebarWidget,GuiManager--Widgets--TitlebarWidget * {
        font-size:14px;
      }
      GuiManager--Containers--GitRepoContainer{
        font-size:14px;
        background-color:#E7EEF4;
      }
      GuiManager--Containers--HomeContainer{
        font-size:14px;
        background-color:#E7EEF4;
      }
      GuiManager--Containers--SettingsContainer{
        font-size:14px;
      }
      GuiManager--Containers--SettingsContainer QSplitter{
        margin-top:30px;
      }
      GuiManager--Containers--SettingsContainer QSplitter::handle:horizontal{
        border:1px solid #E7EEF4;
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
        background-color:#E7EEF4;
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
        qproperty-hoverColor:#F7FAFC;
      }
      GuiManager--Widgets--ToastWidget{
        font-size:14px;
        background-color:yellow;
      }
      QScrollBar:vertical {
        border: none;
        background: #F4F5F7;
        width: 14px;
        margin: 0px;
      }
      QScrollBar::handle:vertical {
        background: #686868;
        min-height: 20px;
        border-radius: 7px;
      }
      QScrollBar::handle:vertical:hover {
        background: #9e9e9e;
      }
      QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
      }
      QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: none;
      }
      QScrollBar:horizontal {
        border: none;
        background: #F4F5F7;
        height: 14px;
        margin: 0px;
      }
      QScrollBar::handle:horizontal {
        background: #686868;
        min-width: 20px;
        border-radius: 7px;
      }
      QScrollBar::handle:horizontal:hover {
        background: #9e9e9e;
      }
      QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {
        width: 0px;
      }
      QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {
        background: none;
      }
      QTabWidget#tabWidgetStashFiles::pane {
        border: none;
        padding: 0px;
        margin: 0px;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab {
        background: transparent;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        padding: 4px 8px;
        margin: 2px;
        font-weight: normal;
        color: #202020;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab:selected {
        background-color: #E7EEF4;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: #202020;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab:hover {
        background-color: #E7EEF4;
        color: #202020;
      }

    )");
QMap<StylesheetKey, QString> STYLESHEETS = {
    {StylesheetKey::SourcererLight, SOURCERER_LIGHT_STYLESHEET},
    {StylesheetKey::SourcererDark, SOURCERER_DARK_STYLESHEET}};
StylesheetKey CURRENT_STYLESHEET = StylesheetKey::SourcererLight;

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

  const auto resolvedKey = STYLESHEETS.contains(key) ? key : CURRENT_STYLESHEET;
  return STYLESHEETS[resolvedKey];
}

StylesheetKey StylesheetManager::currentStylesheet() {
  return CURRENT_STYLESHEET;
}

void StylesheetManager::setCurrentStylesheet(StylesheetKey key) {
  LOG_PERF(0, "Enter");

  if (STYLESHEETS.contains(key)) {
    CURRENT_STYLESHEET = key;
  }

  LOG_PERF(0, "Exit");
}

QString StylesheetManager::stylesheetKeyName(StylesheetKey key) {
  switch (key) {
  case StylesheetKey::SourcererLight:
    return SOURCERER_LIGHT_THEME_NAME;
  case StylesheetKey::SourcererDark:
    return SOURCERER_DARK_THEME_NAME;
  default:
    return SOURCERER_DARK_THEME_NAME;
  }
}

StylesheetKey StylesheetManager::stylesheetKeyFromName(const QString &name) {
  if (name == SOURCERER_LIGHT_THEME_NAME)
    return StylesheetKey::SourcererLight;
  if (name == SOURCERER_DARK_THEME_NAME)
    return StylesheetKey::SourcererDark;
  return StylesheetKey::SourcererDark;
}

QColor StylesheetManager::menuBarButtonIconColor() {
  if (!qApp)
    return QColor(Qt::black);

  return qApp->palette().color(QPalette::ButtonText);
}

QColor StylesheetManager::historyBadgeFillColor() {
  switch (currentStylesheet()) {
  case StylesheetKey::SourcererLight:
    return QColor("#F5EEE4");
  case StylesheetKey::SourcererDark:
  default:
    return QColor("#4A5A66");
  }
}

QColor StylesheetManager::historyBadgeTextColor() {
  if (!qApp)
    return QColor(96, 96, 96);

  const QColor text = qApp->palette().color(QPalette::ButtonText);
  return QColor::fromRgbF(text.redF(), text.greenF(), text.blueF(), 1.0);
}

QPixmap StylesheetManager::recolorPixmap(const QPixmap &source,
                                         const QColor &color) {
  if (source.isNull())
    return source;

  QImage image = source.toImage().convertToFormat(QImage::Format_ARGB32);

  for (int y = 0; y < image.height(); ++y) {
    for (int x = 0; x < image.width(); ++x) {
      const QColor pixelColor = image.pixelColor(x, y);
      if (pixelColor.alpha() == 0)
        continue;

      image.setPixelColor(
          x, y,
          QColor(color.red(), color.green(), color.blue(), pixelColor.alpha()));
    }
  }

  return QPixmap::fromImage(image);
}

void StylesheetManager::refreshMenuBarButtonIcons(QWidget *root) {
  const QColor iconColor = menuBarButtonIconColor();

  const auto widgets =
      root ? root->findChildren<QWidget *>() : qApp->allWidgets();
  for (QWidget *widget : widgets) {
    auto *button = qobject_cast<QAbstractButton *>(widget);
    if (!button)
      continue;

    const QVariant iconSourceVariant = button->property("iconSource");
    if (!iconSourceVariant.isValid())
      continue;

    const QString iconSource = iconSourceVariant.toString();
    if (iconSource.isEmpty())
      continue;

    button->setIcon(recolorPixmap(QPixmap(iconSource), iconColor));
  }
}

void StylesheetManager::applyStylesheet(QApplication *app) {
  LOG_PERF(0, "Enter");

  const StylesheetKey key = CURRENT_STYLESHEET;
  if (app) {
    applyPalette(app, key);
    app->setStyleSheet(STYLESHEETS.value(key));
    refreshMenuBarButtonIcons();
  }

  LOG_PERF(0, "Exit");
}

} // namespace StylesheetManager
