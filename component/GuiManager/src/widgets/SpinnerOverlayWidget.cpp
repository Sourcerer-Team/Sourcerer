// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/SpinnerOverlayWidget.h"

#include <algorithm>

#include <Log.h>
#include <StylesheetManager.h>

#include <QColor>
#include <QFontMetrics>
#include <QSizePolicy>
#include <QStringList>

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
  _labelStatus->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

  const QFontMetrics fontMetrics(_labelStatus->font());
  const QStringList reservedTexts = {
      tr("Creating log archive..."), tr("Uploading bug report..."),
      tr("Uploading... 100%"), QStringLiteral("100%")};

  int reservedWidth = 0;
  for (const QString &text : reservedTexts) {
    reservedWidth =
        std::max(reservedWidth, fontMetrics.horizontalAdvance(text));
  }

  _labelStatus->setMinimumWidth(reservedWidth);
  _labelStatus->setMaximumWidth(reservedWidth);
  _labelStatus->setMinimumHeight(fontMetrics.height());
  _labelStatus->setMaximumHeight(fontMetrics.height());

  const bool lightTheme =
      StylesheetManager::StylesheetManager::currentStylesheet() ==
      StylesheetManager::StylesheetKey::SourcererLight;

  QPalette palette = _labelStatus->palette();
  palette.setColor(QPalette::WindowText,
                   lightTheme ? QColor(38, 38, 38) : Qt::white);
  _labelStatus->setPalette(palette);
  _labelStatus->setVisible(false);

  const auto theme = StylesheetManager::StylesheetManager::currentStylesheet();
  const bool purpleTheme =
      theme == StylesheetManager::StylesheetKey::SourcererPurple;
  const bool blueTheme =
      theme == StylesheetManager::StylesheetKey::SourcererBlue;
  const bool monokaiTheme =
      theme == StylesheetManager::StylesheetKey::SourcererMonokai;
  const bool gruvboxTheme =
      theme == StylesheetManager::StylesheetKey::SourcererGruvbox;
  const bool solarizedTheme =
      theme == StylesheetManager::StylesheetKey::SourcererSolarized;
  const bool draculaTheme =
      theme == StylesheetManager::StylesheetKey::SourcererDracula;
  const bool ayuTheme = theme == StylesheetManager::StylesheetKey::SourcererAyu;
  _spinner->setColor(lightTheme       ? QColor(44, 84, 150)
                     : purpleTheme    ? QColor(143, 180, 224)
                     : blueTheme      ? QColor(81, 186, 229)
                     : monokaiTheme   ? QColor(166, 226, 46)
                     : gruvboxTheme   ? QColor(250, 189, 47)
                     : solarizedTheme ? QColor(38, 139, 210)
                     : draculaTheme   ? QColor(189, 147, 249)
                     : ayuTheme       ? QColor(255, 180, 84)
                                      : QColor(100, 200, 255));
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
  if (_spinner) {
    _singleShotTimer->stop();
    _labelStatus->setVisible(false);
    _spinner->setVisible(false);
    _spinner->stop();
  }
}

} // namespace GuiManager::Widgets
