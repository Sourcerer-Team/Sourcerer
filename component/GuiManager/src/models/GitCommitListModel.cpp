#include "models/GitCommitListModel.h"

#include <Log.h>
#include <QDir>

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

  QScopedPointer<GitCommitListModel> model(new GitCommitListModel());
  model->_modifications = transformedModifications;

  LOG_PERF(0, "Exit");

  return model.take();
}

GitCommitListModel *GitCommitListModel::from(const GitRepo::GitStatus &status)
{
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

  QScopedPointer<GitCommitListModel> model(new GitCommitListModel());
  model->_modifications = transformedModifications;

  LOG_PERF(0, "Exit");

  return model.take();
}

GitCommitListModel *GitCommitListModel::fromStaged(const GitRepo::GitStatus &status)
{
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

  QScopedPointer<GitCommitListModel> model(new GitCommitListModel());
  model->_modifications = transformedModifications;

  LOG_PERF(0, "Exit");

  return model.take();
}

int GitCommitListModel::rowCount(const QModelIndex &parent) const {
  LOG_PERF(0, "Enter");

  LOG_PERF(0, "Exit");

  return _modifications.count();
}

QVariant GitCommitListModel::data(const QModelIndex &index, int role) const {
  LOG_PERF(0, "Enter");

  if (!index.isValid() || index.row() < 0 ||
      index.row() >= static_cast<int>(_modifications.count())) {
    LOG_PERF(0, "Exit");
    return QVariant();
  }

  const GitCommitListModification &modification =
      _modifications.at(index.row());

  switch (role) {
  case Qt::DisplayRole: {
    LOG_PERF(0, "Exit");
    return modification._filePath;
  }
  case Qt::UserRole: {
    LOG_PERF(0, "Exit");
    return static_cast<int>(modification._type);
  }
  default: {
    break;
  }
  }

  LOG_PERF(0, "Exit");
  return QVariant();
}
QHash<int, QByteArray> GitCommitListModel::roleNames() const {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");

  return QAbstractListModel::roleNames();
}

} // namespace GuiManager::Models
