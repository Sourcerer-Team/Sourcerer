#include "widgets/MultilineTextInputOverlayWidget.h"
#include "widgets/OpaqueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

#define THREAD_ID "MultilineTextInputOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

MultilineTextInputOverlayWidget::MultilineTextInputOverlayWidget(
    QWidget *parent)
    : OverlayWidget(parent, ALPHA), _textEditInput(new QTextEdit(this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(520);
  groupBox->setMaximumWidth(520);
  groupBox->setMinimumHeight(220);
  groupBox->setMaximumHeight(220);

  _textEditInput->setParent(groupBox);
  _textEditInput->setObjectName("textEditMultilineOverlayWidgetInput");
  _textEditInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
  _textEditInput->setPlaceholderText(QString());
  _textEditInput->setAcceptRichText(false);
  _textEditInput->setTabChangesFocus(true);
  _textEditInput->setFocus();

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

  const auto textLayout = new QVBoxLayout();
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->setSpacing(0);
  textLayout->addWidget(_textEditInput);

  const auto buttonLayout = new QHBoxLayout();
  buttonLayout->setAlignment(Qt::AlignCenter);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);
  buttonLayout->addWidget(pushButtonSubmit);
  buttonLayout->addWidget(pushButtonCancel);

  const auto verticalLayout = new QVBoxLayout(groupBox);
  verticalLayout->setAlignment(Qt::AlignCenter);
  verticalLayout->setContentsMargins(6, 6, 6, 6);
  verticalLayout->setSpacing(10);
  verticalLayout->addLayout(textLayout);
  verticalLayout->addLayout(buttonLayout);

  getLayout()->addWidget(groupBox);

  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &MultilineTextInputOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &MultilineTextInputOverlayWidget::onButtonCanceled);

  _textEditInput->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

MultilineTextInputOverlayWidget::~MultilineTextInputOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool MultilineTextInputOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true;
    }

    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        (keyEvent->key() == Qt::Key_Return ||
         keyEvent->key() == Qt::Key_Enter) &&
        (keyEvent->modifiers() & Qt::ControlModifier)) {
      emit submitted(_textEditInput->toPlainText());
      return true;
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void MultilineTextInputOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void MultilineTextInputOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  emit submitted(_textEditInput->toPlainText());

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
