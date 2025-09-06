#pragma once

#include <WaitingSpinnerWidget.h>

#include <QDialog>
#include <QLabel>
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

    _labelStatus->setAlignment(Qt::AlignHCenter);

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
