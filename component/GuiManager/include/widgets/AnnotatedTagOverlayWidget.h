#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QCheckBox>
#include <QLineEdit>
#include <QTextEdit>

namespace GuiManager::Widgets {

class AnnotatedTagOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit AnnotatedTagOverlayWidget(QWidget *parent);
  ~AnnotatedTagOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }
  void setTagNamePlaceholder(const QString &text) const;
  void setTagMessagePlaceholder(const QString &text) const;

signals:
  void canceled();
  void submitted(const QString &tagName, const QString &message,
                 bool isAnnotated);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();
  void onAnnotatedCheckBoxToggled(bool checked);

private:
  PersistentPlaceholderLineEdit *_lineEditTagName;
  QTextEdit *_textEditMessage;
  QCheckBox *_checkBoxAnnotated;
};

} // namespace GuiManager::Widgets
