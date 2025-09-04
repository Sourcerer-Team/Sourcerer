#pragma once

#include <BlockingQueue.h>
#include <LockGuard.h>

#include <QObject>
#include <QThread>

#include <climits>
#include <functional>
#include <map>


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
  QThread* _thread = nullptr;
};

class Dispatcher : public QObject
{
  Q_OBJECT

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
  std::map<size_t, QThread*> _threads;
};

} // namespace Util
