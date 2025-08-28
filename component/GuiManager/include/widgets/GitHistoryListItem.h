#pragma once

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

protected:
  void paintEvent(QPaintEvent *) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;

private:
  Ui::GitHistoryListItem *ui;
};

} // namespace Widgets

} // namespace GuiManager
