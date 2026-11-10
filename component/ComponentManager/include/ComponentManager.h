// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "Component.h"

#include <EventManager.h>
#include <LockGuard.h>
#include <Log.h>

#include <QString>
#include <QVector>

namespace ComponentManager {

class ComponentManager {
public:
  ComponentManager();
  virtual ~ComponentManager();

  template <typename C> void addThread() {
    const QString THREAD_ID = "ComponentManager";

    LOG_PERF(0, "Enter");
    LOG_DEBUG("Component thread is starting...");

    _started = false;

    Component *component = new C();

    {
      LOCKGUARD(_threadsMutex);
      _threads += component;
    }

    component->start();
    waitForComponentToStart();

    LOG_PERF(0, "Exit");
  }

  template <typename C> void startProcess() {
    const QString THREAD_ID = "ComponentManager";

    LOG_PERF(0, "Enter");
    LOG_DEBUG("Component process is starting...");

    _started = false;

    Component *component = new C();

    {
      LOCKGUARD(_processMutex);
      _process += component;
    }

    component->start();
    waitForComponentToStart();

    LOG_PERF(0, "Exit");
  }

  void stopAll();
  void waitForComponentToStart();
  void waitForComponentToStop();

private:
  void eventCallback(EventManager::Event event);

private:
  Util::Mutex _threadsMutex;
  Util::Mutex _processMutex;

  QVector<Component *> _threads;
  QVector<Component *> _process;

  bool _started;
  bool _stopped;

  EventManager::EventManager &_eventManager;
};

} // namespace ComponentManager
