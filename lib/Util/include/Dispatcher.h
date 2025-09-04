#pragma once

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <climits>
#include <functional>
#include <map>
#include <thread>

namespace Util
{

enum class DispatcherCommand
{
  Unknown = 0,

  Run,
  Quit,

  last
};

struct DispatcherRecord
{
  DispatcherCommand _command = DispatcherCommand::Quit;
  std::function<void()> _task;
};

struct DispatcherPoolElement
{
  BlockingQueue<DispatcherRecord> _queue;
  std::thread _thread;
};

class Dispatcher
{
  public:
  Dispatcher(int coresNum = INT_MAX);
  virtual ~Dispatcher();

  void dispatch(std::function<void()> task);

  static void dispatchToMainThread(std::function<void()> callback);

  private:
  void processor(BlockingQueue<DispatcherRecord> &queue);

  private:
  Util::Mutex _mutex;
  std::map<size_t, DispatcherPoolElement> _pool;
  std::map<size_t, std::thread> _threads;
};

} // namespace Util
