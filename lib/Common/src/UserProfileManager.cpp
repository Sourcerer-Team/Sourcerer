#include "UserProfileManager.h"

#include <Log.h>

#define THREAD_ID "UserProfileManager"

namespace Common {

UserProfile UserProfileManager::_emptyProfile{"", ""};

UserProfileManager::UserProfileManager() : _currentIndex(-1) {
  LOG_PERF(0, "Enter");

  _currentIndex = 0;
  _profiles += UserProfile{"Aleksandar Dezelin", "dezelin@gmail.com"};

  LOG_PERF(0, "Exit");
}

UserProfileManager &UserProfileManager::instance() {
  static UserProfileManager instance_;
  return instance_;
}

} // namespace Common
