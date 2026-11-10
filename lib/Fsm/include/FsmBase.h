// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "BlockingQueue.h"
#include "FsmState.h"
#include <FsmEvent.h>

#include <QMap>
#include <QObject>
#include <QString>
#include <QThread>
#include <QVector>

namespace Fsm {

class FsmBase : public QObject {
  Q_OBJECT

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
  virtual FsmState *
  firstState(const QMap<int, Fsm::FsmState *> &table) const = 0;

private:
  void chageState(int from, int to);
  void mainLoop();

private:
  QThread *_thread;
  FsmState *_currentState;
  QMap<int, FsmState *> _stateTable;
  QMap<int, QMap<int, bool>> _stateChangeTable;
  BlockingQueue<FsmEvent> _queueEvents;
};

} // namespace Fsm
