#pragma once

#include "BlockingQueue.h"
#include "FsmState.h"
#include <FsmEvent.h>

#include <QMap>
#include <QString>
#include <QVector>

#include <thread>

namespace Fsm {

class FsmBase {
public:
  FsmBase();
  virtual ~FsmBase();

  virtual void start();
  virtual void stop();

  void send(const FsmEvent &event);

protected:
  virtual QString stateName(int state) const = 0;
  virtual QString eventName(int event) const = 0;
  virtual void initStateTable(QMap<int, FsmState *> &table) = 0;
  virtual void initChangeTable(QMap<int, QMap<int, bool>> &table) = 0;
  virtual FsmState* firstState(const QMap<int, Fsm::FsmState *> &table) const = 0;

private:
  void chageState(int from, int to);
  void mainLoop();

private:
  std::thread _thread;
  FsmState *_currentState;
  QMap<int, FsmState *> _stateTable;
  QMap<int, QMap<int, bool>> _stateChangeTable;
  BlockingQueue<FsmEvent> _queueEvents;
};

} // namespace Fsm
