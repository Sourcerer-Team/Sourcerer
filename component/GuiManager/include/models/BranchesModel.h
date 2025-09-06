
#pragma once

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

  [[nodiscard]] int indexForBranch(const QString &branch) const;

  [[nodiscard]] QString branchForIndex(int index) const;

  void setActiveIndex(int index);
  void updateModel(const QStringList &branches, int activeIndex = -1);

private:
  int _prevIndex;
};

} // namespace GuiManager::Models

