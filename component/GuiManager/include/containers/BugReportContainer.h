#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/SpinnerOverlayWidget.h"

#include <ConfigManager.h>
#include <Dispatcher.h>

#include <QEnableSharedFromThis>
#include <QHttpMultiPart>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QWidget>

namespace Ui {
class BugReportContainer;
}

namespace GuiManager::Containers {

using namespace Widgets;

class BugReportContainer final
    : public ContainerWidget,
      public QEnableSharedFromThis<BugReportContainer> {
  Q_OBJECT

  typedef QSharedPointer<BugReportContainer> SharedPtr;
  typedef QWeakPointer<BugReportContainer> WeakPtr;

public:
  explicit BugReportContainer(QWidget *parent = nullptr);
  ~BugReportContainer() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  void selected() override;
  void deselected() override;

  QString path() const override { return {}; }

signals:
  void close();

private slots:
  void onSubmitClicked();
  void onCancelClicked();
  void onInputChanged();
  void onUploadProgress(qint64 bytesSent, qint64 bytesTotal);
  void onUploadFinished();
  void onUploadError(QNetworkReply::NetworkError code);

private:
  bool validateInputs() const;
  QString createLogsZipFile();
  void submitBugReport();
  void onZipCreationFinished(const QString &zipPath);
  void showSpinnerOverlay(bool show = true);
  void updateProgressLabel(int percentage);

private:
  Ui::BugReportContainer *ui;
  ConfigManager::ConfigManager &_configManager;
  QNetworkAccessManager *_networkManager;
  QNetworkReply *_currentReply;
  SpinnerOverlayWidget *_spinnerOverlay;
  QString _tempZipFilePath;
  Util::Dispatcher _dispatcher;
};

} // namespace GuiManager::Containers
