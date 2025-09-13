#pragma once

#include <GitRepo.h>

#include <QStandardItemModel>

namespace GuiManager::Models {

class GitCommitTreeModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit GitCommitTreeModel(QObject *parent = nullptr);
  ~GitCommitTreeModel() override;

  static GitCommitTreeModel *from(const GitRepo::GitCommitStatus &commitStatus);
};
} // namespace GuiManager::Models
