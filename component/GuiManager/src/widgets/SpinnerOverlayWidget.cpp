#include "widgets/SpinnerOverlayWidget.h"

#include <Log.h>
#include <StylesheetManager.h>

#include <QColor>

#define THREAD_ID "SpinnerOverlayWidget"

#define ALPHA 0

namespace GuiManager::Widgets {

SpinnerOverlayWidget::SpinnerOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA, false),
      _configManager(ConfigManager::ConfigManager::getInstance()),
      _singleShotTimer(new QTimer(this)),
      _spinner(new WaitingSpinnerWidget(this)), _labelStatus(new QLabel(this)) {
  setGraphicsEffect(nullptr);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setAutoFillBackground(false);
  setBackgroundColor(QColor(0, 0, 0, 0));

  _singleShotTimer->setSingleShot(true);

  _labelStatus->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);

  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;

  QPalette palette = _labelStatus->palette();
  palette.setColor(QPalette::WindowText,
                   lightTheme ? QColor(38, 38, 38) : Qt::white);
  _labelStatus->setPalette(palette);
  _labelStatus->setVisible(false);

  _spinner->setColor(lightTheme ? QColor(44, 84, 150) : QColor(100, 200, 255));
  _spinner->setInnerRadius(20);
  _spinner->setLineLength(lightTheme ? 10 : 6);
  _spinner->setLineWidth(lightTheme ? 6 : 4);
  _spinner->setVisible(false);

  layout()->addWidget(_spinner);
  layout()->addWidget(_labelStatus);

  connect(_singleShotTimer, &QTimer::timeout, [this]() {
    _labelStatus->setVisible(true);
    _spinner->setVisible(true);
    _spinner->start();
  });
}

SpinnerOverlayWidget::~SpinnerOverlayWidget() {
  _singleShotTimer->stop();
  _spinner->stop();
}

void SpinnerOverlayWidget::startSpinning() {
  if (_spinner && !_spinner->isSpinning()) {
    const qlonglong timerTimeout = _configManager.getLong(
        ConfigManager::ConfigKey::GitRepoSpinnerOverlayTimerTimeout);
    _singleShotTimer->start(timerTimeout);
  }
}

void SpinnerOverlayWidget::stopSpinning() {
  if (_spinner && _spinner->isSpinning()) {
    _singleShotTimer->stop();
    _labelStatus->setVisible(false);
    _spinner->setVisible(false);
    _spinner->stop();
  }
}

} // namespace GuiManager::Widgets
