#include "models/GitCommitTreeModel.h"
#include "models/GitCommitListModel.h"

#include <QDir>
#include <QScopedPointer>

namespace GuiManager::Models {

GitCommitTreeModel::GitCommitTreeModel(QObject *parent)
    : QStandardItemModel(parent) {}

GitCommitTreeModel::~GitCommitTreeModel() = default;

GitCommitTreeModel *
GitCommitTreeModel::from(const GitRepo::GitCommitStatus &commitStatus) {
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

  QScopedPointer<GitCommitTreeModel> model(new GitCommitTreeModel());

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

  return model.take();
}

} // namespace GuiManager::Models
