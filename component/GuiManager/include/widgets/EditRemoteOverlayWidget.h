#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class EditRemoteOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit EditRemoteOverlayWidget(QWidget *parent);
  ~EditRemoteOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

  void setRemoteInfo(const QString &remoteName, const QString &fetchUrl,
                     const QString &pushUrl);
  void reset();

signals:
  void canceled();
  void submitted(const QString &remoteName, const QString &fetchUrl,
                 const QString &pushUrl);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  QLineEdit *_lineEditRemoteName; // Read-only display
  PersistentPlaceholderLineEdit *_lineEditFetchUrl;
  PersistentPlaceholderLineEdit *_lineEditPushUrl;

  QString _originalRemoteName;
};

} // namespace GuiManager::Widgets
