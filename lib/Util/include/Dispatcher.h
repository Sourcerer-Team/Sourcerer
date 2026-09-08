#pragma once

#include "Log.h"

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <QMap>
#include <QThread>
#include <QTimer>

#include <climits>
#include <functional>
#include <map>
#include <qcoreapplication.h>

namespace Util {

enum class DispatcherChannel {
  Any = -1,
  Pool0 = 0
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

struct DispatcherPoolElement {
  BlockingQueue<DispatcherRecord> *_queue = nullptr;
  QThread *_thread = nullptr;
};

class Dispatcher : public QObject {
  Q_OBJECT

public:
  explicit Dispatcher(int coresNum = INT_MAX);
  ~Dispatcher() override;

  bool dispatch(
      const std::function<void()> &task,
      const std::function<void()> &finalTask = nullptr,
      const std::function<void(const std::exception &)> &errorTask = nullptr,
      DispatcherChannel channel = DispatcherChannel::Any);

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

  void stopAll();

  void clearPool();

private:
  static void processor(QSharedPointer<DispatcherPoolElement> poolElement);

private:
  Util::Mutex _mutex;
  QMap<size_t, QSharedPointer<DispatcherPoolElement>> _pool;
};

} // namespace Util
