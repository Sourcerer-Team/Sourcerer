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

  // Tree structure helpers
  [[nodiscard]] bool isRemoteItem(const QModelIndex &index) const;
  [[nodiscard]] bool isTargetItem(const QModelIndex &index) const;
  [[nodiscard]] QString getRemoteName(const QModelIndex &index) const;
  [[nodiscard]] GitRepo::RemoteTarget getTargetInfo(const QModelIndex &index) const;
  [[nodiscard]] QModelIndex getRemoteIndex(const QString &remoteName) const;

  void setActiveIndex(int index);
  void updateModel(const QVector<GitRepo::RemoteInfo> &remotes,
                   int activeIndex = -1);

private:
  int _prevIndex;
  
  // Cache for quick lookups
  QMap<QString, QStandardItem *> _remoteItems;  // remoteName -> item pointer
};

} // namespace GuiManager::Models
