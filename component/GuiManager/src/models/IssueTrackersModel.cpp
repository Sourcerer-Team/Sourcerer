#include "models/IssueTrackersModel.h"

#include <TokensStorage.h>

#include <QDateTime>
#include <QIcon>
#include <QSize>

#define THREAD_ID "IssueTrackersModel"

namespace GuiManager::Models {

IssueTrackersModel::IssueTrackersModel(QObject *parent)
    : QAbstractTableModel{parent} {
  reload();
}

int IssueTrackersModel::columnCount(const QModelIndex &parent) const {
  return 2;
}

int IssueTrackersModel::rowCount(const QModelIndex &parent) const {
  return parent.isValid() ? 0 : _entries.size();
}

QVariant IssueTrackersModel::data(const QModelIndex &index,
                                  const int role) const {
  if (!index.isValid()) {
    return QVariant();
  }

  const int row = index.row();
  if (row < 0 || row >= _entries.size()) {
    return QVariant();
  }

  const auto &entry = _entries.at(row);

  if (role == Qt::DecorationRole && index.column() == 0) {
    return serviceIcon(entry.serviceType);
  }

  if (role == Qt::SizeHintRole && index.column() == 0) {
    return QSize(24, 24);
  }

  if (role == Qt::UserRole && index.column() == 0) {
    return entry.serviceType;
  }

  if (role != Qt::DisplayRole) {
    return QVariant();
  }

  switch (index.column()) {
  case 0:
    return QVariant();
  case 1:
    return entry.repositoryName;
  default:
    return QVariant();
  }
}

QVariant IssueTrackersModel::headerData(const int section,
                                        const Qt::Orientation orientation,
                                        const int role) const {
  if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
    switch (section) {
    case 0:
      return tr("Service");
    case 1:
      return tr("Repository");
    case 2:
      return tr("Status");
    default:
      return QVariant();
    }
  }

  return QAbstractTableModel::headerData(section, orientation, role);
}

void IssueTrackersModel::reload() {
  beginResetModel();
  _entries = IssueTrackers::TokensStorage::getInstance().getAllTokenEntries();
  endResetModel();
}

QIcon IssueTrackersModel::serviceIcon(const QString &serviceType) {
  const QString normalized = normalizedServiceType(serviceType);
  if (normalized.contains(QStringLiteral("github"))) {
    return QIcon(QStringLiteral(":/icons/issue-trackers/github.svg"));
  }
  if (normalized.contains(QStringLiteral("gitlab"))) {
    return QIcon(QStringLiteral(":/icons/issue-trackers/gitlab.svg"));
  }
  if (normalized.contains(QStringLiteral("azure"))) {
    return QIcon(QStringLiteral(":/icons/issue-trackers/azure.svg"));
  }
  if (normalized.contains(QStringLiteral("bitbucket"))) {
    return QIcon(QStringLiteral(":/icons/issue-trackers/bitbucket.svg"));
  }
  if (normalized == QStringLiteral("custom")) {
    return QIcon(QStringLiteral(":/icons/issue-trackers/custom.svg"));
  }

  return QIcon();
}

QString IssueTrackersModel::normalizedServiceType(const QString &serviceType) {
  return serviceType.trimmed().toLower();
}

} // namespace GuiManager::Models
