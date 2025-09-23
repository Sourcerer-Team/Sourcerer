#include "widgets/TextInputOverlayWidget.h"
#include "widgets/OpagueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QGroupBox>
#include <QKeyEvent>
#include <QPushButton>

#define THREAD_ID "TextInputOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

TextInputOverlayWidget::TextInputOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _lineEditInput(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(QColor::fromString("#384D59"), this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(400);
  groupBox->setMaximumWidth(400);
  groupBox->setMinimumHeight(100);
  groupBox->setMaximumHeight(100);

  _lineEditInput->setParent(groupBox);
  _lineEditInput->setObjectName("lineEditTextOverlayWidgetInput");
  _lineEditInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  _lineEditInput->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditInput->setMaximumWidth(250);
  _lineEditInput->setMaximumWidth(250);
  _lineEditInput->setFocus();

  const auto pushButtonSubmit =
      new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  pushButtonSubmit->setObjectName("pushButtonTextOverlayWidgetInputSubmit");
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(QPixmap(":icons/ok.png"));
  pushButtonSubmit->setFlat(false);

  const auto pushButtonCancel =
      new OpaquePushButton(QColor::fromString("#384D59"), groupBox);
  pushButtonCancel->setObjectName("pushButtonTextOverlayWidgetInputCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(QPixmap(":icons/cancel.png"));
  pushButtonSubmit->setFlat(false);

  const auto horizontalLayout = new QHBoxLayout();
  horizontalLayout->setAlignment(Qt::AlignCenter);
  horizontalLayout->setContentsMargins(0, 0, 0, 0);
  horizontalLayout->setSpacing(6);
  horizontalLayout->addWidget(_lineEditInput);
  horizontalLayout->addWidget(pushButtonSubmit);
  horizontalLayout->addWidget(pushButtonCancel);

  const auto horizontalLayoutGroupBox = new QVBoxLayout(groupBox);
  horizontalLayoutGroupBox->setAlignment(Qt::AlignCenter);
  horizontalLayoutGroupBox->setContentsMargins(6, 6, 6, 6);
  horizontalLayoutGroupBox->setSpacing(6);
  horizontalLayoutGroupBox->addLayout(horizontalLayout);

  getLayout()->addWidget(groupBox);

  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &TextInputOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &TextInputOverlayWidget::onButtonCanceled);

  _lineEditInput->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

TextInputOverlayWidget::~TextInputOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

bool TextInputOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true; // Event handled
    } else if (keyEvent->key() == Qt::Key_Return) {
      emit submitted(_lineEditInput->text());
      return true; // Event handled
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void TextInputOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void TextInputOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  emit submitted(_lineEditInput->text());

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
