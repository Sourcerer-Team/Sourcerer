#include "Dispatcher.h"

#include <Log.h>

#include <QCoreApplication>
#include <QTimer>

#include <limits>

#define THREAD_ID "Dispatcher"

namespace Util {

Dispatcher::Dispatcher(const int coresNum) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  const size_t coresCount =
      coresNum == INT_MAX ? std::thread::hardware_concurrency() : coresNum;

  LOG_INFO("{} cores found", coresCount);

  for (size_t i = 0; i < coresCount; ++i) {
    LOG_INFO("Starting pool processor for core {}", i);

    auto poolElement = QSharedPointer<DispatcherPoolElement>::create();

    _pool[i] = poolElement;

    poolElement->_queue = new BlockingQueue<DispatcherRecord>();
    poolElement->_thread = QThread::create([=]() { processor(poolElement); });

    connect(poolElement->_thread, &QThread::finished, poolElement->_thread,
            &QObject::deleteLater);
    connect(poolElement->_thread, &QThread::finished, poolElement->_queue,
            &QObject::deleteLater);

    poolElement->_thread->start();

    LOG_INFO("Pool processor for core {} started.", i);
  }

  LOG_PERF(0, "Exit");
}

Dispatcher::~Dispatcher() {
  LOG_PERF(0, "Enter");

  stopAll();

  LOG_PERF(0, "Exit");
}

void Dispatcher::processor(QSharedPointer<DispatcherPoolElement> poolElement) {
  LOG_PERF(0, "Enter");

  Q_ASSERT(poolElement);
  Q_ASSERT(poolElement->_queue);

  if (!poolElement) {
    LOG_ERROR(Log::ErrorId::Generic, "Internal error! Invalid pool element!");
    LOG_PERF(0, "Exit");
    return;
  }

  if (!poolElement->_queue) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error! Invalid pool element queue!");
    LOG_PERF(0, "Exit");
    return;
  }

  bool quit = false;

  while (!quit) {

    Q_ASSERT(poolElement->_queue);
    if (!poolElement->_queue) {
      LOG_ERROR(Log::ErrorId::Generic, "Internal error! Queue is null!");
      quit = true;
      break;
    }

    switch (const auto [_command, _task, _taskId] = poolElement->_queue->pop();
            _command) {
    case DispatcherCommand::Run: {
      LOG_DEBUG("Dispatching task with id of {}", _taskId);

      try {
        _task();
      } catch (const std::exception &ex) {
        LOG_ERROR(Log::ErrorId::Generic, "Exception has been thrown: {}",
                  ex.what());
      } catch (...) {
        LOG_ERROR(Log::ErrorId::Generic, "Unknown exception has been thrown!");
      }

      poolElement->_pendingTasks.fetch_sub(1);

      LOG_DEBUG("Task with id of {} finished.", _taskId);

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

bool Dispatcher::dispatch(
    const std::function<void()> &task, const std::function<void()> &finalTask,
    const std::function<void(const std::exception &)> &errorTask,
    DispatcherChannel channel) {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  size_t minSize = std::numeric_limits<size_t>::max();
  int index = static_cast<int>(channel);

  if (index == static_cast<int>(DispatcherChannel::Any)) {
    for (const auto &k : _pool.keys()) {
      LOG_DEBUG("Checking pool with key {}", k);

      const auto &v = _pool[k];
      Q_ASSERT(v);

      if (!v) {
        LOG_ERROR(Log::ErrorId::Generic,
                  "Internal error! Invalid pool element!");
        continue;
      }

      Q_ASSERT(v->_queue);
      if (!v->_queue) {
        LOG_ERROR(Log::ErrorId::Generic,
                  "Internal error! Invalid pool element queue!");
        continue;
      }

      if (v->_queue->size() < minSize) {
        index = static_cast<int>(k);
        minSize = v->_queue->size();
      }

      LOG_DEBUG("Minimal pool with previous key is of size {}", minSize);

      if (index == -1) {
        LOG_PERF(0, "Exit");
        return false;
      }
    }
  }

  LOG_DEBUG("Pool size is {}", _pool.size());
  LOG_DEBUG("Choosen pool key is {}", index);

  const auto poolElement = _pool[index];

  Q_ASSERT(poolElement);

  if (!poolElement) {
    LOG_ERROR(Log::ErrorId::Generic, "Internal error! Invalid pool element!");
    LOG_PERF(0, "Exit");
    return false;
  }

  Q_ASSERT(poolElement->_queue);

  if (!poolElement->_queue) {
    LOG_ERROR(Log::ErrorId::Generic,
              "Internal error! Invalid pool element's queue!");
    LOG_PERF(0, "Exit");
    return false;
  }

  const auto compositeTask = [=]() {
    LOG_PERF(0, "Enter");
    if (task) {
      try {
        task();
      } catch (const std::exception &ex) {
        LOG_ERROR(Log::ErrorId::Generic, "Exception: \"{}\"", ex.what());
        errorTask(ex);
      }
    }

    if (finalTask) {
      finalTask();
    }
    LOG_PERF(0, "Exit");
  };

  static int taskIdCounter = 0;

  DispatcherRecord runRecord;
  runRecord._command = DispatcherCommand::Run;
  runRecord._task = compositeTask;
  runRecord._taskId = taskIdCounter++;

  LOG_DEBUG("Enqueing task with id {}", runRecord._taskId);

  // Counted before the push so that the task is never seen as finished before
  // it has been accounted for.
  poolElement->_pendingTasks.fetch_add(1);
  poolElement->_queue->push(runRecord);

  LOG_PERF(0, "Exit");

  return true;
}

bool Dispatcher::idle() const {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  for (const auto &v : _pool) {
    if (v && v->_pendingTasks.load() > 0) {
      LOG_PERF(0, "Exit");
      return false;
    }
  }

  LOG_PERF(0, "Exit");

  return true;
}

void Dispatcher::stopAll() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  DispatcherRecord quitRecord;
  quitRecord._command = DispatcherCommand::Quit;

  for (auto &v : _pool.values()) {
    Q_ASSERT(v);
    if (!v) {
      LOG_ERROR(Log::ErrorId::Generic, "Internal error! Invalid pool element!");
      continue;
    }

    Q_ASSERT(v->_queue);

    if (!v->_queue) {
      LOG_ERROR(Log::ErrorId::Generic,
                "Internal error! Invalid pool element's queue!");
      continue;
    }

    v->_queue->push(quitRecord);
  }

  _pool.clear();

  LOG_PERF(0, "Exit");
}

void Dispatcher::clearPool() {
  LOG_PERF(0, "Enter");

  LOCKGUARD(_mutex);

  for (auto &v : _pool.values()) {
    Q_ASSERT(v);
    if (!v) {
      LOG_ERROR(Log::ErrorId::Generic, "Internal error! Invalid pool element!");
      continue;
    }

    Q_ASSERT(v->_queue);

    if (!v->_queue) {
      LOG_ERROR(Log::ErrorId::Generic,
                "Internal error! Invalid pool element's queue!");
      continue;
    }

    const size_t removed = v->_queue->clear();
    v->_pendingTasks.fetch_sub(removed);
  }

  LOG_PERF(0, "Exit");
}

} // namespace Util
