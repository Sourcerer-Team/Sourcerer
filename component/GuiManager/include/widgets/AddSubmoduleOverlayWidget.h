#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class AddSubmoduleOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit AddSubmoduleOverlayWidget(QWidget *parent);
  ~AddSubmoduleOverlayWidget() override;

  void setNamePlaceholderText(const QString &text);

  [[nodiscard]] bool showing() const override { return true; }

signals:
  void canceled();
  void submitted(const QString &url, const QString &name,
                 const QString &branch);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  PersistentPlaceholderLineEdit *_lineEditUrl;
  PersistentPlaceholderLineEdit *_lineEditName;
  PersistentPlaceholderLineEdit *_lineEditBranch;
};

} // namespace GuiManager::Widgets