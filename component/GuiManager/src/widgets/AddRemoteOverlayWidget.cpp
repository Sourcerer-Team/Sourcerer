// MIT License
// Copyright (c) 2026 Sourcerer

#include "widgets/AddRemoteOverlayWidget.h"
#include "widgets/OpaqueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QGroupBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "AddRemoteOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

AddRemoteOverlayWidget::AddRemoteOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _lineEditRemoteName(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditFetchUrl(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditPushUrl(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxAddRemoteOverlayWidget");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(450);
  groupBox->setMaximumWidth(450);
  groupBox->setMinimumHeight(130);
  groupBox->setMaximumHeight(130);

  _lineEditRemoteName->setParent(groupBox);
  _lineEditRemoteName->setObjectName("lineEditAddRemoteOverlayWidgetName");
  _lineEditRemoteName->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Preferred);
  _lineEditRemoteName->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditRemoteName->setMaximumWidth(300);
  _lineEditRemoteName->setFocus();
  _lineEditRemoteName->setPlaceholderText(tr("Remote name (e.g., origin)"));

  _lineEditFetchUrl->setParent(groupBox);
  _lineEditFetchUrl->setObjectName("lineEditAddRemoteOverlayWidgetFetchUrl");
  _lineEditFetchUrl->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
  _lineEditFetchUrl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditFetchUrl->setMaximumWidth(300);
  _lineEditFetchUrl->setPlaceholderText(tr("Fetch URL"));

  _lineEditPushUrl->setParent(groupBox);
  _lineEditPushUrl->setObjectName("lineEditAddRemoteOverlayWidgetPushUrl");
  _lineEditPushUrl->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
  _lineEditPushUrl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditPushUrl->setMaximumWidth(300);
  _lineEditPushUrl->setPlaceholderText(
      tr("Push URL (leave empty to use fetch URL)"));

  const auto pushButtonSubmit =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonSubmit->setObjectName("pushButtonAddRemoteOverlayWidgetSubmit");
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(overlayButtonIcon(":icons/ok.png"));
  pushButtonSubmit->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonCancel->setObjectName("pushButtonAddRemoteOverlayWidgetCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(overlayButtonIcon(":icons/cancel.png"));
  pushButtonCancel->setFlat(false);

  const auto verticalLayout = new QVBoxLayout();
  verticalLayout->setAlignment(Qt::AlignCenter);
  verticalLayout->setContentsMargins(0, 0, 0, 0);
  verticalLayout->setSpacing(6);
  verticalLayout->addWidget(_lineEditRemoteName);
  verticalLayout->addWidget(_lineEditFetchUrl);
  verticalLayout->addWidget(_lineEditPushUrl);

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
          &AddRemoteOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &AddRemoteOverlayWidget::onButtonCanceled);

  _lineEditRemoteName->installEventFilter(this);
  _lineEditFetchUrl->installEventFilter(this);
  _lineEditPushUrl->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

AddRemoteOverlayWidget::~AddRemoteOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void AddRemoteOverlayWidget::reset() {
  LOG_PERF(0, "Enter");

  _lineEditRemoteName->clear();
  _lineEditFetchUrl->clear();
  _lineEditPushUrl->clear();
  _lineEditRemoteName->setFocus();

  LOG_PERF(0, "Exit");
}

bool AddRemoteOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Return || keyEvent->key() == Qt::Key_Enter) {
      onButtonSubmitted();
      return true;
    } else if (keyEvent->key() == Qt::Key_Escape) {
      onButtonCanceled();
      return true;
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void AddRemoteOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  reset();
  emit canceled();

  LOG_PERF(0, "Exit");
}

void AddRemoteOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  const QString remoteName = _lineEditRemoteName->text().trimmed();
  const QString fetchUrl = _lineEditFetchUrl->text().trimmed();
  const QString pushUrl = _lineEditPushUrl->text().trimmed();

  if (remoteName.isEmpty() || fetchUrl.isEmpty()) {
    LOG_DEBUG("Remote name or fetch URL is empty");
    LOG_PERF(0, "Exit");
    return;
  }

  emit submitted(remoteName, fetchUrl, pushUrl.isEmpty() ? fetchUrl : pushUrl);

  reset();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
