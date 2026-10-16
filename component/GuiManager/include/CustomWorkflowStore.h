#pragma once

#include <GitFlowNextWorkflow.h>

#include <QDateTime>
#include <QObject>

#include <optional>

namespace GuiManager {

// The custom GitFlow-Next workflows, shared by all repositories. They are kept
// in gitflow-next-workflows.json in the application data directory.
class CustomWorkflowStore final : public QObject {
  Q_OBJECT
public:
  static CustomWorkflowStore &instance();

  // Sorted by name. Reloads the file if it changed on disk.
  QVector<GitRepo::GitFlowNextWorkflow> workflows();
  std::optional<GitRepo::GitFlowNextWorkflow> workflow(const QString &id);

  // Adds workflow if its id is empty, otherwise replaces the stored one and
  // raises its revision. workflow is updated with the saved id and revision.
  bool save(GitRepo::GitFlowNextWorkflow &workflow, QString *error = nullptr);
  bool remove(const QString &id, QString *error = nullptr);

  QString filePath() const;

signals:
  void changed();

private:
  CustomWorkflowStore();

  void reloadIfChanged();
  bool write(const QVector<GitRepo::GitFlowNextWorkflow> &workflows,
             QString *error);

  QVector<GitRepo::GitFlowNextWorkflow> _workflows;
  bool _loaded = false;
  QDateTime _loadedModified;
  qint64 _loadedSize = -1;
};

} // namespace GuiManager
