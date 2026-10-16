
#pragma once

#include <GitRepo.h>
#include <GraphiteUtils.h>

#include <QObject>
#include <QStandardItemModel>

namespace GuiManager::Models {

class BranchesModel : public QStandardItemModel {
  Q_OBJECT
public:
  enum Roles {
    // GitRepo::GraphiteBranchRow, only in Graphite mode.
    GraphiteRowRole = Qt::UserRole + 100,
    // GitRepo::GraphiteBranch of tracked branches.
    GraphiteBranchRole,
    // Whether the row is the active branch.
    ActiveRole,
  };

  explicit BranchesModel(QObject *parent = nullptr);
  ~BranchesModel() override;

  [[nodiscard]] virtual QString iconActivePath() const;
  [[nodiscard]] virtual QString iconPath() const;

  Qt::ItemFlags flags(const QModelIndex &index) const override;

  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;

  Qt::DropActions supportedDropActions() const override;

  QStringList mimeTypes() const override;

  QMimeData *mimeData(const QModelIndexList &indexes) const override;

  bool dropMimeData(const QMimeData *data, Qt::DropAction action, int row,
                    int column, const QModelIndex &parent) override;

  [[nodiscard]] int indexForBranch(const QString &branch) const;

  [[nodiscard]] QString branchForIndex(int index) const;

  void setActiveIndex(int index);

  // Attaches the Graphite layout to rows already ordered by
  // GitRepo::graphiteBranchRows(). headerRow is the row of the untracked
  // section header, or -1.
  void setGraphite(const QVector<GitRepo::GraphiteBranchRow> &rows,
                   const GitRepo::GraphiteStacks &stacks, int headerRow);
  void clearGraphite();
  [[nodiscard]] bool isHeaderRow(int row) const;
  void updateModel(const QStringList &branches, int activeIndex = -1);
  void
  updateModelWithStatus(const QVector<GitRepo::BranchInfo> &branchesWithStatus,
                        int activeIndex = -1);

signals:
  void branchesMoved(const QString &sourceBranch, const QString &destBranch);

private:
  [[nodiscard]] QString graphiteToolTip(const QString &branch) const;

  int _prevIndex;
  QHash<QString, GitRepo::GraphiteBranchRow> _graphiteRows;
  GitRepo::GraphiteStacks _graphiteStacks;
  int _graphiteHeaderRow = -1;
};

} // namespace GuiManager::Models
