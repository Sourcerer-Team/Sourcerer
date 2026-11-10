// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include <ConfigManager.h>

#include <QString>
#include <QVector>

namespace Common {

struct UserProfile {
  QString author;
  QString email;
};

class UserProfileManager {
public:
  static UserProfileManager &instance();

  int currentIndex() const { return _currentIndex; }

  void setCurrentIndex(int index) { _currentIndex = index; }

  const QVector<UserProfile> &profiles() const { return _profiles; }

  bool hasCurrentProfile() const {
    return _currentIndex >= 0 && _currentIndex < _profiles.size();
  }

  void setProfiles(const QVector<UserProfile> &profiles) {
    _profiles = profiles;
  }

  const UserProfile &currentProfile() const {
    return _currentIndex >= 0 && _currentIndex < _profiles.size()
               ? _profiles[_currentIndex]
               : _emptyProfile;
  }

  void load();
  void save();

private:
  UserProfileManager();
  ~UserProfileManager() = default;
  UserProfileManager(const UserProfileManager &) = delete;
  UserProfileManager &operator=(const UserProfileManager &) = delete;

private:
  int _currentIndex;
  QVector<UserProfile> _profiles;
  ConfigManager::ConfigManager &_configManager;
  static UserProfile _emptyProfile;
};

} // namespace Common
