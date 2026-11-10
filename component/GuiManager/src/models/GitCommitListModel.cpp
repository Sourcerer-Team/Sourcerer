// MIT License
// Copyright (c) 2026 Sourcerer

#include "models/GitCommitListModel.h"

#include <Log.h>
#include <QDir>

#include <memory>

#define THREAD_ID "GitCommitListModel"

namespace GuiManager::Models {

GitCommitListModel::GitCommitListModel(QObject *parent)
    : QAbstractListModel(parent) {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

GitCommitListModel *
GitCommitListModel::from(const GitRepo::GitCommitStatus &commitStatus) {
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

  std::unique_ptr<GitCommitListModel> model(new GitCommitListModel());
  model->setModifications(QString(), transformedModifications);

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitListModel *GitCommitListModel::from(const GitRepo::GitStatus &status) {
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

  std::unique_ptr<GitCommitListModel> model(new GitCommitListModel());
  model->setModifications(QString(), transformedModifications);

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitListModel *GitCommitListModel::from(const QStringList &files) {
  return from(files, {}, GitCommitListModificationType::Unknown);
}

GitCommitListModel *GitCommitListModel::from(
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

  std::unique_ptr<GitCommitListModel> model(new GitCommitListModel());
  model->setModifications(QString(), modifications);

  LOG_PERF(0, "Exit");

  return model.release();
}

GitCommitListModel *
GitCommitListModel::fromStaged(const GitRepo::GitStatus &status) {
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

  std::unique_ptr<GitCommitListModel> model(new GitCommitListModel());
  model->setModifications(QString(), transformedModifications);

  LOG_PERF(0, "Exit");

  return model.release();
}

int GitCommitListModel::rowCount(const QModelIndex &parent) const {
  return _modifications.count();
}

QVariant GitCommitListModel::data(const QModelIndex &index, int role) const {
  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(_modifications.count())) {
    return QVariant();
  }

  const GitCommitListModification &modification =
      _modifications.at(index.row());

  switch (role) {
  case Qt::DisplayRole: {
    return modification._filePath;
  }
  case Qt::UserRole: {
    return static_cast<int>(modification._type);
  }
  default: {
    break;
  }
  }

  return QVariant();
}
QHash<int, QByteArray> GitCommitListModel::roleNames() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return QAbstractListModel::roleNames();
}

} // namespace GuiManager::Models
