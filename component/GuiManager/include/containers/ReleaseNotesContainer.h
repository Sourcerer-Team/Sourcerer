#pragma once

#include "widgets/ContainerWidget.h"

#include <QEnableSharedFromThis>
#include <QString>
#include <QWidget>

namespace Ui {
class ReleaseNotesContainer;
}

namespace GuiManager::Containers {

using namespace Widgets;

class ReleaseNotesContainer final
    : public ContainerWidget,
      public QEnableSharedFromThis<ReleaseNotesContainer> {
  Q_OBJECT

  typedef QSharedPointer<ReleaseNotesContainer> SharedPtr;
  typedef QWeakPointer<ReleaseNotesContainer> WeakPtr;

public:
  // 'markdown' is the release notes text, 'title' is shown above it.
  explicit ReleaseNotesContainer(const QString &title, const QString &markdown,
                                 QWidget *parent = nullptr);
  ~ReleaseNotesContainer() override;

  SharedPtr getSelf() { return sharedFromThis(); }

  QString path() const override { return {}; }

signals:
  void close();

private slots:
  void onCloseClicked();

private:
  Ui::ReleaseNotesContainer *ui;
};

} // namespace GuiManager::Containers
