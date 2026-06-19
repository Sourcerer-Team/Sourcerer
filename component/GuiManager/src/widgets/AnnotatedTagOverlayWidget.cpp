#include "widgets/AnnotatedTagOverlayWidget.h"
#include "widgets/OpagueGroupBox.h"
#include "widgets/OpaquePushButton.h"

#include <Log.h>

#include <QCheckBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QVBoxLayout>

#define THREAD_ID "AnnotatedTagOverlayWidget"

#define ALPHA 255

namespace GuiManager::Widgets {

AnnotatedTagOverlayWidget::AnnotatedTagOverlayWidget(QWidget *parent)
    : OverlayWidget(parent, ALPHA),
      _lineEditTagName(new PersistentPlaceholderLineEdit(
          QColor::fromString("#969696"), this)),
      _textEditMessage(new QTextEdit(this)),
      _checkBoxAnnotated(new QCheckBox(tr("Create as annotated tag"), this)) {
  LOG_PERF(0, "Enter");

  const auto groupBox = new OpaqueGroupBox(overlayBackgroundColor(), this);
  groupBox->setObjectName("groupBoxAnnotatedTagOverlayWidget");
  groupBox->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
  groupBox->setMinimumWidth(500);
  groupBox->setMaximumWidth(500);
  groupBox->setMinimumHeight(280);
  groupBox->setMaximumHeight(280);

  // Tag name input
  _lineEditTagName->setParent(groupBox);
  _lineEditTagName->setObjectName("lineEditAnnotatedTagName");
  _lineEditTagName->setPlaceholderText(tr("Tag name"));
  _lineEditTagName->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  _lineEditTagName->setFocus();

  // Tag message text area
  _textEditMessage->setParent(groupBox);
  _textEditMessage->setObjectName("textEditAnnotatedTagMessage");
  _textEditMessage->setPlaceholderText(tr("Tag message (optional)"));
  _textEditMessage->setMaximumHeight(120);
  _textEditMessage->setEnabled(
      false); // Initially disabled for lightweight tags

  // Annotated checkbox
  _checkBoxAnnotated->setParent(groupBox);
  _checkBoxAnnotated->setObjectName("checkBoxAnnotatedTag");
  _checkBoxAnnotated->setChecked(
      false); // Default to lightweight tags for better UX
  _checkBoxAnnotated->setStyleSheet("QCheckBox { color: #FFFFFF; }");

  // Submit button
  const auto pushButtonSubmit =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonSubmit->setObjectName("pushButtonAnnotatedTagSubmit");
  pushButtonSubmit->setMaximumWidth(32);
  pushButtonSubmit->setMaximumHeight(32);
  pushButtonSubmit->setIcon(overlayButtonIcon(":icons/ok.png"));
  pushButtonSubmit->setFlat(false);

  // Cancel button
  const auto pushButtonCancel =
      new OpaquePushButton(overlayBackgroundColor(), groupBox);
  pushButtonCancel->setObjectName("pushButtonAnnotatedTagCancel");
  pushButtonCancel->setMaximumWidth(32);
  pushButtonCancel->setMaximumHeight(32);
  pushButtonCancel->setIcon(overlayButtonIcon(":icons/cancel.png"));
  pushButtonCancel->setFlat(false);

  // Main layout
  const auto mainLayout = new QVBoxLayout(groupBox);
  mainLayout->setAlignment(Qt::AlignTop | Qt::AlignHCenter);
  mainLayout->setContentsMargins(12, 12, 12, 12);
  mainLayout->setSpacing(8);

  // Tag name layout
  const auto nameLayout = new QHBoxLayout();
  nameLayout->setContentsMargins(0, 0, 0, 0);
  nameLayout->setSpacing(6);
  nameLayout->addWidget(_lineEditTagName);

  mainLayout->addLayout(nameLayout);

  // Message layout
  const auto messageLayout = new QVBoxLayout();
  messageLayout->setContentsMargins(0, 0, 0, 0);
  messageLayout->setSpacing(4);
  messageLayout->addWidget(_textEditMessage);

  mainLayout->addLayout(messageLayout);

  // Checkbox layout
  const auto checkboxLayout = new QHBoxLayout();
  checkboxLayout->setContentsMargins(0, 0, 0, 0);
  checkboxLayout->setSpacing(6);
  checkboxLayout->addWidget(_checkBoxAnnotated);
  checkboxLayout->addStretch();

  mainLayout->addLayout(checkboxLayout);

  // Buttons layout
  const auto buttonLayout = new QHBoxLayout();
  buttonLayout->setAlignment(Qt::AlignRight);
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->setSpacing(6);
  buttonLayout->addStretch();
  buttonLayout->addWidget(pushButtonSubmit);
  buttonLayout->addWidget(pushButtonCancel);

  mainLayout->addLayout(buttonLayout);
  mainLayout->addStretch();

  getLayout()->addWidget(groupBox);

  connect(pushButtonSubmit, &QPushButton::clicked, this,
          &AnnotatedTagOverlayWidget::onButtonSubmitted);
  connect(pushButtonCancel, &QPushButton::clicked, this,
          &AnnotatedTagOverlayWidget::onButtonCanceled);
  connect(_checkBoxAnnotated, &QCheckBox::toggled, this,
          &AnnotatedTagOverlayWidget::onAnnotatedCheckBoxToggled);

  _lineEditTagName->installEventFilter(this);
  _textEditMessage->installEventFilter(this);

  LOG_PERF(0, "Exit");
}

AnnotatedTagOverlayWidget::~AnnotatedTagOverlayWidget() {
  LOG_PERF(0, "Enter");
  LOG_PERF(0, "Exit");
}

void AnnotatedTagOverlayWidget::setTagNamePlaceholder(
    const QString &text) const {
  _lineEditTagName->setPlaceholderText(text);
}

void AnnotatedTagOverlayWidget::setTagMessagePlaceholder(
    const QString &text) const {
  _textEditMessage->setPlaceholderText(text);
}

bool AnnotatedTagOverlayWidget::eventFilter(QObject *obj, QEvent *event) {
  if (event->type() == QEvent::KeyPress) {
    if (const auto *keyEvent = static_cast<QKeyEvent *>(event);
        keyEvent->key() == Qt::Key_Escape) {
      emit canceled();
      return true; // Event handled
    } else if (keyEvent->key() == Qt::Key_Return &&
               keyEvent->modifiers() == Qt::ControlModifier) {
      // Ctrl+Return to submit
      if (obj == _lineEditTagName || obj == _textEditMessage) {
        emit submitted(_lineEditTagName->text(),
                       _textEditMessage->toPlainText(),
                       _checkBoxAnnotated->isChecked());
        return true;
      }
    }
  }

  return OverlayWidget::eventFilter(obj, event);
}

void AnnotatedTagOverlayWidget::onButtonCanceled() {
  LOG_PERF(0, "Enter");

  emit canceled();

  LOG_PERF(0, "Exit");
}

void AnnotatedTagOverlayWidget::onButtonSubmitted() {
  LOG_PERF(0, "Enter");

  const QString tagName = _lineEditTagName->text().trimmed();
  if (tagName.isEmpty()) {
    LOG_DEBUG("Tag name is empty, cannot submit");
    LOG_PERF(0, "Exit");
    return;
  }

  const QString message = _textEditMessage->toPlainText().trimmed();
  const bool isAnnotated = _checkBoxAnnotated->isChecked();

  emit submitted(tagName, message, isAnnotated);

  LOG_PERF(0, "Exit");
}

void AnnotatedTagOverlayWidget::onAnnotatedCheckBoxToggled(bool checked) {
  LOG_PERF(0, "Enter");

  _textEditMessage->setEnabled(checked);

  if (!checked) {
    _textEditMessage->clear();
  }

  LOG_PERF(0, "Exit");
}

} // namespace GuiManager::Widgets
