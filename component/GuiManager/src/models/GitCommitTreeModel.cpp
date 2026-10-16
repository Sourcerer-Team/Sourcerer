#include "models/GitCommitTreeModel.h"
#include "models/GitCommitListModel.h"

#include <Log.h>

#include <QDir>
#include <memory>

#define THREAD_ID "GitCommitTreeModel"

namespace GuiManager::Models {

GitCommitTreeModel::GitCommitTreeModel(QObject *parent)
    : QStandardItemModel(parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GitCommitTreeModel::~GitCommitTreeModel() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GitCommitTreeModel *
GitCommitTreeModel::from(const GitRepo::GitCommitStatus &commitStatus) {
  LOG_PERF(0, "Enter");

  QList<GitCommitListModification> modifications;

  for (const auto &file : commitStatus.newFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::New;
    modifications.append(modification);
  }

  for (const auto &file : commitStatus.modifiedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Modified;
    modifications.append(modification);
  }

  for (const auto &file : commitStatus.deletedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Deleted;
    modifications.append(modification);
  }

  for (const auto &file : commitStatus.renamedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Renamed;
    modifications.append(modification);
  }

  for (const auto &file : commitStatus.untrackedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Untracked;
    modifications.append(modification);
  }

  QList<GitCommitListModification> transformedModifications;
  transformedModifications.resize(modifications.size());

  std::transform(modifications.begin(), modifications.end(),
                 transformedModifications.begin(),
                 [](const GitCommitListModification &modification) {
                   GitCommitListModification normalized(modification);
                   normalized._filePath =
                       QDir::toNativeSeparators(modification._filePath);
                   return normalized;
                 });

  std::sort(transformedModifications.begin(), transformedModifications.end(),
            [](const GitCommitListModification &x,
               const GitCommitListModification &y) {
              return x._filePath < y._filePath;
            });

  std::unique_ptr<GitCommitTreeModel> model(new GitCommitTreeModel());

  for (const auto &modification : transformedModifications) {
    QStandardItem *parent = model->invisibleRootItem();
    const auto parts = modification._filePath.split(QDir::separator());
    for (const QString &part : parts) {
      // Check if child already exists
      bool found = false;
      for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (child->text() == part) {
          parent = child;
          found = true;
          break;
        }
      }

      if (!found) {
        QStandardItem *newItem = new QStandardItem(part);
        newItem->setData((int)modification._type, Qt::UserRole);
        parent->appendRow(newItem);
        parent = newItem;
      }
    }
  }

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitTreeModel *GitCommitTreeModel::from(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  QList<GitCommitListModification> modifications;

  for (const auto &file : status.newFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::New;
    modifications.append(modification);
  }

  for (const auto &file : status.modifiedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Modified;
    modifications.append(modification);
  }

  for (const auto &file : status.deletedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Deleted;
    modifications.append(modification);
  }

  for (const auto &file : status.renamedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Renamed;
    modifications.append(modification);
  }

  for (const auto &file : status.conflictedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Conflicted;
    modifications.append(modification);
  }

  QList<GitCommitListModification> transformedModifications;
  transformedModifications.resize(modifications.size());

  std::transform(modifications.begin(), modifications.end(),
                 transformedModifications.begin(),
                 [](const GitCommitListModification &modification) {
                   GitCommitListModification normalized(modification);
                   normalized._filePath =
                       QDir::toNativeSeparators(modification._filePath);
                   return normalized;
                 });

  std::sort(transformedModifications.begin(), transformedModifications.end(),
            [](const GitCommitListModification &x,
               const GitCommitListModification &y) {
              return x._filePath < y._filePath;
            });

  std::unique_ptr<GitCommitTreeModel> model(new GitCommitTreeModel());

  for (const auto &modification : transformedModifications) {
    QStandardItem *parent = model->invisibleRootItem();
    const auto parts = modification._filePath.split(QDir::separator());
    for (const QString &part : parts) {
      if (part.isEmpty()) {
        continue;
      }
      // Check if child already exists
      bool found = false;
      for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (child->text() == part) {
          parent = child;
          found = true;
          break;
        }
      }

      if (!found) {
        QStandardItem *newItem = new QStandardItem(part);
        newItem->setData((int)modification._type, Qt::UserRole);
        parent->appendRow(newItem);
        parent = newItem;
      }
    }
  }

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitTreeModel *GitCommitTreeModel::from(const QStringList &files) {
  return from(files, {}, GitCommitListModificationType::Unknown);
}

GitCommitTreeModel *GitCommitTreeModel::from(
    const QStringList &files,
    const QHash<QString, GitCommitListModificationType> &modificationTypes,
    const GitCommitListModificationType defaultType) {
  LOG_PERF(0, "Enter");

  QList<GitCommitListModification> modifications;
  modifications.reserve(files.size());

  for (const auto &file : files) {
    const QString normalizedFile = QDir::toNativeSeparators(file);
    GitCommitListModification modification;
    modification._filePath = normalizedFile;
    modification._type = modificationTypes.value(normalizedFile, defaultType);
    modifications.append(modification);
  }

  std::sort(modifications.begin(), modifications.end(),
            [](const GitCommitListModification &x,
               const GitCommitListModification &y) {
              return x._filePath < y._filePath;
            });

  std::unique_ptr<GitCommitTreeModel> model(new GitCommitTreeModel());

  for (const auto &modification : modifications) {
    QStandardItem *parent = model->invisibleRootItem();
    const auto parts = modification._filePath.split(QDir::separator());
    for (const QString &part : parts) {
      if (part.isEmpty()) {
        continue;
      }

      bool found = false;
      for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (child->text() == part) {
          parent = child;
          found = true;
          break;
        }
      }

      if (!found) {
        QStandardItem *newItem = new QStandardItem(part);
        newItem->setData((int)modification._type, Qt::UserRole);
        parent->appendRow(newItem);
        parent = newItem;
      }
    }
  }

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitTreeModel *
GitCommitTreeModel::fromStaged(const GitRepo::GitStatus &status) {
  LOG_PERF(0, "Enter");

  QList<GitCommitListModification> modifications;

  for (const auto &file : status.newStagedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::New;
    modifications.append(modification);
  }

  for (const auto &file : status.modifiedStagedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Modified;
    modifications.append(modification);
  }

  for (const auto &file : status.deletedStagedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Deleted;
    modifications.append(modification);
  }

  for (const auto &file : status.renamedStagedFiles) {
    GitCommitListModification modification;
    modification._filePath = file;
    modification._type = GitCommitListModificationType::Renamed;
    modifications.append(modification);
  }

  QList<GitCommitListModification> transformedModifications;
  transformedModifications.resize(modifications.size());

  std::transform(modifications.begin(), modifications.end(),
                 transformedModifications.begin(),
                 [](const GitCommitListModification &modification) {
                   GitCommitListModification normalized(modification);
                   normalized._filePath =
                       QDir::toNativeSeparators(modification._filePath);
                   return normalized;
                 });

  std::sort(transformedModifications.begin(), transformedModifications.end(),
            [](const GitCommitListModification &x,
               const GitCommitListModification &y) {
              return x._filePath < y._filePath;
            });

  std::unique_ptr<GitCommitTreeModel> model(new GitCommitTreeModel());

  for (const auto &modification : transformedModifications) {
    QStandardItem *parent = model->invisibleRootItem();
    const auto parts = modification._filePath.split(QDir::separator());
    for (const QString &part : parts) {
      if (part.isEmpty()) {
        continue;
      }
      // Check if child already exists
      bool found = false;
      for (int i = 0; i < parent->rowCount(); ++i) {
        QStandardItem *child = parent->child(i);
        if (child->text() == part) {
          parent = child;
          found = true;
          break;
        }
      }

      if (!found) {
        QStandardItem *newItem = new QStandardItem(part);
        newItem->setData((int)modification._type, Qt::UserRole);
        parent->appendRow(newItem);
        parent = newItem;
      }
    }
  }

  LOG_PERF(0, "Exit");

  return model.release();
}

} // namespace GuiManager::Models
