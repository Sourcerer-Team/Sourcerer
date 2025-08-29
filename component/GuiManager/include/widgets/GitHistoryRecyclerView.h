#pragma once

#include <recyclerview.h>

#include <QObject>

namespace GuiManager {

namespace Widgets {

class GitHistoryRecyclerView : public RecyclerView {
private:
  Q_OBJECT
public:
  explicit GitHistoryRecyclerView(QWidget *parent = nullptr);

protected:
  ViewHolder *populateItem(int dataPos, int y) override;
};

}  // namespace Widgets

}  // namespace GuiManager