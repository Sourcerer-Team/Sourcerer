#pragma once

#include <any>

namespace EventManager {

enum EventType {
  Null = 0,

  // ComponentMonitor
  ComponentStarted,
  ComponentStopped,

  // GitRepoManager
  GitRepoManagerCloseRepo,
  GitRepoManagerCloseRepoResult,
  GitRepoManagerCloseRepoError,
  GitRepoManagerListLocalBranches,
  GitRepoManagerListLocalBranchesResult,
  GitRepoManagerListLocalBranchesError,
  GitRepoManagerListRemoteBranches,
  GitRepoManagerListRemoteBranchesResult,
  GitRepoManagerListRemoteBranchesError,
  GitRepoManagerOpenRepo,
  GitRepoManagerOpenRepoResult,
  GitRepoManagerOpenRepoError,
  GitRepoManagerQuit,
  GitRepoManagerQuitResult,
  GitRepoManagerQuitError,

  last
};

class Event {
public:
  Event(EventType eventType) : _type(eventType) {}

  void addData(std::any data) { _anyData = data; }

  template <typename T> T data() const { return std::any_cast<T>(_anyData); }

  EventType type() const { return _type; }

private:
  EventType _type;
  std::any _anyData;
};

} // namespace EventManager
