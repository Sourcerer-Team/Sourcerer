#pragma once

#include <GitRepo.h>
#include "models/GitCommitListModel.h"

#include <QHash>
#include <QStandardItemModel>

namespace GuiManager::Models {

class GitCommitTreeModel : public QStandardItemModel {
  Q_OBJECT
public:
  explicit GitCommitTreeModel(QObject *parent = nullptr);
  ~GitCommitTreeModel() override;

  static GitCommitTreeModel *from(const GitRepo::GitCommitStatus &commitStatus);

  static GitCommitTreeModel *from(const GitRepo::GitStatus &status);

  static GitCommitTreeModel *from(const QStringList &files);
  static GitCommitTreeModel *from(
      const QStringList &files,
      const QHash<QString, GitCommitListModificationType> &modificationTypes,
      GitCommitListModificationType defaultType =
          GitCommitListModificationType::Unknown);

  static GitCommitTreeModel *fromStaged(const GitRepo::GitStatus &status);
};
} // namespace GuiManager::Models
