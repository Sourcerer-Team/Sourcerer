
#include "Dispatch.h"

#include <QCoreApplication>
#include <QTimer>

namespace Dispatch {

void dispatchToMainThread(std::function<void()> callback) {
  QTimer *timer = new QTimer();
  timer->moveToThread(qApp->thread());
  timer->setSingleShot(true);

  QObject::connect(timer, &QTimer::timeout, [=]() {
    // main thread
    callback();
    timer->deleteLater();
  });

  QMetaObject::invokeMethod(timer, "start", Qt::QueuedConnection,
                            Q_ARG(int, 0));
}

} // namespace Dispatch
