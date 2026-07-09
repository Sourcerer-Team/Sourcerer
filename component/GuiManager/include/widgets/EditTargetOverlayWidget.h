#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class EditTargetOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit EditTargetOverlayWidget(QWidget *parent);
  ~EditTargetOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

  void setTargetInfo(const QString &remoteName, const QString &targetName,
                     const QString &fetchUrl, const QString &pushUrl);
  void reset();

signals:
  void canceled();
  void submitted(const QString &fetchUrl, const QString &pushUrl);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  QLineEdit *_lineEditRemoteName; // Read-only display
  QLineEdit *_lineEditTargetName; // Read-only display
  PersistentPlaceholderLineEdit *_lineEditFetchUrl;
  PersistentPlaceholderLineEdit *_lineEditPushUrl;
};

} // namespace GuiManager::Widgets
