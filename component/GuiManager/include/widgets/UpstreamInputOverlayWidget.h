#pragma once

#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"
#include "OpaqueComboBox.h"

#include <GitRepo.h>

#include <QLineEdit>

namespace GuiManager::Widgets {

class UpstreamInputOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  explicit UpstreamInputOverlayWidget(QWidget *parent);
  ~UpstreamInputOverlayWidget() override;

  bool showing() const override { return true; }
  void setBranchPlaceholderText(const QString &text) const {
    _lineEditBranchInput->setPlaceholderText(text);
  }
  void setRemotes(const QVector<GitRepo::RemoteInfo> &remotes);
  void setRemoteName(const QString &remoteName) const;
  void setRemoteBranchName(const QString &branchName) const {
    _lineEditBranchInput->setText(branchName);
  }
  void selectAllBranchText() const {
    _lineEditBranchInput->selectAll();
    _lineEditBranchInput->setFocus();
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
  OpaqueComboBox *_comboBoxRemoteInput;
  PersistentPlaceholderLineEdit *_lineEditBranchInput;
};

} // namespace GuiManager::Widgets