// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "Log.h"

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <QMap>
#include <QThread>
#include <QTimer>

#include <atomic>
#include <climits>
#include <condition_variable>
#include <deque>
#include <functional>
#include <map>
#include <mutex>
#include <qcoreapplication.h>

namespace Util {

enum class DispatcherChannel { Any = -1, Pool0 = 0 };

enum class DispatcherPriority {
  Normal = 0,
  // Executed one at a time, in the order of dispatching, and only while no
  // normal priority task is queued or being executed. A low priority task
  // that has already started is not interrupted by normal ones dispatched
  // after it.
  Low,
};

enum class DispatcherCommand {
  Unknown = 0,

  Run,
  Quit,

  last
};

struct DispatcherRecord {
  DispatcherCommand _command = DispatcherCommand::Quit;
  std::function<void()> _task;
  int _taskId = -1;
};

// Low priority tasks waiting for the normal priority ones to finish.
struct DispatcherLowPriorityLane {
  std::mutex _mutex;
  std::condition_variable _condition;
  std::deque<DispatcherRecord> _tasks;
  // Normal priority tasks queued or currently being executed by the pool.
  size_t _normalPending = 0;
  bool _quit = false;

  void normalTasksAdded(size_t count);
  void normalTasksRemoved(size_t count);
};

struct DispatcherPoolElement {
  BlockingQueue<DispatcherRecord> *_queue = nullptr;
  QThread *_thread = nullptr;
  // Tasks queued on this element or currently being executed by it.
  std::atomic<size_t> _pendingTasks{0};
  QSharedPointer<DispatcherLowPriorityLane> _lane;
};

class Dispatcher : public QObject {
  Q_OBJECT

public:
  explicit Dispatcher(int coresNum = INT_MAX);
  ~Dispatcher() override;

  // `channel` is ignored for low priority tasks, they have a thread of their
  // own.
  bool dispatch(
      const std::function<void()> &task,
      const std::function<void()> &finalTask = nullptr,
      const std::function<void(const std::exception &)> &errorTask = nullptr,
      DispatcherChannel channel = DispatcherChannel::Any,
      DispatcherPriority priority = DispatcherPriority::Normal);

  template <class T>
  static bool
  dispatchToMainThreadWithOwner(T *owner,
                                const std::function<void()> &callback) {
    if (!owner)
      return false;

    const auto timer = new QTimer();
    timer->moveToThread(qApp->thread());
    timer->setSingleShot(true);

    QObject::connect(timer, &QTimer::timeout, [=]() {
      dispatchToMainThread(owner->getSelf().toWeakRef(), callback);
      timer->deleteLater();
    });

    QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection,
                              Q_ARG(int, 0));

    return true;
  }

  template <class T>
  static bool dispatchToMainThread(const T &guard, const int timeout,
                                   const std::function<void()> &callback) {
    QMetaObject::invokeMethod(
        qApp,
        [=]() {
          if (auto locked = guard.lock()) {
            const auto THREAD_ID = "dispatchToMainThread";
            try {
              callback();
            } catch (const std::exception &e) {
              LOG_ERROR(Log::ErrorId::Gui, "Exception: \"{}\"", e.what());
            } catch (...) {
              LOG_ERROR(Log::ErrorId::Gui, "Unknown exception thrown");
            }
          }
        },
        Qt::QueuedConnection);

    return true;
  }

  template <class T>
  static bool dispatchToMainThread(const T &guard,
                                   const std::function<void()> &callback) {
    return dispatchToMainThread(guard, 0, callback);
  }

  // Returns true if no normal priority task is queued or currently being
  // executed. Low priority tasks are not taken into account.
  bool idle() const;

  void stopAll();

  void clearPool();

private:
  static void processor(QSharedPointer<DispatcherPoolElement> poolElement);
  static void
  lowPriorityProcessor(QSharedPointer<DispatcherLowPriorityLane> lane);

private:
  mutable Util::Mutex _mutex;
  QMap<size_t, QSharedPointer<DispatcherPoolElement>> _pool;
  QSharedPointer<DispatcherLowPriorityLane> _lane;
};

} // namespace Util
