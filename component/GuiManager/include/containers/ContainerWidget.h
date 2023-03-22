#pragma once

#include <QWidget>

namespace GuiManager {

namespace Containers {

enum class ContainerType {
  Unknown = 0,
  Home,
  PleaseWait,

  last
};

class ContainerWidget : public QWidget {
  Q_OBJECT
public:
  explicit ContainerWidget(ContainerType type, QWidget *parent = nullptr);
  virtual ~ContainerWidget();

  virtual ContainerType type() const { return _type; }

signals:

private:
  ContainerType _type;
};

} // namespace Containers
} // namespace GuiManager
