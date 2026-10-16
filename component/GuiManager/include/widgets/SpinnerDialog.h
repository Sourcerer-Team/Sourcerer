#pragma once

#include <WaitingSpinnerWidget.h>

#include <algorithm>

#include <QDialog>
#include <QFontMetrics>
#include <QLabel>
#include <QSizePolicy>
#include <QStringList>
#include <QVBoxLayout>

namespace GuiManager::Widgets {

class SpinnerDialog : public QDialog {
  Q_OBJECT
public:
  explicit SpinnerDialog(QWidget *parent = nullptr)
      : QDialog(parent), _spinner(new WaitingSpinnerWidget(this)),
        _labelStatus(new QLabel(this)) {
    setObjectName("spinnerDialog");
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint |
                   Qt::WindowStaysOnTopHint);
    setWindowModality(Qt::WindowModal);

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

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(32);
    layout->addWidget(_spinner);
    layout->addWidget(_labelStatus);

    resize(320, 96);

    _spinner->setColor(Qt::lightGray);
  }

  ~SpinnerDialog() override { _spinner->stop(); }

  void setText(const QString &t) const { _labelStatus->setText(t); }

  void startSpinning() const {
    if (_spinner && !_spinner->isSpinning())
      _spinner->start();
  }

  void stopSpinning() const {
    if (_spinner && _spinner->isSpinning()) {
      _spinner->stop();
    }
  }

private:
  WaitingSpinnerWidget *_spinner;
  QLabel *_labelStatus;
};

} // namespace GuiManager::Widgets
