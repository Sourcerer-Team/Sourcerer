// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "GitRepo.h"

#include <QAbstractListModel>
#include <QHash>
#include <QSharedPointer>

namespace GuiManager::Models {

enum class GitCommitListModificationType {
  Unknown = 0,

  Conflicted,
  Deleted,
  Ignored,
  Modified,
  New,
  Renamed,
  TypeChanged,
  Unreadable,
  Untracked,
  Other,

  last
};

struct GitCommitListModification {
  QString _filePath;
  GitCommitListModificationType _type{GitCommitListModificationType::Unknown};
};

class GitCommitListModel final : public QAbstractListModel {
  Q_OBJECT

public:
  constexpr static Qt::ItemDataRole ModificationTypeRole = Qt::UserRole;

  static GitCommitListModel *from(const GitRepo::GitCommitStatus &commitStatus);

  static GitCommitListModel *from(const GitRepo::GitStatus &status);

  static GitCommitListModel *from(const QStringList &files);
  static GitCommitListModel *
  from(const QStringList &files,
       const QHash<QString, GitCommitListModificationType> &modificationTypes,
       GitCommitListModificationType defaultType =
           GitCommitListModificationType::Unknown);

  static GitCommitListModel *fromStaged(const GitRepo::GitStatus &status);

public:
  explicit GitCommitListModel(QObject *parent = nullptr);

  int rowCount(const QModelIndex &parent = QModelIndex()) const override;
  QVariant data(const QModelIndex &index,
                int role = Qt::DisplayRole) const override;
  QHash<int, QByteArray> roleNames() const override;

  void setModifications(const QString &commit,
                        const QList<GitCommitListModification> &modifications) {
    _commit = commit;
    _modifications = modifications;
    _modificationTypes.clear();
    for (const auto &modification : _modifications) {
      _modificationTypes.insert(modification._filePath, modification._type);
    }
  }

  QHash<QString, GitCommitListModificationType> modificationTypes() const {
    return _modificationTypes;
  }

  void clear() {
    _commit.clear();
    _modifications.clear();
    _modificationTypes.clear();
  }

private:
  QString _commit;
  QList<GitCommitListModification> _modifications;
  QHash<QString, GitCommitListModificationType> _modificationTypes;
};

} // namespace GuiManager::Models
