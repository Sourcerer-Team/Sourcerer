#include "models/WorkspacesModel.h"
#include "Constants.h"

#include <Git.h>

#include <QBrush>
#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QPalette>
#include <QStandardPaths>

#define THREAD_ID "WorkspacesModel"

namespace GuiManager::Models {

WorkspacesModel::WorkspacesModel(QObject *parent)
    : QAbstractTableModel{parent} {
  reload();
}

QModelIndex WorkspacesModel::createIndex(int row, int column) const {
  return QAbstractItemModel::createIndex(row, column);
}

int WorkspacesModel::columnCount(const QModelIndex &parent) const {
  return 2;
}

int WorkspacesModel::rowCount(const QModelIndex &parent) const {
  return _workspaces.length();
}

QVariant WorkspacesModel::data(const QModelIndex &index, const int role) const {
  const int column = index.column();
  if (role == Qt::ForegroundRole && column == 0) {
    return QBrush(QGuiApplication::palette().color(QPalette::WindowText));
  }

  if (role == Qt::FontRole && column == 0) {
    QFont boldFont;
    boldFont.setBold(true);
    return boldFont;
  }

  if (role != Qt::ItemDataRole::DisplayRole)
    return QVariant();

  if (column != 0)
    return QVariant();

  const int row = index.row();
  if (row < 0 || row > _workspaces.length() - 1)
    return QVariant();

  return _workspaces[row];
}

void WorkspacesModel::clear() {
  _workspaces.clear();
}

void WorkspacesModel::reload() {
  clear();
  load();
}

void WorkspacesModel::load() {
  beginResetModel();

  const QString writableDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
  const QString workspacesDirectory =
      writableDir + "/" + WORKSPACES_SUBDIRECTORY;

  QDir dir(workspacesDirectory);
  dir.setNameFilters({"*.workspace"});

  const QFileInfoList files =
      dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot, QDir::Name);

  for (const auto &file : files) {
    const QString fileName = file.completeBaseName();
    _workspaces += fileName;
  }

  endResetModel();
}

} // namespace GuiManager::Models
