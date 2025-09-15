#include "UserProfileManager.h"

#include <Log.h>

#define THREAD_ID "UserProfileManager"

namespace Common {

UserProfile UserProfileManager::_emptyProfile{"Default", ""};

UserProfileManager::UserProfileManager()
    : _currentIndex(-1),
    _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  load();

  LOG_PERF(0, "Exit");
}

UserProfileManager &UserProfileManager::instance() {
  static UserProfileManager instance_;
  return instance_;
}

void UserProfileManager::load() {
  LOG_PERF(0, "Enter");

  const int count =
    _configManager.getString("userprofilemanager/count").toInt();
  const int currentIndex =
    _configManager.getString("userprofilemanager/currentIndex").toInt();

  _currentIndex = currentIndex;

  _profiles.clear();

  for (int i = 0; i < count; ++i) {
    QString key = QString("userprofilemanager/profile_author_%1").arg(i);
    const QString author = _configManager.getString(key);
    key = QString("userprofilemanager/profile_email_%1").arg(i);
    const QString email = _configManager.getString(key);
    _profiles += Common::UserProfile{author, email};
  }

  if (_currentIndex < 0 || _currentIndex >= _profiles.size()) {
    _currentIndex = -1;
  }

  LOG_PERF(0, "Exit");
}

void UserProfileManager::save() {
  LOG_PERF(0, "Enter");

  const int count =
    _configManager.getString("userprofilemanager/count").toInt();

  for (int i = 0; i < count; ++i) {
    QString key = QString("userprofilemanager/profile_author_%1").arg(i);
    _configManager.removeString(key);
    key = QString("userprofilemanager/profile_email_%1").arg(i);
    _configManager.removeString(key);
  }

  _configManager.setString("userprofilemanager/count",
                           QString::number(_profiles.size()));
  _configManager.setString("userprofilemanager/currentindex",
                           QString::number(_currentIndex));

  for (int i = 0; i < _profiles.size(); ++i) {
    QString key = QString("userprofilemanager/profile_author_%1").arg(i);
    _configManager.setString(key, _profiles[i].author);
    key = QString("userprofilemanager/profile_email_%1").arg(i);
    _configManager.setString(key, _profiles[i].email);
  }

  _configManager.save();

  LOG_PERF(0, "Exit");
}

} // namespace Common
