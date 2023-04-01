
#pragma once

#include <QStandardItemModel>

#include <QObject>

namespace GuiManager {

namespace Models {

class BranchesModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit BranchesModel(QObject *parent = nullptr);
  virtual ~BranchesModel();

  virtual QString iconActivePath() const;
  virtual QString iconPath() const;

  int indexForBranch(const QString &branch) const;

  void setActiveIndex(int index);
  void updateModel(const QStringList &branches, int activeIndex = -1);

private:
  int _prevIndex;
};

} // namespace Models
} // namespace GuiManager
