#include "Dispatcher.h"

#include <Log.h>

#include <QCoreApplication>
#include <QTimer>

#define THREAD_ID "Dispatcher"

namespace Util {

Dispatcher::Dispatcher(int coresNum) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  const size_t coresCount =
      coresNum == INT_MAX ? std::thread::hardware_concurrency() : coresNum;

  LOG_INFO("{} cores found", coresCount);

  for (size_t i = 0; i < coresCount; ++i) {
    LOG_INFO("Starting pool processor for core {}", i);

    auto &pool = _pool[i];
    pool._thread = QThread::create([&]() { processor(pool._queue); });

    connect(pool._thread, &QThread::finished, pool._thread,
            &QObject::deleteLater);

    pool._thread->start();

    LOG_INFO("Pool processor for core {} started.", i);
  }

  LOG_PERF(0, "Exit");
}

Dispatcher::~Dispatcher() {
  LOG_PERF(0, "Enter");

  joinAll();

  LOG_PERF(0, "Exit");
}

void Dispatcher::processor(BlockingQueue<DispatcherRecord> &queue) {
  LOG_PERF(0, "Enter");

  bool quit = false;

  while (!quit) {
    const auto record = queue.pop();

    switch (record._command) {
    case DispatcherCommand::Run: {
      try {
        record._task();
      } catch (const std::exception &ex) {
        LOG_ERROR(Log::ErrorId::Generic, "Exception has been thrown: {}",
                  ex.what());
      } catch (...) {
        LOG_ERROR(Log::ErrorId::Generic, "Unknown exception has been thrown!");
      }

      break;
    }
    case DispatcherCommand::Quit:
    default: {
      quit = true;
      break;
    }
    }
  }

  LOG_PERF(0, "Exit");
}

bool Dispatcher::dispatch(const std::function<void()> &task,
                          const std::function<void()> &finalTask,
                          const std::function<void(const std::exception &)> &errorTask) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  int minSize = INT_MAX;
  int index = -1;

  for (const auto &[k, v] : _pool) {
    if (v._queue.size() < minSize) {
      index = k;
      minSize = v._queue.size();
    }
  }

  if (index == -1) {
    LOG_PERF(0, "Exit");
    return false;
  }

  const auto compositeTask = [=]() {
    if (task) {
      try {
        task();
      } catch (const std::exception& ex) {
        errorTask(ex);
      }
    }

    if (finalTask) {
      finalTask();
    }
  };

  DispatcherRecord runRecord;
  runRecord._command = DispatcherCommand::Run;
  runRecord._task = compositeTask;

  _pool[index]._queue.push(runRecord);

  LOG_PERF(0, "Exit");

  return true;
}

bool Dispatcher::dispatchToMainThread(const std::function<void()> &callback) {
  const auto timer = new QTimer();
  timer->moveToThread(qApp->thread());
  timer->setSingleShot(true);

  QObject::connect(timer, &QTimer::timeout, [=]() {
    // main thread
    callback();
    timer->deleteLater();
  });

  QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection,
                            Q_ARG(int, 0));

  return true;
}

void Dispatcher::joinAll() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  DispatcherRecord quitRecord;
  quitRecord._command = DispatcherCommand::Quit;

  for (auto &[k, v] : _pool) {
    LOG_INFO("Stopping pool processor for core {}", k);

    v._queue.push(quitRecord);

    v._thread->wait();

    LOG_INFO("Pool processor for core {} stopped.", k);
  }

  _pool.clear();

  LOG_PERF(0, "Exit");
}

} // namespace Util
