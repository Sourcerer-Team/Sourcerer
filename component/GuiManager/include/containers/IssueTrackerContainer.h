// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>

#include <QEvent>
#include <QLineEdit>
#include <QMouseEvent>
#include <QPoint>
#include <QResizeEvent>
#include <QString>
#include <QToolButton>
#include <QUrl>
#include <QWidget>

namespace Ui {
class IssueTrackerContainer;
}

// Set by CMake only when Qt6::WebEngineWidgets is found and linked. Do not
// detect via __has_include: the headers may be installed without the
// WebEngine CMake package being available, which breaks compile and link.
#ifndef GUI_MANAGER_HAS_WEBENGINE
#define GUI_MANAGER_HAS_WEBENGINE 0
#endif

#if GUI_MANAGER_HAS_WEBENGINE
#include <QtWebEngineCore/QWebEngineProfile>
#include <QtWebEngineWidgets/QWebEngineView>
#else
#include <QtWidgets/QTextBrowser>
#endif

namespace GuiManager::Containers {

using namespace Widgets;

class IssueTrackerContainer final : public ContainerWidget {
  Q_OBJECT

public:
  // startUrl replaces the repository issues page as the first page, e.g. a
  // GitHub pull request page.
  explicit IssueTrackerContainer(QString serviceType, QString repositoryName,
                                 QUrl startUrl = QUrl(),
                                 QWidget *parent = nullptr);
  ~IssueTrackerContainer() override;

  void init() override;
  void deselected() override;
  void selected() override;

  QString serviceType() const { return _serviceType; }
  QString repositoryName() const { return _repositoryName; }
  QUrl startUrl() const { return _startUrl; }
  QString path() const override { return QString(); }

protected:
  void resizeEvent(QResizeEvent *event) override;
  void changeEvent(QEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

signals:
  void close();

private:
  void updateIssueUrl();
  void loadIssuePage();
#if GUI_MANAGER_HAS_WEBENGINE
  void loadAfterGitHubLoginCheck();
#endif
  void showUnsupportedMessage(const QString &message);
  void showLoadFailedMessage(const QString &message);
  void openIssueTrackerExternallyAndClose();
  void updateNavigationState();
  void buildToolbar();
  void applyToolbarTheme();
  void restoreToolbarPosition();
  void moveToolbarTo(const QPoint &relativePosition, bool persist);
  QPoint defaultToolbarPosition() const;
  QPoint loadToolbarPosition() const;
  QPoint clampToolbarPosition(const QPoint &relativePosition) const;
  void persistToolbarPosition(const QPoint &relativePosition);

private:
  ConfigManager::ConfigManager &_configManager;
  Widgets::ToastHostWidget &_toastHost;
  Ui::IssueTrackerContainer *ui;
  bool _selected;
  QString _serviceType;
  QString _repositoryName;
  QString _serviceHost;
  QUrl _startUrl;
  QUrl _issuesUrl;
  QWidget *_toolbarWidget;
  QWidget *_toolbarHandle;
  QToolButton *_backButton;
  QToolButton *_forwardButton;
  QToolButton *_refreshButton;
  bool _draggingToolbar;
  bool _externalBrowserOpened;
  bool _pageLoaded;
  QPoint _toolbarDragOffset;
  QPoint _toolbarRelativePosition;
#if GUI_MANAGER_HAS_WEBENGINE
  QWebEngineProfile *_webEngineProfile;
  QWebEngineView *_browserView;
#else
  QTextBrowser *_browserView;
#endif
};

} // namespace GuiManager::Containers
