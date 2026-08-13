#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>
#include <NewsTracker.h>
#include <UpdateManager.h>

#include <QApplication>
#include <QByteArray>
#include <QCoreApplication>
#include <QtCore/Qt>

namespace {

void appendChromiumFlag(QByteArray &chromiumFlags, const QByteArray &flag) {
  if (chromiumFlags.contains(flag)) {
    return;
  }

  if (!chromiumFlags.isEmpty() && !chromiumFlags.endsWith(' ')) {
    chromiumFlags.append(' ');
  }
  chromiumFlags.append(flag);
}

QByteArray chromiumFlagsWithWebEngineLogging() {
  QByteArray chromiumFlags = qgetenv("QTWEBENGINE_CHROMIUM_FLAGS");
  return chromiumFlags;
}

struct WebEngineLoggingInitializer {
  WebEngineLoggingInitializer() {
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlagsWithWebEngineLogging());
  }
};

const WebEngineLoggingInitializer webEngineLoggingInitializer;

} // namespace

int main(int argc, char *argv[]) {
  QCoreApplication::setAttribute(Qt::AA_UseSoftwareOpenGL);
  QApplication app(argc, argv);

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addThread<NewsTracker::NewsTracker>();
  componentManager.addThread<UpdateManager::UpdateManager>();
  componentManager.startProcess<GuiManager::GuiManager>();
  componentManager.stopAll();

  return 0;
}
