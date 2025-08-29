#include "widgets/GitHistoryListItem.h"
#include "widgets/ui_GitHistoryListItem.h"

#include <Log.h>

#include <QDebug>
#include <QMouseEvent>
#include <QTimer>

#define THREAD_ID "GitHistoryListItem"

namespace GuiManager {

namespace Widgets {

GitHistoryListItem::GitHistoryListItem(QWidget *parent)
    : StyledRecyclerListItem(parent), ui(new Ui::GitHistoryListItem),
      _configManager(ConfigManager::ConfigManager::getInstance()) {
  LOG_PERF(0, "Enter");

  ui->setupUi(this);

  restoreSplitterState();

  connect(ui->splitter, &QSplitter::splitterMoved, this,
          &GitHistoryListItem::onSplitterMoved);

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

void GitHistoryListItem::restoreSplitterState() const {
  LOG_PERF(0, "Enter");

  const QByteArray splitterState = QByteArray::fromHex(
      _configManager
          .getString(ConfigManager::ConfigKey::
                         GitRepoBranchRecyclerViewItemsSplitterPos)
          .toUtf8());
  ui->splitter->restoreState(splitterState);

  const auto sizes = ui->splitter->sizes();

  ui->widgetGraph->setMinimumWidth(sizes[0]);
  ui->widgetGraph->setMaximumWidth(sizes[0]);
  ui->labelCommitMessage->setMinimumWidth(sizes[1]);
  ui->labelCommitMessage->setMaximumWidth(sizes[1]);
  ui->labelCommitDateTime->setMinimumWidth(sizes[2]);
  ui->labelCommitDateTime->setMaximumWidth(sizes[2]);
  ui->labelAuthor->setMinimumWidth(sizes[3]);
  ui->labelAuthor->setMaximumWidth(sizes[3]);
  ui->labelSHA->setMinimumWidth(sizes[4]);
  ui->labelSHA->setMaximumWidth(sizes[4]);

  // Remove constraints after synchronization
  QTimer::singleShot(0, this, [this]() {
        ui->widgetGraph->setMinimumWidth(0);
        ui->widgetGraph->setMaximumWidth(QWIDGETSIZE_MAX);
        ui->labelCommitMessage->setMinimumWidth(0);
        ui->labelCommitMessage->setMaximumWidth(QWIDGETSIZE_MAX);
        ui->labelCommitDateTime->setMinimumWidth(0);
        ui->labelCommitDateTime->setMaximumWidth(QWIDGETSIZE_MAX);
        ui->labelAuthor->setMinimumWidth(0);
        ui->labelAuthor->setMaximumWidth(QWIDGETSIZE_MAX);
        ui->labelSHA->setMinimumWidth(0);
        ui->labelSHA->setMaximumWidth(QWIDGETSIZE_MAX);
  });

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::saveSplitterState() const {
  LOG_PERF(0, "Enter");

  const QString splitterState = ui->splitter->saveState().toHex();
  _configManager.setString(
      ConfigManager::ConfigKey::GitRepoBranchRecyclerViewItemsSplitterPos,
      splitterState);
  _configManager.save();

  LOG_PERF(0, "Exit");
}

void GitHistoryListItem::onSplitterMoved(const int pos, const int index) {
  LOG_PERF(0, "Enter");
  LOG_DEBUG("pos = {}, index = {}", pos, index);

  saveSplitterState();

  emit splitterMoved();

  LOG_PERF(0, "Exit");
}

} // namespace Widgets

} // namespace GuiManager
