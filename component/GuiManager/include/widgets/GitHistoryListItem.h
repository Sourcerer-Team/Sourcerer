#pragma once

#include <ConfigManager.h>
#include <styledrecyclerlistitem.h>

#include <QWidget>

namespace Ui {
class GitHistoryListItem;
}

namespace GuiManager {

namespace Widgets {

class GitHistoryListItem : public StyledRecyclerListItem {
  Q_OBJECT
public:
  explicit GitHistoryListItem(QWidget *parent = nullptr);
  ~GitHistoryListItem() override;

  void setCommitSummary(const QString &message) const;
  void setCommitDateTime(const QString &dateTime) const;
  void setAuthor(const QString &author) const;
  void setSHA(const QString &sha) const;

  void restoreSplitterState() const;
  void saveSplitterState() const;

signals:
  void splitterMoved();

private slots:
  void onSplitterMoved(int pos, int index);

private:
  Ui::GitHistoryListItem *ui;
  ConfigManager::ConfigManager &_configManager;
};

} // namespace Widgets

} // namespace GuiManager
