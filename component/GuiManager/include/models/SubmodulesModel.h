
#pragma once

#include "BranchesModel.h"

#include <GitRepo.h>

#include <QObject>

namespace GuiManager {

namespace Models {

class SubmodulesModel : public BranchesModel {
  Q_OBJECT
public:
  explicit SubmodulesModel(QObject *parent = nullptr);
  virtual ~SubmodulesModel();

  // BranchesModel interface
public:
  QString iconActivePath() const override;
  QString iconPath() const override;
  void updateModelWithStatus(
      const QVector<GitRepo::SubmoduleInfo> &submodulesWithStatus,
      int activeIndex = -1);
};

} // namespace Models
} // namespace GuiManager
