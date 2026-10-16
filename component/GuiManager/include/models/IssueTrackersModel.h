#pragma once

#include <QAbstractTableModel>
#include <QIcon>

#include <TokensStorage.h>

namespace GuiManager::Models {

class IssueTrackersModel final : public QAbstractTableModel {
public:
  explicit IssueTrackersModel(QObject *parent = nullptr);

  // QAbstractItemModel interface
  int columnCount(const QModelIndex &parent) const override;
  int rowCount(const QModelIndex &parent) const override;
  QVariant data(const QModelIndex &index, int role) const override;
  QVariant headerData(int section, Qt::Orientation orientation,
                      int role) const override;

  void reload();

private:
  static QIcon serviceIcon(const QString &serviceType);
  static QString normalizedServiceType(const QString &serviceType);

private:
  QList<IssueTrackers::TokensStorage::TokenEntry> _entries;
};

} // namespace GuiManager::Models
