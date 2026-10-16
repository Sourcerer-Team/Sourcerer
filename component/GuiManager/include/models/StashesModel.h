
#pragma once

#include <QStandardItemModel>

namespace GuiManager::Models {

class StashesModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit StashesModel(QObject *parent = nullptr);
  ~StashesModel() override;

  [[nodiscard]] virtual QString iconActivePath() const;
  [[nodiscard]] virtual QString iconPath() const;

  [[nodiscard]] int indexForStash(const QString &branch) const;

  [[nodiscard]] QString stashForIndex(int index) const;

  void setActiveIndex(int index);
  void updateModel(const QStringList &stashes, int activeIndex = -1);

private:
  int _prevIndex;
};

} // namespace GuiManager::Models
