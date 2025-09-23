#include "widgets/TextInputOverlayWidget.h"

#include <Log.h>

#include <QGroupBox>
#include <QKeyEvent>
#include <QPushButton>

#define THREAD_ID "TextInputOverlayWidget"

#define ALPHA 200

namespace GuiManager::Widgets {

TextInputOverlayWidget::TextInputOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA), _lineEditInput(new QLineEdit(this)) {
  LOG_PERF(0, "Enter");

  _lineEditInput->setObjectName("lineEditTextOverlayWidgetInput");
  _lineEditInput->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  _lineEditInput->setAlignment(Qt::AlignHCenter | Qt::AlignVCenter);
  _lineEditInput->setMaximumWidth(250);
  _lineEditInput->setMaximumWidth(250);

  const auto pushButtonSubmit = new QPushButton(this);
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(QPixmap(":icons/ok.png"));

  const auto pushButtonCancel = new QPushButton(this);
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(QPixmap(":icons/cancel.png"));

  const auto groupBox = new QGroupBox(this);
  groupBox->setObjectName("groupBoxTextOverlayWidgetInput");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(640);
  groupBox->setMaximumWidth(640);
  groupBox->setMinimumHeight(100);
  groupBox->setMaximumHeight(100);

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
