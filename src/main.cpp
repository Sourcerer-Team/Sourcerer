#include <ComponentManager.h>
#include <GitRepoManager.h>
#include <GuiManager.h>
#include <NewsTracker.h>
#include <UpdateManager.h>

#include <QApplication>
#include <QByteArray>

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
  appendChromiumFlag(chromiumFlags, QByteArrayLiteral("--enable-logging=stderr"));
  appendChromiumFlag(chromiumFlags, QByteArrayLiteral("--v=2"));
  appendChromiumFlag(chromiumFlags, QByteArrayLiteral("--log-level=0"));
  appendChromiumFlag(
      chromiumFlags,
      QByteArrayLiteral(
          "--vmodule=*/webauth*=3,*/device*=3,*/authenticator*=3,*/fido*=3"));

  return chromiumFlags;
}

QByteArray qtLoggingRulesWithWebEngineLogging() {
  QByteArray loggingRules = qgetenv("QT_LOGGING_RULES");
  const QByteArray webEngineRules =
      QByteArrayLiteral("qt.webengine*.debug=true;qt.webengine*.info=true");

  if (!loggingRules.contains(webEngineRules)) {
    if (!loggingRules.isEmpty() && !loggingRules.endsWith(';')) {
      loggingRules.append(';');
    }
    loggingRules.append(webEngineRules);
  }

  return loggingRules;
}

struct WebEngineLoggingInitializer {
  WebEngineLoggingInitializer() {
    qputenv("QT_LOGGING_RULES", qtLoggingRulesWithWebEngineLogging());
    qputenv("QTWEBENGINE_CHROMIUM_FLAGS", chromiumFlagsWithWebEngineLogging());
  }
};

const WebEngineLoggingInitializer webEngineLoggingInitializer;

} // namespace

int main(int argc, char *argv[]) {
  QApplication app(argc, argv);

  ComponentManager::ComponentManager componentManager;
  componentManager.addThread<GitRepoManager::GitRepoManager>();
  componentManager.addThread<NewsTracker::NewsTracker>();
  componentManager.addThread<UpdateManager::UpdateManager>();
  componentManager.startProcess<GuiManager::GuiManager>();
  componentManager.stopAll();

  return 0;
}
