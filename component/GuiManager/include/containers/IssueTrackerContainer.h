#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/ToastHostWidget.h"

#include <ConfigManager.h>

#include <QString>
#include <QUrl>
#include <QWidget>

namespace Ui {
class IssueTrackerContainer;
}

#if __has_include(<QtWebEngineWidgets/QWebEngineView>)
#include <QtWebEngineWidgets/QWebEngineView>
#define GUI_MANAGER_HAS_WEBENGINE 1
#else
#include <QtWidgets/QTextBrowser>
#define GUI_MANAGER_HAS_WEBENGINE 0
#endif

namespace GuiManager::Containers {

using namespace Widgets;

class IssueTrackerContainer final : public ContainerWidget {
  Q_OBJECT

public:
  explicit IssueTrackerContainer(QString serviceType, QString repositoryName,
                                 QWidget *parent = nullptr);
  ~IssueTrackerContainer() override;

  void init() override;
  void deselected() override;
  void selected() override;

  QString serviceType() const { return _serviceType; }
  QString repositoryName() const { return _repositoryName; }
  QString path() const override { return QString(); }

private:
  void updateIssueUrl();
  void loadIssuePage();
  void showUnsupportedMessage(const QString &message);

private:
  ConfigManager::ConfigManager &_configManager;
  Widgets::ToastHostWidget &_toastHost;
  Ui::IssueTrackerContainer *ui;
  bool _selected;
  QString _serviceType;
  QString _repositoryName;
  QUrl _issuesUrl;
#if GUI_MANAGER_HAS_WEBENGINE
  QWebEngineView *_browserView;
#else
  QTextBrowser *_browserView;
#endif
};

} // namespace GuiManager::Containers
