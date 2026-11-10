// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OverlayWidget.h"

#include <QTextEdit>

namespace GuiManager::Widgets {

class MultilineTextInputOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit MultilineTextInputOverlayWidget(QWidget *parent);
  ~MultilineTextInputOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }
  void setPlaceholderText(const QString &text) const {
    _textEditInput->setPlaceholderText(text);
  }
  void setText(const QString &text) const {
    _textEditInput->setPlainText(text);
  }
  void focusAndSelectAll() const {
    _textEditInput->setFocus();
    _textEditInput->selectAll();
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
  QTextEdit *_textEditInput;
};

} // namespace GuiManager::Widgets
