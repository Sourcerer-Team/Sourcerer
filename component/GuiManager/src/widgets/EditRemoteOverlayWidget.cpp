#include "widgets/EditRemoteOverlayWidget.h"
#include "widgets/OpagueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QGroupBox>
#include <QKeyEvent>
#include <QPushButton>
#include <QVBoxLayout>

#define THREAD_ID "EditRemoteOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

EditRemoteOverlayWidget::EditRemoteOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA), _lineEditRemoteName(new QLineEdit(this)),
      _lineEditFetchUrl(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _lineEditPushUrl(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(QColor::fromString("#384D59"), this);
  groupBox->setObjectName("groupBoxEditRemoteOverlayWidget");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(450);
  groupBox->setMaximumWidth(450);
  groupBox->setMinimumHeight(130);
  groupBox->setMaximumHeight(130);

  _lineEditRemoteName->setParent(groupBox);
  _lineEditRemoteName->setObjectName("lineEditEditRemoteOverlayWidgetName");
  _lineEditRemoteName->setSizePolicy(QSizePolicy::Expanding,
                                     QSizePolicy::Preferred);
  _lineEditRemoteName->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditRemoteName->setMaximumWidth(300);
  _lineEditRemoteName->setReadOnly(true);
  _lineEditRemoteName->setPlaceholderText(tr("Remote name"));

  _lineEditFetchUrl->setParent(groupBox);
  _lineEditFetchUrl->setObjectName("lineEditEditRemoteOverlayWidgetFetchUrl");
  _lineEditFetchUrl->setSizePolicy(QSizePolicy::Expanding,
                                   QSizePolicy::Preferred);
  _lineEditFetchUrl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditFetchUrl->setMaximumWidth(300);
  _lineEditFetchUrl->setFocus();
  _lineEditFetchUrl->setPlaceholderText(tr("Fetch URL"));

  _lineEditPushUrl->setParent(groupBox);
  _lineEditPushUrl->setObjectName("lineEditEditRemoteOverlayWidgetPushUrl");
  _lineEditPushUrl->setSizePolicy(QSizePolicy::Expanding,
                                  QSizePolicy::Preferred);
  _lineEditPushUrl->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditPushUrl->setMaximumWidth(300);
  _lineEditPushUrl->setPlaceholderText(
      tr("Push URL (leave empty to use fetch URL)"));

  const auto pushButtonSubmit =
      new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  pushButtonSubmit->setObjectName("pushButtonEditRemoteOverlayWidgetSubmit");
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(QPixmap(":icons/ok.png"));
  pushButtonSubmit->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  pushButtonCancel->setObjectName("pushButtonEditRemoteOverlayWidgetCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(QPixmap(":icons/cancel.png"));
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
          &EditRemoteOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &EditRemoteOverlayWidget::onButtonCanceled);

  _lineEditFetchUrl->installEventFilter(this);
  _lineEditPushUrl->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

EditRemoteOverlayWidget::~EditRemoteOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void EditRemoteOverlayWidget::setRemoteInfo(const QString &remoteName,
                                            const QString &fetchUrl,
                                            const QString &pushUrl) {
  LOG_PERF(0, "Enter");

  _originalRemoteName = remoteName;
  _lineEditRemoteName->setText(remoteName);
  _lineEditFetchUrl->setText(fetchUrl);
  _lineEditPushUrl->setText(pushUrl);
  _lineEditFetchUrl->setFocus();

  LOG_PERF(0, "Exit");
}

void EditRemoteOverlayWidget::reset() {
  LOG_PERF(0, "Enter");

  _lineEditRemoteName->clear();
  _lineEditFetchUrl->clear();
  _lineEditPushUrl->clear();
  _originalRemoteName.clear();

  LOG_PERF(0, "Exit");
}

bool EditRemoteOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
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

void EditRemoteOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  reset();
  emit canceled();

  LOG_PERF(0, "Exit");
}

void EditRemoteOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  const QString fetchUrl = _lineEditFetchUrl->text().trimmed();
  const QString pushUrl = _lineEditPushUrl->text().trimmed();

  if (fetchUrl.isEmpty()) {
    LOG_DEBUG("Fetch URL is empty");
    LOG_PERF(0, "Exit");
    return;
  }

  emit submitted(fetchUrl, pushUrl.isEmpty() ? fetchUrl : pushUrl);

  reset();

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
