// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class TextInputOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit TextInputOverlayWidget(QWidget *parent);
  ~TextInputOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }
  void setPlaceholderText(const QString &text) const {
    _lineEditInput->setPlaceholderText(text);
  }
  void setText(const QString &text) const { _lineEditInput->setText(text); }
  void focusAndSelectAll() const {
    _lineEditInput->setFocus();
    _lineEditInput->selectAll();
  }

signals:
  void canceled();
  void submitted(const QString &text);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  PersistentPlaceholderLineEdit *_lineEditInput;
};

} // namespace GuiManager::Widgets