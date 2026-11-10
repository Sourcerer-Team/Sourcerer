// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class AddRemoteOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit AddRemoteOverlayWidget(QWidget *parent);
  ~AddRemoteOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

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
  PersistentPlaceholderLineEdit *_lineEditRemoteName;
  PersistentPlaceholderLineEdit *_lineEditFetchUrl;
  PersistentPlaceholderLineEdit *_lineEditPushUrl;
};

} // namespace GuiManager::Widgets
