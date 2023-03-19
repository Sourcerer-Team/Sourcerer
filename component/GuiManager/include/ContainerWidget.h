#pragma once

#include <QWidget>

namespace GuiManager {

class ContainerWidget : public QWidget {
  Q_OBJECT
public:
  explicit ContainerWidget(QWidget *parent = nullptr);
  virtual ~ContainerWidget();

signals:
};

} // namespace GuiManager
