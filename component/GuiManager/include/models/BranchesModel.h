
#pragma once

#include <GitRepo.h>

#include <QObject>
#include <QStandardItemModel>

namespace GuiManager::Models {

class BranchesModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit BranchesModel(QObject *parent = nullptr);
  ~BranchesModel() override;

  [[nodiscard]] virtual QString iconActivePath() const;
  [[nodiscard]] virtual QString iconPath() const;

  Qt::ItemFlags flags(const QModelIndex &index) const override;

  Qt::DropActions supportedDropActions() const override;

  QStringList mimeTypes() const override;

  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;

  [[nodiscard]] int indexForBranch(const QString &branch) const;

  [[nodiscard]] QString branchForIndex(int index) const;

  void setActiveIndex(int index);
  void updateModel(const QStringList &branches, int activeIndex = -1);
  void
  updateModelWithStatus(const QVector<GitRepo::BranchInfo> &branchesWithStatus,
                        int activeIndex = -1);

signals:
  void branchesMoved(const QString &sourceBranch, const QString &destBranch);

private:
  int _prevIndex;
};

} // namespace GuiManager::Models
