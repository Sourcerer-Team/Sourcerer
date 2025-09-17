#pragma once

#include <QObject>
#include <QThread>

#include <atomic>

namespace Util {

class DirectoryWatcher : public QObject {
  Q_OBJECT
  public:
    explicit DirectoryWatcher(const QString &path, QObject *parent = nullptr);
    ~DirectoryWatcher() override;

    bool startWatching();
    bool stopWatching();

signals:
    void directoryChanged(const QString &path);

private:
    void run();

private:
    QThread *_thread;
    std::atomic_bool _quit;
    QString _path;
};

}
