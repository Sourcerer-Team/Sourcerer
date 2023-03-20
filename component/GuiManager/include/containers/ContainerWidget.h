#pragma once

#include <QWidget>

namespace GuiManager {

namespace Containers {

class ContainerWidget : public QWidget {
  Q_OBJECT
public:
  explicit ContainerWidget(QWidget *parent = nullptr);
  virtual ~ContainerWidget();

signals:
};

} // namespace Containers
} // namespace GuiManager
