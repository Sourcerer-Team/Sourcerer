#pragma once

#include <GitRepo.h>

#include <QStandardItemModel>

namespace GuiManager::Models {

class RemotesModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit RemotesModel(QObject *parent = nullptr);
  ~RemotesModel() override;

  [[nodiscard]] virtual QString iconActivePath() const;
  [[nodiscard]] virtual QString iconPath() const;

  [[nodiscard]] int indexForRemote(const QString &remoteName) const;

  [[nodiscard]] QString remoteForIndex(int index) const;

  void setActiveIndex(int index);
  void updateModel(const QVector<GitRepo::RemoteInfo> &remotes,
                   int activeIndex = -1);

private:
  int _prevIndex;
};

} // namespace GuiManager::Models
