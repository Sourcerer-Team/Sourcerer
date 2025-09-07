#pragma once

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <QThread>

#include <climits>
#include <functional>
#include <map>

namespace Util {

enum class DispatcherCommand {
  Unknown = 0,

  Run,
  Quit,

  last
};

struct DispatcherRecord {
  DispatcherCommand _command = DispatcherCommand::Quit;
  std::function<void()> _task;
};

struct DispatcherPoolElement {
  BlockingQueue<DispatcherRecord> _queue;
  QThread *_thread = nullptr;
};

class Dispatcher : public QObject {
  Q_OBJECT

public:
  Dispatcher(int coresNum = INT_MAX);
  ~Dispatcher() override;

  bool dispatch(
      const std::function<void()> &task,
      const std::function<void()> &finalTask = nullptr,
      const std::function<void(const std::exception &)> &errorTask = nullptr);

  static bool dispatchToMainThread(const std::function<void()> &callback);

  void joinAll();

private:
  static void processor(BlockingQueue<DispatcherRecord> &queue);

private:
  Util::Mutex _mutex;
  std::map<size_t, DispatcherPoolElement> _pool;
  std::map<size_t, QThread *> _threads;
};

} // namespace Util
