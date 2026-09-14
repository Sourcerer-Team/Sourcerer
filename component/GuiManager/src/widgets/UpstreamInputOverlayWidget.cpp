#include "widgets/UpstreamInputOverlayWidget.h"
#include "widgets/OpaqueComboBox.h"
#include "widgets/OpaqueGroupBox.h"
#include "widgets/OpaqueLabel.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>

#define THREAD_ID "UpstreamInputOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

UpstreamInputOverlayWidget::UpstreamInputOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _comboBoxRemoteInput(new OpaqueComboBox(overlayBackgroundColor(), this)),
      _lineEditBranchInput(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(640);
  groupBox->setMaximumWidth(640);
  groupBox->setMinimumHeight(100);
  groupBox->setMaximumHeight(100);

  _comboBoxRemoteInput->setParent(groupBox);
  _comboBoxRemoteInput->setObjectName("comboBoxTextOverlayWidgetInputRemote");
  _comboBoxRemoteInput->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
  _comboBoxRemoteInput->setMaximumWidth(250);
  _comboBoxRemoteInput->setMinimumHeight(24);
  _comboBoxRemoteInput->setMaximumHeight(24);
  _comboBoxRemoteInput->setFocus();

  _lineEditBranchInput->setParent(groupBox);
  _lineEditBranchInput->setObjectName("lineEditTextOverlayWidgetInputBranch");
  _lineEditBranchInput->setSizePolicy(QSizePolicy::Expanding,
                                      QSizePolicy::Preferred);
  _lineEditBranchInput->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditBranchInput->setMaximumWidth(250);
  _lineEditBranchInput->setMaximumWidth(250);

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

  const auto labelSlash = new OpaqueLabel(overlayBackgroundColor(), groupBox);
  labelSlash->setObjectName("labelTextOverlayWidgetInputSlash");
  labelSlash->setText("/");
  labelSlash->setAlignment(Qt::AlignCenter);
  labelSlash->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  labelSlash->setMinimumWidth(8);
  labelSlash->setMaximumWidth(8);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setAlignment(Qt::AlignCenter);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSpacing(6);
  horizontalLayout->addWidget(_comboBoxRemoteInput);
  horizontalLayout->addWidget(labelSlash);
  horizontalLayout->addWidget(_lineEditBranchInput);
  horizontalLayout->addWidget(pushButtonSubmit);
  horizontalLayout->addWidget(pushButtonCancel);

  const auto horizontalLayoutGroupBox = new QVBoxLayout(groupBox);
  horizontalLayoutGroupBox->setAlignment(Qt::AlignCenter);
  horizontalLayoutGroupBox->setContentsMargins(6, 6, 6, 6);
  horizontalLayoutGroupBox->setSpacing(6);
  horizontalLayoutGroupBox->addLayout(horizontalLayout);

  getLayout()->addWidget(groupBox);

  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &UpstreamInputOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &UpstreamInputOverlayWidget::onButtonCanceled);

  _comboBoxRemoteInput->installEventFilter(this);
  _lineEditBranchInput->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

UpstreamInputOverlayWidget::~UpstreamInputOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void UpstreamInputOverlayWidget::setRemotes(
    const QVector<GitRepo::RemoteInfo> &remotes) {
  LOG_PERF(0, "Enter");

  _comboBoxRemoteInput->clear();

  for (const auto &remote : remotes) {
    _comboBoxRemoteInput->addItem(remote.name);
  }

  LOG_PERF(0, "Exit");
}

void UpstreamInputOverlayWidget::setRemoteName(
    const QString &remoteName) const {
  LOG_PERF(0, "Enter");

  const int index = _comboBoxRemoteInput->findText(remoteName);
  if (index >= 0) {
    _comboBoxRemoteInput->setCurrentIndex(index);
  }

  LOG_PERF(0, "Exit");
}

bool UpstreamInputOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true; // Event handled
    } else if (keyEvent->key() == Qt::Key_Return) {
      emit submitted(_comboBoxRemoteInput->currentText(),
                     _lineEditBranchInput->text());
      return true; // Event handled
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void UpstreamInputOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void UpstreamInputOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  emit submitted(_comboBoxRemoteInput->currentText(),
                 _lineEditBranchInput->text());

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
