#include "widgets/AddSubmoduleOverlayWidget.h"
#include "widgets/OpagueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QGroupBox>
#include <QKeyEvent>
#include <QPushButton>

#define THREAD_ID "AddSubmoduleOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

AddSubmoduleOverlayWidget::AddSubmoduleOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _lineEditUrl(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditName(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditBranch(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(400);
  groupBox->setMaximumWidth(400);
  groupBox->setMinimumHeight(130);
  groupBox->setMaximumHeight(130);

  _lineEditUrl->setParent(groupBox);
  _lineEditUrl->setObjectName("lineEditAddSubmoduleOverlayWidgetUrl");
  _lineEditUrl->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  _lineEditUrl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditUrl->setMaximumWidth(250);
  _lineEditUrl->setMaximumWidth(250);
  _lineEditUrl->setFocus();
  _lineEditUrl->setPlaceholderText(tr("Enter repository URL"));

  _lineEditName->setParent(groupBox);
  _lineEditName->setObjectName("lineEditAddSubmoduleOverlayWidgetName");
  _lineEditName->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  _lineEditName->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditName->setMaximumWidth(250);
  _lineEditName->setMaximumWidth(250);
  _lineEditName->setPlaceholderText(tr("Enter submodule name"));

  _lineEditBranch->setParent(groupBox);
  _lineEditBranch->setObjectName("lineEditAddSubmoduleOverlayWidgetBranch");
  _lineEditBranch->setSizePolicy(QSizePolicy::Expanding,
                                 QSizePolicy::Preferred);
  _lineEditBranch->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditBranch->setMaximumWidth(250);
  _lineEditBranch->setMaximumWidth(250);
  _lineEditBranch->setPlaceholderText(tr("Enter branch to track"));

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
  pushButtonSubmit->setFlat(false);

  const auto verticalLayout = new QVBoxLayout();
  verticalLayout->setAlignment(Qt::AlignCenter);
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->setSpacing(6);
  verticalLayout->addWidget(_lineEditUrl);
  verticalLayout->addWidget(_lineEditName);
  verticalLayout->addWidget(_lineEditBranch);

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

  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &AddSubmoduleOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &AddSubmoduleOverlayWidget::onButtonCanceled);

  _lineEditUrl->installEventFilter(this);
  _lineEditName->installEventFilter(this);
  _lineEditBranch->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

AddSubmoduleOverlayWidget::~AddSubmoduleOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool AddSubmoduleOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = dynamic_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true; // Event handled
    } else if (keyEvent->key() == Qt::Key_Return) {
      emit submitted(_lineEditUrl->text(), _lineEditName->text(),
                     _lineEditBranch->text());
      return true; // Event handled
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void AddSubmoduleOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void AddSubmoduleOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  emit submitted(_lineEditUrl->text(), _lineEditName->text(),
                 _lineEditBranch->text());

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets