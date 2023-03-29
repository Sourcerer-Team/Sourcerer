#pragma once

#include <Log.h>

#include <QDateTime>
#include <QVariant>
#include <QVector>

#include <stdexcept>

namespace Fsm {

struct FsmEvent {
  FsmEvent(int type)
      : _type(type), _startTime(QDateTime::currentMSecsSinceEpoch()) {}

  virtual ~FsmEvent() {}

  int type() const { return _type; }

  long long int startTime() const { return _startTime; }

  void addParam(const QVariant &param) { _params += param; }

  template <typename C> C nextParam() {
    if (_params.length() == 0)
      throw std::logic_error("Parameters are drained.");

    const QVariant &result = _params.first();
    if (!result.canConvert<C>())
      throw std::logic_error("Error casting std::any.");

    _params.pop_front();

    return result.value<C>();
  }

private:
  int _type;
  QVector<QVariant> _params;
  long long int _startTime;
};

} // namespace Fsm
