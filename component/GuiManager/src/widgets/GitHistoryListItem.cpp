#include "widgets/GitHistoryListItem.h"
#include "widgets/ui_GitHistoryListItem.h"

#include <Log.h>

#define THREAD_ID "GitHistoryListItem"

namespace GuiManager {

namespace Widgets {

GitHistoryListItem::GitHistoryListItem(QWidget *parent)
    : StyledRecyclerListItem(parent), ui(new Ui::GitHistoryListItem) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  LOG_PERF(0, "Exit");
}

GitHistoryListItem::~GitHistoryListItem() {
  LOG_PERF(0, "Enter");

  delete ui;

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::setCommitSummary(const QString &message) const {
  LOG_PERF(0, "Enter");

  ui->labelCommitMessage->setText(message);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::setCommitDateTime(const QString &dateTime) const {
  LOG_PERF(0, "Enter");

  ui->labelCommitDateTime->setText(dateTime);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::setAuthor(const QString &author) const {
  LOG_PERF(0, "Enter");

  ui->labelAuthor->setText(author);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::setSHA(const QString &sha) const {
  LOG_PERF(0, "Enter");

  ui->labelSHA->setText(sha);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::paintEvent(QPaintEvent *event) {
  LOG_PERF(0, "Enter");

  StyledRecyclerListItem::paintEvent(event);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::enterEvent(QEnterEvent *event) {
  LOG_PERF(0, "Enter");

  StyledRecyclerListItem::enterEvent(event);

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::leaveEvent(QEvent *event) {
  LOG_PERF(0, "Enter");

  StyledRecyclerListItem::leaveEvent(event);

  LOG_PERF(0, "Exit");
}

} // namespace Widgets

} // namespace GuiManager
