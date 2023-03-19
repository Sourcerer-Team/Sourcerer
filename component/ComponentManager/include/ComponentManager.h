#pragma once

#include <EventManager.h>
#include <LockGuard.h>
#include <Log.h>

#include <future>
#include <thread>

#include <QString>
#include <QVector>

namespace ComponentManager {

class ComponentManager {
public:
  ComponentManager();
  ~ComponentManager();

  struct ThreadData {
    std::thread thread;
    std::function<void()> function;
    QString name;
  };

  struct ProcessData {
    std::function<void()> function;
    QString name;
  };

  template <typename C> void addThread() {
    const QString THREAD_ID = "ComponentManager";

    LOG_DEBUG("Component thread is starting...");

    LOCKGUARD(_threadsMutex);

    _started = false;

    std::function<void()> function = [&]() {
      static C instance;
      instance.start();
    };

    ThreadData threadData;

    threadData.thread = std::move(std::thread(function));
    threadData.function = function;
    threadData.name = QString::fromStdString(typeid(C).name());

    _threads.append(std::move(threadData));

    waitForComponent();
  }

  template <typename C> void addProcess() {
    const QString THREAD_ID = "ComponentManager";

    LOG_DEBUG("Component process is starting...");

    _started = false;

    ProcessData processData;

    {
      LOCKGUARD(_processMutex);

      std::function<void()> function = [&]() {
        static C instance;
        instance.start();
      };

      processData.function = function;
      processData.name = QString::fromStdString(typeid(C).name());

      _process.append(processData);
    }

    processData.function();

    waitForComponent();
  }

  void stopAll();

  void waitForComponent();

private:
  void eventCallback(EventManager::Event event);

private:
  Util::Mutex _threadsMutex;
  Util::Mutex _processMutex;

  QVector<ThreadData> _threads;
  QVector<ProcessData> _process;

  bool _done;
  bool _started;
};

} // namespace ComponentManager
