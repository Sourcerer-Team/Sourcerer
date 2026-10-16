#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class AddTargetOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit AddTargetOverlayWidget(QWidget *parent);
  ~AddTargetOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

  void setRemoteName(const QString &remoteName);
  void reset();

signals:
  void canceled();
  void submitted(const QString &targetName, const QString &fetchUrl,
                 const QString &pushUrl);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  QLineEdit *_lineEditRemoteName; // Read-only display
  PersistentPlaceholderLineEdit *_lineEditTargetName;
  PersistentPlaceholderLineEdit *_lineEditFetchUrl;
  PersistentPlaceholderLineEdit *_lineEditPushUrl;
};

} // namespace GuiManager::Widgets
