// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OpaqueComboBox.h"
#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <QCheckBox>
#include <QComboBox>
#include <QLineEdit>

namespace GuiManager::Widgets {

class AddWorktreeOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  // `branches` fills the branch dropdown, `activeBranch` is preselected in it.
  explicit AddWorktreeOverlayWidget(QWidget *parent,
                                    const QString &defaultPath = QString(),
                                    const QStringList &branches = {},
                                    const QString &activeBranch = QString());
  ~AddWorktreeOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

signals:
  void canceled();
  // When `createBranch` is true `branch` is the name of the new branch and
  // `baseBranch` is the branch it is created from. Otherwise `branch` is the
  // existing branch to check out and `baseBranch` is empty.
  void submitted(const QString &path, const QString &branch, bool createBranch,
                 const QString &baseBranch);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
  void onButtonBrowse();
  void onButtonCanceled();
  void onButtonSubmitted();
  void onCreateBranchToggled(bool checked);

private:
  void submit();

  PersistentPlaceholderLineEdit *_lineEditPath;
  PersistentPlaceholderLineEdit *_lineEditNewBranch;
  OpaqueComboBox *_comboBoxBranch;
  QCheckBox *_checkBoxCreateBranch;
};

} // namespace GuiManager::Widgets
