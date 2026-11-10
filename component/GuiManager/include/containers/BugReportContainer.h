// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "widgets/ContainerWidget.h"
#include "widgets/SpinnerOverlayWidget.h"

#include <CloudRepo.h>
#include <ConfigManager.h>
#include <Dispatcher.h>

#include <QEnableSharedFromThis>
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
  void onUploadFailed(const QString &error);

private:
  bool validateInputs() const;
  QString createLogsZipFile(const QString &title, const QString &description);
  void submitBugReport();
  void onZipCreationFinished(const WeakPtr &guard, const QString &zipPath);
  void showSpinnerOverlay(bool show = true);
  void updateProgressLabel(int percentage);
  void restoreSubmissionUi();

private:
  Ui::BugReportContainer *ui;
  ConfigManager::ConfigManager &_configManager;
  SpinnerOverlayWidget *_spinnerOverlay;
  QString _tempZipFilePath;
  Util::Dispatcher _dispatcher;
};

} // namespace GuiManager::Containers
