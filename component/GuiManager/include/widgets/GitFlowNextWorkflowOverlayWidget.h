// MIT License
// Copyright (c) 2026 Sourcerer

#pragma once

#include "OpaqueComboBox.h"
#include "OpaqueLabel.h"
#include "OpaquePushButton.h"
#include "OverlayWidget.h"
#include "PersistentPlaceholderLineEdit.h"

#include <GitFlowNextWorkflow.h>

#include <QCheckBox>

class QVBoxLayout;

namespace GuiManager::Widgets {

// Edits a custom GitFlow-Next workflow: its name, base branches and topic
// branch types with their git-flow-next options.
class GitFlowNextWorkflowOverlayWidget final : public OverlayWidget {
  Q_OBJECT
public:
  GitFlowNextWorkflowOverlayWidget(
      QWidget *parent, const GitRepo::GitFlowNextWorkflow &workflow);
  ~GitFlowNextWorkflowOverlayWidget() override;

  [[nodiscard]] bool showing() const override { return true; }

  void focusName();

signals:
  void canceled();
  // Carries the edited workflow with the id it was opened with.
  void submitted(const GitRepo::GitFlowNextWorkflow &workflow);

protected:
  bool eventFilter(QObject *obj, QEvent *event) override;

private:
  struct BaseRow {
    QWidget *row = nullptr;
    PersistentPlaceholderLineEdit *name = nullptr;
    OpaqueComboBox *parent = nullptr;
    QCheckBox *autoUpdate = nullptr;
    OpaqueComboBox *downstream = nullptr;
  };

  struct TopicRow {
    QWidget *row = nullptr;
    PersistentPlaceholderLineEdit *name = nullptr;
    PersistentPlaceholderLineEdit *prefix = nullptr;
    OpaqueComboBox *parent = nullptr;
    OpaqueComboBox *startPoint = nullptr;
    OpaqueComboBox *upstream = nullptr;
    OpaqueComboBox *downstream = nullptr;
    QCheckBox *tag = nullptr;
    PersistentPlaceholderLineEdit *tagPrefix = nullptr;
    QCheckBox *keep = nullptr;
  };

  void addBaseRow(const GitRepo::GitFlowNextBranch &branch);
  void addTopicRow(const GitRepo::GitFlowNextBranch &branch);
  QWidget *headerRow(const QStringList &titles, const QVector<int> &widths);
  PersistentPlaceholderLineEdit *lineEdit(QWidget *parent,
                                          const QString &placeholder,
                                          const QString &text, int width);
  OpaqueComboBox *comboBox(QWidget *parent, int width);
  QCheckBox *checkBox(QWidget *parent, const QString &text, bool checked,
                      int width);
  OpaquePushButton *removeButton(QWidget *parent);

  QStringList baseNames() const;
  void refreshBaseChoices();
  [[nodiscard]] GitRepo::GitFlowNextWorkflow collect() const;
  void validate();
  void submit();
  void watch(QWidget *widget);

  GitRepo::GitFlowNextWorkflow _workflow;
  PersistentPlaceholderLineEdit *_lineEditName;
  QVBoxLayout *_baseRowsLayout;
  QVBoxLayout *_topicRowsLayout;
  QVector<BaseRow> _baseRows;
  QVector<TopicRow> _topicRows;
  OpaqueLabel *_labelProblems;
  OpaquePushButton *_pushButtonSubmit;
  bool _refreshing = false;
};

} // namespace GuiManager::Widgets
