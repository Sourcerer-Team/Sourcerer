#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QLineEdit>

namespace GuiManager::Widgets {

class UpstreamInputOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit UpstreamInputOverlayWidget(QWidget *parent);
  ~UpstreamInputOverlayWidget() override;

  bool showing() const override { return true; }
  void setRemotePlaceholderText(const QString &text) const {
    _lineEditRemoteInput->setPlaceholderText(text);
  }
  void setBranchPlaceholderText(const QString &text) const {
    _lineEditBranchInput->setPlaceholderText(text);
  }
  void setRemoteName(const QString &remoteName) const {
    _lineEditRemoteInput->setText(remoteName);
  }
  void setRemoteBranchName(const QString &branchName) const {
    _lineEditBranchInput->setText(branchName);
  }
  void selectAllRemoteText() const {
    _lineEditRemoteInput->selectAll();
    _lineEditRemoteInput->setFocus();
  }

signals:
  void canceled();
  void submitted(const QString &remoteText, const QString &branchText);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonCanceled();
  void onButtonSubmitted();

private:
  PersistentPlaceholderLineEdit *_lineEditRemoteInput;
  PersistentPlaceholderLineEdit *_lineEditBranchInput;
};

} // namespace GuiManager::Widgets