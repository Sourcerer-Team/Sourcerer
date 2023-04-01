
#pragma once

#include "BranchesModel.h"

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
  QString iconPath() const override;
};

} // namespace Models
} // namespace GuiManager
