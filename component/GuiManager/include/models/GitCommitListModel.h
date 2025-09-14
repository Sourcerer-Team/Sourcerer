#pragma once

#include "GitRepo.h"

#include <QAbstractListModel>
#include <QSharedPointer>

namespace GuiManager::Models {

enum class GitCommitListModificationType {
  Unknown = 0,

  Conflicted,
  Deleted,
  Modified,
  New,
  Renamed,
  TypeChanged,
  Unreadable,

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
  }

  void clear() {
    _commit.clear();
    _modifications.clear();
  }

private:
  QString _commit;
  QList<GitCommitListModification> _modifications;
};

} // namespace GuiManager::Models
