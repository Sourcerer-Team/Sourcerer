#pragma once

#include <QString>
#include <QStringList>

#include <functional>

// Installs a macOS update delivered as a .dmg containing Sourcerer.app. Used by
// the launcher only; see launcher.cpp.
namespace MacUpdater {

enum class Status {
  Installed, // The bundle was replaced.
  UpToDate,  // The image does not contain a newer version; nothing was changed.
  Cancelled, // The user declined the administrator password prompt.
  Failed,    // The update is unusable or could not be installed.
};

struct Outcome {
  Status status = Status::Failed;
  QString message; // Human readable details for Failed / Cancelled.
};

struct Callbacks {
  // Progress text for the launcher window.
  std::function<void(const QString &)> status;
  // The system password dialog must not be covered by the (always on top)
  // launcher window, so it is hidden while the dialog is shown.
  std::function<void(bool)> setWindowVisible;
};

// Verifies the disk image, mounts it, validates the contained application and
// replaces the running bundle with it. Elevates through the system password
// dialog if the bundle cannot be modified by the current user.
Outcome installFromDmg(const QString &dmgPath, const Callbacks &callbacks);

// Entry point of `SourcererLauncher --apply-update ...`, executed as root
// through the system password dialog. `arguments` are the command line
// arguments following `--apply-update`. Returns the process exit code.
int runPrivilegedHelper(const QStringList &arguments);

// The command line switch that selects the privileged helper mode.
QString privilegedHelperSwitch();

} // namespace MacUpdater
