#include "widgets/AddWorktreeOverlayWidget.h"
#include "widgets/OpaqueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QFileDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "AddWorktreeOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

AddWorktreeOverlayWidget::AddWorktreeOverlayWidget(QWidget *parent,
                                                   const QString &defaultPath,
                                                   const QStringList &branches,
                                                   const QString &activeBranch)
    : OverlayWidget(parent, ALPHA),
      _lineEditPath(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditNewBranch(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _comboBoxBranch(new OpaqueComboBox(overlayBackgroundColor(), this)),
      _checkBoxCreateBranch(new QCheckBox(tr("Create new branch"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(440);
  groupBox->setMaximumWidth(440);
  groupBox->setMinimumHeight(190);
  groupBox->setMaximumHeight(190);

  _lineEditPath->setParent(groupBox);
  _lineEditPath->setObjectName("lineEditAddWorktreeOverlayWidgetPath");
  _lineEditPath->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  _lineEditPath->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditPath->setFocus();
  _lineEditPath->setPlaceholderText(tr("Enter worktree folder"));
  if (!defaultPath.isEmpty()) {
    _lineEditPath->setText(defaultPath);
  }

  const auto pushButtonBrowse =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonBrowse->setObjectName("pushButtonAddWorktreeOverlayWidgetBrowse");
  pushButtonBrowse->setText(tr("..."));
  pushButtonBrowse->setToolTip(tr("Browse for folder"));
  pushButtonBrowse->setMaximumWidth(32);
  pushButtonBrowse->setMaximumHeight(32);
  pushButtonBrowse->setFlat(false);

  _comboBoxBranch->setParent(groupBox);
  _comboBoxBranch->setObjectName("comboBoxAddWorktreeOverlayWidgetBranch");
  _comboBoxBranch->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
  _comboBoxBranch->setMinimumHeight(24);
  _comboBoxBranch->setMaximumHeight(24);
  _comboBoxBranch->addItems(branches);
  if (const int index = _comboBoxBranch->findText(activeBranch); index >= 0) {
    _comboBoxBranch->setCurrentIndex(index);
  }

  _checkBoxCreateBranch->setParent(groupBox);
  _checkBoxCreateBranch->setObjectName(
      "checkBoxAddWorktreeOverlayWidgetCreateBranch");
  _checkBoxCreateBranch->setChecked(true);
  _checkBoxCreateBranch->setToolTip(
      tr("Create a new branch starting at the selected branch. Uncheck to "
         "check out the selected branch, which must not be checked out "
         "anywhere else."));

  _lineEditNewBranch->setParent(groupBox);
  _lineEditNewBranch->setObjectName(
      "lineEditAddWorktreeOverlayWidgetNewBranch");
  _lineEditNewBranch->setSizePolicy(QSizePolicy::Expanding,
                                    QSizePolicy::Preferred);
  _lineEditNewBranch->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditNewBranch->setPlaceholderText(tr("Enter new branch name"));

  const auto pushButtonSubmit =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonSubmit->setObjectName("pushButtonTextOverlayWidgetInputSubmit");
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(overlayButtonIcon(":icons/ok.png"));
  pushButtonSubmit->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonCancel->setObjectName("pushButtonTextOverlayWidgetInputCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(overlayButtonIcon(":icons/cancel.png"));
  pushButtonCancel->setFlat(false);

  const auto pathLayout = new QHBoxLayout();
  pathLayout->setContentsMargins(0, 0, 0, 0);
  pathLayout->setSpacing(6);
  pathLayout->addWidget(_lineEditPath);
  pathLayout->addWidget(pushButtonBrowse);

  const auto verticalLayout = new QVBoxLayout();
  verticalLayout->setAlignment(Qt::AlignCenter);
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->setSpacing(6);
  verticalLayout->addLayout(pathLayout);
  verticalLayout->addWidget(_comboBoxBranch);
  verticalLayout->addWidget(_checkBoxCreateBranch);
  verticalLayout->addWidget(_lineEditNewBranch);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setAlignment(Qt::AlignCenter);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSpacing(6);
  horizontalLayout->addLayout(verticalLayout);
  horizontalLayout->addWidget(pushButtonSubmit);
  horizontalLayout->addWidget(pushButtonCancel);

  const auto horizontalLayoutGroupBox = new QVBoxLayout(groupBox);
  horizontalLayoutGroupBox->setAlignment(Qt::AlignCenter);
  horizontalLayoutGroupBox->setContentsMargins(6, 6, 6, 6);
  horizontalLayoutGroupBox->setSpacing(6);
  horizontalLayoutGroupBox->addLayout(horizontalLayout);

  getLayout()->addWidget(groupBox);

  connect(pushButtonBrowse, &QPushButton::clicked, this,
          &AddWorktreeOverlayWidget::onButtonBrowse);
  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &AddWorktreeOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &AddWorktreeOverlayWidget::onButtonCanceled);

  connect(_checkBoxCreateBranch, &QCheckBox::toggled, this,
          &AddWorktreeOverlayWidget::onCreateBranchToggled);

  _lineEditPath->installEventFilter(this);
  _lineEditNewBranch->installEventFilter(this);
  _comboBoxBranch->installEventFilter(this);

  onCreateBranchToggled(_checkBoxCreateBranch->isChecked());

  LOG_PERF(0, "Exit");
}

AddWorktreeOverlayWidget::~AddWorktreeOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool AddWorktreeOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true; // Event handled
    } else if (keyEvent->key() == Qt::Key_Return) {
      submit();
      return true; // Event handled
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void AddWorktreeOverlayWidget::submit() {
  const QString selectedBranch = _comboBoxBranch->currentText().trimmed();
  const bool createBranch = _checkBoxCreateBranch->isChecked();

  if (createBranch) {
    emit submitted(_lineEditPath->text().trimmed(),
                   _lineEditNewBranch->text().trimmed(), true, selectedBranch);
  } else {
    emit submitted(_lineEditPath->text().trimmed(), selectedBranch, false,
                   QString());
  }
}

void AddWorktreeOverlayWidget::onCreateBranchToggled(const bool checked) {
  _lineEditNewBranch->setVisible(checked);
  if (checked) {
    _lineEditNewBranch->setFocus();
  }
}

void AddWorktreeOverlayWidget::onButtonBrowse() {
  LOG_PERF(0, "Enter");

  const QString current = _lineEditPath->text().trimmed();
  const QString dir = QFileDialog::getExistingDirectory(
      this, tr("Select worktree folder"), current);
  if (!dir.isEmpty()) {
    _lineEditPath->setText(dir);
  }

  _lineEditPath->setFocus();

  LOG_PERF(0, "Exit");
}

void AddWorktreeOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void AddWorktreeOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  submit();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
