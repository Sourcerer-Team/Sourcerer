#include "widgets/GitFlowNextWorkflowOverlayWidget.h"
#include "widgets/OpaqueGroupBox.h"

#include <Log.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScrollArea>
#include <QVBoxLayout>

#define THREAD_ID "GitFlowNextWorkflowOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

namespace {

const QColor kPlaceholderColor = QColor::fromString("#969696");
constexpr int kRowHeight = 24;
constexpr int kSpacing = 6;
constexpr int kRemoveWidth = 24;

// Column widths of the base branch rows.
const QVector<int> kBaseWidths = {150, 130, 150, 90, kRemoveWidth};
// Column widths of the topic branch rows.
const QVector<int> kTopicWidths = {100, 100, 110, 110, 80,
                                   80,  48,  60,  56,  kRemoveWidth};

const QStringList kUpstreamStrategies = {QStringLiteral("merge"),
                                         QStringLiteral("rebase"),
                                         QStringLiteral("squash")};
const QStringList kDownstreamStrategies = {QStringLiteral("merge"),
                                           QStringLiteral("rebase")};

void selectData(OpaqueComboBox *comboBox, const QString &value) {
  const int index = comboBox->findData(value);
  comboBox->setCurrentIndex(index >= 0 ? index : 0);
}

int rowsWidth(const QVector<int> &widths) {
  int width = 0;
  for (const int column : widths) {
    width += column;
  }
  return width + kSpacing * (int(widths.size()) - 1);
}

} // namespace

GitFlowNextWorkflowOverlayWidget::GitFlowNextWorkflowOverlayWidget(
    QWidget *parent, const GitRepo::GitFlowNextWorkflow &workflow)
    : OverlayWidget(parent, ALPHA), _workflow(workflow),
      _lineEditName(new PersistentPlaceholderLineEdit(kPlaceholderColor, this)),
      _baseRowsLayout(new QVBoxLayout()), _topicRowsLayout(new QVBoxLayout()),
      _labelProblems(new OpaqueLabel(overlayBackgroundColor(), this)),
      _pushButtonSubmit(new OpaquePushButton(overlayBackgroundColor(), this)) {
  LOG_PERF(0, "Enter");

  const int contentWidth = rowsWidth(kTopicWidths);

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  groupBox->setMaximumWidth(contentWidth + 2 * 6 + 2 * 16 + 24);
  groupBox->setMaximumHeight(560);

  const auto title = new OpaqueLabel(overlayBackgroundColor(), groupBox);
  title->setText(workflow.id.isEmpty() ? tr("New custom workflow")
                                       : tr("Edit custom workflow"));
  QFont titleFont = title->font();
  titleFont.setBold(true);
  title->setFont(titleFont);

  _lineEditName->setParent(groupBox);
  _lineEditName->setObjectName("lineEditGitFlowNextWorkflowOverlayName");
  _lineEditName->setPlaceholderText(tr("Workflow name"));
  _lineEditName->setText(workflow.name);
  _lineEditName->setMinimumHeight(kRowHeight);
  // Centered like in the other overlays, where the persistent placeholder is
  // drawn at the same place as Qt's.
  _lineEditName->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  watch(_lineEditName);
  connect(_lineEditName, &QLineEdit::textChanged, this,
          &GitFlowNextWorkflowOverlayWidget::validate);

  // The rows can be wider and taller than the window, so they scroll.
  const auto scrollArea = new QScrollArea(groupBox);
  scrollArea->setWidgetResizable(true);
  scrollArea->setFrameShape(QFrame::NoFrame);
  scrollArea->setMinimumHeight(220);
  scrollArea->setAutoFillBackground(true);
  QPalette scrollPalette = scrollArea->palette();
  scrollPalette.setColor(QPalette::Window, overlayBackgroundColor());
  scrollArea->setPalette(scrollPalette);

  const auto content = new QWidget();
  content->setAutoFillBackground(true);
  content->setPalette(scrollPalette);
  const auto contentLayout = new QVBoxLayout(content);
  contentLayout->setContentsMargins(0, 0, 0, 0);
  contentLayout->setSpacing(kSpacing);

  const auto sectionLabel = [&](const QString &text) {
    const auto label = new OpaqueLabel(overlayBackgroundColor(), content);
    label->setText(text);
    QFont font = label->font();
    font.setBold(true);
    label->setFont(font);
    return label;
  };

  const auto addButton = [&](const QString &text) {
    const auto button = new OpaquePushButton(overlayBackgroundColor(), content);
    button->setText(text);
    button->setFlat(false);
    button->setMaximumHeight(kRowHeight + 4);
    button->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    return button;
  };

  _baseRowsLayout->setContentsMargins(0, 0, 0, 0);
  _baseRowsLayout->setSpacing(kSpacing);
  _topicRowsLayout->setContentsMargins(0, 0, 0, 0);
  _topicRowsLayout->setSpacing(kSpacing);

  contentLayout->addWidget(sectionLabel(tr("Base branches")));
  contentLayout->addWidget(headerRow(
      {tr("Branch"), tr("Parent"), tr("Keep up to date"), tr("Update by")},
      kBaseWidths));
  contentLayout->addLayout(_baseRowsLayout);
  const auto addBaseButton = addButton(tr("+ Base branch"));
  contentLayout->addWidget(addBaseButton);

  contentLayout->addSpacing(kSpacing);
  contentLayout->addWidget(sectionLabel(tr("Topic branches")));
  contentLayout->addWidget(
      headerRow({tr("Type"), tr("Prefix"), tr("Parent"), tr("Starts from"),
                 tr("Finish by"), tr("Update by"), tr("Tag"), tr("Tag prefix"),
                 tr("Keep")},
                kTopicWidths));
  contentLayout->addLayout(_topicRowsLayout);
  const auto addTopicButton = addButton(tr("+ Topic branch type"));
  contentLayout->addWidget(addTopicButton);
  contentLayout->addStretch(1);

  scrollArea->setWidget(content);

  _labelProblems->setParent(groupBox);
  _labelProblems->setWordWrap(true);
  QPalette problemsPalette = _labelProblems->palette();
  problemsPalette.setColor(QPalette::WindowText, QColor(0xe0, 0x9a, 0x2a));
  _labelProblems->setPalette(problemsPalette);

  _pushButtonSubmit->setParent(groupBox);
  _pushButtonSubmit->setObjectName("pushButtonTextOverlayWidgetInputSubmit");
  _pushButtonSubmit->setMaximumWidth(32);
  _pushButtonSubmit->setMaximumHeight(32);
  _pushButtonSubmit->setIcon(overlayButtonIcon(":icons/ok.png"));
  _pushButtonSubmit->setToolTip(tr("Save (Ctrl+Enter)"));
  _pushButtonSubmit->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonCancel->setObjectName("pushButtonTextOverlayWidgetInputCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(overlayButtonIcon(":icons/cancel.png"));
  pushButtonCancel->setToolTip(tr("Cancel (Esc)"));
  pushButtonCancel->setFlat(false);

  const auto nameLayout = new QHBoxLayout();
  nameLayout->setContentsMargins(0, 0, 0, 0);
  nameLayout->setSpacing(kSpacing);
  nameLayout->addWidget(_lineEditName, 1);
  nameLayout->addWidget(_pushButtonSubmit);
  nameLayout->addWidget(pushButtonCancel);

  const auto groupLayout = new QVBoxLayout(groupBox);
  groupLayout->setContentsMargins(6, 6, 6, 6);
  groupLayout->setSpacing(kSpacing);
  groupLayout->addWidget(title);
  groupLayout->addLayout(nameLayout);
  groupLayout->addWidget(scrollArea, 1);
  groupLayout->addWidget(_labelProblems);

  getLayout()->addWidget(groupBox);

  connect(addBaseButton, &QPushButton::clicked, this, [this]() {
    addBaseRow({});
    refreshBaseChoices();
    _baseRows.last().name->setFocus();
    validate();
  });
  connect(addTopicButton, &QPushButton::clicked, this, [this]() {
    GitRepo::GitFlowNextBranch branch;
    branch.topic = true;
    addTopicRow(branch);
    refreshBaseChoices();
    _topicRows.last().name->setFocus();
    validate();
  });
  connect(_pushButtonSubmit, &QPushButton::clicked, this,
          &GitFlowNextWorkflowOverlayWidget::submit);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &GitFlowNextWorkflowOverlayWidget::canceled);

  for (const GitRepo::GitFlowNextBranch &branch : workflow.branches) {
    if (branch.topic) {
      addTopicRow(branch);
    } else {
      addBaseRow(branch);
    }
  }
  refreshBaseChoices();
  validate();

  LOG_PERF(0, "Exit");
}

GitFlowNextWorkflowOverlayWidget::~GitFlowNextWorkflowOverlayWidget() = default;

void GitFlowNextWorkflowOverlayWidget::focusName() {
  _lineEditName->setFocus();
  _lineEditName->selectAll();
}

QWidget *
GitFlowNextWorkflowOverlayWidget::headerRow(const QStringList &titles,
                                            const QVector<int> &widths) {
  const auto row = new QWidget();
  const auto layout = new QHBoxLayout(row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(kSpacing);
  for (int i = 0; i < widths.size(); ++i) {
    const auto label = new OpaqueLabel(overlayBackgroundColor(), row);
    label->setText(titles.value(i));
    label->setFixedWidth(widths.at(i));
    QPalette palette = label->palette();
    palette.setColor(QPalette::WindowText,
                     palette.color(QPalette::PlaceholderText));
    label->setPalette(palette);
    layout->addWidget(label);
  }
  layout->addStretch(1);
  return row;
}

PersistentPlaceholderLineEdit *GitFlowNextWorkflowOverlayWidget::lineEdit(
    QWidget *parent, const QString &placeholder, const QString &text,
    const int width) {
  const auto edit =
      new PersistentPlaceholderLineEdit(kPlaceholderColor, parent);
  edit->setPlaceholderText(placeholder);
  edit->setText(text);
  edit->setFixedSize(width, kRowHeight);
  edit->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  watch(edit);
  connect(edit, &QLineEdit::textChanged, this,
          &GitFlowNextWorkflowOverlayWidget::validate);
  return edit;
}

OpaqueComboBox *GitFlowNextWorkflowOverlayWidget::comboBox(QWidget *parent,
                                                           const int width) {
  const auto combo = new OpaqueComboBox(overlayBackgroundColor(), parent);
  combo->setFixedSize(width, kRowHeight);
  watch(combo);
  connect(combo, &QComboBox::currentIndexChanged, this, [this]() {
    if (!_refreshing) {
      validate();
    }
  });
  return combo;
}

QCheckBox *GitFlowNextWorkflowOverlayWidget::checkBox(QWidget *parent,
                                                      const QString &text,
                                                      const bool checked,
                                                      const int width) {
  const auto box = new QCheckBox(text, parent);
  box->setChecked(checked);
  box->setFixedWidth(width);
  watch(box);
  connect(box, &QCheckBox::toggled, this,
          &GitFlowNextWorkflowOverlayWidget::validate);
  return box;
}

OpaquePushButton *
GitFlowNextWorkflowOverlayWidget::removeButton(QWidget *parent) {
  const auto button = new OpaquePushButton(overlayBackgroundColor(), parent);
  button->setText(QStringLiteral("−"));
  button->setToolTip(tr("Remove"));
  button->setFlat(false);
  button->setFixedSize(kRemoveWidth, kRowHeight);
  return button;
}

void GitFlowNextWorkflowOverlayWidget::addBaseRow(
    const GitRepo::GitFlowNextBranch &branch) {
  BaseRow row;
  row.row = new QWidget();
  const auto layout = new QHBoxLayout(row.row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(kSpacing);

  row.name = lineEdit(row.row, tr("e.g. develop"), branch.name, kBaseWidths[0]);
  row.parent = comboBox(row.row, kBaseWidths[1]);
  row.parent->setProperty("selected", branch.parent);
  row.autoUpdate =
      checkBox(row.row, tr("from parent"), branch.autoUpdate, kBaseWidths[2]);
  row.autoUpdate->setToolTip(
      tr("Update this branch from its parent after a topic branch was "
         "finished into the parent (autoUpdate)."));
  row.downstream = comboBox(row.row, kBaseWidths[3]);
  for (const QString &strategy : kDownstreamStrategies) {
    row.downstream->addItem(strategy, strategy);
  }
  selectData(row.downstream, branch.downstreamStrategy);
  const auto remove = removeButton(row.row);

  layout->addWidget(row.name);
  layout->addWidget(row.parent);
  layout->addWidget(row.autoUpdate);
  layout->addWidget(row.downstream);
  layout->addWidget(remove);
  layout->addStretch(1);
  _baseRowsLayout->addWidget(row.row);

  connect(row.name, &QLineEdit::textChanged, this,
          &GitFlowNextWorkflowOverlayWidget::refreshBaseChoices);
  QWidget *rowWidget = row.row;
  connect(remove, &QPushButton::clicked, this, [this, rowWidget]() {
    _baseRows.erase(std::remove_if(_baseRows.begin(), _baseRows.end(),
                                   [&](const BaseRow &candidate) {
                                     return candidate.row == rowWidget;
                                   }),
                    _baseRows.end());
    rowWidget->deleteLater();
    refreshBaseChoices();
    validate();
  });

  _baseRows.append(row);
}

void GitFlowNextWorkflowOverlayWidget::addTopicRow(
    const GitRepo::GitFlowNextBranch &branch) {
  TopicRow row;
  row.row = new QWidget();
  const auto layout = new QHBoxLayout(row.row);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(kSpacing);

  row.name =
      lineEdit(row.row, tr("e.g. feature"), branch.name, kTopicWidths[0]);
  row.prefix =
      lineEdit(row.row, tr("feature/"), branch.prefix, kTopicWidths[1]);
  row.parent = comboBox(row.row, kTopicWidths[2]);
  row.parent->setProperty("selected", branch.parent);
  row.startPoint = comboBox(row.row, kTopicWidths[3]);
  row.startPoint->setProperty("selected", branch.startPoint);
  row.upstream = comboBox(row.row, kTopicWidths[4]);
  for (const QString &strategy : kUpstreamStrategies) {
    row.upstream->addItem(strategy, strategy);
  }
  selectData(row.upstream, branch.upstreamStrategy);
  row.downstream = comboBox(row.row, kTopicWidths[5]);
  for (const QString &strategy : kDownstreamStrategies) {
    row.downstream->addItem(strategy, strategy);
  }
  selectData(row.downstream, branch.downstreamStrategy);
  row.tag = checkBox(row.row, QString(), branch.tag, kTopicWidths[6]);
  row.tag->setToolTip(tr("Tag the parent when a branch is finished."));
  row.tagPrefix =
      lineEdit(row.row, tr("e.g. v"), branch.tagPrefix, kTopicWidths[7]);
  row.keep = checkBox(row.row, QString(), branch.keepOnFinish, kTopicWidths[8]);
  row.keep->setToolTip(tr("Keep the branch after it was finished."));
  const auto remove = removeButton(row.row);

  layout->addWidget(row.name);
  layout->addWidget(row.prefix);
  layout->addWidget(row.parent);
  layout->addWidget(row.startPoint);
  layout->addWidget(row.upstream);
  layout->addWidget(row.downstream);
  layout->addWidget(row.tag);
  layout->addWidget(row.tagPrefix);
  layout->addWidget(row.keep);
  layout->addWidget(remove);
  layout->addStretch(1);
  _topicRowsLayout->addWidget(row.row);

  // Suggest the usual prefix while the prefix hasn't been edited.
  PersistentPlaceholderLineEdit *prefix = row.prefix;
  connect(row.name, &QLineEdit::textEdited, this,
          [prefix](const QString &name) {
            const QString suggested = prefix->property("suggested").toString();
            if (prefix->text().isEmpty() || prefix->text() == suggested) {
              const QString value =
                  name.isEmpty() ? QString() : name + QLatin1Char('/');
              prefix->setText(value);
              prefix->setProperty("suggested", value);
            }
          });
  QCheckBox *tag = row.tag;
  PersistentPlaceholderLineEdit *tagPrefix = row.tagPrefix;
  tagPrefix->setEnabled(tag->isChecked());
  connect(tag, &QCheckBox::toggled, tagPrefix, &QWidget::setEnabled);

  QWidget *rowWidget = row.row;
  connect(remove, &QPushButton::clicked, this, [this, rowWidget]() {
    _topicRows.erase(std::remove_if(_topicRows.begin(), _topicRows.end(),
                                    [&](const TopicRow &candidate) {
                                      return candidate.row == rowWidget;
                                    }),
                     _topicRows.end());
    rowWidget->deleteLater();
    validate();
  });

  _topicRows.append(row);
}

QStringList GitFlowNextWorkflowOverlayWidget::baseNames() const {
  QStringList names;
  for (const BaseRow &row : _baseRows) {
    const QString name = row.name->text().trimmed();
    if (!name.isEmpty() && !names.contains(name)) {
      names.append(name);
    }
  }
  return names;
}

// The parent and start point choices are the base branch names, which change
// while editing. The selection is kept by name.
void GitFlowNextWorkflowOverlayWidget::refreshBaseChoices() {
  _refreshing = true;
  const QStringList names = baseNames();

  const auto refill = [&](OpaqueComboBox *combo, const QString &emptyText,
                          const QString &exclude) {
    const QString selected = combo->count() > 0
                                 ? combo->currentData().toString()
                                 : combo->property("selected").toString();
    combo->clear();
    if (!emptyText.isEmpty()) {
      combo->addItem(emptyText, QString());
    }
    for (const QString &name : names) {
      if (name != exclude) {
        combo->addItem(name, name);
      }
    }
    // Keep a selection that no longer matches, so validation can point at it.
    if (!selected.isEmpty() && combo->findData(selected) < 0) {
      combo->addItem(selected, selected);
    }
    selectData(combo, selected);
  };

  for (const BaseRow &row : std::as_const(_baseRows)) {
    refill(row.parent, tr("(root)"), row.name->text().trimmed());
  }
  for (const TopicRow &row : std::as_const(_topicRows)) {
    refill(row.parent, QString(), QString());
    refill(row.startPoint, tr("(parent)"), QString());
  }

  _refreshing = false;
  validate();
}

GitRepo::GitFlowNextWorkflow GitFlowNextWorkflowOverlayWidget::collect() const {
  GitRepo::GitFlowNextWorkflow workflow = _workflow;
  workflow.name = _lineEditName->text().trimmed();
  workflow.branches.clear();

  for (const BaseRow &row : _baseRows) {
    GitRepo::GitFlowNextBranch branch;
    branch.name = row.name->text().trimmed();
    branch.parent = row.parent->currentData().toString();
    branch.autoUpdate = row.autoUpdate->isChecked();
    branch.downstreamStrategy = row.downstream->currentData().toString();
    workflow.branches.append(branch);
  }
  for (const TopicRow &row : _topicRows) {
    GitRepo::GitFlowNextBranch branch;
    branch.topic = true;
    branch.name = row.name->text().trimmed();
    branch.prefix = row.prefix->text().trimmed();
    branch.parent = row.parent->currentData().toString();
    branch.startPoint = row.startPoint->currentData().toString();
    branch.upstreamStrategy = row.upstream->currentData().toString();
    branch.downstreamStrategy = row.downstream->currentData().toString();
    branch.tag = row.tag->isChecked();
    branch.tagPrefix = branch.tag ? row.tagPrefix->text().trimmed() : QString();
    branch.keepOnFinish = row.keep->isChecked();
    workflow.branches.append(branch);
  }

  return workflow;
}

void GitFlowNextWorkflowOverlayWidget::validate() {
  if (_refreshing) {
    return;
  }

  const QStringList problems = GitRepo::validateGitFlowNextWorkflow(collect());
  _pushButtonSubmit->setEnabled(problems.isEmpty());

  QStringList shown = problems.mid(0, 3);
  if (problems.size() > shown.size()) {
    shown.append(tr("... and %1 more").arg(problems.size() - shown.size()));
  }
  _labelProblems->setText(shown.join(QLatin1Char('\n')));
  _labelProblems->setVisible(!problems.isEmpty());
}

void GitFlowNextWorkflowOverlayWidget::submit() {
  const GitRepo::GitFlowNextWorkflow workflow = collect();
  if (!GitRepo::validateGitFlowNextWorkflow(workflow).isEmpty()) {
    validate();
    return;
  }
  emit submitted(workflow);
}

void GitFlowNextWorkflowOverlayWidget::watch(QWidget *widget) {
  widget->installEventFilter(this);
}

bool GitFlowNextWorkflowOverlayWidget::eventFilter(QObject *obj,
                                                   QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    const auto *keyEvent = static_cast<QKeyEvent *>(event);
    if (keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true;
    }
    // Plain Enter stays in the form, a form with many fields can't be saved
    // on every Enter.
    if ((keyEvent->key() == Qt::Key_Return ||
         keyEvent->key() == Qt::Key_Enter) &&
        keyEvent->modifiers().testFlag(Qt::ControlModifier)) {
      submit();
      return true;
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

} // namespace GuiManager::Widgets
