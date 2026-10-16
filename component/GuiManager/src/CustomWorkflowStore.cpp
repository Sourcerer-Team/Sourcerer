#include "CustomWorkflowStore.h"

#include <Log.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>

#include <algorithm>

#define THREAD_ID "CustomWorkflowStore"

namespace GuiManager {

CustomWorkflowStore &CustomWorkflowStore::instance() {
  static CustomWorkflowStore store;
  return store;
}

CustomWorkflowStore::CustomWorkflowStore() = default;

QString CustomWorkflowStore::filePath() const {
  return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
      .absoluteFilePath(QStringLiteral("gitflow-next-workflows.json"));
}

void CustomWorkflowStore::reloadIfChanged() {
  const QFileInfo info(filePath());
  const QDateTime modified = info.exists() ? info.lastModified() : QDateTime();
  const qint64 size = info.exists() ? info.size() : -1;
  if (_loaded && modified == _loadedModified && size == _loadedSize) {
    return;
  }

  _loaded = true;
  _loadedModified = modified;
  _loadedSize = size;
  _workflows.clear();

  QFile file(info.absoluteFilePath());
  if (!info.exists() || !file.open(QIODevice::ReadOnly)) {
    return;
  }
  _workflows = GitRepo::parseGitFlowNextWorkflows(file.readAll());
  std::sort(_workflows.begin(), _workflows.end(),
            [](const GitRepo::GitFlowNextWorkflow &left,
               const GitRepo::GitFlowNextWorkflow &right) {
              return QString::localeAwareCompare(left.name, right.name) < 0;
            });
  LOG_DEBUG("Loaded {} custom workflows", _workflows.size());
}

QVector<GitRepo::GitFlowNextWorkflow> CustomWorkflowStore::workflows() {
  reloadIfChanged();
  return _workflows;
}

std::optional<GitRepo::GitFlowNextWorkflow>
CustomWorkflowStore::workflow(const QString &id) {
  reloadIfChanged();
  for (const GitRepo::GitFlowNextWorkflow &workflow :
       std::as_const(_workflows)) {
    if (workflow.id == id) {
      return workflow;
    }
  }
  return std::nullopt;
}

bool CustomWorkflowStore::write(
    const QVector<GitRepo::GitFlowNextWorkflow> &workflows, QString *error) {
  QDir().mkpath(QFileInfo(filePath()).absolutePath());

  // QSaveFile replaces the file only once it is completely written.
  QSaveFile file(filePath());
  if (!file.open(QIODevice::WriteOnly) ||
      file.write(GitRepo::serializeGitFlowNextWorkflows(workflows)) < 0 ||
      !file.commit()) {
    if (error) {
      *error = tr("Failed to save the custom workflows to '%1': %2")
                   .arg(filePath(), file.errorString());
    }
    LOG_ERROR(Log::ErrorId::Gui, "Failed to write '{}': {}",
              filePath().toStdString(), file.errorString().toStdString());
    return false;
  }

  // Read back what was written, so the cache matches the file.
  _loaded = false;
  reloadIfChanged();
  emit changed();
  return true;
}

bool CustomWorkflowStore::save(GitRepo::GitFlowNextWorkflow &workflow,
                               QString *error) {
  reloadIfChanged();
  QVector<GitRepo::GitFlowNextWorkflow> workflows = _workflows;

  if (workflow.id.isEmpty()) {
    workflow.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    workflow.revision = 1;
    workflows.append(workflow);
  } else {
    const auto it =
        std::find_if(workflows.begin(), workflows.end(),
                     [&](const GitRepo::GitFlowNextWorkflow &stored) {
                       return stored.id == workflow.id;
                     });
    if (it == workflows.end()) {
      workflow.revision = 1;
      workflows.append(workflow);
    } else {
      workflow.revision = it->revision + 1;
      *it = workflow;
    }
  }

  return write(workflows, error);
}

bool CustomWorkflowStore::remove(const QString &id, QString *error) {
  reloadIfChanged();
  QVector<GitRepo::GitFlowNextWorkflow> workflows = _workflows;
  workflows.erase(
      std::remove_if(workflows.begin(), workflows.end(),
                     [&](const GitRepo::GitFlowNextWorkflow &stored) {
                       return stored.id == id;
                     }),
      workflows.end());
  return write(workflows, error);
}

} // namespace GuiManager
