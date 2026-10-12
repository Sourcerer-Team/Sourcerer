
#include "widgets/InfoToast.h"

#include <StylesheetManager.h>

#include <Log.h>

#include <QLabel>
#include <QTimer>
#include <QVBoxLayout>

#include <Uuid.h>

#define THREAD_ID "InfoToast"

namespace GuiManager::Widgets {

InfoToast::InfoToast(const QString &message, const QString &title,
                     bool stayForever, QWidget *parent)
    : ToastWidget{parent},
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  setTitleText(title);

  const auto widget = new QWidget(this);
  widget->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);

  const auto messageLabel = new QLabel(message, widget);
  messageLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);
  messageLabel->setWordWrap(true);

  const auto widgetLayout = new QVBoxLayout(widget);
  widgetLayout->setContentsMargins(8, 8, 8, 8);
  widgetLayout->setSpacing(0);
  widgetLayout->addWidget(messageLabel);

  layout()->addWidget(widget);

  const auto theme = StylesheetManager::StylesheetManager::currentStylesheet();
  if (theme == StylesheetManager::StylesheetKey::SourcererLight) {
    applyToastColors(QColor("#FFF4E2"), QColor("#E3AE5A"), QColor("#2E2418"));
  } else if (theme == StylesheetManager::StylesheetKey::SourcererPurple) {
    applyToastColors(QColor("#4A4656"), QColor("#B8860B"), QColor("#FFFFFF"));
  } else if (theme == StylesheetManager::StylesheetKey::SourcererBlue) {
    applyToastColors(QColor("#2D3B55"), QColor("#B8860B"), QColor("#FFFFFF"));
  } else if (theme == StylesheetManager::StylesheetKey::SourcererMonokai) {
    applyToastColors(QColor("#3E3D32"), QColor("#B8860B"), QColor("#FFFFFF"));
  } else if (theme == StylesheetManager::StylesheetKey::SourcererGruvbox) {
    applyToastColors(QColor("#3C3836"), QColor("#B8860B"), QColor("#FBF1C7"));
  } else {
    applyToastColors(QColor("#505050"), QColor("#B8860B"), QColor("#FFFFFF"));
  }

  if (!stayForever) {
    const qlonglong displayTime =
        _configManager.getLong(ConfigManager::ConfigKey::InfoToastDisplayTime);

    QTimer::singleShot(displayTime, this, [this]() { stop(); });
  }

  LOG_PERF(0, "Exit");
}

InfoToast::~InfoToast() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void InfoToast::stop() {
  LOG_PERF(0, "Enter");

  ToastWidget::stop();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
