#include "StylesheetManager.h"

#include <Log.h>

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QColor>
#include <QIcon>
#include <QImage>
#include <QList>
#include <QMap>
#include <QMenu>
#include <QPainter>
#include <QPair>
#include <QPalette>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QWidget>

#define THREAD_ID "StylesheetManager"

#define SOURCERER_DARK_THEME_NAME "SourcererDark"
#define SOURCERER_LIGHT_THEME_NAME "SourcererLight"
#define SOURCERER_PURPLE_THEME_NAME "SourcererPurple"
#define SOURCERER_BLUE_THEME_NAME "SourcererBlue"

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
  } else if (key == StylesheetKey::SourcererPurple) {
    palette.setColor(QPalette::Window, QColor(51, 47, 63));
    palette.setColor(QPalette::WindowText, QColor(232, 230, 238));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(125, 120, 137));
    palette.setColor(QPalette::Base, QColor(43, 39, 53));
    palette.setColor(QPalette::AlternateBase, QColor(56, 51, 63));
    palette.setColor(QPalette::ToolTipBase, QColor(63, 58, 76));
    palette.setColor(QPalette::ToolTipText, QColor(232, 230, 238));
    palette.setColor(QPalette::Text, QColor(232, 230, 238));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(125, 120, 137));
    palette.setColor(QPalette::Dark, QColor(38, 34, 46));
    palette.setColor(QPalette::Shadow, QColor(27, 24, 33));
    palette.setColor(QPalette::Button, QColor(51, 47, 63));
    palette.setColor(QPalette::ButtonText, QColor(232, 230, 238));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(125, 120, 137));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Link, QColor(42, 130, 218));
    palette.setColor(QPalette::Highlight, QColor(55, 70, 89));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
                     QColor(74, 70, 86));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(125, 120, 137));
  } else if (key == StylesheetKey::SourcererBlue) {
    palette.setColor(QPalette::Window, QColor(33, 45, 67));
    palette.setColor(QPalette::WindowText, QColor(230, 236, 245));
    palette.setColor(QPalette::Disabled, QPalette::WindowText,
                     QColor(120, 136, 158));
    palette.setColor(QPalette::Base, QColor(26, 36, 53));
    palette.setColor(QPalette::AlternateBase, QColor(38, 51, 73));
    palette.setColor(QPalette::ToolTipBase, QColor(45, 59, 85));
    palette.setColor(QPalette::ToolTipText, QColor(230, 236, 245));
    palette.setColor(QPalette::Text, QColor(230, 236, 245));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor(120, 136, 158));
    palette.setColor(QPalette::Dark, QColor(24, 33, 48));
    palette.setColor(QPalette::Shadow, QColor(15, 22, 34));
    palette.setColor(QPalette::Button, QColor(33, 45, 67));
    palette.setColor(QPalette::ButtonText, QColor(230, 236, 245));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                     QColor(120, 136, 158));
    palette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    palette.setColor(QPalette::Link, QColor(81, 186, 229));
    palette.setColor(QPalette::Highlight, QColor(32, 84, 114));
    palette.setColor(QPalette::Disabled, QPalette::Highlight,
                     QColor(60, 72, 90));
    palette.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    palette.setColor(QPalette::Disabled, QPalette::HighlightedText,
                     QColor(120, 136, 158));
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

QString makeDarkStylesheet() {
  return QStringLiteral(R"__DARK_0__(
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
)__DARK_0__") +
         QStringLiteral(R"__DARK_1__(        border-radius: 5px;
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
        bo)__DARK_1__") +
         QStringLiteral(R"__DARK_2__(rder-radius: 5px;
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
      QToolButton#historyFilterButton {
        background-color: transparent;
        border: 1px solid transparent;
        border-radius: 4px;
        padding: 2px;
      }
      QToolButton#historyFilterButton:hover {
        background-color: rgba(255, 255, 255, 24);
        border: 1px solid rgba(255, 255, 255, 55);
      }
      QToolButton#historyFilterButton:checked {
        background-color: #384D59;
        border: 1px solid #718793;
      }
      QGroupBox#historyFilterWidget {
        background-color: #384D59;
        border: 1px solid #607481;
        border-radius: 6px;
        color: #F5F7F8;
        margin-top: 10px;
        padding: 12px 8px 8px 8px;
      }
      QGroupBox#historyFilterWidget::title {
        subcontrol-origin: margin;
        left: 10px;
        padding: 1px 8px;
        color: #FFFFFF;
        background-color: #384D59;
        border: 1px solid #607481;
        border-radius: 8px;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QLabel {
        color: #DCE4E8;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QLineEdit,
      QGroupBox#historyFilterWidget QDateEdit {
        background-color: #2A2A2A;
        color: #FFFFFF;
        border: 1px solid #607481;
        border-radius: 4px;
        padding: 4px 7px;
        min-height: 20px;
      }
      QGroupBox#historyFilterWidget QLineEdit:focus,
      QGroupBox#historyFilterWidget QDateEdit:focus {
        border: 1px solid #2A82DA;
      }
      QGroupBox#historyFilterWidget QLineEdit::placeholder {
        color: #9BAAB3;
      }
      QGroupBox#historyFilterWidget QDateEdit::drop-down {
        width: 24px;
        border-left: 1px solid #607481;
      }
      QGroupBox#historyFilterWidget QDateEdit::down-arrow {
        image: url(:/icons/history_filter_calendar_dark.svg);
        width: 12px;
        height: 12px;
      }
      QGroupBox#historyFilterWidget QDateEdit:disabled {
        background-color: #30383D;
        color: #8D999F;
        border-color: #4D5A61;
      }
      QGroupBox#historyFilterWidget QCheckBox {
        color: #DCE4E8;
        spacing: 6px;
        padding: 2px 0;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QCheckBox::indicator {
        width: 13px;
        height: 13px;
        border: 1px solid #8A9AA3;
        border-radius: 3px;
        background-color: transparent;
      }
      QGroupBox#historyFilterWidget QCheckBox::indicator:checked {
        border-color: #2A82DA;
        background-color: #2A82DA;
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
   )__DARK_2__") +
         QStringLiteral(R"__DARK_3__(   }
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
        border-ra)__DARK_3__") +
         QStringLiteral(R"__DARK_4__(dius: 5px;
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
      QToolButton#pushButtonPush,
      QToolButton#gitFlowDropdown{
        background-color: #424242;
        border: 1px solid #5A5A5A;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:hover,
      QPushButton)__DARK_4__") +
         QStringLiteral(R"__DARK_5__(#pushButtonStash:hover,
      QToolButton#pullFetchDropdown:hover,
      QToolButton#pushButtonPush:hover,
      QToolButton#gitFlowDropdown:hover{
        background-color: #4D4D4D;
        border: 1px solid #2A82DA;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:pressed,
      QPushButton#pushButtonStash:pressed,
      QToolButton#pullFetchDropdown:pressed,
      QToolButton#pushButtonPush:pressed,
      QToolButton#gitFlowDropdown:pressed{
        background-color: #2D4A66;
        border: 1px solid #2A82DA;
      }
      QToolButton#pullFetchDropdown::menu-button,
      QToolButton#pushButtonPush::menu-button,
      QToolButton#gitFlowDropdown::menu-button{
        border: none;
        background: transparent;
      }
      QToolButton#pullFetchDropdown::menu-indicator,
      QToolButton#pushButtonPush::menu-indicator,
      QToolButton#gitFlowDropdown::menu-indicator{
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
      QPushButton#generate)__DARK_5__") +
         QStringLiteral(R"__DARK_6__(Button,
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
     )__DARK_6__") +
         QStringLiteral(R"__DARK_7__(   background-color: #384D59;
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
      QPushButton#push)__DARK_7__") +
         QStringLiteral(R"__DARK_8__(ButtonBrowse:hover{
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
      QPushButton#pushButtonRebase{
        background-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonRebase:hover{
        background-color: #008400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClose,
      QPushButton#pushButtonSubmit{
        ba)__DARK_8__") +
         QStringLiteral(R"__DARK_9__(ckground-color: #006400;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClose:hover,
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
 )__DARK_9__") +
         QStringLiteral(R"__DARK_10__(       border: 1px solid gray;
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
       )__DARK_10__") +
         QStringLiteral(R"__DARK_11__( border: 1px solid gray;
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
 )__DARK_11__") +
         QStringLiteral(R"__DARK_12__(       border: 1px solid gray;
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
      QPushButton#pushBu)__DARK_12__") +
         QStringLiteral(R"__DARK_13__(ttonTextOverlayWidgetInputCancel{
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
      QGroupBox#groupBoxInfo,
      QGroupBox#stashContentInfoPanel{
        background-color: #202020;
        border: 1px solid #3A3A3A;
        border-radius: 10px;
        margin-top: 10px;
        padding: 18px 10px 10px 10px;
      }
      QGroupBox#groupBoxInfo::title,
      QGroupBox#stashContentInfoPanel::title {
        subcontrol-origin: margin;
        left: 10px;
        padding: 1px 8px;
        color: #F5F7F8;
        background-color: #202020;
        border: 1px solid #3A3A3A;
        border-radius: 8px;
        font-weight: bold;
      }
      QWidget#widgetPageDisplaySignatureSection{
        background: transparent;
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
      QToolButton#toolButtonPag)__DARK_13__") +
         QStringLiteral(R"__DARK_14__(eDisplaySignatureToggle {
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
      Q)__DARK_14__") +
         QStringLiteral(
             R"__DARK_15__(TabBar#tabWidgetIndexTabBar::tab:selected {
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
        border: 1px solid #6D6E6E)__DARK_15__") +
         QStringLiteral(R"__DARK_16__(;
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
 )__DARK_16__") +
         QStringLiteral(R"__DARK_17__(       border: 1px solid gray;
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
      GuiManager--Widgets--TitleBarButton#titleBarSettingsButton,
      GuiManager--Widgets--TitleBarButton#titleBarMinimizeButton,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton {
        background-color: #353535;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarSettingsButton:hover,
      GuiManager--)__DARK_17__") +
         QStringLiteral(
             R"__DARK_18__(Widgets--TitleBarButton#titleBarMinimizeButton:hover,
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
      GuiManager--Containers--SettingsContainer QT)__DARK_18__") +
         QStringLiteral(R"__DARK_19__(reeView{
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
      QS)__DARK_19__") +
         QStringLiteral(R"__DARK_20__(crollBar::handle:vertical:hover {
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
      )__DARK_20__") +
         QStringLiteral(
             R"__DARK_21__(QTabWidget#tabWidgetStashFiles QTabBar::tab:selected {
        background-color: #384D59;
        border: 2px solid #6D6E6E;
        font-weight: bold;
        color: white;
      }
      QTabWidget#tabWidgetStashFiles QTabBar::tab:hover {
        background-color: #384D59;
        color: white;
      }
      QPushButton#pushButtonHistorySearchRepository[searching="true"] {
        background-color: #B00020;
        border: 1px solid #7F0018;
        border-radius: 4px;
        padding: 4px 12px;
        color: white;
      }
      QPushButton#pushButtonHistorySearchRepository[searching="true"]:hover {
        background-color: #D32F2F;
      }
    )__DARK_21__");
}

QString makeLightStylesheet() {
  return QStringLiteral(R"__LIGHT_0__(

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
        border: 2px s)__LIGHT_0__") +
         QStringLiteral(R"__LIGHT_1__(olid #0078d7;
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
        border: )__LIGHT_1__") +
         QStringLiteral(R"__LIGHT_2__(1px solid #B8C0C8;
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
      QToolButton#historyFilterButton {
        background-color: transparent;
        border: 1px solid transparent;
        border-radius: 4px;
        padding: 2px;
      }
      QToolButton#historyFilterButton:hover {
        background-color: rgba(0, 0, 0, 12);
        border: 1px solid rgba(0, 0, 0, 35);
      }
      QToolButton#historyFilterButton:checked {
        background-color: #E7EEF4;
        border: 1px solid #B8C0C8;
      }
      QGroupBox#historyFilterWidget {
        background-color: #F3F6F8;
        border: 1px solid #B8C0C8;
        border-radius: 6px;
        color: #202020;
        margin-top: 10px;
        padding: 12px 8px 8px 8px;
      }
      QGroupBox#historyFilterWidget::title {
        subcontrol-origin: margin;
        left: 10px;
        padding: 1px 8px;
        color: #202020;
        background-color: #F3F6F8;
        border: 1px solid #B8C0C8;
        border-radius: 8px;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QLabel {
        color: #3A4248;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QLineEdit,
      QGroupBox#historyFilterWidget QDateEdit {
        background-color: #FFFFFF;
        color: #202020;
        border: 1px solid #B8C0C8;
        border-radius: 4px;
        padding: 4px 7px;
        min-height: 20px;
      }
      QGroupBox#historyFilterWidget QLineEdit:focus,
      QGroupBox#historyFilterWidget QDateEdit:focus {
        border: 1px solid #0078D7;
      }
      QGroupBox#historyFilterWidget QLineEdit::placeholder {
        color: #666666;
      }
      QGroupBox#historyFilterWidget QDateEdit::drop-down {
        width: 24px;
        border-left: 1px solid #B8C0C8;
      }
      QGroupBox#historyFilterWidget QDateEdit::down-arrow {
        image: url(:/icons/history_filter_calendar_light.svg);
        width: 12px;
        height: 12px;
      }
      QGroupBox#historyFilterWidget QDateEdit:disabled {
        background-color: #E7EBEE;
        color: #8A9298;
        border-color: #C7CED3;
      }
      QGroupBox#historyFilterWidget QCheckBox {
        color: #3A4248;
        spacing: 6px;
        padding: 2px 0;
        font-weight: bold;
      }
      QGroupBox#historyFilterWidget QCheckBox::indicator {
        width: 13px;
        height: 13px;
        border: 1px solid #8A9298;
        border-radius: 3px;
        background-color: #FFFFFF;
      }
      QGroupBox#historyFilterWidget QCheckBox::indicator:checked {
        border-color: #0078D7;
        background-color: #0078D7;
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
        border: 1px solid palett)__LIGHT_2__") +
         QStringLiteral(R"__LIGHT_3__(e(button-text);
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
      QTableView#tableViewRecent,
      QTableView#tableViewWorkspaces,
      QTableView#tableViewIssueTrackerConnections {
        color: palette(window-text);
        background-color: palette(window);
        alternate-background-color: palette(window);
      }
      QListView::item, QTreeView::item, QTableVi)__LIGHT_3__") +
         QStringLiteral(R"__LIGHT_4__(ew::item {
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
        padding: 0px 5px;
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
      QPushButton#commit)__LIGHT_4__") +
         QStringLiteral(R"__LIGHT_5__(AndMergeIndexTreeUnstageAllButton{
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
        paddi)__LIGHT_5__") +
         QStringLiteral(R"__LIGHT_6__(ng: 5px;
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
      QToolButton#pushButtonPush,
      QToolButton#gitFlowDropdown{
        background-color: #FFFFFF;
        border: 1px solid #C8C8C8;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:hover,
      QPushButton#pushButtonStash:hover,
      QToolButton#pullFetchDropdown:hover,
      QToolButton#pushButtonPush:hover,
      QToolButton#gitFlowDropdown:hover{
        background-color: #E5F1FB;
        border: 1px solid #0078D7;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonPop:pressed,
      QPushButton#pushButtonStash:pressed,
      QToolButton#pullFetchDropdown:pressed,
      QToolButton#pushButtonPush:pressed,
      QToolButton#gitFlowDropdown:pressed{
        background-color: #CCE4F7;
        border: 1px solid #0078D7;
      }
      QToolButton#pullFetchDropdown::menu-button,
      QToolButton#pushButtonPush::menu-button,
      QToolButton#gitFlowDropdown::menu-button{
        border: none;)__LIGHT_6__") +
         QStringLiteral(R"__LIGHT_7__(
        background: transparent;
      }
      QToolButton#pullFetchDropdown::menu-indicator,
      QToolButton#pushButtonPush::menu-indicator,
      QToolButton#gitFlowDropdown::menu-indicator{
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
   )__LIGHT_7__") +
         QStringLiteral(R"__LIGHT_8__(   }
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
  )__LIGHT_8__") +
         QStringLiteral(R"__LIGHT_9__(      background-color: #E7EEF4;
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
      QPushBut)__LIGHT_9__") +
         QStringLiteral(R"__LIGHT_10__(ton#pushButtonClone:hover{
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
      QPushButton#pushButtonRebase{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonRebase:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClose,
      QPushButton#pushButtonSubmit{
        background-color: #E7F5E7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonClose:hover,
      QPushButton#pushButtonSubmit:hover{
        background-color: #D7EFD7;
        border: 1px solid gray;
        border-radius: 5px;
        padding: 5px;
      }
      QPushButton#pushButtonMerge{
        background-color: #E7F5E7)__LIGHT_10__") +
         QStringLiteral(R"__LIGHT_11__(;
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
        bord)__LIGHT_11__") +
         QStringLiteral(R"__LIGHT_12__(er: 1px solid gray;
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
        border: 1)__LIGHT_12__") +
         QStringLiteral(R"__LIGHT_13__(px solid gray;
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
        border:)__LIGHT_13__") +
         QStringLiteral(R"__LIGHT_14__( 1px solid gray;
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
      QPushButton#pushButtonTextOverla)__LIGHT_14__") +
         QStringLiteral(R"__LIGHT_15__(yWidgetInputCancel{
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
      QGroupBox#groupBoxInfo,
      QGroupBox#stashContentInfoPanel{
        background-color: #FFFFFF;
        border: 1px solid #D8DEE5;
        border-radius: 10px;
        margin-top: 10px;
        padding: 18px 10px 10px 10px;
      }
      QGroupBox#groupBoxInfo::title,
      QGroupBox#stashContentInfoPanel::title {
        subcontrol-origin: margin;
        left: 10px;
        padding: 1px 8px;
        color: #202020;
        background-color: #FFFFFF;
        border: 1px solid #D8DEE5;
        border-radius: 8px;
        font-weight: bold;
      }
      QWidget#widgetPageDisplaySignatureSection{
        background: transparent;
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
        padding: 0)__LIGHT_15__") +
         QStringLiteral(R"__LIGHT_16__(px;
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
        border-radi)__LIGHT_16__") +
         QStringLiteral(R"__LIGHT_17__(us: 8px;
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
        margin:)__LIGHT_17__") +
         QStringLiteral(R"__LIGHT_18__( 0px;
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
      QTabBar#tabWidgetPageDisplay)__LIGHT_18__") +
         QStringLiteral(R"__LIGHT_19__(FilesTabBar::tab:hover {
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
      GuiManager--Widgets--TitleBarButton#titleBarSettingsButton {
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarMinimize)__LIGHT_19__") +
         QStringLiteral(R"__LIGHT_20__(Button,
      GuiManager--Widgets--TitleBarButton#titleBarMaximizeButton {
        background-color: transparent;
      }
      GuiManager--Widgets--TitleBarButton#titleBarProfileButton:hover,
      GuiManager--Widgets--TitleBarButton#titleBarSettingsButton:hover,
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
        b)__LIGHT_20__") +
         QStringLiteral(R"__LIGHT_21__(order:1px solid #E7EEF4;
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
        )__LIGHT_21__") +
         QStringLiteral(R"__LIGHT_22__(width: 14px;
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
        background: transparent;)__LIGHT_22__") +
         QStringLiteral(R"__LIGHT_23__(
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
      QPushButton#pushButtonHistorySearchRepository[searching="true"] {
        background-color: #C62828;
        border: 1px solid #8E0000;
        border-radius: 4px;
        padding: 4px 12px;
        color: white;
      }
      QPushButton#pushButtonHistorySearchRepository[searching="true"]:hover {
        background-color: #E53935;
      }

    )__LIGHT_23__");
}

// The purple and blue themes share the dark theme's layout rules and only swap
// its colors, so they stay in sync when the dark stylesheet changes.
QString makeDarkDerivedStylesheet(
    const QList<QPair<QString, QString>> &colorMap, const QString &grayColor) {
  QString stylesheet = makeDarkStylesheet();
  for (const auto &[from, to] : colorMap) {
    const QRegularExpression pattern(
        QStringLiteral("%1(?![0-9A-Fa-f])").arg(from),
        QRegularExpression::CaseInsensitiveOption);
    stylesheet.replace(pattern, to);
  }

  static const QRegularExpression grayPattern(
      QStringLiteral("\\bgr[ae]y\\b"));
  stylesheet.replace(grayPattern, grayColor);

  return stylesheet;
}

QString makePurpleStylesheet() {
  static const QList<QPair<QString, QString>> colorMap = {
      // Neutral surfaces: the dark theme's greys, tinted toward purple-grey
      {QStringLiteral("#202020"), QStringLiteral("#26222E")},
      {QStringLiteral("#282828"), QStringLiteral("#2B2735")},
      {QStringLiteral("#2A2A2A"), QStringLiteral("#2B2735")},
      {QStringLiteral("#30383D"), QStringLiteral("#352F40")},
      {QStringLiteral("#353535"), QStringLiteral("#332F3F")},
      {QStringLiteral("#3A3A3A"), QStringLiteral("#3B3646")},
      {QStringLiteral("#3E3E42"), QStringLiteral("#3E3A48")},
      {QStringLiteral("#424242"), QStringLiteral("#433E4D")},
      {QStringLiteral("#4D4D4D"), QStringLiteral("#4A4556")},
      {QStringLiteral("#5A5A5A"), QStringLiteral("#5A5566")},
      {QStringLiteral("#5E5E5E"), QStringLiteral("#5E5970")},
      {QStringLiteral("#686868"), QStringLiteral("#686474")},
      {QStringLiteral("#6D6E6E"), QStringLiteral("#6E6A7A")},
      {QStringLiteral("#9E9E9E"), QStringLiteral("#9C98A8")},
      {QStringLiteral("#C0C0C0"), QStringLiteral("#C8C5D0")},
      {QStringLiteral("#012456"), QStringLiteral("#1F1C27")},
      // Slate selection and tooltip tones, as seen in the reference
      {QStringLiteral("#384D59"), QStringLiteral("#374659")},
      {QStringLiteral("#4A5A66"), QStringLiteral("#3F4D63")},
      {QStringLiteral("#4A6070"), QStringLiteral("#41526A")},
      {QStringLiteral("#4D5A61"), QStringLiteral("#4A5566")},
      {QStringLiteral("#607481"), QStringLiteral("#5F6F85")},
      {QStringLiteral("#718793"), QStringLiteral("#6F7F95")},
      {QStringLiteral("#8A9AA3"), QStringLiteral("#8A97AB")},
      {QStringLiteral("#8D999F"), QStringLiteral("#8D98AA")},
      {QStringLiteral("#9BAAB3"), QStringLiteral("#9BA6B8")},
      {QStringLiteral("#DCE4E8"), QStringLiteral("#DCE2EC")},
      {QStringLiteral("#F5F7F8"), QStringLiteral("#F4F5F8")},
  };
  return makeDarkDerivedStylesheet(colorMap, QStringLiteral("#7D7889"));
}

QString makeBlueStylesheet() {
  static const QList<QPair<QString, QString>> colorMap = {
      {QStringLiteral("#202020"), QStringLiteral("#1A2435")},
      {QStringLiteral("#282828"), QStringLiteral("#1E2A3D")},
      {QStringLiteral("#2A2A2A"), QStringLiteral("#1F2B3E")},
      {QStringLiteral("#30383D"), QStringLiteral("#243247")},
      {QStringLiteral("#353535"), QStringLiteral("#212D43")},
      {QStringLiteral("#3A3A3A"), QStringLiteral("#2A3750")},
      {QStringLiteral("#3E3E42"), QStringLiteral("#2D3B55")},
      {QStringLiteral("#424242"), QStringLiteral("#334260")},
      {QStringLiteral("#4D4D4D"), QStringLiteral("#3D4D66")},
      {QStringLiteral("#5A5A5A"), QStringLiteral("#4A5C78")},
      {QStringLiteral("#5E5E5E"), QStringLiteral("#4F6180")},
      {QStringLiteral("#686868"), QStringLiteral("#5A6E8C")},
      {QStringLiteral("#6D6E6E"), QStringLiteral("#5F7594")},
      {QStringLiteral("#9E9E9E"), QStringLiteral("#8FA3BF")},
      {QStringLiteral("#C0C0C0"), QStringLiteral("#C5D3E6")},
      {QStringLiteral("#012456"), QStringLiteral("#0F1C2E")},
      {QStringLiteral("#384D59"), QStringLiteral("#205472")},
      {QStringLiteral("#4A5A66"), QStringLiteral("#245A7A")},
      {QStringLiteral("#4A6070"), QStringLiteral("#2A6585")},
      {QStringLiteral("#4D5A61"), QStringLiteral("#35566E")},
      {QStringLiteral("#607481"), QStringLiteral("#4E7593")},
      {QStringLiteral("#718793"), QStringLiteral("#6A8AA3")},
      {QStringLiteral("#8A9AA3"), QStringLiteral("#8AA6BA")},
      {QStringLiteral("#8D999F"), QStringLiteral("#8FA3B5")},
      {QStringLiteral("#9BAAB3"), QStringLiteral("#A4B9C8")},
      {QStringLiteral("#DCE4E8"), QStringLiteral("#DCE8F0")},
      {QStringLiteral("#F5F7F8"), QStringLiteral("#F2F7FA")},
      {QStringLiteral("#0078D7"), QStringLiteral("#3FA9E0")},
      {QStringLiteral("#2A82DA"), QStringLiteral("#3FA9E0")},
      {QStringLiteral("#53A0ED"), QStringLiteral("#7CC8F0")},
      {QStringLiteral("#2D4A66"), QStringLiteral("#1F4A66")},
  };
  return makeDarkDerivedStylesheet(colorMap, QStringLiteral("#5F7594"));
}

const QString SOURCERER_DARK_STYLESHEET = makeDarkStylesheet();
const QString SOURCERER_LIGHT_STYLESHEET = makeLightStylesheet();
const QString SOURCERER_PURPLE_STYLESHEET = makePurpleStylesheet();
const QString SOURCERER_BLUE_STYLESHEET = makeBlueStylesheet();

const QMap<StylesheetKey, QString> &stylesheets() {
  static const QMap<StylesheetKey, QString> map = []() {
    QMap<StylesheetKey, QString> stylesheets;
    stylesheets.insert(StylesheetKey::SourcererLight,
                       SOURCERER_LIGHT_STYLESHEET);
    stylesheets.insert(StylesheetKey::SourcererDark, SOURCERER_DARK_STYLESHEET);
    stylesheets.insert(StylesheetKey::SourcererPurple,
                       SOURCERER_PURPLE_STYLESHEET);
    stylesheets.insert(StylesheetKey::SourcererBlue,
                       SOURCERER_BLUE_STYLESHEET);
    return stylesheets;
  }();

  return map;
}

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

  const auto &styleMap = stylesheets();
  const auto resolvedKey = styleMap.contains(key) ? key : CURRENT_STYLESHEET;

  // QMap::operator[] const returns by value, look up the stored element to
  // return a reference that outlives this call.
  const auto it = styleMap.constFind(resolvedKey);
  if (it == styleMap.constEnd()) {
    static const QString empty;
    return empty;
  }

  return it.value();
}

StylesheetKey StylesheetManager::currentStylesheet() {
  return CURRENT_STYLESHEET;
}

void StylesheetManager::setCurrentStylesheet(StylesheetKey key) {
  LOG_PERF(0, "Enter");

  if (stylesheets().contains(key)) {
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
  case StylesheetKey::SourcererPurple:
    return SOURCERER_PURPLE_THEME_NAME;
  case StylesheetKey::SourcererBlue:
    return SOURCERER_BLUE_THEME_NAME;
  default:
    return SOURCERER_DARK_THEME_NAME;
  }
}

StylesheetKey StylesheetManager::stylesheetKeyFromName(const QString &name) {
  if (name == SOURCERER_LIGHT_THEME_NAME)
    return StylesheetKey::SourcererLight;
  if (name == SOURCERER_DARK_THEME_NAME)
    return StylesheetKey::SourcererDark;
  if (name == SOURCERER_PURPLE_THEME_NAME)
    return StylesheetKey::SourcererPurple;
  if (name == SOURCERER_BLUE_THEME_NAME)
    return StylesheetKey::SourcererBlue;
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
  case StylesheetKey::SourcererPurple:
    return QColor("#574A72");
  case StylesheetKey::SourcererBlue:
    return QColor("#2D4A66");
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

QVector<QColor> StylesheetManager::graphLaneColors() {
  switch (currentStylesheet()) {
  case StylesheetKey::SourcererLight:
    return {QColor("#1B7F3B"), QColor("#1F6FD1"), QColor("#C2410C"),
            QColor("#8E44AD"), QColor("#B58900"), QColor("#0F8B8D"),
            QColor("#C0392B"), QColor("#5D6D7E"), QColor("#D6336C"),
            QColor("#6B8E23")};
  case StylesheetKey::SourcererDark:
  default:
    return {QColor("#4CC470"), QColor("#5AA9FF"), QColor("#FF9F5A"),
            QColor("#C792EA"), QColor("#E5C454"), QColor("#4DD0D2"),
            QColor("#FF6B6B"), QColor("#9FB3C8"), QColor("#F783AC"),
            QColor("#A9D86E")};
  }
}

QString StylesheetManager::issueTrackerToolbarStyleSheet() {
  switch (currentStylesheet()) {
  case StylesheetKey::SourcererLight:
    return QStringLiteral(
        "QWidget#issueTrackerToolbar {"
        "  background: palette(window);"
        "  border: none;"
        "  border-radius: 0px;"
        "}"
        "QWidget#issueTrackerToolbarHandle {"
        "  min-height: 5px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 rgba(255,255,255,150),"
        "    stop:0.5 rgba(224,224,224,180),"
        "    stop:1 rgba(176,176,176,150));"
        "  border-top: 1px solid rgba(255,255,255,180);"
        "  border-bottom: 1px solid rgba(120,120,120,120);"
        "  border-radius: 2px;"
        "}"
        "QWidget#issueTrackerToolbar QLabel { color: #202020; }"
        "QWidget#issueTrackerToolbar QToolButton {"
        "  color: #202020;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "  margin: 0px;"
        "  min-width: 22px;"
        "  min-height: 22px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:hover {"
        "  background: rgba(0, 0, 0, 6);"
        "  border-radius: 4px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:pressed {"
        "  background: rgba(0, 0, 0, 12);"
        "  border-radius: 4px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:disabled {"
        "  color: rgba(0, 0, 0, 90);"
        "}");
  case StylesheetKey::SourcererDark:
  default:
    return QStringLiteral(
        "QWidget#issueTrackerToolbar {"
        "  background: palette(window);"
        "  border: none;"
        "  border-radius: 0px;"
        "}"
        "QWidget#issueTrackerToolbarHandle {"
        "  min-height: 5px;"
        "  background: qlineargradient(x1:0, y1:0, x2:0, y2:1,"
        "    stop:0 rgba(255,255,255,30),"
        "    stop:0.5 rgba(255,255,255,12),"
        "    stop:1 rgba(0,0,0,45));"
        "  border-top: 1px solid rgba(255,255,255,40);"
        "  border-bottom: 1px solid rgba(0,0,0,60);"
        "  border-radius: 2px;"
        "}"
        "QWidget#issueTrackerToolbar QLabel { color: white; }"
        "QWidget#issueTrackerToolbar QToolButton {"
        "  color: white;"
        "  background: transparent;"
        "  border: none;"
        "  padding: 0px;"
        "  margin: 0px;"
        "  min-width: 22px;"
        "  min-height: 22px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:hover {"
        "  background: rgba(255,255,255,35);"
        "  border-radius: 4px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:pressed {"
        "  background: rgba(255,255,255,60);"
        "  border-radius: 4px;"
        "}"
        "QWidget#issueTrackerToolbar QToolButton:disabled {"
        "  color: rgba(255,255,255,90);"
        "}");
  }
}

namespace {

QPixmap renderSvgPathToPixmap(const QString &path, const QColor &color) {
  QSvgRenderer renderer(path);
  if (!renderer.isValid()) {
    return QPixmap();
  }

  const QSize defaultSize = renderer.defaultSize();
  const QSize size = defaultSize.isValid() && !defaultSize.isEmpty()
                         ? defaultSize
                         : QSize(24, 24);
  QPixmap pixmap(size);
  pixmap.fill(Qt::transparent);

  QPainter painter(&pixmap);
  renderer.render(&painter);
  painter.end();

  return StylesheetManager::StylesheetManager::recolorPixmap(pixmap, color);
}

QPixmap loadPixmap(const QString &path) {
  if (path.endsWith(QStringLiteral(".svg"), Qt::CaseInsensitive)) {
    QSvgRenderer renderer(path);
    if (!renderer.isValid()) {
      return {};
    }

    const QSize defaultSize = renderer.defaultSize();
    const QSize size = defaultSize.isValid() && !defaultSize.isEmpty()
                           ? defaultSize
                           : QSize(24, 24);
    QPixmap pixmap(size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    renderer.render(&painter);
    return pixmap;
  }

  return QPixmap(path);
}

QIcon issueTrackerToolbarIcon(const QString &path,
                              const QString &disabledPath) {
  const QColor iconColor =
      StylesheetManager::StylesheetManager::menuBarButtonIconColor();

  QIcon icon;
  const QPixmap source = renderSvgPathToPixmap(path, iconColor);
  if (source.isNull()) {
    return icon;
  }

  icon.addPixmap(source, QIcon::Normal, QIcon::Off);

  const QPixmap disabledSource = renderSvgPathToPixmap(disabledPath, iconColor);
  if (!disabledSource.isNull()) {
    icon.addPixmap(disabledSource, QIcon::Disabled, QIcon::Off);
  }

  return icon;
}

} // namespace

QPixmap StylesheetManager::renderSvgToPixmap(const QString &path,
                                             const QColor &color) {
  return renderSvgPathToPixmap(path, color);
}

QIcon StylesheetManager::issueTrackerToolbarBackIcon() {
  return issueTrackerToolbarIcon(
      QStringLiteral(":/icons/issue_tracker_toolbar_back.svg"),
      QStringLiteral(":/icons/issue_tracker_toolbar_back_disabled.svg"));
}

QIcon StylesheetManager::issueTrackerToolbarForwardIcon() {
  return issueTrackerToolbarIcon(
      QStringLiteral(":/icons/issue_tracker_toolbar_forward.svg"),
      QStringLiteral(":/icons/issue_tracker_toolbar_forward_disabled.svg"));
}

QIcon StylesheetManager::issueTrackerToolbarRefreshIcon() {
  return issueTrackerToolbarIcon(
      QStringLiteral(":/icons/issue_tracker_toolbar_refresh.svg"),
      QStringLiteral(":/icons/issue_tracker_toolbar_refresh_disabled.svg"));
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
    if (auto *button = qobject_cast<QAbstractButton *>(widget)) {
      const QVariant iconSourceVariant = button->property("iconSource");
      if (!iconSourceVariant.isValid())
        continue;

      const QString iconSource = iconSourceVariant.toString();
      if (iconSource.isEmpty())
        continue;

      button->setIcon(recolorPixmap(loadPixmap(iconSource), iconColor));
      continue;
    }

    if (auto *menu = qobject_cast<QMenu *>(widget)) {
      const auto menuActions = menu->actions();
      for (QAction *action : menuActions) {
        if (!action)
          continue;

        const QVariant iconSourceVariant = action->property("iconSource");
        if (!iconSourceVariant.isValid())
          continue;

        const QString iconSource = iconSourceVariant.toString();
        if (iconSource.isEmpty())
          continue;

        action->setIcon(recolorPixmap(loadPixmap(iconSource), iconColor));
      }
    }
  }
}

void StylesheetManager::applyStylesheet(QApplication *app) {
  LOG_PERF(0, "Enter");

  const StylesheetKey key = CURRENT_STYLESHEET;
  if (app) {
    applyPalette(app, key);
    app->setStyleSheet(stylesheets().value(key));
    refreshMenuBarButtonIcons();
  }

  LOG_PERF(0, "Exit");
}

} // namespace StylesheetManager
