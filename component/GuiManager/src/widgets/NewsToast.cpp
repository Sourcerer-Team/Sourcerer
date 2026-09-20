#include "widgets/NewsToast.h"

#include <StylesheetManager.h>

#include <Log.h>

#include <QColor>
#include <QDesktopServices>
#include <QHBoxLayout>
#include <QLabel>
#ifndef Q_OS_WIN
#include <QProcess>
#include <QProcessEnvironment>
#endif
#include <QPushButton>
#include <QSizePolicy>
#include <QUrl>
#include <QVBoxLayout>

#define THREAD_ID "NewsToast"

namespace GuiManager::Widgets {

#ifndef Q_OS_WIN
static void openUrlFromAppImage(const QUrl &url) {
  QProcess process;
  QProcessEnvironment env = QProcessEnvironment::systemEnvironment();

         // Unset AppImage environment variables so external processes load host system libraries
  env.remove("LD_LIBRARY_PATH");
  env.remove("QT_PLUGIN_PATH");
  env.remove("QML2_IMPORT_PATH");

  process.setProcessEnvironment(env);
  process.start("xdg-open", QStringList() << url.toString());
  process.startDetached(); // Allow xdg-open to run independently without blocking
}
#endif

NewsToast::NewsToast(const NewsTracker::Structs::NewsItem &news,
                     QWidget *parent)
    : ToastWidget(parent) {
  LOG_PERF(0, "Enter");

  setTitleText(news.title());

  const auto widget = new QWidget(this);
  widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  const auto widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setContentsMargins(8, 8, 8, 8);
  widgetLayout->setSpacing(8);

  if (news.isUpdate() && !news.version.trimmed().isEmpty()) {
    const auto versionLabel =
        new QLabel(QStringLiteral("Version: %1").arg(news.version), widget);
    versionLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                          Qt::TextSelectableByKeyboard);
    widgetLayout->addWidget(versionLabel);
  }

  const auto messageLabel = new QLabel(news.text, widget);
  messageLabel->setWordWrap(true);
  messageLabel->setTextInteractionFlags(Qt::TextSelectableByMouse |
                                        Qt::TextSelectableByKeyboard);
  widgetLayout->addWidget(messageLabel);
  widgetLayout->addStretch();

  const QString downloadUrl = news.downloadUrl();
  if (!downloadUrl.trimmed().isEmpty()) {
    const auto buttonRow = new QWidget(widget);
    const auto buttonLayout = new QHBoxLayout(buttonRow);
    buttonLayout->setContentsMargins(0, 0, 0, 0);
    buttonLayout->setSpacing(0);

    const auto downloadButton =
        new QPushButton(tr("Download update"), buttonRow);
    downloadButton->setCursor(Qt::PointingHandCursor);
    connect(downloadButton, &QPushButton::clicked, this,
            [downloadUrl]() {
#ifdef Q_OS_WIN
      QDesktopServices::openUrl(QUrl(downloadUrl));
#else
      openUrlFromAppImage(QUrl(downloadUrl));
#endif
            });

      buttonLayout->addWidget(downloadButton, 0, Qt::AlignLeft);
      buttonLayout->addStretch();
      widgetLayout->addWidget(buttonRow);
  }

  layout()->addWidget(widget);

  applyNewsColors(news);

  LOG_PERF(0, "Exit");
  }

  NewsToast::~NewsToast() {
    LOG_PERF(0, "Enter");
    LOG_PERF(0, "Exit");
  }

  void NewsToast::applyNewsColors(const NewsTracker::Structs::NewsItem &news) {
    LOG_PERF(0, "Enter");

    const bool lightTheme =
        StylesheetManager::StylesheetManager::currentStylesheet() ==
        StylesheetManager::StylesheetKey::SourcererLight;

    if (news.isUpdate()) {
      if (lightTheme) {
        applyToastColors(QColor("#E6F6EA"), QColor("#76A56F"),
                         QColor("#1F2C1F"));
      } else {
        applyToastColors(QColor("#21402B"), QColor("#3A7D44"),
                         QColor("#FFFFFF"));
      }
    } else {
      if (lightTheme) {
        applyToastColors(QColor("#E8F4FF"), QColor("#6B9CC7"),
                         QColor("#1F2A33"));
      } else {
        applyToastColors(QColor("#233245"), QColor("#3F6F9C"),
                         QColor("#FFFFFF"));
      }
    }

    LOG_PERF(0, "Exit");
  }

} // namespace GuiManager::Widgets
